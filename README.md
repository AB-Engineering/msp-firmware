# msp-firmware
A&A Milano Smart Park Project

Firmware developed with the Arduino IDE by Norman Mulinacci @ 2020

The project runs on Espressif's ESP32-DevkitC with ESP32-WROVER-B module

### Required external libraries (you can download them through the Arduino IDE):

- [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32)
- [BSEC Arduino library](https://github.com/BoschSensortec/BSEC-Arduino-library)
- [U8g2 Arduino library](https://github.com/olikraus/U8g2_Arduino)
- [SSLClient Library](https://github.com/OPEnSLab-OSU/SSLClient)
- PMS_Extended Library (custom)
- MiCS6814-I2C Library (custom)

### Hardware:

- ESP32-DevkitC with ESP32-WROVER-B module
- 1.3" 128x64 OLED screen SH1106 (IIC interface)
      
                         ____________________
                        |                     |
               3V3--- o-|-3V3             GND-|-o
                      o-|-EN              D23-|-o --- MOSI SDCARD
                      o-|-VP(D36)         D22-|-o --- SCL OLED+BME+MICS6814
                      o-|-VN(D39)     (D1)TX0-|-o
                      o-|-D34         (D3)RX0-|-o
     ZE25-O3 D0 ----- o-|-D35             D21-|-o --- SDA OLED+BME+MICS6814
     ZE25-O3 A0 ----- o-|-D32             GND-|-o
                      o-|-D33             D19-|-o --- MISO SDCARD u8
                      o-|-D25             D18-|-o --- SCK SDCARD
                      o-|-D26             D5 -|-o --- CS SDCARD
                      o-|-D27             D17-|-o  -x-x- protected, don't use
       RX SENS PM --- o-|-D14             D16-|-o  -x-x- protected, don't use
       TX SENS PM --- o-|-D12             D4 -|-o
                      o-|-GND             D0 -|-o
                      o-|-D13             D2 -|-o
                      o-|-SD2  ---xxx     D15-|-o  -x-x- protected, don't use
                      o-|-SD3  ---xxx---  SD1-|-o  -x-x- protected, don't use
                      o-|-CMD  ---xxx---  SD0-|-o  -x-x- protected, don't use
       VCC-SD+OLED--- o-|-5Vin    xxx---  CLK-|-o  -x-x- protected, don't use
                        |                     |
                        |      _______        |
                        |     |       |       |
                        |     |       |       |
                        |_____|       |_______|
                              |_______|