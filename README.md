# DS18B20_water_temp
Arduino script for measuring temperature (water temp) using DS18B20, with IRIDIUM satellite communications. 

# Installation
To run this package the [Arduino IDE](https://www.arduino.cc/en/software) must be installed and [configured](https://learn.adafruit.com/adafruit-feather-m0-adalogger/setup).
This script can then be cloned into the users Sketchbook location directory. Note, this code has been tested
only on a [Adafruit Adalogger](https://learn.adafruit.com/adafruit-feather-m0-adalogger/) based on the ATSAMD21G18 
ARM Cortex M0 processor, but should work on most boards based on this chip such as the [Adruino MKR Zero](https://store-usa.arduino.cc/products/arduino-mkr-zero-i2s-bus-sd-for-sound-music-digital-audio-data?selectedStore=us).
See the Dependencies section for list of required librires. Also not that becuase this script employs the "ArduinoLowPower" library,
attempted uploads to the board while asleep will fail. To upload to the board while its asleep, double-click the 
reset button when the Arduino IDE displays "Uploading ...", or remove the SD card and reset the power, the built in LED
will flash indicating no SD card, but the MCU will not enter sleep mode and code can be uploaded. 

# Dependencies 

- <OneWire.h> //Needed for oneWire communication 
- <DallasTemperature.h> //Needed for communication with DS18B20
- "RTClib.h" //Needed for communication with Real Time Clock
- <SPI.h> //Needed for working with SD card
- <SD.h> //Needed for working with SD card
- "ArduinoLowPower.h" //Needed for putting Feather M0 to sleep between samples
- <IridiumSBD.h> //Needed for communication with IRIDIUM modem 
- <CSV_Parser.h> //Needed for parsing CSV data

# Wiring Diagram 

Please find a wiring schematic [here](). 

# Error Codes

The built in LED on the MCU will blink at specific time intervals to convey diffrent errors. 

- Repaeated 1-sec blink -> Cannot intitalize SD card, likley missing or ill formated.
- Repeated 0.5-sec blink -> The RTC cannot be initilized, check wiring and coin cell battery. 
- 2 5-sec blinks -> The attempted IRIDIUM transmission failed, however, code will continue. 



 