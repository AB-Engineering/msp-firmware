//Firmware_N System Functions and Methods


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void drawScrHead() { //draws the screen header

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);

  // stato del sistema
  u8g2.setCursor(0, 13); u8g2.print("#" + codice + "#");

  if (SD_ok) {
    u8g2.drawXBMP(72, 0, 16, 16, sd_icon16x16);
  }
  if (dataora_ok) {
    u8g2.drawXBMP(92, 0, 16, 16, clock_icon16x16);
  }
  if (connected_ok) {
    u8g2.drawXBMP(112, 0, 16, 16, wifi1_icon16x16);
  } else {
    u8g2.drawXBMP(112, 0, 16, 16, nocon_icon16x16);
  }

  u8g2.drawLine(0, 17, 127, 17);

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

String floatToComma(float value) { //Converts float values in strings with the decimal part separated from the integer part by a comma

  String convert = String(value, 3);
  convert.replace(".", ",");
  return convert;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void displayMeasures() { //prints data on the U8g2 screen, on four pages

  if (DEBDUG) Serial.println("...aggiorno i dati del display...");

  // pagina 1
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("Temp:  " + floatToComma(temp) + "*C");
  u8g2.setCursor(5, 39); u8g2.print("Hum:  " + floatToComma(hum) + "%");
  u8g2.setCursor(5, 50); u8g2.print("Pre:  " + floatToComma(pre) + "hPa");
  u8g2.setCursor(5, 61); u8g2.print("PM10:  " + String(PM10) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 2
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("PM2,5:  " + String(PM25) + "ug/m3");
  u8g2.setCursor(5, 39); u8g2.print("PM1:  " + String(PM1) + "ug/m3");
  u8g2.setCursor(5, 50); u8g2.print("NOx:  " + floatToComma(MICS6814_NO2) + "ug/m3");
  u8g2.setCursor(5, 61); u8g2.print("CO:  " + floatToComma(MICS6814_CO) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 3
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("O3:  " + floatToComma(ozone) + "ug/m3");
  u8g2.setCursor(5, 39); u8g2.print("VOC:  " + floatToComma(VOC) + "kOhm");
  u8g2.setCursor(5, 50); u8g2.print("NH3:  " + floatToComma(MICS6814_NH3) + "ug/m3");
  u8g2.setCursor(5, 61); u8g2.print("C3H8:  " + floatToComma(MICS6814_C3H8) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

  // pagina 4
  drawScrHead();
  u8g2.setCursor(5, 28); u8g2.print("C4H10:  " + floatToComma(MICS6814_C4H10) + "ug/m3");
  u8g2.setCursor(5, 39); u8g2.print("CH4:  " + floatToComma(MICS6814_CH4) + "ug/m3");
  u8g2.setCursor(5, 50); u8g2.print("H2:  " + floatToComma(MICS6814_H2) + "ug/m3");
  u8g2.setCursor(5, 61); u8g2.print("C2H5OH:  " + floatToComma(MICS6814_C2H5OH) + "ug/m3");
  u8g2.sendBuffer();
  delay(5000);

}

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

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void appendFile(fs::FS &fs, String path, String message) {

  if (DEBDUG) {
    Serial.print("Appending to file: "); Serial.println(path);
  }
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    if (DEBDUG) Serial.println("Failed to open file for appending");
    return;
  }
  if (file.println(message)) {
    if (DEBDUG) Serial.println("Message appended");
  } else {
    if (DEBDUG) Serial.println("Append failed");
  }
  file.close();

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool addToLog(fs::FS &fs, String path, String message) { //adds new line to the log file at the top, after the header lines

  String temp = "";
  if (DEBDUG) Serial.println("ADDTOLOG: Log file is: " + path);
  File logfile = fs.open(path, FILE_READ);
  if (!logfile) {
    if (DEBDUG) Serial.println("ADDTOLOG: Error opening the log file!");
    return false;
  }
  File tempfile = fs.open("/templog", FILE_WRITE);
  if (!tempfile) {
    if (DEBDUG) Serial.println("ADDTOLOG: Error creating templog!");
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
    if (DEBDUG) Serial.println("ADDTOLOG: Error recreating the log file!");
    return false;
  }
  tempfile = fs.open("/templog", FILE_READ);
  if (!tempfile) {
    if (DEBDUG) Serial.println("ADDTOLOG: Error reopening templog!");
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

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool parseConfig(File fl) { //parses the configuration file on the SD card

  bool esito = true;
  String command[10];
  String temp;
  int i = 0;
  unsigned long lastpos = 0;
  if (DEBDUG) Serial.println();
  // Storing the config file in a string array
  while (fl.available() && i < 10) {
    fl.seek(lastpos);
    if (i == 0) temp = fl.readStringUntil('#');
    command[i] = fl.readStringUntil(';');
    temp = fl.readStringUntil('#');
    if (DEBDUG) {
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
    if (DEBDUG) Serial.println("PARSECFG: Errore! Comando sleep_time non riconosciuto. Fallback a 1 minuto.");
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
  /*
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
  */
  //iaqon
  /*
    if (command[8].startsWith("abilita_IAQ", 0)) {
    temp = "";
    temp = command[8].substring(command[8].indexOf("abilita_IAQ") + 12, command[8].length());
    if (temp.toInt() == 0 || temp.toInt() == 1) {
      //iaqon = temp.toInt();
    } else {
      //Serial.println("PARSECFG: Errore! Valore di abilita_IAQ non valido. Fallback a true.");
      //iaqon = true;
    }
    //Serial.printf("iaqon = %s\n", iaqon ? "true" : "false");
    } else {
    //Serial.println("PARSECFG: Errore! Comando abilita_IAQ non riconosciuto. Fallback a true.");
    //iaqon = true;
    }
  */
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

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void setDefaults() {

  wifipow = WIFI_POWER_19_5dBm;
  attesa = 20;
  avg_measurements = 5;
  avg_delay = 6;
  //MQ7_run = false;
  //iaqon = true;

}

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

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void initWifi() {

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  delay(1000); // Waiting a bit for Wifi mode set
  if (DEBDUG) Serial.printf("wifipow: %d\n", wifipow);
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
          connected_ok = true;
          Serial.println("\nWiFi CONNESSO....");
          drawScrHead();
          u8g2.drawStr(15, 45, "WiFi Connesso");
          u8g2.sendBuffer();
          delay(2000);
          //lancio connessione a server orario e sincronizzo l'ora
          syncNTPTime();
        } else {
          connected_ok = false;
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

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool CheckSensor() { //Checks BME680 status

  if (iaqSensor.status < BSEC_OK) {
    if (DEBDUG) Serial.printf("BSEC error, status %d!\n", iaqSensor.status);
    return false;
  } else if (iaqSensor.status > BSEC_OK && DEBDUG) {
    Serial.printf("BSEC warning, status %d!\n", iaqSensor.status);
  }

  if (iaqSensor.bme680Status < BME680_OK) {
    if (DEBDUG) Serial.printf("Sensor error, bme680_status %d!\n", iaqSensor.bme680Status);
    return false;
  } else if (iaqSensor.bme680Status > BME680_OK && DEBDUG) {
    Serial.printf("Sensor warning, status %d!\n", iaqSensor.bme680Status);
  }

  iaqSensor.status = BSEC_OK;
  return true;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool isAnalogO3Connected() {

  int test = analogRead(35);
  if (test >= 3500) return true;
  return false;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

float analogPpmO3Read(int *points) { //needs an external points variable to store raw adc value

  *points = 0;
  for (int i = 0; i < 10; i++) {
    *points += analogRead(32);
    delay(10);
  }
  *points /= 10;
  int currval = *points - 304;
  if (currval <= 0) return 0.0;
  return ((float(currval) / 3783.0) * 303.611557) / 1000.0;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

float convertPpmToUgM3(float ppm, float mm, float T, float P) {

  // mm is molar mass and must be g/mol
  if (!BME_run) { //if no BME is connected, assume OSHA standard conditions to perform the conversion
    T = 25.0;
    P = 1013.25;
  }
  const float R = 83.1446261815324; //gas constant (L * hPa * K^−1 * mol^−1)
  float Vm = (R * (T + 273.15)) / P; //molar volume (L * mol^-1)
  return (ppm * 1000) * (mm / Vm);

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
