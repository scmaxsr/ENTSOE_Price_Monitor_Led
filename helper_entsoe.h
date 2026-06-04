#ifndef HELPER_ENTSOE_H
#define HELPER_ENTSOE_H

/*
 * ENTSO-E API Helper
 * 
 * Fetches day-ahead electricity prices from the ENTSO-E Transparency Platform API.
 * Parses the XML response and stores prices in a structure compatible with the LED matrix.
 * 
 * Note: ENTSO-E returns XML by default. On ESP8266 we parse it with simple string
 * extraction to avoid the overhead of a full XML library.
 */

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

// Forward declarations from helper_time.h
extern int getHoursOfDay();

// A single price valid for one hour
struct entsoe_price {
  char starttime[14];   // e.g. "20260506T1800"
  int price;            // EUR/MWh multiplied by 100, scaled for LED display
  int level;            // Price Level (1-5) - calculated based on min/max
  boolean isNull;       // True if data not available
};

static const int ENTSOE_PRICE_HOURS = 24;
static const int MATRIX_DISPLAY_HOURS = 8;

// Prices for the next 24 hours. The 8x8 LED matrix displays the first 8 entries.
struct entsoe_prices {
  entsoe_price price[ENTSOE_PRICE_HOURS];
  int minimumPrice = 100000;  // min EUR/MWh multiplied by 100
  int maximumPrice = 0;        // max EUR/MWh multiplied by 100
};

// Global prices structure (declared extern, defined in the .ino file)
extern entsoe_prices PRICES;
extern int entsoeLastHttpCode;
extern int entsoeLastResponseLength;
extern int entsoeLastPointCount;
extern int entsoeLastExtractedCount;
extern char entsoeLastPeriodStart[13];
extern char entsoeLastPeriodEnd[13];
extern char entsoeLastPreview[241];
extern char entsoeLastSeriesSummary[321];
extern char entsoeLastPointContext[321];
extern char entsoeLastExpectedCheck[81];

// Accessors for API keys and bidding zone (provided by WiFi/portal module)
const char* getApiKey();
const char* getBiddingZone();

// Prototypes for implementations in helper_entsoe.cpp
String extractBetween(const String& data, const String& startMarker, const String& endMarker, int startPos = 0);
void printPrices();
void calculateLevels();
void parseEntsoeXml(const String& xmlResponse, const String& requestPeriodStart, bool resetPrices = true);
String getDateString(int daysFromNow);
void getEntsoePrices();

#endif // HELPER_ENTSOE_H
