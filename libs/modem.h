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
  log_i("Issuing modem reset (takes some time)...");
  modem.restart();

  log_i("Done!");

  log_d("Modem Name: %s", modem.getModemName().c_str());

  log_d("Modem Info: %s", modem.getModemInfo().c_str());

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool connectToGPRS() {

  short retries = 0;
  while (retries < 2) {
    log_i("Waiting for network...");
    if (modem.waitForNetwork()) {
      log_i("Connected to network!");
      break;
    }
    log_e("Couldn't find network!");
    if (retries < 1) {
      log_i("Retrying in 10 seconds...");
      delay(10000);
    }
    retries++;
  }

  if (!modem.isNetworkConnected()) {
    log_e("Network is not connected!");
    return false;
  }

  retries = 0;
  while (retries < 4) {
    log_i("Connecting to GPRS...");
    if (modem.gprsConnect("", "", "")) {
      log_i("GPRS connected!");
      break;
    }
    log_e("Connection failed!");
    if (retries < 3) {
      log_i("Retrying in 5 seconds...");
      delay(5000);
    }
    retries++;
  }

  if (!modem.isGprsConnected()) {
    log_e("GPRS is not connected!");
    return false;
  }

  log_d("CCID: %s", modem.getSimCCID().c_str());

  log_d("IMEI: %s", modem.getIMEI().c_str());

  log_d("IMSI: %s", modem.getIMSI().c_str());

  log_d("Operator: %s", modem.getOperator().c_str());

  log_d("Signal quality: &d", modem.getSignalQuality());

  return true;

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#endif