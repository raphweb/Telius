#include <Arduino.h>

#include <Preferences.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <gason.h>
#include <base64.h>

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

#define DEBUG_PRINTLNC(m) Serial.println("[Core " + String(xPortGetCoreID()) + "]" + m)
#define DEBUG_PRINTC(m) Serial.print("[Core " + String(xPortGetCoreID()) + "]" + m)

#else
#define DEBUG_PRINTLN(m)
#define DEBUG_PRINT(m)

#define DEBUG_PRINTLNC(m)
#define DEBUG_PRINTC(m)
#endif


Preferences preferences;
HTTPClient https;

char currMonday[9] = "";
char nextFriday[9] = "";
uint32_t currentMonday = 0; // used to store the date of the monday of the current week in YYYYmmdd
uint64_t secondsTillNextWakeup = 0;

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

  const uint8_t hoursTill5 = (29 - timeinfo.tm_hour) % 24;
  secondsTillNextWakeup = 3600 * hoursTill5 - 60; // updating will take about a minute
  time_t wTimeSecs = nowSecs + secondsTillNextWakeup;
  struct tm wTime;
  localtime_r(&wTimeSecs, &wTime);
  // the wakeup time will drift over time, but will always happen some time between 5 and 6 am
  DEBUG_PRINT("Next wakeup will be scheduled at: ");
  DEBUG_PRINTLN(asctime(&wTime));
}

#define NUM_DAYS 5
#define NUM_PERIODS 6

struct SessionData {
  const uint16_t klasseId;
  const uint16_t personId;
  const String sessionCookie;
  const uint8_t personType;
};

struct TTRowHeaderCell {
  const char time1[6];
  const char time2[6];
  const uint8_t offset;
};

struct TTColHeaderCell {
  const char day[11];
  const uint16_t offset;
};

struct TTLesson {
  char name[4] = {0};
  char teacher[5] = {0};
  char room[11] = {0};
  uint8_t changedFlags = 0;
};

uint8_t getPeriodNumberForStartTime(uint16_t startTime) {
  switch (startTime) {
    case  800: return 0;
    case  845: return 1;
    case  930: return 2;
    case 1040: return 3;
    case 1125: return 4;
    case 1210: return 5;
    default: return 0;
  }
}

struct TimeTable {
  const TTRowHeaderCell periods[NUM_PERIODS] = {
    {"08:00", "08:45", 1},
    {"08:45", "09:30", 3},
    {"09:30", "10:15", 5},
    {"10:40", "11:25", 8},
    {"11:25", "12:10", 10},
    {"12:10", "12:55", 12}
  };
  const TTColHeaderCell days[NUM_DAYS] = {
    {"Montag", 84},
    {"Dienstag", 174},
    {"Mittwoch", 277},
    {"Donnerstag", 381},
    {"Freitag", 514}
  };
  TTLesson lessonGrid[NUM_DAYS][NUM_PERIODS];
  uint32_t lastUpdateDay = 0; // used to store the day of the last update in YYYYmmdd, e.g., 20230113
};

TimeTable timeTable;

const char pauseStr[] = "Pause";
const uint8_t rowHeight = 30;

void drawTable() {
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(GxEPD_BLACK);
  for(const auto& column : timeTable.days) {
    display.drawFastVLine(column.offset - 5, 6, display.height() - 12, GxEPD_BLACK);
    display.setCursor(column.offset, 31);
    display.print(column.day);
  }
  for(const auto& row : timeTable.periods) {
    display.drawFastHLine(0, 11 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
    display.setCursor(6, 33 + rowHeight*row.offset);
    display.print(row.time1);
    display.drawFastHLine(15, 10 + rowHeight + rowHeight*row.offset, 40, GxEPD_BLACK);
    display.setCursor(6, 33 + rowHeight*(row.offset + 1));
    display.print(row.time2);
    display.drawFastHLine(0, 10 + rowHeight*2 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
  }
  display.setCursor(6, 33 + rowHeight*7);
  display.print(pauseStr);
  // fill time table
  display.setFont(&FreeSans9pt7b);
  for(uint8_t cDay = 0; cDay < NUM_DAYS; cDay++) {
    const uint16_t columnOffset = timeTable.days[cDay].offset;
    for(uint8_t cPeriod = 0; cPeriod < NUM_PERIODS; cPeriod++) {
      const uint8_t rowOffset = timeTable.periods[cPeriod].offset;
      display.setCursor(columnOffset, 33 + rowHeight*rowOffset);
      display.print(timeTable.lessonGrid[cDay][cPeriod].name);
      display.setCursor(columnOffset + 50, 33 + rowHeight*rowOffset);
      display.print(timeTable.lessonGrid[cDay][cPeriod].teacher);
      display.setCursor(columnOffset, 33 + rowHeight*(rowOffset+1));
      display.print(timeTable.lessonGrid[cDay][cPeriod].room);
    }
  }
}


SessionData *currentSession = nullptr;

bool webUntisLogin(WiFiClientSecure &client) {
  const String school = preferences.getString("WU_SCHOOL");
  const String user = preferences.getString("WU_USER");
  const String pass = preferences.getString("WU_PASS");
  DEBUG_PRINTLN("Loaded WebUntis school: " + school + ", user: " + user + ", pass: " + pass);

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

String webUntisTimetable(WiFiClientSecure &client) {
  if (currentSession) {
    return webUntisRequest(client, "getTimetable", "\"options\":{\"element\":{\"id\":\"" + String(currentSession->personId) + "\",\"type\":" +
      String(currentSession->personType) + "},\"startDate\":" + currMonday + ",\"endDate\":" + nextFriday + ",\"klasseFields\":[\"name\"]," +
      "\"teacherFields\":[\"name\"],\"subjectFields\":[\"name\"],\"roomFields\":[\"name\"]}");
  }
  return "";
}

void webUntisLogout(WiFiClientSecure &client) {
  webUntisRequest(client, "logout");
  currentSession = nullptr;
}

String *getEntity(JsonValue &v, bool &changed) {
  String *ret = nullptr;
  if (v.getTag() == JSON_ARRAY) {
    for(auto ae:v) {
      if (ae->value.getTag() == JSON_OBJECT) {
        for(auto f:ae->value) {
          if (strcmp(f->key, "name") == 0 && f->value.getTag() == JSON_STRING) {
            ret = new String(f->value.toString());
            ret->replace("Ü", "Ue");
            const int ind = ret->indexOf('_');
            if (ind > -1) {
              ret->remove(ind);
            }
          } else if (strcmp(f->key, "orgname") == 0) {
            changed = true;
          }
        }
      }
    }
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
    String *periodName, *teacherName, *roomName;
    bool periodChanged, teacherChanged, roomChanged;
    if (value.getTag() == JSON_OBJECT) {
      for(auto r:value) {
        if (strcmp(r->key, "result") == 0 && r->value.getTag() == JSON_ARRAY) {
          for(auto v:r->value) {
            if (v->value.getTag() == JSON_OBJECT) {
              cDay = NUM_DAYS;
              cPeriod = NUM_PERIODS;
              periodName = teacherName = roomName = nullptr;
              periodChanged = teacherChanged = roomChanged = false;
              for(auto p:v->value) {
                if (strcmp(p->key, "date") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cDay = ((uint32_t)p->value.toNumber() - currentMonday);
                } else if (strcmp(p->key, "startTime") == 0 && p->value.getTag() == JSON_NUMBER) {
                  cPeriod = getPeriodNumberForStartTime(p->value.toNumber());
                } else if (strcmp(p->key, "su") == 0) {
                  periodName = getEntity(p->value, periodChanged);
                } else if (strcmp(p->key, "te") == 0) {
                  teacherName = getEntity(p->value, teacherChanged);
                } else if (strcmp(p->key, "ro") == 0) {
                  roomName = getEntity(p->value, roomChanged);
                }
              }
              if (cDay < NUM_DAYS && cPeriod < NUM_PERIODS) {
                if (periodName)
                  periodName->toCharArray(timeTable.lessonGrid[cDay][cPeriod].name, 4);
                if (teacherName)
                  teacherName->toCharArray(timeTable.lessonGrid[cDay][cPeriod].teacher, 5);
                if (roomName)
                  roomName->toCharArray(timeTable.lessonGrid[cDay][cPeriod].room, 11);
                ret = (periodName || teacherName || roomName);
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
  DEBUG_PRINTLN("Loaded SSID: " + wifissid + ", PASS: " + wifipass);
  
  // Connect to Wi-Fi network with SSID and password
  DEBUG_PRINTLN("Connecting to " + wifissid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifissid.c_str(), wifipass.c_str());
  WiFi.setHostname(ESP_NAME);
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
      updateDisplay |= updateTimeTable(webUntisTimetable(*client));
      webUntisLogout(*client);
    }
  
    delete client;
  } else {
    DEBUG_PRINTLN("Unable to create client");
  }
  delay(1000);

  if (updateDisplay) {
    // display setup and printing
    display.init(115200, true, 20, false, *(new SPIClass(VSPI)), SPISettings(4000000, MSBFIRST, SPI_MODE0));

    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      drawTable();
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
