/*
  64-hall reader using 8 x ADG708BRUZ (one per row)
  readHall(x,y) -> x = column 0..7, y = row 0..7
  - Address pins for each row assumed in the order you gave (A0, A1, A2).
  - EN polarity: per ADG708 datasheet EN=1 enables a channel; EN=0 disables all.
  - Uses direct port access (PORTA, PORTC, PORTL, PORTH, PORTB).
*/

#include <Arduino.h>
#include <avr/io.h>

// ---------- configuration per your wiring (as provided) ----------
// EN pins (one ADG708 per row):
// Row 0 - PA3
// Row 1 - PA7
// Row 2 - PC0
// Row 3 - PC4
// Row 4 - PL0
// Row 5 - PL4
// Row 6 - PH3
// Row 7 - PB4

// Address pins for each row (listed in your message).
// Row 0: PA0, PA1, PA2        (A0,A1,A2)
// Row 1: PA4, PA5, PA6        (A0,A1,A2)
// Row 2: PC3, PC2, PC1        (A0,A1,A2)
// Row 3: PC7, PC6, PC5        (A0,A1,A2)
// Row 4: PL3, PL2, PL1        (A0,A1,A2)
// Row 5: PL7, PL6, PL5        (A0,A1,A2)
// Row 6: PH4, PH5, PH6        (A0,A1,A2)
// Row 7: PB7, PB6, PB5        (A0,A1,A2)

// ---------- helper inline functions ----------
static inline void setBit(volatile uint8_t &port, uint8_t bit, bool value) {
  if (value) port |= _BV(bit);
  else       port &= ~_BV(bit);
}

// Disable all EN lines (set EN = 0 = disabled)
void disableAllENs() {
  // PA3, PA7, PC0, PC4, PL0, PL4, PH3, PB4 -> clear their PORT bits
  PORTA &= ~(_BV(3) | _BV(7));
  PORTC &= ~(_BV(0) | _BV(4));
  PORTL &= ~(_BV(0) | _BV(4));
  PORTH &= ~_BV(3);
  PORTB &= ~_BV(4);
}

// Enable exactly one EN (EN = 1 enables). We first clear all ENs then set the one.
void enableRowEN(uint8_t row) {
  disableAllENs();
  switch (row) {
    case 0: PORTA |= _BV(3); break; // PA3
    case 1: PORTA |= _BV(7); break; // PA7
    case 2: PORTC |= _BV(0); break; // PC0
    case 3: PORTC |= _BV(4); break; // PC4
    case 4: PORTL |= _BV(0); break; // PL0
    case 5: PORTL |= _BV(4); break; // PL4
    case 6: PORTH |= _BV(3); break; // PH3
    case 7: PORTB |= _BV(4); break; // PB4
    default: break;
  }
}

// Set the three address pins (A0..A2) for a given row to value 'col' (0..7).
// Assumes the order you gave maps to A0, A1, A2 respectively.
void setAddressPins(uint8_t row, uint8_t col) {
  bool b0 = col & 0x1;
  bool b1 = col & 0x2;
  bool b2 = col & 0x4;

  switch (row) {
    case 0: // PA0, PA1, PA2
      setBit(PORTA, 0, b0);
      setBit(PORTA, 1, b1);
      setBit(PORTA, 2, b2);
      break;
    case 1: // PA4, PA5, PA6
      setBit(PORTA, 4, b0);
      setBit(PORTA, 5, b1);
      setBit(PORTA, 6, b2);
      break;
    case 2: // PC3, PC2, PC1
      setBit(PORTC, 3, b0);
      setBit(PORTC, 2, b1);
      setBit(PORTC, 1, b2);
      break;
    case 3: // PC7, PC6, PC5
      setBit(PORTC, 7, b0);
      setBit(PORTC, 6, b1);
      setBit(PORTC, 5, b2);
      break;
    case 4: // PL3, PL2, PL1
      setBit(PORTL, 3, b0);
      setBit(PORTL, 2, b1);
      setBit(PORTL, 1, b2);
      break;
    case 5: // PL7, PL6, PL5
      setBit(PORTL, 7, b0);
      setBit(PORTL, 6, b1);
      setBit(PORTL, 5, b2);
      break;
    case 6: // PH4, PH5, PH6
      setBit(PORTH, 4, b0);
      setBit(PORTH, 5, b1);
      setBit(PORTH, 6, b2);
      break;
    case 7: // PB7, PB6, PB5
      setBit(PORTB, 7, b0);
      setBit(PORTB, 6, b1);
      setBit(PORTB, 5, b2);
      break;
    default:
      break;
  }
}

// Call once in setup to configure DDRx for all used pins as outputs
void initMuxPins() {
  // Address pins and EN pins -> configure corresponding DDRx bits

  // PORTA pins used: PA0,PA1,PA2,PA3 (EN), PA4,PA5,PA6,PA7(EN)
  DDRA |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);

  // PORTC pins used: PC0(EN), PC1,PC2,PC3, PC4(EN), PC5,PC6,PC7
  DDRC |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);

  // PORTL pins used: PL0(EN), PL1,PL2,PL3, PL4(EN), PL5,PL6,PL7
  DDRL |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);

  // PORTH pins used: PH3(EN), PH4,PH5,PH6
  DDRH |= _BV(3)|_BV(4)|_BV(5)|_BV(6);

  // PORTB pins used: PB4(EN), PB5,PB6,PB7
  DDRB |= _BV(4)|_BV(5)|_BV(6)|_BV(7);

  // initial states: clear address pins and disable ENs
  PORTA &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTC &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTL &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTH &= ~(_BV(3)|_BV(4)|_BV(5)|_BV(6));
  PORTB &= ~(_BV(4)|_BV(5)|_BV(6)|_BV(7));
}

// ---------- the API function requested ----------
/*
  Read the hall sensor at column x (0..7), row y (0..7).
  Returns averaged ADC value (0..1023) on default Arduino ADC resolution (10-bit).
  Returns -1 if x or y out of range.
*/
int readHall(uint8_t x, uint8_t y) {
  if (x > 7 || y > 7) return -1;

  const uint8_t samples = 8;
  int sum = 0;

  // Ensure all ENs disabled while we change address lines:
  disableAllENs();

  // Set address bits for the targeted multiplexer (row)
  setAddressPins(y, x);

  // small settle after address change (depends on your hardware; increase if needed)
  delayMicroseconds(10);

  // Enable only the desired row
  enableRowEN(y);

  // allow mux + ADC to settle
  delayMicroseconds(80);

  // Read ADC on PFy (ADC channel y). Use analogRead(A0 + y).
  for (uint8_t i = 0; i < samples; ++i) {
    sum += analogRead(A0 + y);
    delayMicroseconds(15); // tiny gap between samples
  }

  // Optionally disable row again to avoid leakage/ghosting
  disableAllENs();

  return sum / samples;
}

// ---------- example usage ----------


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
void setLED(int x, int y, bool status, bool disableOther = true) {
  y=7-y;
  //Logic: Map x,y to 1 of 4 demuxes and 1 of 16 pins
  int demuxIndex = x / 2;
  int pinAddress = (x % 2) * 8 + y;

  if (disableOther) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
  }}

  if (status) {
    //1. Set the 4-bit address on the specific demux pins
    for (int i = 0; i < 4; i++) {
      digitalWrite(LEDaddrPins[demuxIndex][i], (pinAddress >> i) & 0x01);
    }
    //2. Enable only that demux
    digitalWrite(LEDenPins[demuxIndex], LOW);
  }
}



#include <SoftwareSerial.h> 
SoftwareSerial MyFix(10, 19);

void setup() {
  LEDsetup();
  MyFix.begin(9600); 
  initMuxPins();
  delay(200);
  MyFix.println(F("64-hall reader ready"));
}

void loop() {
  // Example: sweep all sensors once and print
  for (uint8_t r = 0; r < 8; ++r) {
    for (uint8_t c = 0; c < 8; ++c) {
      int v = readHall(c, r);
      MyFix.print(v);
      MyFix.print('\t');
      if (v==0 && c<=5) {
        setLED(c,r,true, 1);
      }
      //else if (v>1 && c<=5) {
      //  setLED(c,r,false, false);
      //}
    }
    MyFix.println();
  }
  delay(500);
  MyFix.println("---");
}
