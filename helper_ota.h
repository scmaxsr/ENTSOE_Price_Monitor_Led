#ifndef HELPER_OTA_H
#define HELPER_OTA_H

/*
 * OTA (Over-The-Air) Update Helper for ESP8266
 * 
 * Allows firmware updates via web browser without USB cable.
 * Uses ESP8266HTTPUpdateServer (included in ESP8266 Arduino Core).
 * 
 * Usage:
 *   1. Compile your sketch in Arduino IDE
 *   2. Export compiled binary: Sketch → Export Compiled Binary
 *   3. Go to http://{ESP-IP}/update in your browser
 *   4. Upload the .ino.bin file
 *   5. ESP reboots with new firmware
 * 
 * Security: OTA page is password-protected with the WiFi password
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

// Forward declaration: we need the web server from helper_wifi_portal
extern ESP8266WebServer server;

ESP8266HTTPUpdateServerTemplate<WiFiServer> httpUpdater;

// Initialize OTA update server
// Mounts at /update path
void initOTA() {
  httpUpdater.setup(&server, "/update", config.password, config.password);
  Serial.println("OTA update available at /update");
  Serial.printf("  Username: %s\n", config.password);
  Serial.println("  Upload .ino.bin file (Sketch -> Export Compiled Binary)");
}

#endif // HELPER_OTA_H