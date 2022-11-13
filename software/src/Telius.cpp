#include <Arduino.h>

#include <GxEPD2_7C.h>
#include <Fonts/FreeSans12pt7b.h>
#include "DisplayConfig.h"

const char helloWorldStr[] = "Hello colorful world!";

void helloWorld() {
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(helloWorldStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = ((display.width() - tbw) >> 1) - tbx;
  uint16_t y = ((display.height() - tbh) >> 1) - tby;
  uint16_t xc = ((display.width() - 140) >> 1);
  uint16_t yc = y+10;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(helloWorldStr);
    display.fillRect(xc, yc, 20, 20, GxEPD_BLACK);
    display.fillRect(xc+20, yc, 20, 20, GxEPD_WHITE);
    display.fillRect(xc+40, yc, 20, 20, GxEPD_BLUE);
    display.fillRect(xc+60, yc, 20, 20, GxEPD_RED);
    display.fillRect(xc+80, yc, 20, 20, GxEPD_YELLOW);
    display.fillRect(xc+100, yc, 20, 20, GxEPD_GREEN);
    display.fillRect(xc+120, yc, 20, 20, GxEPD_ORANGE);
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  Serial.println("Ready.");

  display.init(115200, true, 20, false, *(new SPIClass(VSPI)), SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.setFont(&FreeSans12pt7b);

  helloWorld();

  display.hibernate();
}

void loop() {
  delay(2000);
}
