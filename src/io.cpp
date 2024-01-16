#include "definitions.h"
#include <Arduino.h>
#include "io.h"
#include <Arduino.h>
#include "FlashStorage_STM32.hpp"
#include <JC_Button.h>

Button enc_button(enc_button_pin);
Button start_button(start_button_pin);
HardwareTimer *myEncoder;

#define DEBUG_IO
#define DEBUG_CALIBRATE


IO::IO() { 
  readEEPROM();
  storedRPM = RPM_SET;
  enc_button.begin();
  start_button.begin();
};

void IO::encoderInit() 
{ 
  TIM_TypeDef *Instance  = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(pinCLK), PinMap_PWM);
  uint32_t     channel_1 = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(pinCLK), PinMap_PWM));
  uint32_t     channel_2 = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(pinDT ), PinMap_PWM));

  myEncoder = new HardwareTimer(Instance);
  myEncoder->pause();
  myEncoder->setOverflow(65536  , TICK_FORMAT);
  myEncoder->setPrescaleFactor(1);
  myEncoder->setMode(channel_1, TIMER_INPUT_CAPTURE_FALLING, pinCLK);
  myEncoder->setMode(channel_2, TIMER_INPUT_CAPTURE_FALLING, pinDT );

  Instance->SMCR |= TIM_ENCODERMODE_TI12;    // set encoder moder (counting both rising and falling edges on both channels) 
  myEncoder->setCount(2e6);  //somewhere in the middle
  myEncoder->resume();
  v = myEncoder->getCount()/ENC_TOL;
  lastv = v;
}

void IO::readEEPROM() { 
 // EEPROM or in STM32's case, simulated EEPROM of Flash. watch out for limited write cycles!
  EEPROM.get(eeAddress, RPM_SET);
  EEPROM.get(eeAddress + sizeof(u_int16_t), viewmode);
  EEPROM.get(eeAddress + sizeof(u_int16_t) + sizeof(bool), purgeMode);
  EEPROM.get(eeAddress + sizeof(u_int16_t) + 2*sizeof(bool), maxRPM);
  EEPROM.get(eeAddress + 2* sizeof(u_int16_t) + 2*sizeof(bool), minRPM);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 2*sizeof(bool), sleep_time_min);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 2*sizeof(bool) + sizeof(uint8_t), buttonPurgeMode);

  for(u_int16_t i = 0; i<(sizeof(currentArray))/sizeof(uint16_t); i++) { 
  EEPROM.get(highAddress + 2*i, currentArray[i]);
  #ifdef DEBUG_CALIBRATE
  SerialUSB.print("read: "), SerialUSB.println(currentArray[i]);
  #endif
  }

  if(maxRPM > absolute_max_rpm || maxRPM < absolute_min_rpm || minRPM > absolute_max_rpm || minRPM < absolute_min_rpm || RPM_SET < minRPM || RPM_SET > maxRPM) {
    maxRPM = maxRPM_DEFAULT;
    minRPM = minRPM_DEFAULT;
    RPM_SET = RPM_DEFAULT;
    viewmode = default_viewmode;
    purgeMode = default_purgemode;
    buttonPurgeMode = default_buttonpurgemode;
    sleep_time_min = default_sleep_time_min;
  }
  #ifdef DEBUG_IO
  SerialUSB.println("eeprom read");
  #endif
  encoderInit();  //eeprom reading breaks the encoder.. put too??
}

u_int16_t IO::setRPM() { 
  v = myEncoder->getCount()/ENC_TOL;  

  RPM_SET = RPM_SET + (v - lastv)*rpmScalar;
  lastv = v;

  if(RPM_SET <= minRPM) 
  { 
    RPM_SET = minRPM;
  } else if (RPM_SET >= maxRPM) 
  { 
    RPM_SET = maxRPM;
  }
  writeToEEPROM(0);
  return RPM_SET;
}

int16_t IO::count() { 
  static uint8_t count;
  v = myEncoder->getCount()/ENC_TOL;  

  count = count + (v - lastv);
  lastv = v;
  
  return count;
}

// 0 = RPM_SET, 1 = viewmode, 2 = purgemode, 3 = calibration, 4 = maxrpm, 5 = minrpm
void IO::writeToEEPROM(uint8_t what) {    
  static unsigned long lastChange = 0;
  static uint16_t lastRPM = RPM_SET;
  static bool newData = false;

if (what == 0) {  //set RPM
  if (RPM_SET != lastRPM) { 
    lastRPM = RPM_SET;
    lastChange = millis();
    newData = true;
  } else if(newData == true && lastChange + 10000 < millis()) {     //only after 10s it changes.

    if(storedRPM != RPM_SET) {      //If put back to the old RPM, it doesn't save to Flash. only 10k writes... that's only like 14 years of 2 writes per day.
    EEPROM.put(eeAddress, RPM_SET);  //Commented for  testing, no waste!
    storedRPM = RPM_SET;
    #ifdef DEBUG_IO
    SerialUSB.println("new rpm stored");
    #endif
    }
    newData = false;
  }
} 
if (what == 1) { //viewmode
  EEPROM.put(eeAddress + sizeof(u_int16_t), viewmode);
}
if (what == 2) { //purgemode
  EEPROM.put(eeAddress + sizeof(u_int16_t) + sizeof(bool), purgeMode);
}
if (what == 4) {
  EEPROM.put(eeAddress + sizeof(u_int16_t) + 2*sizeof(bool), maxRPM);
}
if (what == 5) {
  EEPROM.put(eeAddress + 2* sizeof(u_int16_t) + 2*sizeof(bool), minRPM);
}
if (what == 6) { 
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 2*sizeof(bool), sleep_time_min);
}
if (what == 7) { 
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 2*sizeof(bool) + sizeof(uint8_t), buttonPurgeMode);
}

#ifdef DEBUG_IO
  SerialUSB.print("put: "), SerialUSB.println(what);
#endif

}

bool IO::ENC_BUTTON() { 
  enc_button.read();
    if (enc_button.wasReleased()) { 
        return true;
    }
    return false;
}

bool IO::START_BUTTON() {
    start_button.read();
    if (start_button.wasReleased()) { 
        return true;
    }
    return false;
}