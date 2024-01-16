#pragma once
#include "Arduino.h"
#include "definitions.h"

// settings
#define SD1     0x02    //  SEW SD1 (Master -> Inverter)
#define SD2     0x1D    //  SEW SD2 (Inverter -> Master)

/* 
My SEW is an MC07B0015-2B1-4-00 (1.5kW, 220V 1 phase) equipped with both the 'Basic unit'/FBG11B and FSC11B comm module.
MAX485 RS485 is connected (including ground, in my case, wise or not), to the X45 terminal on the FSC11B (comm module is necessary)

More detailed explanation of the RS485 Movilink protocol: https://download.sew-eurodrive.com/download/pdf/11264926.pdf 

Note, more complex communication is possible through CAN, and the rest of the Movidrive protocol (parameter settings for ex, by changing the message type)
but not necessary for my goal of controlling one motor with one drive by one Arduino. 

For this to work, parameter 100 (2) and 101 (1) must be set to RS485 on the inverter.
Further take note of: Setting PO1, PO2, PO3 variables (p870-872) correctly, making sure PO data = enabled (P876 = YES, this changes everytime you change p870-p872)

P302 (n_max on the FBG11B) = max speed, or if you use the PO max speed, that.

My unit had a fault on DI 04, which corresponds to using internal fixed setpoint N11, it's always HIGH for some reason. 
You can re-set the definition of this in the P60x settings, i just didn't link it to any physical DI.

You can check actual DI input levels in P039 

Apart from setting and receiving correct messages, you HAVE TO also use the physical terminal connections to enable / disable the unit. 
By default this means DI 01 and DI 03 must be HIGH (preferably still by the Arduino to prevent any comm issues not stopping the motor, at  your own risk)

The 24VIO can be used for this aswell.
for testing purposed i connected a switch up, as 'shorting' it to the screwhead on top was VERY instable as you need to press really hard for it to make connection (?)

THE MOVILINK PROTOCOL:

All 'words' are actually a word. 16 bit / 2 byte (control word & status word and process output/PO and process input/PI words)
All messages most significant byte first / MSB first, using a parity bit and start/stop (like UART protocol). I use two stop bits for <insert reason>

I've only implemented the cyclical Process Word communication, not yet the cyclical or acyclical parameter settings. 

A standard cyclical message:
__ start pause __ (3.5ms min)
Byte1: Start delimiter (Request = 0x02, response = 0x1D)
Byte2: Slave ADDR (0-99 for single addressing, 254 for universal point to point, see PDF for more)
Byte3: TYPE / PDU type (0x05 for 3 Process words, 0x06 for 8 byte parameter channel, x04 for 3 PWs + 8byte param channel) all in CYCLIC MODE
Byte4+5: PD1 (assume TYPE 0x05) so either a PO or PI (set value in page 330 SEW MC07b manual --> Parameter 870-876, i use DEFAULT settings)
Byte6+7: PD2 
Byte8+9: PD3
Byte10: BCC (Block Check Character, makes all bits in the same position of the bytes combined an EVEN number) 

Example data for BCC:
1 0 0 1 0 1 
1 1 0 0 1 1
1 0 0 1 0 0
Then the BCC is: making every column of bits even:
1 1 0 0 1 0
This is achieved by the XOR bitwise operator (^).
Note that the PDs are words, and for the BCC split up into two bytes. 

Practical parameters to check on the device to see if its working:
- P094 = PO1 / received control word
- P095 = PO2 / received PO2 / speed command
- P096 = PO3 / unused but now you know
- P039 for actual DI status (low or high lines)
*/

class SEW
{ 
  private:
  unsigned long lastSend = 0;
  uint8_t state = 0;
  int highAddress = 100;

  public:
  SEW(); //constructor

  // Control word bools
  bool controller_inhibit; //control word1 bit 0 --> Controller inhibit = "1" / enable = "O"
  bool controller_rapidStop; //control word1 bit 1 --> Enable = "1" / rapid stop = "O"
  bool controller_stop; //control word1 bit 2 --> Enable = "1" / stop = "0"
  bool set_ramp; // control word 1 bit 4 --> 1 = Ramp generator selection: Integrator 1 = "1" / integrator 2 = "O"
  bool set_param; // control word 1 bit 5 --> 1 = Parameter set switchover: Parameter set 2 = "1" / parameter set 1 = "O"
  bool controller_reset; //control word1 bit 6 --> Reset: reset pending fault = "1" / not active = "O" --> needs 0/1 edge to enable.
  bool direction; //control word1 bit 7 --> 0 = CW, 1 = CCW
  // status word bools
  bool output_enabled; //status word1 bit 0 --> Output stage enabled "1" / output stage inhibited "O"
  bool inverter_ready; //status word1 bit 1 --> Inverter ready "1" / inverter not ready "O"
  bool PO_data_enabled; //status word1 bit 2 --> PO data enabled "1" / PO data disabled "O"
  bool c_ramp_set;      //status word1 bit 4 --> Current ramp generator set: Integrator 2 "1" / integrator 1 "O"
  bool c_param_set;     // status word1 bit --> Current parameter set: Parameter set 2 "1" / parameter set 1 "O"
  bool fault_warning; //status word1 bit 5 --> Fault/warning: Fault/warning pending "1" / no fault "O"

  uint8_t corrupt_counter = 0;
  int8_t dataDiff = 0;

  enum Status {disconnected, notReady, faultTrue, ready, warning, enabled}; //combination of SW bit 1 and bit 5
  Status status = disconnected;

  uint8_t faultcode; //high byte status word1, find cause in p260 of the manual.
  // Receive data (receiveSEW puts it in here)

  int16_t PI2 = 0;
  int16_t PI3 = 0;

  bool sendSEW(uint8_t SA, int16_t Speed, int16_t PO3, u_int8_t interval); //Cyclic data without parameter 
  bool receiveSEW(); //returns true when new msg received
  bool calibrate(uint16_t maxRPM);  //calibrate the current at all RPM settings.

  // RS485 functions
  
  inline void setTXmode() { digitalWrite(RS485DE, HIGH); };
  inline void setRXmode() { digitalWrite(RS485DE, LOW); };

  int16_t calibrateArray[((absolute_max_rpm-absolute_min_rpm)/rpmScalar)] = {0};

};

