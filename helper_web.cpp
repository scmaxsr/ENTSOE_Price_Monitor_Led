#include "helper_web.h"
#include "helper_config.h"
#include "helper_ota.h"
#include "helper_wifi_portal.h"
#include <time.h>

bool checkWebAuth() {
  if (strlen(config.webUser) == 0 || strlen(config.webPass) == 0) {
    server.send(503, "text/plain", "Web login is not configured. Reconfigure in AP mode.");
    return false;
  }
  if (!server.authenticate(config.webUser, config.webPass)) {
    server.requestAuthentication(BASIC_AUTH, "ENTSO-E Monitor");
    return false;
  }
  return true;
}

static String jsonEscapeForApi(const String& value) {
  String escaped;
  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value[i];
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) escaped += ' ';
        else escaped += c;
        break;
    }
  }
  return escaped;
}

void handleApiPrices() {
  if (!checkWebAuth()) return;
  String json = "{\"prices\":[";
  float minCtKwh = 9999, maxCtKwh = -9999, totalCtKwh = 0;
  int count = 0;
  
  for (int i = 0; i < MATRIX_DISPLAY_HOURS; i++) {
    if (i > 0) json += ",";
    json += "{";
    if (!PRICES.price[i].isNull) {
      float eurMwh = PRICES.price[i].price / 100.0;
      float ctKwh = eurMwh / 10.0;
      json += "\"null\":false,\"price\":" + String(PRICES.price[i].price);
      json += ",\"priceEurMwh\":" + String(eurMwh, 2);
      json += ",\"priceCtKwh\":" + String(ctKwh, 2);
      json += ",\"level\":" + String(PRICES.price[i].level);
      json += ",\"height\":" + String((PRICES.maximumPrice - PRICES.minimumPrice > 0) ? 
        (7 * (PRICES.price[i].price - PRICES.minimumPrice) / (PRICES.maximumPrice - PRICES.minimumPrice) + 1) : 4);
      
      char buf[7];
      if (strlen(PRICES.price[i].starttime) >= 13) {
        snprintf(buf, sizeof(buf), "%c%c:%c%c",
                 PRICES.price[i].starttime[9],
                 PRICES.price[i].starttime[10],
                 PRICES.price[i].starttime[11],
                 PRICES.price[i].starttime[12]);
      } else {
        snprintf(buf, sizeof(buf), "--:--");
      }
      json += ",\"label\":\"" + String(buf) + "\"";
      
      if (ctKwh < minCtKwh) minCtKwh = ctKwh;
      if (ctKwh > maxCtKwh) maxCtKwh = ctKwh;
      totalCtKwh += ctKwh;
      count++;
    } else {
      json += "\"null\":true,\"price\":0,\"priceEurMwh\":0,\"priceCtKwh\":0,\"level\":3,\"height\":1,\"label\":\"--:--\"";
    }
    json += "}";
  }
  
  if (count == 0) { minCtKwh = 0; maxCtKwh = 0; }
  float avgCtKwh = count > 0 ? totalCtKwh / count : 0;
  float currentCtKwh = !PRICES.price[0].isNull ? (PRICES.price[0].price / 100.0) / 10.0 : 0;
  
  json += "],\"minCtKwh\":" + String(minCtKwh, 2);
  json += ",\"maxCtKwh\":" + String(maxCtKwh, 2);
  json += ",\"avgCtKwh\":" + String(avgCtKwh, 2);
  json += ",\"currentCtKwh\":" + String(currentCtKwh, 2);
  json += ",\"zone\":\"" + String(config.biddingZone) + "\"";
  json += ",\"firmwareVersion\":\"" + String(firmwareVersion) + "\"";
  json += ",\"debugHttp\":" + String(entsoeLastHttpCode);
  json += ",\"debugResponseLen\":" + String(entsoeLastResponseLength);
  json += ",\"debugPoints\":" + String(entsoeLastPointCount);
  json += ",\"debugExtracted\":" + String(entsoeLastExtractedCount);
  json += ",\"debugPeriodStart\":\"" + String(entsoeLastPeriodStart) + "\"";
  json += ",\"debugPeriodEnd\":\"" + String(entsoeLastPeriodEnd) + "\"";
  json += ",\"debugPreview\":\"" + jsonEscapeForApi(String(entsoeLastPreview)) + "\"";
  json += ",\"debugSeries\":\"" + jsonEscapeForApi(String(entsoeLastSeriesSummary)) + "\"";
  json += ",\"debugPointContext\":\"" + jsonEscapeForApi(String(entsoeLastPointContext)) + "\"";
  json += ",\"debugExpected\":\"" + jsonEscapeForApi(String(entsoeLastExpectedCheck)) + "\"";
  json += ",\"diagnosticSource\":\"" + String(entsoeLastSource) + "\"";
  json += ",\"diagnosticHttp\":" + String(entsoeLastHttpCode);
  json += ",\"diagnosticHours\":" + String(entsoeLastDisplayCount);
  json += ",\"diagnosticFreeHeap\":" + String(entsoeLastFreeHeap);
  json += ",\"diagnosticMaxBlock\":" + String(entsoeLastMaxFreeBlock);
  json += ",\"diagnosticFragmentation\":" + String(entsoeLastHeapFragmentation);
  if (entsoeLastSuccessMillis > 0) {
    json += ",\"diagnosticAgeSeconds\":" +
            String((millis() - entsoeLastSuccessMillis) / 1000UL);
  } else {
    json += ",\"diagnosticAgeSeconds\":-1";
  }
  if (entsoeLastSuccessEpoch > 0) {
    struct tm successTime;
    localtime_r(&entsoeLastSuccessEpoch, &successTime);
    char successBuf[20];
    strftime(successBuf, sizeof(successBuf), "%H:%M %d/%m", &successTime);
    json += ",\"diagnosticLastSuccess\":\"" + String(successBuf) + "\"";
  } else {
    json += ",\"diagnosticLastSuccess\":\"Never\"";
  }
  
  // Current time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[20];
    strftime(buf, sizeof(buf), "%H:%M %d/%m", &timeinfo);
    json += ",\"time\":\"" + String(buf) + "\"";
  } else {
    json += ",\"time\":\"--:--\"";
  }
  
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiConfig() {
  if (!checkWebAuth()) return;
  String maskedApiKey = "";
  size_t apiLen = strlen(config.apiKey);
  if (apiLen > 0) {
    if (apiLen <= 8) {
      maskedApiKey = "********";
    } else {
      maskedApiKey = String(config.apiKey).substring(0, 4) + "..." + String(config.apiKey).substring(apiLen - 4);
    }
  }

  String json = "{";
  json += "\"ssid\":\"" + jsonEscapeForApi(String(config.ssid)) + "\"";
  json += ",\"apiKey\":\"" + jsonEscapeForApi(maskedApiKey) + "\"";
  json += ",\"hasApiKey\":" + String(apiLen > 0 ? "true" : "false");
  json += ",\"webUser\":\"" + jsonEscapeForApi(String(config.webUser)) + "\"";
  json += ",\"hasWebPassword\":" + String(strlen(config.webPass) > 0 ? "true" : "false");
  json += ",\"biddingZone\":\"" + jsonEscapeForApi(String(config.biddingZone)) + "\"";
  json += ",\"timezone\":\"" + jsonEscapeForApi(String(config.timezone)) + "\"";
  json += ",\"ledBrightness\":" + String(config.ledBrightness > 0 ? config.ledBrightness : defaultLedBrightness);
  json += ",\"configured\":" + String(config.configured ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiSaveConfig() {
  if (!checkWebAuth()) return;
  if (!server.hasArg("ssid") || !server.hasArg("apiKey")) {
    server.send(400, "text/plain", "SSID and API Key are required");
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String apiKey = server.arg("apiKey");
  String webUser = server.arg("webUser");
  String webPass = server.arg("webPass");
  String biddingZone = server.arg("biddingZone");
  String timezone = server.arg("timezone");
  String ledBrightness = server.arg("ledBrightness");
  
  if (ssid.length() == 0 || (apiKey.length() == 0 && strlen(config.apiKey) == 0) ||
      webUser.length() == 0 || (webPass.length() == 0 && strlen(config.webPass) == 0)) {
    server.send(400, "text/plain", "SSID, API Key, Web Username and Web Password cannot be empty");
    return;
  }
  
  if (biddingZone.length() == 0) biddingZone = "10YNL----------L";
  if (timezone.length() == 0) timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  int brightnessValue = ledBrightness.length() > 0 ? ledBrightness.toInt() : config.ledBrightness;
  if (brightnessValue < 1) brightnessValue = 1;
  if (brightnessValue > 100) brightnessValue = 100;
  
  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  if (password.length() > 0) {
    password.toCharArray(config.password, sizeof(config.password));
  }
  if (apiKey.length() > 0) {
    apiKey.toCharArray(config.apiKey, sizeof(config.apiKey));
  }
  webUser.toCharArray(config.webUser, sizeof(config.webUser));
  if (webPass.length() > 0) {
    webPass.toCharArray(config.webPass, sizeof(config.webPass));
  }
  biddingZone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  timezone.toCharArray(config.timezone, sizeof(config.timezone));
  config.ledBrightness = (uint8_t)brightnessValue;
  config.configured = true;
  
  if (saveConfig()) {
    server.send(200, "text/plain", "Settings saved! Rebooting...");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Error saving configuration");
  }
}

void handleApiReset() {
  if (!checkWebAuth()) return;
  clearConfig();
  server.send(200, "text/plain", "Configuration cleared. Device will reboot...");
  delay(500);
  ESP.restart();
}

void handleApiLatestOta() {
  if (!checkWebAuth()) return;
  if (!requestLatestOtaUpdate()) {
    server.send(409, "application/json",
                "{\"ok\":false,\"message\":\"OTA update already queued\"}");
    return;
  }
  server.send(202, "application/json",
              "{\"ok\":true,\"message\":\"Downloading the latest GitHub release. The device will reboot after installation.\"}");
}

void initWebInterface() {
  // Dashboard
  server.on("/", []() {
    if (!checkWebAuth()) return;
    server.send(200, "text/html", dashboardHTML);
  });
  server.on("/dashboard", []() {
    if (!checkWebAuth()) return;
    server.send(200, "text/html", dashboardHTML);
  });
  server.on("/diagnostics", []() {
    if (!checkWebAuth()) return;
    server.send(200, "text/html", diagnosticsHTML);
  });
  
  // API endpoints
  server.on("/api/prices", handleApiPrices);
  server.on("/api/config", HTTP_GET, handleApiConfig);
  server.on("/api/config", HTTP_POST, handleApiSaveConfig);
  server.on("/api/reset", HTTP_POST, handleApiReset);
  server.on("/api/ota/latest", HTTP_POST, handleApiLatestOta);
  server.on("/scan", []() {
    if (!checkWebAuth()) return;
    handleScan();
  });
  
  // Settings page
  server.on("/settings", []() {
    if (!checkWebAuth()) return;
    server.send(200, "text/html", settingsHTML);
  });
  
  // Reset page
  server.on("/reset", []() {
    if (!checkWebAuth()) return;
    server.send(200, "text/html", resetHTML);
  });
  
  Serial.println("Web interface initialized:");
  Serial.println("  /dashboard   - Price chart");
  Serial.println("  /settings    - Change configuration");
  Serial.println("  /api/prices  - JSON price data");
  Serial.println("  /api/config  - JSON config data");
  Serial.println("  /api/reset   - Factory reset");
  Serial.println("  /api/ota/latest - Install latest GitHub release");
  Serial.println("  /scan        - WiFi network scan");
  startWiFiScan();
}
