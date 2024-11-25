/**
 * ESP32_LCD_NightScout.ino
 *
 *  Version date: 2024-11-23
 *  
 *  For diabetics using a NightScout database
 *  Retrieve glycemia every 2 minutes
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

#include "lcd_16x2.h"

// wifi credentials, NightScout URL and API key
#include "secrets.h"

// Tests for time difference
#include "time_diff.hpp"


WiFiMulti wifiMulti;


void setTimezone(char* timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone);
  configTzTime(timezone, ntpServer1, ntpServer2);
}


void initTime(char* timezone){
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    Serial.println("  Failed to obtain time");
    return;
  }
  Serial.println("  Got the time from NTP");
  // Set timezone
  setTimezone(timezone);
}


void printLocalTime(){
  configTzTime(local_time_zone, ntpServer1, ntpServer2);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println("System time in local TZ: ");
  Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}


void printUtcTime(){
  configTzTime(utc_time_zone, ntpServer1, ntpServer2);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println("System time in UTC TZ  : ");
  Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
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
  // timeClient.begin();
  // initTime(&local_time_zone);
  printLocalTime();

  timeDiff();
}

void loop() {
  // wait for WiFi connection
  if((wifiMulti.run() == WL_CONNECTED)) {
      
    HTTPClient http;

    Serial.println();
    Serial.println("[HTTP] begin...");
    http.begin(NS_API_URL); // latest value
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
          //
          // Most of the time, you can rely on the implicit casts.
          // In other case, you can do doc["time"].as<long>();
          const char* type = httpResponseBody[0]["type"];
          // Directions: Flat, FortyFiveUp, FortyFiveDown, SingleUp, SingleDown, DoubleUp, DoubleDown
          const char* direct = httpResponseBody[0]["direction"];
          long sgv = long(httpResponseBody[0]["sgv"]);
          
          const char* sysTime = httpResponseBody[0]["sysTime"]; // GMT
          char measureTime[9];
          strncpy(measureTime, sysTime + (12 - 1), 8); // GMT
          measureTime[8] = '\0'; // end of chain

        
          // Display values
          Serial.println(type);
          Serial.println(direct);
          Serial.println(sgv);
          Serial.println(sysTime); // GMT of the measure
          Serial.println(measureTime); // GMT of the measure
          Serial.println("\n0123456789012345\n"); // New line before printing same message on serial than LCD
          
          if (strcmp(type, "sgv") == 0){
            lcd.clear();
            if (sgv < 100){
              lcd.setCursor(3,0);
              Serial.print("__");
            } else {
              lcd.setCursor(2,0);
              Serial.print("_");
            }
            lcd.print(sgv);
            Serial.print(sgv);
            // lcd.setCursor(6,0);
            lcd.print (" mg/dL ");
            Serial.print("_mg/dL_");
            
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
            lcd.setCursor(0,1);
            Serial.println();
            lcd.print("at ");
            Serial.print("at_");
            lcd.print(measureTime);
            Serial.print(measureTime);
            lcd.setCursor(11,1);
            lcd.print(" UTC ");
            Serial.print("_UTC_");
          }
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
  
  // timeClient.update();
  // Serial.println(timeClient.getFormattedTime());
  
  delay(2*60*1000); // in ms -> retrieve data and update every 2 minutes
}
