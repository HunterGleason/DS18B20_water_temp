// Include the libraries we need
#include <OneWire.h> //Needed for oneWire communication 
#include <DallasTemperature.h> //Needed for communication with DS18B20
#include "RTClib.h" //Needed for communication with Real Time Clock
#include <SPI.h>//Needed for working with SD card
#include <SD.h>//Needed for working with SD card
#include "ArduinoLowPower.h"//Needed for putting Feather M0 to sleep between samples
#include <IridiumSBD.h>//Needed for communication with IRIDIUM modem  

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
String filename = "tempdata.csv"; //Name of log file
int sample_intv = 1; //Sample interval in minutes
float sample_sum = 0.0;//Sample sum for computing daily average
int sample_n = 0;//Sample N for computing daily average
float avg_temp_c = 0;//Daily average var
float min_temp_c = 100;//Daily min var
float max_temp_c = -100;//Daily max var 
DateTime IridTime;//Varible for keeping IRIDIUM transmit time
int err; //IRIDIUM status var 

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

void irid_daily_stats(DateTime current_time)
{
  //Increase IridTime by one day
  IridTime = DateTime(current_time.year(), current_time.month(), current_time.day(), current_time.hour(), 0, 0) + TimeSpan(1, 0, 0, 0);

  avg_temp_c = sample_sum / (float) sample_n;

  String irid_str = datestamp + "," + String(min_temp_c) + "," + String(avg_temp_c) + "," + String(max_temp_c);

  digitalWrite(IridPwrPin, HIGH);

  delay(1000);

  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200);

  err = modem.begin();
  if (err != ISBD_SUCCESS)
  {
    digitalWrite(LED, HIGH);
    delay(3000);
    digitalWrite(LED, LOW);
    delay(3000);
  }

  // Send the message
  err = modem.sendSBDText(irid_str.c_str());
  if (err != ISBD_SUCCESS)
  {
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
  }
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
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
  }


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

    //Update the sample_sum and sample_n, min and max
    sample_sum = sample_sum + tempC;
    sample_n = sample_n + 1;
    if (tempC > max_temp_c)
    {
      max_temp_c = tempC;
    }
    if (tempC < min_temp_c)
    {
      min_temp_c = tempC;
    }


    //Gen current time stamp
    gen_datestamp(now);

    //Datastring to write to SD card
    String datastring = datestamp + "," + String(tempC);

    //Write header if first time writing to the file
    if (!SD.exists(filename))
    {
      dataFile = SD.open(filename, FILE_WRITE);
      if (dataFile)
      {
        String header = "DateTime,TempC";
        dataFile.println("TempC");
        dataFile.close();
      }

    } else {
      //Write datastring and close logfile on SD card
      dataFile = SD.open(filename, FILE_WRITE);
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
      irid_daily_stats(now);
    }

    //Uncomment for trouble shooting 
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);

    //Logger sleep time in milliseconds 
    uint32_t sleep_time = sample_intv * 60000;

    //Put logger in low power mode for lenght 'sleep_time'
    LowPower.sleep(sleep_time);



  } else {
    // Indicate to user there was an issue by blinking built in LED
    while (1) {
      digitalWrite(LED, HIGH);
      delay(500);
      digitalWrite(LED, LOW);
      delay(500);
    }
  }

}
