#include <Arduino.h>
#include "sew.h"
#include <HardwareSerial.h>
#include "io.h"
#include "display.h"
#include "FlashStorage_STM32.hpp"

//#define DEBUG_CALIBRATE

HardwareSerial Serial2(PA3, PA2); 
IO *io2;
Display *display2;

#define DEBUG_CALIBRATE

SEW::SEW() { 
  Serial2.begin(9600, SERIAL_8E2);  // PA2(TX) & PA3(RX)
  pinMode(RS485DE, OUTPUT);
  setRXmode();
}


bool SEW::sendSEW(uint8_t SA, int16_t Speed, int16_t PO3, u_int8_t interval) // (1) Slave address, (2) Speed% (0-0x4000), (3) PO3 setpoint (undefined). Control word 1 (=PO1) meaning see sew.h
{ 
  if(lastSend + interval < millis()) 
  {
    uint8_t BCC = 0;
    uint8_t CW1 = 0;
    uint8_t byte5 = 0;
    uint8_t byte6 = 0;
    uint8_t byte7 = 0;
    uint8_t byte8 = 0;
    setTXmode();                 //  transmit mode
    delayMicroseconds(3500);     // delayMicroseconds because this is not interrupt based. Which leaves you free to do other interrupt stuff in the meantime
    // Byte 0
    Serial2.write((uint8_t)SD1); 
    BCC = BCC ^ SD1;
    // Byte 1
    Serial2.write((uint8_t)SA);
    BCC = BCC^SA;
    // Byte 2
    Serial2.write((uint8_t)0x05);
    BCC = BCC^(uint8_t)0x05;
    // Byte 3, Most significant byte of CW1 = 0 - this is unused for now. Only the 'basic status block'ie low byte is used
    Serial2.write((uint8_t)0x00);
    // Byte 4, CW1
    CW1 = 0x00; // set 0
    controller_inhibit? bitSet(CW1, 0): bitWrite(CW1, 0, 0);
    controller_rapidStop? bitSet(CW1, 1): 0;
    controller_stop? bitSet(CW1, 2): 0;
    set_ramp? bitSet(CW1, 4): 0; // bit 4 (3 is reserved)
    set_param? bitSet(CW1, 5): 0; // bit 5                          
    controller_reset? bitSet(CW1, 6): 0; //bit 6 
    direction? bitSet(CW1, 7) : 0; // CW or CCW, not sure this works *manual says* "direction of motor potentiometer"

    Serial2.write(uint8_t(CW1));
    BCC ^= uint8_t(CW1); 

    // Byte 5, PO2 high
    byte5 = Speed >> 8;
    Serial2.write(uint8_t(byte5)); // high byte of PO2
    BCC ^= uint8_t(byte5);

    // Byte 6, PO2 low
    byte6 =(Speed & 0x00ff);
    Serial2.write(uint8_t(byte6)); // Low byte of PO2
    BCC ^= uint8_t(byte6);

    // Byte 7, PO3 high
    byte7 = (PO3 >> 8);
    Serial2.write(uint8_t(byte7)); // high byte of PO3
    BCC ^= uint8_t(byte7);

    // Byte 8, PO3 low
    byte8 = (PO3 & 0x00ff);
    Serial2.write(uint8_t(byte8)); // Low byte of PO3
    BCC ^= uint8_t(byte8);

    // Byte 9, the BCC
    Serial2.write(uint8_t(BCC));

    Serial2.flush(); //wait for stuff to flush the serial before changing to RX mode
    setRXmode();
    state = 0;      //Improves stability, this is the receive state
    dataDiff++;     //for debugging, see difference between send & received msg'es (should be 0)
    lastSend = millis();
    return true;
  } else { 
    return false;
  }
};


bool SEW::receiveSEW() //returns slave address, or 0 if no new data received.
{
  static unsigned long lastReceive = 0;
  static uint8_t BCC_in = 0;
  static uint8_t length = 0;
  static uint8_t buffer[6] = {0,0,0,0,0,0};
  static uint8_t bidx = 0;
  static uint8_t v = 0;

  if(lastReceive + 1000 < millis()) { 
    status = disconnected;
  }

  if (Serial2.available() == 0 && state != 4) return false;
  
  v = Serial2.read();

  switch(state)
  {
    //  waiting for new packet
    case 0:
      if (v == 29) //Msg from the inverter!
      {
        bidx = 0;
        for(int i=0;i<6; i++) buffer[i] = 0; 
        BCC_in = 0;
        BCC_in = BCC_in^v;   //in MoviLink everything counts towards the BCC.
        state = 1;
      }
      break;

    //  Check origin / slave ID --> always 1, cancel if not. This if for now mostly because the BCC or PO values can also correspond to SD1 or SD2.
    case 1:
      if(v != 1) { 
        state = 0;
      } else 
      { 
        BCC_in = BCC_in ^v;
        state = 2;
      }
    break;
    //  extract TYPE
    case 2:
      BCC_in = BCC_in ^v;
      if(v == uint8_t(0x05)) {   //Type = 5 standard cyclical message 3PD, 6 bytes msg, ALWAYS the case for me. so another check.
        length = 6; 
        state = 3;        
      } else 
      {      
        state = 0;
      }
      break;

    // //  MSG bytes
    case 3:
      if (length == 0)
      {
        //  expect checksum
        if (BCC_in == v) { 
          state = 4;      //parsing
        } else            //failed BCC, data corrupted
        {
          state = 0; 
          corrupt_counter++;
        }
        break;
      }
      BCC_in = BCC_in^v;
      buffer[bidx] = v;
      bidx++;
      length--;
      break;
    
  //   // Parsing
    case 4:
      //Assuming PI1 = status word 1, msg0 is high byte of status word and thus the 'faultcode'
      faultcode = buffer[0]; 

      lastReceive = millis();

      output_enabled = bitRead(buffer[1], 0); // bit 0 MSB
      inverter_ready = bitRead(buffer[1], 1); // bit 1
      PO_data_enabled = bitRead(buffer[1], 2); //bit 2
      c_ramp_set = bitRead(buffer[1], 3); // bit 3
      c_param_set = bitRead(buffer[1], 4); // bit 4
      fault_warning = bitRead(buffer[1], 5); // bit 5, bit 6&7 reserved (for inter inverter comm?)

      PI2 = ((uint16_t)buffer[2] << 8) | buffer[3];
      PI3 = ((uint16_t)buffer[4] << 8) | buffer[5];
      
      if(inverter_ready == false) 
      { 
        status = fault_warning ? faultTrue : notReady;
      } else { 
        status = fault_warning ? warning : ready;
      } 

      if(status == ready && output_enabled == true) { 
        status = enabled;
      }

      dataDiff--;
      state = 0;

     return true;  //New data available

      //End of Switch
  }

  return false;

  } 

bool SEW::calibrate(uint16_t maxRPM) { 
  #ifdef DEBUG_CALIBRATE
  SerialUSB.println("calibrate!");
  #endif
  uint16_t speedPrct = 0;
  uint16_t speed = 0;
  uint16_t actualSpeed = 0;
  uint16_t displayCurrent = 0;
  uint8_t lastWritten = 0;
  unsigned long checkTime;

  if(status < 3) { 
    display2->printError(status);
    #ifdef DEBUG_CALIBRATE
    SerialUSB.print("calibration failed, status: "), SerialUSB.println(status);
    #endif
    delay(1000);
    return false;
  }

  digitalWrite(relay_pin, HIGH);   //enable physical controller IO.
  controller_inhibit = false; //enable controller 
  controller_rapidStop = true;
  controller_stop = true;

  for(u_int16_t i = 0; i < sizeof(calibrateArray)/sizeof(uint16_t); i++) {
    speed = i*rpmScalar + absolute_min_rpm;
    speedPrct = speed/float(maxRPM)*(0x4000);
    uint8_t counter = 0;
    uint32_t sum = 0;
    checkTime = millis();
    if(i > 0) {
    EEPROM.put(highAddress + sizeof(uint16_t)*(i-1), calibrateArray[i-1]); //this is really f'in slow so we do it during..
    lastWritten = i-1;
    #ifdef DEBUG_CALIBRATE
      SerialUSB.print("written: "), SerialUSB.println(i-1);
    #endif
    }
    while(1) 
    { 
      sendSEW(1, speedPrct, 0, msgInterval);
  
      if(receiveSEW() == true) {
        actualSpeed = PI2/5;               // 1 digit = 0.2 rpm
        displayCurrent = (PI3*Inom);    // 1 digit = 0.1% of nominal current -> here mA;
        display2->printCalibrate(speed, actualSpeed, displayCurrent);
        
        if(checkTime + 1500 < millis()) {  //allow 500ms of stabilizing
          counter++;
          sum = sum + PI3;
        }
      }

      if (io2->ENC_BUTTON() == true || io2->START_BUTTON() == true) 
      { 
        controller_inhibit = false; //disable controller 
        controller_rapidStop = false;
        controller_stop = false;
        digitalWrite(relay_pin, LOW);
        while(sendSEW(1, 0, 0, minMsgInterval) == false); //fast shutoff
        display2->printError(10);
        return false;
      }

      if (checkTime + 2500 < millis()) 
      { 
        break;
      }
    }
    #ifdef DEBUG_CALIBRATE
    SerialUSB.print("calibrated: "), SerialUSB.print(speed), SerialUSB.print("  sum: "), SerialUSB.print(sum), SerialUSB.print("  counter: "), SerialUSB.print(counter), SerialUSB.print("  current: "), SerialUSB.println((sum/counter)*Inom);
    #endif
    calibrateArray[i] = uint16_t(sum / counter);
    if (speed >= maxRPM) 
    { 
      break;
    }
  }
  //write last value
  //display2->printInfo(1);
  EEPROM.put(highAddress + sizeof(uint16_t)*(lastWritten+1), calibrateArray[lastWritten+1]); //this is really f'in slow so we do it during..
  #ifdef DEBUG_CALIBRATE
      SerialUSB.print("written: "), SerialUSB.print(lastWritten+1), SerialUSB.print(" RPM = "), SerialUSB.println((lastWritten+1)*rpmScalar + absolute_min_rpm);
  #endif
  controller_inhibit = false; //disable controller 
  controller_rapidStop = false;
  controller_stop = false;
  digitalWrite(relay_pin, LOW);
  delay(100);
  while(sendSEW(1, 0, 0, minMsgInterval) == false); //fast shutoff
  //io2->writeToEEPROM(3);
  return true;
}