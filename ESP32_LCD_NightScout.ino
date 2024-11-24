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

// Internet time
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#include <time.h>

#include <ArduinoJson.h>

#include <LiquidCrystal.h>

// wifi credentials, NightScout URL and API key
#include "secrets.h"

WiFiMulti wifiMulti;

// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP);


/* 16x2 LCD
   LCD pins (right to left) - ESP32 pins
   1 GND supply             - GND
   2 VDD 5v                 - 5V
   3 Vo contrast adjustment - 16 (PWM)
   4 RS register select     - 22
   5 R/W read/write         - GND
   6 En Enable Signal       - 21
   7 DB0 Data Bit 0         - unused
   8 DB1 Data Bit 1         - unused
   9 DB2 Data Bit 2         - unused
  10 DB3 Data Bit 3         - unused
  11 DB4 Data Bit 4         -  5
  12 DB5 Data Bit 5         - 18
  13 DB6 Data Bit 6         - 23
  14 DB7 Data Bit 7         - 19
  15 +5V backlight optional (16 pins LCDs)
  16 GND backlight optional (16 pins LCDs)
*/

// Time zone for local time and daylight saving
// list here:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* local_time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; // set for Europe/Paris
const char* gmt_time_zone = "GMT0";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
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
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}


// PWM configuration for LCD contrast
const int pwmChannel = 0;   //Channel for PWM (0-15)
const int frequency = 1000; // PWM frequency 1 KHz
const int resolution = 8;   // PWM resolution 8 bits, 256 possible values
const int pwmPin = 16;
const int contrast = 75;    // 0-255

// 16x2 LCD
const int rs = 22, en = 21, d4 = 5, d5 = 18, d6 = 23, d7 = 19;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Arrows for glucose tendency: Flat; FortyFiveDown, SingleDown, DoubleDown, FortyFiveUp, SingleUp, DoubleUp
byte flat[8] = {
  B00000,
  B00100,
  B00010,
  B11111,
  B00010,
  B00100,
  B00000,
};

byte fourtyFiveDown[8] = {
  B00000,
  B10000,
  B01001,
  B00101,
  B00011,
  B01111,
  B00000,
};

byte singleDown[8] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100,
};

byte doubleDown[8] = {
  B01010,
  B01010,
  B01010,
  B01010,
  B10001,
  B01010,
  B00100,
};

byte fourtyFiveUp[8] = {
  B00000,
  B01111,
  B00011,
  B00101,
  B01001,
  B10000,
  B00000,
};

byte singleUp[8] = {
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B00100,
  B00100,
};

byte doubleUp[8] = {
  B00100,
  B01010,
  B10001,
  B01010,
  B01010,
  B01010,
  B01010,
};

void timeDiff(){
  // Tests for time difference - NOT WORKING
  
  const char* localT1 = "Saturday, November 23 2024 21:30:38 zone CET +0100";
  const char* utcT2 = "2024-11-23T20:28:55.000Z";

  // Parse UTC time to time_t
  struct tm tmUTC;
  strptime(utcT2, "%Y-%m-%dT%H:%M:%S.000Z", &tmUTC);  // Parse UTC time
  time_t tUTC = mktime(&tmUTC);  // Convert to time_t (UTC)
  
  // Print the UTC time for reference
  Serial.print("UTC Time (t2): ");
  Serial.println(ctime(&tUTC));

  // Parse local time (localT1) to struct tm
  struct tm tmLocal;
  strptime(localT1, "%A, %B %d %Y %H:%M:%S zone", &tmLocal);  // Parse local time string
  
  // Convert local time to time_t by applying correct local timezone adjustment
  time_t tLocal = mktime(&tmLocal);  // This handles the CET/CEST conversion automatically

  // Print the local time (adjusted to UTC) for reference
  Serial.print("Local Time (t1, adjusted to UTC): ");
  Serial.println(ctime(&tLocal));

  // Calculate the difference in minutes
  double diff = difftime(tLocal, tUTC) / 60.0;  // Difference in seconds, converted to minutes
  
  // Print the difference in minutes
  Serial.print("Difference in minutes: ");
  Serial.println(diff);
}


void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println("ESP start");

  // Initialize display
  Serial.println();
  Serial.println("[DISPLAY] Initializing LCD");
  // LCD Contrast
  ledcSetup(pwmChannel, frequency, resolution);
  ledcAttachPin(pwmPin, pwmChannel);
  ledcWrite(pwmChannel, contrast);
  //LCD arrows
  lcd.createChar(0, flat);
  lcd.createChar(1, fourtyFiveDown);
  lcd.createChar(2, singleDown);
  lcd.createChar(3, doubleDown);
  lcd.createChar(4, fourtyFiveUp);
  lcd.createChar(5, singleUp);
  lcd.createChar(6, doubleUp);
  
  lcd.begin(16,2); // columns, rows of the LCD, zero index( 0-15, 0-1 )

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
  initTime(local_time_zone);
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
