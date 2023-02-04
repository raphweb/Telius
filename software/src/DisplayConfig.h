#ifndef DISPLAY_CONFIG_HPP_
#define DISPLAY_CONFIG_HPP_

#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <u8g2_fonts.h>

#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_565c
// Connections for e.g. LOLIN D32
#define EPD_BUSY  4 // to EPD BUSY
#define EPD_CS    5 // to EPD CS
#define EPD_RST  16 // to EPD RST
#define EPD_DC   17 // to EPD DC
#define EPD_SCK  18 // to EPD CLK
#define EPD_MISO 19 // Master-In Slave-Out not used, as no data from display
#define EPD_MOSI 23 // to EPD DIN

#define MAX_DISPLAY_BUFFER_SIZE 65536UL
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
#undef MAX_DISPLAY_BUFFER_SIZE
#undef MAX_HEIGHT

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void initialiseDisplay() {
    display.init(115200, true, 20, false/*, *(new SPIClass(HSPI)), SPISettings(4000000, MSBFIRST, SPI_MODE0)*/);
    SPI.end();
    SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    u8g2Fonts.begin(display);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    display.fillScreen(GxEPD_WHITE);
    display.setFullWindow();
}

#endif