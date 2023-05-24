/*
                        Milano Smart Park Firmware
                      Copyright (c) Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007

             Parts of this code are based on open source works
                 freely distributed by Luca Crotti @2019
*/

// Network Management Functions

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void timeavailable(struct timeval *t) { // Callback function (gets called when time adjusts via NTP)

  Serial.println("Got time adjustment from NTP!\n");

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool connectWiFi() { // sets WiFi mode and tx power (var wifipow), performs connection to WiFi network (vars ssid, passw)

  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  delay(1500); // Waiting a bit for Wifi mode to set

  WiFi.setTxPower(wifipow); // Set WiFi transmission power
  log_i("WIFIPOW set to %d", wifipow);
  log_i("Legend: -4(-1dBm), 8(2dBm), 20(5dBm), 28(7dBm), 34(8.5dBm), 44(11dBm), 52(13dBm), 60(15dBm), 68(17dBm), 74(18.5dBm), 76(19dBm), 78(19.5dBm)\n");

  for (short retry = 0; retry < 4; retry++) { // Scan WiFi for selected network and connect
    log_i("Scanning WiFi networks...");
    drawTwoLines("Scanning networks...", "", 0);
    short networks = WiFi.scanNetworks(); // WiFi.scanNetworks will return the number of networks found
    log_i("Scanning complete\n");
    drawTwoLines("Scanning networks...", "Scanning complete", 1);

    if (networks > 0) { // Looking through found networks
      log_i("%d networks found\n", networks);
      String netCnt = "Networks found: " + String(networks);
      drawTwoLines(netCnt.c_str(), "", 0);
      bool ssid_ok = false; // For selected network if found
      for (short i = 0; i < networks; i++) { // Prints SSID and RSSI for each network found, checks against ssid
        log_v("%d: %s(%d) %s%c", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "ENCRYPTED", i == networks - 1 ? '\n' : ' ');
        if (WiFi.SSID(i) == ssid) {
          ssid_ok = true;
          break;
        }
      }

      if (ssid_ok) { // Begin connection
        log_i("%s found!\n", ssid.c_str());
		String foundNet = ssid + " OK!";
        drawTwoLines(netCnt.c_str(), foundNet.c_str(), 2);
        log_i("Connecting to %s, please wait...", ssid.c_str());
        drawTwoLines("Wait for conn. to: ", ssid.c_str(), 0);

        WiFi.begin(ssid.c_str(), passw.c_str());
        auto start = millis(); // starting time
        while (WiFi.status() != WL_CONNECTED) {
          auto timeout = millis() - start;
          if (timeout > 10000) {
            log_e("Can't connect to network!\n");
            drawLine("WiFi connect err!", 3);
            WiFi.disconnect();
            break;
          }
        }
        
        if (WiFi.status() == WL_CONNECTED) { // Connection successful

          return true;
        }
      } else {
        log_e("%s not found!\n", ssid.c_str());
        String noNet = "NO " + ssid + "!";
        drawTwoLines(netCnt.c_str(), noNet.c_str(), 4);
      }
    } else {
      log_e("No networks found!\n");
      drawLine("No networks found!", 2);
    }

    if (retry < 3) { // Print remaining tries
      log_i("Retrying, %d retries left\n", 3 - retry);
      String remain = String(3 - retry) + " tries remain.";
      drawTwoLines("Retrying...", remain.c_str(), 3);
    } else if (retry == 3) {
      log_e("No internet connection!\n");
      drawLine("No internet!", 3);
    }
  }

  return false;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void connAndGetTime() { // calls connectWifi and retrieves time from NTP server
  
  datetime_ok = false; // resetting the date&time var
  sntp_set_time_sync_notification_cb(timeavailable);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2)
  Serial.println("Connecting to WiFi...\n");
  connected_ok = connectWiFi();
  if (connected_ok) {
    Serial.println("Connection with " + ssid + " made successfully!\n");
    drawLine("WiFi connected!", 2);
    Serial.println("Retrieving date&time from NTP...");
    drawTwoLines("Getting date&time...", "Please wait...", 0);
    auto start = millis();
    while (!datetime_ok) { // Connecting with NTP server and retrieving date&time
      auto timeout = millis() - start;
      datetime_ok = getLocalTime(&timeinfo);
      if (datetime_ok || timeout > 120000) break;
      delay(5000);
    }
    if (datetime_ok) {
      drawTwoLines("Getting date&time...", "OK!", 2);
      strftime(Date, sizeof(Date), "%d/%m/%Y", &timeinfo); // Formatting date as DD/MM/YYYY
      strftime(Time, sizeof(Time), "%T", &timeinfo); // Formatting time as HH:MM:SS
      String tempT = String(Date) + " " + String(Time);
      Serial.println("Current date&time: " + tempT);
      drawTwoLines("Date & Time:", tempT.c_str(), 0);
    } else {
      log_e("Failed to obtain date&time! Is this WiFi network connected to the internet?\n");
      drawTwoLines("Date & time err!", "Is internet ok?", 0);
    }
    Serial.println();
    delay(3000);
  }

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
