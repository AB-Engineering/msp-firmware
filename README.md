# Milano Smart Park Firmware for Arduino IDE

A&A Milano Smart Park Project

Firmware developed with the Arduino IDE by Norman Mulinacci @ 2020

The project runs on Espressif's ESP32-DevkitC with ESP32-WROVER-B module

### Required external libraries (you can also download them through the Arduino IDE):

- [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32)
	+ You need to add the following URLs in File -> Settings -> Additional URL to download the core in the IDE: https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
- [U8g2 Arduino library](https://github.com/olikraus/U8g2_Arduino)
- [SSLClient Library](https://github.com/OPEnSLab-OSU/SSLClient)
- [BSEC Arduino library](https://github.com/BoschSensortec/BSEC-Arduino-library)
	+ In order for the linker to work properly, you need to perform the modifications described in the BSEC page. They work fine in Arduino IDE v1.8.13.
- [PMS Library](https://github.com/fu-hsi/pms)
- MiCS6814-I2C Library (custom)
