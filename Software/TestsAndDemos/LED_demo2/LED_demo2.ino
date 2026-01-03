// Array of Enable Pins for the 4 Demuxes
const int LEDenPins[] = {4, 5, 66, 67}; 

// 2D Array for Address Pins: [Demux Number][A0, A1, A2, A3]
const int LEDaddrPins[4][4] = {
  {0, 1, 2, 3},     // Demux 0: PE0, PE1, PE4, PE5
  {38, 39, 40, 41}, // Demux 1: PD7, PG2, PG1, PG0
  {68, 69, 18, 14}, // Demux 2: PK6, PK7, PD3, PJ1
  {16, 17, 19, 15}  // Demux 3: PH1, PH0, PD2, PJ0
};

/**
 * Setup all LEDs before operation
 */
void LEDsetup() {
  //Initialize all Enable pins to HIGH (Disabled) first
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
    pinMode(LEDenPins[i], OUTPUT);
    
    //Initialize Address pins as OUTPUT
    for (int j = 0; j < 4; j++) {
      pinMode(LEDaddrPins[i][j], OUTPUT);
    }
  }
}

/**
 * The unified control function
 * @param x       X (0-7)
 * @param y       Y (0-7)
 * @param status  ON or OFF
 */
void setLED(int x, int y, bool status) {
  y=7-y;
  //Logic: Map x,y to 1 of 4 demuxes and 1 of 16 pins
  int demuxIndex = x / 2;
  int pinAddress = (x % 2) * 8 + y;

  //Always disable all first for safety and to avoid ghosting
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
  }

  if (status) {
    //1. Set the 4-bit address on the specific demux pins
    for (int i = 0; i < 4; i++) {
      digitalWrite(LEDaddrPins[demuxIndex][i], (pinAddress >> i) & 0x01);
    }
    //2. Enable only that demux
    digitalWrite(LEDenPins[demuxIndex], LOW);
  }
}





void setup() {
  LEDsetup();
}

void loop() {
  // :)
  setLED(1, 1, true);
  setLED(6, 1, true);

  setLED(1, 5, true);
  setLED(6, 5, true);
  for (int i = 2; i<6; i++){
    setLED(i, 6, true);
  }
}