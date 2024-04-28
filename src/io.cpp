#include "definitions.h"
#include <Arduino.h>
#include "io.h"
#include <Arduino.h>
#include "FlashStorage_STM32.hpp"
#include <JC_Button.h>
#include <math.h>

Button enc_button(enc_button_pin);
Button start_button(start_button_pin);
HardwareTimer *myEncoder;

// #define DEBUG_IO
// #define DEBUG_CALIBRATE

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

  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + sizeof(uint8_t), purge_framesLow);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 2*sizeof(uint8_t), purge_framesHigh);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 3*sizeof(uint8_t), purge_prctLow);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 4*sizeof(uint8_t), purge_prctHigh);
  EEPROM.get(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 5*sizeof(uint8_t), purge_delay);
  EEPROM.get(eeAddress + 4* sizeof(u_int16_t) + 3*sizeof(bool) + 5*sizeof(uint8_t), purge_time);

  #ifdef DEBUG_IO
  SerialUSB.print("Min RPM: "), SerialUSB.println(minRPM);
  SerialUSB.print("RPM_SET: "), SerialUSB.println(RPM_SET);
  SerialUSB.print("max RPM: "), SerialUSB.println(maxRPM);
  #endif

  for(u_int16_t i = 0; i<(sizeof(currentArray))/sizeof(uint16_t); i++) { 
  EEPROM.get(highAddress + 2*i, currentArray[i]);
  #ifdef DEBUG_CALIBRATE
  SerialUSB.print("read: "), SerialUSB.println(currentArray[i]);
  #endif
  }

  if(maxRPM > absolute_max_rpm || maxRPM < absolute_min_rpm || minRPM > absolute_max_rpm || minRPM < absolute_min_rpm || RPM_SET < minRPM || RPM_SET > maxRPM) {
    RPM_SET = RPM_DEFAULT;
    writeToEEPROM(0);
    viewmode = default_viewmode;
    writeToEEPROM(1);
    purgeMode = default_purgemode;
    writeToEEPROM(2);
    maxRPM = maxRPM_DEFAULT;
    writeToEEPROM(4);
    minRPM = minRPM_DEFAULT;
    writeToEEPROM(5);
    sleep_time_min = default_sleep_time_min;
    writeToEEPROM(6);
    buttonPurgeMode = default_buttonpurgemode;
    writeToEEPROM(7);
    purge_framesLow = default_purge_frames;
    writeToEEPROM(8);
    purge_framesHigh = default_purge_frames;
    writeToEEPROM(9);
    purge_prctLow = default_purgeprct;
    writeToEEPROM(10);
    purge_prctHigh = default_purgeprct;
    writeToEEPROM(11);
    purge_delay = default_purge_delay;
    writeToEEPROM(12);
    purge_time = default_purge_time;
    writeToEEPROM(13);
  }

  #ifdef DEBUG_IO
  SerialUSB.println("eeprom read");
  #endif

  encoderInit();  //eeprom reading breaks the encoder.. whatever.
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

// 0 = RPM_SET, 1 = viewmode, 2 = purgemode, 3 = calibration, 4 = maxrpm, 5 = minrpm, 6 = sleeptime, 7 = buttonpurgemode, 8 = framesLow, 9 = framesHigh, 10 = prctLow, 11 = prctHigh, 12 = purgeDelay, 13 = purgeTime
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
if (what == 8) {
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + sizeof(uint8_t), purge_framesLow);
} 
if (what == 9) { 
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 2*sizeof(uint8_t), purge_framesHigh);
} 
if (what == 10) {
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 3*sizeof(uint8_t), purge_prctLow);
}
if (what == 11) { 
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 4*sizeof(uint8_t), purge_prctHigh);
}
if (what == 12) {
  EEPROM.put(eeAddress + 3* sizeof(u_int16_t) + 3*sizeof(bool) + 5*sizeof(uint8_t), purge_delay);
}
if (what == 13) {
  EEPROM.put(eeAddress + 4* sizeof(u_int16_t) + 3*sizeof(bool) + 5*sizeof(uint8_t), purge_time);
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

void IO::ledAction(uint8_t type) {  //Type: 0 = off, 1 = on, 2 = flashing, 3 = fading, 4 flashing fast, 5 = flashing fastest
static uint8_t lastType = 0;
static unsigned long lastFlash = millis();

  if (type == 0) { 
    analogWrite(start_button_led, 0);
  }

  if (type == 1) {
    analogWrite(start_button_led, 255);
  }

  //flashing
  if (type == 2 || type == 4 || type == 5) { 
    static bool ledOn = true;

    if(lastType != type) 
    { 
      ledOn = true;
      analogWrite(start_button_led, 255);
      lastFlash = millis();
    }


      if(lastFlash + (type == 2? flashTime : type == 4? fastFlashTime : fastestFlashTime) < millis()) { 
        ledOn = !ledOn;
        analogWrite(start_button_led, ledOn == true? 255 : 0);
        lastFlash = millis();
      }
  }

  if (type == 3) { //fading
    static double cosValue = 0;
    static uint8_t ledValue = 0;
    static uint8_t lastLedValue = 0;

    if (lastType != type) { 
      lastType == 1? cosValue = 2*PI : cosValue = PI;
      lastFlash = millis();
    }

    cosValue > 3*PI? cosValue = PI : 0;
    cosValue += ((millis() - lastFlash)/(float)fadeTime) * PI;
    ledValue = ((cos(cosValue) + 1) * 255) / 2;

    lastFlash = millis();

    if(lastLedValue != ledValue) { 
      analogWrite(start_button_led, ledValue);
      lastLedValue = ledValue;
    }
  }

  lastType = type;
}
