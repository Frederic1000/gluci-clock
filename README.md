# ESP32-LCD-NightScout
Display NightScout data on 16x2 LCD screen connected to an ESP32

Card profile in Arduino: ESP32 dev module

### Wifi
To connect to Wifi, create a secret.h file:
```C
// secrets.h file

// Wifi credentials for secured or open networks

struct wifi_cred {
  const char *ssid;
  const char *password;
};

const struct wifi_cred WIFI_CREDENTIALS[] = {
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
```
Don't foreget to add this ```secrets.h``` file in ```.gitignore``` file.

### Dependencies
ArduinoJson-6.21.5 https://www.arduino.cc/reference/en/libraries/arduinojson/

LiquidCrystal 1.0.7 https://www.arduino.cc/reference/en/libraries/liquidcrystal/

WifiMulti_Generic 1.2.2 https://www.arduino.cc/reference/en/libraries/wifimulti_generic/

