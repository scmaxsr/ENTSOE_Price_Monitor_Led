
#ifndef HELPER_TIME_H
#define HELPER_TIME_H

#include <Arduino.h>

// Time helper prototypes (implemented in helper_time.cpp)
void setTimezone(const String& timezone);
void initTime(const String& timezone);
void updateTime(const String& timezone);
int getHoursOfDay();
void printLocalTime();
int minutesToHour();

#endif // HELPER_TIME_H