#include "helper_entsoe.h"
#include "helper_time.h"
#include "settings.h"
#include <BearSSLHelpers.h>

int entsoeLastHttpCode = 0;
int entsoeLastResponseLength = 0;
int entsoeLastPointCount = 0;
int entsoeLastExtractedCount = 0;
char entsoeLastPeriodStart[13] = "";
char entsoeLastPeriodEnd[13] = "";
char entsoeLastPreview[241] = "";
char entsoeLastSeriesSummary[321] = "";
char entsoeLastPointContext[321] = "";
char entsoeLastExpectedCheck[81] = "";

String extractBetween(const String& data, const String& startMarker, const String& endMarker, int startPos) {
  int startIdx = data.indexOf(startMarker, startPos);
  if (startIdx == -1) return "";
  startIdx += startMarker.length();
  int endIdx = data.indexOf(endMarker, startIdx);
  if (endIdx == -1) return "";
  return data.substring(startIdx, endIdx);
}

static bool isXmlTagDelimiter(char value) {
  return value == '>' || value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static bool matchesLocalTagName(const String& data, int tagStart, const char* tagName, bool closingTag) {
  int nameStart = tagStart + 1;
  if (closingTag) nameStart++;
  if (nameStart < 0 || nameStart >= (int)data.length()) return false;

  int nameEnd = nameStart;
  while (nameEnd < (int)data.length() && !isXmlTagDelimiter(data[nameEnd])) {
    nameEnd++;
  }

  int localStart = nameStart;
  for (int i = nameStart; i < nameEnd; i++) {
    if (data[i] == ':') localStart = i + 1;
  }

  int localLen = nameEnd - localStart;
  int tagLen = strlen(tagName);
  if (localLen != tagLen) return false;

  for (int i = 0; i < tagLen; i++) {
    if (data[localStart + i] != tagName[i]) return false;
  }
  return true;
}

static int findTagStart(const String& data, const char* tagName, int startPos) {
  int pos = startPos;
  while (true) {
    pos = data.indexOf('<', pos);
    if (pos == -1) return -1;
    if (pos + 1 < (int)data.length() && data[pos + 1] != '/' &&
        matchesLocalTagName(data, pos, tagName, false)) {
      return pos;
    }
    pos++;
  }
}

static int findTagEndStart(const String& data, const char* tagName, int startPos) {
  int pos = startPos;
  while (true) {
    pos = data.indexOf("</", pos);
    if (pos == -1) return -1;
    if (matchesLocalTagName(data, pos, tagName, true)) return pos;
    pos += 2;
  }
}

static int findTagContentStart(const String& data, int tagStart) {
  int close = data.indexOf('>', tagStart);
  if (close == -1) return -1;
  return close + 1;
}

static String extractLocalTagTextBounded(const String& data, const char* tagName, int startPos, int maxPos) {
  int tagStart = findTagStart(data, tagName, startPos);
  if (tagStart == -1 || tagStart >= maxPos) return "";
  int contentStart = findTagContentStart(data, tagStart);
  if (contentStart == -1 || contentStart > maxPos) return "";
  int tagEnd = findTagEndStart(data, tagName, contentStart);
  if (tagEnd == -1 || tagEnd > maxPos) return "";
  return data.substring(contentStart, tagEnd);
}

static bool parseXmlUtcTime(const String& value, struct tm* out) {
  if (value.length() < 16 || out == nullptr) return false;
  memset(out, 0, sizeof(struct tm));
  out->tm_year = value.substring(0, 4).toInt() - 1900;
  out->tm_mon = value.substring(5, 7).toInt() - 1;
  out->tm_mday = value.substring(8, 10).toInt();
  out->tm_hour = value.substring(11, 13).toInt();
  out->tm_min = value.substring(14, 16).toInt();
  out->tm_sec = 0;
  return out->tm_year >= 100 && out->tm_mon >= 0 && out->tm_mon <= 11;
}

static int32_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

static time_t getUtcFromTm(const struct tm* utcTime) {
  int year = utcTime->tm_year + 1900;
  unsigned month = (unsigned)(utcTime->tm_mon + 1);
  unsigned day = (unsigned)utcTime->tm_mday;
  int32_t days = daysFromCivil(year, month, day);
  return (time_t)days * 86400 + (time_t)utcTime->tm_hour * 3600 +
         (time_t)utcTime->tm_min * 60 + utcTime->tm_sec;
}

static bool getTargetUtc(time_t* targetUtc, char labels[][14], int count) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;

  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t baseLocal = mktime(&timeinfo);
  for (int i = 0; i < count; i++) {
    time_t localSlot = baseLocal + ((time_t)i * 3600);
    struct tm* localTime = localtime(&localSlot);
    strftime(labels[i], 14, "%Y%m%dT%H%M", localTime);
    targetUtc[i] = localSlot;
  }
  return true;
}

struct ParsedSeries {
  int classification;
  int resolutionSeconds;
  time_t startUtc;
  int pointCount;
  int prices[ENTSOE_PRICE_HOURS * 6];
  bool valid[ENTSOE_PRICE_HOURS * 6];
};

static int getSeriesPriority(int classification) {
  if (classification == 1) return 0;
  if (classification == 2) return 1;
  return 2;
}

static bool averageSeriesHour(const ParsedSeries& series, time_t targetUtc, int* averagePrice) {
  long sum = 0;
  int samples = 0;
  time_t hourEnd = targetUtc + 3600;

  for (int pointIndex = 0; pointIndex < series.pointCount; pointIndex++) {
    if (!series.valid[pointIndex]) continue;
    time_t pointUtc = series.startUtc + ((time_t)pointIndex * series.resolutionSeconds);
    if (pointUtc >= targetUtc && pointUtc < hourEnd) {
      sum += series.prices[pointIndex];
      samples++;
    }
  }

  if (samples == 0) return false;
  *averagePrice = (int)((sum + (samples / 2)) / samples);
  return true;
}

static void resetEntsoePrices() {
  PRICES.minimumPrice = 100000;
  PRICES.maximumPrice = 0;
  for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
    PRICES.price[i].isNull = true;
    PRICES.price[i].price = 0;
    PRICES.price[i].level = 3;
    PRICES.price[i].starttime[0] = '\0';
  }
}

static int countDisplayPrices() {
  int count = 0;
  for (int i = 0; i < MATRIX_DISPLAY_HOURS; i++) {
    if (!PRICES.price[i].isNull) count++;
  }
  return count;
}

static String getLocalDatePath(int daysFromNow) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "2000/01/01";
  time_t localTime = mktime(&timeinfo) + ((time_t)daysFromNow * 86400);
  struct tm* shifted = localtime(&localTime);
  char buf[11];
  strftime(buf, sizeof(buf), "%Y/%m/%d", shifted);
  return String(buf);
}

static void readHttpResponseBody(BearSSL::WiFiClientSecure* client, String& response) {
  static const unsigned long idleTimeoutMs = 2500;
  static const unsigned long totalTimeoutMs = 30000;
  static const int maxResponseBytes = 12000;

  uint8_t buffer[256];
  unsigned long startMs = millis();
  unsigned long lastDataMs = millis();

  while ((millis() - startMs) < totalTimeoutMs && response.length() < maxResponseBytes) {
    int availableBytes = client->available();
    if (availableBytes > 0) {
      size_t readLen = availableBytes;
      if (readLen > sizeof(buffer)) readLen = sizeof(buffer);
      if (readLen > (size_t)(maxResponseBytes - response.length())) {
        readLen = maxResponseBytes - response.length();
      }
      size_t len = client->readBytes(buffer, readLen);
      if (len > 0) {
        response.concat((const char*)buffer, len);
        lastDataMs = millis();
      }
      continue;
    }

    if (!client->connected() || (millis() - lastDataMs) > idleTimeoutMs) break;
    delay(10);
  }
}

void printPrices() {
  Serial.println("--- ENTSO-E Prices ---");
  for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
    if (!PRICES.price[i].isNull) {
      Serial.printf("  %s: %i (%i)\n", PRICES.price[i].starttime, PRICES.price[i].price, PRICES.price[i].level);
    } else {
      Serial.printf("  Hour %d: No data\n", i);
    }
  }
  Serial.printf("Min: %i  Max: %i\n", PRICES.minimumPrice, PRICES.maximumPrice);
}

void calculateLevels() {
  int range = PRICES.maximumPrice - PRICES.minimumPrice;
  if (range <= 0) {
    for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
      if (!PRICES.price[i].isNull) {
        PRICES.price[i].level = 3;
      }
    }
    return;
  }
  for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
    if (!PRICES.price[i].isNull) {
      float pct = (float)(PRICES.price[i].price - PRICES.minimumPrice) / range;
      if (pct < 0.2)      PRICES.price[i].level = 1;
      else if (pct < 0.4) PRICES.price[i].level = 2;
      else if (pct < 0.6) PRICES.price[i].level = 3;
      else if (pct < 0.8) PRICES.price[i].level = 4;
      else                 PRICES.price[i].level = 5;
    }
  }
}

void parseEntsoeXml(const String& xmlResponse, const String& requestPeriodStart, bool resetPrices) {
  Serial.println("Parsing ENTSO-E XML response...");
  entsoeLastPointCount = 0;
  entsoeLastExtractedCount = 0;
  entsoeLastSeriesSummary[0] = '\0';
  entsoeLastPointContext[0] = '\0';
  entsoeLastExpectedCheck[0] = '\0';
  if (resetPrices) {
    resetEntsoePrices();
  }
  int currentHour = getHoursOfDay();
  if (currentHour < 0) currentHour = 0;
  Serial.printf("Current hour: %d\n", currentHour);

  (void)requestPeriodStart;
  
  // Count points in first pass
  int pointCount = 0;
  int searchPos = 0;
  while (true) {
    int pos = findTagStart(xmlResponse, "Point", searchPos);
    if (pos == -1) break;
    pointCount++;
    searchPos = pos + 1;
  }
  entsoeLastPointCount = pointCount;
  Serial.printf("Found %d price points\n", pointCount);
  if (pointCount == 0) {
    Serial.println("ERROR: No price points found in XML!");
    return;
  }

  int firstPointForContext = findTagStart(xmlResponse, "Point", 0);
  if (firstPointForContext >= 0) {
    int contextStart = firstPointForContext > 220 ? firstPointForContext - 220 : 0;
    xmlResponse.substring(contextStart, firstPointForContext + 100)
      .toCharArray(entsoeLastPointContext, sizeof(entsoeLastPointContext));
  }
  snprintf(entsoeLastExpectedCheck, sizeof(entsoeLastExpectedCheck),
           "66.56=%d;87.19=%d;107.87=%d;123.19=%d",
           xmlResponse.indexOf("66.56") >= 0,
           xmlResponse.indexOf("87.19") >= 0,
           xmlResponse.indexOf("107.87") >= 0,
           xmlResponse.indexOf("123.19") >= 0);
  
  static const int MAX_SERIES = 8;
  static ParsedSeries seriesList[MAX_SERIES];
  memset(seriesList, 0, sizeof(seriesList));
  int seriesCount = 0;
  entsoeLastExtractedCount = 0;

  String seriesSummary = "";
  int seriesPos = 0;
  int seriesIndex = 0;
  while (seriesCount < MAX_SERIES) {
    int seriesStart = findTagStart(xmlResponse, "TimeSeries", seriesPos);
    if (seriesStart == -1) break;
    int seriesContentStart = findTagContentStart(xmlResponse, seriesStart);
    if (seriesContentStart == -1) break;
    int seriesEnd = findTagEndStart(xmlResponse, "TimeSeries", seriesStart);
    if (seriesEnd == -1) break;

    String classification = extractLocalTagTextBounded(
      xmlResponse,
      "classificationSequence_AttributeInstanceComponent.position",
      seriesContentStart,
      seriesEnd);
    classification.trim();

    int periodPos = seriesContentStart;
    while (seriesCount < MAX_SERIES) {
      int periodTagStart = findTagStart(xmlResponse, "Period", periodPos);
      if (periodTagStart == -1 || periodTagStart > seriesEnd) break;
      int periodContentStart = findTagContentStart(xmlResponse, periodTagStart);
      if (periodContentStart == -1 || periodContentStart > seriesEnd) break;
      int periodEnd = findTagEndStart(xmlResponse, "Period", periodTagStart);
      if (periodEnd == -1 || periodEnd > seriesEnd) break;

      ParsedSeries& current = seriesList[seriesCount];
      current.classification = classification.toInt();

      String startStr = extractLocalTagTextBounded(xmlResponse, "start", periodContentStart, periodEnd);
      startStr.trim();
      struct tm periodStart;
      if (!parseXmlUtcTime(startStr, &periodStart)) {
        periodPos = periodEnd + 9;
        continue;
      }
      current.startUtc = getUtcFromTm(&periodStart);

      String resolution = extractLocalTagTextBounded(xmlResponse, "resolution", periodContentStart, periodEnd);
      resolution.trim();
      if (resolution == "PT15M") current.resolutionSeconds = 900;
      else if (resolution == "PT30M") current.resolutionSeconds = 1800;
      else current.resolutionSeconds = 3600;

      seriesSummary += "S";
      seriesSummary += String(seriesIndex);
      seriesSummary += "(c";
      seriesSummary += String(current.classification);
      seriesSummary += ",";
      seriesSummary += resolution;
      seriesSummary += ":";

      int pointPos = periodContentStart;
      int highestPosition = 0;
      int sampleCount = 0;
      const int maxPoints = (int)(sizeof(current.prices) / sizeof(current.prices[0]));
      while (true) {
        int pointStart = findTagStart(xmlResponse, "Point", pointPos);
        if (pointStart == -1 || pointStart > periodEnd) break;
        int pointEnd = findTagEndStart(xmlResponse, "Point", pointStart);
        if (pointEnd == -1 || pointEnd > periodEnd) break;

        String positionStr = extractLocalTagTextBounded(xmlResponse, "position", pointStart, pointEnd);
        String priceStr = extractLocalTagTextBounded(xmlResponse, "price.amount", pointStart, pointEnd);
        positionStr.trim();
        priceStr.trim();
        int position = positionStr.toInt();
        if (position > 0 && position <= maxPoints && priceStr.length() > 0) {
          float priceFloat = priceStr.toFloat();
          int pointIndex = position - 1;
          current.prices[pointIndex] = (int)(priceFloat * 100);
          current.valid[pointIndex] = true;
          if (position > highestPosition) highestPosition = position;
          if (sampleCount < 4) {
            if (sampleCount > 0) seriesSummary += ",";
            seriesSummary += priceStr;
            sampleCount++;
          }
          entsoeLastExtractedCount++;
        }
        pointPos = pointEnd + 1;
      }

      current.pointCount = highestPosition;

      seriesSummary += ")=";
      seriesSummary += String(current.pointCount);
      seriesSummary += ";";
      Serial.printf("Series %d: classification=%d resolution=%d points=%d\n",
                    seriesIndex, current.classification, current.resolutionSeconds, current.pointCount);

      seriesIndex++;
      if (current.pointCount > 0) seriesCount++;
      periodPos = periodEnd + 1;
    }
    seriesPos = seriesEnd + 1;
  }

  if (seriesCount == 0) {
    int periodPos = 0;
    while (seriesCount < MAX_SERIES) {
      int periodTagStart = findTagStart(xmlResponse, "Period", periodPos);
      if (periodTagStart == -1) break;
      int periodContentStart = findTagContentStart(xmlResponse, periodTagStart);
      if (periodContentStart == -1) break;
      int periodEnd = findTagEndStart(xmlResponse, "Period", periodTagStart);
      if (periodEnd == -1) break;

      ParsedSeries& current = seriesList[seriesCount];
      current.classification = 0;

      String startStr = extractLocalTagTextBounded(xmlResponse, "start", periodContentStart, periodEnd);
      startStr.trim();
      struct tm periodStart;
      if (!parseXmlUtcTime(startStr, &periodStart)) {
        periodPos = periodEnd + 1;
        continue;
      }
      current.startUtc = getUtcFromTm(&periodStart);

      String resolution = extractLocalTagTextBounded(xmlResponse, "resolution", periodContentStart, periodEnd);
      resolution.trim();
      if (resolution == "PT15M") current.resolutionSeconds = 900;
      else if (resolution == "PT30M") current.resolutionSeconds = 1800;
      else current.resolutionSeconds = 3600;

      seriesSummary += "P";
      seriesSummary += String(seriesIndex);
      seriesSummary += "(";
      seriesSummary += resolution;
      seriesSummary += ":";

      int pointPos = periodContentStart;
      int highestPosition = 0;
      int sampleCount = 0;
      const int maxPoints = (int)(sizeof(current.prices) / sizeof(current.prices[0]));
      while (true) {
        int pointStart = findTagStart(xmlResponse, "Point", pointPos);
        if (pointStart == -1 || pointStart > periodEnd) break;
        int pointEnd = findTagEndStart(xmlResponse, "Point", pointStart);
        if (pointEnd == -1 || pointEnd > periodEnd) break;

        String positionStr = extractLocalTagTextBounded(xmlResponse, "position", pointStart, pointEnd);
        String priceStr = extractLocalTagTextBounded(xmlResponse, "price.amount", pointStart, pointEnd);
        positionStr.trim();
        priceStr.trim();
        int position = positionStr.toInt();
        if (position > 0 && position <= maxPoints && priceStr.length() > 0) {
          int pointIndex = position - 1;
          current.prices[pointIndex] = (int)(priceStr.toFloat() * 100);
          current.valid[pointIndex] = true;
          if (position > highestPosition) highestPosition = position;
          if (sampleCount < 4) {
            if (sampleCount > 0) seriesSummary += ",";
            seriesSummary += priceStr;
            sampleCount++;
          }
          entsoeLastExtractedCount++;
        }
        pointPos = pointEnd + 1;
      }

      current.pointCount = highestPosition;
      seriesSummary += ")=";
      seriesSummary += String(current.pointCount);
      seriesSummary += ";";

      seriesIndex++;
      if (current.pointCount > 0) seriesCount++;
      periodPos = periodEnd + 1;
    }
  }

  if (seriesCount == 0) {
    ParsedSeries& current = seriesList[0];
    current.classification = 0;

    int firstPointStart = findTagStart(xmlResponse, "Point", 0);
    int startTag = firstPointStart > 0 ? xmlResponse.lastIndexOf("<start>", firstPointStart) : -1;
    int resolutionTag = firstPointStart > 0 ? xmlResponse.lastIndexOf("<resolution>", firstPointStart) : -1;

    String startStr = startTag >= 0 ? extractBetween(xmlResponse, "<start>", "</start>", startTag) :
                                      extractLocalTagTextBounded(xmlResponse, "start", 0, firstPointStart > 0 ? firstPointStart : xmlResponse.length());
    startStr.trim();
    struct tm periodStart;
    if (parseXmlUtcTime(startStr, &periodStart)) {
      current.startUtc = getUtcFromTm(&periodStart);

      String resolution = resolutionTag >= 0 ? extractBetween(xmlResponse, "<resolution>", "</resolution>", resolutionTag) :
                                               extractLocalTagTextBounded(xmlResponse, "resolution", 0, firstPointStart > 0 ? firstPointStart : xmlResponse.length());
      resolution.trim();
      if (resolution == "PT15M") current.resolutionSeconds = 900;
      else if (resolution == "PT30M") current.resolutionSeconds = 1800;
      else current.resolutionSeconds = 3600;

      seriesSummary += "G(";
      seriesSummary += resolution;
      seriesSummary += ":";

      int pointPos = 0;
      int highestPosition = 0;
      int sampleCount = 0;
      const int maxPoints = (int)(sizeof(current.prices) / sizeof(current.prices[0]));
      while (true) {
        int pointStart = findTagStart(xmlResponse, "Point", pointPos);
        if (pointStart == -1) break;
        int pointEnd = findTagEndStart(xmlResponse, "Point", pointStart);
        if (pointEnd == -1) break;

        String positionStr = extractLocalTagTextBounded(xmlResponse, "position", pointStart, pointEnd);
        String priceStr = extractLocalTagTextBounded(xmlResponse, "price.amount", pointStart, pointEnd);
        positionStr.trim();
        priceStr.trim();
        int position = positionStr.toInt();
        if (position > 0 && position <= maxPoints && priceStr.length() > 0) {
          int pointIndex = position - 1;
          current.prices[pointIndex] = (int)(priceStr.toFloat() * 100);
          current.valid[pointIndex] = true;
          if (position > highestPosition) highestPosition = position;
          if (sampleCount < 4) {
            if (sampleCount > 0) seriesSummary += ",";
            seriesSummary += priceStr;
            sampleCount++;
          }
          entsoeLastExtractedCount++;
        }
        pointPos = pointEnd + 1;
      }

      current.pointCount = highestPosition;
      seriesSummary += ")=";
      seriesSummary += String(current.pointCount);
      seriesSummary += ";";
      if (current.pointCount > 0) seriesCount = 1;
    }
  }
  seriesSummary.toCharArray(entsoeLastSeriesSummary, sizeof(entsoeLastSeriesSummary));
  
  if (seriesCount == 0 || entsoeLastExtractedCount == 0) {
    Serial.println("ERROR: Could not extract any prices!");
    return;
  }

  time_t targetUtc[ENTSOE_PRICE_HOURS];
  char labels[ENTSOE_PRICE_HOURS][14];
  if (!getTargetUtc(targetUtc, labels, ENTSOE_PRICE_HOURS)) {
    Serial.println("ERROR: Local time unavailable for price alignment");
    return;
  }
  
  for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
    int selectedPrice = 0;
    int selectedPriority = 99;
    bool found = false;

    for (int s = 0; s < seriesCount; s++) {
      ParsedSeries& current = seriesList[s];
      int averagedPrice = 0;
      if (!averageSeriesHour(current, targetUtc[i], &averagedPrice)) continue;

      int priority = getSeriesPriority(current.classification);
      if (!found || priority < selectedPriority) {
        selectedPrice = averagedPrice;
        selectedPriority = priority;
        found = true;
      }
    }

    if (!found) continue;

    PRICES.price[i].isNull = false;
    PRICES.price[i].price = selectedPrice;
    PRICES.price[i].level = 3;
    strncpy(PRICES.price[i].starttime, labels[i], sizeof(PRICES.price[i].starttime) - 1);
    PRICES.price[i].starttime[sizeof(PRICES.price[i].starttime) - 1] = '\0';

    if (selectedPrice < PRICES.minimumPrice) PRICES.minimumPrice = selectedPrice;
    if (selectedPrice > PRICES.maximumPrice) PRICES.maximumPrice = selectedPrice;
  }
  calculateLevels();
  printPrices();
}

String getDateString(int daysFromNow) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "20000101";
  }
  time_t t = mktime(&timeinfo) + (daysFromNow * 86400);
  struct tm* newTime = gmtime(&t);
  char buf[9];
  strftime(buf, sizeof(buf), "%Y%m%d", newTime);
  return String(buf);
}

String getLocalHourUtcString(int hoursFromNow) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "200001010000";
  }
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t t = mktime(&timeinfo) + ((time_t)hoursFromNow * 3600);
  struct tm* utcTime = gmtime(&t);
  char buf[13];
  strftime(buf, sizeof(buf), "%Y%m%d%H%M", utcTime);
  return String(buf);
}

String getLocalMidnightUtcString(int daysFromNow) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "200001010000";
  }
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t t = mktime(&timeinfo) + ((time_t)daysFromNow * 86400);
  struct tm* utcTime = gmtime(&t);
  char buf[13];
  strftime(buf, sizeof(buf), "%Y%m%d%H%M", utcTime);
  return String(buf);
}

static bool fetchEntsoeWindow(const String& periodStart, const String& periodEnd, bool resetPrices) {
  if (resetPrices) resetEntsoePrices();
  periodStart.toCharArray(entsoeLastPeriodStart, sizeof(entsoeLastPeriodStart));
  periodEnd.toCharArray(entsoeLastPeriodEnd, sizeof(entsoeLastPeriodEnd));

  const char* apiUrls[] = {entsoeApi, entsoeApiFallback};
  bool fetched = false;

  for (uint8_t apiIndex = 0; apiIndex < (sizeof(apiUrls) / sizeof(apiUrls[0])); apiIndex++) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    HTTPClient https;
    https.useHTTP10(true);

    String url = String(apiUrls[apiIndex]) +
                 "?securityToken=" + getApiKey() +
                 "&documentType=A44" +
                 "&contract_MarketAgreement.type=A01" +
                 "&in_Domain=" + getBiddingZone() +
                 "&out_Domain=" + getBiddingZone() +
                 "&periodStart=" + periodStart +
                 "&periodEnd=" + periodEnd;

    Serial.print("URL: ");
    Serial.println(url);

    Serial.print("[HTTPS] GET...\n");
    if (https.begin(*client, url)) {
      https.addHeader("Accept", "application/xml");

      int httpCode = https.GET();
      entsoeLastHttpCode = httpCode;
      Serial.printf("HTTP Response code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK) {
        int totalSize = https.getSize();
        Serial.printf("Total response size: %d bytes\n", totalSize);
        String response;
        response.reserve(12000);
        readHttpResponseBody(client.get(), response);

        Serial.printf("Response length: %d bytes\n", response.length());
        entsoeLastResponseLength = response.length();
        response.substring(0, 240).toCharArray(entsoeLastPreview, sizeof(entsoeLastPreview));

        if (response.length() > 0) {
          Serial.print("Response preview: ");
          Serial.println(response.substring(0, 200));
          parseEntsoeXml(response, periodStart, false);
          fetched = entsoeLastExtractedCount >= 32;
          if (!fetched) {
            Serial.println("ERROR: ENTSO-E response did not contain enough price points");
          }
        } else {
          Serial.println("ERROR: Response is empty!");
        }
      } else {
        Serial.printf("HTTP Error: %s\n", https.errorToString(httpCode).c_str());
        if (httpCode == HTTP_CODE_UNAUTHORIZED) {
          https.end();
          break;
        }
      }

      https.end();
    } else {
      Serial.println("[HTTPS] Unable to connect to ENTSO-E API");
    }

    if (fetched) break;
    Serial.println("Trying next ENTSO-E API endpoint...");
  }

  return fetched;
}

static bool appendSpotDayToPrices(const String& datePath, bool resetPrices) {
  WiFiClient client;
  HTTPClient http;

  String url = "http://spot.utilitarian.io/electricity/NL/";
  url += datePath;
  url += "/";

  Serial.print("Spot HTTP fallback URL: ");
  Serial.println(url);

  if (!http.begin(client, url)) {
    Serial.println("Spot fallback: unable to connect");
    return false;
  }

  http.setTimeout(15000);
  int httpCode = http.GET();
  entsoeLastHttpCode = httpCode;
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Spot fallback HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  entsoeLastResponseLength = response.length();
  response.substring(0, 240).toCharArray(entsoeLastPreview, sizeof(entsoeLastPreview));
  if (response.length() == 0) return false;

  time_t targetUtc[ENTSOE_PRICE_HOURS];
  char labels[ENTSOE_PRICE_HOURS][14];
  if (!getTargetUtc(targetUtc, labels, ENTSOE_PRICE_HOURS)) return false;

  long sums[ENTSOE_PRICE_HOURS];
  uint8_t samples[ENTSOE_PRICE_HOURS];
  memset(sums, 0, sizeof(sums));
  memset(samples, 0, sizeof(samples));

  int pos = 0;
  int records = 0;
  while (true) {
    int timestampMarker = response.indexOf("\"timestamp\": \"", pos);
    if (timestampMarker == -1) break;
    String timestamp = extractBetween(response, "\"timestamp\": \"", "\"", timestampMarker);

    int valueMarker = response.indexOf("\"value\": \"", timestampMarker);
    if (valueMarker == -1) break;
    String value = extractBetween(response, "\"value\": \"", "\"", valueMarker);
    pos = valueMarker + 10;

    struct tm utcTm;
    if (!parseXmlUtcTime(timestamp, &utcTm)) continue;
    time_t pointUtc = getUtcFromTm(&utcTm);
    int price = (int)(value.toFloat() * 100);

    for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
      if (pointUtc >= targetUtc[i] && pointUtc < targetUtc[i] + 3600) {
        sums[i] += price;
        if (samples[i] < 255) samples[i]++;
        break;
      }
    }
    records++;
  }

  if (resetPrices) resetEntsoePrices();

  int extracted = 0;
  for (int i = 0; i < ENTSOE_PRICE_HOURS; i++) {
    if (samples[i] == 0) continue;
    int averagePrice = (int)((sums[i] + (samples[i] / 2)) / samples[i]);
    PRICES.price[i].isNull = false;
    PRICES.price[i].price = averagePrice;
    PRICES.price[i].level = 3;
    strncpy(PRICES.price[i].starttime, labels[i], sizeof(PRICES.price[i].starttime) - 1);
    PRICES.price[i].starttime[sizeof(PRICES.price[i].starttime) - 1] = '\0';

    if (averagePrice < PRICES.minimumPrice) PRICES.minimumPrice = averagePrice;
    if (averagePrice > PRICES.maximumPrice) PRICES.maximumPrice = averagePrice;
    extracted++;
  }

  entsoeLastPointCount = records;
  entsoeLastExtractedCount = extracted;
  snprintf(entsoeLastSeriesSummary, sizeof(entsoeLastSeriesSummary),
           "SpotHTTP(%s):records=%d;hours=%d", datePath.c_str(), records, extracted);

  calculateLevels();
  return extracted > 0;
}

static bool fetchSpotRollingPrices() {
  bool fetched = appendSpotDayToPrices(getLocalDatePath(0), true);
  appendSpotDayToPrices(getLocalDatePath(1), !fetched);
  printPrices();
  return countDisplayPrices() == MATRIX_DISPLAY_HOURS;
}

void getEntsoePrices() {
  Serial.println("\n--- Fetching ENTSO-E Prices ---");

  fetchEntsoeWindow(getLocalHourUtcString(-2), getLocalHourUtcString(10), true);
  if (countDisplayPrices() < MATRIX_DISPLAY_HOURS) {
    Serial.println("ENTSO-E XML did not fill rolling display window, using Spot HTTP fallback");
    fetchSpotRollingPrices();
  }

  Serial.println("--- ENTSO-E Fetch Complete ---\n");
}



