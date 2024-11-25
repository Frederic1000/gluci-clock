// Parameters for 16x2 LCD use with ESP32
// and displaying arrows

#include <LiquidCrystal.h>

/* 16x2 LCD wiring:
   LCD pins (right to left) - ESP32 pins
   _____________________________________
   1 GND supply             - GND
   2 VDD 5v                 - 5V
   3 Vo contrast adjustment - 16 (PWM)
   4 RS register select     - 22
   5 R/W read/write         - GND
   6 En Enable Signal       - 21
   7 DB0 Data Bit 0         - unused
   8 DB1 Data Bit 1         - unused
   9 DB2 Data Bit 2         - unused
  10 DB3 Data Bit 3         - unused
  11 DB4 Data Bit 4         -  5
  12 DB5 Data Bit 5         - 18
  13 DB6 Data Bit 6         - 23
  14 DB7 Data Bit 7         - 19
  15 +5V backlight optional (16 pins LCDs)
  16 GND backlight optional (16 pins LCDs)
*/


// PWM configuration for LCD contrast
const int pwmChannel = 0;   //Channel for PWM (0-15)
const int frequency = 1000; // PWM frequency 1 KHz
const int resolution = 8;   // PWM resolution 8 bits, 256 possible values
const int pwmPin = 16;
const int contrast = 75;    // 0-255


// 16x2 LCD
const int rs = 22, en = 21, d4 = 5, d5 = 18, d6 = 23, d7 = 19;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


// Arrows for glucose tendency: Flat; FortyFiveDown, SingleDown, DoubleDown, FortyFiveUp, SingleUp, DoubleUp
byte flat[8] = {
  B00000,
  B00100,
  B00010,
  B11111,
  B00010,
  B00100,
  B00000,
};

byte fourtyFiveDown[8] = {
  B00000,
  B10000,
  B01001,
  B00101,
  B00011,
  B01111,
  B00000,
};

byte singleDown[8] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100,
};

byte doubleDown[8] = {
  B01010,
  B01010,
  B01010,
  B01010,
  B10001,
  B01010,
  B00100,
};

byte fourtyFiveUp[8] = {
  B00000,
  B01111,
  B00011,
  B00101,
  B01001,
  B10000,
  B00000,
};

byte singleUp[8] = {
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B00100,
  B00100,
};

byte doubleUp[8] = {
  B00100,
  B01010,
  B10001,
  B01010,
  B01010,
  B01010,
  B01010,
};

void initialize_LCD(){
  Serial.println();
  Serial.println("[DISPLAY] Initializing LCD");
  // LCD Contrast
  ledcSetup(pwmChannel, frequency, resolution);
  ledcAttachPin(pwmPin, pwmChannel);
  ledcWrite(pwmChannel, contrast);
  //LCD arrows
  lcd.createChar(0, flat);
  lcd.createChar(1, fourtyFiveDown);
  lcd.createChar(2, singleDown);
  lcd.createChar(3, doubleDown);
  lcd.createChar(4, fourtyFiveUp);
  lcd.createChar(5, singleUp);
  lcd.createChar(6, doubleUp);
  
  lcd.begin(16,2); // columns, rows of the LCD, zero index( 0-15, 0-1 )
}
