
#define DISPLAY_ADDR 0x3C // I2C Address - use 0x3C or 0x3D depending on your display 

/* SSD1306 pinout
STM32F411	SSD1306 I2C OLED
----------------------------
3V3			VCC
GND			GND
B7			SDA
B6			SCL
*/

//Pinout for RS485 Serial2 == PA2(TX) & PA3(RX)
#define RS485DE      PB1     // This is both RE and DE together (high = TX)

#define pinCLK PA0           // Rotary encoder (uses hardware timer TIM2)
#define pinDT  PA1

#define enc_button_pin PC15
#define start_button_pin PA5
#define start_button_led PA6

#define relay_pin PC14
#define power_relay_pin PA7

// SEW Settings

#define Inom 7.3
#define minRPM_DEFAULT 250
#define maxRPM_DEFAULT 2200     //n_max on the SEW controller
#define absolute_max_rpm 3000
#define absolute_min_rpm 100
#define RPM_DEFAULT 1400
#define rpmScalar 25    //25 rpm increase per tick on the encoder

// RS485 x SEW stuff

#define msgInterval 100 //interval for sending SEW msg'es
#define minMsgInterval 60

//Purge stuff

#define default_purge_frames 5
#define default_purge_delay 1500
#define default_purge_time 3000
#define default_purgeprct 125 //110% of test current for purging

// Menu / IO stuff

#define fadeTime 1750
#define flashTime 250
#define fastFlashTime 150
#define fastestFlashTime 75

#define menuItemsCount 13
#define default_sleep_time_min 5
#define max_sleep_time 99
#define default_viewmode 1
#define default_purgemode 0
#define default_buttonpurgemode 1

#define ENC_TOL 4       //4 clicks per tick on the rotary encoder

#define baseAddress 0




