/*
                        Milano Smart Park Firmware
                       Developed by Norman Mulinacci

          This code is usable under the terms and conditions of the
             GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

// Modem Management Functions

#ifndef MODEM_H
#define MODEM_H

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void initializeModem() {

  log_i("Initializing SIM800L modem serial connection...");

  // Set GSM module baud rate
  gsmSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);

  // Restart takes quite some time
  log_i("Issuing modem reset...");
  modem.restart();

  String name = modem.getModemName();
  log_d("Modem Name: %s", name.c_str());

  String modemInfo = modem.getModemInfo();
  log_d("Modem Info: %s", modemInfo.c_str());

  String ccid = modem.getSimCCID();
  log_d("CCID: %s", ccid.c_str());

  String imei = modem.getIMEI();
  log_d("IMEI: %s", imei.c_str());

  String imsi = modem.getIMSI();
  log_d("IMSI: %s", imsi.c_str());

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool connectToGPRS() {

  log_i("Waiting for network...");
  if (!modem.waitForNetwork()) {
    log_e("Network not found!");
    return false;
  }
/*
  if (!modem.isNetworkConnected()) {
    log_e("Couldn't find network!");
    return false;
  }
*/
  log_i("Connected to network!");

  short retries = 0;
  while (retries < 4) {
    log_i("Connecting to GPRS...");
    if (modem.gprsConnect("", "", "")) {
      log_i("GPRS connected!");
      break;
    }
    log_e("Connection failed!");
    if (retries < 3) {
      log_e("Retrying in 5 seconds...");
      delay(5000);
    }
    retries++;
  }

  if (!modem.isGprsConnected()) {
    log_e("GPRS is not connected!");
    return false;
  }

  return true;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#endif