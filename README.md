# DESCRIPTION
This repository contains Arduino code for operating a waterproof [DS18B20 temperature](https://www.adafruit.com/product/381) probe and datalogger intended for use in hydrologic studies. The code was developed and tested on the [Adafruit Adalogger M0](https://www.adafruit.com/product/2796) based on the SAMD21 processor but likely will work on similar boards. This code contains functions for sending daily Iridium satellite communications using the [RockBlock 9603](https://www.iridium.com/products/rock-seven-rockblock-9603/) satellite modem. The code requires several parameters be specified using a parameter file, this is explained below. In addition, a wiring diagram of the required circuit is provided. Because this script uses the RockBlock Iridium modem it is important to make sure that a line rental with Ground Control is active and the modem is registered with credits available.  

# DEPENDENCIES 
There are several libraries required for running this code in addition to having the Arduino IDE installed and [configured](https://learn.adafruit.com/adafruit-feather-m0-adalogger/setup) to work with the Adalogger M0, and are listed below.

- <OneWire.h> //Needed for oneWire communication 
- "RTClib.h" //Needed for communication with Real Time Clock
- <SPI.h>//Needed for working with SD card
- <SD.h>//Needed for working with SD card
- "ArduinoLowPower.h"//Needed for putting Feather M0 to sleep between samples
- <IridiumSBD.h>//Needed for communication with IRIDIUM modem 
- <CSV_Parser.h>//Needed for parsing CSV data

# LOGGER OPERATION
This section outlines the steps for operating the logger.

## Set up a parameter file
The first step to using the logger is to obtain a micro-SD card and add to it a parameter file the logger will use to define several important variables. This file must be named "PARAM.txt" (case sensitive),comma delimited, and should look like the example parameter file shown below:

```
filename,sample_intvl
MYFILE.CSV,10
```

Where the "filename" header declares the name of the log file to be written (!!MUST BE LESS THEN 8 CHARACTERS IN LENGTH!!) and is user defined, the "sampl_intvl" header stands for sample interval and declares the sampling rate of the logger in minutes. Note that the extension ".CSV" is capitalized. Note that every sample will be written to the SD card but only hourly averages will be sent over the IRIDIUM modem daily. The sapling rate should be <60 minutes.

## Set the time on the RTC
To keep track of time this code relies on a Real Time Clock, specifically the PCF8523. To set the time run the [pcf8523 script](https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-arduino) from the "RTClib" library and upload it to the MCU BEFORE uploading the logging script. We recommend setting RTC to UTC time to avoid complications with daylight savings time. 

## Launch the logger
Once the desired time has been set on the RTC, the logging script "DS18B20_water_temp.ino" can be uploaded. This script will sample the DS18B20 temperature probe at the specified sampling interval and record each sample to the Micro-SD card along with the sample datetime. In addition, at midnight of each day on the RTC, the logger will transmit the hourly averages for the day over the IRIDIUM modem.

1. To launch the logger, first remove the Micros-SD card, plug the Adalogger into a computer over USB, make sure the power switch is set to "ON" on the Feather Terminal block, and upload the code. Upon uploading the MCU should flash the built in LED at 1-sec intervals indicating the Micro-SD card cannot be initialized, this is ok as we removed it on purpose.
2. Unplug the MCU from the computer and toggle the power switch to "OFF" on the Feather Terminal Block.
3. Plug in the Micro-SD card (!!with "PARAM.txt" parameter file present!!) and plug in the 5-Pin to USB adapter to the Adalogger USB port.
4. Toggle the power switch to "ON" on the Feather Terminal Block, a reading should be taken immediately, indicated by short flash of built in LED, and the latching relay should be audible. 

After sampling the Adalogger is put into low power mode until the next sample. !!During this time, it will be impossible to upload code to the MCU without double clicking the reset button during upload, or removing the SD card and cycling the power to the MCU!! If for whatever reason after turning on the MCU the built in LED continues to flash at a given interval, consult the error codes section below to help identify the issue. Note that upon removing the Micro-SD card after logging there should be three files, the "PARAM.txt" file provided at launch, a log file with the name specified in "PARAM.txt", and one called "DAILY.CSV"; this file is used for Iridium transmissions and can be ignored, if deleted hourly data for the current day will be deleted, but subsequent days will be complete. 

## Error Codes
Errors are conveyed via the built in LED (Pin 13) on the Adalogger as repeated blinking at specific time intervals, these intervals and their meaning are listed below:

- Repeated 4-sec: Problem connecting to DS18B20 temperature probe, check wiring. 
- Repeated 2-sec: Problem initializing SD card, check SD is inserted, [formatted](https://www.arduino.cc/reference/en/libraries/sd/) correctly. 
- Repeated 1-sec: Problem parsing "PARAM.txt" parameter file, make sure it is present on SD card, named correctly, and formatted as specified above. 
- Repeated 0.5-sec: Problem initializing RTC, check that wiring is correct, and coin cell is inserted and at appropriate voltage.
- Two consecutive 1-sec blinks during Iridium transmission: Modem failed to begin, check wiring logging may continue.  
- Two consecutive 5-sec blinks during Iridium transmission: Send binary message failed, signal at location may not be adequate, however logging will continue, make sure a line rental is active with Ground Control and the modem is registered with credits available.  

- Note that the built in LED will be held HIGH during Iridium transmission to indicate that it is trying to send the message. 
- During normal operation while the MCU is sleeping the built in LED will be OFF, this is OK, the logger is still operating, however code cannot be uploaded without double clicking the reset button during upload.
- A 0.25-sec LED flash at the end of each sample will occur for each sample to indicate a sample was just taken, this is normal, and the latching relay should be heard during each sample as well.  
