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
