/**
 * NEVER EVER upload this secret file to GitHub
 */


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


// Parameters for time NTP server
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

// Time zone for local time and daylight saving
// list here:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
char* local_time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; // set for Europe/Paris
char* utc_time_zone = "GMT0";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


// Contrast level for LCD (0-255)
const int contrast = 75;