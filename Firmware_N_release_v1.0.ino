//
//  Progetto MILANO SMART PARK - Parco Segantini
//  Firmware N per ESP32-DevkitC con ESP32-WROVER-B
//  Versioni originali by Luca Crotti
//  Versioni N e Mobile by Norman Mulinacci
//
//  Librerie richieste: Pacchetto esp32 per Arduino, BSEC Software Library, PMS_Extended Library, MiCS6814-I2C Library, U8g2 Library
//

//Basic system libraries
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <Wire.h>
#include <SPI.h>

// WiFi and NTP time management libraries
#include <WiFi.h>
#include "time.h"

// Sensors management libraries
#include "bsec.h" //for BME_680
#include "PMS.h" //for PMS5003
#include "MiCS6814-I2C.h" //for MICS6814

// OLED display library
#include "U8g2lib.h"

String ver = "1.0"; //current firmware version

// i2c bus pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Serial for PMS5003
HardwareSerial pmsSerial(2);  // seriale1  (UART0=0; UART1=1; UART2=2)

// BME680, PMS5003 and MICS6814 sensors instances
Bsec iaqSensor;
PMS pms(pmsSerial);
MiCS6814 gas;

// Instance for the OLED 1.3" display with the SH1106 controller
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);   // ESP32 Thing, HW I2C with pin remapping

// Reboots after sleep counter
RTC_DATA_ATTR int bootCount = 0;

// Define WiFi Client
WiFiClient client;

//Define conversion factor from micro seconds to minutes
#define uS_TO_M_FACTOR 60000000


//++++++++++++++++++++++ DEBUG variable ++++++++++++++++++++++++++++++++
bool DEBBUG = false;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// Network and system setup variables
String ssid = "";
String pwd = "";
String codice = "";
String splash = "";
String logpath = "";
wifi_power_t wifipow;
int sleep_time = 0;
int preheat = 0;
int avg_measurements = 0;
int avg_delay = 0;

// Variables and structs for BME680
float hum = 0.0;
float temp = 0.0;
float pre = 0.0;
float IAQ = 0.0;

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

// Variables for MQ-7
const float Res1 = 5000.0;
const float rangeV = 4096.0;
float COppm = 0.0;

// variabili di stato
bool SD_ok = false;
bool cfg_ok = false;
bool ssid_ok = false;
bool connesso_ok = false;
bool dataora_ok = false;
bool invio_ok = false;

// variabili per gestione orario da server
String dayStamp = "";
String timeStamp = "";

// Sensor active variables
bool BME680_run;
bool PMS_run;
bool MICS6814_run;
bool MQ7_run;

// Variable for the MAC address
String MACA = "";
byte mac[6];


//+++++++++++++++++  I C O N E  D I  S I S T E M A  +++++++++++++++++
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

  String convert = String(value);
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
  u8g2.setCursor(5, 61); u8g2.print("NH3:  " + floatToComma(MICS6814_NH3) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 2
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("COx:  " + floatToComma(MICS6814_CO) + "ppm");
  u8g2.setCursor(5, 39); u8g2.print("NOx:  " + floatToComma(MICS6814_NO2) + "ppm");
  u8g2.setCursor(5, 50); u8g2.print("PM2,5:  " + floatToComma(PM25) + "ug/m3");
  u8g2.setCursor(5, 61); u8g2.print("PM10:  " + floatToComma(PM10) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 3
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("PM1:  " + floatToComma(PM1) + "ug/m3");
  u8g2.setCursor(5, 39); u8g2.print("IAQ:  " + floatToComma(IAQ));
  u8g2.setCursor(5, 50); u8g2.print("CO:  " + floatToComma(COppm) + "ppm");
  u8g2.setCursor(5, 61); u8g2.print("C3H8:  " + floatToComma(MICS6814_C3H8) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 4
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("C4H10:  " + floatToComma(MICS6814_C4H10) + "ppm");
  u8g2.setCursor(5, 39); u8g2.print("CH4:  " + floatToComma(MICS6814_CH4) + "ppm");
  u8g2.setCursor(5, 50); u8g2.print("H2:  " + floatToComma(MICS6814_H2) + "ppm");
  u8g2.setCursor(5, 61); u8g2.print("C2H5OH:  " + floatToComma(MICS6814_C2H5OH) + "ppm");
  u8g2.sendBuffer();
  delay(5000);

}// end of displayMeasures()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void print_wakeup_reason() {

  //Method to print the reason by which ESP32 has been awaken from sleep
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }

}// end of print_wakeup_reason()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool initializeSD() {

  if (!SD.begin()) {
    Serial.println("Errore lettore SD CARD!");
    return false;
  } else {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("LETTORE SD CARD inizializzato, CARD non presente - controllare!");
      return false;  // se attivo il return, in caso ci arrivo il codice non va avanti
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
    return true;
  }// fine else
  Serial.println();
  delay(300);

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
      if (DEBBUG) Serial.println("PARSECFG: Errore! SSID assente.");
      esito = false;
    }
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando ssid non riconosciuto.");
    esito = false;
  }
  //pwd
  if (command[1].startsWith("password", 0)) {
    pwd = command[1].substring(command[1].indexOf("password") + 9, command[1].length());
    Serial.print("pwd = *"); Serial.print(pwd); Serial.println("*");
    if (pwd.length() == 0) {
      if (DEBBUG) Serial.println("PARSECFG: Errore! Password assente.");
      esito = false;
    }
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando pwd non riconosciuto.");
    esito = false;
  }
  //codice
  if (command[2].startsWith("codice", 0)) {
    codice = command[2].substring(command[2].indexOf("codice") + 7, command[2].length());
    Serial.print("codice = *"); Serial.print(codice); Serial.println("*");
    if (codice.length() == 0) {
      if (DEBBUG) Serial.println("PARSECFG: Errore! Codice assente.");
      esito = false;
    }
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando codice non riconosciuto.");
    esito = false;
  }
  //splash
  if (command[3].startsWith("splash", 0)) {
    splash = command[3].substring(command[3].indexOf("splash") + 7, command[3].length());
    Serial.print("splash = *"); Serial.print(splash); Serial.println("*");
    if (splash.length() == 0) {
      if (DEBBUG) Serial.println("PARSECFG: Errore! Splash assente.");
      esito = false;
    }
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando splash non riconosciuto.");
    esito = false;
  }
  //potenza_wifi
  if (command[4].startsWith("potenza_wifi", 0)) {
    temp = "";
    temp = command[4].substring(command[4].indexOf("potenza_wifi") + 13, command[4].length());
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
      if (DEBBUG) Serial.println("PARSECFG: Errore! Valore di potenza_wifi non valido. Fallback a 19.5dBm.");
      wifipow = WIFI_POWER_19_5dBm;
    }
    Serial.print("potenza_wifi = *"); Serial.print(wifipow); Serial.println("*");
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando potenza_wifi non riconosciuto. Fallback a 19.5dBm.");
    wifipow = WIFI_POWER_19_5dBm;
  }
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
  //preheat
  if (command[6].startsWith("preriscaldamento(minuti)", 0)) {
    temp = "";
    temp = command[6].substring(command[6].indexOf("preriscaldamento(minuti)") + 25, command[6].length());
    preheat = temp.toInt();
    Serial.printf("preheat = *%d*\n", preheat);
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando preheat non riconosciuto. Fallback a 5 minuti.");
    preheat = 5;
  }
  //avg_measurements
  if (command[7].startsWith("numero_misurazioni_media", 0)) {
    temp = "";
    temp = command[7].substring(command[7].indexOf("numero_misurazioni_media") + 25, command[7].length());
    avg_measurements = temp.toInt();
    Serial.printf("avg_measurements = *%d*\n", avg_measurements);
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando avg_measurements non riconosciuto. Fallback a 10 misurazioni");
    avg_measurements = 10;
  }
  //avg_delay
  if (command[8].startsWith("ritardo_media(secondi)", 0)) {
    temp = "";
    temp = command[8].substring(command[8].indexOf("ritardo_media(secondi)") + 23, command[8].length());
    avg_delay = temp.toInt();
    Serial.printf("avg_delay = *%d*\n", avg_delay);
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando avg_delay non riconosciuto. Fallback a 1 secondo");
    avg_delay = 1;
  }
  //MQ7_run
  if (command[9].startsWith("attiva_MQ7", 0)) {
    temp = "";
    temp = command[9].substring(command[9].indexOf("attiva_MQ7") + 11, command[9].length());
    if (temp.indexOf("true") == 0) {
      MQ7_run = true;
    } else if (temp.indexOf("false") == 0) {
      MQ7_run = false;
    } else {
      if (DEBBUG) Serial.println("PARSECFG: Errore! Valore di attiva_MQ7 non valido. Fallback a false.");
      MQ7_run = false;
    }
    Serial.printf("MQ7_run = %s\n", MQ7_run ? "true" : "false");
  } else {
    if (DEBBUG) Serial.println("PARSECFG: Errore! Comando attiva_MQ7 non riconosciuto. Fallback a false.");
    MQ7_run = false;
  }

  return esito;

}// end of parseConfig()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void scansioneWifi() {
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
}// end of scansioneWifi()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool connessioneWifi() {

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
    Serial.println("");
    Serial.println("WiFi CONNESSO....");
    drawScrHead();
    u8g2.drawStr(15, 45, "WiFi Connesso");
    u8g2.sendBuffer();
    delay(500);
    return true;
  } else {
    Serial.println("");
    Serial.println("WiFi NON Connesso.");
    drawScrHead();
    u8g2.drawStr(15, 45, "WiFi NON Connesso");
    u8g2.sendBuffer();
    delay(500);
    dataora_ok = false;
    return false;
  }

}// end of connessioneWifi()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
String getMacAdr(byte* mc) {

  String macAdr = "";
  WiFi.macAddress(mc);
  macAdr += String(mc[0], HEX); macAdr += ":";
  macAdr += String(mc[1], HEX); macAdr += ":";
  macAdr += String(mc[2], HEX); macAdr += ":";
  macAdr += String(mc[3], HEX); macAdr += ":";
  macAdr += String(mc[4], HEX); macAdr += ":";
  macAdr += String(mc[5], HEX);
  macAdr.toUpperCase();


  if (DEBBUG) {
    Serial.print("MAC: ");
    Serial.print(mc[0], HEX);
    Serial.print(":");
    Serial.print(mc[1], HEX);
    Serial.print(":");
    Serial.print(mc[2], HEX);
    Serial.print(":");
    Serial.print(mc[3], HEX);
    Serial.print(":");
    Serial.print(mc[4], HEX);
    Serial.print(":");
    Serial.println(mc[5], HEX);
    Serial.println(macAdr);
  }
  MACA = macAdr;
  return macAdr;

}// end of getMacAdr()
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

}// end of syncNTPTime()
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//*******************************************************************************************************************************
//******************************************  S E T U P  ************************************************************************
//*******************************************************************************************************************************
void setup() {

  // INIZIALIZZO I2C, DISPLAY E SERIALE ++++++++++++++++++++++++++++++++++++
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
  Serial.begin(115200);
  delay(1500);// serve per dare tempo alla seriale di attivarsi


  // SPLASH INIZIO ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // HELLO via seriale
  Serial.println();
  Serial.println("MILANO SMART PARK");
  Serial.print("FIRMWARE N - VERSIONE "); Serial.println(ver);
  Serial.println("by Norman Mulinacci, 2020"); Serial.println();
  // SPLASH a schermo
  u8g2.firstPage();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(5, 15, "MILANO SMART PARK");
  u8g2.setCursor(5, 30); u8g2.print("FIRMW. N VER. " + ver);
  u8g2.drawStr(5, 45, "Norman Mulinacci '20");
  u8g2.sendBuffer();
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // INIZIALIZZAZIONE SD CARD E FILE DI CONFIGURAZIONE+++++++++++++++++++++++++++++++++++++++++++++++++
  SD_ok = initializeSD();
  Serial.println();
  delay(100);
  if (SD_ok == true) {
    File cfgfile;
    if (SD.exists("/config.cfg")) {
      cfgfile = SD.open("/config.cfg", FILE_READ);// apri il file in lettura
      Serial.println("File di configurazione aperto.\n");
      cfg_ok = parseConfig(cfgfile);
      if (cfg_ok) {
        u8g2.setFont(u8g2_font_t0_17b_mf);
        u8g2.drawStr(25, 60, splash.c_str()); u8g2.sendBuffer();
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
      cfgfile = SD.open("/config.cfg", FILE_WRITE);// apri il file in scrittura
      if (cfgfile) {
        cfgfile.close();
        appendFile(SD, "/config.cfg", "#ssid=;");
        appendFile(SD, "/config.cfg", "#password=;");
        appendFile(SD, "/config.cfg", "#codice=;");
        appendFile(SD, "/config.cfg", "#splash=;");
        appendFile(SD, "/config.cfg", "#potenza_wifi=19.5dBm;");
        appendFile(SD, "/config.cfg", "#sleep(minuti)=20;");
        appendFile(SD, "/config.cfg", "#preriscaldamento(minuti)=5;");
        appendFile(SD, "/config.cfg", "#numero_misurazioni_media=10;");
        appendFile(SD, "/config.cfg", "#ritardo_media(secondi)=1;");
        appendFile(SD, "/config.cfg", "#attiva_MQ7=false;");
        appendFile(SD, "/config.cfg", "");
        appendFile(SD, "/config.cfg", "//Valori possibili per potenza_wifi: -1, 2, 5, 7, 8.5, 11, 13, 15, 17, 18.5, 19, 19.5 dBm");
        appendFile(SD, "/config.cfg", "//Per ritardo_media(secondi) consigliati 1 o 2 secondi al massimo");
        Serial.println("File creato!");
        Serial.println();
        cfgfile = SD.open("/config.cfg", FILE_READ);// apri il file in lettura
        cfg_ok = parseConfig(cfgfile);
      } else {
        Serial.println("Errore nel creare il file cfg!");
        Serial.println();
        cfg_ok = false;
        //Setting some system variables to default fallback values
        wifipow = WIFI_POWER_19_5dBm;
        sleep_time = 1;
        preheat = 5;
        avg_measurements = 10;
        avg_delay = 1;
        MQ7_run = false;
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
    //Setting some system variables to default fallback values
    wifipow = WIFI_POWER_19_5dBm;
    sleep_time = 1;
    preheat = 5;
    avg_measurements = 10;
    avg_delay = 1;
    MQ7_run = false;
  }
  Serial.println();
  delay(2000);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // CONTROLLO LOGFILE +++++++++++++++++++++++++++++++++++++
  if (SD_ok == true) {
    logpath = "/LOG_N_" + codice + "_v" + ver + ".csv";
    if (!SD.exists(logpath)) {
      Serial.println("File di log non presente, lo creo...");
      File filecsv = SD.open(logpath, FILE_WRITE);
      if (filecsv) {
        filecsv.close();
        String headertext = "File di log della centralina: " + codice + " | Versione firmware N: " + ver;
        appendFile(SD, logpath, headertext);
        appendFile(SD, logpath, "Date;Time;Temp(*C);Hum(%);Pre(hPa);NH3(ppm);COx(ppm);NOx(ppm);PM2,5(ug/m3);PM10(ug/m3);PM1(ug/m3);O3;IAQ;CO(ppm);C3H8(ppm);C4H10(ppm);CH4(ppm);H2(ppm);C2H5OH(ppm)");
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
  }//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


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


  // SCANSIONE E CONNESSIONE ALLA RETE +++++++++++++++++++++++++++++++++++++++++++++++++++
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000); // Waiting a bit for Wifi mode set
  if (DEBBUG) Serial.printf("wifipow: %d\n", wifipow);
  WiFi.setTxPower(wifipow);
  Serial.printf("Potenza Wifi impostata a %d\n", WiFi.getTxPower());
  Serial.println("Legenda: -4(-1dBm), 8(2dBm), 20(5dBm), 28(7dBm), 34(8.5dBm), 44(11dBm), 52(13dBm), 60(15dBm), 68(17dBm), 74(18.5dBm), 76(19dBm), 78(19.5dBm)");
  Serial.println();
  if (cfg_ok) {
    for (int m = 0; m < 4; m++) {
      scansioneWifi();
      if (cfg_ok && ssid_ok) {
        Serial.print(ssid); Serial.println(" trovata!");
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setCursor(5, 55); u8g2.print(ssid + " OK!");
        u8g2.sendBuffer();
        delay(4000);
        //lancio la connessione wifi
        connesso_ok = connessioneWifi();
        //lancio connessione a server orario e sincronizzo l'ora
        syncNTPTime();
        break;
      } else if (!ssid_ok) {
        Serial.print("Errore! "); Serial.print(ssid); Serial.println(" non trovata!");
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setCursor(5, 55); u8g2.print("NO " + ssid + "!");
        u8g2.sendBuffer();
        delay(4000);
        if (m < 3) {
          Serial.println("Riprovo...");
          drawScrHead();
          u8g2.drawStr(35, 45, "RIPROVO...");
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
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // +++++++ RILEVAMENTO E INIZIALIZZAZIONE SENSORI +++++++++++++++

  //messaggio seriale
  Serial.println("Rilevamento sensori in corso...\n");
  //messaggio a schermo
  drawScrHead();
  u8g2.drawStr(8, 45, "Rilevo i sensori...");
  u8g2.sendBuffer();


  // inizializzo BME680 ++++++++++++++++++++++++++++++++++++++++++
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  if (iaqSensor.status == BSEC_OK && iaqSensor.bme680Status == BME680_OK) {
    Serial.println("Sensore BME680 connesso, inizializzo...");
    Serial.println();
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
  } else {
    Serial.println("Sensore BME680 non connesso.");
    if (DEBBUG) Serial.println("BME680 error code : " + String(iaqSensor.bme680Status));
    Serial.println();
    BME680_run = false;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // inizializzo SENSORE PMS5003 ++++++++++++++++++++++++++++++++++++++++++
  // pmsSerial: with WROVER module don't use UART 2 mode on pins 16 and 17, it crashes!
  pmsSerial.begin(9600, SERIAL_8N1, 14, 12); // baud, type, ESP_RX, ESP_TX
  delay(1500);
  pms.wakeUp(); // Waking up sensor after sleep, needed with ESP32 deep sleep
  delay(1500);
  if (pms.readUntil(data)) {
    Serial.println("Sensore PMS5003 connesso, inizializzo...");
    Serial.println();
    PMS_run = true;
  } else {
    Serial.println("Sensore PMS5003 non connesso.");
    Serial.println();
    PMS_run = false;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // inizializzo SENSORE MICS6814 ++++++++++++++++++++++++++++++++++++
  if (gas.begin()) { // Connect to sensor using default I2C address (0x04)
    Serial.println("Sensore MICS6814 connesso, inizializzo...");
    MICS6814_run = true;
    // accensione riscaldatore e led
    gas.powerOn();
    gas.ledOn();
    Serial.println("Valori delle resistenze di base del MICS6814:");
    Serial.print("OX: "); Serial.print(gas.getBaseResistance(CH_OX));
    Serial.print(" ; RED: "); Serial.print(gas.getBaseResistance(CH_RED));
    Serial.print(" ; NH3: "); Serial.println(gas.getBaseResistance(CH_NH3));
    Serial.println();
  } else {
    Serial.println("Sensore MICS6814 non connesso.");
    Serial.println();
    MICS6814_run = false;
  }
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


  // PRERISCALDAMENTO SENSORI +++++++++++++++++++++++++++++++++++++++++++
  if (PMS_run || MICS6814_run || MQ7_run) {
    for (int i = 60 * preheat; i > 0; i--) {
      String output = "";
      output = String(i / 60) + ":";
      if (i % 60 >= 0 && i % 60 <= 9) {
        output += "0";
      }
      output += String(i % 60);
      Serial.println("Preriscaldamento sensori in corso, attendere " + output + " min.");
      drawScrHead();
      u8g2.setCursor(5, 35); u8g2.print("Preriscaldamento...");
      u8g2.setCursor(8, 55); u8g2.print("ATTENDI " + output + " MIN.");
      u8g2.sendBuffer();
      delay(1000);
    }
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  Serial.println();

}// end of SETUP
//*******************************************************************************************************************************
//*******************************************************************************************************************************
//*******************************************************************************************************************************





//*******************************************************************************************************************************
//********************************************  L O O P  ************************************************************************
//*******************************************************************************************************************************
void loop() {

  delay(100);

  if (DEBBUG) {
    Serial.print("codice: --> "); Serial.println(codice);
    Serial.print("SD_ok: --> "); Serial.println(SD_ok);
    Serial.print("cfg_ok: --> "); Serial.println(cfg_ok);
    Serial.print("connesso_ok: --> "); Serial.println(connesso_ok);
    Serial.print("dataora_ok: --> "); Serial.println(dataora_ok);
    Serial.print("invio_ok: --> "); Serial.println(invio_ok);
  }


  //------------------------------------------------------------------------
  //++++++++++++++++ AGGIORNAMENTO SENSORI ++++++++++++++++++++++++++++++

  int maxmes = 0;

  // Zeroing out the variables
  temp = 0.0;
  pre = 0.0;
  hum = 0.0;
  IAQ = 0.0;
  PM1 = 0.0;
  PM25 = 0.0;
  PM10 = 0.0;
  MICS6814_CO = 0.0;
  MICS6814_NO2 = 0.0;
  MICS6814_NH3 = 0.0;
  MICS6814_C3H8 = 0.0;
  MICS6814_C4H10 = 0.0;
  MICS6814_CH4 = 0.0;
  MICS6814_H2 = 0.0;
  MICS6814_C2H5OH = 0.0;
  COppm = 0.0;

  //+++++++++  AGGIORNAMENTO SENSORE BME680  ++++++++++++
  if (BME680_run) {
    if (DEBBUG) Serial.println("...campiono BME680...");
    String output = "";
    int time_trigger = 0;
    bool printed = false;
    while (maxmes < avg_measurements) {
      if (iaqSensor.run()) {
        if (iaqSensor.iaqAccuracy < 1) {
          output = String(time_trigger / 60) + ":";
          if (time_trigger % 60 >= 0 && time_trigger % 60 <= 9) {
            output += "0";
          }
          output += String(time_trigger % 60);
          Serial.println("Calibrazione BME680 in corso, attendere...");
          Serial.println("Tempo trascorso: " + output);
          drawScrHead();
          u8g2.setCursor(5, 35); u8g2.print("Calibro il BME680...");
          u8g2.setCursor(8, 55); u8g2.print("ATTENDI...  " + String(output));
          u8g2.sendBuffer();
          time_trigger += 3;
          if (DEBBUG) {
            Serial.printf("Temperatura(*C): %.2f\n", iaqSensor.temperature);
            Serial.printf("Pressione(hPa): %.2f\n", iaqSensor.pressure / 100.0);
            Serial.printf("Umidità(perc.): %.2f\n", iaqSensor.humidity);
            Serial.printf("Resistenza Gas(kOhm): %.2f\n", iaqSensor.gasResistance / 1000.0);
            Serial.printf("IAQ: %.0f\n", iaqSensor.iaq);
            Serial.printf("Accuratezza IAQ: %d\n", iaqSensor.iaqAccuracy);
            Serial.println();
          }
        } else if (iaqSensor.iaq > 26.00) {
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
          IAQ += iaqSensor.iaq;
          maxmes++;
        } else if (!printed) {
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
    }
    if (maxmes > 0) {
      temp /= maxmes;
      pre /= maxmes;
      hum /= maxmes;
      IAQ /= maxmes;
    }
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++

  if (!BME680_run) {
    //messaggio seriale
    Serial.println("Sto effettuando le misurazioni...\n");
    //messaggio a schermo
    drawScrHead();
    u8g2.drawStr(15, 35, "Sto effettuando");
    u8g2.drawStr(15, 55, "le misurazioni...");
    u8g2.sendBuffer();
  }

  //+++++++++  AGGIORNAMENTO SENSORE PMS5003  +++++++++++
  if (PMS_run) {
    if (DEBBUG) Serial.println("...campiono PMS5003...");
    maxmes = 0;
    while (maxmes < avg_measurements) {
      while (pmsSerial.available()) {
        pmsSerial.read(); // Clears buffer (removes potentially old data) before read.
      }
      if (!pms.readUntil(data)) {
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
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++

  //+++++++++  AGGIORNAMENTO SENSORE MICS6814  ++++++++++++
  if (MICS6814_run) {
    if (DEBBUG) Serial.println("...campiono MICS6814...");
    maxmes = 0;
    while (maxmes < avg_measurements) {
      gas.ledOff();
      float c1 = gas.measureCO();
      float c2 = gas.measureNO2();
      float c3 = gas.measureNH3();
      float c4 = gas.measureC3H8();
      float c5 = gas.measureC4H10();
      float c6 = gas.measureCH4();
      float c7 = gas.measureH2();
      float c8 = gas.measureC2H5OH();
      gas.ledOn();
      if (c1 >= 0 && c2 >= 0 && c3 >= 0 && c4 >= 0 && c5 >= 0 && c6 >= 0 && c7 >= 0 && c8 >= 0) {
        gas.ledOff();
        MICS6814_CO += c1;
        MICS6814_NO2 += c2;
        MICS6814_NH3 += c3;
        MICS6814_C3H8 += c4;
        MICS6814_C4H10 += c5;
        MICS6814_CH4 += c6;
        MICS6814_H2 += c7;
        MICS6814_C2H5OH += c8;
        maxmes++;
      }
      delay(avg_delay * 1000);
      gas.ledOn();
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


  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



  //------------------------------------------------------------------------
  //+++++++++++ AGGIORNO DATA E ORA ++++++++++++++++++++++
  //lancio connessione a server orario e sincronizzo l'ora
  if (connesso_ok) {
    syncNTPTime();
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  //------------------------------------------------------------------------
  //+++++++++++++ LOG SU SERIALE  +++++++++++++++++++++++
  if (DEBBUG) Serial.println("...log su seriale...");
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println();
  Serial.print("Date: " + dayStamp); Serial.println("; Time: " + timeStamp);
  Serial.print("Temp: " + floatToComma(temp) + "°C"); Serial.print("; Hum: " + floatToComma(hum) + "%"); Serial.println("; Pre: " + floatToComma(pre) + "hPa");
  Serial.print("NH3: " + floatToComma(MICS6814_NH3) + "ppm"); Serial.print("; COx: " + floatToComma(MICS6814_CO) + "ppm"); Serial.println("; NOx: " + floatToComma(MICS6814_NO2) + "ppm");
  Serial.print("PM2,5: " + floatToComma(PM25) + "ug/m3"); Serial.print("; PM10: " + floatToComma(PM10) + "ug/m3"); Serial.println("; PM1: " + floatToComma(PM1) + "ug/m3");
  Serial.print("O3:     "); Serial.print("; IAQ: " + floatToComma(IAQ)); Serial.println("; CO: " + floatToComma(COppm) + "ppm");
  Serial.print("C3H8: " + floatToComma(MICS6814_C3H8) + "ppm"); Serial.println("; C4H10: " + floatToComma(MICS6814_C4H10) + "ppm");
  Serial.print("CH4: " + floatToComma(MICS6814_CH4) + "ppm"); Serial.print("; H2: " + floatToComma(MICS6814_H2) + "ppm"); Serial.println("; C2H5OH: " + floatToComma(MICS6814_C2H5OH) + "ppm");
  Serial.println();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  //------------------------------------------------------------------------
  //+++++++++++++ LOG SU SD CARD  ++++++++++++++++++++++++++++++++++++++++++
  SD.end(); //ricontrollo presenza effettiva della scheda SD
  delay(100);
  SD_ok = false;
  SD_ok = SD.begin();
  delay(100);

  if (SD_ok) {
    if (DEBBUG) Serial.println("...log su scheda SD...");
    //"Date;Time;Temp(*C);Hum(%);Pre(hPa);NH3(ppm);COx(ppm);NOx(ppm);PM2,5(ug/m3);PM10(ug/m3);PM1(ug/m3);O3;IAQ;CO(ppm);C3H8(ppm);C4H10(ppm);CH4(ppm);H2(ppm);C2H5OH(ppm)"
    String logvalue = "";
    logvalue += dayStamp; logvalue += ";";
    logvalue += timeStamp; logvalue += ";";
    logvalue += floatToComma(temp); logvalue += ";";
    logvalue += floatToComma(hum); logvalue += ";";
    logvalue += floatToComma(pre); logvalue += ";";
    logvalue += floatToComma(MICS6814_NH3); logvalue += ";";
    logvalue += floatToComma(MICS6814_CO); logvalue += ";";
    logvalue += floatToComma(MICS6814_NO2); logvalue += ";";
    logvalue += floatToComma(PM25); logvalue += ";";
    logvalue += floatToComma(PM10); logvalue += ";";
    logvalue += floatToComma(PM1); logvalue += ";";
    logvalue += ""; logvalue += ";";
    logvalue += floatToComma(IAQ); logvalue += ";";
    logvalue += floatToComma(COppm); logvalue += ";";
    logvalue += floatToComma(MICS6814_C3H8); logvalue += ";";
    logvalue += floatToComma(MICS6814_C4H10); logvalue += ";";
    logvalue += floatToComma(MICS6814_CH4); logvalue += ";";
    logvalue += floatToComma(MICS6814_H2); logvalue += ";";
    logvalue += floatToComma(MICS6814_C2H5OH);
    File filelog = SD.open(logpath, FILE_APPEND);
    delay(100);
    if (!filelog) {
      Serial.println("errore su apertura scheda SD...");
      return;
    }
    if (!filelog.println(logvalue)) {
      Serial.println("Append FALLITO");
    }
    filelog.close();
  } else {
    Serial.println("Errore lettore SD CARD!");
    u8g2.drawXBMP(72, 0, 16, 16, blank_icon16x16);
    u8g2.sendBuffer();
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  //------------------------------------------------------------------------
  //+++++++++++ AGGIORNO DISPLAY  ++++++++++++++++++++++++++++++++++++++++++
  displayMeasures();
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  //------------------------------------------------------------------------
  //+++++++++++ CHECK connessione - RICONNESSIONE  +++++++++++++++++++++
  if (WiFi.status() != WL_CONNECTED && cfg_ok) {  // dentro se devo ritentare una connessione
    if (DEBBUG) Serial.println("...ritento la connessione...");
    connesso_ok = false;
    dataora_ok = false;
    WiFi.disconnect();
    u8g2.drawXBMP(52, 0, 16, 16, blank_icon16x16);
    u8g2.drawXBMP(112, 0, 16, 16, nocon_icon16x16);
    u8g2.sendBuffer();
    delay(2000);
    if (DEBBUG) Serial.println("disconnesso e aspetto...");

    // -------RICONNESSIONE WIFI----------
    WiFi.begin(ssid.c_str(), pwd.c_str());
    int ritento = 1;
    int riprovato = 1;
    if (DEBBUG) Serial.println("ri-connessione...");
    delay(3000);

    // ciclo di attesa connessione...
    while (WiFi.status() != WL_CONNECTED) {
      delay(3000);
      ritento = ritento + 1;
      if (DEBBUG) Serial.print(ritento); Serial.print("-");
      if (ritento >= 5) {  // qui attendo fino a 3 secondi di avere connessione
        WiFi.disconnect();
        delay(300);
        WiFi.begin(ssid.c_str(), pwd.c_str());
        delay(3000);
        ritento = 1;
        riprovato = riprovato + 1; if (DEBBUG) Serial.print(riprovato); Serial.print("*");
      }// fine if ritenta

      if (riprovato >= 5) {
        if (DEBBUG) Serial.println("***** impossibile connettersi al wifi - riprovo tra 1 minuto...");
        //ritento=1;
        //riprovato=1;
        if (DEBBUG) Serial.println("uso il break..");
        break; //esco dal while...
      }// fine IF TIMEOUT ARRIVATO
    }// fine WHILE esco da loop se wifi connesso... o per timeout
    if (DEBBUG) Serial.println("fuori da while...");

    // aggiorno lo stato se WIFI connesso
    if (WiFi.status() == WL_CONNECTED) {
      connesso_ok = true;
      u8g2.drawXBMP(112, 0, 16, 16, wifi1_icon16x16);
      u8g2.sendBuffer();
      //lancio connessione a server orario e sincronizzo l'ora
      syncNTPTime();
      if (dataora_ok) {
        u8g2.drawXBMP(52, 0, 16, 16, clock_icon16x16);
        u8g2.sendBuffer();
      }
    } else {
      connesso_ok = false;
      dataora_ok = false;
    }
  }// FINE If not connected
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  //------------------------------------------------------------------------
  //++++++ AGGIORNAMENTO SERVER ++++++++++++++++++++++++++++++++++++++++++++
  if (connesso_ok && dataora_ok) { // dentro se connesso al wifi e data e ora sono ok
    if (DEBBUG) Serial.println("...upload dei dati sul SERVER");
    u8g2.drawXBMP(92, 0, 16, 16, arrow_up_icon16x16);
    u8g2.sendBuffer();
    if (DEBBUG) Serial.println("...disegnata icona...");
    // trasmissione su THINGSPEAK
    if (DEBBUG) Serial.print(temp);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(hum);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(pre);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(IAQ);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(MICS6814_CO);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(PM1);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(PM25);
    if (DEBBUG) Serial.print("-");
    if (DEBBUG) Serial.print(PM10);
    if (DEBBUG) Serial.println("---;");


    if (client.connect("api.milanosmartpark.net", 80)) {    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      String postStr = "apikey=";
      postStr += codice;
      postStr += "&temp=";
      postStr += String(temp);
      postStr += "&hum=";
      postStr += String(hum);
      postStr += "&pre=";
      postStr += String(pre);
      postStr += "&voc=";
      postStr += String(IAQ);
      postStr += "&co=";
      postStr += String(MICS6814_NH3);
      postStr += "&cox=";
      postStr += String(MICS6814_CO);
      postStr += "&nox=";
      postStr += String(MICS6814_NO2);
      postStr += "&pm1=";
      postStr += String(PM1);
      postStr += "&pm25=";
      postStr += String(PM25);
      postStr += "&pm10=";
      postStr += String(PM10);
      postStr += "&mac=";
      postStr += getMacAdr(mac);
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
      if (DEBBUG) Serial.println("...fine print dei CLIENT.PRINT...");
      postStr = "";
    } else {
      Serial.println(".............errore........");
      Serial.println(".............non connesso........");
      Serial.println(".............non connesso........");
    }

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    delay(1000);
    client.stop();

    u8g2.drawXBMP(92, 0, 16, 16, blank_icon16x16);
    u8g2.sendBuffer();

  } // fine if connesso OK
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //------------------------------------------------------------------------


  // +++++++++ RECUPERO MAC ADDRESS +++++++++++++++
  delay(900);// ritardo su ciclo di circa 1 secondo
  MACA = getMacAdr(mac);


  // +++++++++ STANDBY FOR PMS5003 AND MICS6814 ++++++++++++++
  if (PMS_run) pms.sleep();
  if (MICS6814_run) {
    gas.powerOff();
    gas.ledOff();
  }
  delay(1000);
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


  // ++++++++ DEEP SLEEP +++++++++++++++++++++++++++++++++++++++++
  Serial.println();
  Serial.println("Attivo lo sleep...");
  drawScrHead();
  u8g2.drawStr(5, 32, "MAC:");
  u8g2.setCursor(17, 44);
  u8g2.print(MACA.c_str());
  u8g2.drawXBMP(52, 0, 16, 16, blank_icon16x16);
  u8g2.drawXBMP(112, 0, 16, 16, nocon_icon16x16);
  u8g2.setFont(u8g2_font_6x13_tf); u8g2.drawStr(25, 62, "SLEEP ATTIVO");
  u8g2.sendBuffer();
  esp_deep_sleep_start();
  Serial.println("Se leggi questo, lo sleep non ha funzionato!");
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


}// end of LOOP
