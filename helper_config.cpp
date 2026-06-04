#include "helper_config.h"
#include <Arduino.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static String jsonVal(const String& j, const char* k) {
  String s = String("\"") + k + "\":\"";
  int a = j.indexOf(s);
  if (a < 0) return "";
  a += s.length();

  String value;
  bool escaped = false;
  for (unsigned int i = a; i < j.length(); i++) {
    char c = j[i];
    if (escaped) {
      switch (c) {
        case '"': value += '"'; break;
        case '\\': value += '\\'; break;
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        default: value += c; break;
      }
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '"') {
      return value;
    } else {
      value += c;
    }
  }
  return "";
}

static String jsonEscape(const String& value) {
  String escaped;
  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value[i];
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '\"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped += c; break;
    }
  }
  return escaped;
}

bool loadConfig() {
  if (!SPIFFS.exists(configFile)) return false;
  File f = SPIFFS.open(configFile, "r"); if(!f) return false;
  String j = f.readString(); f.close();

  Serial.printf("Loading config from %s (%d bytes)\n", configFile, j.length());

  String ssid = jsonVal(j, "ssid");
  String pwd  = jsonVal(j, "pwd");
  String api  = jsonVal(j, "api");
  String zone = jsonVal(j, "zone");
  String tz   = jsonVal(j, "tz");
  String webUser = jsonVal(j, "webUser");
  String webPass = jsonVal(j, "webPass");
  String brightness = jsonVal(j, "brightness");

  if (ssid.length() == 0 || webUser.length() == 0 || webPass.length() == 0) {
    Serial.println("Config load failed: ssid or web login missing");
    return false;
  }

  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  pwd.toCharArray(config.password, sizeof(config.password));
  api.toCharArray(config.apiKey, sizeof(config.apiKey));
  zone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  tz.toCharArray(config.timezone, sizeof(config.timezone));
  webUser.toCharArray(config.webUser, sizeof(config.webUser));
  webPass.toCharArray(config.webPass, sizeof(config.webPass));
  int brightnessValue = brightness.length() > 0 ? brightness.toInt() : defaultLedBrightness;
  if (brightnessValue < 1) brightnessValue = 1;
  if (brightnessValue > 100) brightnessValue = 100;
  config.ledBrightness = (uint8_t)brightnessValue;
  config.configured = true;

  Serial.printf("Loaded config: ssid='%s' pwdlen=%d apiKeylen=%d zone='%s' tz='%s' webUser='%s' brightness=%d\n",
                config.ssid, strlen(config.password), strlen(config.apiKey),
                config.biddingZone, config.timezone, config.webUser, config.ledBrightness);
  return true;
}

bool saveConfig() {
  File f = SPIFFS.open(configFile, "w");
  if (!f) {
    Serial.printf("Failed to open %s for writing\n", configFile);
    return false;
  }
  String j = "{\"ssid\":\"" + jsonEscape(String(config.ssid)) + "\","
             "\"pwd\":\"" + jsonEscape(String(config.password)) + "\","
             "\"api\":\"" + jsonEscape(String(config.apiKey)) + "\","
             "\"zone\":\"" + jsonEscape(String(config.biddingZone)) + "\","
             "\"tz\":\"" + jsonEscape(String(config.timezone)) + "\","
             "\"webUser\":\"" + jsonEscape(String(config.webUser)) + "\","
             "\"webPass\":\"" + jsonEscape(String(config.webPass)) + "\","
             "\"brightness\":\"" + String(config.ledBrightness) + "\"}";
  f.print(j);
  f.close();
  Serial.printf("Saved config to %s (%d bytes)\n", configFile, j.length());
  return true;
}

bool hasValidConfig() {
  if (!loadConfig()) return false;
  if (strlen(config.ssid) < 1 || strlen(config.apiKey) < 5) return false;
  if (strlen(config.webUser) < 1 || strlen(config.webPass) < 1) return false;
  return true;
}

void clearConfig() {
  if (SPIFFS.exists(configFile)) SPIFFS.remove(configFile);
  config.configured = false;
}

#pragma GCC diagnostic pop
