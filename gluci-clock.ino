/**
 * ESP32_LCD_NightScout.ino
 *
 *  Version date: 2024-12-02
 *  
 *  For diabetics using a NightScout database
 *  Retrieve glycemia every minute
 *  and display on a 16x2 LCD screen
 *
*/

// development IDE: Arduino 1.8.9
// intall libraries: multiwifi, arduinoJson, ESP32

// Response example from NightScout:
// [{
//     "_id":"999999999999999999999999",
//     "type":"sgv",
//     "sgv":217,
//     "direction":"Flat",
//     "device":"nightscout-librelink-up",
//     "date":1732386771000,
//     "dateString":"2024-11-23T18:32:51.000Z",
//     "utcOffset":0,
//     "sysTime":"2024-11-23T18:32:51.000Z",
//     "mills":1732386771000
// }]

// ----------------------------
// Library Defines - Need to be defined before library import
// ----------------------------

#define ESP_DRD_USE_SPIFFS true

#include <Arduino.h>

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

#include <WiFiManager.h>
// Captive portal for configuring the WiFi

// Can be installed from the library manager (Search for "WifiManager", install the Alhpa version)
// https://github.com/tzapu/WiFiManager

#include <ESP_DoubleResetDetector.h>
// A library for checking if the reset button has been pressed twice
// Can be used to enable config mode
// Can be installed from the library manager (Search for "ESP_DoubleResetDetector")
//https://github.com/khoih-prog/ESP_DoubleResetDetector


// Constants and function to display glycemia on LCD
#include "lcd_16x2.hpp"



// -------------------------------------
// -------   Other Config   ------
// -------------------------------------

const int PIN_LED = 2;

#define JSON_CONFIG_FILE "/sample_config.json"

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0


// -----------------------------

// -----------------------------

DoubleResetDetector *drd;

//flag for saving data
bool shouldSaveConfig = false;


// url and API key for Nightscout

// Don't foreget to add '?count=1' to the URL
char NS_API_URL[150] = "https://your-nightscout-site//api/v1/entries.json?count=1";

// key defined in NS > Hamburger menu > Admin tools
// with API READ rights
char NS_API_SECRET[50]= "testapirea-1234567890abcdef";


// Parameters for time NTP server
char ntpServer1[50] = "pool.ntp.org";
char ntpServer2[50] = "time.nist.gov";

// Time zone for local time and daylight saving
// list here:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
char local_time_zone[50] = "CET-1CEST,M3.5.0,M10.5.0/3"; // set for Europe/Paris
char utc_time_zone[] = "GMT0";
int  gmtOffset_sec = 3600;
long daylightOffset_sec = 3600; // long


// Contrast level for LCD (0-255)
int contrast = 75;

// time zone offset in minutes, initialized to UTC
int tzOffset = 0;

void serialPrintParams(){
  Serial.println("\tNS_API_URL : " + String(NS_API_URL));
  Serial.println("\tNS_API_SECRET: " + String(NS_API_SECRET));
  Serial.println("\tntpServer1 : " + String(ntpServer1));
  Serial.println("\tntpServer2 : " + String(ntpServer2));
  Serial.println("\tlocal_time_zone : " + String(local_time_zone));
  Serial.println("\tgmtOffset_sec : " + String(gmtOffset_sec));
  Serial.println("\tdaylightOffset_sec : " + String(daylightOffset_sec));
  Serial.println("\tcontrast : " + String(contrast));
}

void saveConfigFile()
{
  Serial.println(F("Saving config"));
  StaticJsonDocument<512> json;

  json["NS_API_URL"] = NS_API_URL;
  json["NS_API_SECRET"] = NS_API_SECRET;
  json["ntpServer1"] = ntpServer1;
  json["ntpServer2"] = ntpServer2;
  json["local_time_zone"] = local_time_zone;
  json["gmtOffset_sec"] = gmtOffset_sec;
  json["daylightOffset_sec"] = daylightOffset_sec;
  json["contrast"] = contrast;

  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }

  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();

  delay(3000);
  ESP.restart();
  delay(5000);
}

bool loadConfigFile()
{
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  // May need to make it begin(true) first time you are using SPIFFS
  // NOTE: This might not be a good way to do this! begin(true) reformats the spiffs
  // it will only get called if it fails to mount, which probably means it needs to be
  // formatted, but maybe dont use this if you have something important saved on spiffs
  // that can't be replaced.
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("opened config file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          strcpy(NS_API_URL, json["NS_API_URL"]);
          strcpy(NS_API_SECRET, json["NS_API_SECRET"]);
          strcpy(ntpServer1, json["ntpServer1"]);
          strcpy(ntpServer2, json["ntpServer2"]);
          strcpy(local_time_zone, json["local_time_zone"]);
          gmtOffset_sec= json["gmtOffset_sec"];
          daylightOffset_sec = json["daylightOffset_sec"];
          contrast = json["contrast"];

          Serial.println("\nThe loaded values are: ");
          serialPrintParams();

          return true;
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
    // SPIFFS.format();
  }
  //end read
  return false;
}




//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiManager wm;

//String getDefaultPassword() {
//    String defaultApName = wm.getDefaultAPName();
//    int len = defaultApName.length();
//    if (len <= 8) {
//        return defaultApName; // If the string is 8 characters or shorter, return the whole string
//    }
//    return defaultApName.substring(len - 8); // Return the last 8 characters
//}


// default password for Access Point, made from macID
char * getDefaultPassword(){
  // example:
  // const char * password = getDefaultPassword();

  // source for chipId: Espressif library example ChipId
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  static char pw[9]; // with +1 char for end of chain
  sprintf(pw, "%08d", chipId);
  return pw;
}
const char * apPassword = getDefaultPassword();

//callback notifying us of the need to save parameters
void saveParamsCallback()
{
  Serial.println("Should save params");
  shouldSaveConfig = true;
  wm.stopConfigPortal(); // will abort config portal after page is sent 
}

// This gets called when the config mode is launced, might
// be useful to update a display with this info.
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered Conf Mode");

  lcd.clear();
  lcd.setCursor(0,0); // first line
  lcd.print("Wifi:Gluci-clock");
  lcd.setCursor(0,1); // second line
  lcd.print(apPassword);

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Config password: ");
  Serial.println(apPassword);
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}


/**
 * Set the timezone
 */
void setTimezone(char* timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone);
  configTzTime(timezone, ntpServer1, ntpServer2);
}

/**
 * Get time from internal clock
 * returns time for the timezone defined by setTimezone(char* timezone)
 */
struct tm getActualTzTime(){
  struct tm timeinfo = {0};
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time!");
    return(timeinfo); // return {0} if error
  }
  Serial.print("System time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  return(timeinfo);
}



/**
 * Returns the offset in seconds between local time and UTC
 */
int getTzOffset(char* timezone){

  // set timezone to UTC
  setTimezone(utc_time_zone); 
  
  // and get tm struct
  struct tm tm_utc_now = getActualTzTime();
  
  // convert to time_t
  time_t t_utc_now = mktime (&tm_utc_now);
  
  // set timezone to local
  setTimezone(local_time_zone);

  // convert time_t to tm struct
  struct tm tm_local_now = *localtime(&t_utc_now);

  // set timezone back to UTC
  setTimezone(utc_time_zone);

  // convert tm to time_t
  time_t t_local_now = mktime (&tm_local_now);

  // calculate difference between the two time_t, in seconds
  int tzOffset = round(difftime(t_local_now, t_utc_now));
  Serial.printf("\nTzOffset : %d\n", tzOffset);

  return (tzOffset);
}

/**
 * Setup : 
 * - initialize LCD, 
 * - connect to wifi, 
 * - evaluate offset between local time and UTC
 */
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println("ESP start");

  pinMode(PIN_LED, OUTPUT);
   
  // Initialize display
  initialize_LCD(contrast);

  lcd.clear();
  lcd.setCursor(0,0); // first line
  lcd.print("  Gluci |");
  lcd.setCursor(0,1); // second line
  lcd.print("        | clock");

  bool forceConfig = false;

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset())
  {
    Serial.println(F("Forcing config mode as there was a Double reset detected"));
    forceConfig = true;
  }

  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }

  //WiFi.disconnect();
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  delay(10);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wm.setTimeout(10*60);

  //wm.resetSettings(); // wipe settings
  
  //set callbacks
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setSaveParamsCallback(saveParamsCallback);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  // Custom parameters here
  wm.setTitle("Gluci-clock");

  // Set cutom menu via menu[] or vector
  // const char* menu[] = {"wifi", "wifinoscan", "info", "param", "close", "sep", "erase", "restart", "exit", "erase", "update", "sep"};
  const char* wmMenu[] = {"param", "wifi", "close", "sep", "info", "restart", "exit"};
  wm.setMenu (wmMenu, 7); // custom menu array must provide length


  //--- additional Configs params ---

  // url and API key for Nightscout
  
  // Don't foreget to add '?count=1' to the URL
  // char NS_API_URL[150] = "https://your-nightscout-site//api/v1/entries.json?count=1"
  WiFiManagerParameter custom_NS_API_URL("NS_API_URL", "NightScout API URL", NS_API_URL, 150);
  
  // key defined in NS > Hamburger menu > Admin tools
  // with API READ rights
  // const char NS_API_SECRET[50] = "your-API-secret or token";
  WiFiManagerParameter custom_NS_API_SECRET("NS_API_SECRET", "NightScout API secret", NS_API_SECRET, 50);
  
  // Parameters for time NTP server
  // char ntpServer1[50] = "pool.ntp.org";
  WiFiManagerParameter custom_ntpServer1("ntpServer1", "NTP server 1", ntpServer1, 50);
  // char ntpServer2[50] = "time.nist.gov";
  WiFiManagerParameter custom_ntpServer2("ntpServer2", "NTP server 2", ntpServer2, 50);;
  
  
  // Time zone for local time and daylight saving
  // list here:
  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  // char[50] local_time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; // set for Europe/Paris
  WiFiManagerParameter custom_local_time_zone("local_time_zone", "local_time_zone", local_time_zone, 50);
  
  //int  gmtOffset_sec = 3600;
  char str_gmtOffset_sec[5];
  sprintf(str_gmtOffset_sec, "%d", gmtOffset_sec);
  WiFiManagerParameter custom_gmtOffset_sec("gmtOffset_sec", "gmtOffset sec", str_gmtOffset_sec, 5);
  
  
  // int   daylightOffset_sec = 3600;
  char str_daylightOffset_sec[5];
  sprintf(str_daylightOffset_sec, "%d", daylightOffset_sec);
  WiFiManagerParameter custom_daylightOffset_sec("daylightOffset_sec", "daylightOffset sec", str_daylightOffset_sec, 5);
  
  
  // Contrast level for LCD (0-255)
  // int contrast = 75;
  char str_contrast[4];
  sprintf(str_contrast, "%d", contrast);
  WiFiManagerParameter custom_contrast("contrast", "contrast 0-255", str_contrast, 4);

  // add app parameters to web interface
  wm.addParameter(&custom_NS_API_URL);
  wm.addParameter(&custom_NS_API_SECRET);
  wm.addParameter(&custom_ntpServer1);
  wm.addParameter(&custom_ntpServer2);
  wm.addParameter(&custom_local_time_zone);
  wm.addParameter(&custom_gmtOffset_sec);
  wm.addParameter(&custom_daylightOffset_sec);
  wm.addParameter(&custom_contrast);

  //--- End additional parameters
  

  Serial.println("hello");

  digitalWrite(PIN_LED, LOW);
  if (forceConfig)
  {
    Serial.println("forceconfig = True");
//    lcd.clear();
//    lcd.setCursor(0,0); // first line
//    lcd.print("Wifi:Gluci-clock");
//    lcd.setCursor(0,1); // second line
//    lcd.print("clock123");
    if (!wm.startConfigPortal("Gluci-clock", apPassword))
    {
      Serial.print("shouldSaveConfig: ");
      Serial.println(shouldSaveConfig);
      if (!shouldSaveConfig){
        Serial.println("failed to connect CP and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
      }
    }
  }
  else
  {
    Serial.println("Running wm.autoconnect");
//    lcd.setCursor(0,0); // first line
//    lcd.clear();
//    lcd.print("Wifi:Gluci-clock");
//    lcd.setCursor(0,1); // second line
//    lcd.print("clock123");
    if (!wm.autoConnect("Gluci-clock",  apPassword))
    {
      Serial.println("failed to connect AC and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  // If we get here, we are connected to the WiFi or should save params
  digitalWrite(PIN_LED, HIGH);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Lets deal with the user config values

  strcpy(NS_API_URL, custom_NS_API_URL.getValue());
  strcpy(NS_API_SECRET, custom_NS_API_SECRET.getValue());
  strcpy(ntpServer1, custom_ntpServer1.getValue());
  strcpy(ntpServer2, custom_ntpServer2.getValue());
  strcpy(local_time_zone, custom_local_time_zone.getValue());
  gmtOffset_sec = atoi(custom_gmtOffset_sec.getValue());
  daylightOffset_sec = atoi(custom_daylightOffset_sec.getValue());
  contrast = atoi(custom_contrast.getValue());

  Serial.println("\nThe values returned are: ");
  serialPrintParams();

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }
  
  // offset in seconds between local time and UTC
  tzOffset = getTzOffset(local_time_zone);
  
  // set timezone to local
  setTimezone(local_time_zone);
}

/**
 * Connect to wifi
 * Retrieve glucose data from NightScout server and parse it to Json
 * Calculate delay between glucose measure and actual time
 * get arrow for display of glucose evolution on LCD
 * Display on LCD: 
 * - clock (date and HH:M), 
 * - glucose value, 
 * - evolution as an arrow, 
 * - delay since measure
 */
void loop() {

  // Retrieve glucose data from NightScout Server
  HTTPClient http;
  Serial.println("\n[HTTP] begin...");
  http.begin(NS_API_URL); // fetch latest value
  http.addHeader("API-SECRET", NS_API_SECRET);
  Serial.print("[HTTP] GET...\n");
  
  // start connection and send HTTP header
  int httpCode = http.GET();   
  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // NightScout data received from server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);

      // parse NightScour data Json response
      // buffer for Json parser
      StaticJsonDocument<500> httpResponseBody;
      DeserializationError error = deserializeJson(httpResponseBody, payload);
      
      // Test if parsing NightScout data succeeded
      if (error) {
        Serial.println();
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      } 
      else {
        // Get glucose values
        
        // Type must be "sgv" (skin glucose value)
        const char* type = httpResponseBody[0]["type"];
        
        // Directions: 
        // Flat, FortyFiveUp, FortyFiveDown, SingleUp, SingleDown, DoubleUp, DoubleDown
        const char* direct = httpResponseBody[0]["direction"];
        
        // Glucose value
        long sgv = long(httpResponseBody[0]["sgv"]);
        
        // Time of the measure, returned in UTC timezone in Nightscout Json
        const char* strMeasureTime = httpResponseBody[0]["sysTime"];

        // Convert Glucose measure time UTC to time_t for time difference calc
        // Parse UTC time to tm_time structure
        struct tm tmMeasureTimeUTC;
        strptime(strMeasureTime, "%Y-%m-%dT%H:%M:%S.000Z", &tmMeasureTimeUTC);
        // Convert UTC measure time to time_t (seconds since epoch)
        time_t tMeasureTimeUTC = mktime(&tmMeasureTimeUTC);

        // Actual time local TZ for clock display
        struct tm tmActualTimeLocal = getActualTzTime();
        time_t tActualTimeLocal = mktime(&tmActualTimeLocal);

        // prepare LCD clock display: "HH:MM", "dd Mmm"
        char time_lcd[10];
        strftime(time_lcd, 10, "%H:%M", &tmActualTimeLocal);
        char date_lcd[10];
        strftime(date_lcd, 10, "%d %b", &tmActualTimeLocal);
        
        // Actual time UTC as time_t for calculation of elapsed time since measure
        time_t tActualTimeUTC = tActualTimeLocal - tzOffset;
      
        // Calculate the elapsed time since glucose measure - in minutes
        int elapsed_mn = round(difftime(tActualTimeUTC, tMeasureTimeUTC) / 60.0);

        // Display values as below
        // 0123456789012345       LCD has 16 chars
        //  18:05  |  113 u       time | sgv
        // 25 Nov  |   1 mn       date | elapsed time since svg measure
        
        Serial.println("\n0123456789012345");
  
        if (strcmp(type, "sgv") == 0){
          lcd.clear();
          
          // first line: clock, glucose value, glucose evolution arrow
          lcd.setCursor(1,0);
          Serial.print(" ");
          lcd.print(time_lcd);
          Serial.print(time_lcd);

          lcd.printf("  | %3d ", sgv);
          Serial.printf("  | %3d ", sgv);

          
          // arrow for glucose evolution
          if(strcmp(direct, "Flat") == 0) {
            lcd.write(byte(0));
            Serial.print("f");
          }
          else if(strcmp(direct, "FortyFiveDown") == 0) {
            lcd.write(byte(1));
            Serial.print("d");
          }
          else if(strcmp(direct, "SingleDown") == 0) {
            lcd.write(byte(2));
            Serial.print("D");
          }
          else if(strcmp(direct, "DoubleDown") == 0) {
            lcd.write(byte(3));
            Serial.print("D");
          }
          else if(strcmp(direct, "FortyFiveUp") == 0) {
            lcd.write(byte(4));
            Serial.print("u");
          }
          else if(strcmp(direct, "SingleUp") == 0) {
            lcd.write(byte(5));
            Serial.print("U");
          }
          else if(strcmp(direct, "DoubleUp") == 0) {
            lcd.write(byte(6));
            Serial.print("U");
          }
          else {
            lcd.print(direct);
            Serial.print(direct);
          }

          
          // second line : date, delay since measure
          lcd.setCursor(1,1);
          Serial.print("\n ");
          
          lcd.print(date_lcd);
          Serial.print(date_lcd);

          lcd.printf(" | %3d mn", elapsed_mn);
          Serial.printf(" | %3d mn", elapsed_mn);
        }
      }
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  

  // in ms -> retrieve data and update every minute
  delay(1*60*1000);
}
