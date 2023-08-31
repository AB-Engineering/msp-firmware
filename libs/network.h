/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

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

  log_i("Setting WiFi STATION mode...");
  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  WiFi.setTxPower(wifipow); // Set WiFi transmission power
  log_i("WIFIPOW set to %d", wifipow);
  log_i("Legend: -4(-1dBm), 8(2dBm), 20(5dBm), 28(7dBm), 34(8.5dBm), 44(11dBm), 52(13dBm), 60(15dBm), 68(17dBm), 74(18.5dBm), 76(19dBm), 78(19.5dBm)\n");

  for (short retry = 0; retry < 4; retry++) { // Scan WiFi for selected network and connect
    log_i("Scanning WiFi networks...");
    short networks = WiFi.scanNetworks(); // WiFi.scanNetworks will return the number of networks found
    log_i("Scanning complete\n");

    if (networks > 0) { // Looking through found networks
      log_i("%d networks found\n", networks);
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
        log_i("Connecting to %s, please wait...", ssid.c_str());

        WiFi.begin(ssid.c_str(), passw.c_str());
        auto start = millis(); // starting time
        while (WiFi.status() != WL_CONNECTED) {
          auto timeout = millis() - start;
          if (timeout > 10000) {
            WiFi.disconnect();
			log_e("Can't connect to network!\n");
            drawLine("WiFi connect err!", 2);
            break;
          }
        }
        
        if (WiFi.status() == WL_CONNECTED) { // Connection successful

          return true;
        }
      } else {
        log_e("%s not found!\n", ssid.c_str());
        String noNet = "NO " + ssid + "!";
        drawLine(noNet.c_str(), 2);
      }
    } else {
      log_e("No networks found!\n");
      drawLine("No networks found!", 2);
    }

    if (retry < 3) { // Print remaining tries
      log_i("Retrying, %d retries left\n", 3 - retry);
      String remain = String(3 - retry) + " tries remain.";
      drawTwoLines("Retrying...", remain.c_str(), 2);
    } else if (retry == 3) {
      log_e("No internet connection!\n");
      drawLine("No internet!", 2);
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
  drawTwoLines("Connecting to", "WiFi...", 1);
  connected_ok = connectWiFi();
  if (connected_ok) {
    Serial.println("Connection with " + ssid + " made successfully!\n");
    drawLine("WiFi connected!", 1);
    Serial.println("Retrieving date&time from NTP...");
    drawTwoLines("Getting date&time...", "Please wait...", 1);
    auto start = millis();
    while (!datetime_ok) { // Connecting with NTP server and retrieving date&time
      auto timeout = millis() - start;
      datetime_ok = getLocalTime(&timeinfo);
      if (datetime_ok || timeout > 120000) break;
    }
    if (datetime_ok) {
      drawTwoLines("Getting date&time...", "OK!", 1);
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

void connectToServer() {

  time_t epochTime = mktime(&timeinfo); // converting UTC date&time in UNIX Epoch Time format

  client.setVerificationTime((epochTime / 86400UL) + 719528UL, epochTime % 86400UL); // setting SLLClient's verification time to current time while converting UNIX Epoch Time to BearSSL's expected format

  auto start = millis(); // time for connection
  Serial.println("Uploading data to server through HTTPS in progress...\n");
  drawTwoLines("Uploading data", "to server...", 0);

  short retries = 0;
  while (retries < 4) {
    if (client.connect(server.c_str(), 443)) {
      log_i("Connection to server made! Time: %d\n", millis() - start);
      // Building the post string:
      String postStr = "X-MSP-ID=" + deviceid;
      if (BME_run) {
        postStr += "&temp=";
        postStr += String(temp, 3);
        postStr += "&hum=";
        postStr += String(hum, 3);
        postStr += "&pre=";
        postStr += String(pre, 3);
        postStr += "&voc=";
        postStr += String(VOC, 3);
      }
      if (MICS_run) {
        postStr += "&cox=";
        postStr += String(MICS_CO, 3);
        postStr += "&nox=";
        postStr += String(MICS_NO2, 3);
        postStr += "&nh3=";
        postStr += String(MICS_NH3, 3);
      }
      if (PMS_run) {
        postStr += "&pm1=";
        postStr += String(PM1);
        postStr += "&pm25=";
        postStr += String(PM25);
        postStr += "&pm10=";
        postStr += String(PM10);
      }
      if (O3_run) {
        postStr += "&o3=";
        postStr += String(ozone, 3);
      }
      postStr += "&msp=";
      postStr += String(MSP);
      postStr += "&recordedAt=";
      postStr += String(epochTime);

      // Sending client requests
      String postLine = "POST /api/v1/records HTTP/1.1\nHost: " + server + "\nAuthorization: Bearer " + api_secret_salt + ":" + deviceid + "\n";
      postLine += "Connection: close\nUser-Agent: MilanoSmartPark\nContent-Type: application/x-www-form-urlencoded\nContent-Length: " + String(postStr.length());
      log_d("Post line:\n%s\n", postLine.c_str());
      log_d("Post string: %s\n", postStr.c_str());
      client.print(postLine + "\n\n" + postStr);
      client.flush();

      // Get answer from server
      start = millis();
      String answLine = "";
      while (client.available()) {
        char c = client.read();
        answLine += c;
        auto timeout = millis() - start;
        if (timeout > 10000) break;
      }
      client.stop(); // Stopping the client

      // Check server answer
      if (answLine.startsWith("HTTP/1.1 201 Created", 0)) {
        log_i("Server answer ok! Data uploaded successfully!\n");
        drawTwoLines("Data uploaded", "successfully!", 2);
        sent_ok = true;
      } else {
        log_e("Server answered with an error! Data not uploaded!\n");
        log_e("The full answer is:\n%s\n", answLine.c_str());
        drawTwoLines("Serv answ error!", "Data not sent!", 10);
      }
      break; // exit

    } else {
      log_e("Error while connecting to server!");
      String mesg = "";
      if (retries == 3) {
        log_e("Data not uploaded!\n");
        mesg = "Data not sent!";
      } else {
        log_i("Trying again, %d retries left...\n", 3 - retries);
        mesg = String(3 - retries) + " retries left...";
      }
      drawTwoLines("Serv conn error!", mesg.c_str(), 10);
      retries++;

    }
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++