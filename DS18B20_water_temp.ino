// Include the libraries we need
#include <OneWire.h> //Needed for oneWire communication 
#include <DallasTemperature.h> //Needed for communication with DS18B20
#include "RTClib.h" //Needed for communication with Real Time Clock
#include <SPI.h>//Needed for working with SD card
#include <SD.h>//Needed for working with SD card
#include "ArduinoLowPower.h"//Needed for putting Feather M0 to sleep between samples
#include <IridiumSBD.h>//Needed for communication with IRIDIUM modem 
#include <CSV_Parser.h>/*Needed for parsing CSV data*/ 

// Data wire is plugged into port 12 on the Feather M0
#define ONE_WIRE_BUS 12

// Define Iridium seriel communication COM1
#define IridiumSerial Serial1


/*Create library instances*/
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Setup a PCF8523 Real Time Clock instance
RTC_PCF8523 rtc;

// Setup a log file instance
File dataFile;

// Declare the IridiumSBD object
IridiumSBD modem(IridiumSerial);

/*Define global constants*/
const byte LED = 13; // Built in LED pin
const byte chipSelect = 4; // For SD card
const byte TmpPwrPin = 11; // Pwr pin to temp. probe
const byte IridPwrPin = 6; // Pwr pin to Iridium modem



/*Define global vars */
String datestamp; //For printing to SD card and IRIDIUM payload
char **filename; //Name of log file
String filestr;//Filename as String 
int16_t *sample_intvl;//Sample interval in minutes
DateTime IridTime;//Varible for keeping IRIDIUM transmit time
int err; //IRIDIUM status var
uint32_t sleep_time;//Logger sleep time in milliseconds

/*Define functions */
// Function takes a current DateTime and updates the date stamp as YYYY-MM-DD HH:MM:SS
void gen_datestamp(DateTime current_time)
{

  String month_str;
  String day_str;
  String hour_str;
  String min_str;
  String sec_str;

  String year_str = String(current_time.year());

  int month_int = current_time.month();
  if (month_int < 10)
  {
    month_str = "0" + String(month_int);
  } else {
    month_str = String(month_int);
  }

  int day_int = current_time.day();
  if (day_int < 10)
  {
    day_str = "0" + String(day_int);
  } else {
    day_str = String(day_int);
  }

  int hour_int = current_time.hour();
  if (hour_int < 10)
  {
    hour_str = "0" + String(hour_int);
  } else {
    hour_str = String(hour_int);
  }

  int min_int = current_time.minute();
  if (min_int < 10)
  {
    min_str = "0" + String(min_int);
  } else {
    min_str = String(min_int);
  }

  int sec_int = current_time.second();
  if (sec_int < 10)
  {
    sec_str = "0" + String(sec_int);
  } else {
    sec_str = String(sec_int);
  }

  datestamp = year_str + "-" + month_str + "-" + day_str + " " + hour_str + ":" + min_str + ":" + sec_str;
}

/*Function reads data from a daily logfile, and uses Iridium modem to send all observations
   for the previous day over satellite at midnight on the RTC.
*/
void send_daily_data(DateTime now)
{

  //For capturing Iridium errors
  int err;

  //Provide power to Iridium Modem
  digitalWrite(IridPwrPin, HIGH);
  delay(30);


  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200);

  // Begin satellite modem operation
  err = modem.begin();
  if (err != ISBD_SUCCESS)
  {
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
  }


  //Set paramters for parsing the log file
  CSV_Parser cp("sf", true, ',');

  //Varibles for holding data fields
  char **datetimes;
  float *h2o_temps;

  //Read IRID.CSV
  cp.readSDfile("/DAILY.CSV");


  //Populate data arrays from logfile
  datetimes = (char**)cp["datetime"];
  h2o_temps = (float*)cp["temp_c"];

  //Binary bufffer for iridium transmission (max allowed buffer size 340 bytes)
  uint8_t dt_buffer[340];
  int buff_idx = 0;

  //Get the start datetime stamp as string
  String datestamp = String(datetimes[0]).substring(0, 10);

  for (int i = 0; i < datestamp.length(); i++)
  {
    dt_buffer[buff_idx] = datestamp.charAt(buff_idx);
    buff_idx++;
  }

  dt_buffer[buff_idx] = ':';
  buff_idx++;


  for (int day_hour = 0; day_hour < 24; day_hour++)
  {


    float mean_temp = 999.0;
    boolean is_obs = false;
    int N = 0;

    //For each observation in the CSV
    for (int i = 0; i < cp.getRowsCount(); i++) {

      String datetime = String(datetimes[i]);
      int dt_hour = datetime.substring(11, 13).toInt();

      if (dt_hour == day_hour)
      {

        float h2o_temp = h2o_temps[i];

        if (is_obs == false)
        {
          mean_temp = h2o_temp;
          is_obs = true;
          N++;
        } else {
          mean_temp = mean_temp + h2o_temp;
          N++;
        }

      }
    }

    if (N > 0)
    {
      mean_temp = (mean_temp / N) * 10.0;
    }

    String datastring = String(round(mean_temp)) + ':';

    for (int i = 0; i < datastring.length(); i++)
    {
      dt_buffer[buff_idx] = datastring.charAt(i);
      buff_idx++;
    }

  }

  digitalWrite(LED, HIGH);
  //transmit binary buffer data via iridium
  err = modem.sendSBDBinary(dt_buffer, buff_idx);
  digitalWrite(LED, LOW);

  if(err != ISBD_SUCCESS)
  {
    digitalWrite(LED, HIGH);
    delay(5000);
    digitalWrite(LED, LOW);
    delay(5000);
    digitalWrite(LED, HIGH);
    delay(5000);
    digitalWrite(LED, LOW);
    delay(5000);
  }

  err = modem.sleep();


  //Kill power to Iridium Modem
  digitalWrite(IridPwrPin, LOW);
  delay(30);


  //Remove previous daily values CSV
  SD.remove("/DAILY.CSV");

  //Update IridTime to next day at midnight 
  IridTime = DateTime(now.year(), now.month(), now.day() + 1, 0, 0, 0);
}


/*
   The setup function. We only start the sensors, RTC and SD here
*/
void setup(void)
{
  // Set pin modes
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(TmpPwrPin, OUTPUT);
  digitalWrite(TmpPwrPin, LOW);
  pinMode(IridPwrPin, OUTPUT);
  digitalWrite(IridPwrPin, LOW);

  // Start up the DallasTemp library
  sensors.begin();

  //Make sure a SD is available (1-sec flash LED means SD card did not initialize)
  while (!SD.begin(chipSelect)) {
    digitalWrite(LED, HIGH);
    delay(2000);
    digitalWrite(LED, LOW);
    delay(2000);
  }

  //Set paramters for parsing the log file
  CSV_Parser cp("sd", true, ',');


  //Read IRID.CSV
  while(!cp.readSDfile("/PARAM.txt"))
  {
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
  }


  //Populate data arrays from logfile
  filename = (char**)cp["filename"];
  sample_intvl = (int16_t*)cp["sample_intvl"];

  sleep_time = sample_intvl[0] * 60000;
  filestr = String(filename[0]);


  // Make sure RTC is available
  while (!rtc.begin())
  {
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  //Get current datetime
  DateTime now = rtc.now();

  //Set iridium transmit time (IridTime) to end of current day (midnight)
  IridTime = DateTime(now.year(), now.month(), now.day() + 1, 0, 0, 0);


}

/*
   Main function, get and show the temperature
*/
void loop(void)
{

  //Power the DS18B20
  digitalWrite(TmpPwrPin, HIGH);

  //Set resolution to 12 bit, 10 bit is too corse
  sensors.setResolution(12);

  //Get current datetime from RTC
  DateTime now = rtc.now();

  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures

  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  float tempC = sensors.getTempCByIndex(0);

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C)
  {

    //Gen current time stamp
    gen_datestamp(now);

    //Datastring to write to SD card
    String datastring = datestamp + "," + String(tempC);

    //Write header if first time writing to the file
    if (!SD.exists(filestr.c_str()))
    {
      dataFile = SD.open(filestr.c_str(), FILE_WRITE);
      if (dataFile)
      {
        String header = "datetime,temp_c";
        dataFile.println(header);
        dataFile.close();
      }

    } else {
      //Write datastring and close logfile on SD card
      dataFile = SD.open(filestr.c_str(), FILE_WRITE);
      if (dataFile)
      {
        dataFile.println(datastring);
        dataFile.close();
      }
    }


    //Power down DS18B20
    digitalWrite(TmpPwrPin, LOW);
    delay(1);

    //If new day, send daily temp. stats over IRIDIUM modem
    if (now >= IridTime)
    {
      send_daily_data(now);
    }

    //Write header if first time writing to the file
    if (!SD.exists("DAILY.CSV"))
    {
      dataFile = SD.open("DAILY.CSV", FILE_WRITE);
      if (dataFile)
      {
        String header = "datetime,temp_c";
        dataFile.println(header);
        dataFile.close();
      }

    } else {
      //Write datastring and close logfile on SD card
      dataFile = SD.open("DAILY.CSV", FILE_WRITE);
      if (dataFile)
      {
        dataFile.println(datastring);
        dataFile.close();
      }
    }
    
    //Flash LED to indicate sample taken 
    digitalWrite(LED, HIGH);
    delay(250);
    digitalWrite(LED, LOW);
    delay(250);

    //Put logger in low power mode for lenght 'sleep_time'
    LowPower.sleep(sleep_time);



  } else {
    // Indicate to user there was an issue by blinking built in LED
    while (1) {
      digitalWrite(LED, HIGH);
      delay(4000);
      digitalWrite(LED, LOW);
      delay(4000);
    }
  }

}
