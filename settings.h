#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Default settings (overwritten by config from SPIFFS)
#define default_ssid "YourWiFiSSID"
#define default_password "YourWiFiPassword"
#define default_apiKey "YOUR_ENTSOE_API_KEY"
#define default_biddingZone "10YNL----------L"
#define entsoeApi "https://web-api.tp.entsoe.eu/api"
#define entsoeApiFallback "https://external-api.tp.entsoe.eu/api"
static const uint16_t port = 443;
#define ntpServer "pool.ntp.org"
static const long gmtOffset_sec = 3600;
static const int daylightOffset_sec = 3600;
#define timeZoneNL "CET-1CEST,M3.5.0,M10.5.0/3"
#define defaultLedBrightness 20
#define configFile "/cfg"
#define portalDomain "pricemonitor"
#define firmwareVersion "v1.3.3"
#define githubLatestFirmwareUrl "https://github.com/scmaxsr/ENTSOE_Price_Monitor_Led/releases/latest/download/ENTSOE_Price_Monitor_Led_esp8266.bin"

// Configuration structure
struct Config {
  char ssid[32];
  char password[64];
  char apiKey[64];
  char biddingZone[32];
  char timezone[48];
  char webUser[32];
  char webPass[64];
  uint8_t ledBrightness;
  bool configured;
};

#endif
