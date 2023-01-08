#include <Arduino.h>

#include <Preferences.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans12pt7b.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <gason.h>

#include "DisplayConfig.h"

#define US_TO_SECONDS_FACTOR 1000000
#define SLEEP_DURATION 60

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
  configTime(0, 0, "pool.ntp.org");

  DEBUG_PRINT("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    DEBUG_PRINT(".");
    yield();
    nowSecs = time(nullptr);
  }

  DEBUG_PRINTLN();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  DEBUG_PRINT("Current time: ");
  DEBUG_PRINT(asctime(&timeinfo));
}

typedef struct {
  const char time1[6];
  const char time2[6];
  const uint8_t offset;
} TableRow;

const TableRow schedule[] = {
  {"08:00", "08:45", 1},
  {"08:45", "09:30", 3},
  {"09:30", "10:15", 5},
  {"10:40", "11:25", 8},
  {"11:25", "12:10", 10},
  {"12:10", "12:55", 12}
};

typedef struct {
  const char day[11];
  const uint16_t offset;
} TableColumn;

const TableColumn days[] = {
  {"Montag", 84},
  {"Dienstag", 174},
  {"Mittwoch", 277},
  {"Donnerstag", 381},
  {"Freitag", 514}
};

const char pauseStr[] = "Pause";
const uint8_t rowHeight = 30;

void drawTable() {
  for(const auto& column : days) {
    display.drawFastVLine(column.offset - 5, 6, display.height() - 12, GxEPD_BLACK);
    display.setCursor(column.offset, 31);
    display.print(column.day);
  }
  for(const auto& row : schedule) {
    display.drawFastHLine(0, 11 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
    display.setCursor(6, 33 + rowHeight*row.offset);
    display.print(row.time1);
    display.drawFastHLine(15, 10 + rowHeight + rowHeight*row.offset, 40, GxEPD_BLACK);
    display.setCursor(6, 33 + rowHeight*(row.offset + 1));
    display.print(row.time2);
    display.drawFastHLine(0, 10 +rowHeight*2 + rowHeight*row.offset, display.width(), GxEPD_BLACK);
  }
  display.setCursor(6, 33 + rowHeight*7);
  display.print(pauseStr);
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  preferences.begin(ESP_NAME);

#ifdef SAVE_CREDENTIALS
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
#else
  const String wifissid = preferences.getString("WL_SSID");
  const String wifipass = preferences.getString("WL_PASS");
  DEBUG_PRINTLN("Loaded SSID: " + wifissid + ", PASS: " + wifipass);
  const String webUntisSchool = preferences.getString("WU_SCHOOL");
  const String webUntisUser = preferences.getString("WU_USER");
  const String webUntisPass = preferences.getString("WU_PASS");
  DEBUG_PRINTLN("Loaded WebUntis school: " + webUntisSchool + ", user: " + webUntisUser + ", pass: " + webUntisPass);
#endif
  
  // Connect to Wi-Fi network with SSID and password
  DEBUG_PRINTLN("Connecting to " + String(wifissid));
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

  WiFiClientSecure *client = new WiFiClientSecure;

  if(client) {
    client->setCACert(rootCACertificate);

    {
      HTTPClient https;
  
      DEBUG_PRINTLN("[HTTPS] begin...");
      if (https.begin(*client, "https://ajax.webuntis.com/WebUntis/jsonrpc.do?school=" + webUntisSchool)) {  // HTTPS
        DEBUG_PRINTLN("[HTTPS] POST...");
        // start connection and send HTTP header
        int httpCode = https.POST("{\"id\":\"243\",\"method\":\"authenticate\",\"params\":{\"user\":\"" + webUntisUser + "\",\"password\":\"" + webUntisPass + "\",\"client\":\"" + String(ESP_NAME) + "\"},\"jsonrpc\":\"2.0\"}");
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            DEBUG_PRINTLN(payload);
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        Serial.flush();
  
        https.end();
      } else {
        DEBUG_PRINTLN("[HTTPS] Unable to connect");
      }
    }
  
    delete client;
  } else {
    DEBUG_PRINTLN("Unable to create client");
  }

  // display setup and printing
  display.init(115200, true, 20, false, *(new SPIClass(VSPI)), SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.setFont(&FreeSans12pt7b);

  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawTable();
  } while (display.nextPage());

  display.hibernate();
  DEBUG_PRINTLN("Display up to date.");

  preferences.end();

  // deep sleep configuration
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * US_TO_SECONDS_FACTOR);
  DEBUG_PRINTLN("Going to deep sleep...");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  // never executed when using deep sleep
}
