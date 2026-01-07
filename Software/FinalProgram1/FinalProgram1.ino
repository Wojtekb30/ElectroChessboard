#include <Arduino.h>
#include <mcu-max.h>
#include <SoftwareSerial.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>

/* =================================================
 * LED CONTROL VARIABLES
 * ================================================= */

// Array of Enable Pins for the 4 Demuxes
const int LEDenPins[] = {4, 5, 66, 67}; 

// 2D Array for Address Pins: [Demux Number][A0, A1, A2, A3]
const int LEDaddrPins[4][4] = {
  {0, 1, 2, 3},     // Demux 0: PE0, PE1, PE4, PE5
  {38, 39, 40, 41}, // Demux 1: PD7, PG2, PG1, PG0
  {68, 69, 18, 14}, // Demux 2: PK6, PK7, PD3, PJ1
  {16, 17, 19, 15}  // Demux 3: PH1, PH0, PD2, PJ0
};

// Global variables to store the last move the bot made
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

// forward declaration of readHall
int readHall(uint8_t x, uint8_t y);

// AI strength
#define MCUMAX_NODE_MAX 1000
#define MCUMAX_DEPTH_MAX 3
#define GAME_VALID_MOVES_NUM_MAX 181

/* -------------------------------------------------
 * PIECE ENCODING (user board)
 * ------------------------------------------------- */

uint8_t board[8][8];
uint8_t oldboard[8][8];
bool changemap[8][8];

 // mcu-max piece symbols (official)
const char *mcumax_symbols = ".PPNKBRQ.ppnkbrq";

// symbols for printing *your* board
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

  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
  }

  if (status) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(LEDaddrPins[demuxIndex][i], (pinAddress >> i) & 0x01);
    }
    digitalWrite(LEDenPins[demuxIndex], LOW);
  }
}

void clearLEDs() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEDenPins[i], HIGH);
  }
}

void lightUpBotMove() {
  if (botFromX != -1 && botFromY != -1) {
    setLED(botFromX, botFromY, true);
    delay(2); 
  }
  
  if (botToX != -1 && botToY != -1) {
    setLED(botToX, botToY, true);
    delay(2); 
  }
}

/* =================================================
 * EXISTING UTILITIES
 * ================================================= */

static void print_board_array(const char *title, uint8_t arr[8][8]) {
  Serial.println("");
  Serial.print(title);
  Serial.println(":");
  Serial.println("  +-----------------+");
  for (uint8_t y = 0; y < 8; y++) {
    Serial.print(8 - y);
    Serial.print(" | ");
    for (uint8_t x = 0; x < 8; x++) {
      uint8_t v = arr[y][x];
      char c = (v < sizeof(user_symbols)) ? user_symbols[v] : '?';
      Serial.print(c);
      Serial.print(' ');
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------+");
  Serial.println("    a b c d e f g h");
  Serial.println("");
}

static void print_bool_board(const char *title, bool arr[8][8]) {
  Serial.println("");
  Serial.print(title);
  Serial.println(":");
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      Serial.print(arr[y][x] ? '1' : '0');
      Serial.print(' ');
    }
    Serial.println("");
  }
  Serial.println("");
}

static void print_changemap() {
  Serial.println("");
  Serial.println("changemap:");
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      Serial.print(changemap[y][x] ? '1' : '0');
      Serial.print(' ');
    }
    Serial.println("");
  }
  Serial.println("");
}

static void print_humanMove() {
  Serial.print("humanMove (string): '");
  Serial.print(humanMove);
  Serial.println("'");
}

static void print_all_hall_readings(const char *title) {
  Serial.print("=== Hall ADC matrix: ");
  Serial.print(title);
  Serial.println(" ===");
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      int v = readHall(x, y);
      if (v < 0) Serial.print("ERR");
      else {
        if (v < 10) Serial.print(' ');
        if (v < 100) Serial.print(' ');
        if (v < 1000) Serial.print(' ');
        Serial.print(v);
      }
      Serial.print(' ');
    }
    Serial.println("");
  }
  Serial.println("======================================");
}

uint8_t mcumax_piece_to_uint8(uint8_t p) {
  char c = mcumax_symbols[p];
  switch (c) {
    case 'P': return 1;
    case 'p': return 2;
    case 'N': return 3;
    case 'n': return 4;
    case 'B': return 5;
    case 'b': return 6;
    case 'R': return 7;
    case 'r': return 8;
    case 'Q': return 9;
    case 'q': return 10;
    case 'K': return 11;
    case 'k': return 12;
    default:  return 0;
  }
}

void sync_board_from_mcumax() {
  memcpy(oldboard, board, sizeof(board));
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      mcumax_square sq = 0x10 * y + x;
      uint8_t mp = mcumax_get_piece(sq);
      board[y][x] = mcumax_piece_to_uint8(mp);
    }
  }
  print_board_array("oldboard (before sync)", oldboard);
  print_board_array("board (after sync)", board);
  print_changemap();
  print_humanMove();
}

void print_board() {
  Serial.println("");
  Serial.println("  +-----------------+");
  for (uint8_t y = 0; y < 8; y++) {
    Serial.print(8 - y);
    Serial.print(" | ");
    for (uint8_t x = 0; x < 8; x++) {
      Serial.print(user_symbols[board[y][x]]);
      Serial.print(' ');
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------+");
  Serial.println("    a b c d e f g h");
  Serial.println("");
  Serial.print("Move: ");
}

void print_square(mcumax_square square) {
  Serial.print((char)('a' + (square & 0x07)));
  Serial.print((char)('1' + 7 - ((square & 0x70) >> 4)));
}

void print_move(mcumax_move move) {
  if (move.from == MCUMAX_SQUARE_INVALID ||
      move.to   == MCUMAX_SQUARE_INVALID) {
    Serial.print("(none)");
  } else {
    print_square(move.from);
    print_square(move.to);
  }
}

mcumax_square get_square(char *s) {
  uint8_t file = s[0] - 'a';
  uint8_t rank = '8' - s[1];
  if (file > 7 || rank > 7)
    return MCUMAX_SQUARE_INVALID;
  return 0x10 * rank + file;
}

bool check_any_button_press() {
  return (digitalRead(A8) == LOW) ||
          (digitalRead(A9) == LOW) ||
          (digitalRead(A10) == LOW) ||
          (digitalRead(A11) == LOW);
}

static inline void setBit(volatile uint8_t &port, uint8_t bit, bool value) {
  if (value) port |= _BV(bit);
  else       port &= ~_BV(bit);
}

void disableAllENs() {
  PORTA &= ~(_BV(3) | _BV(7));
  PORTC &= ~(_BV(0) | _BV(4));
  PORTL &= ~(_BV(0) | _BV(4));
  PORTH &= ~_BV(3);
  PORTB &= ~_BV(4);
}

void enableRowEN(uint8_t row) {
  disableAllENs();
  switch (row) {
    case 0: PORTA |= _BV(3); break;
    case 1: PORTA |= _BV(7); break;
    case 2: PORTC |= _BV(0); break;
    case 3: PORTC |= _BV(4); break;
    case 4: PORTL |= _BV(0); break;
    case 5: PORTL |= _BV(4); break;
    case 6: PORTH |= _BV(3); break;
    case 7: PORTB |= _BV(4); break;
    default: break;
  }
}

void setAddressPins(uint8_t row, uint8_t col) {
  bool b0 = col & 0x1;
  bool b1 = col & 0x2;
  bool b2 = col & 0x4;
  switch (row) {
    case 0: setBit(PORTA, 0, b0); setBit(PORTA, 1, b1); setBit(PORTA, 2, b2); break;
    case 1: setBit(PORTA, 4, b0); setBit(PORTA, 5, b1); setBit(PORTA, 6, b2); break;
    case 2: setBit(PORTC, 3, b0); setBit(PORTC, 2, b1); setBit(PORTC, 1, b2); break;
    case 3: setBit(PORTC, 7, b0); setBit(PORTC, 6, b1); setBit(PORTC, 5, b2); break;
    case 4: setBit(PORTL, 3, b0); setBit(PORTL, 2, b1); setBit(PORTL, 1, b2); break;
    case 5: setBit(PORTL, 7, b0); setBit(PORTL, 6, b1); setBit(PORTL, 5, b2); break;
    case 6: setBit(PORTH, 4, b0); setBit(PORTH, 5, b1); setBit(PORTH, 6, b2); break;
    case 7: setBit(PORTB, 7, b0); setBit(PORTB, 6, b1); setBit(PORTB, 5, b2); break;
    default: break;
  }
}

void initMuxPins() {
  DDRA |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);
  DDRC |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);
  DDRL |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7);
  DDRH |= _BV(3)|_BV(4)|_BV(5)|_BV(6);
  DDRB |= _BV(4)|_BV(5)|_BV(6)|_BV(7);
  PORTA &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTC &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTL &= ~(_BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5)|_BV(6)|_BV(7));
  PORTH &= ~(_BV(3)|_BV(4)|_BV(5)|_BV(6));
  PORTB &= ~(_BV(4)|_BV(5)|_BV(6)|_BV(7));
}

int readHall(uint8_t x, uint8_t y) {
  if (x > 7 || y > 7) return -1;
  const uint8_t samples = 8;
  int sum = 0;
  
  disableAllENs(); 
  
  setAddressPins(y, x);
  delayMicroseconds(10);
  enableRowEN(y);
  delayMicroseconds(80);
  for (uint8_t i = 0; i < samples; ++i) {
    sum += analogRead(A0 + y);
    delayMicroseconds(15);
  }
  disableAllENs();
  return sum / samples;
}

static void read_sensor_occupancy(bool occ[8][8]) {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      int v = readHall(x, y); 
      occ[y][x] = (v == 0);
    }
  }
}

static void coord_to_alg(uint8_t x, uint8_t y, char *out2) {
  out2[0] = 'a' + x;
  out2[1] = '0' + (8 - y);
}


// Return the legal move that best matches the sensor occupancy before/after
bool pickMoveByOccupancy(const bool prevOcc[8][8],
                         const bool stableOcc[8][8],
                         const uint8_t prevBoard[8][8],
                         mcumax_move &outMove)
{
  mcumax_move legal[GAME_VALID_MOVES_NUM_MAX];
  uint32_t n = mcumax_search_valid_moves(legal, GAME_VALID_MOVES_NUM_MAX);

  int bestScore = 999;
  mcumax_move best = {MCUMAX_SQUARE_INVALID, MCUMAX_SQUARE_INVALID};

  for (uint32_t i = 0; i < n; ++i) {
    // Build expected occupancy after playing this move on prevOcc
    bool simOcc[8][8];
    memcpy(simOcc, prevOcc, sizeof(simOcc));

    uint8_t fx = legal[i].from & 0x07;
    uint8_t fy = (legal[i].from >> 4) & 0x07;
    uint8_t tx = legal[i].to & 0x07;
    uint8_t ty = (legal[i].to >> 4) & 0x07;

    simOcc[fy][fx] = false;  // from is now empty
    simOcc[ty][tx] = true;   // to is now occupied (capture or not)

    // Count mismatches vs. the sensor reading
    int mismatches = 0;
    for (uint8_t y = 0; y < 8; ++y)
      for (uint8_t x = 0; x < 8; ++x)
        if (simOcc[y][x] != stableOcc[y][x]) mismatches++;

    if (mismatches < bestScore) {
      bestScore = mismatches;
      best = legal[i];
    }
  }

  if (bestScore == 0) {
    outMove = best;
    return true;
  }

  // Accept a near‑perfect match if you want some tolerance:
  // if (bestScore <= 2) { outMove = best; return true; }

  return false;
}


bool get_human_move() {
  Serial.println("Waiting for button press... (LEDs showing bot move)");

  while (!check_any_button_press()) {
    if (botFromX != -1) {
      lightUpBotMove();
    } else {
      delay(10);
    }
  }

  clearLEDs();

  // === ANIMATION START ===
  Serial.println("Starting scan...");
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      setLED(y, x, true);
      delay(20);
      setLED(y, x, false);
    }
  }
  // =======================

  clearLEDs(); // Ensure animation is off

  delay(100);

  // Snapshot before move
  uint8_t prevBoard[8][8];
  memcpy(prevBoard, board, sizeof(prevBoard));

  bool prevOcc[8][8];
  for (uint8_t y = 0; y < 8; y++)
    for (uint8_t x = 0; x < 8; x++)
      prevOcc[y][x] = (prevBoard[y][x] != 0);

  print_board_array("prevBoard (at button press)", prevBoard);
  print_bool_board("prevOcc (non-empty squares before move)", prevOcc);
  print_all_hall_readings("initial (before stabilization)");

  // Stabilise occupancy
  const uint8_t requiredStableReads = 3;
  bool candidate[8][8];
  bool tmp[8][8];
  read_sensor_occupancy(candidate);
  Serial.println("Initial occupancy candidate read (1=occupied, 0=empty):");
  print_bool_board("candidate (first)", candidate);
  uint8_t stableCount = 1;
  int loopChecks = 0;
  delay(80);

  while (stableCount < requiredStableReads) {
    loopChecks++;
    if (loopChecks >= 30) {
      Serial.println("Timeout: Sensor readings unstable. Please press button again.");
      return false;
    }

    read_sensor_occupancy(tmp);
    Serial.print("Stability iteration: comparing read -> ");
    bool same = (memcmp(tmp, candidate, sizeof(tmp)) == 0);
    if (same) {
      stableCount++;
      Serial.print("same (stableCount=");
      Serial.print(stableCount);
      Serial.println(")");
    } else {
      memcpy(candidate, tmp, sizeof(tmp));
      stableCount = 1;
      Serial.println("different -> reset candidate and stableCount=1");
    }
    print_bool_board("current tmp occupancy", tmp);
    delay(80);
  }
  bool stableOcc[8][8];
  memcpy(stableOcc, candidate, sizeof(stableOcc));
  Serial.println("Final stable occupancy (after stabilization):");
  print_bool_board("stableOcc", stableOcc);

  // Optional: show from/to diffs for debugging
  struct Coord { uint8_t x; uint8_t y; };
  Coord froms[16]; uint8_t fromCount = 0;
  Coord tos[16];   uint8_t toCount = 0;
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      bool before = prevOcc[y][x];
      bool after  = stableOcc[y][x];
      if (before && !after)      froms[fromCount++] = {x, y};
      else if (!before && after) tos[toCount++]   = {x, y};
    }
  }
  Serial.print("fromCount = "); Serial.println(fromCount);
  Serial.print("toCount   = "); Serial.println(toCount);
  // ...print froms/tos if desired...

  // Try to find the legal move that best matches the occupancy change
  mcumax_move legal[GAME_VALID_MOVES_NUM_MAX];
  uint32_t n = mcumax_search_valid_moves(legal, GAME_VALID_MOVES_NUM_MAX);

  int bestScore = 999;
  mcumax_move best = {MCUMAX_SQUARE_INVALID, MCUMAX_SQUARE_INVALID};

  for (uint32_t i = 0; i < n; ++i) {
    bool simOcc[8][8];
    memcpy(simOcc, prevOcc, sizeof(simOcc));

    uint8_t fx = legal[i].from & 0x07;
    uint8_t fy = (legal[i].from >> 4) & 0x07;
    uint8_t tx = legal[i].to & 0x07;
    uint8_t ty = (legal[i].to >> 4) & 0x07;

    simOcc[fy][fx] = false; // from now empty
    simOcc[ty][tx] = true;  // to now occupied (capture or not)

    int mismatches = 0;
    for (uint8_t yy = 0; yy < 8; ++yy)
      for (uint8_t xx = 0; xx < 8; ++xx)
        if (simOcc[yy][xx] != stableOcc[yy][xx]) mismatches++;

    if (mismatches < bestScore) {
      bestScore = mismatches;
      best = legal[i];
      if (bestScore == 0) break; // exact match, can stop early
    }
  }

  if (bestScore > 0 || best.from == MCUMAX_SQUARE_INVALID) {
    Serial.println("Could not find a legal move matching the sensor data.");
    return false;
  }

  // Build algebraic string
  uint8_t fx = best.from & 0x07;
  uint8_t fy = (best.from >> 4) & 0x07;
  uint8_t tx = best.to   & 0x07;
  uint8_t ty = (best.to   >> 4) & 0x07;
  coord_to_alg(fx, fy, humanMove);
  coord_to_alg(tx, ty, humanMove + 2);
  humanMove[4] = '\0';

  Serial.print("Detected move (by matching legal moves): ");
  Serial.println(humanMove);
  Serial.print("Final chosen coords: from (");
  Serial.print(fx); Serial.print(','); Serial.print(fy);
  Serial.print(") -> to (");
  Serial.print(tx); Serial.print(','); Serial.print(ty);
  Serial.println(")");
  print_board_array("board (final after detection/resync)", board);
  print_all_hall_readings("after detection (final ADC snapshot)");
  return true;
}

void setup() {
  Serial.begin(9600);
  
  LEDsetup();
  clearLEDs();
  
  setup_all_pins();
  initMuxPins();        
  mcumax_init();
  sync_board_from_mcumax();

  Serial.print("PK0 = ");
  Serial.println(digitalRead(A8));
  delay(200);

  Serial.println("mcu-max serial port example");
  Serial.println("---------------------------");
  Serial.println("Enter moves like e2e4");

  print_board();
  print_board_array("Initial board (setup)", board);
  print_changemap();
  print_humanMove();
  print_all_hall_readings("initial setup ADC snapshot");
}

/* * MAIN LOOP Modification:
 * Now loops internally for the Human Turn until a VALID move is entered.
 */
void loop() {
  bool validHumanMovePlayed = false;
  
  mcumax_move valid_moves[GAME_VALID_MOVES_NUM_MAX]; 

  Serial.println("=== YOUR TURN ===");
  Serial.println("Move a piece and press the button.");

  // Retry loop: Keep asking for a move until a valid legal one is played
  while (!validHumanMovePlayed) {
    
    // 1. Wait for physical move and button press
    if (!get_human_move()) {
      Serial.println("Sensor logic failed to identify a move. Please try pressing button again.");
      continue; 
    }
    Serial.println("");

    // 2. Convert string to Engine Move
    mcumax_move move = {
      get_square(humanMove),
      get_square(humanMove + 2)
    };

    // 3. Search Legal Moves to validate
    uint32_t n = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
    bool isLegal = false;
    for (uint32_t i = 0; i < n; i++) {
      if (valid_moves[i].from == move.from && valid_moves[i].to == move.to) { isLegal = true; break; }
    }

    if (!isLegal) {
      Serial.print("ILLEGAL MOVE detected: ");
      Serial.println(humanMove);
      Serial.println("Move rejected. Please correct board and press button again.");
      
      sync_board_from_mcumax(); 
      print_board();
      continue; 
    }

    // 4. Play the move in the engine
    if (!mcumax_play_move(move)) {
      Serial.println("Engine rejected move (internal error). Resyncing.");
      sync_board_from_mcumax();
      print_board();
      continue; 
    }

    validHumanMovePlayed = true;
  }

  // --- AI TURN ---
  sync_board_from_mcumax();
  
  botFromX = -1; 
  botToX = -1;
  clearLEDs();

  mcumax_move opp_moves[GAME_VALID_MOVES_NUM_MAX];
  uint32_t opp_n = mcumax_search_valid_moves(opp_moves, GAME_VALID_MOVES_NUM_MAX);
  if (opp_n == 0) {
    Serial.println("Game over: opponent has no legal moves (checkmate or stalemate).");
    print_board();
    while(1); 
  }

  // === ANIMATION START ===
  Serial.println("Bot is thinking...");
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      setLED(x, y, true);
      delay(20); // Faster scan during thinking
      setLED(x, y, false);
    }
  }
  // =======================

  mcumax_move reply = mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);
  
  clearLEDs(); // Ensure animation is off before showing move

  if (reply.from == MCUMAX_SQUARE_INVALID) {
    Serial.println("Game over — engine has no move.");
    while(1);
  } else if (!mcumax_play_move(reply)) {
    Serial.println("Engine attempted invalid reply. Resyncing.");
    sync_board_from_mcumax();
    print_board();
    return;
  } else {
    sync_board_from_mcumax();
    Serial.print("Opponent moves: ");
    print_move(reply);
    Serial.println("");
    
    // === LED UPDATE ===
    botFromX = reply.from & 0x07;
    botFromY = (reply.from >> 4) & 0x07;
    
    botToX = reply.to & 0x07;
    botToY = (reply.to >> 4) & 0x07;
    // ==================
  }

  uint32_t you_n = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
  if (you_n == 0) {
    Serial.println("Game over: you have no legal moves (checkmate or stalemate).");
    print_board();
    while(1);
  }

  print_board();
}