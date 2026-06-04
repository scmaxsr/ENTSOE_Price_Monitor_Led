#include "helper_wifi_portal.h"
#include "helper_config.h"
#include "helper_led.h"

static String jsonEscapeValue(const String& value) {
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
        if ((uint8_t)c < 0x20) {
          escaped += ' ';
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}

// Start an async WiFi scan so the portal can fetch results from /scan
void startWiFiScan() {
  Serial.println("Starting async WiFi scan...");
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
}

// Scan WiFi networks and return JSON list
void handleScan() {
  Serial.println("Scanning WiFi networks...");
  if (server.hasArg("refresh") && server.arg("refresh") == "1") {
    startWiFiScan();
    server.send(200, "text/plain", "scanning");
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    startWiFiScan();
    server.send(200, "text/plain", "scanning");
    return;
  }

  if (n == WIFI_SCAN_RUNNING) {
    // Scan still in progress
    server.send(200, "text/plain", "scanning");
    return;
  }
  
  String json = "[";
  bool first = true;
  int rssiThreshold = -90; // Skip very weak networks
  
  for (int i = 0; i < n; i++) {
    if (WiFi.RSSI(i) < rssiThreshold) continue;
    
    if (!first) json += ",";
    first = false;
    
    json += "{\"ssid\":\"";
    json += jsonEscapeValue(WiFi.SSID(i));
    json += "\",\"rssi\":";
    json += WiFi.RSSI(i);
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
  Serial.printf("Found %d networks, returned %d\n", n, n);
}

// Root page - show config form
void handleRoot() {
  server.send(200, "text/html", configHTML);
}

// Handle save POST request
void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("apiKey") || !server.hasArg("webUser") || !server.hasArg("webPass")) {
    server.send(400, "text/plain", "SSID, API Key, Web Username and Web Password are required");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String apiKey = server.arg("apiKey");
  String webUser = server.arg("webUser");
  String webPass = server.arg("webPass");
  String biddingZone = server.arg("biddingZone");
  String timezone = server.arg("timezone");

  if (ssid.length() == 0 || apiKey.length() == 0 || webUser.length() == 0 || webPass.length() == 0) {
    server.send(400, "text/plain", "SSID, API Key, Web Username and Web Password cannot be empty");
    return;
  }

  if (biddingZone.length() == 0) {
    biddingZone = "10YNL----------L"; // Default NL zone
  }

  if (timezone.length() == 0) {
    timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // Default NL timezone
  }

  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  password.toCharArray(config.password, sizeof(config.password));
  apiKey.toCharArray(config.apiKey, sizeof(config.apiKey));
  webUser.toCharArray(config.webUser, sizeof(config.webUser));
  webPass.toCharArray(config.webPass, sizeof(config.webPass));
  biddingZone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  timezone.toCharArray(config.timezone, sizeof(config.timezone));
  config.ledBrightness = defaultLedBrightness;
  config.configured = true;

  if (saveConfig()) {
    server.send(200, "text/plain", "Configuration saved! Device is rebooting...");
    Serial.println("Config saved via portal. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Error saving configuration");
  }
}

// Handle not-found (captive portal redirect)
void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Start the configuration portal in AP mode
void startConfigPortal() {
  Serial.println("\n========================================");
  Serial.println("  NO CONFIGURATION FOUND");
  Serial.println("  Starting in AP mode...");
  Serial.println("========================================\n");

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ENTSOE-Monitor-Config");
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);

  // Start DNS server (captive portal)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", handleScan);
  server.onNotFound(handleNotFound);
  
  // Start WiFi scan for available networks
  startWiFiScan();

  server.begin();
  Serial.println("Web server started at http://" + apIP.toString());

  // Wait for configuration (with timeout)
  unsigned long portalStart = millis();
  const unsigned long PORTAL_TIMEOUT = 300000; // 5 minutes
  
  while (!config.configured && (millis() - portalStart) < PORTAL_TIMEOUT) {
    dnsServer.processNextRequest();
    server.handleClient();
    // Show AP mode animation on LED matrix
    matrixShowAPMode();
    // Small delay to prevent watchdog issues
    delay(10);
  }

  if (!config.configured) {
    Serial.println("Portal timeout - no configuration received. Rebooting...");
    delay(1000);
    ESP.restart();
  }
}

// Try to connect to WiFi with saved config
bool connectWithStoredConfig() {
  if (!config.configured || strlen(config.ssid) < 1) {
    return false;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(config.ssid);
  Serial.printf("Password length: %d, API key length: %d\n", strlen(config.password), strlen(config.apiKey));
  Serial.printf("Zone: %s, Timezone: %s\n", config.biddingZone, config.timezone);
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config.ssid, config.password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.printf("\nWiFi connection failed (status=%d)\n", WiFi.status());
    if (WiFi.status() == WL_NO_SSID_AVAIL) {
      Serial.println("SSID not available. Check that the router is in range and the SSID is correct.");
    }
    return false;
  }
}

// Main initialization - call from setup()
bool initWiFi() {
  Serial.println("\n--- WiFi Initialization ---");
  
  // Try to load saved config
  config.configured = false;
  
  if (loadConfig()) {
    Serial.println("Saved config found, connecting to WiFi...");
    if (connectWithStoredConfig()) {
      return true;
    } else {
      Serial.println("Could not connect with saved WiFi. Starting portal...");
      // Clear config so portal can overwrite
      clearConfig();
    }
  }

  // Start config portal
  startConfigPortal();
  
  // After portal, try connecting again
  return connectWithStoredConfig();
}

// Disconnect WiFi to save power
void disconnectWiFi() {
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disabled (power saving)");
}

// Getter functions for config values
const char* getApiKey() { return config.apiKey; }
const char* getBiddingZone() { return config.biddingZone; }
const char* getTimezone() { return config.timezone; }
