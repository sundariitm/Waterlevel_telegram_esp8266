
///////////////////////////////////
// Feature/Hardware peripheral macros
//*********************************
//#define PRODUCTION
#define ENA_AHT10_SENSOR
#define ENA_BMP180_SENSOR
//#define ENA_RAIN_SENSOR
//#define ENA_DHT22_SENSOR
#define TELEGRAM_BOT
///////////////////////////////////

///////////////////////////////////
// Board macros
//*********************************
#define NODEMCU
// #define ESP8266  - This is automatically defined in board defintion
// #define ESP32
//*********************************
///////////////////////////////////
///////////////////////////////////
//  Pin defintions as per board/peripherals
//*********************************

#define ADC_PIN0 A0
#define ADC_PIN1 255

#define DIG_PIN1      1

#define BUILTIN_LED         16   // Builtin Blue LED - GPIO16 
#define PIN_D1_DHT          0   // Pin D1(silk screen) : GPIO5  Pin D3 : GPIO0
#define ADC_INPUT           A0     // select the input ADC pin
#define FLOAT_SW_INPUT      13     // Pin D7 (silk screen) in ESP8266 Nodemcu

//  UnUSED - #define MAINS_CHECK_PIN     4   // Pin D2 si on GPIO4 of wroom2
//  UnUSED - #define PIN_D5_12V_FET_SW   14  // Pin D5
//  UnUSED - #define PIN_D6_5V_FET_SW    12  // Pin D6
//  UnUSED - #define PIN_D7_S1_CTRL      13  // Pin D7 of wroom2 used for Solinoid water control
//*********************************
///////////////////////////////////


#ifdef ENA_DHT22_SENSOR
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
DHTesp dht;
#endif

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif


#ifdef ESP32
#include "TZdef.h"
#else
#include "TZ.h"
#endif


#include <time.h>
#include <FS.h>
#include <LittleFS.h>
// #include <CertStoreBearSSL.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include <FluidLevelClass.h>

#ifdef TELEGRAM_BOT
#include <UniversalTelegramBot.h>
#endif


#ifdef PRODUCTION
// Update the node information this frequently -
#define   CLOUD_UPDATE_INTERNAL_MS     3*60*1000   // update every 5 mins
#define   ONE_MIN_ACT_MSEC             1*60*1000
#define   WAIT_AFTER_POWERCYCLE         10*1000     //
#define   WIFI_CONNECT_TIMEOUT          70*1000

#else

#define   CLOUD_UPDATE_INTERNAL_MS     1*30*1000      // update every 10 seconds
#define   ONE_MIN_ACT_MSEC             1*5*1000       // check every 5 seconds
#define   WAIT_AFTER_POWERCYCLE         10*1000
#define   WIFI_CONNECT_TIMEOUT          70*1000
#endif

#ifdef ENA_BMP180_SENSOR
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
float temp_bmp180;
int   pressure_bmp180;
#endif


#ifdef ENA_AHT10_SENSOR
#include <AHTxx.h>
float ahtValue;
float temp_aht10;
float rh_aht10;                               //to store T/RH result
AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR); //sensor address, sensor type
#endif

#ifdef ENA_DHT22_SENSOR
float dht22_humidity;
float dht22_temperature;
#endif // #ifdef ENA_DHT22_SENSOR

#ifdef PRODUCTION
// Update these with values suitable for your network.
const char* ssid = "FILL HERE ";
const char* password = "FILL HERE ";
#else
// Update these with values suitable for your network.
// const char* ssid = " FILL HERE";
// const char* password = "FILL HERE  ";
const char* ssid = "FILL HERE ";
const char* password = "FILL HERE ";
#endif
// Global variable store from different sensors
float bmp180TempC , bmp180Alti;
int   bmp180Pressure,  bmp180SeaLev;
float ahtHumidity, ahtTempC;  

int powerCycleCnt = 0;

/*******************************/
/* Helper SplitString function */
/*******************************/
String strs[12];
int splitSring (String str) {
  int StringCount = 0;
  Serial.println(str);

  // Split the string into substrings
  while (str.length() > 0)
  {
    int index = str.indexOf(' ');
    if (index == -1) // No space found
    {
      strs[StringCount++] = str;
      break;
    }
    else
    {
      strs[StringCount++] = str.substring(0, index);
      str = str.substring(index + 1);
    }
  }

  return StringCount;
}
/*******************************/

/*******************************/
/* Helper printSubStr function */
/*******************************/
void printSubStr(int StringCount) {
  // Show the resulting substrings
  for (int i = 0; i < StringCount; i++)
  {
    Serial.print(i);
    Serial.print(": \"");
    Serial.print(strs[i]);
    Serial.println("\"");
  }
}
/*******************************/



#ifdef TELEGRAM_BOT
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN " Long string from Botfather"
const unsigned long BOT_MTBS = 1000; // mean time between scan messages
#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ADC_PIN0
FluidLevelClass oh_tank ( ADC_PIN0, FLOAT_SW_INPUT);
// Rain sensor doesnt have any Digital input. So just passing undefined input
FluidLevelClass rain_sensor ( ADC_PIN1, 255);

unsigned long bot_lasttime; // last time messages' scan has been done
String gchat_id;
unsigned long lastTime = 0;
unsigned long lastTime200ms = 0;

unsigned long cloud_updt_interval = CLOUD_UPDATE_INTERNAL_MS;
char webErrCnt = 0;
unsigned int onBattMin, onMainsMin;
unsigned int waterOnMin = 0;
////////////////////////////////////////////////
// Handle messages coming from Telegram channel
////////////////////////////////////////////////
void handleNewMessages(int numNewMessages)
{
  String chat_id = bot.messages[0].chat_id;
  String txtMsg = "";

  for (int i = 0; i < numNewMessages; i++)
  {
    //bot.sendMessage(bot.messages[i].chat_id, bot.messages[i].text, "");
    //bot.sendMessage(chat_id, "ABCD", "");
    bot.sendMessage(bot.messages[i].chat_id, "Hello there, I am a NSbot! I will send you periodic weather report!!", "");

    telegramMessage &msg = bot.messages[i];
    Serial.println("Received " + msg.text);
    Serial.println(msg.text.length());
    printSubStr(splitSring(msg.text));

    if ( msg.text == "RESET") {
      powerCycleCnt += 1;
    }

    // command WATER@ON <seconds>
    //    if ( msg.text == "WATERON") {
    if ( strs[0] == "WATERON") {
      Serial.println("WATER_ON");
      waterOnMin = strs[1].toInt() ; // Minutes to turn-ON
    }


    if ( msg.text == "WATEROFF") {
      Serial.println("WATER_OFF");
      waterOnMin = 0;
    }
    Serial.println ( "waterOnMin = " + String( waterOnMin));


  }
}
#endif



unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (800)
char msg[MSG_BUFFER_SIZE];
char latestTime[50];

float value = 0;

int sensorValue = 0;        // value read from the pot
int floatSwValue = 0;        // value output to the PWM (analog out)
#ifdef ENA_RAIN_SENSOR


int rain_int = 0;

#endif // #ifdef ENA_RAIN_SENSOR

char setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

#ifdef TELEGRAM_BOT
#ifdef ESP8266
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
#endif
#endif

  return check_wifi ();

}
///////////////////
///////////////////
char check_wifi() {
  char cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (cnt++ >= WIFI_CONNECT_TIMEOUT / 500) {
      Serial.println(F("WiFi unable to connect"));
      return 0;
    }
  }

  randomSeed(micros());
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  return 1;
}
///////////////////



void setDateTime(int ntpTimeout) {
  int timeoutcnt = 0;
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
//#ifdef ESP8266
  configTime(TZ_Asia_Kolkata, "pool.ntp.org", "time.nist.gov");
//#endif

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
    if (timeoutcnt++ > 10 * ntpTimeout) { // Give up after 2 mins
      Serial.printf("Giving up reaching NTP server");
      return;
    }
  }
  Serial.println();

  now += 19800;  // Convert to IST
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
  return;
}

void getCurrentTime ( ) {

  time_t now = time(nullptr);
  now += 19800;  // Convert to IST
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));

  latestTime[0] = '\0';
  snprintf (latestTime, 50, "------\n%s %s\n-------",  tzname[0], asctime(&timeinfo)  );
}
#ifdef ENA_DHT22_SENSOR
void access_dht22_temprh()
{
  delay(dht.getMinimumSamplingPeriod());

  dht22_humidity = dht.getHumidity();
  dht22_temperature = dht.getTemperature();

  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(dht22_humidity, 1);
  Serial.print("\t\t");
  Serial.print(dht22_temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.toFahrenheit(dht22_temperature), 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(dht22_temperature, dht22_humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(dht22_temperature), dht22_humidity, true), 1);
  //delay(300);
}
#endif  // #ifdef ENA_DHT22_SENSOR



// Giveup after trying for 90 seconds to reach NTP server
int ntpTimeout = 45;

void setup()
{
  Serial.begin(115200);
  LittleFS.begin();
  setup_wifi();

#ifdef ENA_BMP180_SENSOR
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP180 sensor, check wiring!");
  }
#endif //  ENA_BMP180_SENSOR 

#ifdef ENA_AHT10_SENSOR
  while (aht10.begin() != true) //for ESP-01 use aht10.begin(0, 2);
  {
    Serial.println(F("AHT1x not connected or fail to load calibration coefficient")); //(F()) save string to flash & keeps dynamic memory free

    delay(5000);
  }
  Serial.println(F("AHT10 OK"));
#endif   //  #ifdef ENA_AHT10_SENSOR


  String thisBoard = ARDUINO_BOARD;
  Serial.println(thisBoard);
  pinMode(BUILTIN_LED, OUTPUT);


#ifdef ENA_DHT22_SENSOR
  dht.setup(PIN_D1_DHT, DHTesp::DHT22); // Connect DHT sensor to PIN_D1_DHT
#endif
  Serial.println();
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");

  cloud_updt_interval = CLOUD_UPDATE_INTERNAL_MS;
  setDateTime(ntpTimeout);

//   pinMode(FLOAT_SW_INPUT, INPUT_PULLUP);


}

void access_bmp180_pressure() {
  
   Serial.print("Temperature = ");
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");
    
    Serial.print("Pressure = ");
    Serial.print(bmp.readPressure());
    Serial.println(" Pa");
    
    // Calculate altitude assuming 'standard' barometric
    // pressure of 1013.25 millibar = 101325 Pascal
    Serial.print("Altitude = ");
    Serial.print(bmp.readAltitude());
    Serial.println(" meters");

    Serial.print("Pressure at sealevel (calculated) = ");
    Serial.print(bmp.readSealevelPressure());
    Serial.println(" Pa");

  // you can get a more precise measurement of altitude
  // if you know the current sea level pressure which will
  // vary with weather and such. If it is 1015 millibars
  // that is equal to 101500 Pascals.
   // Serial.print("Real altitude = ");
   // Serial.print(bmp.readAltitude(101500));
    //Serial.println(" meters");
    
    Serial.println();

    bmp180TempC = bmp.readTemperature();  // Temp
    bmp180Pressure =  bmp.readPressure();  // Pascal
    bmp180Alti = bmp.readAltitude(); // meters
    bmp180SeaLev =  bmp.readSealevelPressure(); // Pa 
}


/**************************************************************************/
/*
    printStatus()

    Print last command status
*/
/**************************************************************************/
void printStatus()
{
  switch (aht10.getStatus())
  {
    case AHTXX_NO_ERROR:
      Serial.println(F("no error"));
      break;

    case AHTXX_BUSY_ERROR:
      Serial.println(F("sensor busy, increase polling time"));
      break;

    case AHTXX_ACK_ERROR:
      Serial.println(F("sensor didn't return ACK, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)"));
      break;

    case AHTXX_DATA_ERROR:
      Serial.println(F("received data smaller than expected, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)"));
      break;

    case AHTXX_CRC8_ERROR:
      Serial.println(F("computed CRC8 not match received CRC8, this feature supported only by AHT2x sensors"));
      break;

    default:
      Serial.println(F("unknown status"));    
      break;
  }
}
/*************************************/
// *****************************/
// *****************************/
//  Begin :  access_aht10_temprh
// *****************************/

void access_aht10_temprh  (void ) {


  ahtValue = aht10.readTemperature(); //read 6-bytes via I2C, takes 80 milliseconds

  Serial.print(F("Temperature: "));
  
  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    Serial.print(ahtValue);
    Serial.println(F(" +-0.3C"));
  }
    else
  {
    printStatus(); //print temperature command status not humidity!!! RH measurement use same 6-bytes from T measurement
  }
  ahtTempC = ahtValue;


   ahtValue = aht10.readHumidity(AHTXX_USE_READ_DATA); //use 6-bytes from temperature reading, takes zero milliseconds!!!

  Serial.print(F("Humidity...: "));
  
  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    Serial.print(ahtValue);
    Serial.println(F(" +-2%"));
  }
    else
  {
    printStatus(); //print temperature command status not humidity!!! RH measurement use same 6-bytes from T measurement
  }
  ahtHumidity = ahtValue;
}
// *****************************/
//  End : access_aht10_temprh
// *****************************/
// *****************************/


byte webDownCnt = 0;
byte wifiDownCnt = 0;
int adc_val = 0;
long adc_val_acc = 0;
long water_lev_avg = 0;
char avg_cnt = 0;
//////////////////////////////////////////////////////
//////////////  Main Loop
//////////////////////////////////////////////////////
void loop()
{

  unsigned long now = millis();
  int waterlevel;
  float waterper;
  String yesOrNo = "YES";

  ////////////////////////
  // 200 ms activity - check ADC sensor, water level and average
  ////////////////////////

  if ( millis() - lastTime200ms >  200 )  {

    lastTime200ms = millis();
//    sensorValue = analogRead (ADC_PIN0);
//    floatSwValue = digitalRead (FLOAT_SW_INPUT);
//   Serial.print(F("ADC input :"));
//   Serial.println ( sensorValue);
//
//   Serial.print(F("Float Sw input :"));
//   Serial.println ( floatSwValue);

    getCurrentTime ();

    

  }
  ////////////////////////
  // 1 minute activity - check Wifi & web connections
  ////////////////////////
  if ( millis() - lastTime >  ONE_MIN_ACT_MSEC )  {

    lastTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wifiDownCnt++;
      Serial.println(F("Wifi Down Cnt:"));
      Serial.print(wifiDownCnt);
    }

    waterlevel = oh_tank.ReadWaterLevel();

    if (oh_tank.calibratedFlag == 1)
        waterper = waterlevel / oh_tank.newFullLevel;  // Convert to percentage
    else
        waterper = 999;  //  when full tank POT value not known
        
    if (oh_tank.tankFullFlag)
      yesOrNo = "YES";
    else
      yesOrNo = "NO";


    msg[0] = '\0';
    snprintf (msg, MSG_BUFFER_SIZE, "%s \n =====\n Water Level\t = %3d \n Water percent \t = %2.1f \n Full Tank \t = %s \n newFullLevel \t = %3d \n  \ 
floatSwitch \t = %1d \n calibratedFlag \t = %1d \n ===== \n ahtTempC \t = %2.2f C \n bmp180TempC \t = %2.2f C \n ahtHumidity \t = %2.2f % \n \
bmp180Pressure \t = %d Pa \n bmp180Alti \t = %4.1f m \n bmp180SeaLev \t = %d Pa \n =====\n", latestTime, waterlevel, waterper, yesOrNo, oh_tank.newFullLevel, \
oh_tank.floatSwitch , oh_tank.calibratedFlag,  \
ahtTempC, bmp180TempC, ahtHumidity, bmp180Pressure,bmp180Alti,bmp180SeaLev  );



    Serial.println(msg);


  }   // activity that gets scheduled ONE_MIN_ACT_MSEC
  //////////////////////////////////////////////////////  

#ifdef TELEGRAM_BOT
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages)
  {
    Serial.println("got response");
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
#endif

  if (now - lastMsg > cloud_updt_interval) {
    lastMsg = now;
    ++value;

#ifdef ENA_AHT10_SENSOR
    access_aht10_temprh();
#endif
#ifdef ENA_BMP180_SENSOR
    access_bmp180_pressure();
#endif

#ifdef ENA_DHT22_SENSOR
    access_dht22_temprh();
#endif

    //   waterlevel = ReadWaterLevel();

#ifdef TELEGRAM_BOT
    //    msg[0] = '\0';
    //    snprintf (msg, MSG_BUFFER_SIZE, "OnMainsMin= %4d\nOnBattMin= %4d\nTemp(C)\t = %2.1f \nRH\%\t = %2.1f", onMainsMin, onBattMin, dht22_temperature,dht22_humidity);
    //
    //    msg[0] = '\0';
    //    snprintf (msg, MSG_BUFFER_SIZE, "Temp(C)\t = %2.1f C \nRH\%\t = %2.1f \% \nWaterON = %4d seconds left", dht22_temperature,dht22_humidity, waterOnMin);

    // msg[0] = '\0';
    // snprintf (msg, MSG_BUFFER_SIZE, "Sensor Value \t = %d ", sensorValue);

    gchat_id = "7095195549"; // chat ID for bot user
    gchat_id = "-1002000740523"; // chat ID for public channel
    if (!bot.sendMessage(gchat_id, msg, ""))
      webErrCnt++;

#endif


    digitalWrite(BUILTIN_LED, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(300);
    digitalWrite(BUILTIN_LED, LOW);
  }
}  // loop ()
