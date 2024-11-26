/**
 * ESP32_LCD_NightScout.ino
 *
 *  Version date: 2024-11-26
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

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <time.h>

#include <ArduinoJson.h>

// Constants and function to display glycemia on LCD
#include "lcd_16x2.hpp"

// wifi credentials, NightScout URL and API key
#include "secrets.h"

WiFiMulti wifiMulti;

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

// time zone offset in minutes, initialized to UTC
int tzOffset = 0;

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

  // Initialize display
  initialize_LCD();

  lcd.clear();
  lcd.setCursor(0,0); // first line
  lcd.print("NightScout to");
  lcd.setCursor(0,1); // second line
  lcd.print("ESP32 with LCD");

  // Connect to Wifi
  for (int i = 0; i < sizeof(WIFI_CREDENTIALS) / sizeof(wifi_cred); i++) {
    if (WIFI_CREDENTIALS[i].password == NULL) {
      wifiMulti.addAP(WIFI_CREDENTIALS[i].ssid);
    } else {
      wifiMulti.addAP(WIFI_CREDENTIALS[i].ssid, WIFI_CREDENTIALS[i].password);
    }
  }

  if((wifiMulti.run() == WL_CONNECTED)) {
    Serial.print("\n[WiFiMulti] Connected to: ");
    Serial.print(WiFi.SSID());
    Serial.print(" IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("\n[WiFiMulti] Unable to connect to wifi");
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
  // wait for WiFi connection
  if((wifiMulti.run() == WL_CONNECTED)) {

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
  }

  // in ms -> retrieve data and update every minute
  delay(1*60*1000);
}
