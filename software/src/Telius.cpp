#include <Arduino.h>

#include <Preferences.h>
#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <u8g2_fonts.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include "time.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <gason.h>
#include <base64.h>
#include <vector>

#include "DisplayConfig.h"

#define US_TO_SECONDS_FACTOR 1000000L
#define SLEEP_DURATION 60 // in seconds

#define ESP_NAME "telius"
//#define SAVE_CREDENTIALS

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
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
Preferences preferences;
HTTPClient https;

char currMonday[9] = "";
char nextFriday[9] = "";
uint32_t currentMonday = 0; // used to store the date of the monday of the current week in YYYYmmdd
uint64_t secondsTillNextWakeup = 0;
#define WAKEUP_HOUR 6

// This is lets-encrypt-r3.pem, the intermediate Certificate Authority that
// signed the server certifcate for the WebUntis server https://ajax.webuntis.com
// used here. This certificate is valid until Sep 15 16:00:00 2025 GMT
// See: https://letsencrypt.org/certificates/
const char* rootCACertificate PROGMEM = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIFFjCCAv6gAwIBAgIRAJErCErPDBinU/bWLiWnX1owDQYJKoZIhvcNAQELBQAw\n" \
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjAwOTA0MDAwMDAw\n" \
  "WhcNMjUwOTE1MTYwMDAwWjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
  "RW5jcnlwdDELMAkGA1UEAxMCUjMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
  "AoIBAQC7AhUozPaglNMPEuyNVZLD+ILxmaZ6QoinXSaqtSu5xUyxr45r+XXIo9cP\n" \
  "R5QUVTVXjJ6oojkZ9YI8QqlObvU7wy7bjcCwXPNZOOftz2nwWgsbvsCUJCWH+jdx\n" \
  "sxPnHKzhm+/b5DtFUkWWqcFTzjTIUu61ru2P3mBw4qVUq7ZtDpelQDRrK9O8Zutm\n" \
  "NHz6a4uPVymZ+DAXXbpyb/uBxa3Shlg9F8fnCbvxK/eG3MHacV3URuPMrSXBiLxg\n" \
  "Z3Vms/EY96Jc5lP/Ooi2R6X/ExjqmAl3P51T+c8B5fWmcBcUr2Ok/5mzk53cU6cG\n" \
  "/kiFHaFpriV1uxPMUgP17VGhi9sVAgMBAAGjggEIMIIBBDAOBgNVHQ8BAf8EBAMC\n" \
  "AYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMBMBIGA1UdEwEB/wQIMAYB\n" \
  "Af8CAQAwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMB8GA1UdIwQYMBaA\n" \
  "FHm0WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcw\n" \
  "AoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzAnBgNVHR8EIDAeMBygGqAYhhZodHRw\n" \
  "Oi8veDEuYy5sZW5jci5vcmcvMCIGA1UdIAQbMBkwCAYGZ4EMAQIBMA0GCysGAQQB\n" \
  "gt8TAQEBMA0GCSqGSIb3DQEBCwUAA4ICAQCFyk5HPqP3hUSFvNVneLKYY611TR6W\n" \
  "PTNlclQtgaDqw+34IL9fzLdwALduO/ZelN7kIJ+m74uyA+eitRY8kc607TkC53wl\n" \
  "ikfmZW4/RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\n" \
  "CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\n" \
  "lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\n" \
  "avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\n" \
  "yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\n" \
  "yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\n" \
  "hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\n" \
  "HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\n" \
  "MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\n" \
  "nLRbwHOoq7hHwg==\n" \
  "-----END CERTIFICATE-----\n";

void fillDateWeekDays(time_t currentDay);

void setClock() {
  // Europe/Berlin timezone
  // see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

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
  const uint8_t offset;
};

struct TTLesson {
  char name[4] = {0};
  char teacher[5] = {0};
  char teacherName[32] = {0};
  char room[11] = {0};
  uint8_t dayLesson = 0;
  uint8_t changedFlags = 0;
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
  const char days[7][11] = {
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

#define pauseStr "Pause"
#define rowHeight 30

void drawString(int16_t x, int16_t y, const char *text, const AlignmentKind alignment = BOTTOM_LEFT) {
  const uint16_t w = u8g2Fonts.getUTF8Width(text);
  const uint16_t h = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
  if (alignment & RIGHT)  x -= w;
  if (alignment & CENTER) x -= w / 2;
  if (alignment & MIDDLE) y += h / 2;
  if (alignment & TOP)    y += h;
  //display.drawRect(x, y-u8g2Fonts.getFontAscent(), w, h, GxEPD_RED);
  u8g2Fonts.drawUTF8(x, y, text);
}

void drawView1() {
  display.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  for(uint8_t cDay = 0; cDay < timeTable.maxDays; cDay++) {
    const char dayName[] = {timeTable.days[cDay][0], timeTable.days[cDay][1], 0};
    display.drawFastVLine(79 + cDay*104, 6, display.height() - 12, GxEPD_BLACK);
    drawString(130 + cDay*104, 33, dayName, BOTTOM_CENTER);
  }
  for(uint8_t cPeriod = 0; cPeriod < timeTable.maxPeriods; cPeriod++) {
    const auto& row = timeTable.periods->at(cPeriod);
    display.drawFastHLine(0, 11 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
    const char time1[] = getTimeStrForTime(row.time1);
    drawString(38, 35 + rowHeight*row.offset, time1, BOTTOM_CENTER);
    display.drawFastHLine(15, 10 + rowHeight + rowHeight*row.offset, 40, GxEPD_BLACK);
    const char time2[] = getTimeStrForTime(row.time2);
    drawString(38, 35 + rowHeight*(row.offset + 1), time2, BOTTOM_CENTER);
    display.drawFastHLine(0, 10 + rowHeight*2 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
  }
  drawString(38, 35 + rowHeight*7, pauseStr, BOTTOM_CENTER);
  // fill time table
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  for(const auto& lesson : *timeTable.lessonGrid) {
    const uint16_t columnOffset = 83 + UPHA(lesson.dayLesson)*104;
    const uint8_t rowOffset = timeTable.periods->at(LOHA(lesson.dayLesson)).offset;
    drawString(columnOffset, 34 + rowHeight*rowOffset, lesson.name);
    drawString(columnOffset + 96, 34 + rowHeight*rowOffset, lesson.teacher, BOTTOM_RIGHT);
    drawString(columnOffset, 34 + rowHeight*(rowOffset+1), lesson.room);
  }
}

void fillWithColor(uint16_t color) {
  display.firstPage();
  do {
    display.fillScreen(color);
  } while (display.nextPage());
}

void clearAllColors() {
  fillWithColor(GxEPD_BLACK);
  fillWithColor(GxEPD_GREEN);
  fillWithColor(GxEPD_BLUE);
  fillWithColor(GxEPD_RED);
  fillWithColor(GxEPD_YELLOW);
  fillWithColor(GxEPD_ORANGE);
  delay(1000);
}

SessionData *currentSession = nullptr;

bool webUntisLogin(WiFiClientSecure &client) {
  const String school = preferences.getString("WU_SCHOOL");
  const String user = preferences.getString("WU_USER");
  const String pass = preferences.getString("WU_PASS");
  //DEBUG_PRINTLN("Loaded WebUntis school: " + school + ", user: " + user + ", pass: " + pass);

  if (https.begin(client, "https://ajax.webuntis.com/WebUntis/jsonrpc.do?school=" + school)) {
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
    if (https.begin(client, "https://ajax.webuntis.com/WebUntis/jsonrpc.do?school=" + school)) {
      https.addHeader("Cookie", currentSession->sessionCookie);
      int httpCode = https.POST("{\"id\":\"" + String(ESP_NAME) + "243\",\"method\":\"" + method + "\",\"params\":{" + params + "},\"jsonrpc\":\"2.0\"}");
      if (httpCode > 0) {
        DEBUG_PRINTLN("[HTTPS] POST " + method + "... code: " + httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          DEBUG_PRINTLN(payload);
          return payload;
        }
      } else {
        DEBUG_PRINTLN("[HTTPS] POST " + method + "... failed, error: " + https.errorToString(httpCode));
      }
    }
    https.end();
  }
  return "";
}

String webUntisTimegrid(WiFiClientSecure &client) {
  return webUntisRequest(client, "getTimegridUnits");
}

String webUntisTeachers(WiFiClientSecure &client) {
  return webUntisRequest(client, "getTeachers");
}

String webUntisTimetable(WiFiClientSecure &client) {
  if (currentSession) {
    return webUntisRequest(client, "getTimetable", "\"options\":{\"element\":{\"id\":\"" + String(currentSession->personId) + "\",\"type\":" +
      String(currentSession->personType) + "},\"startDate\":" + currMonday + ",\"endDate\":" + nextFriday + ",\"klasseFields\":[\"name\"]," +
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

bool updateTimeUnits(String payload) {
  bool ret = false;
  char *source = payload.begin();
  char *endptr;
  JsonValue value;
  JsonAllocator allocator;
  int status = jsonParse(source, &endptr, &value, allocator);
  if (status == JSON_OK) {
    uint16_t startTime = UINT16_MAX, endTime = UINT16_MAX;
    uint8_t offset = 1;
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
                          offset++;
                        }
                        TTRowHeaderCell period = {startTime, endTime, offset};
                        timeTable.periods->push_back(period);
                        offset += 2;
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
  return ret;
}

bool updateTimeTable(String payload) {
  bool ret = false;
  char *source = payload.begin();
  char *endptr;
  JsonValue value;
  JsonAllocator allocator;
  int status = jsonParse(source, &endptr, &value, allocator);
  if (status == JSON_OK) {
    uint8_t cDay, cPeriod;
    String *periodName, *teacherName, *teacherLastname, *roomName;
    bool periodChanged, teacherChanged, roomChanged;
    if (value.getTag() == JSON_OBJECT) {
      for(auto r:value) {
        if (strcmp(r->key, "result") == 0 && r->value.getTag() == JSON_ARRAY) {
          for(auto v:r->value) {
            if (v->value.getTag() == JSON_OBJECT) {
              cDay = UINT8_MAX;
              cPeriod = UINT8_MAX;
              periodName = teacherName = teacherLastname = roomName = nullptr;
              periodChanged = teacherChanged = roomChanged = false;
              bool irregular = false;
              for(auto p:v->value) {
                if (strcmp(p->key, "code") == 0 && p->value.getTag() == JSON_STRING) {
                  if (strcmp(p->value.toString(), "cancelled") == 0) {
                    cPeriod = UINT8_MAX;
                    break;
                  } else if (strcmp(p->value.toString(), "irregular") == 0) {
                    irregular = true;
                  }
                } else if (strcmp(p->key, "date") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cDay = getNumDayOfWeekFromDate((uint32_t)p->value.toNumber());
                } else if (strcmp(p->key, "startTime") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cPeriod = getPeriodNumberForStartTime(p->value.toNumber());
                } else if (strcmp(p->key, "su") == 0) {
                  periodName = getEntity(p->value, periodChanged);
                } else if (strcmp(p->key, "te") == 0) {
                  teacherName = getEntity(p->value, teacherChanged);
                  teacherLastname = getEntity(p->value, teacherChanged);
                } else if (strcmp(p->key, "ro") == 0) {
                  roomName = getEntity(p->value, roomChanged);
                }
              }
              if (cDay < UINT8_MAX && cPeriod < UINT8_MAX) {
                TTLesson newLesson;
                newLesson.dayLesson = (cDay << 4) ^ cPeriod;
                if (irregular && !periodName && !teacherName && !roomName) {
                  String("---").toCharArray(newLesson.name, 4);
                  String("EVENT").toCharArray(newLesson.room, 11);
                } else {
                  if (periodName)
                    periodName->toCharArray(newLesson.name, 4);
                  if (teacherName)
                    teacherName->toCharArray(newLesson.teacher, 5);
                  if (teacherLastname)
                    teacherLastname->toCharArray(newLesson.teacherName, 32);
                  if (roomName)
                    roomName->toCharArray(newLesson.room, 11);
                }
                if (cPeriod+1 > timeTable.maxPeriods) {
                  timeTable.maxPeriods = cPeriod+1;
                }
                timeTable.lessonGrid->push_back(newLesson);
                ret = true;
              }
            }
          }
        }
      }
    }
  } else {
    DEBUG_PRINTLN(String(jsonStrError(status)) + " at " + String(endptr - source));
    return false;
  }
  return ret;
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  preferences.begin(ESP_NAME);

#ifdef SAVE_CREDENTIALS
  {
    const String wifissid = "WLAN";
    const String wifipass = "****";
    const String webUntisSchool = "school";
    const String webUntisUser = "USER";
    const String webUntisPass = "****";
    preferences.clear();
    preferences.putString("WL_SSID", wifissid);
    preferences.putString("WL_PASS", wifipass);
    preferences.putString("WU_SCHOOL", webUntisSchool);
    preferences.putString("WU_USER", webUntisUser);
    preferences.putString("WU_PASS", webUntisPass);
    preferences.end();
    esp_deep_sleep_start();
  }
#endif
  const String wifissid = preferences.getString("WL_SSID");
  const String wifipass = preferences.getString("WL_PASS");
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
  // Print local IP address and start web server
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
      updateDisplay |= updateTimeUnits(webUntisTimegrid(*client)) && updateTimeTable(webUntisTimetable(*client));
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
    display.init(115200, true, 20, false, *(new SPIClass(VSPI)), SPISettings(4000000, MSBFIRST, SPI_MODE0));

    u8g2Fonts.begin(display);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);

    display.setFullWindow();
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

void loop() {
  // never executed when using deep sleep
}
