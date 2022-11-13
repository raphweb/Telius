#include <Arduino.h>
#include "DisplayConfig.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Ready.");

  display.init();
  // comment out next line to have no or minimal Adafruit_GFX code
  display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    // comment out next line to have no or minimal Adafruit_GFX code
    display.print("Hello World!");
  }
  while (display.nextPage());
}

void loop() {
  delay(2000);
}
