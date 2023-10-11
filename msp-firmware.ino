/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007 
*/

// I2C bus pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// PMS5003 serial pins
// with WROVER module don't use UART 2 mode on pins 16 and 17: it crashes!
#define PMSERIAL_RX 14
#define PMSERIAL_TX 12

// Select modem type
#define TINY_GSM_MODEM_SIM800

// Serial modem pins
#define MODEM_RST            4
#define MODEM_TX             13
#define MODEM_RX             15

// O3 sensor ADC pin
#define O3_ADC_PIN 32

// Basic system libraries
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <Wire.h>

// current firmware version
#ifdef VERSION_STRING
String ver = VERSION_STRING;
#else
String ver = "DEV";
#endif

// WiFi Client, NTP time management, Modem and SSL libraries
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include "libs/trust_anchor.h" // Server Trust Anchor

// Sensors management libraries
//for BME_680
#include <bsec.h>
//for PMS5003
#include <PMS.h>
//for MICS6814
#include <MiCS6814-I2C.h>

// OLED display library
#include <U8g2lib.h>

#ifdef API_SECRET_SALT
String api_secret_salt = API_SECRET_SALT;
#else
String api_secret_salt = "secret_salt";
#endif

// Default server name for SSL data upload
#ifdef API_SERVER
String server = API_SERVER;
bool server_ok = true;
#else
String server = "";
bool server_ok = false;
#endif

//Modem stuff
#if !defined(TINY_GSM_RX_BUFFER) // Increase RX buffer to capture the entire response
#define TINY_GSM_RX_BUFFER 650
#endif
//#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed

// Hardware UART definitions. Modes: UART0=0(debug out); UART1=1; UART2=2
HardwareSerial gsmSerial(1);
HardwareSerial pmsSerial(2);

// Modem instance
/*
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(gsmSerial, Serial);
TinyGsm        modem(debugger);
#else*/
TinyGsm        modem(gsmSerial);
//#endif

// Pin to get semi-random data from for SSL
// Pick a pin that's not connected or attached to a randomish voltage source
const int rand_pin = 27;

// Initialize the SSL client library
// We input a client, our trust anchors, and the analog pin
WiFiClient wifi_base;
TinyGsmClient gsm_base(modem);
SSLClient wificlient(wifi_base, TAs, (size_t)TAs_NUM, rand_pin, 1, SSLClient::SSL_ERROR);
SSLClient gsmclient(gsm_base, TAs, (size_t)TAs_NUM, rand_pin, 1, SSLClient::SSL_ERROR);

// BME680, PMS5003 and MICS6814 sensors instances
Bsec bme680;
PMS pms(pmsSerial);
MiCS6814 gas;

// Instance for the OLED 1.3" display with the SH1106 controller
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);   // ESP32 Thing, HW I2C with pin remapping

// Global network and system setup variables defaults
bool SD_ok = false;
bool cfg_ok = false;
bool connected_ok = false;
bool use_modem = true;
bool datetime_ok = false;
String ssid = "";
String passw = "";
String apn = "TM";
String deviceid = "";
String logpath = "";
wifi_power_t wifipow = WIFI_POWER_17dBm;
int avg_measurements = 30;
int avg_delay = 55;

// Sensor active variables
bool BME_run = false;
bool PMS_run = false;
bool MICS_run = false;
bool MICS4514_run = false;
bool O3_run = false;

// Variables for BME680
float hum = 0.0;
float temp = 0.0;
float pre = 0.0;
float VOC = 0.0;
float sealevelalt = 122.0; // sea level altitude in meters, defaults to Milan, Italy

// Variables and structure for PMS5003
PMS::DATA data;
int PM1 = 0;
int PM10 = 0;
int PM25 = 0;

// Variables for MICS
uint16_t MICSCal[3] = {955, 900, 163}; // default R0 values for the sensor (in order: RED, OX, NH3)
int MICSOffset[3] = {0, 0, 0}; // default ppm offset values for sensor measurements (in order: RED, OX, NH3)
const float MICSmm[3] = {28.01, 46.01, 17.03}; // molar mass of (in order): CO, NO2, NH3
float MICS_CO = 0.0;
float MICS_NO2 = 0.0;
float MICS_NH3 = 0.0;

// Variables for ZE25-O3
float ozone = 0.0;
int o3zeroval = -1; // ozone sensor ADC zero default offset is 1489, -1 to disable it (0.4V to 1.1V range in 12bit resolution at 0dB attenuation)

// Variables for compensation (MICS6814-OX and BME680-VOC)
float compH = 0.6; // default humidity compensation
float compT = 1.352; // default temperature compensation
float compP = 0.0132; // default pressure compensation

// Date and time vars
struct tm timeinfo;
char Date[11] = {0};
char Time[9] = {0};

// Var for MSP# evaluation
short MSP = -1; // set to -1 to distinguish from grey (0)

// Sending data to server was successful?
bool sent_ok = false;

// Include system functions
#include "libs/sensors.h"
#include "libs/display.h"
#include "libs/sdcard.h"
#include "libs/network.h"

//*******************************************************************************************************************************
//******************************************  S E T U P  ************************************************************************
//*******************************************************************************************************************************
void setup() {

  // INIT SERIAL, I2C, DISPLAY ++++++++++++++++++++++++++++++++++++
  Serial.begin(115200);
  delay(2000); // give time to serial to initialize properly
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // SET PIN MODES ++++++++++++++++++++++++++++++++++++
  pinMode(33, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(25, OUTPUT);
  pinMode(O3_ADC_PIN, INPUT_PULLDOWN);
  analogSetAttenuation(ADC_11db);
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // BOOT STRINGS ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("\nMILANO SMART PARK");
  Serial.println("FIRMWARE " + ver + " by Norman Mulinacci");
  Serial.println("Compiled " + String(__DATE__) + " " + String(__TIME__) + "\n");
  drawBoot(&ver);
#ifdef VERSION_STRING
  log_i("ver was defined at compile time.\n");
#else
  log_i("ver is the default.\n");
#endif
#ifdef API_SECRET_SALT
  log_i("api_secret_salt was defined at compile time.\n");
#else
  log_i("api_secret_salt is the default.\n");
#endif
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  printWiFiMACAddr();

  readSD();

  //++++++++++++++++ DETECT AND INIT SENSORS ++++++++++++++++++++++++++++++
  log_i("Detecting and initializing sensors...\n");

  // BME680 +++++++++++++++++++++++++++++++++++++
  drawTwoLines("Detecting BME680...", "", 0);
  bme680.begin(BME68X_I2C_ADDR_HIGH, Wire);
  if (checkBMESensor()) {
    log_i("BME680 sensor detected, initializing...\n");
    drawTwoLines("Detecting BME680...", "BME680 -> Ok!", 1);
    bsec_virtual_sensor_t sensor_list[] = {
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };
    bme680.updateSubscription(sensor_list, sizeof(sensor_list) / sizeof(sensor_list[0]), BSEC_SAMPLE_RATE_LP);
    BME_run = true;
  } else {
    log_e("BME680 sensor not detected!\n");
    drawTwoLines("Detecting BME680...", "BME680 -> Err!", 1);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++

  // PMS5003 ++++++++++++++++++++++++++++++++++++
  pmsSerial.begin(9600, SERIAL_8N1, PMSERIAL_RX, PMSERIAL_TX); // baud, type, ESP_RX, ESP_TX
  delay(1500);
  drawTwoLines("Detecting PMS5003...", "", 0);
  pms.wakeUp(); // Waking up sensor after sleep
  delay(1500);
  if (pms.readUntil(data)) {
    log_i("PMS5003 sensor detected, initializing...\n");
    drawTwoLines("Detecting PMS5003...", "PMS5003 -> Ok!", 1);
    PMS_run = true;
    pms.sleep(); // Putting sensor to sleep
  } else {
    log_e("PMS5003 sensor not detected!\n");
    drawTwoLines("Detecting PMS5003...", "PMS5003 -> Err!", 1);
  }
  //++++++++++++++++++++++++++++++++++++++++++++++

  // MICS6814 ++++++++++++++++++++++++++++++++++++
  drawTwoLines("Detecting MICS6814...", "", 0);
  if (gas.begin()) { // Connect to sensor using default I2C address (0x04)
    log_i("MICS6814 sensor detected, initializing...\n");
    drawTwoLines("Detecting MICS6814...", "MICS6814 -> Ok!", 0);
    MICS_run = true;
    gas.powerOn(); // turn on heating element and led
    gas.ledOn();
    delay(1500);
    if (checkMicsValues()) {
      log_i("MICS6814 R0 values are already as default!\n");
      drawLine("MICS6814 values OK!", 1);
    } else {
      log_i("Setting MICS6814 R0 values as default... ");
      drawLine("Setting MICS6814...", 1);
      writeMicsValues();
      log_i("Done!\n");
      drawLine("Done!", 1);
    }
    drawMicsValues(gas.getBaseResistance(CH_RED), gas.getBaseResistance(CH_OX), gas.getBaseResistance(CH_NH3));
  } else {
    log_e("MICS6814 sensor not detected!\n");
    drawTwoLines("Detecting MICS6814...", "MICS6814 -> Err!", 1);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++

  // O3 ++++++++++++++++++++++++++++++++++++++++++
  drawTwoLines("Detecting O3...", "", 1);
    if (!isAnalogO3Connected()) {
      log_e("O3 sensor not detected!\n");
      drawTwoLines("Detecting O3...", "O3 -> Err!", 0);
    } else {
      log_i("O3 sensor detected, running...\n");
      drawTwoLines("Detecting O3...", "O3 -> Ok!", 0);
      O3_run = true;
    }
  delay(1500);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // CONNECT TO INTERNET AND GET DATE&TIME +++++++++++++++++++++++++++++++++++++++++++++++++++
  if (cfg_ok) connAndGetTime();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

}// end of SETUP
//*******************************************************************************************************************************
//*******************************************************************************************************************************
//*******************************************************************************************************************************



//*******************************************************************************************************************************
//********************************************  L O O P  ************************************************************************
//*******************************************************************************************************************************
void loop() {

  // DISCONNECTING AND TURNING OFF WIFI +++++++++++++++++++++++++++++++++++++
  if (!use_modem) disconnectWiFi();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // CLEARING VARIABLES +++++++++++++++++++++++++++++++++++++
  log_d("Clearing system variables...\n");
  sent_ok = false;
  Date[0] = '\0';
  Time[0] = '\0';
  int errcount = 0;
  short BMEfails = 0;
  temp = 0.0;
  float currtemp = 0.0; // used for compensating gas measures
  float currpre = 0.0; // used for compensating gas measures
  float currhum = 0.0; // used for compensating gas measures
  pre = 0.0;
  hum = 0.0;
  VOC = 0.0;
  short PMSfails = 0;
  PM1 = 0;
  PM25 = 0;
  PM10 = 0;
  short MICSfails = 0;
  MICS_CO = 0.0;
  MICS_NO2 = 0.0;
  MICS_NH3 = 0.0;
  short O3fails = 0;
  ozone = 0.0;
  MSP = -1; // reset to -1 to distinguish from grey (0)
  bool senserrs[4] = {false, false, false, false};
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  //------------------------------------------------------------------------
  //++++++++++++++++ READING SENSORS FOR AVERAGE ++++++++++++++++++++++++++++++


  //++++++++++++++++ MAIN MEASUREMENTS LOOP ++++++++++++++++++++++++++++++
  for (int k = 0; k < avg_measurements; k++) {

    int curdelay = 0;
    if (PMS_run) {
      curdelay = avg_delay - 45; // 45 seconds compensation for PMS5003
    } else {
      curdelay = avg_delay;
    }

    //+++++++++ NEXT MEASUREMENTS CYCLE DELAY ++++++++++++
    Serial.printf("Wait %d min. and %d sec. for measurement cycle %d of %d\n\n", curdelay / 60, curdelay % 60, k + 1, avg_measurements);
    String cyclemsg = "Cycle " + String(k + 1) + " of " + String(avg_measurements);
    drawCountdown(curdelay, cyclemsg.c_str());
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ WAKE UP AND PREHEAT PMS5003 ++++++++++++
    if (PMS_run) {
      Serial.println("Waking up and preheating PMS5003 sensor for 45 seconds...\n");
      pms.wakeUp();
      drawCountdown(45, "Preheating PMS5003...");
    }
    //++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ MEASUREMENTS MESSAGE ++++++++++++
    log_i("Measurements in progress...\n");
    drawTwoLines("Measurements", "in progress...", 0);
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ READING BME680 ++++++++++++
    if (BME_run) {
      float bmeread = 0.0; // temporary read var
      log_i("Sampling BME680 sensor...");
      while (1) {
        if (!checkBMESensor()) {
          if (errcount > 2) {
            log_e("Error while sampling BME680 sensor!");
            BMEfails++;
            break;
          }
          delay(1000);
          errcount++;
          continue;
        }
        if (!bme680.run()) continue;

        bmeread = bme680.temperature;
        log_v("Temperature(*C): %.3f", bmeread);
        currtemp = bmeread;
        temp += bmeread;

        bmeread = bme680.pressure / 100.0;
        // Normalizing pressure based on sea level altitude and current temperature
        bmeread = (bmeread * pow(1 - (0.0065 * sealevelalt / (currtemp + 0.0065 * sealevelalt + 273.15)), -5.257));
        log_v("Pressure(hPa): %.3f", bmeread);
        currpre = bmeread;
        pre += bmeread;

        bmeread = bme680.humidity;
        log_v("Humidity(perc.): %.3f", bmeread);
        currhum = bmeread;
        hum += bmeread;

        bmeread = bme680.gasResistance / 1000.0;
        bmeread = no2AndVocCompensation(bmeread, currtemp, currpre, currhum); // VOC Compensation
        log_v("Compensated gas resistance(kOhm): %.3f\n", bmeread);
        VOC += bmeread;

        break;
      }
    }
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ READING PMS5003 +++++++++++
    if (PMS_run) {
      log_i("Sampling PMS5003 sensor...");
      errcount = 0;
      while (1) {
        while (pmsSerial.available()) {
          pmsSerial.read(); // Clears buffer (removes potentially old data) before read.
        }
        if (!pms.readUntil(data)) {
          if (errcount > 2) {
            log_e("Error while sampling PMS5003 sensor!");
            PMSfails++;
            break;
          }
          delay(1000);
          errcount++;
          continue;
        }
        log_v("PM1(ug/m3): %d", data.PM_AE_UG_1_0);
        PM1 += data.PM_AE_UG_1_0;

        log_v("PM2,5(ug/m3): %d", data.PM_AE_UG_2_5);
        PM25 += data.PM_AE_UG_2_5;

        log_v("PM10(ug/m3): %d\n", data.PM_AE_UG_10_0);
        PM10 += data.PM_AE_UG_10_0;

        break;
      }
    }
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ READING MICS6814 ++++++++++++
    if (MICS_run) {
      log_i("Sampling MICS6814 sensor...");
      errcount = 0;
      while (1) {
                
        float micsread[3]; // array for temporary readings

        micsread[0] = gas.measureCO();
        micsread[1] = gas.measureNO2();
        micsread[2] = gas.measureNH3();

        if (micsread[0] < 0 || micsread[1] < 0 || micsread[2] < 0) {
          if (errcount > 2) {
            log_e("Error while sampling!");
            MICSfails++;
            break;
          }
          errcount++;
          delay(1000);
          continue;
        }

        micsread[0] = convertPpmToUgM3(micsread[0], MICSmm[0]);
        log_v("CO(ug/m3): %.3f", micsread[0]);
        MICS_CO += micsread[0];

        micsread[1] += MICSOffset[1]; //add ppm offset
        micsread[1] = convertPpmToUgM3(micsread[1], MICSmm[1]);
        if (BME_run) micsread[1] = no2AndVocCompensation(micsread[1], currtemp, currpre, currhum); // NO2 Compensation
        log_v("NOx(ug/m3): %.3f", micsread[1]);
        MICS_NO2 += micsread[1];

        micsread[2] = convertPpmToUgM3(micsread[2], MICSmm[2]);
        log_v("NH3(ug/m3): %.3f\n", micsread[2]);
        MICS_NH3 += micsread[2];

        break;
      }
    }
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ READING O3 ++++++++++++
    if (O3_run) {
      float o3read = 0.0; // temporary read var
      log_i("Sampling O3 sensor...");
      errcount = 0;
      while (1) {
        if (!isAnalogO3Connected()) {
          if (errcount > 2) {
            log_e("Error while sampling O3 sensor!");
            O3fails++;
            break;
          }
          delay(1000);
          errcount++;
          continue;
        }
        o3read = analogUgM3O3Read(&currtemp); // based on the current measured temperature
        log_v("O3(ug/m3): %.3f", o3read);
        ozone += o3read;

        break;
      }
      Serial.println();
    }
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++

    //+++++++++ PUT PMS5003 TO SLEEP ++++++++++++
    if (PMS_run) {
      log_i("Putting PMS5003 sensor to sleep...\n");
      pms.sleep();
      delay(1500);
    }
    //++++++++++++++++++++++++++++++++++++++++++++

  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------

  if (BME_run || PMS_run || MICS_run || MICS4514_run || O3_run) performAverages(BMEfails, PMSfails, MICSfails, O3fails, senserrs);

  // MSP# Index evaluation
  MSP = evaluateMSPIndex(PM25, MICS_NO2, ozone); // implicitly casting PM25 as float for MSP evaluation only

  if (cfg_ok) connAndGetTime();

  printMeasurementsOnSerial();

  drawMeasurements(); // draw on display

  if (server_ok && connected_ok && datetime_ok) connectToServer((use_modem) ? &gsmclient : &wificlient);

  if (SD_ok) {
    if (checkLogFile()) {
      logToSD();
    }
  }

  for (short i = 0; i < 4; i++) { // restore sensors from errors
    if (senserrs[i] == true) {
      switch (i) {
        case 0:
          BME_run = true;
          break;
        case 1:
          PMS_run = true;
          break;
        case 2:
          MICS_run = true;
          break;
        case 3:
          O3_run = true;
          break;
      }
    }
  }

}