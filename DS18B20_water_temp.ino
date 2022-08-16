

/*Include the libraries we need*/
#include "RTClib.h" //Needed for communication with Real Time Clock
#include <SPI.h> //Needed for working with SD card
#include <SD.h> //Needed for working with SD card
#include "ArduinoLowPower.h" //Needed for putting Feather M0 to sleep between samples
#include <IridiumSBD.h> //Needed for communication with IRIDIUM modem 
#include <CSV_Parser.h> //Needed for parsing CSV data
#include <OneWire.h> //Needed for oneWire communication 
#include <DallasTemperature.h> //Needed for communication with DS18B20


/*Define global constants*/
const byte LED = 13; // Built in LED pin
const byte chipSelect = 4; // Chip select pin for SD card
const byte IridPwrPin = 6; // Power base PN2222 transistor pin to Iridium modem
const byte TmpPwrPin = 11; // Pwr pin to temp. probe


/*Define global vars */
char **filename; // Name of log file(Read from PARAM.txt)
char **start_time;// Time at which first Iridum transmission should occur (Read from PARAM.txt)
String filestr; // Filename as string
int16_t *sample_intvl; // Sample interval in minutes (Read from PARAM.txt)
int16_t *irid_freq; // Iridium transmit freqency in hours (Read from PARAM.txt)
uint32_t irid_freq_hrs; // Iridium transmit freqency in hours
uint32_t sleep_time;// Logger sleep time in milliseconds
DateTime transmit_time;// Datetime varible for keeping IRIDIUM transmit time
DateTime present_time;// Var for keeping the current time
int err; //IRIDIUM status var


/*Define Iridium seriel communication as Serial1 */
#define IridiumSerial Serial1

// Data wire is plugged into port 12 on the Feather M0
#define ONE_WIRE_BUS 12

/*Create library instances*/
RTC_PCF8523 rtc; // Setup a PCF8523 Real Time Clock instance
File dataFile; // Setup a log file instance
IridiumSBD modem(IridiumSerial); // Declare the IridiumSBD object

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*Function reads data from a .csv logfile, and uses Iridium modem to send all observations
   since the previous transmission over satellite at midnight on the RTC.
*/
int send_hourly_data()
{

  // For capturing Iridium errors
  int err;

  // Provide power to Iridium Modem
  digitalWrite(IridPwrPin, HIGH);
  // Allow warm up
  delay(200);


  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200);

  // Prevent from trying to charge to quickly, low current setup
  modem.setPowerProfile(IridiumSBD::USB_POWER_PROFILE);

  // Begin satellite modem operation, blink LED (1-sec) if there was an issue
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



  // Set paramters for parsing the log file
  CSV_Parser cp("sf", true, ',');

  // Varibles for holding data fields
  char **datetimes;
  float *h2o_temps;


  // Read HOURLY.CSV file
  cp.readSDfile("/HOURLY.CSV");


  //Populate data arrays from logfile
  datetimes = (char**)cp["datetime"];
  h2o_temps = (float*)cp["h2o_temp_deg_c"];

  //Binary bufffer for iridium transmission (max allowed buffer size 340 bytes)
  uint8_t dt_buffer[340];

  //Buffer index counter var
  int buff_idx = 0;

  //Formatted for CGI script >> sensor_letter_code:date_of_first_obs:hour_of_first_obs:data
  String datestamp = "B:" + String(datetimes[0]).substring(0, 10) + ":" + String(datetimes[0]).substring(11, 13) + ":";

  //Populate buffer with datestamp
  for (int i = 0; i < datestamp.length(); i++)
  {
    dt_buffer[buff_idx] = datestamp.charAt(i);
    buff_idx++;
  }

  //For each hour 0-23
  for (int day_hour = 0; day_hour < 24; day_hour++)
  {

    //Declare average vars for each HYDROS21 output
    float mean_temp;
    boolean is_first_obs = false;
    int N = 0;

    //For each observation in the HOURLY.CSV
    for (int i = 0; i < cp.getRowsCount(); i++) {

      //Read the datetime and hour
      String datetime = String(datetimes[i]);
      int dt_hour = datetime.substring(11, 13).toInt();

      //If the hour matches day hour
      if (dt_hour == day_hour)
      {

        //Get data
        float h2o_temp = h2o_temps[i];

        //Check if this is the first observation for the hour
        if (is_first_obs == false)
        {
          //Update average vars
          mean_temp = h2o_temp;
          is_first_obs = true;
          N++;
        } else {
          //Update average vars
          mean_temp = mean_temp + h2o_temp;
          N++;
        }

      }
    }

    //Check if there were any observations for the hour
    if (N > 0)
    {
      //Compute averages
      mean_temp = (mean_temp / N) * 10.0;


      //Assemble the data string
      String datastring =  String(round(mean_temp)) + ':';


      //Populate the buffer with the datastring
      for (int i = 0; i < datastring.length(); i++)
      {
        dt_buffer[buff_idx] = datastring.charAt(i);
        buff_idx++;
      }
    }
  }

  //Indicate the modem is trying to send with LED
  digitalWrite(LED, HIGH);

  //transmit binary buffer data via iridium
  err = modem.sendSBDBinary(dt_buffer, buff_idx);

  //If transmission failed and message is not too large try once more, increase time out
  if (err != ISBD_SUCCESS)
  {
    err = modem.begin();
    modem.adjustSendReceiveTimeout(500);
    err = modem.sendSBDBinary(dt_buffer, buff_idx);

  }
  digitalWrite(LED, LOW);


  //Kill power to Iridium Modem by writing the base pin low on PN2222 transistor
  digitalWrite(IridPwrPin, LOW);
  delay(30);


  //Remove previous daily values CSV as long as send was succesfull, or if message is more than 340 bites

  SD.remove("/HOURLY.CSV");

  return err;


}


/*
   The setup function. We only start the sensors, RTC and SD here
*/
void setup(void)
{
  // Set pin modes
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(IridPwrPin, OUTPUT);
  digitalWrite(IridPwrPin, LOW);
  pinMode(TmpPwrPin, OUTPUT);
  digitalWrite(TmpPwrPin, HIGH);

  //Make sure a SD is available (2-sec flash LED means SD card did not initialize)
  while (!SD.begin(chipSelect)) {
    digitalWrite(LED, HIGH);
    delay(2000);
    digitalWrite(LED, LOW);
    delay(2000);
  }

  //Set paramters for parsing the parameter file PARAM.txt
  CSV_Parser cp("sdds", true, ',');


  //Read the parameter file 'PARAM.txt', blink (1-sec) if fail to read
  while (!cp.readSDfile("/PARAM.txt"))
  {
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
    delay(1000);
  }


  //Populate data arrays from parameter file PARAM.txt
  filename = (char**)cp["filename"];
  sample_intvl = (int16_t*)cp["sample_intvl"];
  irid_freq = (int16_t*)cp["irid_freq"];
  start_time = (char**)cp["start_time"];

  //Sleep time between samples in miliseconds
  sleep_time = sample_intvl[0] * 1000;

  //Log file name
  filestr = String(filename[0]);

  //Iridium transmission frequency in hours
  irid_freq_hrs = irid_freq[0];

  //Get logging start time from parameter file
  int start_hour = String(start_time[0]).substring(0, 3).toInt();
  int start_minute = String(start_time[0]).substring(3, 5).toInt();
  int start_second = String(start_time[0]).substring(6, 8).toInt();

  // Make sure RTC is available
  while (!rtc.begin())
  {
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  //Get the present time
  present_time = rtc.now();

  //Update the transmit time to the start time for present date
  transmit_time = DateTime(present_time.year(),
                           present_time.month(),
                           present_time.day(),
                           start_hour + irid_freq_hrs,
                           start_minute,
                           start_second);

  // Start up the DallasTemp library and test sensor
  digitalWrite(TmpPwrPin, HIGH);
  delay(300);
  sensors.begin();
  sensors.setResolution(12);
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempCByIndex(0);
  while (tempC == DEVICE_DISCONNECTED_C)
  {
    digitalWrite(LED, HIGH);
    delay(4000);
    digitalWrite(LED, LOW);
    delay(4000);
  }
  digitalWrite(TmpPwrPin, LOW);
}

/*
   Main function, sample HYDROS21 and sample interval, log to SD, and transmit hourly averages over IRIDIUM at midnight on the RTC
*/
void loop(void)
{

  //Get the present datetime
  present_time = rtc.now();

  //If the presnet time has reached transmit_time send all data since last transmission averaged hourly
  if (present_time >= transmit_time)
  {
    // Send the hourly data over Iridium
    int send_status = send_hourly_data();

    //Update next Iridium transmit time by 'irid_freq_hrs'
    transmit_time = (transmit_time + TimeSpan(0, irid_freq_hrs, 0, 0));
  }

  //Drive temp. pwr pin high
  digitalWrite(TmpPwrPin, HIGH);

  //Set resolution to 12 bit, 10 bit is too corse
  sensors.setResolution(12);

  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures

  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  float tempC = sensors.getTempCByIndex(0);

  //Drive temp. pwr pin high
  digitalWrite(TmpPwrPin, LOW);

  String datastring = present_time.timestamp() + "," + String(tempC);

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C)
  {

    //Write header if first time writing to the logfile
    if (!SD.exists(filestr.c_str()))
    {
      dataFile = SD.open(filestr.c_str(), FILE_WRITE);
      if (dataFile)
      {
        dataFile.println("datetime,h2o_temp_deg_c");
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


    /*The HOURLY.CSV file is the same as the log-file, but only contains observations since the last transmission and is used by the send_hourly_data() function */

    //Write header if first time writing to the DAILY file
    if (!SD.exists("HOURLY.CSV"))
    {
      //Write datastring and close logfile on SD card
      dataFile = SD.open("HOURLY.CSV", FILE_WRITE);
      if (dataFile)
      {
        dataFile.println("datetime,h2o_temp_deg_c");
        dataFile.close();
      }
    } else {

      //Write datastring and close logfile on SD card
      dataFile = SD.open("HOURLY.CSV", FILE_WRITE);
      if (dataFile)
      {
        dataFile.println(datastring);
        dataFile.close();
      }
    }
  } else {
    // Indicate to user there was an issue by blinking built in LED
    for (int i = 0; i < 100; i++)
    {
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
    }
  }

  //Flash LED to idicate a sample was just taken
  digitalWrite(LED, HIGH);
  delay(250);
  digitalWrite(LED, LOW);
  delay(250);

  //Put logger in low power mode for lenght 'sleep_time'
  LowPower.sleep(sleep_time);


}
