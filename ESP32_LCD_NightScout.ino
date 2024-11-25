/**
 * ESP32_LCD_NightScout.ino
 *
 *  Version date: 2024-11-25
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

#include "lcd_16x2.hpp"

// wifi credentials, NightScout URL and API key
#include "secrets.h"

WiFiMulti wifiMulti;


void setTimezone(char* timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone);
  configTzTime(timezone, ntpServer1, ntpServer2);
}


struct tm getTzTime(char* timezone){
  configTzTime(timezone, ntpServer1, ntpServer2);
  struct tm timeinfo = {0};
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return(timeinfo); // return {0} if error
  }
  Serial.print("System time in TZ: ");
  Serial.println(timezone);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  return(timeinfo);
}

// time zone offset in minutes
int tzOffsetMn = 0;

int getTzOffsetMn(char* timezone){
  //TODO
  return 60;
}

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
    Serial.println();
    Serial.print("[WiFiMulti] Connected to: ");
    Serial.print(WiFi.SSID());
    Serial.print(" IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.printf("[WiFiMulti] Unable to connect to wifi");
  }

  tzOffsetMn = getTzOffsetMn(local_time_zone);
}

void loop() {
  // wait for WiFi connection
  if((wifiMulti.run() == WL_CONNECTED)) {
      
    HTTPClient http;

    Serial.println();
    
    Serial.println("[HTTP] begin...");
    http.begin(NS_API_URL); // fetch latest value
    http.addHeader("API-SECRET", NS_API_SECRET);
    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();   
    // httpCode will be negative on error
    if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);

        // parse Json response
        StaticJsonDocument<500> httpResponseBody; // buffer size for Json parser
        DeserializationError error = deserializeJson(httpResponseBody, payload);
        // Test if parsing succeeds.
        if (error) {
          Serial.println();
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
        } else {
          // Fetch values.
          const char* type = httpResponseBody[0]["type"];
          // Directions: Flat, FortyFiveUp, FortyFiveDown, SingleUp, SingleDown, DoubleUp, DoubleDown
          const char* direct = httpResponseBody[0]["direction"];
          // Glucose value
          long sgv = long(httpResponseBody[0]["sgv"]);
          
          // Time of the measure, UTC
          const char* sysTime = httpResponseBody[0]["sysTime"]; // UTC, measure

          // Convert Glucose measure time UTC to time_t for time difference calc
          struct tm tmMeasureTimeUTC;
          strptime(sysTime, "%Y-%m-%dT%H:%M:%S.000Z", &tmMeasureTimeUTC);  // Parse UTC time
          time_t tMeasureTimeUTC = mktime(&tmMeasureTimeUTC);  // Convert to time_t (UTC)

          // Actual time local TZ for clock display
          struct tm tmEpochTimeLocal = getTzTime(local_time_zone);
          time_t tEpochTimeLocal = mktime(&tmEpochTimeLocal);
          
          char time_lcd[10];
          strftime(time_lcd, 10, "%H:%M", &tmEpochTimeLocal);
          char date_lcd[10];
          strftime(date_lcd, 10, "%d %b", &tmEpochTimeLocal);
          
          Serial.print(time_lcd);
          Serial.print("_");
          Serial.print(date_lcd);
          Serial.println("_local_TZ");

          // Actual time UTC for calculation of elapsed time since measure
          // TODO : calculate it by adding a UTCoffset
          struct tm tmEpochTimeUTC = getTzTime(utc_time_zone);
          time_t tEpochTimeUTC = mktime(&tmEpochTimeUTC);
        
        
          // Calculate the elapsed time since glucose measure - in minutes
          int elapsed_mn = round(difftime(tEpochTimeUTC, tMeasureTimeUTC) / 60.0);  // Difference in seconds, converted to minutes
          
          // Print the difference in minutes
          Serial.print("Difference in minutes: ");
          Serial.println(elapsed_mn);

        
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
            //lcd.print("mn");
            //Serial.print("mn");
            
          }
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
  
  delay(1*60*1000); // in ms -> retrieve data and update every minute
}
