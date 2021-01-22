//
//  Progetto MILANO SMART PARK - Parco Segantini
//  Firmware N per ESP32-DevkitC con ESP32-WROVER-B
//  Versioni originali by Luca Crotti
//  Versioni N by Norman Mulinacci
//
//  Librerie richieste: Pacchetto esp32 per Arduino, BSEC Software Library, PMS_Extended Library (custom), MiCS6814-I2C Library (custom), Winsen ZE-O3 Library (custom), U8g2 Library
//

//Basic system libraries
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <Wire.h>

// WiFi and NTP time management libraries
#include <WiFi.h>
#include "time.h"

// Sensors management libraries
//for BME_680
#include "bsec.h"
#include <EEPROM.h>
//for PMS5003
#include "PMS.h"
//for MICS6814
#include "MiCS6814-I2C.h"
//for ZE25-O3
#include "WinsenZE03.h"

// OLED display library
#include "U8g2lib.h"


String ver = "2.0"; //current firmware version


//++++++++++++++++++++++ DEBUG enable ++++++++++++++++++++++++++++++++
bool DEBBUG = false;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// i2c bus pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Software UART definitions for PMS5003 and ZE25-O3
// modes (UART0=0; UART1=1; UART2=2)
HardwareSerial O3Serial(1);
HardwareSerial pmsSerial(2);

// BME680, PMS5003, MICS6814 and ZE25-O3 sensors instances
Bsec iaqSensor;
PMS pms(pmsSerial);
MiCS6814 gas;
WinsenZE03 O3sens;

// Instance for the OLED 1.3" display with the SH1106 controller
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);   // ESP32 Thing, HW I2C with pin remapping

// Reboots after sleep counter
//RTC_DATA_ATTR int bootCount = 0;

// Define WiFi Client
WiFiClient client;

//Define conversion factor from micro seconds to minutes
#define uS_TO_M_FACTOR 60000000

// Network and system setup variables
bool SD_ok;
bool cfg_ok;
bool ssid_ok;
bool connesso_ok;
bool dataora_ok;
bool invio_ok;
String ssid = "";
String pwd = "";
String codice = "";
//String splash = "";
String logpath = "";
wifi_power_t wifipow;
//int sleep_time = 0;
int attesa = 0;
int avg_measurements = 0;
int avg_delay = 0;
bool iaqon;

// Variables and structures for BME680
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};
#define STATE_SAVE_PERIOD  UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
float hum = 0.0;
float temp = 0.0;
float pre = 0.0;
float VOC = 0.0;
float sealevelalt = 0.0;

// Variables and structure for PMS5003
PMS::DATA data;
float PM1 = 0.0;
float PM10 = 0.0;
float PM25 = 0.0;

// Variables for MICS6814
float MICS6814_NH3   = 0.0;
float MICS6814_CO   = 0.0;
float MICS6814_NO2   = 0.0;
float MICS6814_C3H8  = 0.0;
float MICS6814_C4H10 = 0.0;
float MICS6814_CH4   = 0.0;
float MICS6814_H2    = 0.0;
float MICS6814_C2H5OH = 0.0;

// Variables for ZE25-O3
float ozone = 0.0;

// Variables for MQ-7
const float Res1 = 5000.0;
const float rangeV = 4096.0;
float COppm = 0.0;

// Server time management
String dayStamp = "";
String timeStamp = "";

// Sensor active variables
bool BME680_run;
bool PMS_run;
bool MICS6814_run;
bool O3_run;
bool MQ7_run;

//String to store the MAC Address
String macAdr = "";


//+++++++++++++++++  I C O N E  D I  S I S T E M A  +++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static unsigned char msp_icon64x64[] = {
  0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x80, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xFF, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x1F, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xE0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x03, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x07, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x1F, 0xE0, 0x0F, 0x00,
  0x00, 0x00, 0x00, 0x80, 0x7F, 0xC0, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x80,
  0xFF, 0x81, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0x3F, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xFC, 0x07, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xE0, 0x1F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x1F, 0xF8, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x7E, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x80, 0x07, 0xFC, 0xE0, 0x03,
  0x00, 0x00, 0x00, 0xC0, 0x1F, 0xF8, 0xC1, 0x07, 0x00, 0x00, 0x00, 0xC0,
  0x7F, 0xF0, 0xC3, 0x07, 0x00, 0x00, 0x00, 0x80, 0x7F, 0xE0, 0x83, 0x0F,
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xC1, 0x87, 0x0F, 0x00, 0x00, 0x3E, 0x00,
  0xFC, 0xC1, 0x07, 0x1F, 0x00, 0x80, 0xFF, 0x00, 0xF0, 0x83, 0x0F, 0x1F,
  0x00, 0xC0, 0xFF, 0x01, 0xE0, 0x07, 0x0F, 0x1E, 0x00, 0xE0, 0xFF, 0x01,
  0xC0, 0x07, 0x1F, 0x3E, 0x00, 0xFF, 0xFF, 0x7F, 0x80, 0x0F, 0x1E, 0x3C,
  0xC0, 0xFF, 0xFF, 0xFF, 0x00, 0x1F, 0x3E, 0x7C, 0xE0, 0xFF, 0xFF, 0xFF,
  0x03, 0x1F, 0x3E, 0x7C, 0xF0, 0xFF, 0xFF, 0xFF, 0x03, 0x1E, 0x3E, 0x7C,
  0xF0, 0xFF, 0xFF, 0xFF, 0x07, 0x3E, 0x7C, 0x78, 0xF8, 0xFF, 0xFF, 0xFF,
  0x07, 0x3E, 0x7C, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0x07, 0x3E, 0x7C, 0xF8,
  0xF8, 0xFF, 0xFF, 0xFF, 0x0F, 0x3E, 0x7C, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF,
  0x07, 0x7E, 0x7C, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0x07, 0x3E, 0x7C, 0xF8,
  0xFC, 0xFF, 0xFF, 0xFF, 0x1F, 0x3E, 0x78, 0xF0, 0xFE, 0xFF, 0xFF, 0xFF,
  0x1F, 0x3E, 0xFC, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0x3F, 0x3C, 0x78, 0xF0,
  0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0x7C, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF,
  0x7F, 0x00, 0x7C, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x7C, 0xF0,
  0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x78, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF,
  0x3F, 0x00, 0x10, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0x00, 0xF8,
  0xFE, 0xFF, 0xFF, 0xFF, 0x1F, 0x00, 0x00, 0xF8, 0xFC, 0xFF, 0xFF, 0xFF,
  0x1F, 0x00, 0x00, 0xF0, 0xF8, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x20,
  0xF8, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF,
  0x07, 0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00,
  0xF8, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0xFF,
  0x07, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00,
  0xE0, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0xFF, 0xFF,
  0x01, 0x00, 0x00, 0x00, 0xC0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xFE, 0xFF, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xFF, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char wifi1_icon16x16[] = {
  0b00000000, 0b00000000, //
  0b11100000, 0b00000111, //      ######
  0b11111000, 0b00011111, //    ##########
  0b11111100, 0b00111111, //   ############
  0b00001110, 0b01110000, //  ###        ###
  0b11100110, 0b01100111, //  ##  ######  ##
  0b11110000, 0b00001111, //     ########
  0b00011000, 0b00011000, //    ##      ##
  0b11000000, 0b00000011, //       ####
  0b11100000, 0b00000111, //      ######
  0b00100000, 0b00000100, //      #    #
  0b10000000, 0b00000001, //        ##
  0b10000000, 0b00000001, //        ##
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char arrow_up_icon16x16[] = {
  0b10000000, 0b00000001, //        ##
  0b11000000, 0b00000011, //       ####
  0b11100000, 0b00000111, //      ######
  0b11110000, 0b00001111, //     ########
  0b01111000, 0b00011110, //    ####  ####
  0b00111100, 0b00111100, //   ####    ####
  0b00011110, 0b01111000, //  ####      ####
  0b00111111, 0b11111100, // ######    ######
  0b00111111, 0b11111100, // ######    ######
  0b00111110, 0b01111100, //  #####    #####
  0b00111000, 0b00011100, //    ###    ###
  0b00111000, 0b00011100, //    ###    ###
  0b00111000, 0b00011100, //    ###    ###
  0b11111000, 0b00011111, //    ##########
  0b11111000, 0b00011111, //    ##########
  0b11110000, 0b00001111, //     ########
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char blank_icon16x16[] = {
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char nocon_icon16x16[] = {
  0b00000000, 0b00000000, //
  0b11100000, 0b00000011, //       #####
  0b11111000, 0b00001111, //     #########
  0b11111100, 0b00011111, //    ###########
  0b00111110, 0b00111110, //   #####   #####
  0b01111110, 0b00111000, //   ###    ######
  0b11111111, 0b01110000, //  ###    ########
  0b11110111, 0b01110001, //  ###   ##### ###
  0b11000111, 0b01110011, //  ###  ####   ###
  0b10000111, 0b01110111, //  ### ####    ###
  0b00001110, 0b00111111, //   ######    ###
  0b00011110, 0b00111110, //   #####    ####
  0b11111100, 0b00011111, //    ###########
  0b11111000, 0b00001111, //     #########
  0b11100000, 0b00000011, //       #####
  0b00000000, 0b00000000, //
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char sd_icon16x16[] = {
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b11111110, 0b01111111, //  ##############
  0b11111111, 0b11111111, // ################
  0b11111111, 0b11000011, // ##    ##########
  0b11111111, 0b11111111, // ################
  0b11111111, 0b11000011, // ##    ##########
  0b11111111, 0b11111111, // ################
  0b11111111, 0b11000011, // ##    ##########
  0b11111111, 0b11111111, // ################
  0b11111111, 0b11000011, // ##    ##########
  0b11111111, 0b11111111, // ################
  0b11111111, 0b01111111, //  ###############
  0b00111111, 0b00000111, //      ###  ######
  0b00011110, 0b00000011, //       ##   ####
  0b00000000, 0b00000000, //
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
unsigned char clock_icon16x16[] = {
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //
  0b11100000, 0b00000011, //       #####
  0b11110000, 0b00000111, //      #######
  0b00011000, 0b00001100, //     ##     ##
  0b00001100, 0b00011000, //    ##       ##
  0b00000110, 0b00110000, //   ##         ##
  0b00000110, 0b00110000, //   ##         ##
  0b11111110, 0b00110000, //   ##    #######
  0b10000110, 0b00110000, //   ##    #    ##
  0b10000110, 0b00110000, //   ##    #    ##
  0b10001100, 0b00011000, //    ##   #   ##
  0b00011000, 0b00001100, //     ##     ##
  0b11110000, 0b00000111, //      #######
  0b11100000, 0b00000011, //       #####
  0b00000000, 0b00000000, //
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//*****************************************************************
//************** F U N Z I O N I   A U S I L I A R I E ************
//*****************************************************************

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void drawScrHead() { //disegna l'header dello schermo con tutte le icone

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);

  // stato del sistema
  u8g2.setCursor(0, 13); u8g2.print("#" + codice + "#");

  if (dataora_ok) {
    u8g2.drawXBMP(52, 0, 16, 16, clock_icon16x16);
  } else {
    u8g2.drawXBMP(52, 0, 16, 16, blank_icon16x16);
  }
  if (SD_ok) {
    u8g2.drawXBMP(72, 0, 16, 16, sd_icon16x16);
  } else {
    u8g2.drawXBMP(72, 0, 16, 16, blank_icon16x16);
  }
  if (connesso_ok) {
    u8g2.drawXBMP(112, 0, 16, 16, wifi1_icon16x16);
  } else {
    u8g2.drawXBMP(112, 0, 16, 16, nocon_icon16x16);
  }

  u8g2.drawLine(0, 17, 127, 17);

}// end of drawScrHead()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
String floatToComma(float value) { //Converts float values in strings with the decimal part separated from the integer part by a comma

  String convert = String(value, 3);
  convert.replace(".", ",");
  return convert;

}// end of floatToComma()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void displayMeasures() { //prints data on the U8g2 screen, on four pages

  if (DEBBUG) Serial.println("...aggiorno i dati del display...");

  // pagina 1
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("Temp:  " + floatToComma(temp) + "*C");
  u8g2.setCursor(5, 39); u8g2.print("Hum:  " + floatToComma(hum) + "%");
  u8g2.setCursor(5, 50); u8g2.print("Pre:  " + floatToComma(pre) + "hPa");
  u8g2.setCursor(5, 61); u8g2.print("PM10:  " + floatToComma(PM10) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 2
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("PM2,5:  " + floatToComma(PM25) + "ug/m3");
  u8g2.setCursor(5, 39); u8g2.print("PM1:  " + floatToComma(PM1) + "ug/m3");
  u8g2.setCursor(5, 50); u8g2.print("NOx:  " + floatToComma(MICS6814_NO2) + "ppm");
  u8g2.setCursor(5, 61); u8g2.print("CO:  " + floatToComma(MICS6814_CO) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 3
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("O3:  " + floatToComma(ozone) + "ppm");
  if (iaqon) {
    u8g2.setCursor(5, 39); u8g2.print("IAQ:  " + floatToComma(VOC));
  } else {
    u8g2.setCursor(5, 39); u8g2.print("VOC:  " + floatToComma(VOC) + "kOhm");
  }
  u8g2.setCursor(5, 50); u8g2.print("NH3:  " + floatToComma(MICS6814_NH3) + "ppm");
  u8g2.setCursor(5, 61); u8g2.print("C3H8:  " + floatToComma(MICS6814_C3H8) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 4
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("C4H10:  " + floatToComma(MICS6814_C4H10) + "ppm");
  u8g2.setCursor(5, 39); u8g2.print("CH4:  " + floatToComma(MICS6814_CH4) + "ppm");
  u8g2.setCursor(5, 50); u8g2.print("H2:  " + floatToComma(MICS6814_H2) + "ppm");
  u8g2.setCursor(5, 61); u8g2.print("C2H5OH:  " + floatToComma(MICS6814_C2H5OH) + "ppm");
  //u8g2.print("CO(MQ-7):  " + floatToComma(COppm) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

}// end of displayMeasures()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool initializeSD() {

  if (!SD.begin()) {
    delay(5000);
    if (!SD.begin()) {
      Serial.println("Errore lettore SD CARD!");
      return false;
    }
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("LETTORE SD CARD inizializzato, CARD non presente - controllare!");
    return false;
  }
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("TIPO CARD NON IDENTIFICATO - formattare la SD CARD");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("Dimensione SD CARD: %lluMB\n", cardSize);
  Serial.println();
  delay(300);
  return true;

}// end of initializeSD()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void appendFile(fs::FS &fs, String path, String message) {

  if (DEBBUG) {
    Serial.print("Appending to file: "); Serial.println(path);
  }
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    if (DEBBUG) Serial.println("Failed to open file for appending");
    return;
  }
  if (file.println(message)) {
    if (DEBBUG) Serial.println("Message appended");
  } else {
    if (DEBBUG) Serial.println("Append failed");
  }
  file.close();

}// end of appendFile()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool addToLog(fs::FS &fs, String path, String message) { //adds new line to the log file at the top, after the header lines

  String temp = "";
  if (DEBBUG) Serial.println("ADDTOLOG: Log file is: " + path);
  File logfile = fs.open(path, FILE_READ);
  if (!logfile) {
    if (DEBBUG) Serial.println("ADDTOLOG: Error opening the log file!");
    return false;
  }
  File tempfile = fs.open("/templog", FILE_WRITE);
  if (!tempfile) {
    if (DEBBUG) Serial.println("ADDTOLOG: Error creating templog!");
    logfile.close();
    return false;
  }
  while (logfile.available()) { // copies entire log file into temp log
    temp = logfile.readStringUntil('\r'); //reads until carriage return character
    tempfile.print(temp);
    tempfile.print('_');
    temp = logfile.readStringUntil('\n'); //discarding line feed character
  }
  logfile.close();
  tempfile.close();
  fs.remove(path); // deletes log file
  logfile = fs.open(path, FILE_WRITE); // recreates empty logfile
  if (!logfile) {
    if (DEBBUG) Serial.println("ADDTOLOG: Error recreating the log file!");
    return false;
  }
  tempfile = fs.open("/templog", FILE_READ);
  if (!tempfile) {
    if (DEBBUG) Serial.println("ADDTOLOG: Error reopening templog!");
    return false;
  }
  for (int i = 0; i < 2; i++) { // copying the header lines from tempfile
    temp = tempfile.readStringUntil('_');
    logfile.println(temp);
  }
  logfile.println(message); // printing the new line
  while (tempfile.available()) { // copying the remaining strings
    temp = tempfile.readStringUntil('_');
    logfile.println(temp);
  }
  tempfile.close();
  logfile.close();
  fs.remove("/templog");

  return true;

}// end of addToLog()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool parseConfig(File fl) {

  bool esito = true;
  String command[10];
  String temp;
  int i = 0;
  unsigned long lastpos = 0;
  if (DEBBUG) Serial.println();
  // Storing the config file in a string array
  while (fl.available() && i < 10) {
    fl.seek(lastpos);
    if (i == 0) temp = fl.readStringUntil('#');
    command[i] = fl.readStringUntil(';');
    temp = fl.readStringUntil('#');
    if (DEBBUG) {
      Serial.printf("PARSECFG: Stringa numero %d: ", i + 1);
      Serial.println(command[i]);
    }
    lastpos = fl.position();
    i++;
  }
  fl.close();
  // Importing variables from the string array
  //ssid
  if (command[0].startsWith("ssid", 0)) {
    ssid = command[0].substring(command[0].indexOf("ssid") + 5, command[0].length());
    Serial.print("ssid = *"); Serial.print(ssid); Serial.println("*");
    if (ssid.length() == 0) {
      Serial.println("PARSECFG: Errore! SSID assente.");
      esito = false;
    }
  } else {
    Serial.println("PARSECFG: Errore! Comando ssid non riconosciuto.");
    esito = false;
  }
  //pwd
  if (command[1].startsWith("password", 0)) {
    pwd = command[1].substring(command[1].indexOf("password") + 9, command[1].length());
    Serial.print("pwd = *"); Serial.print(pwd); Serial.println("*");
    if (pwd.length() == 0) {
      Serial.println("PARSECFG: Errore! Password assente.");
      esito = false;
    }
  } else {
    Serial.println("PARSECFG: Errore! Comando pwd non riconosciuto.");
    esito = false;
  }
  //codice
  if (command[2].startsWith("codice", 0)) {
    codice = command[2].substring(command[2].indexOf("codice") + 7, command[2].length());
    Serial.print("codice = *"); Serial.print(codice); Serial.println("*");
    if (codice.length() == 0) {
      Serial.println("PARSECFG: Errore! Codice assente.");
      esito = false;
    }
  } else {
    Serial.println("PARSECFG: Errore! Comando codice non riconosciuto.");
    esito = false;
  }
  /*
    //splash
    if (command[3].startsWith("splash", 0)) {
    splash = command[3].substring(command[3].indexOf("splash") + 7, command[3].length());
    Serial.print("splash = *"); Serial.print(splash); Serial.println("*");
    if (splash.length() == 0) {
      Serial.println("PARSECFG: Errore! Splash assente.");
      esito = false;
    }
    } else {
    Serial.println("PARSECFG: Errore! Comando splash non riconosciuto.");
    esito = false;
    }
  */
  //potenza_wifi
  if (command[3].startsWith("potenza_wifi", 0)) {
    temp = "";
    temp = command[3].substring(command[3].indexOf("potenza_wifi") + 13, command[3].length());
    if (temp.indexOf("19.5dBm") == 0) {
      wifipow = WIFI_POWER_19_5dBm;
    } else if (temp.indexOf("19dBm") == 0) {
      wifipow = WIFI_POWER_19dBm;
    } else if (temp.indexOf("18.5dBm") == 0) {
      wifipow = WIFI_POWER_18_5dBm;
    } else if (temp.indexOf("17dBm") == 0) {
      wifipow = WIFI_POWER_17dBm;
    } else if (temp.indexOf("15dBm") == 0) {
      wifipow = WIFI_POWER_15dBm;
    } else if (temp.indexOf("13dBm") == 0) {
      wifipow = WIFI_POWER_13dBm;
    } else if (temp.indexOf("11dBm") == 0) {
      wifipow = WIFI_POWER_11dBm;
    } else if (temp.indexOf("8.5dBm") == 0) {
      wifipow = WIFI_POWER_8_5dBm;
    } else if (temp.indexOf("7dBm") == 0) {
      wifipow = WIFI_POWER_7dBm;
    } else if (temp.indexOf("5dBm") == 0) {
      wifipow = WIFI_POWER_5dBm;
    } else if (temp.indexOf("2dBm") == 0) {
      wifipow = WIFI_POWER_2dBm;
    } else if (temp.indexOf("-1dBm") == 0) {
      wifipow = WIFI_POWER_MINUS_1dBm;
    } else {
      Serial.println("PARSECFG: Errore! Valore di potenza_wifi non valido. Fallback a 19.5dBm.");
      wifipow = WIFI_POWER_19_5dBm;
    }
    Serial.print("potenza_wifi = *"); Serial.print(wifipow); Serial.println("*");
  } else {
    Serial.println("PARSECFG: Errore! Comando potenza_wifi non riconosciuto. Fallback a 19.5dBm.");
    wifipow = WIFI_POWER_19_5dBm;
  }
  /*
    //sleep_time
    if (command[5].startsWith("sleep(minuti)", 0)) {
    temp = "";
    temp = command[5].substring(command[5].indexOf("sleep(minuti)") + 14, command[5].length());
    sleep_time = temp.toInt();
    Serial.printf("sleep_time = *%d*\n", sleep_time);
    } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando sleep_time non riconosciuto. Fallback a 1 minuto.");
    sleep_time = 1;
    }
  */
  //attesa
  if (command[4].startsWith("attesa(minuti)", 0)) {
    temp = "";
    temp = command[4].substring(command[4].indexOf("attesa(minuti)") + 15, command[4].length());
    attesa = temp.toInt();
    Serial.printf("attesa = *%d*\n", attesa);
  } else {
    Serial.println("PARSECFG: Errore! Comando attesa non riconosciuto. Fallback a 20 minuti.");
    attesa = 20;
  }
  //avg_measurements
  if (command[5].startsWith("numero_misurazioni_media", 0)) {
    temp = "";
    temp = command[5].substring(command[5].indexOf("numero_misurazioni_media") + 25, command[5].length());
    avg_measurements = temp.toInt();
    Serial.printf("avg_measurements = *%d*\n", avg_measurements);
  } else {
    Serial.println("PARSECFG: Errore! Comando avg_measurements non riconosciuto. Fallback a 5 misurazioni.");
    avg_measurements = 5;
  }
  //avg_delay
  if (command[6].startsWith("ritardo_media(secondi)", 0)) {
    temp = "";
    temp = command[6].substring(command[6].indexOf("ritardo_media(secondi)") + 23, command[6].length());
    avg_delay = temp.toInt();
    Serial.printf("avg_delay = *%d*\n", avg_delay);
  } else {
    Serial.println("PARSECFG: Errore! Comando avg_delay non riconosciuto. Fallback a 6 secondi.");
    avg_delay = 6;
  }
  //MQ7_run
  if (command[7].startsWith("attiva_MQ7", 0)) {
    temp = "";
    temp = command[7].substring(command[7].indexOf("attiva_MQ7") + 11, command[7].length());
    if (temp.toInt() == 0 || temp.toInt() == 1) {
      MQ7_run = temp.toInt();
    } else {
      Serial.println("PARSECFG: Errore! Valore di attiva_MQ7 non valido. Fallback a false.");
      MQ7_run = false;
    }
    Serial.printf("MQ7_run = %s\n", MQ7_run ? "true" : "false");
  } else {
    Serial.println("PARSECFG: Errore! Comando attiva_MQ7 non riconosciuto. Fallback a false.");
    MQ7_run = false;
  }
  //iaqon
  if (command[8].startsWith("abilita_IAQ", 0)) {
    temp = "";
    temp = command[8].substring(command[8].indexOf("abilita_IAQ") + 12, command[8].length());
    if (temp.toInt() == 0 || temp.toInt() == 1) {
      iaqon = temp.toInt();
    } else {
      Serial.println("PARSECFG: Errore! Valore di abilita_IAQ non valido. Fallback a true.");
      iaqon = true;
    }
    Serial.printf("iaqon = %s\n", iaqon ? "true" : "false");
  } else {
    Serial.println("PARSECFG: Errore! Comando abilita_IAQ non riconosciuto. Fallback a true.");
    iaqon = true;
  }
  //sealevelalt
  if (command[9].startsWith("altitudine_s.l.m.", 0)) {
    temp = "";
    temp = command[9].substring(command[9].indexOf("altitudine_s.l.m.") + 18, command[9].length());
    sealevelalt = temp.toFloat();
    Serial.printf("sealevelalt = *%.2f*\n", sealevelalt);
  } else {
    Serial.println("PARSECFG: Errore! Comando altitudine_s.l.m. non riconosciuto.");
  }

  return esito;

}// end of parseConfig()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void setDefaults() {

  wifipow = WIFI_POWER_19_5dBm;
  attesa = 20;
  avg_measurements = 5;
  avg_delay = 6;
  MQ7_run = false;
  iaqon = true;

}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void initWifi() {

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  delay(1000); // Waiting a bit for Wifi mode set
  if (DEBBUG) Serial.printf("wifipow: %d\n", wifipow);
  WiFi.setTxPower(wifipow);
  Serial.printf("Potenza Wifi impostata a %d\n", WiFi.getTxPower());
  Serial.println("Legenda: -4(-1dBm), 8(2dBm), 20(5dBm), 28(7dBm), 34(8.5dBm), 44(11dBm), 52(13dBm), 60(15dBm), 68(17dBm), 74(18.5dBm), 76(19dBm), 78(19.5dBm)");
  Serial.println();
  if (cfg_ok) {
    for (int m = 0; m < 4; m++) {
      for (int k = 0; k < 4; k++) {
        Serial.println("Inizio scansione WiFi...");
        drawScrHead();
        u8g2.drawStr(0, 30, "SCANSIONE della rete:");
        u8g2.sendBuffer();
        delay(100);
        // WiFi.scanNetworks will return the number of networks found
        int n = WiFi.scanNetworks();
        Serial.println("Scansione completata.");
        delay(300);
        u8g2.setFont(u8g2_font_6x13_tf); u8g2.drawStr(3, 42, "SCANSIONE COMPLETATA"); u8g2.sendBuffer();
        delay(1000);
        if (n == 0) {
          Serial.println("no networks found");
          Serial.printf("%d retryes left\n", 3 - k);
          u8g2.setFont(u8g2_font_6x13_tf); u8g2.drawStr(8, 55, "NESSUNA RETE!"); u8g2.sendBuffer();
          if (k < 3) {
            Serial.println("Trying again...");
            drawScrHead();
            u8g2.drawStr(35, 45, "RIPROVO...");
            u8g2.sendBuffer();
            delay(2000);
            continue;
          }
          delay(1000);
        } else {
          Serial.print(n);
          Serial.println(" reti trovate.");
          drawScrHead();
          delay(200);
          drawScrHead();
          u8g2.setCursor(5, 35); u8g2.print("RETI TROVATE:"); u8g2.setCursor(95, 35); u8g2.print(String(n));
          u8g2.sendBuffer();
          delay(100);
          for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1); Serial.print(": ");
            Serial.print(WiFi.SSID(i)); Serial.print(" ("); Serial.print(WiFi.RSSI(i)); Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "---OPEN---" : "***");
            if (WiFi.SSID(i) == ssid) {
              ssid_ok = true;
            }
            delay(100);
          }
          Serial.println();
          break;
        }
      }
      if (cfg_ok && ssid_ok) {
        Serial.print(ssid); Serial.println(" trovata!");
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setCursor(5, 55); u8g2.print(ssid + " OK!");
        u8g2.sendBuffer();
        delay(4000);
        //lancio la connessione wifi
        Serial.print("MI CONNETTO A: ");
        Serial.println(ssid);
        drawScrHead();
        u8g2.drawStr(5, 30, "Connessione a: "); u8g2.drawStr(8, 42, ssid.c_str());
        u8g2.sendBuffer();
        int ritento = 1;
        int num_volte = 1;
        // -------CONNESSIONE WIFI----------
        WiFi.begin(ssid.c_str() + '\0', pwd.c_str() + '\0');
        delay(500);
        // ciclo di attesa connessione...
        while (WiFi.status() != WL_CONNECTED) {
          delay(5000);
          Serial.print(".");
          u8g2.drawStr((7 * num_volte) + (2 * ritento), 54, ". "); u8g2.sendBuffer();
          ritento = ritento + 1;
          if (ritento >= 5) {  // qui attendo fino a 2 secondi di avere connessione
            WiFi.disconnect();
            delay(2000);
            WiFi.begin(ssid.c_str(), pwd.c_str());
            ritento = 1;
            num_volte = num_volte + 1;
            delay(3000);
          }// fine if ritenta
          if (num_volte >= 5) {
            Serial.println("***** impossibile connettersi al wifi - riprovo tra 1 minuto...");
            drawScrHead();
            u8g2.drawStr(15, 35, "WiFi NON Connesso"); u8g2.drawStr(5, 55, "Riprovo tra poco...");
            u8g2.sendBuffer();
            ritento = 1;
            num_volte = 1;
            break;
          }// fine IF TIMEOUT ARRIVATO
        }// fine WHILE esco da loop se wifi connesso... o per timeout
        // aggiorno lo stato se WIFI connesso
        if (WiFi.status() == WL_CONNECTED) {
          connesso_ok = true;
          Serial.println("\nWiFi CONNESSO....");
          drawScrHead();
          u8g2.drawStr(15, 45, "WiFi Connesso");
          u8g2.sendBuffer();
          delay(2000);
          //lancio connessione a server orario e sincronizzo l'ora
          syncNTPTime();
        } else {
          connesso_ok = false;
          dataora_ok = false;
          Serial.println("\nWiFi NON Connesso.");
          drawScrHead();
          u8g2.drawStr(15, 45, "WiFi NON Connesso");
          u8g2.sendBuffer();
          delay(2000);
        }
        break;
      } else if (!ssid_ok) {
        Serial.print("Errore! "); Serial.print(ssid); Serial.println(" non trovata!");
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setCursor(5, 55); u8g2.print("NO " + ssid + "!");
        u8g2.sendBuffer();
        delay(4000);
        if (m < 3) {
          Serial.println("Riprovo, tentativo " + String(m + 1) + " di 3");
          drawScrHead();
          u8g2.drawStr(35, 35, "RIPROVO...");
          u8g2.setCursor(28, 55); u8g2.print("TENT. " + String(m + 1) + " di 3");
          u8g2.sendBuffer();
          delay(3000);
          continue;
        }
        Serial.println("Nessuna connessione!"); Serial.println();
        drawScrHead();
        u8g2.setCursor(5, 45); u8g2.print("NESSUNA CONNESSIONE!");
        u8g2.sendBuffer();
        delay(3000);
      }
    }
  }

}// end of initWifi()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void syncNTPTime() {   //Stores time and date in a convenient format

  dayStamp = "";
  timeStamp = "";
  //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer), Italy is GMT+1, DST is +1hour
  configTime(3600, 3600, "pool.ntp.org");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    dataora_ok = false;
    return;
  }
  char Date[11], Time[9];
  strftime(Date, 11, "%d/%m/%Y", &timeinfo);
  strftime(Time, 9, "%T", &timeinfo);
  dayStamp = String(Date);
  timeStamp = String(Time);
  dataora_ok = true;
  Serial.print("Data e ora correnti: "); Serial.print(dayStamp); Serial.print(" "); Serial.println(timeStamp);
  drawScrHead();
  u8g2.drawStr(15, 45, "Data e ora ok");
  u8g2.sendBuffer();
  delay(2000);

}// end of syncNTPTime()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool CheckSensor() { //Checks BME680 status

  if (iaqSensor.status < BSEC_OK) {
    if (DEBBUG) Serial.printf("BSEC error, status %d!\n", iaqSensor.status);
    return false;
  } else if (iaqSensor.status > BSEC_OK && DEBBUG) {
    Serial.printf("BSEC warning, status %d!\n", iaqSensor.status);
  }

  if (iaqSensor.bme680Status < BME680_OK) {
    if (DEBBUG) Serial.printf("Sensor error, bme680_status %d!\n", iaqSensor.bme680Status);
    return false;
  } else if (iaqSensor.bme680Status > BME680_OK && DEBBUG) {
    Serial.printf("Sensor warning, status %d!\n", iaqSensor.bme680Status);
  }

  iaqSensor.status = BSEC_OK;
  return true;

}// end of CheckSensor()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void loadState() { //Loads BME680 state from ESP32 EEPROM

  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    if (DEBBUG) Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      if (DEBBUG) Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
  } else {
    // Erase the EEPROM with zeroes
    if (DEBBUG) Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }

}// end of loadState()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void updateState(void) { //Stores BME680 state to ESP32 EEPROM

  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) {
    if (iaqSensor.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);

    if (DEBBUG) Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);
      if (DEBBUG) Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }

}// end of updateState()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++





//*******************************************************************************************************************************
//******************************************  S E T U P  ************************************************************************
//*******************************************************************************************************************************
void setup() {

  // INIZIALIZZO EEPROM, I2C, DISPLAY, SERIALE ++++++++++++++++++++++++++++++++++++
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
  Serial.begin(115200);
  delay(2000);// serve per dare tempo alla seriale di attivarsi


  // INIZIO ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // HELLO via seriale
  Serial.println(); Serial.println("MILANO SMART PARK");
  Serial.print("FIRMWARE N - VERSIONE "); Serial.println(ver);
  Serial.println("by Norman Mulinacci, 2020"); Serial.println();
  // SPLASH a schermo
  u8g2.firstPage();
  u8g2.clearBuffer();
  u8g2.drawXBM(0, 0, 64, 64, msp_icon64x64);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(74, 10, "Milano"); u8g2.drawStr(74, 23, "Smart"); u8g2.drawStr(74, 36, "Park");
  u8g2.setCursor(74, 62); u8g2.print("N v" + ver);
  u8g2.sendBuffer();
  delay(5000);
  u8g2.clearBuffer();
  u8g2.drawXBM(0, 0, 64, 64, msp_icon64x64);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(74, 23, "by"); u8g2.drawStr(74, 36, "Norman M."); u8g2.drawStr(74, 49, "2020");
  u8g2.sendBuffer();
  delay(2000);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // INIZIALIZZAZIONE SD CARD E FILE DI CONFIGURAZIONE+++++++++++++++++++++++++++++++++++++++++++++++++
  SD_ok = initializeSD();
  Serial.println();
  delay(100);
  if (SD_ok == true) {
    File cfgfile;
    if (SD.exists("/config_v2.cfg")) {
      cfgfile = SD.open("/config_v2.cfg", FILE_READ);// apri il file in lettura
      Serial.println("File di configurazione aperto.\n");
      cfg_ok = parseConfig(cfgfile);
      if (cfg_ok) {
        Serial.println("File di configurazione ok!");
      } else {
        Serial.println("Errore durante la lettura del file di configurazione!");
        drawScrHead();
        u8g2.drawStr(5, 35, "ERRORE LETTURA FILE!");
        u8g2.drawStr(25, 55, "..NO WEB..");
        u8g2.sendBuffer();
      }
    } else {
      Serial.println("Nessun file di configurazione trovato!");
      drawScrHead();
      u8g2.drawStr(5, 35, "NESSUN CFG TROVATO!");
      u8g2.drawStr(25, 55, "..NO WEB..");
      u8g2.sendBuffer();
      Serial.println("Creo un file configurazione di base...");
      cfgfile = SD.open("/config_v2.cfg", FILE_WRITE);// apri il file in scrittura
      if (cfgfile) {
        cfgfile.close();
        appendFile(SD, "/config_v2.cfg", "#ssid=;\n#password=;\n#codice=;\n#potenza_wifi=19.5dBm;\n#attesa(minuti)=20;\n#numero_misurazioni_media=5;\n#ritardo_media(secondi)=6;\n#attiva_MQ7=0;\n#abilita_IAQ=1;\n#altitudine_s.l.m.=122.0;\n\n//altitudine_s.l.m. influenza la misura della pressione atmosferica e va adattata a seconda della zona. 122m è l'altitudine di Milano\n//Valori possibili per potenza_wifi: -1, 2, 5, 7, 8.5, 11, 13, 15, 17, 18.5, 19, 19.5 dBm");
        Serial.println("File creato!");
        Serial.println();
        cfgfile = SD.open("/config_v2.cfg", FILE_READ);// apri il file in lettura
        cfg_ok = parseConfig(cfgfile);
      } else {
        Serial.println("Errore nel creare il file cfg!");
        Serial.println();
        cfg_ok = false;
        setDefaults();
      }
    }
  } else { // qui se sd non presente...
    Serial.println("Errore: nessuna scheda SD inserita!");
    drawScrHead();
    u8g2.drawStr(5, 35, "NESSUNA SCHEDA SD!");
    u8g2.drawStr(25, 55, "..NO WEB..");
    u8g2.sendBuffer();
    SD_ok = false;
    cfg_ok = false;
    setDefaults();
  }
  Serial.println();
  delay(2000);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  //+++++++++++++ RECUPERO MAC ADDRESS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  byte mc[6];
  WiFi.macAddress(mc);
  macAdr += String(mc[0], HEX); macAdr += ":";
  macAdr += String(mc[1], HEX); macAdr += ":";
  macAdr += String(mc[2], HEX); macAdr += ":";
  macAdr += String(mc[3], HEX); macAdr += ":";
  macAdr += String(mc[4], HEX); macAdr += ":";
  macAdr += String(mc[5], HEX);
  macAdr.toUpperCase();
  Serial.println("\nMAC: " + macAdr + "\n");
  drawScrHead();
  u8g2.setCursor(28, 35); u8g2.print("MAC ADDRESS:");
  u8g2.setCursor(12, 55); u8g2.print(macAdr);
  u8g2.sendBuffer();
  delay(6000);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // CONTROLLO LOGFILE +++++++++++++++++++++++++++++++++++++
  if (SD_ok == true) {
    logpath = "/LOG_N_" + codice + "_v" + ver + ".csv";
    if (!SD.exists(logpath)) {
      Serial.println("File di log non presente, lo creo...");
      File filecsv = SD.open(logpath, FILE_WRITE);
      if (filecsv) {
        filecsv.close();
        String headertext = "File di log della centralina: " + codice + " | Versione firmware N: " + ver + " | MAC: " + macAdr;
        appendFile(SD, logpath, headertext);
        appendFile(SD, logpath, "Data;Ora;Temp(*C);Hum(%);Pre(hPa);PM10(ug/m3);PM2,5(ug/m3);PM1(ug/m3);NOx(ppm);CO(ppm);O3(ppm);VOC(IAQ/kOhm);NH3(ppm);C3H8(ppm);C4H10(ppm);CH4(ppm);H2(ppm);C2H5OH(ppm);CO(MQ-7)(ppm)");
        Serial.println("File di log creato!");
        Serial.println();
      } else {
        Serial.println("Errore nel creare il file di log!");
        Serial.println();
      }
    } else {
      Serial.println("File di log presente!");
      Serial.println();
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  /*
    //++++++++++++++++++++ CONFIGURAZIONE SLEEP MODE +++++++++++++++++++++++++++++
    //- Incrementa boot_number ad ogni reboot+++++++++
    ++bootCount;
    Serial.println("Avvio numero: " + String(bootCount));
    //visualizza la ragione del reboot
    print_wakeup_reason();
    //First we configure the wake up source
    esp_sleep_enable_timer_wakeup(sleep_time * uS_TO_M_FACTOR);
    Serial.println("ESP32 impostato per lo sleep ogni " + String(sleep_time) + " minuti");
    Serial.println();
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

  // +++++++ RILEVAMENTO E INIZIALIZZAZIONE SENSORI +++++++++++++++

  //messaggio seriale
  Serial.println("Rilevamento sensori in corso...\n");
  //messaggio a schermo
  drawScrHead();
  u8g2.drawStr(8, 45, "Rilevo i sensori...");
  u8g2.sendBuffer();


  // inizializzo BME680 ++++++++++++++++++++++++++++++++++++++++++
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  if (CheckSensor()) {
    Serial.println("Sensore BME680 rilevato, inizializzo...");
    iaqSensor.setConfig(bsec_config_iaq);
    loadState();
    bsec_virtual_sensor_t sensor_list[] = {
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };
    iaqSensor.updateSubscription(sensor_list, sizeof(sensor_list) / sizeof(sensor_list[0]), BSEC_SAMPLE_RATE_LP);
    BME680_run = true;
    Serial.println();
  } else {
    Serial.println("Sensore BME680 non rilevato.\n");
    BME680_run = false;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // inizializzo SENSORE PMS5003 ++++++++++++++++++++++++++++++++++++++++++
  // pmsSerial: with WROVER module don't use UART 2 mode on pins 16 and 17, it crashes!
  pmsSerial.begin(9600, SERIAL_8N1, 14, 12); // baud, type, ESP_RX, ESP_TX
  delay(1500);
  pms.wakeUp(); // Waking up sensor after sleep
  delay(1500);
  if (pms.readUntil(data)) {
    Serial.println("Sensore PMS5003 rilevato, inizializzo...");
    Serial.println();
    PMS_run = true;
    pms.sleep(); // Putting sensor to sleep
    delay(1500);
  } else {
    Serial.println("Sensore PMS5003 non rilevato.");
    Serial.println();
    PMS_run = false;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // inizializzo SENSORE MICS6814 ++++++++++++++++++++++++++++++++++++
  if (gas.begin()) { // Connect to sensor using default I2C address (0x04)
    Serial.println("Sensore MICS6814 rilevato, inizializzo...");
    MICS6814_run = true;
    // accensione riscaldatore e led
    gas.powerOn();
    gas.ledOn();
    if (DEBBUG) {
      Serial.println("\nValori delle resistenze di base del MICS6814:");
      Serial.print("OX: "); Serial.print(gas.getBaseResistance(CH_OX));
      Serial.print(" ; RED: "); Serial.print(gas.getBaseResistance(CH_RED));
      Serial.print(" ; NH3: "); Serial.println(gas.getBaseResistance(CH_NH3));
    }
    Serial.println();
  } else {
    Serial.println("Sensore MICS6814 non rilevato.");
    Serial.println();
    MICS6814_run = false;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // inizializzo SENSORE ZE25-O3 ++++++++++++++++++++++++++++++++++++++++++
  O3Serial.begin(9600, SERIAL_8N1, 26, 25); // baud, type, ESP_RX, ESP_TX
  delay(1500);
  O3sens.begin(&O3Serial, O3); // begin ozone sensor
  O3sens.setAs(QA); //set as Q&A mode
  if (O3sens.readManual() < 0) {
    Serial.println("Sensore ZE25-O3 non rilevato.");
    O3_run = false;
  } else {
    Serial.println("Sensore ZE25-O3 rilevato, inizializzo...");
    O3_run = true;
    if (DEBBUG) Serial.printf("Valore di O3 letto: %.3f\n", O3sens.readManual());
  }
  Serial.println();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // messaggio MQ-7 ++++++++++++++++++++++++++++++++++++++++++++
  if (MQ7_run) {
    Serial.println("Sensore MQ-7 abilitato.");
    Serial.println();
  } else {
    Serial.println("Sensore MQ-7 disabilitato.");
    Serial.println();
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // CONNESSIONE ALLA RETE e RECUPERO DATA/ORA +++++++++++++++++++++++++++++++++++++++++++++++++++
  initWifi();
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Serial.println();

}// end of SETUP
//*******************************************************************************************************************************
//*******************************************************************************************************************************
//*******************************************************************************************************************************





//*******************************************************************************************************************************
//********************************************  L O O P  ************************************************************************
//*******************************************************************************************************************************
void loop() {

  // DISCONNESSIONE E SPEGNIMENTO WIFI +++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------
  Serial.println("Spengo il WiFi...\n");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(1000); // Waiting a bit for Wifi mode set
  connesso_ok = false;
  dataora_ok = false;
  u8g2.drawXBMP(52, 0, 16, 16, blank_icon16x16);
  u8g2.drawXBMP(112, 0, 16, 16, nocon_icon16x16);
  u8g2.sendBuffer();
  //------------------------------------------------------------------------
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  if (DEBBUG) {
    Serial.print("codice: --> "); Serial.println(codice);
    Serial.print("SD_ok: --> "); Serial.println(SD_ok);
    Serial.print("cfg_ok: --> "); Serial.println(cfg_ok);
    Serial.print("connesso_ok: --> "); Serial.println(connesso_ok);
    Serial.print("dataora_ok: --> "); Serial.println(dataora_ok);
    Serial.print("invio_ok: --> "); Serial.println(invio_ok);
  }


  // ATTESA CICLO MISURE +++++++++++++++++++++++++++++++++++++++++++
  if (!iaqon || !BME680_run) {
    for (int i = 60 * attesa; i > 0; i--) {
      String output = "";
      output = String(i / 60) + ":";
      if (i % 60 >= 0 && i % 60 <= 9) {
        output += "0";
      }
      output += String(i % 60);
      Serial.println("Attendi " + output + " min. per il ciclo di misure");
      drawScrHead();
      u8g2.setCursor(5, 35); u8g2.print("Attesa per misure...");
      u8g2.setCursor(8, 55); u8g2.print("ATTENDI " + output + " MIN.");
      u8g2.sendBuffer();
      delay(1000);
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



  //------------------------------------------------------------------------
  //++++++++++++++++ AGGIORNAMENTO SENSORI ++++++++++++++++++++++++++++++

  int maxmes = 0; // stores how many measurements are actually performed
  int time_trigger = 0; //for BME680's IAQ calibration cycles
  bool printed = false;

  // Zeroing out the variables
  temp = 0.0;
  pre = 0.0;
  hum = 0.0;
  VOC = 0.0;
  PM1 = 0.0;
  PM25 = 0.0;
  PM10 = 0.0;
  ozone = 0.0;
  MICS6814_CO = 0.0;
  MICS6814_NO2 = 0.0;
  MICS6814_NH3 = 0.0;
  MICS6814_C3H8 = 0.0;
  MICS6814_C4H10 = 0.0;
  MICS6814_CH4 = 0.0;
  MICS6814_H2 = 0.0;
  MICS6814_C2H5OH = 0.0;
  COppm = 0.0;

  if (!BME680_run || !PMS_run) {
    if (!printed) {
      //messaggio seriale
      Serial.println();
      Serial.println("Sto effettuando le misurazioni...\n");
      //messaggio a schermo
      drawScrHead();
      u8g2.drawStr(15, 35, "Sto effettuando");
      u8g2.drawStr(15, 55, "le misurazioni...");
      u8g2.sendBuffer();
      printed = true;
    }
  }

  //+++++++++  AGGIORNAMENTO SENSORE BME680  ++++++++++++
  if (BME680_run) {
    if (DEBBUG) Serial.println("...campiono BME680...");
    String output = "";
    while (maxmes < avg_measurements) {
      if (iaqSensor.run()) {
        if (iaqon) {
          if (iaqSensor.iaqAccuracy < 3) {
            output = String(time_trigger / 60) + ":";
            if (time_trigger % 60 >= 0 && time_trigger % 60 <= 9) {
              output += "0";
            }
            output += String(time_trigger % 60);
            if (time_trigger == 0) Serial.println("Calibrazione BME680 in corso, attendere...");
            Serial.println("Tempo trascorso: " + output);
            drawScrHead();
            u8g2.setCursor(5, 35); u8g2.print("Calibro il BME680...");
            u8g2.setCursor(8, 55); u8g2.print("ATTENDI...  " + String(output));
            u8g2.sendBuffer();
            time_trigger += 3;
            if (DEBBUG) {
              Serial.printf("Temperatura(*C): %.3f\n", iaqSensor.temperature);
              Serial.printf("Pressione(hPa): %.3f\n", iaqSensor.pressure / 100.0);
              Serial.printf("Umidità(perc.): %.3f\n", iaqSensor.humidity);
              Serial.printf("Resistenza Gas(kOhm): %.3f\n", iaqSensor.gasResistance / 1000.0);
              Serial.printf("VOC: %.0f\n", iaqSensor.iaq);
              Serial.printf("Accuratezza VOC: %d\n", iaqSensor.iaqAccuracy);
            }
          } else {
            //messaggio seriale
            Serial.println();
            Serial.println("Sto effettuando le misurazioni...\n");
            //messaggio a schermo
            drawScrHead();
            u8g2.drawStr(15, 35, "Sto effettuando");
            u8g2.drawStr(15, 55, "le misurazioni...");
            u8g2.sendBuffer();
            temp = iaqSensor.temperature;
            pre = (iaqSensor.pressure / 100.0);
            hum = iaqSensor.humidity;
            VOC = iaqSensor.iaq;
            maxmes = 99999; // provides exit condition
          }
          updateState(); //updates EEPROM state
        } else {
          if (!printed) {
            //messaggio seriale
            Serial.println();
            Serial.println("Sto effettuando le misurazioni...\n");
            //messaggio a schermo
            drawScrHead();
            u8g2.drawStr(15, 35, "Sto effettuando");
            u8g2.drawStr(15, 55, "le misurazioni...");
            u8g2.sendBuffer();
            printed = true;
          }
          temp += iaqSensor.temperature;
          pre += (iaqSensor.pressure / 100.0);
          hum += iaqSensor.humidity;
          VOC += (iaqSensor.gasResistance / 1000.0);
          maxmes++;
          if (DEBBUG) {
            Serial.printf("Temperatura(*C): %.3f\n", temp);
            Serial.printf("Pressione(hPa): %.3f\n", pre);
            Serial.printf("Umidità(perc.): %.3f\n", hum);
            Serial.printf("Resistenza Gas(kOhm): %.3f\n", VOC);
            Serial.println();
          }
          delay(avg_delay * 1000);
        }
      } else {
        delay(1000);
      }
    }
    if (!iaqon && maxmes > 0) {
      temp /= maxmes;
      pre /= maxmes;
      hum /= maxmes;
      VOC /= maxmes;
    }
    // Normalizing pressure based on sea level altitude and current temperature
    pre = (pre * pow(1 - (0.0065 * sealevelalt / (temp + 0.0065 * sealevelalt + 273.15)), -5.257));
    // Rounding IAQ value to integer
    if (iaqon) {
      int b = VOC;
      if ((VOC - float(b)) >= 0.500) {
        VOC = b + 1;
      } else {
        VOC = b;
      }
    }
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++  AGGIORNAMENTO SENSORE PMS5003  +++++++++++
  if (PMS_run) {
    if (DEBBUG) Serial.println("...campiono PMS5003...");
    pms.wakeUp(); // Waking up sensor after sleep
    maxmes = 0;
    for (int i = 60; i > 0; i--) { //Preheating sensor for accurate measurements, 30 seconds is fine, 1 minute for good measure
      String output = "";
      output = String(i / 60) + ":";
      if (i % 60 >= 0 && i % 60 <= 9) {
        output += "0";
      }
      output += String(i % 60);
      Serial.println("Attendi " + output + " min. per riscaldamento PMS5003");
      drawScrHead();
      u8g2.setCursor(5, 35); u8g2.print("Riscaldo PMS5003...");
      u8g2.setCursor(8, 55); u8g2.print("ATTENDI " + output + " MIN.");
      u8g2.sendBuffer();
      delay(1000);
    }
    //messaggio seriale
    Serial.println("Sto effettuando le misurazioni...\n");
    //messaggio a schermo
    drawScrHead();
    u8g2.drawStr(15, 35, "Sto effettuando");
    u8g2.drawStr(15, 55, "le misurazioni...");
    u8g2.sendBuffer();
    while (maxmes < avg_measurements) {
      while (pmsSerial.available()) {
        pmsSerial.read(); // Clears buffer (removes potentially old data) before read.
      }
      if (!pms.readUntil(data)) {
        delay(1000);
        continue;
      }
      PM1 += data.PM_AE_UG_1_0;
      PM25 += data.PM_AE_UG_2_5;
      PM10 += data.PM_AE_UG_10_0;
      maxmes++;
      delay(avg_delay * 1000);
    }
    if (maxmes > 0) {
      PM1 /= maxmes;
      PM25 /= maxmes;
      PM10 /= maxmes;
    }
    pms.sleep(); // Putting sensor to sleep
    delay(1500);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++  AGGIORNAMENTO SENSORE MICS6814  ++++++++++++
  if (MICS6814_run) {
    if (DEBBUG) Serial.println("...campiono MICS6814...");
    maxmes = 0;
    while (maxmes < avg_measurements) {
      float c1 = gas.measureCO();
      float c2 = gas.measureNO2();
      float c3 = gas.measureNH3();
      float c4 = gas.measureC3H8();
      float c5 = gas.measureC4H10();
      float c6 = gas.measureCH4();
      float c7 = gas.measureH2();
      float c8 = gas.measureC2H5OH();
      MICS6814_CO += c1;
      MICS6814_NO2 += c2;
      MICS6814_NH3 += c3;
      MICS6814_C3H8 += c4;
      MICS6814_C4H10 += c5;
      MICS6814_CH4 += c6;
      MICS6814_H2 += c7;
      MICS6814_C2H5OH += c8;
      maxmes++;
      delay(avg_delay * 1000);
    }
    if (maxmes > 0) {
      MICS6814_CO /= maxmes;
      MICS6814_NO2 /= maxmes;
      MICS6814_NH3 /= maxmes;
      MICS6814_C3H8 /= maxmes;
      MICS6814_C4H10 /= maxmes;
      MICS6814_CH4 /= maxmes;
      MICS6814_H2 /= maxmes;
      MICS6814_C2H5OH /= maxmes;
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++  AGGIORNAMENTO SENSORE ZE25-O3  ++++++++++++
  if (O3_run) {
    if (DEBBUG) Serial.println("...campiono ZE25-O3...");
    maxmes = 0;
    int halt = 0;
    while (maxmes < avg_measurements) {
      if (O3sens.readManual() < 0) {
        if (halt > 5) { //if errors for more than 5 times, exit
          ozone = -1.0;
          break;
        }
        halt++;
        delay(1000);
        continue;
      }
      ozone += O3sens.readManual();
      maxmes++;
      delay(avg_delay * 1000);
    }
    if (maxmes > 0) {
      ozone /= maxmes;
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++  AGGIORNAMENTO SENSORE CO MQ7  ++++++++++++
  if (MQ7_run) {
    if (DEBBUG) Serial.println("...campiono MQ7...");
    maxmes = 0;
    while (maxmes < avg_measurements) {
      int COx = 0;
      float VoltLev, Rs, ratio, LogCOppm, tempco;
      // gestione dato ANALOG da sensore CO MQ7
      COx = analogRead(32);
      delay(10);
      // calcolo PPM di CO
      VoltLev = COx / rangeV;               // calcolo il livello di tensione letto
      Rs = Res1 * (5 - VoltLev) / VoltLev;  // calcolo la resistenza del sensore
      ratio = Rs / Res1;
      float k1 = -1.3;                      // k1 entità che pesa la linearità (minore = meno lineare)
      float k2 = 4.084;                     // k2 fattore moltiplicativo esponenziale
      LogCOppm = (log10(ratio) * (k1)) + k2;
      tempco = pow(10, LogCOppm);
      if (tempco >= 0) {
        COppm += tempco;
        maxmes++;
      }
      delay(avg_delay * 1000);
    }
    if (maxmes > 0) {
      COppm /= maxmes;
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++



  //------------------------------------------------------------------------
  //+++++++++++ ATTESA DINAMICA PREINVIO AGGIUNTIVA (SOLO IAQ ON) +++++++++++++++++++++
  if (BME680_run && iaqon && time_trigger < (attesa * 60)) {
    // resto è specificato in secondi. attesa in minuti.
    int resto = (attesa * 60) - time_trigger;
    for (int i = resto; i > 0; i--) {
      String output = "";
      output = String(i / 60) + ":";
      if (i % 60 >= 0 && i % 60 <= 9) {
        output += "0";
      }
      output += String(i % 60);
      Serial.println("Attendi " + output + " min. per l'invio dei dati");
      drawScrHead();
      u8g2.setCursor(5, 35); u8g2.print("Attesa per invio...");
      u8g2.setCursor(8, 55); u8g2.print("ATTENDI " + output + " MIN.");
      u8g2.sendBuffer();
      delay(1000);
    }
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------



  //------------------------------------------------------------------------
  //+++++++++++ AGGIORNO DISPLAY  ++++++++++++++++++++++++++++++++++++++++++
  displayMeasures();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------



  //------------------------------------------------------------------------
  //+++++++++++ RICONNESSIONE e AGGIORNAMENTO DATA/ORA +++++++++++++++++++++
  if (cfg_ok) {
    if (DEBBUG) Serial.println("...riconnessione e aggiornamento ora...");
    initWifi();
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------



  //------------------------------------------------------------------------
  //+++++++++++++ LOG SU SERIALE  +++++++++++++++++++++++
  if (DEBBUG) Serial.println("...log su seriale...");
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println();
  Serial.print("Data: " + dayStamp); Serial.println("  Ora: " + timeStamp + "\n");
  Serial.println("Temperatura: " + floatToComma(temp) + "°C\n");
  Serial.println("Umidita': " + floatToComma(hum) + "%\n");
  Serial.println("Pressione: " + floatToComma(pre) + "hPa\n");
  Serial.println("PM10: " + floatToComma(PM10) + "ug/m3\n");
  Serial.println("PM2,5: " + floatToComma(PM25) + "ug/m3\n");
  Serial.println("PM1: " + floatToComma(PM1) + "ug/m3\n");
  Serial.println("NOx: " + floatToComma(MICS6814_NO2) + "ppm\n");
  Serial.println("CO: " + floatToComma(MICS6814_CO) + "ppm\n");
  Serial.println("O3: " + floatToComma(ozone) + "ppm\n");
  if (iaqon) {
    Serial.println("IAQ: " + floatToComma(VOC) + "\n");
  } else {
    Serial.println("VOC: " + floatToComma(VOC) + "kOhm\n");
  }
  Serial.println("NH3: " + floatToComma(MICS6814_NH3) + "ppm\n");
  Serial.println("C3H8: " + floatToComma(MICS6814_C3H8) + "ppm\n");
  Serial.println("C4H10: " + floatToComma(MICS6814_C4H10) + "ppm\n");
  Serial.println("CH4: " + floatToComma(MICS6814_CH4) + "ppm\n");
  Serial.println("H2: " + floatToComma(MICS6814_H2) + "ppm\n");
  Serial.println("C2H5OH: " + floatToComma(MICS6814_C2H5OH) + "ppm\n");
  Serial.println("CO(MQ-7): " + floatToComma(COppm) + "ppm");
  Serial.println();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------



  //------------------------------------------------------------------------
  //+++++++++++++ LOG SU SD CARD  ++++++++++++++++++++++++++++++++++++++++++

  if (SD_ok) {
    if (DEBBUG) Serial.println("...log su scheda SD...");
    //"Data;Ora;Temp(*C);Hum(%);Pre(hPa);PM10(ug/m3);PM2,5(ug/m3);PM1(ug/m3);NOx(ppm);CO(ppm);O3(ppm);VOC(IAQ/kOhm);NH3(ppm);C3H8(ppm);C4H10(ppm);CH4(ppm);H2(ppm);C2H5OH(ppm);CO(MQ-7)(ppm)"
    String logvalue = "";
    logvalue += dayStamp; logvalue += ";";
    logvalue += timeStamp; logvalue += ";";
    logvalue += floatToComma(temp); logvalue += ";";
    logvalue += floatToComma(hum); logvalue += ";";
    logvalue += floatToComma(pre); logvalue += ";";
    logvalue += floatToComma(PM10); logvalue += ";";
    logvalue += floatToComma(PM25); logvalue += ";";
    logvalue += floatToComma(PM1); logvalue += ";";
    logvalue += floatToComma(MICS6814_NO2); logvalue += ";";
    logvalue += floatToComma(MICS6814_CO); logvalue += ";";
    logvalue += floatToComma(ozone); logvalue += ";";
    logvalue += floatToComma(VOC); logvalue += ";";
    logvalue += floatToComma(MICS6814_NH3); logvalue += ";";
    logvalue += floatToComma(MICS6814_C3H8); logvalue += ";";
    logvalue += floatToComma(MICS6814_C4H10); logvalue += ";";
    logvalue += floatToComma(MICS6814_CH4); logvalue += ";";
    logvalue += floatToComma(MICS6814_H2); logvalue += ";";
    logvalue += floatToComma(MICS6814_C2H5OH); logvalue += ";";
    logvalue += floatToComma(COppm);
    if (addToLog(SD, logpath, logvalue)) {
      Serial.println("Log su SD aggiornato con successo!\n");
    } else {
      Serial.println("ERRORE aggiornamento log su SD!\n");
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------



  //------------------------------------------------------------------------
  //++++++ AGGIORNAMENTO SERVER ++++++++++++++++++++++++++++++++++++++++++++
  if (connesso_ok && dataora_ok) { // dentro se connesso al wifi e data e ora sono ok
    if (DEBBUG) Serial.println("...upload dei dati sul SERVER");
    u8g2.drawXBMP(92, 0, 16, 16, arrow_up_icon16x16);
    u8g2.sendBuffer();
    if (DEBBUG) Serial.println("...disegnata icona...");

    if (client.connect("api.milanosmartpark.net", 80)) {    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      String postStr = "apikey=";
      postStr += codice;
      postStr += "&temp=";
      postStr += String(temp, 3);
      postStr += "&hum=";
      postStr += String(hum, 3);
      postStr += "&pre=";
      postStr += String(pre, 3);
      postStr += "&voc=";
      postStr += String(VOC, 3);
      postStr += "&cox=";
      postStr += String(MICS6814_CO, 3);
      postStr += "&nox=";
      postStr += String(MICS6814_NO2, 3);
      postStr += "&nh3=";
      postStr += String(MICS6814_NH3, 3);
      postStr += "&pm1=";
      postStr += String(PM1, 3);
      postStr += "&pm25=";
      postStr += String(PM25, 3);
      postStr += "&pm10=";
      postStr += String(PM10, 3);
      postStr += "&o3=";
      postStr += String(ozone, 3);
      postStr += "&mac=";
      postStr += macAdr;
      postStr += "&data=";
      postStr += dayStamp;
      postStr += "&ora=";
      postStr += timeStamp;

      if (DEBBUG) Serial.println("...aggiunte tutte le stringhe a POSTSTR...");

      client.print("POST /api/channels/writelog HTTP/1.1\r\n");
      client.print("Host: api.milanosmartpark.net\r\n");
      client.print("Connection: close\r\n");
      client.print("User-Agent: Wondermade\r\n");
      client.print("Content-Type: application/x-www-form-urlencoded\r\n");
      client.print("Content-Length: ");
      client.print(postStr.length());
      client.print("\r\n\r\n");
      client.print(postStr);
      if (DEBBUG) Serial.println("STRINGA DI POST INVIATA..........");
      if (DEBBUG) Serial.println(postStr);
      // Read all the lines of the reply from server and print them to Serial
      while (client.available()) { //Does this actually do anything?
        String line = client.readStringUntil('\r');
        Serial.println(line);
      }
      delay(1000);
      postStr = "";
      client.stop();
      u8g2.drawXBMP(92, 0, 16, 16, blank_icon16x16);
      u8g2.sendBuffer();
      Serial.println("Dati al server inviati con successo!\n");
    } else {
      Serial.println("ERRORE durante la connessione al server. Invio non effettuato!\n");
    }
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


}// end of LOOP
