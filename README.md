# Gluci-clock
![Gluci-clock prototype photo](static/gluci-clock.jpg)
For diabetics using a Continuous Glucose Monitor and a NightScout database.
This project will allow displaying NightScout data on 16x2 LCD screen connected to an ESP32.

Information displayed on the screen:
- Glucose level
- Glucose evolution tendency
- Number of minutes since the last measure
- Day and time

This will make a very affordable clock displaying glucose level and evolution.

### Setup
#### Configuring Wifi, NightScout and timezone
To configure, create a secret.h file:
```C
// secrets.h file

// Wifi credentials for secured or open networks

struct wifi_cred {
  const char *ssid;
  const char *password;
};

const wifi_cred WIFI_CREDENTIALS[] = {
  {"wifi_ssid1", "wifi_pw1"},
  {"wifi_ssid2", "wifi_pw2"},
  {"wifi_ssid3"},
};


// url and API key for Nightscout

// Don't foreget to add '?count=1' to the URL
const char *NS_API_URL = "https://your-nightscout-site/api/v1/entries.json?count=1"

// key defined in NS > Hamburger menu > Admin tools
// with API READ rights
const char *NS_API_SECRET = "your-API-secret or token";


// Parameters for time NTP server
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

// Time zone for local time and daylight saving
// list here:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* local_time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; // set for Europe/Paris
const char* utc_time_zone = "GMT0";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
```
Don't foreget to add this ```secrets.h``` file in ```.gitignore``` file, never share it on GitHub.

#### Install card and libraries in Arduino IDE, compile code and flash card. Connect the LCD as instructed below.

### Materials needed:

#### ESP32 development module:

AZDelivery ESP32 NodeMCU module WLAN Wifi Dev Kit C (with CP 2102).
https://amzn.eu/d/8uNnGYs

You might need to press Boot button to flash your board.

#### 16x2 LCD screen:
I used an old screen from my DIY box...

#### Cables

### Wiring between ESP32 and LCD:

The LCD used in this diagram is a 14 pins without backlight. More common ones are 16 pins with backlight, pins order may then be reversed.

| 16x2 LCD pins (right to left) |  ESP32 pins  |
| ----------------------------- | ------------ |
| 1 GND supply                  |     GND      |
| 2 VDD 5v                      |      5V      |
| 3 Vo contrast adjustment      |   16 (PWM)   |
| 4 RS register select          |      22      |
| 5 R/W read/write              |     GND      |
| 6 En Enable Signal            |      21      |
| 7 DB0 Data Bit 0              |    unused    |
| 8 DB1 Data Bit 1              |    unused    |
| 9 DB2 Data Bit 2              |    unused    |
|10 DB3 Data Bit 3              |    unused    |
|11 DB4 Data Bit 4              |       5      |
|12 DB5 Data Bit 5              |      18      |
|13 DB6 Data Bit 6              |      23      |
|14 DB7 Data Bit 7              |      19      |
|15 +5V backlight optional      |(16 pins LCDs)|
|16 GND backlight optional      |(16 pins LCDs)|

![ESP32 with LCD 16x2 wiring diagram](static/esp32-sc1602lcd.jpg)

### Development environment:
Arduino IDE version 1.8.19

Card: ESP32 dev module. https://github.com/espressif/arduino-esp32

Upload speed: 921600

CPU frequency: 240 MHZ

Flash frequency: 80 MHZ

Flash mode: QIO

Flash size: 4 Mbits

Partition scheme: default 4MB with spiffs

Core debug level: None

PSRAM: disabled

Arduino Runs On: Core 1

Events Runs On: Core 1

Erase Flash Before Sketch Upload: disabled

JTAG Adapter: disabled

Programmer: Esptool

### Dependencies: libraries used
ArduinoJson-6.21.5 https://www.arduino.cc/reference/en/libraries/arduinojson/

LiquidCrystal 1.0.7 https://www.arduino.cc/reference/en/libraries/liquidcrystal/

WifiMulti_Generic 1.2.2 https://www.arduino.cc/reference/en/libraries/wifimulti_generic/




