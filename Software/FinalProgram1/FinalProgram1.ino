#include <Arduino.h>
#include <mcu-max.h>
#include <SoftwareSerial.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>

//RTC and SD card
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
//#include <SD.h>
#include <SdFat.h>
RTC_DS1307 rtc;
SdFat SD;
FsFile myFile;

// OLED display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
String OLEDtext = "";
bool OLEDworks = true;
#define OLED_ROTATION_VAR 2

void printOnOLED(const String& text, int size = 1, int x = 0, int y = 0) {
  if (OLEDworks) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
  display.display();
  }
}

/* =================================================
 * LED CONTROL VARIABLES
 * ================================================= */
const int LEDenPins[] = {4, 5, 66, 67}; 
const int LEDaddrPins[4][4] = {
  {0, 1, 2, 3},     
  {38, 39, 40, 41}, 
  {68, 69, 18, 14}, 
  {16, 17, 19, 15}  
};

int botFromX = -1, botFromY = -1;
int botToX = -1,   botToY = -1;

/* =================================================
 * EXISTING CONFIGURATION
 * ================================================= */

void setup_all_pins(){
  pinMode(A8,  INPUT_PULLUP);
  pinMode(A9,  INPUT_PULLUP);
  pinMode(A10, INPUT_PULLUP);
  pinMode(A11, INPUT_PULLUP);
}

SoftwareSerial MyFix(10, 19);
#define Serial MyFix

#define MCUMAX_NODE_MAX 1000
#define MCUMAX_DEPTH_MAX 3
#define GAME_VALID_MOVES_NUM_MAX 181

uint8_t board[8][8];
uint8_t oldboard[8][8];
const char *mcumax_symbols = ".PPNKBRQ.ppnkbrq";
const char user_symbols[] = ".PpNnBbRrQqKk";
char humanMove[5] = "";

/* =================================================
 * LED FUNCTIONS
 * ================================================= */
void LEDsetup() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
    pinMode(LEDenPins[i], OUTPUT);
    for (int j = 0; j < 4; j++) {
      pinMode(LEDaddrPins[i][j], OUTPUT);
    }
  }
}

void setLED(int x, int y, bool status) {
  y = 7 - y; 
  int demuxIndex = x / 2;
  int pinAddress = (x % 2) * 8 + y;
  for (int i = 0; i < 4; i++) digitalWrite(LEDenPins[i], HIGH);
  if (status) {
    for (int i = 0; i < 4; i++) digitalWrite(LEDaddrPins[demuxIndex][i], (pinAddress >> i) & 0x01);
    digitalWrite(LEDenPins[demuxIndex], LOW);
  }
}

void clearLEDs() {
  for (int i = 0; i < 4; digitalWrite(LEDenPins[i++], HIGH));
}

void lightUpBotMove() {
  if (botFromX != -1) { setLED(botFromX, botFromY, true); delay(2); }
  if (botToX != -1)   { setLED(botToX, botToY, true); delay(2); }
}

/* =================================================
 * UTILITIES
 * ================================================= */
uint8_t mcumax_piece_to_uint8(uint8_t p) {
  char c = mcumax_symbols[p];
  switch (c) {
    case 'P': return 1; case 'p': return 2;
    case 'N': return 3; case 'n': return 4;
    case 'B': return 5; case 'b': return 6;
    case 'R': return 7; case 'r': return 8;
    case 'Q': return 9; case 'q': return 10;
    case 'K': return 11;case 'k': return 12;
    default:  return 0;
  }
}

void sync_board_from_mcumax() {
  memcpy(oldboard, board, sizeof(board));
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      board[y][x] = mcumax_piece_to_uint8(mcumax_get_piece(0x10 * y + x));
    }
  }
}

void print_board() {
  Serial.println("\n  +-----------------+");
  for (uint8_t y = 0; y < 8; y++) {
    Serial.print(8 - y); Serial.print(" | ");
    for (uint8_t x = 0; x < 8; x++) {
      Serial.print(user_symbols[board[y][x]]); Serial.print(' ');
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------+\n    a b c d e f g h");
}

mcumax_square get_square(char *s) {
  return 0x10 * ('8' - s[1]) + (s[0] - 'a');
}

void coord_to_alg(uint8_t x, uint8_t y, char *out2) {
  out2[0] = 'a' + x;
  out2[1] = '0' + (8 - y);
}

bool check_any_button_press() {
  return !digitalRead(A8) || !digitalRead(A9) || !digitalRead(A10) || !digitalRead(A11);
}

int wait_for_button_selection(int max_options) {
  Serial.println("Select option (Buttons 1-4 correspond to options):");
  while (true) {
    if (!digitalRead(A8) && max_options >= 1) return 0;
    if (!digitalRead(A9) && max_options >= 2) return 1;
    if (!digitalRead(A10) && max_options >= 3) return 2;
    if (!digitalRead(A11) && max_options >= 4) return 3;
    delay(50);
  }
}

// Low-level MUX code
static inline void setBit(volatile uint8_t &port, uint8_t bit, bool value) {
  if (value) port |= _BV(bit); else port &= ~_BV(bit);
}
void disableAllENs() {
  PORTA &= ~(_BV(3)|_BV(7)); PORTC &= ~(_BV(0)|_BV(4));
  PORTL &= ~(_BV(0)|_BV(4)); PORTH &= ~_BV(3); PORTB &= ~_BV(4);
}
void enableRowEN(uint8_t row) {
  disableAllENs();
  switch (row) {
    case 0: PORTA |= _BV(3); break; case 1: PORTA |= _BV(7); break;
    case 2: PORTC |= _BV(0); break; case 3: PORTC |= _BV(4); break;
    case 4: PORTL |= _BV(0); break; case 5: PORTL |= _BV(4); break;
    case 6: PORTH |= _BV(3); break; case 7: PORTB |= _BV(4); break;
  }
}
void setAddressPins(uint8_t row, uint8_t col) {
  bool b0=col&1, b1=col&2, b2=col&4;
  switch (row) {
    case 0: setBit(PORTA,0,b0);setBit(PORTA,1,b1);setBit(PORTA,2,b2); break;
    case 1: setBit(PORTA,4,b0);setBit(PORTA,5,b1);setBit(PORTA,6,b2); break;
    case 2: setBit(PORTC,3,b0);setBit(PORTC,2,b1);setBit(PORTC,1,b2); break;
    case 3: setBit(PORTC,7,b0);setBit(PORTC,6,b1);setBit(PORTC,5,b2); break;
    case 4: setBit(PORTL,3,b0);setBit(PORTL,2,b1);setBit(PORTL,1,b2); break;
    case 5: setBit(PORTL,7,b0);setBit(PORTL,6,b1);setBit(PORTL,5,b2); break;
    case 6: setBit(PORTH,4,b0);setBit(PORTH,5,b1);setBit(PORTH,6,b2); break;
    case 7: setBit(PORTB,7,b0);setBit(PORTB,6,b1);setBit(PORTB,5,b2); break;
  }
}
void initMuxPins() {
  DDRA|=0xFF; DDRC|=0xFF; DDRL|=0xFF; DDRH|=0x78; DDRB|=0xF0;
  disableAllENs();
}
int readHall(uint8_t x, uint8_t y) {
  if (x>7||y>7) return -1;
  disableAllENs(); setAddressPins(y, x);
  delayMicroseconds(10); enableRowEN(y); delayMicroseconds(80);
  int sum=0; for(int i=0;i<8;i++) { sum+=analogRead(A0+y); delayMicroseconds(15); }
  disableAllENs(); return sum/8;
}
static void read_sensor_occupancy(bool occ[8][8]) {
  for (uint8_t y=0; y<8; y++) for (uint8_t x=0; x<8; x++) occ[y][x] = (readHall(x, y) == 0);
}

// Helper to check sub-row pattern
bool check_row_pattern(const bool occ[8][8], uint8_t y, uint8_t x_start, uint8_t len, const bool* pattern) {
  for(uint8_t i=0; i<len; i++) {
    if (occ[y][x_start + i] != pattern[i]) return false;
  }
  return true;
}

bool get_human_move() {
  Serial.println("Waiting for move...");
  while (!check_any_button_press()) {
    lightUpBotMove();
  }
  clearLEDs();
  delay(200); // Debounce

  // 1. Build State Tables
  bool prevOcc[8][8];
  int prevCount = 0;
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      prevOcc[y][x] = (board[y][x] != 0);
      if (prevOcc[y][x]) prevCount++;
    }
  }

  bool currOcc[8][8];
  int currCount = 0;
  read_sensor_occupancy(currOcc);
  for (uint8_t y = 0; y < 8; y++) 
    for (uint8_t x = 0; x < 8; x++) 
      if (currOcc[y][x]) currCount++;

  /* -------------------------------------------------
   * CASTLING CHECK
   * ------------------------------------------------- */
  const bool w_short_prev[] = {1, 0, 0, 1}; // e f g h
  const bool w_short_curr[] = {0, 1, 1, 0}; 
  const bool w_long_prev[]  = {1, 0, 0, 0, 1}; // a b c d e
  const bool w_long_curr[]  = {0, 0, 1, 1, 0};

  uint8_t ROW_WHITE = 7;
  uint8_t ROW_BLACK = 0;

  if (check_row_pattern(prevOcc, ROW_WHITE, 4, 4, w_short_prev) &&
      check_row_pattern(currOcc, ROW_WHITE, 4, 4, w_short_curr)) {
    strcpy(humanMove, "e1g1");
    Serial.println("Detected: White Castling Short (e1g1)");
    return true;
  }
  if (check_row_pattern(prevOcc, ROW_WHITE, 0, 5, w_long_prev) &&
      check_row_pattern(currOcc, ROW_WHITE, 0, 5, w_long_curr)) {
    strcpy(humanMove, "e1c1");
    Serial.println("Detected: White Castling Long (e1c1)");
    return true;
  }
  if (check_row_pattern(prevOcc, ROW_BLACK, 4, 4, w_short_prev) &&
      check_row_pattern(currOcc, ROW_BLACK, 4, 4, w_short_curr)) {
    strcpy(humanMove, "e8g8");
    Serial.println("Detected: Black Castling Short (e8g8)");
    return true;
  }
  if (check_row_pattern(prevOcc, ROW_BLACK, 0, 5, w_long_prev) &&
      check_row_pattern(currOcc, ROW_BLACK, 0, 5, w_long_curr)) {
    strcpy(humanMove, "e8c8");
    Serial.println("Detected: Black Castling Long (e8c8)");
    return true;
  }

  /* -------------------------------------------------
   * CHANGE ANALYSIS
   * ------------------------------------------------- */
  int diffCount = 0;
  int diffX[4], diffY[4]; 
  
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      if (prevOcc[y][x] != currOcc[y][x]) {
        if (diffCount < 4) { diffX[diffCount] = x; diffY[diffCount] = y; }
        diffCount++;
      }
    }
  }

  /* -------------------------------------------------
   * CAPTURE LOGIC (Count decreased by 1)
   * ------------------------------------------------- */
  if (prevCount - currCount == 1) {
    Serial.println("Checking Capture...");
    int fromX = -1, fromY = -1;
    int oneToZeroCount = 0;
    for (int i = 0; i < diffCount; i++) {
      int x = diffX[i]; int y = diffY[i];
      if (prevOcc[y][x] && !currOcc[y][x]) {
        fromX = x; fromY = y;
        oneToZeroCount++;
      }
    }

    if (oneToZeroCount != 1) return false;

    // Find valid captures for this piece
    mcumax_move legal_moves[GAME_VALID_MOVES_NUM_MAX];
    uint32_t n = mcumax_search_valid_moves(legal_moves, GAME_VALID_MOVES_NUM_MAX);
    mcumax_move candidates[10];
    int candCount = 0;
    mcumax_square mcuFrom = (mcumax_square)(0x10 * fromY + fromX);

    for (uint32_t i = 0; i < n; i++) {
      if (legal_moves[i].from == mcuFrom) {
        int tx = legal_moves[i].to & 0x07;
        int ty = (legal_moves[i].to >> 4) & 0x07;
        if (board[ty][tx] != 0) { // Target occupied in old board
          candidates[candCount++] = legal_moves[i];
          if (candCount >= 10) break;
        }
      }
    }

    if (candCount == 0) return false;
    else if (candCount == 1) {
      int tx = candidates[0].to & 0x07;
      int ty = (candidates[0].to >> 4) & 0x07;
      coord_to_alg(fromX, fromY, humanMove);
      coord_to_alg(tx, ty, humanMove + 2);
      humanMove[4] = '\0';
      Serial.print("Detected Capture: "); Serial.println(humanMove);
      return true;
    } else {
      OLEDtext = "Ambiguous Capture!\n\nSelect target:\n";
      Serial.println(OLEDtext);
      for (int i = 0; i < candCount; i++) {
        char temp[5];
        int tx = candidates[i].to & 0x07;
        int ty = (candidates[i].to >> 4) & 0x07;
        coord_to_alg(fromX, fromY, temp);
        coord_to_alg(tx, ty, temp + 2);
        temp[4] = '\0';
        Serial.print("Btn "); 
        Serial.print(i+1); 
        Serial.print(": "); 
        Serial.println(temp);
        OLEDtext += "\nButton " + String(i+1) + ": " + String(temp);
      }

      printOnOLED(OLEDtext,1);

      int choice = wait_for_button_selection(candCount);
      int tx = candidates[choice].to & 0x07;
      int ty = (candidates[choice].to >> 4) & 0x07;
      coord_to_alg(fromX, fromY, humanMove);
      coord_to_alg(tx, ty, humanMove + 2);
      humanMove[4] = '\0';
      return true;
    }
  }

  /* -------------------------------------------------
   * STANDARD MOVE
   * ------------------------------------------------- */
  if (diffCount == 2 && prevCount == currCount) {
    int fromX = -1, fromY = -1;
    int toX = -1,   toY = -1;

    for (int i = 0; i < 2; i++) {
      int x = diffX[i]; int y = diffY[i];
      if (prevOcc[y][x] && !currOcc[y][x]) { fromX = x; fromY = y; }
      else if (!prevOcc[y][x] && currOcc[y][x]) { toX = x; toY = y; }
    }

    if (fromX != -1 && toX != -1) {
      coord_to_alg(fromX, fromY, humanMove);
      coord_to_alg(toX,   toY,   humanMove + 2);
      humanMove[4] = '\0';
      Serial.print("Detected Move: "); Serial.println(humanMove);
      return true;
    }
  }

  return false;
}

String gameLogFilePath = "data.csv";
bool rtcWorks = true;
bool sdCardWorks = true;

void writeToSD(const String& text) {
  MyFix.print("Writing to SD card... ");
  myFile = SD.open(gameLogFilePath.c_str(), FILE_WRITE);
  if (myFile) {
    myFile.println(text);
    myFile.close();
    MyFix.println("done");
  } else {
    MyFix.println("FAILED to open file for writing");
  }
}

void setup() {
  Serial.begin(9600);

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    OLEDworks = false;
  } else {
    display.setRotation(OLED_ROTATION_VAR);
    printOnOLED("Make white\nmove, then\npress any button.",2);
  }

  // RTC setup
  // Initialize I2C for the RTC
  if (!rtc.begin()) {
    MyFix.println("RTC not found!");
    rtcWorks = false;
  }

  if ((!rtc.isrunning()) && rtcWorks) {
    MyFix.println("RTC was stopped, setting time now...");
    // Sets the RTC to the time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  if (rtcWorks) {
    MyFix.print("RTC works! Time: ");
    MyFix.println(rtc.now().timestamp());
  }

  // SD card init
    if (!SD.begin(53)) {
    MyFix.println("SD init failed! Check wiring and card.");
    sdCardWorks = false;
  } else {
    MyFix.println("SD init success!");
  }

  auto randomSeedHallRead = readHall(4,4);
  randomSeed(randomSeedHallRead);
  long randomNum = random(9999);
  MyFix.print("Random number seed: ");
  MyFix.println(randomSeedHallRead);
  MyFix.print("Random number: ");
  MyFix.println(randomNum);

  if (sdCardWorks) {
    if (rtcWorks) {
      String fileTime = String(rtc.now().timestamp());
      fileTime.replace(":","");
      gameLogFilePath = "game_" + fileTime + "_" + String(randomNum) + ".csv";
      writeToSD("time;player;move;legal;");
    } else {
      gameLogFilePath = "game_" + String(randomNum) + ".csv";
      writeToSD("player;move;legal;");
    }
    MyFix.println("Filename: "+gameLogFilePath);
  }

  LEDsetup();
  clearLEDs();
  setup_all_pins();
  initMuxPins();        
  mcumax_init();
  sync_board_from_mcumax();
  Serial.println("Ready.");
  print_board();
}

void loop() {

  bool validHumanMovePlayed = false;
  mcumax_move valid_moves[GAME_VALID_MOVES_NUM_MAX]; 

  Serial.println("\n=== YOUR TURN ===");
  while (!validHumanMovePlayed) {
    if (!get_human_move()) {
      Serial.println("Try again."); continue; 
    }
    
    mcumax_move move = { get_square(humanMove), get_square(humanMove + 2) };
    
    uint32_t n = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
    bool isLegal = false;
    for (uint32_t i = 0; i < n; i++) {
      if (valid_moves[i].from == move.from && valid_moves[i].to == move.to) { isLegal = true; break; }
    }

    if (!isLegal) {
      Serial.print("ILLEGAL: "); Serial.println(humanMove);
      OLEDtext = "Your last\nmove: " + String(humanMove) + "\n\nis ILLEGAL";
      printOnOLED(OLEDtext,2);
      sync_board_from_mcumax(); 
      print_board();
      continue; 
    }

    if (!mcumax_play_move(move)) {
      Serial.println("Engine Error."); sync_board_from_mcumax(); continue; 
    }
    validHumanMovePlayed = true;

    if (sdCardWorks) {
      if (rtcWorks) 
      {
        writeToSD(String(rtc.now().timestamp())+";human;"+String(humanMove)+";"+String(isLegal));
      } else 
      {
        writeToSD("human;"+String(humanMove)+";"+String(isLegal));
      }
    }

  }


  uint32_t bot_moves_count = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
  
  if (bot_moves_count == 0) {
    sync_board_from_mcumax();
    print_board();
    Serial.println("\n## GAME OVER: Checkmate or stalemate! ##");
    Serial.println("## YOU WON! ##");
    printOnOLED("You\n  won!",3,SCREEN_WIDTH/4);
    while(1); // Stop program
  }


  // AI TURN
  sync_board_from_mcumax();
  botFromX = -1; botToX = -1; clearLEDs();

  mcumax_move reply = mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);
  if (reply.from == MCUMAX_SQUARE_INVALID) {
    Serial.println("Game Over.");
    printOnOLED("Game\nover.",3);
    while(1);
  } else {
    mcumax_play_move(reply);
    sync_board_from_mcumax();
    Serial.print("Bot moves: "); 
    char bM[5];
    coord_to_alg(reply.from&7, 7-((reply.from>>4)&7), bM);
    coord_to_alg(reply.to&7, 7-((reply.to>>4)&7), bM+2);
    bM[4]=0; 
    Serial.println(bM);
    
    const bool constantTrue = true;
    if (sdCardWorks) {
      if (rtcWorks) 
      {
        writeToSD(String(rtc.now().timestamp())+";bot;"+String(bM)+";"+String(constantTrue));
      } else 
      {
        writeToSD("bot;"+String(bM)+";"+String(constantTrue));
      }
    }

    OLEDtext = "Your last\nmove: " + String(humanMove) + "\nBot: " + String(bM);
    printOnOLED(OLEDtext,2);

    botFromX = reply.from & 0x07;
    botFromY = (reply.from >> 4) & 0x07; 
    
    botToX = reply.to & 0x07;
    botToY = (reply.to >> 4) & 0x07;
  }
  print_board();

  uint32_t human_moves_count = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
  
  if (human_moves_count == 0) {
    Serial.println("\n## GAME OVER: Checkmate or stalemate! ##");
    Serial.println("## YOU LOST! ##");
    printOnOLED("You\n lost!",3,SCREEN_WIDTH/4);
    lightUpBotMove(); 
    while(1);
  }
}