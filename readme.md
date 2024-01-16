# SEW Movidrive B MC07B inverter control using STM32F411CE Blackpill over RS485 / MAX485

Hey! I made a sketch to control a 3 phase Coffee grinder with an SEW Movidrive B MC07B over RS485 using the Movilink protocol.
This makes use of the FSC11b (or another) comms module, he MC07B base unit does NOT have RS485 capability. However these comm modules are very common.

The concept is simple, have a little screen set/realtime showing RPM, a rotary encoder to change it, a button to start it all up, and a menu to control some modes.
The purpose of this is to have a readout of the current going to the motor in Sensorless Vector Control (SVC), so we can sense when the motor is working thus actually grinding.

It can then, when grinding is finished, ramp up the RPM to the max setting to blow out any residual coffee grounds. Like the auto purge function found on the P100 from Option O.

### BOM

BOM: 
- SEW Movidrive B / MC07Bxxxx 
- STM32F411CE BlackPill We Act studio + STLink V2 (clone) + USB C cable for Serial (optional, but yeah, VERY practical for simple serial.print debugging later)
- MAX485 RS485 transceiver (the cheap kind)
- SSD1306 0.96" 128x64 i2c OLED display (optional, handy)
- Rotary Encoder with pushbutton
- Any momentary button (to ground)
- Your PC/Mac, with VSCode, PlatformIO, STM32CubeProgrammer, CH34xVCPDriver and the firmware on the STLink updated.

### How To?

- Wire up according to the definitions
- Flash using ST Link on PlatformIO
- Pray I wrote this well

### Good to know & setting up your Movidrive inverter

My SEW is an MC07B0015-2B1-4-00 (1.5kW, 220V 1 phase) equipped with both the 'Basic unit' / FBG11B and FSC11B comm module.
MAX485 is connected (including ground, in my case, wise or not), to the X45 terminal. 

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

The 24VIO can be used for this.
for testing purposed i connected a switch up, as 'shorting' it to the screwhead on top was VERY instable as you need to press really hard for it to make connection (?)

### The data

THE MOVILINK PROTOCOL:

All 'words' are actually a word. 16 bit / 2 byte (control word & status word and process output/PO and process input/PI words). High byte first. (MSB)
All bytes most significant bit first / MSB first, using a parity bit and start/stop (like UART protocol). I use two stop bits for 'reasons'

I've only implemented the cyclical Process Word communication, not yet the cyclical or acyclical parameter settings. 

A standard cyclical message:
__ start pause __ (3.5ms min)
- Byte1: Start delimiter (Request = 0x02, response = 0x1D)
- Byte2: Slave ADDR (0-99 for single addressing, 254 for universal point to point, see PDF for more)
- Byte3: TYPE / PDU type (0x05 for 3 Process words, 0x06 for 8 byte parameter channel, x04 for 3 PWs + 8byte param channel) all in CYCLIC MODE
- Byte4+5: PD1 (assume TYPE 0x05) so either a PO or PI (set value in page 330 SEW MC07b manual --> Parameter 870-876, i use DEFAULT settings)
- Byte6+7: PD2 
- Byte8+9: PD3
- Byte10: BCC (Block Check Character, makes all bits in the same position of the bytes combined an EVEN number) 

Example data for BCC:
- 1 0 0 1 0 1 
- 1 1 0 0 1 1
- 1 0 0 1 0 0
- Then the BCC is: making every column of bits even:
- 1 1 0 0 1 0
- This is achieved by the XOR bitwise operator (^).
Note that the PDs are words, and for the BCC split up into two bytes. 

Practical parameters to check on the device to see if its working:
- P094 = PO1 / received control word
- P095 = PO2 / received PO2 / speed command
- P096 = PO3 / unused but now you know
- P039 for actual DI status (low or high lines)

