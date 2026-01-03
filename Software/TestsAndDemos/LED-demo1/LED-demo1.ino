// Array of Enable Pins for the 4 Demuxes
const int enPins[] = {4, 5, 66, 67}; 

// 2D Array for Address Pins: [Demux Number][A0, A1, A2, A3]
const int addrPins[4][4] = {
  {0, 1, 2, 3},     // Demux 0: PE0, PE1, PE4, PE5
  {38, 39, 40, 41}, // Demux 1: PD7, PG2, PG1, PG0
  {68, 69, 18, 14}, // Demux 2: PK6, PK7, PD3, PJ1
  {16, 17, 19, 15}  // Demux 3: PH1, PH0, PD2, PJ0
};

void setup() {
  //Initialize all Enable pins to HIGH (Disabled) first
  for (int i = 0; i < 4; i++) {
    digitalWrite(enPins[i], HIGH);
    pinMode(enPins[i], OUTPUT);
    
    //Initialize Address pins as OUTPUT
    for (int j = 0; j < 4; j++) {
      pinMode(addrPins[i][j], OUTPUT);
    }
  }
}

/**
 * The unified control function
 * @param x       Column (0-7)
 * @param y       Row (0-7)
 * @param status  ON or OFF
 */
void setLED(int x, int y, bool status) {
  //Logic: Map x,y to 1 of 4 demuxes and 1 of 16 pins
  int demuxIndex = y / 2;
  int pinAddress = (y % 2) * 8 + x;

  //Always disable all first for safety and to avoid ghosting
  for (int i = 0; i < 4; i++) {
    digitalWrite(enPins[i], HIGH);
  }

  if (status) {
    //1. Set the 4-bit address on the specific demux pins
    for (int i = 0; i < 4; i++) {
      digitalWrite(addrPins[demuxIndex][i], (pinAddress >> i) & 0x01);
    }
    //2. Enable only that demux
    digitalWrite(enPins[demuxIndex], LOW);
  }
}

void loop() {
  //PATTERN: Snake through every single LED on the board one by one
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      setLED(x, y, true);
      delay(100);
      setLED(x, y, false); //Turn off before moving to next
    }
  }
}