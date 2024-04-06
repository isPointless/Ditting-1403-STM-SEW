#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FlashStorage_STM32.h>
#include "definitions.h"
#include "sew.h"
#include "display.h"
#include "io.h"

// *** stuff *** //
// ************* //
// void readEEPROM();
// void writeToEEPROM(uint8_t what);

SEW *sew;
Display *display;
IO *io;

// // EEPROM
// int16_t currentArray[((absolute_max_rpm-absolute_min_rpm)/rpmScalar)];
// int eeAddress = 0;
// int highAddress = 100;
// u_int16_t storedRPM = 0;

//PIs
int16_t motorRPM = 0;
int16_t motor_current = 0;
int16_t test_current = 0;

//POs
int16_t RPMprct = 0; //set RPM in % of max (p302). with max being 0x4000, this is how you control the motor RPM.
int16_t last_rpm = 0;
int16_t last_current;
uint16_t last_setRPM = 0;


unsigned long lastSend = 0; //start after 1s.
unsigned long lastActivity = 0;
unsigned long startTime = 0;
bool dataAvailable = false;

enum States {IDLE, RUNNING, MENU, SLEEP, PURGING};
uint8_t state = IDLE;
uint8_t lastStatus = 0;


enum menuItems {EXIT, VIEW, PURGE, CALIBRATE, SETMAX, SETMIN, SETSLEEP, SETBUTTONPURGE};
int8_t menu = EXIT;
bool selected = false;
int16_t value = 0;
int16_t lastNavigate = 0;
int16_t lastCount = 0;
int16_t change = 0;
uint8_t frameCounter;
bool ready_purge = 0;
bool newMsg = 0;


//DEBUG 
// #define DEBUG
// #define DEBUG_RS485
// #define DEBUG_IO
// #define DEBUG_CALIBRATE

// *** SETUP *** //
// ************* //

void setup() {
  #ifdef DEBUG
  SerialUSB.begin(115200); //USB-CDC (Takes PA8,PA9,PA10,PA11)
  #endif
  display = new Display();
  sew = new SEW();
  io = new IO();

  pinMode(relay_pin, OUTPUT);
  pinMode(power_relay_pin, OUTPUT);

  // see sew.h for meaning of control bits. Result of bitset below is 0x06, which is the most basic 'enable' signal
  sew->controller_inhibit = false; //enable controller 
  sew->controller_rapidStop = false;
  sew->controller_stop = false;
    //       bit 3 = 0
  sew->set_ramp = false;
  sew->set_param = false;
  sew->controller_reset = false;
  sew->direction = false;

  io->ledAction(1);
}

// *** Loop ***  //
// ************* //

void loop() 
{
  // SEND
  sew->sendSEW(1, RPMprct, 0, msgInterval);
  // RECEIVE (continuous)
  if(sew->receiveSEW() == true)   //correctly received a new msg from sew
  { 
    #ifdef DEBUG_RS485
    SerialUSB.print("Received, delta: "), SerialUSB.print(sew->dataDiff), SerialUSB.print(" corrupted: "), SerialUSB.println(sew->corrupt_counter);
    #endif
    // Newdata 
    motorRPM = sew->PI2/5;               // 1 digit = 0.2 rpm
    motor_current = (sew->PI3*Inom);      // 1 digit = 0.1% of nominal current -> here in mA.

    if(motorRPM != last_rpm || motor_current != last_current) 
    { 
      dataAvailable = true;
      last_rpm = motorRPM;
      last_current = motor_current;
    }
    newMsg = true;
  }

  if(sew->status != lastStatus)   // here because this doesn't need a correct msg (timeout disconnected)
  { 
    lastStatus = sew->status;
    dataAvailable = true;
  }

  // SLEEP ?
  if(lastActivity + (io->sleep_time_min)*60000 < millis() && io->sleep_time_min > 0)
  { 
    state = SLEEP;
    digitalWrite(relay_pin, LOW);
    sew->controller_inhibit = true; //inhibit controller 
    sew->controller_rapidStop = false;
    sew->controller_stop = false;
    while(!sew->sendSEW(1, 0, 0, minMsgInterval));
    digitalWrite(power_relay_pin, LOW); //turn off power to inverter
    io->ledAction(0);
  }

  // *** IDLE ***  //
  if(state == IDLE) {
      sew->controller_inhibit = false; //disable controller 
      sew->controller_rapidStop = false;
      sew->controller_stop = false;
      digitalWrite(relay_pin, LOW);
      io->ledAction(3);

      if(io->setRPM() != last_setRPM) 
      {
        RPMprct= io->setRPM()/float(io->maxRPM)*(0x4000);
        last_setRPM = io->setRPM();
        dataAvailable = true;
        lastActivity = millis();
        #ifdef DEBUG_IO
          SerialUSB.print("new rpm: "), SerialUSB.println(last_setRPM);
        #endif
      }
   
      if(dataAvailable == true)  //Show it on the screen
      { 
        io->viewmode? display->printRPM(sew->status == 5? motorRPM : io->setRPM(), sew->status) : display->printData(sew->status == 5? motorRPM : io->setRPM(), motorRPM, motor_current, sew->status);  
        dataAvailable = false;
      }

      if (io->ENC_BUTTON() == true) { 
        state = MENU;
        menu = EXIT;
        dataAvailable = true;
        lastActivity = millis();
      }

      if (io->START_BUTTON() == true) 
      { 
        if(sew->status != 3) { 
          display->printError(sew->status);
        } else 
        {
          digitalWrite(relay_pin, HIGH);   //enable physical controller IO.
          sew->controller_inhibit = false; //enable controller 
          sew->controller_rapidStop = true;
          sew->controller_stop = true;
          dataAvailable = true;
          lastActivity = millis();
          state = RUNNING;
          startTime = millis();
          frameCounter = 0;
          ready_purge = false;
        }
      }
  }
    // *** RUNNING ***  //
  else if (state == RUNNING) {

    //Flash responsively based on auto purge.
    if (ready_purge == false) { 
      frameCounter > 1? io->ledAction(4) : io->ledAction(2);
    } else {
      io->ledAction(5);
    }

    if(io->setRPM() != last_setRPM) 
      {
        RPMprct= io->setRPM()/float(io->maxRPM)*(0x4000);
        last_setRPM = io->setRPM();
        dataAvailable = true;
        lastActivity = millis();
        frameCounter = 0;
        ready_purge = false;
      }
  
    if(dataAvailable == true)  //Show it on the screen
      { 
        io->viewmode? display->printRPM(motorRPM, sew->status) : display->printData(io->setRPM(), motorRPM, motor_current, sew->status);  
        dataAvailable = false;
      }
    
    if(io->buttonPurgeMode == false) {
      if (io->ENC_BUTTON() == true || (io->START_BUTTON() == true)) 
      { 
        sew->controller_inhibit = false; //disable controller 
        sew->controller_rapidStop = false;
        sew->controller_stop = false;
        digitalWrite(relay_pin, LOW);
        while(!sew->sendSEW(1, 0, 0, minMsgInterval)); 
        state = IDLE;
        dataAvailable = true;
        lastActivity = millis();
      }
    } 
    if(io->buttonPurgeMode == true) 
    {
      if (io->START_BUTTON() == true) 
      { 
        RPMprct=(0x4000);  //setmax RPM
        startTime = millis();
        dataAvailable = true;
        frameCounter = 0; //should already be 0 but ok.
        state = PURGING;
      }

      if (io->ENC_BUTTON() == true)   //stop the show
      { 
        sew->controller_inhibit = false; //disable controller 
        sew->controller_rapidStop = false;
        sew->controller_stop = false;
        digitalWrite(relay_pin, LOW);
        while(!sew->sendSEW(1, 0, 0, minMsgInterval)); 
        state = IDLE;
        dataAvailable = true;
        lastActivity = millis();
      }
    }

    if(io->purgeMode == true && newMsg == true && startTime + 4000 < millis() && lastActivity + 1000 < millis()) 
    { 
      newMsg = false;
      test_current = io->currentArray[((io->setRPM()-absolute_min_rpm)/rpmScalar)]*Inom;

      #ifdef DEBUG_CALIBRATE
      SerialUSB.print("test cur: "), SerialUSB.print((test_current*test_scalar)/100), SerialUSB.print(" m_cur: "), SerialUSB.println(motor_current);
      #endif

      if(motor_current > (test_current*test_scalar)/100) { //x% above test current
        frameCounter++;
        #ifdef DEBUG_CALIBRATE
          SerialUSB.print("frame counter: "), SerialUSB.println(frameCounter);
        #endif
      } else { 
        frameCounter = 0;
      }

      if(frameCounter >= purge_frames)  //if atleast x consecutive frames are y% above test current
      { 
        #ifdef DEBUG_CALIBRATE
        SerialUSB.println("ready for purge");
        #endif
        ready_purge = true;
        lastActivity = millis();
      }
      
      if(ready_purge == true && (lastActivity + purge_delay) < millis()) 
      { 
        #ifdef DEBUG_CALIBRATE
        SerialUSB.println("go purge");
        #endif
        RPMprct= (0x4000);  //setmax RPM
        startTime = millis();
        dataAvailable = true;
        ready_purge = false;
        frameCounter = 0; //should already be 0 but ok.
        state = PURGING;
        }
    }
    
  } 
  // *** PURGING ***  //
  else if (state == PURGING) 
  { 
    if (io->ENC_BUTTON() == true || io->START_BUTTON() == true || (startTime + purge_time) < millis()) 
    { 
      sew->controller_inhibit = false; //disable controller 
      sew->controller_rapidStop = false;
      sew->controller_stop = false;
      digitalWrite(relay_pin, LOW);
      RPMprct = io->setRPM()/float(io->maxRPM)*(0x4000);
      while(!sew->sendSEW(1, 0, 0, minMsgInterval)); 
      state = IDLE;
      dataAvailable = true;
    }

    if(dataAvailable == true)  //Show it on the screen
    { 
      lastActivity = millis();
      io->viewmode? display->printRPM(motorRPM, 6) : display->printData(io->setRPM(), motorRPM, motor_current, 6);  //show status = purging
      dataAvailable = false;
    }
    io->ledAction(1);
  }
  // *** MENU ***  //
  else if (state == MENU) {
    io->ledAction(0);
    sew->controller_inhibit = false; //disable controller 
    sew->controller_rapidStop = false;
    sew->controller_stop = false;
    digitalWrite(relay_pin, LOW);
      
    if (selected == false && io->ENC_BUTTON() == true) 
    { 
      if (menu == 0) {       //exit menu
        state = IDLE;
        dataAvailable = true;
        return; 
      } else { 
      selected = true;
      menu == VIEW? (io->viewmode? value = 1 : value = 0) : 0;
      menu == PURGE? (io->purgeMode? value = 1 : value = 0) : 0;
      menu == CALIBRATE? value = 0 : 0;
      menu == SETMAX? value = io->maxRPM : 0;
      menu == SETMIN? value = io->minRPM : 0;
      menu == SETSLEEP? value = io->sleep_time_min: 0;
      menu == SETBUTTONPURGE? value = io->buttonPurgeMode: 0;
      }
      dataAvailable = true;
    }

    if (io->count() != lastCount) 
    { 
      change = io->count() - lastCount;
      lastCount = io->count();
      dataAvailable = true;
      if (selected == false) {    //Scrolling through menu
        menu = menu - change;
        menu < 0? menu = menuItemsCount : 0;
        menu > menuItemsCount? menu = 0 : 0;
      } 
      if (selected == true)
      {
        if(change != 0 && (menu == VIEW || menu == PURGE || menu == CALIBRATE || menu == SETBUTTONPURGE)) 
        { 
          value == 0? value = 1: value = 0; //value =!value doesnt work since its not a bool?
        } 
        if(menu == SETMAX || menu == SETMIN) { 
          value = value + change*rpmScalar;
          value >= absolute_max_rpm? value = absolute_max_rpm : 0;
          value <= absolute_min_rpm? value = absolute_min_rpm : 0;

        }
        if(menu == SETSLEEP) { 
          value = value + change;
          value >= max_sleep_time? value = max_sleep_time : 0;
          value < 0 ? value = 0 : 0;
        }
      }
    }

    if(selected == true && io->ENC_BUTTON() == true)   // SAVE STUFF
    { 
      if(menu == 3 && value == 1) {   //Calibration
        if (sew->calibrate(io->maxRPM) == true)   //Start calibration! stops loop for long time this way, but oh well
        { 
          display->printInfo(0);
          io->readEEPROM();
          state = IDLE;
          dataAvailable = true;
        } else { 
          #ifdef DEBUG
          SerialUSB.println("Calibrate failed");
          #endif
        }
        sew->controller_inhibit = false; //disable controller, double check
        sew->controller_rapidStop = false;
        sew->controller_stop = false;
        digitalWrite(relay_pin, LOW);
        lastActivity = millis();        
      } else {  //everything
        menu == 1? io->viewmode = value : 0;
        menu == 2? io->purgeMode = value : 0;
        menu == 4? io->maxRPM = value : 0;
        menu == 5? io->minRPM = value : 0;
        menu == 6? io->sleep_time_min = value : 0;
        menu == 7? io->buttonPurgeMode = value : 0;
        io->writeToEEPROM(menu);
        dataAvailable = true;
      }
      selected = false;
    }

    if(dataAvailable == true)  //Show it on the screen
    { 
      lastActivity = millis();
      display->printMenu(menu, selected, value);
      dataAvailable = false;
    }

  }

  // *** SLEEP ***  //
  else if(state == SLEEP) 
  { 
    lastCount = io->count();
    display->off();
    while(true) 
    { 
      if(io->ENC_BUTTON() == true || io->START_BUTTON() == true  || io->count() != lastCount) { 
        lastActivity = millis();
        lastCount = io->count();
        break;
      }
    }
    dataAvailable = true;
    state = IDLE;
    digitalWrite(power_relay_pin, HIGH);
  }

  // MORE LOOP STUFF 
  
  #ifdef DEBUG_RS485
  if(lastSend + 1000 < millis() )  // Send new msg every 100ms (this is more than enough to feel responsive)
  { 
    lastSend = millis();
    // Give back over USB serial
    SerialUSB.print(" RPMo: "), SerialUSB.print(RPMprct, HEX), SerialUSB.print(" RPMi: "), SerialUSB.print(sew->PI2);
    SerialUSB.print(" curr-in "), SerialUSB.println(sew->PI3);
  }
  #endif
}