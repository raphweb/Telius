#include <Arduino.h>

#include <Preferences.h>
#include <DisplayConfig.hpp>
#include <WiFi.h>
#include "esp_wpa2.h"
#include "time.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <gason.h>
#include <base64.h>
#include <vector>

#define US_TO_SECONDS_FACTOR 1000000L
#define SLEEP_DURATION 60 // in seconds

#define ESP_NAME "telius"

/**************************** DEBUG *******************************/
#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(m) Serial.println(m)
#define DEBUG_PRINT(m) Serial.print(m)

#else
#define DEBUG_PRINTLN(m)
#define DEBUG_PRINT(m)

#define DEBUG_PRINTLNC(m)
#define DEBUG_PRINTC(m)
#endif

enum AlignmentKind {
  LEFT          = 1,
  CENTER        = 2,
  RIGHT         = 4,
  BOTTOM        = 8,
  MIDDLE        = 16,
  TOP           = 32,
  BOTTOM_LEFT   = BOTTOM | LEFT,
  MIDDLE_LEFT   = MIDDLE | LEFT,
  TOP_LEFT      = TOP    | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER,
  MIDDLE_CENTER = MIDDLE | CENTER,
  TOP_CENTER    = TOP    | CENTER,
  BOTTOM_RIGHT  = BOTTOM | RIGHT,
  MIDDLE_RIGHT  = MIDDLE | RIGHT,
  TOP_RIGHT     = TOP    | RIGHT
};

Preferences preferences;
HTTPClient https;

char currMonday[9] = "";
char nextFriday[9] = "";
uint32_t currentMonday = 0; // used to store the date of the monday of the current week in YYYYmmdd
uint64_t secondsTillNextWakeup = 0;
#define WAKEUP_HOUR 6

// This is lets-encrypt-r11.pem, the intermediate Certificate Authority that
// signed the server certifcate for the WebUntis server https://webuntis.com
// used here. This certificate is valid until 2027-03-12
// See: https://letsencrypt.org/certificates/
const char* rootCACertificate PROGMEM = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAw\n" \
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n" \
  "WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
  "RW5jcnlwdDEMMAoGA1UEAxMDUjExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n" \
  "CgKCAQEAuoe8XBsAOcvKCs3UZxD5ATylTqVhyybKUvsVAbe5KPUoHu0nsyQYOWcJ\n" \
  "DAjs4DqwO3cOvfPlOVRBDE6uQdaZdN5R2+97/1i9qLcT9t4x1fJyyXJqC4N0lZxG\n" \
  "AGQUmfOx2SLZzaiSqhwmej/+71gFewiVgdtxD4774zEJuwm+UE1fj5F2PVqdnoPy\n" \
  "6cRms+EGZkNIGIBloDcYmpuEMpexsr3E+BUAnSeI++JjF5ZsmydnS8TbKF5pwnnw\n" \
  "SVzgJFDhxLyhBax7QG0AtMJBP6dYuC/FXJuluwme8f7rsIU5/agK70XEeOtlKsLP\n" \
  "Xzze41xNG/cLJyuqC0J3U095ah2H2QIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB\n" \
  "hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB\n" \
  "/wIBADAdBgNVHQ4EFgQUxc9GpOr0w8B6bJXELbBeki8m47kwHwYDVR0jBBgwFoAU\n" \
  "ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC\n" \
  "hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG\n" \
  "A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN\n" \
  "AQELBQADggIBAE7iiV0KAxyQOND1H/lxXPjDj7I3iHpvsCUf7b632IYGjukJhM1y\n" \
  "v4Hz/MrPU0jtvfZpQtSlET41yBOykh0FX+ou1Nj4ScOt9ZmWnO8m2OG0JAtIIE38\n" \
  "01S0qcYhyOE2G/93ZCkXufBL713qzXnQv5C/viOykNpKqUgxdKlEC+Hi9i2DcaR1\n" \
  "e9KUwQUZRhy5j/PEdEglKg3l9dtD4tuTm7kZtB8v32oOjzHTYw+7KdzdZiw/sBtn\n" \
  "UfhBPORNuay4pJxmY/WrhSMdzFO2q3Gu3MUBcdo27goYKjL9CTF8j/Zz55yctUoV\n" \
  "aneCWs/ajUX+HypkBTA+c8LGDLnWO2NKq0YD/pnARkAnYGPfUDoHR9gVSp/qRx+Z\n" \
  "WghiDLZsMwhN1zjtSC0uBWiugF3vTNzYIEFfaPG7Ws3jDrAMMYebQ95JQ+HIBD/R\n" \
  "PBuHRTBpqKlyDnkSHDHYPiNX3adPoPAcgdF3H2/W0rmoswMWgTlLn1Wu0mrks7/q\n" \
  "pdWfS6PJ1jty80r2VKsM/Dj3YIDfbjXKdaFU5C+8bhfJGqU3taKauuz0wHVGT3eo\n" \
  "6FlWkWYtbt4pgdamlwVeZEW+LM7qZEJEsMNPrfC03APKmZsJgpWCDWOKZvkZcvjV\n" \
  "uYkQ4omYCTX5ohy+knMjdOmdH9c7SpqEWBDC86fiNex+O0XOMEZSa8DA\n" \
  "-----END CERTIFICATE-----\n";

void fillDateWeekDays(time_t currentDay);

void setClock() {
  // Europe/Berlin timezone
  // see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  const String timezone = preferences.getString("TIMEZONE");
  configTzTime(timezone.c_str(), "pool.ntp.org");

  DEBUG_PRINT("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 3600) { // maximum wait for an hour
    delay(500);
    DEBUG_PRINT(".");
    yield();
    nowSecs = time(nullptr);
  }

  DEBUG_PRINTLN();
  struct tm timeinfo;
  localtime_r(&nowSecs, &timeinfo);
  DEBUG_PRINT("Current time: ");
  DEBUG_PRINT(asctime(&timeinfo));

  const uint8_t daysTillFriday = (12 - timeinfo.tm_wday) % 7;
  time_t nTimeSecs = nowSecs + 86400 * daysTillFriday;
  struct tm nTime;
  localtime_r(&nTimeSecs, &nTime);
  DEBUG_PRINT("Next Friday: ");
  strftime(nextFriday, 9, "%Y%m%d", &nTime);
  DEBUG_PRINTLN(nextFriday);
  nTimeSecs = nowSecs + 86400 * (daysTillFriday - 4);
  localtime_r(&nTimeSecs, &nTime);
  DEBUG_PRINT("Current week Monday: ");
  strftime(currMonday, 9, "%Y%m%d", &nTime);
  DEBUG_PRINTLN(currMonday);
  currentMonday = strtoul(currMonday, NULL, 10);
  fillDateWeekDays(nTimeSecs);

  const uint8_t hoursTillWakeup = (timeinfo.tm_hour < WAKEUP_HOUR) ? WAKEUP_HOUR - timeinfo.tm_hour : 24 + WAKEUP_HOUR - timeinfo.tm_hour;
  secondsTillNextWakeup = 3600 * hoursTillWakeup;
  time_t wTimeSecs = nowSecs + secondsTillNextWakeup;
  struct tm wTime;
  localtime_r(&wTimeSecs, &wTime);
  // the wakeup time will drift over time, but will always happen some time between WAKEUP_HOUR and WAKEUP_HOUR+1
  DEBUG_PRINT("Next wakeup will be scheduled at: ");
  DEBUG_PRINTLN(asctime(&wTime));
}

#define LOHA(n) (n&15) // lower half of a byte
#define UPHA(n) (n>>4) // upper half of a byte

struct SessionData {
  const uint16_t klasseId;
  const uint16_t personId;
  const String sessionCookie;
  const uint8_t personType;
};

struct TTRowHeaderCell {
  const uint16_t time1;
  const uint16_t time2;
  const uint16_t offset;
};

struct TTLesson {
  String name;
  String teacher;
  String teacherName;
  String room;
  // MSB .... LSB
  // 4 bits start lesson | 4 bits end lesson
  uint8_t lesson = 0;
  // 4 bits day | 4 bits flags:
  //                3: homework
  //                2: period changed
  //                1: teacher changed
  //                0: room changed
  uint8_t dayAndFlags = 0;
};

#define getTimeStrForTime(timeNumber) \
  { \
    (char)(timeNumber / 1000u + '0'), \
    (char)(timeNumber / 100u % 10u + '0'), \
    ':', \
    (char)(timeNumber % 100u / 10u + '0'), \
    (char)(timeNumber % 10u + '0'), \
    0 \
  }

struct TimeTable {
  const String days[7] = {
    "Montag",
    "Dienstag",
    "Mittwoch",
    "Donnerstag",
    "Freitag",
    "Samstag",
    "Sonntag"
  };
  uint8_t maxDays = 0;
  uint8_t maxPeriods = 0;
  uint32_t dateWeekDay[5] = {0};
  std::vector<TTRowHeaderCell> *periods = new std::vector<TTRowHeaderCell>();
  std::vector<TTLesson> *lessonGrid = new std::vector<TTLesson>();
  uint32_t lastUpdateDay = 0; // used to store the day of the last update in YYYYmmdd, e.g., 20230113
};

TimeTable timeTable;

struct Homework {
  const uint16_t id;
  String text;
  String teacher;
  uint16_t teacher_id;
  String subject;
  uint16_t subject_id;
  uint8_t day;
};

bool checkAndMergeExistingLesson(TTLesson &newLesson) {
  for(auto &exLesson:*timeTable.lessonGrid) {
    if (exLesson.name == newLesson.name &&
        exLesson.teacher == newLesson.teacher &&
        exLesson.room == newLesson.room &&
        exLesson.dayAndFlags == newLesson.dayAndFlags) {
      if (timeTable.periods->at(LOHA(exLesson.lesson)).time2 == timeTable.periods->at(UPHA(newLesson.lesson)).time1) {
        exLesson.lesson = (exLesson.lesson & 0xF0) ^ (newLesson.lesson & 0x0F);
      } else if (timeTable.periods->at(UPHA(exLesson.lesson)).time1 == timeTable.periods->at(LOHA(newLesson.lesson)).time2) {
        exLesson.lesson = (exLesson.lesson & 0x0F) ^ (newLesson.lesson & 0xF0);
      }
      //DEBUG_PRINTLN(String("  Start of (ex, new): ") + String(LOHA(exLesson.lesson)) + ", " + String(LOHA(newLesson.lesson)));
      //DEBUG_PRINTLN(String("  End   of (ex, new): ") + String(UPHA(exLesson.lesson)) + ", " + String(LOHA(newLesson.lesson)));
      return true;
    }
  }
  return false;
}

void fillDateWeekDays(const time_t mondayDate) {
  struct tm nTime;
  time_t currentDay = mondayDate;
  for(uint8_t i = 0; i < 5; i++) {
    currentDay = mondayDate + 86400 * i;
    localtime_r(&currentDay, &nTime);
    char currentDayStr[9];
    strftime(currentDayStr, 9, "%Y%m%d", &nTime);
    uint32_t currentDayUInt32 = strtoul(currentDayStr, NULL, 10);
    timeTable.dateWeekDay[i] = currentDayUInt32;
    DEBUG_PRINTLN("Date " + String(currentDayUInt32) + " is " + String(i) + " day of the week.");
  }
}

uint8_t getNumDayOfWeekFromDate(uint32_t date) {
  for(uint8_t numDay = 0; numDay < 5; numDay++)
    if (timeTable.dateWeekDay[numDay] == date) return numDay;
  return 0;
}

uint8_t getPeriodNumberForStartTime(uint16_t startTime) {
  uint8_t ret = 0;
  for(const auto& period : *timeTable.periods) {
    if (period.time1 == startTime) return ret;
    ret++;
  }
  return 0;
}

uint8_t getPeriodNumberForEndTime(uint16_t endTime) {
  uint8_t ret = 0;
  for(const auto& period : *timeTable.periods) {
    if (period.time2 == endTime) return ret;
    ret++;
  }
  return 0;
}

#define rowHeight 34
#define coluWidth 144

const uint16_t emphBG = GxEPD_ORANGE;
const String hwIcon = "H";

void drawString(int16_t x, int16_t y, const String &text, const AlignmentKind alignment = BOTTOM_LEFT, const bool emph = false) {
  const uint16_t w = u8g2Fonts.getUTF8Width(text.c_str());
  const uint16_t h = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
  if (alignment & RIGHT)  x -= w;
  if (alignment & CENTER) x -= w / 2;
  if (alignment & MIDDLE) y += h / 2;
  if (alignment & TOP)    y += h;
  if (emph) {
    display.fillRect(x, y-u8g2Fonts.getFontAscent(), w, h, emphBG);
  }
  u8g2Fonts.drawUTF8(x, y, text.c_str());
}

void drawView1() {
  display.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  for(uint8_t cDay = 0; cDay < timeTable.maxDays; cDay++) {
    display.drawFastVLine(79 + cDay*coluWidth, 6, 35, GxEPD_BLACK);
    drawString(79 + coluWidth/2 + cDay*coluWidth, 33, timeTable.days[cDay], BOTTOM_CENTER);
  }
  for(uint8_t cPeriod = 0; cPeriod < timeTable.maxPeriods; cPeriod++) {
    const auto& row = timeTable.periods->at(cPeriod);
    display.drawFastHLine(0, 10 + row.offset, 79, GxEPD_BLACK);
    const char time1[] = getTimeStrForTime(row.time1);
    drawString(38, 35 + row.offset, time1, BOTTOM_CENTER);
    display.drawFastHLine(15, 10 + rowHeight + row.offset, 40, GxEPD_BLACK);
    const char time2[] = getTimeStrForTime(row.time2);
    drawString(38, 35 + row.offset + rowHeight, time2, BOTTOM_CENTER);
    display.drawFastHLine(0, 10 + rowHeight*2 + row.offset, 79, GxEPD_BLACK);
  }
  // fill time table
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  for(const auto& lesson : *timeTable.lessonGrid) {
    const uint16_t columnOffset = 79 + UPHA(lesson.dayAndFlags)*coluWidth;
    const uint16_t rowStartOffset = timeTable.periods->at(UPHA(lesson.lesson)).offset;
    const uint16_t lessonHeight = timeTable.periods->at(LOHA(lesson.lesson)).offset - rowStartOffset + 2*rowHeight;
    display.fillRect(columnOffset, 10 + rowStartOffset, coluWidth + 1, lessonHeight + 1, (lesson.dayAndFlags&7) == 7 ? GxEPD_ORANGE : GxEPD_YELLOW);
    display.drawRect(columnOffset, 10 + rowStartOffset, coluWidth + 1, lessonHeight + 1, GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor((lesson.dayAndFlags&4) == 4 ? GxEPD_ORANGE : GxEPD_YELLOW);
    drawString(columnOffset +             4, 34 + rowStartOffset,    lesson.name, BOTTOM_LEFT, (lesson.dayAndFlags&4) == 4);
    u8g2Fonts.setBackgroundColor((lesson.dayAndFlags&1) == 1 ? GxEPD_ORANGE : GxEPD_YELLOW);
    drawString(columnOffset + coluWidth - 4, 34 + rowStartOffset,    lesson.room, BOTTOM_RIGHT, (lesson.dayAndFlags&1) == 1);
    u8g2Fonts.setBackgroundColor((lesson.dayAndFlags&2) == 2 ? GxEPD_ORANGE : GxEPD_YELLOW);
    drawString(columnOffset +             4, 34 + rowStartOffset + rowHeight, lesson.teacherName, BOTTOM_LEFT, (lesson.dayAndFlags&2) == 2);
    if (lesson.dayAndFlags&8) {
      drawString(columnOffset + coluWidth - 4, 34 + rowStartOffset + rowHeight, hwIcon, BOTTOM_RIGHT, false);
    }
  }
}

void fillWithColor(uint16_t color) {
  display.firstPage();
  do {
    display.fillScreen(color);
  } while (display.nextPage());
}

void clearAllColors() {
  /*fillWithColor(GxEPD_BLACK);
  fillWithColor(GxEPD_GREEN);
  fillWithColor(GxEPD_BLUE);
  fillWithColor(GxEPD_RED);
  fillWithColor(GxEPD_YELLOW);
  fillWithColor(GxEPD_ORANGE);*/
  delay(1000);
}

SessionData *currentSession = nullptr;

bool webUntisLogin(WiFiClientSecure &client) {
  const String school = preferences.getString("WU_SCHOOL");
  const String subdom = preferences.getString("WU_SUBDOM");
  const String user = preferences.getString("WU_USER");
  const String pass = preferences.getString("WU_PASS");
  //DEBUG_PRINTLN("Loaded WebUntis school: " + school + ", user: " + user + ", pass: " + pass);

  if (https.begin(client, "https://" + subdom + ".webuntis.com/WebUntis/jsonrpc.do?school=" + school)) {
    int httpCode = https.POST("{\"id\":\"" + String(ESP_NAME) + "243\",\"method\":\"authenticate\",\"params\":{\"user\":\""
        + user + "\",\"password\":\"" + pass + "\",\"client\":\"" + String(ESP_NAME) + "243\"},\"jsonrpc\":\"2.0\"}");

    if (httpCode > 0) {
      DEBUG_PRINTLN("[HTTPS] POST authenticate... code: " + String(httpCode));

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        DEBUG_PRINTLN(payload);
        char *source = payload.begin();
        char *endptr;
        JsonValue value;
        JsonAllocator allocator;
        int status = jsonParse(source, &endptr, &value, allocator);
        if (status == JSON_OK) {
          uint16_t klasseId = 0;
          uint16_t personId = 0;
          String *sessionId = nullptr;
          uint8_t personType = 0;
          if (value.getTag() == JSON_OBJECT) {
            for(auto r:value) {
              if (strcmp(r->key, "result") == 0 && r->value.getTag() == JSON_OBJECT) {
                for(auto v:r->value) {
                  if (strcmp(v->key, "sessionId") == 0 && v->value.getTag() == JSON_STRING) {
                    sessionId = new String(v->value.toString());
                  } else if (strcmp(v->key, "personId") == 0 && v->value.getTag() == JSON_NUMBER) {
                    personId = v->value.toNumber();
                  } else if (strcmp(v->key, "klasseId") == 0 && v->value.getTag() == JSON_NUMBER) {
                    klasseId = v->value.toNumber();
                  } else if (strcmp(v->key, "personType") == 0 && v->value.getTag() == JSON_NUMBER) {
                    personType = v->value.toNumber();
                  }
                }
              }
            }
            if (sessionId) {
              currentSession = new SessionData{klasseId, personId,
                  String("JSESSIONID=" + *sessionId + "; schoolname=_" + base64::encode(school)), personType};
              DEBUG_PRINTLN("Using session id: " + currentSession->sessionCookie);
              https.end();
              return true;
            } else {
              https.end();
              return false;
            }
          }
        } else {
          DEBUG_PRINTLN(String(jsonStrError(status)) + " at " + String(endptr - source));
          https.end();
          return false;
        }
        allocator.deallocate();
      }
    } else {
      DEBUG_PRINTLN("[HTTPS] authenticate... failed, error: " + https.errorToString(httpCode));
      https.end();
      return false;
    }
  }
  https.end();
  return false;
}

String webUntisRequest(WiFiClientSecure &client, String method, String params = "") {
  if (currentSession) {
    const String school = preferences.getString("WU_SCHOOL");
    const String subdom = preferences.getString("WU_SUBDOM");
    if (https.begin(client, "https://" + subdom + ".webuntis.com/WebUntis/jsonrpc.do?school=" + school)) {
      https.addHeader("Cookie", currentSession->sessionCookie);
      int httpCode = https.POST("{\"id\":\"" + String(ESP_NAME) + "243\",\"method\":\"" + method + "\",\"params\":{" + params + "},\"jsonrpc\":\"2.0\"}");
      if (httpCode > 0) {
        DEBUG_PRINTLN("[HTTPS] POST " + method + "... code: " + httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          DEBUG_PRINTLN(payload);
          https.end();
          return payload;
        }
      } else {
        DEBUG_PRINTLN("[HTTPS] POST " + method + "... failed, error: " + https.errorToString(httpCode));
      }
      https.end();
    }
  }
  return "";
}

String webUntisGetHomework(WiFiClientSecure &client) {
  if (currentSession) {
    const String subdom = preferences.getString("WU_SUBDOM");
    if (https.begin(client, "https://" + subdom + ".webuntis.com/WebUntis/api/homeworks/lessons?startDate=" + currMonday + "&endDate=" + nextFriday)) {
      https.addHeader("Cookie", currentSession->sessionCookie);
      int httpCode = https.GET();
      if (httpCode > 0) {
        DEBUG_PRINTLN("[HTTPS] GET homework... code: " + httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          DEBUG_PRINTLN(payload);
          https.end();
          return payload;
        }
      } else {
        DEBUG_PRINTLN("[HTTPS] GET homework... failed, error: " + https.errorToString(httpCode));
      }
      https.end();
    }
  }
  return "";
}

String webUntisTimegrid(WiFiClientSecure &client) {
  return webUntisRequest(client, "getTimegridUnits");
}

String webUntisTimetable(WiFiClientSecure &client) {
  if (currentSession) {
    return webUntisRequest(client, "getTimetable", "\"options\":{\"element\":{\"id\":\"" + String(currentSession->personId) + "\",\"type\":" +
      String(currentSession->personType) + "},\"startDate\":" + currMonday + ",\"endDate\":" + nextFriday + ",\"showLsText\":true,\"klasseFields\":[\"name\"]," +
      "\"teacherFields\":[\"name\",\"longname\"],\"subjectFields\":[\"name\"],\"roomFields\":[\"name\"]}");
  }
  return "";
}

void webUntisLogout(WiFiClientSecure &client) {
  webUntisRequest(client, "logout");
  currentSession = nullptr;
}

String *getEntity(JsonValue &v, bool &changed, const char *entityName = "name") {
  String *ret = nullptr;
  if (v.getTag() == JSON_ARRAY) {
    for(auto ae:v) {
      if (ae->value.getTag() == JSON_OBJECT) {
        for(auto f:ae->value) {
          if (strcmp(f->key, entityName) == 0 && f->value.getTag() == JSON_STRING) {
            ret = new String(f->value.toString());
            const int ind = ret->indexOf('_');
            if (ind > -1) {
              ret->remove(ind);
            }
          } else if (strcmp(f->key, String(String("org") + entityName).c_str()) == 0) {
            changed = true;
          }
        }
      }
    }
  }
  return ret;
}

bool updateTimeUnits(char *source) {
  bool ret = false;
  char *endptr;
  JsonValue value;
  JsonAllocator allocator;
  int status = jsonParse(source, &endptr, &value, allocator);
  if (status == JSON_OK) {
    uint16_t startTime = UINT16_MAX, endTime = UINT16_MAX;
    uint16_t offset = rowHeight;
    if (value.getTag() == JSON_OBJECT) {
      for(auto r:value) {
        if (strcmp(r->key, "result") == 0 && r->value.getTag() == JSON_ARRAY) {
          uint8_t dayCount = 0;
          for(auto v:r->value) {
            if (v->value.getTag() == JSON_OBJECT) {
              if (dayCount == 0) {
                // assuming that all other days have the same period times
                for(auto p:v->value) {
                  if (strcmp(p->key, "timeUnits") == 0 && p->value.getTag() == JSON_ARRAY) {
                    uint16_t lastEndTime = UINT16_MAX;
                    for(auto t:p->value) {
                      if (t->value.getTag() == JSON_OBJECT) {
                        for(auto l:t->value) {
                          if (strcmp(l->key, "startTime") == 0 && l->value.getTag() == JSON_NUMBER) {
                            startTime = l->value.toNumber();
                          } else if (strcmp(l->key, "endTime") == 0 && l->value.getTag() == JSON_NUMBER) {
                            endTime = l->value.toNumber();
                          }
                        }
                      }
                      if (startTime < UINT16_MAX && endTime < UINT16_MAX) {
                        if (lastEndTime < UINT16_MAX && lastEndTime + 5 < startTime) {
                          // break with more than 5 minutes duration
                          offset += rowHeight/3;
                        }
                        TTRowHeaderCell period = {startTime, endTime, offset};
                        timeTable.periods->push_back(period);
                        offset += 2 * rowHeight;
                        lastEndTime = endTime;
                        ret = true;
                      }
                    }
                  }
                }
              }
              dayCount++;
            }
          }
          if (timeTable.maxDays == 0) {
            timeTable.maxDays = dayCount;
          }
        }
      }
    }
  } else {
    DEBUG_PRINTLN(String(jsonStrError(status)) + " at " + String(endptr - source));
    return false;
  }
  allocator.deallocate();
  return ret;
}

bool updateTimeTable(char *source) {
  bool ret = false;
  char *endptr;
  JsonValue value;
  JsonAllocator allocator;
  int status = jsonParse(source, &endptr, &value, allocator);
  if (status == JSON_OK) {
    uint8_t cDay, cStartPeriod, cEndPeriod;
    String *periodName, *teacherName, *teacherLastname, *roomName, *lstext;
    bool periodChanged, teacherChanged, roomChanged;
    if (value.getTag() == JSON_OBJECT) {
      for(auto r:value) {
        if (strcmp(r->key, "result") == 0 && r->value.getTag() == JSON_ARRAY) {
          for(auto v:r->value) {
            if (v->value.getTag() == JSON_OBJECT) {
              cDay = UINT8_MAX;
              cStartPeriod = UINT8_MAX;
              cEndPeriod = UINT8_MAX;
              periodName = teacherName = teacherLastname = roomName = lstext = nullptr;
              periodChanged = teacherChanged = roomChanged = false;
              bool irregular = false;
              for(auto p:v->value) {
                if (strcmp(p->key, "code") == 0 && p->value.getTag() == JSON_STRING) {
                  if (strcmp(p->value.toString(), "cancelled") == 0) {
                    cStartPeriod = UINT8_MAX;
                    cEndPeriod = UINT8_MAX;
                    break;
                  } else if (strcmp(p->value.toString(), "irregular") == 0) {
                    irregular = true;
                  }
                } else if (strcmp(p->key, "date") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cDay = getNumDayOfWeekFromDate((uint32_t)p->value.toNumber());
                } else if (strcmp(p->key, "startTime") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cStartPeriod = getPeriodNumberForStartTime(p->value.toNumber());
                } else if (strcmp(p->key, "endTime") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cEndPeriod = getPeriodNumberForEndTime(p->value.toNumber());
                } else if (strcmp(p->key, "su") == 0) {
                  periodName = getEntity(p->value, periodChanged);
                } else if (strcmp(p->key, "te") == 0) {
                  teacherName = getEntity(p->value, teacherChanged);
                  teacherLastname = getEntity(p->value, teacherChanged, "longname");
                } else if (strcmp(p->key, "ro") == 0) {
                  roomName = getEntity(p->value, roomChanged);
                } else if (strcmp(p->key, "lstext") == 0 && p->value.getTag() == JSON_STRING) {
                  lstext = new String(p->value.toString());
                }
              }
              if (cDay < UINT8_MAX && cStartPeriod < UINT8_MAX && cEndPeriod < UINT8_MAX) {
                TTLesson newLesson;
                newLesson.lesson = (cStartPeriod << 4) ^ cEndPeriod;
                newLesson.dayAndFlags = (cDay << 4) ^ periodChanged << 2 ^ teacherChanged << 1 ^ roomChanged;
                if (irregular && lstext) {
                  newLesson.dayAndFlags |= 7;
                }
                if (irregular && !periodName && !teacherName && !roomName && !lstext) {
                  newLesson.name = "---";
                  newLesson.room = "EVENT";
                } else {
                  if (periodName)
                    newLesson.name = *periodName;
                  if (!periodName && lstext)
                    if (lstext->startsWith("Jahrgangsstufentest ")) {
                      newLesson.name = "JGST" + lstext->substring(19);
                    } else {
                      newLesson.name = *lstext;
                    }
                  if (teacherName)
                    newLesson.teacher = *teacherName;
                  if (teacherLastname)
                    newLesson.teacherName = *teacherLastname;
                  if (roomName)
                    newLesson.room = *roomName;
                }
                if (cEndPeriod+1 > timeTable.maxPeriods) {
                  timeTable.maxPeriods = cEndPeriod+1;
                }
                if (!checkAndMergeExistingLesson(newLesson)) {
                  //DEBUG_PRINTLN(String("Found double lesson: ") + newLesson.name);
                  timeTable.lessonGrid->push_back(newLesson);
                }
                ret = true;
              }
            }
          }
        }
      }
    }
  } else {
    DEBUG_PRINTLN(String(jsonStrError(status)) + " at " + String(endptr - source));
    ret = false;
  }
  allocator.deallocate();
  return ret;
}

bool updateHomework(char *source) {
  bool ret = false;
  char *endptr;
  JsonValue value;
  JsonAllocator allocator;
  int status = jsonParse(source, &endptr, &value, allocator);
  if (status == JSON_OK) {
    std::vector<Homework> *homework = new std::vector<Homework>();
    if (value.getTag() == JSON_OBJECT) {
      for(auto d:value) {
        if (strcmp(d->key, "data") == 0 && d->value.getTag() == JSON_OBJECT) {
          for(auto v:d->value) {
            if (strcmp(v->key, "records") == 0 && v->value.getTag() == JSON_ARRAY) {
              for(auto r:v->value) {
                if (r->value.getTag() == JSON_OBJECT) {
                  Homework *newHomework = nullptr;
                  for(auto z:r->value) {
                    if (strcmp(z->key, "homeworkId") == 0 && z->value.getTag() == JSON_NUMBER) {
                      newHomework = new Homework{(uint16_t)z->value.toNumber()};
                    } else if (newHomework && strcmp(z->key, "teacherId") == 0 && z->value.getTag() == JSON_NUMBER) {
                      newHomework->teacher_id = z->value.toNumber();
                      homework->push_back(*newHomework);
                    }
                  }
                }
              }
            } else if (strcmp(v->key, "homeworks") == 0 && v->value.getTag() == JSON_ARRAY) {
              for(auto h:v->value) {
                if (h->value.getTag() == JSON_OBJECT) {
                  Homework *newHomework = nullptr;
                  for(auto z:h->value) {
                    if (strcmp(z->key, "id") == 0 && z->value.getTag() == JSON_NUMBER) {
                      uint16_t hwId = z->value.toNumber();
                      for(auto &existingHomework:*homework) {
                        if (existingHomework.id == hwId) {
                          newHomework = &existingHomework;
                          break;
                        }
                      }
                    } else if (newHomework) {
                      if (strcmp(z->key, "lessonId") == 0 && z->value.getTag() == JSON_NUMBER) {
                        newHomework->subject_id = z->value.toNumber();
                      } else if (strcmp(z->key, "dueDate") == 0 && z->value.getTag() == JSON_NUMBER) {
                        newHomework->day = getNumDayOfWeekFromDate((uint32_t)z->value.toNumber());
                      } else if (strcmp(z->key, "text") == 0 && z->value.getTag() == JSON_STRING) {
                        newHomework->text = z->value.toString();
                      }
                    }
                  }
                }
              }
            } else if (strcmp(v->key, "teachers") == 0 && v->value.getTag() == JSON_ARRAY) {
              for(auto t:v->value) {
                if (t->value.getTag() == JSON_OBJECT) {
                  std::vector<Homework*> *newHomeworks = new std::vector<Homework*>();
                  for(auto z:t->value) {
                    if (strcmp(z->key, "id") == 0 && z->value.getTag() == JSON_NUMBER) {
                      const uint16_t teId = z->value.toNumber();
                      for(auto &existingHomework:*homework) {
                        if (existingHomework.teacher_id == teId) {
                          newHomeworks->push_back(&existingHomework);
                        }
                      }
                    } else if (strcmp(z->key, "name") == 0 && z->value.getTag() == JSON_STRING) {
                      String teacherName = z->value.toString();
                      for(auto *newHomework:*newHomeworks) {
                        newHomework->teacher = teacherName;
                        //DEBUG_PRINTLN(String("Set teacher name to " + newHomework->teacher + " for teacher id " + newHomework->teacher_id));
                      }
                    }
                  }
                  delete newHomeworks;
                }
              }
            } else if (strcmp(v->key, "lessons") == 0 && v->value.getTag() == JSON_ARRAY) {
              for(auto l:v->value) {
                if (l->value.getTag() == JSON_OBJECT) {
                  std::vector<Homework*> *newHomeworks = new std::vector<Homework*>();
                  for(auto z:l->value) {
                    if (strcmp(z->key, "id") == 0 && z->value.getTag() == JSON_NUMBER) {
                      uint16_t subId = z->value.toNumber();
                      for(auto &existingHomework:*homework) {
                        if (existingHomework.subject_id == subId) {
                          newHomeworks->push_back(&existingHomework);
                        }
                      }
                    } else if (strcmp(z->key, "subject") == 0 && z->value.getTag() == JSON_STRING) {
                      String subject = z->value.toString();
                      const int ind = subject.indexOf('_');
                      if (ind > -1) {
                        subject.remove(ind);
                      }
                      for(auto *newHomework:*newHomeworks) {
                        newHomework->subject = subject;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    for(auto &hw:*homework) {
      DEBUG_PRINT(String("Looking for matching homework for id ") + hw.id + " day " + hw.day + " teacher " + hw.teacher + " subject " + hw.subject);
      for(auto &lesson:*timeTable.lessonGrid) {
        if (lesson.name == hw.subject && lesson.teacher == hw.teacher && UPHA(lesson.dayAndFlags) == hw.day) {
          lesson.dayAndFlags ^= 8;
          DEBUG_PRINTLN(" -> Found!");
        }
      }
    }
    delete homework;
    ret = true;
  } else {
    DEBUG_PRINTLN(String(jsonStrError(status)) + " at " + String(endptr - source));
    ret = false;
  }
  allocator.deallocate();
  return ret;
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
//#define SAVE_CREDENTIALS
#ifdef SAVE_CREDENTIALS
  {
    preferences.begin(ESP_NAME);
    preferences.clear();
    preferences.putString("WIFI_SSID", "SSID");
    preferences.putString("WIFI_PASS", "****");
    preferences.putString("TIMEZONE", "TZ");
    preferences.putString("WU_SCHOOL", "SCHOOL");
    preferences.putString("WU_SUBDOM", "SUBDOM");
    preferences.putString("WU_USER", "USER");
    preferences.putString("WU_PASS", "****");
    preferences.end();
    esp_deep_sleep_start();
  }
#endif
  preferences.begin(ESP_NAME, true);
  const String wifissid = preferences.getString("WIFI_SSID");
  const String wifipass = preferences.getString("WIFI_PASS");
  //DEBUG_PRINTLN("Loaded SSID: " + wifissid + ", PASS: " + wifipass);
  
  // Connect to Wi-Fi network with SSID and password
  DEBUG_PRINTLN("Connecting to " + wifissid);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifissid.c_str(), wifipass.c_str());
  WiFi.setHostname(ESP_NAME);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    DEBUG_PRINTLN("WiFi connection failed! Code " + WiFi.waitForConnectResult());
    delay(1000);
    ESP.restart();
  }
  setClock();
  // Print local IP
  DEBUG_PRINTLN("WiFi connected.");
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  bool updateDisplay = false;

  // secure wifi connection (for https)
  WiFiClientSecure *client = new WiFiClientSecure;

  if(client) {
    client->setCACert(rootCACertificate);

    // get current time table for the week via WebUntis JSON RPC API
    if (webUntisLogin(*client)) {
      updateDisplay |= updateTimeUnits(webUntisTimegrid(*client).begin()) &&
          updateTimeTable(webUntisTimetable(*client).begin()) &&
          updateHomework(webUntisGetHomework(*client).begin());
      webUntisLogout(*client);
    }
  
    delete client;
  } else {
    DEBUG_PRINTLN("Unable to create client");
  }
  delay(1000);

  if (updateDisplay) {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    // display setup and printing
    initialiseDisplay();

    clearAllColors();
    display.firstPage();
    do {
      drawView1();
    } while (display.nextPage());

    display.powerOff();
    DEBUG_PRINTLN("Display has been updated.");
    DEBUG_PRINT("Waking up in ");
    DEBUG_PRINT(secondsTillNextWakeup);
    DEBUG_PRINTLN(" seconds.");
    DEBUG_PRINTLN("Going to deep sleep...");
    esp_sleep_enable_timer_wakeup(secondsTillNextWakeup * US_TO_SECONDS_FACTOR);
  } else {
    DEBUG_PRINTLN("Waking up in " + String(SLEEP_DURATION) + " seconds...");
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * US_TO_SECONDS_FACTOR);
  }
  preferences.end();

  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {}
