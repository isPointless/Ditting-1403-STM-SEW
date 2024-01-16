#pragma once

class Adafruit_SSD1306;

class Display{
  private:
    Adafruit_SSD1306* display;
    uint8_t lastPrintID = 0; //1 = printData, 2 = printRPM, 3 = printError, 4 = Menu, 5 = Off, 6 = printCalibrate

  public:
    Display();
    void printData(uint16_t setRPM, uint16_t rpm, uint16_t ampere, u_int8_t status); //ID = 1
    void printRPM(uint16_t rpm, u_int8_t status); //ID =2
    void printError(uint8_t errorcode); //ID =3 
    void printMenu(int8_t menuItem, bool selected, uint16_t value); //ID =4
    void off();  //ID = 5
    void printCalibrate(uint16_t setSpeed, uint16_t speed, uint16_t current); //ID = 6
    void printInfo(uint8_t infocode); //id = 7
};

extern Display *display;