#include "helper_time.h"
#include "settings.h"

static bool waitForLocalTime(struct tm& timeinfo, int timeoutSeconds = 10) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)timeoutSeconds * 1000) {
    if (getLocalTime(&timeinfo)) {
      return true;
    }
    delay(500);
  }
  return false;
}

void setTimezone(const String& timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  static char tzBuf[64];
  strncpy(tzBuf, timezone.c_str(), sizeof(tzBuf)-1);
  tzBuf[sizeof(tzBuf)-1] = '\0';
  setenv("TZ", tzBuf, 1);
  tzset();
}

void initTime(const String& timezone){
  struct tm timeinfo;
  Serial.println("Setting up time");
  setTimezone("GMT0");
  Serial.println("  Trying NTP servers: pool.ntp.org, time.nist.gov, time.apple.com");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.apple.com");
  Serial.println("  Waiting for NTP time...");
  if(!waitForLocalTime(timeinfo, 15)){
    Serial.println("  Failed to obtain time");
    setTimezone(timezone);
    return;
  }
  Serial.println("  Got the time from NTP");
  setTimezone(timezone);
}

void updateTime(const String& timezone){
  struct tm timeinfo;
  setTimezone("GMT0");
  Serial.println("  Trying NTP servers: pool.ntp.org, time.nist.gov, time.apple.com");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.apple.com");
  Serial.println("  Refreshing NTP time...");
  if(!waitForLocalTime(timeinfo, 15)){
    Serial.println("  Failed to obtain time");
    setTimezone(timezone);
    return;
  }
  setTimezone(timezone);
}

int getHoursOfDay() {
  struct tm timeinfo;
  if(!waitForLocalTime(timeinfo, 5)){
    Serial.println("Failed to obtain time");
    return -1;
  }
  return (int) timeinfo.tm_hour;
}

void printLocalTime(){
  struct tm timeinfo;
  if(!waitForLocalTime(timeinfo, 5)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("Local Time: ");
  char localTime[9];
  strftime(localTime,9, "%H:%M:%S", &timeinfo);
  Serial.println(localTime);
}

int minutesToHour(){
    struct tm timeinfo;
    if(!waitForLocalTime(timeinfo, 5)){
      Serial.println("Failed to obtain time");
      return -1;
    }
  return 59 - (int) timeinfo.tm_min;
}
