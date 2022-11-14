#include <Arduino.h>

#include <GxEPD2_7C.h>
#include <Fonts/FreeSans12pt7b.h>
#include "DisplayConfig.h"

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
  Serial.println("Ready.");

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
}

void loop() {
  delay(2000);
}
