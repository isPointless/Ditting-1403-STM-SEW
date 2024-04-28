#include "definitions.h"
#include <Arduino.h>
#include "display.h"
#include <Wire.h>
#include <SPI.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "nano_gfx.h"

SAppMenu menuSelect;

const char *menuItems[] =
{
  "exit Menu", //0 
  "ViewMode", // 1
  "Auto Purge", // 2
  "Calibrate", 
  "set Max RPM",
  "set Min RPM",
  "Sleep time",
  "button->purge", // 7
  "PurgeFramesL", // 8
  "PurgeFramesH", // 9
  "PurgeScalarL", // 10
  "PurgeScalarH", // 11
  "Purge delay", // 12
  "Purge time" // 13
};

const char *infoCodes[] =
  {
    "Calibration complete",
    "saving.."
  };

const char *statusCodes[] =
  {
    "disconnected",
    "not-Ready",  
    "Fault", 
    "Ready",
    "Warning",
    "Output enabled",
    "Purging",
  };

Display::Display() {
  delay(100); //display needs time to charge up? 5v / 3v3
  ssd1306_128x64_i2c_init();  //no for(;;) loop as we don't want display halting the rest, mostly the communication.
  ssd1306_clearScreen();
}

void Display::printData(uint16_t setRPM, uint16_t rpm, uint16_t ampere, u_int8_t status) {
  static uint8_t lastStatus;
  if(lastPrintID != 1 || lastStatus != status) {
    ssd1306_fillScreen(0x00);
    ssd1306_setFixedFont(ssd1306xled_font8x16);

    ssd1306_printFixed(4, 2, "sRPM", STYLE_NORMAL);
    ssd1306_printFixed(4, 16, "RPM", STYLE_NORMAL);
    ssd1306_printFixed(4, 32, "mA", STYLE_NORMAL);


    ssd1306_setFixedFont(ssd1306xled_font8x16);
    ssd1306_printFixed(64 - strlen(statusCodes[status])*4, 50, statusCodes[status], STYLE_NORMAL);
    lastPrintID = 1;
    lastStatus = status;
  } 
    ssd1306_setFixedFont(courier_new_font11x16_digits);

    ssd1306_printFixed(45,2, "     ", STYLE_NORMAL);
    ssd1306_printFixed(45,16, "     ", STYLE_NORMAL);
    ssd1306_printFixed(45,32, "     ", STYLE_NORMAL);
 
    char showRpm[5];
    itoa(rpm, showRpm, 10);
    char showAmp[5];
    itoa(ampere, showAmp, 10);
    char showSetRpm[5];
    itoa(setRPM, showSetRpm, 10);

    ssd1306_printFixed(45,2, showSetRpm, STYLE_NORMAL);
    ssd1306_printFixed(45,16, showRpm, STYLE_NORMAL);
    ssd1306_printFixed(45,32, showAmp, STYLE_NORMAL);
}

void Display::printRPM(uint16_t rpm, u_int8_t status) {
  static uint8_t lastStatus;
  if(lastPrintID != 2 || lastStatus != status) {
    ssd1306_fillScreen(0x00);
    lastPrintID = 2;
    lastStatus = status;

    ssd1306_setFixedFont(ssd1306xled_font8x16);
    ssd1306_printFixed(64 - strlen(statusCodes[status])*4, 50 , statusCodes[status], STYLE_NORMAL);
  }

  ssd1306_setFixedFont(comic_sans_font24x32_123);

  char showRpm[5];
  itoa(rpm, showRpm, 10);
  ssd1306_printFixed(18,8, "    ", STYLE_NORMAL);
  ssd1306_printFixed(rpm<1000? 30 : 18,15, showRpm, STYLE_NORMAL);

}

void Display::off() { 
  ssd1306_fillScreen(0x00);
  lastPrintID = 5;
}

void Display::printMenu(int8_t menuItem, bool selected, uint16_t value) 
{ 
  lastPrintID = 4;
  char showValue[5];
  ssd1306_fillScreen(0x00);
  if (selected == false) {
    ssd1306_setFixedFont(ssd1306xled_font8x16);
    ssd1306_printFixed(20, 24, menuItems[menuItem], STYLE_NORMAL);
    ssd1306_printFixed(4, 4, menuItems[menuItem-1 < 0? menuItemsCount : menuItem-1], STYLE_NORMAL);
    ssd1306_printFixed(4, 50, menuItems[menuItem+1 > menuItemsCount? 0 : menuItem + 1], STYLE_NORMAL);
    ssd1306_drawRect(10, 20, (strlen(menuItems[menuItem])*8 + 27) > 127? (127) : (strlen(menuItems[menuItem])*8 + 27), 45);
  } else 
  {
    
    if (menuItem == 1) // view mode
    {         
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_printFixed(15, 30, "INFO", STYLE_BOLD);
      ssd1306_printFixed(70, 30, "FANCY", STYLE_BOLD);
      if(value == 0) { 
        ssd1306_drawRect(5, 20, 55, 45);
      } 
      if (value == 1) { 
        ssd1306_drawRect(60, 20, 120, 45);
      }
    }
    if (menuItem == 2 || menuItem == 7) //purge mode, or Button purge mode 
    { 
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_printFixed(4, 30, "Enabled", STYLE_BOLD);
      ssd1306_printFixed(63, 30, "Disabled", STYLE_BOLD);
      if(value == 1) { 
        ssd1306_drawRect(2, 20, 61, 45);
      } 
      if (value == 0) { 
        ssd1306_drawRect(61, 20, 127, 45);
      }
      ssd1306_setFixedFont(ssd1306xled_font6x8);
      if (menuItem == 2) 
      { 
        ssd1306_printFixed(4, 52, "Auto purge mode", STYLE_NORMAL);
      }
      if (menuItem == 7) 
      {
        ssd1306_printFixed(4, 52, "Press button=purge", STYLE_NORMAL);
      }
    }
    if (menuItem == 3) //Calibrate
    { 
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_printFixed(20, 30, "Exit", STYLE_BOLD);
      ssd1306_printFixed(70, 30, "GO!", STYLE_BOLD);
      if(value == 0) { 
        ssd1306_drawRect(15, 20, 55, 45);
      } 
      if (value == 1) { 
        ssd1306_drawRect(60, 20, 100, 45);
      }
    }
    if (menuItem == 4 || menuItem == 5)  //Max & min RPM
    { 
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_setFixedFont(ssd1306xled_font6x8);
      ssd1306_printFixed(4, 52, "match inverter", STYLE_NORMAL);
      ssd1306_setFixedFont(courier_new_font11x16_digits);
      itoa(value, showValue, 10);
      ssd1306_printFixed(64 - strlen(showValue)*6, 30, showValue, STYLE_BOLD);
    }
    if (menuItem == 6)  // Sleep time
    {                 
      itoa(value, showValue, 10);
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_printFixed(60 + strlen(showValue)*6, 30, "min", STYLE_BOLD);
      
      ssd1306_setFixedFont(ssd1306xled_font6x8);
      ssd1306_printFixed(4, 52, "set 0 to disable", STYLE_NORMAL);
      
      ssd1306_setFixedFont(courier_new_font11x16_digits);
      ssd1306_printFixed(55 - strlen(showValue)*6, 30, showValue, STYLE_BOLD);
    }
    if (menuItem == 8 || menuItem == 9)  //Purge Frames (low & high)
    { 
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_setFixedFont(ssd1306xled_font6x8);
      ssd1306_printFixed(4, 52, menuItem == 8? "=< 500 RPM" : "> 500 RPM", STYLE_NORMAL);
      ssd1306_setFixedFont(courier_new_font11x16_digits);
      itoa(value, showValue, 10);
      ssd1306_printFixed(64 - strlen(showValue)*6, 30, showValue, STYLE_BOLD);
    }

    if (menuItem == 10 || menuItem == 11)  //Purge Prct (low & high)
    { 
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_setFixedFont(ssd1306xled_font6x8);
      ssd1306_printFixed(4, 52, menuItem == 10? "=< 500 RPM" : "> 500 RPM", STYLE_NORMAL);
      ssd1306_setFixedFont(courier_new_font11x16_digits);
      itoa(value, showValue, 10);
      ssd1306_printFixed(64 - strlen(showValue)*6, 30, showValue, STYLE_BOLD);
    }

    if (menuItem == 12 || menuItem == 13)  //Purge delay & time
    { 
      itoa(value, showValue, 10);
      ssd1306_setFixedFont(ssd1306xled_font8x16);
      ssd1306_printFixed(64 - strlen(menuItems[menuItem])*4, 4, menuItems[menuItem], STYLE_NORMAL);
      ssd1306_printFixed(60 + strlen(showValue)*6, 30, "ms", STYLE_BOLD);
      
      ssd1306_setFixedFont(courier_new_font11x16_digits);
      ssd1306_printFixed(55 - strlen(showValue)*6, 30, showValue, STYLE_BOLD);
    }
  }

}

void Display::printError(uint8_t errorcode) { 
  lastPrintID = 3;   
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font8x16);
  ssd1306_printFixed(44, 10, "ERROR", STYLE_NORMAL);  
  ssd1306_printFixed(64 - strlen(statusCodes[errorcode]) * 4, 30, statusCodes[errorcode], STYLE_NORMAL);
}

void Display::printInfo(uint8_t infocode) { 
  lastPrintID = 7;
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font8x16);
  ssd1306_printFixed(64 - strlen(infoCodes[infocode])*4, 20, infoCodes[infocode], STYLE_NORMAL);  
}
 

void Display::printCalibrate(uint16_t setSpeed, uint16_t speed, uint16_t current) { 
if(lastPrintID != 6) {
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font8x16);
  ssd1306_printFixed(25, 4, "Calibrating", STYLE_NORMAL);

  ssd1306_printFixed(4, 16, "sRPM", STYLE_NORMAL);
  ssd1306_printFixed(4, 32, "RPM", STYLE_NORMAL);
  ssd1306_printFixed(4, 48, "mA", STYLE_NORMAL);

  lastPrintID = 6;
  }

  ssd1306_setFixedFont(ssd1306xled_font8x16);

  ssd1306_printFixed(45,16, "     ", STYLE_NORMAL);
  ssd1306_printFixed(45,32, "     ", STYLE_NORMAL);
  ssd1306_printFixed(45,48, "     ", STYLE_NORMAL);

  char showRpm[5];
  itoa(speed, showRpm, 10);
  char showAmp[5];
  itoa(current, showAmp, 10);
  char showSetRpm[5];
  itoa(setSpeed, showSetRpm, 10);

  ssd1306_printFixed(45,16, showSetRpm, STYLE_NORMAL);
  ssd1306_printFixed(45,32, showRpm, STYLE_NORMAL);
  ssd1306_printFixed(45,48, showAmp, STYLE_NORMAL);
}