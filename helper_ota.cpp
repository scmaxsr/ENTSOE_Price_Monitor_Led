#include "helper_ota.h"

#include <Arduino.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>

#include "settings.h"

static bool otaUpdatePending = false;

bool requestLatestOtaUpdate() {
  if (otaUpdatePending) return false;
  otaUpdatePending = true;
  return true;
}

void processLatestOtaUpdate() {
  if (!otaUpdatePending) return;
  otaUpdatePending = false;

  delay(500);
  Serial.printf("Downloading latest OTA firmware: %s\n", githubLatestFirmwareUrl);

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  ESPhttpUpdate.setClientTimeout(30000);
  ESPhttpUpdate.onStart([]() {
    Serial.println("OTA update started");
  });
  ESPhttpUpdate.onProgress([](int current, int total) {
    if (total > 0) {
      Serial.printf("OTA progress: %d%%\n", (current * 100) / total);
    }
  });
  ESPhttpUpdate.onEnd([]() {
    Serial.println("OTA update completed; rebooting");
  });
  ESPhttpUpdate.onError([](int error) {
    Serial.printf("OTA update error %d: %s\n",
                  error, ESPhttpUpdate.getLastErrorString().c_str());
  });

  HTTPUpdateResult result = ESPhttpUpdate.update(client, githubLatestFirmwareUrl);
  if (result == HTTP_UPDATE_NO_UPDATES) {
    Serial.println("OTA update: no update available");
  } else if (result == HTTP_UPDATE_FAILED) {
    Serial.printf("OTA update failed: %s\n",
                  ESPhttpUpdate.getLastErrorString().c_str());
  }
}
