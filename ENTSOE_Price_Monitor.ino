// ENTSOE Monitor Recovery v3 - always format + always AP
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <FS.h>
#include "helper_config.h"
#include "helper_wifi_portal.h"
#include "helper_web.h"
#include "helper_time.h"
#include "helper_entsoe.h"
#include "helper_led.h"
#include "helper_ota.h"

bool apMode = false;

ESP8266WebServer server(80);
DNSServer dnsServer;
Config config;
entsoe_prices PRICES;
const char* apSSID = "ENTSOE-Monitor-Config";
static int lastPriceRefreshHour = -1;
static unsigned long lastHourCheckMs = 0;
static unsigned long lastPriceRefreshAttemptMs = 0;
static const unsigned long HOUR_CHECK_INTERVAL_MS = 30000;
static const unsigned long PRICE_RETRY_INTERVAL_MS = 300000;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ENTSO-E Monitor</title>
<style>
body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;margin:20px;max-width:500px}
h1{color:#00d4aa}label{display:block;margin:12px 0 4px;color:#aaa}
input{width:100%;padding:8px;border:1px solid #333;border-radius:4px;background:#16213e;color:#fff;box-sizing:border-box}
button{background:#00d4aa;color:#111;border:none;padding:12px 24px;border-radius:4px;font-size:16px;margin-top:16px;width:100%;cursor:pointer}
</style></head><body>
<h1>ENTSO-E Monitor</h1>
<form id="cfg">
<label>WiFi SSID</label><input id="ssid" name="ssid" required>
<label>WiFi Wachtwoord</label><input type="password" id="pwd" name="password">
<label>API Key</label><input id="api" name="apiKey">
<label>Bidding Zone</label><input id="zone" name="biddingZone" value="10YNL----------L">
<label>Tijdzone</label><input id="tz" name="timezone" value="CET-1CEST,M3.5.0,M10.5.0/3">
<button type="submit">Opslaan & Herstarten</button>
</form>
<script>
document.getElementById('cfg').onsubmit=async(e)=>{
e.preventDefault();const d=new FormData(e.target);
const r=await fetch('/save',{method:'POST',body:new URLSearchParams(d)});
const text = await r.text();
if (r.ok) {
  alert('Opgeslagen!\n' + text);
  setTimeout(()=>location.reload(),2000);
} else {
  alert('Fout: ' + text);
}
</script></body></html>
)rawliteral";

const char RESET_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Reset</title>
<style>body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}
h1{color:#e17055}button{background:#e17055;color:#fff;padding:12px 24px;border:none;cursor:pointer}
</style></head><body><h1>Reset</h1>
<button onclick="fetch('/api/reset',{method:'POST'}).then(r=>r.json()).then(j=>{alert(j.ok?'Reset!':'Fout');location.reload()})">Reset ESP</button></body></html>
)rawliteral";

// Config is handled by helper_config.* (loadConfig/saveConfig)


void handleReset() {
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  SPIFFS.format(); ESP.rtcUserMemoryWrite(0,(uint32_t*)0,0);
  #pragma GCC diagnostic pop
  server.send(200,"application/json","{\"ok\":true}");
  delay(100); ESP.restart();
}

void handleStatus() {
  if (!apMode && !checkWebAuth()) return;
  String j="{\"ap\":"+String(WiFi.getMode()==WIFI_AP?"true":"false");
  j+=" ,\"ip\":\"" + WiFi.localIP().toString() + "\",\"ssid\":\"" + WiFi.SSID() + "\"}";
  server.send(200,"application/json",j);
}

void handleResetPage() { server.send(200,"text/html",FPSTR(RESET_HTML)); }

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\n=== ENTSOE Recovery v3 ===");
  
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if(!SPIFFS.begin()) {
    SPIFFS.format(); SPIFFS.begin(); 
  }
  #pragma GCC diagnostic pop
  
  bool hasCfg = loadConfig();
  bool connected = false;
  
  if(hasCfg) {
    Serial.printf("Config found: SSID=\"%s\" zone=%s\n",config.ssid,config.biddingZone);
    Serial.print("Trying STA... ");
    WiFi.mode(WIFI_STA);
    WiFi.hostname(portalDomain);
    WiFi.begin(config.ssid, config.password);
    int w=0;
    const int maxAttempts = 60; // 30 seconds
    while (WiFi.status() != WL_CONNECTED && w < maxAttempts) {
      delay(500);
      Serial.print(".");
      w++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.printf("\nConnected! STA IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
      Serial.printf("Subnet: %s\n", WiFi.subnetMask().toString().c_str());
      Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
      Serial.printf("Mode: %d\n", WiFi.getMode());
    } else {
      Serial.printf("\nSTA failed (%d), starting AP mode\n", WiFi.status());
      Serial.printf("Saved SSID: %s\n", config.ssid);
      Serial.printf("Saved password length: %d\n", strlen(config.password));
    }
  }
  
  if (connected) {
    apMode = false;
    Serial.println("Refreshing time and ENTSO-E price data...");
    matrixInitialize();
    matrixShowConnecting();
    initTime(config.timezone);
    bool pricesLoaded = getEntsoePrices();
    lastPriceRefreshHour = pricesLoaded ? getHoursOfDay() : -1;
    matrixShowEntsoe();
    Serial.println("Starting local web interface on internal network...");
    initWebInterface();
    server.on("/status",handleStatus);
    server.onNotFound([]{server.send(404,"text/plain","404");});
    server.begin();
    if (MDNS.begin(portalDomain)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("mDNS available at http://%s.local\n", portalDomain);
    } else {
      Serial.println("mDNS initialization failed");
    }
    Serial.printf("Web interface available at http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID);
    Serial.printf("AP: %s on 192.168.4.1\n",apSSID);
    if (!hasCfg) {
      Serial.println("No config - AP only");
    }
    dnsServer.start(53,"*",WiFi.softAPIP());
    server.on("/",handleRoot);
    server.on("/scan",handleScan);
    server.on("/save",HTTP_POST,handleSave);
    server.on("/reset",handleResetPage);
    server.on("/api/reset",HTTP_POST,handleReset);
    server.on("/status",handleStatus);
    server.onNotFound([]{server.send(404,"text/plain","404");});
    server.begin();
    startWiFiScan();
    Serial.println("✓ AP portal running");
    Serial.println("Config: http://192.168.4.1");
  }
}

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
  } else {
    MDNS.update();
    unsigned long now = millis();
    if (lastHourCheckMs == 0 || now - lastHourCheckMs >= HOUR_CHECK_INTERVAL_MS) {
      lastHourCheckMs = now;
      int currentHour = getHoursOfDay();
      bool refreshRequired = currentHour >= 0 &&
                             (lastPriceRefreshHour < 0 || currentHour != lastPriceRefreshHour);
      bool retryAllowed = lastPriceRefreshAttemptMs == 0 ||
                          now - lastPriceRefreshAttemptMs >= PRICE_RETRY_INTERVAL_MS;
      if (refreshRequired && retryAllowed) {
        Serial.printf("Hour changed from %d to %d, refreshing price window...\n",
                      lastPriceRefreshHour, currentHour);
        lastPriceRefreshAttemptMs = now;
        updateTime(config.timezone);
        if (getEntsoePrices()) {
          matrixShowEntsoe();
          int refreshedHour = getHoursOfDay();
          lastPriceRefreshHour = refreshedHour >= 0 ? refreshedHour : currentHour;
          lastPriceRefreshAttemptMs = 0;
        } else {
          Serial.println("Price refresh failed; retrying in 5 minutes");
        }
      } else if (currentHour >= 0 && lastPriceRefreshHour < 0) {
        // Keep retrying until the first valid price window has been loaded.
      }
    }
  }
  server.handleClient();
  if (!apMode) {
    processLatestOtaUpdate();
  }
}
