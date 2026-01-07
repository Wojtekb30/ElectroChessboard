#include <Arduino.h>
#include <mcu-max.h>
#include <SoftwareSerial.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>

// FIX: Replaced PK0-PK3 (register bits) with A8-A11 (Arduino Pins)
void setup_all_pins(){
  pinMode(A8,  INPUT_PULLUP);
  pinMode(A9,  INPUT_PULLUP);
  pinMode(A10, INPUT_PULLUP);
  pinMode(A11, INPUT_PULLUP);
}

// Define Pin 19 (PD2 / Pin 45) as TX
SoftwareSerial MyFix(10, 19);
#define Serial MyFix

// forward declaration of readHall (implemented below)
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

/* -------------------------------------------------
 * Helper: verbose printing utilities
 * ------------------------------------------------- */

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

// FIX: Updated to use Arduino Pin constants
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

bool get_human_move() {
  // Wait for button press
  while (!check_any_button_press()) {
    delay(10);
  }
  delay(60); 

  uint8_t prevBoard[8][8];
  memcpy(prevBoard, board, sizeof(prevBoard));

  bool prevOcc[8][8];
  for (uint8_t y = 0; y < 8; y++)
    for (uint8_t x = 0; x < 8; x++)
      prevOcc[y][x] = (prevBoard[y][x] != 0);

  print_board_array("prevBoard (at button press)", prevBoard);
  print_bool_board("prevOcc (non-empty squares before move)", prevOcc);
  print_all_hall_readings("initial (before stabilization)");

  const uint8_t requiredStableReads = 3;
  bool candidate[8][8];
  bool tmp[8][8];
  read_sensor_occupancy(candidate);
  Serial.println("Initial occupancy candidate read (1=occupied, 0=empty):");
  print_bool_board("candidate (first)", candidate);
  uint8_t stableCount = 1;
  delay(80);

  while (stableCount < requiredStableReads) {
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

  struct Coord { uint8_t x; uint8_t y; };
  Coord froms[16]; uint8_t fromCount = 0;
  Coord tos[16];   uint8_t toCount = 0;

  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      bool before = prevOcc[y][x];
      bool after = stableOcc[y][x];
      if (before && !after) { froms[fromCount++] = {x, y}; }
      else if (!before && after) { tos[toCount++] = {x, y}; }
    }
  }

  Serial.print("fromCount = "); Serial.println(fromCount);
  Serial.print("toCount   = "); Serial.println(toCount);
  if (fromCount) {
    Serial.println("From squares (x,y):");
    for (uint8_t i = 0; i < fromCount; i++) {
      Serial.print("  ["); Serial.print(i); Serial.print("] ");
      Serial.print((char)('a' + froms[i].x));
      Serial.print(froms[i].y + 1);
      Serial.print("  (x="); Serial.print(froms[i].x);
      Serial.print(",y="); Serial.print(froms[i].y);
      Serial.println(")");
    }
  }
  if (toCount) {
    Serial.println("To squares (x,y):");
    for (uint8_t i = 0; i < toCount; i++) {
      Serial.print("  ["); Serial.print(i); Serial.print("] ");
      Serial.print((char)('a' + tos[i].x));
      Serial.print(tos[i].y + 1);
      Serial.print("  (x="); Serial.print(tos[i].x);
      Serial.print(",y="); Serial.print(tos[i].y);
      Serial.println(")");
    }
  }

  sync_board_from_mcumax();

  uint8_t fx=0, fy=0, tx=0, ty=0;
  bool found = false;

  if (fromCount == 1 && toCount == 1) {
    fx = froms[0].x; fy = froms[0].y;
    tx = tos[0].x; ty = tos[0].y;
    found = true;
    Serial.println("Case: fromCount==1 && toCount==1 -> direct mapping");
  } else if (fromCount == 1 && toCount == 0) {
    Serial.println("Case: fromCount==1 && toCount==0 -> searching best match on board diffs");
    uint8_t fromPiece = prevBoard[froms[0].y][froms[0].x];
    int bestIdx = -1, bestDist = 999;
    for (uint8_t y = 0; y < 8; y++) for (uint8_t x = 0; x < 8; x++) {
      if (prevBoard[y][x] != board[y][x]) {
        if (board[y][x] == fromPiece && fromPiece != 0) {
          int dist = abs((int)froms[0].x - (int)x) + abs((int)froms[0].y - (int)y);
          if (dist < bestDist) { bestDist = dist; bestIdx = y*8 + x; }
        } else {
          int dist = abs((int)froms[0].x - (int)x) + abs((int)froms[0].y - (int)y) + 5;
          if (dist < bestDist) { bestDist = dist; bestIdx = y*8 + x; }
        }
      }
    }
    if (bestIdx >= 0) {
      fx = froms[0].x; fy = froms[0].y;
      tx = bestIdx % 8; ty = bestIdx / 8;
      found = true;
      Serial.print("Found bestIdx via board diff: idx="); Serial.println(bestIdx);
    } else {
      Serial.println("No diffs matched; fallback search on board for new piece location");
      int bestDist2 = 999, bestIdx2 = -1;
      for (uint8_t y = 0; y < 8; y++) for (uint8_t x = 0; x < 8; x++) {
        if (board[y][x] != 0 && !(prevBoard[y][x] != 0)) {
          int dist = abs((int)froms[0].x - (int)x) + abs((int)froms[0].y - (int)y);
          if (dist < bestDist2) { bestDist2 = dist; bestIdx2 = y*8 + x; }
        }
      }
      if (bestIdx2 >= 0) {
        fx = froms[0].x; fy = froms[0].y;
        tx = bestIdx2 % 8; ty = bestIdx2 / 8;
        found = true;
        Serial.print("Found bestIdx2 via presence check: idx="); Serial.println(bestIdx2);
      }
    }
  } else if (fromCount > 1 && toCount >= 1) {
    Serial.println("Case: multiple froms and at least one to -> try to match piece types");
    for (uint8_t i = 0; i < fromCount && !found; i++) {
      for (uint8_t j = 0; j < toCount && !found; j++) {
        uint8_t p = prevBoard[froms[i].y][froms[i].x];
        if (p != 0 && board[tos[j].y][tos[j].x] == p) {
          fx = froms[i].x; fy = froms[i].y;
          tx = tos[j].x; ty = tos[j].y;
          found = true;
        }
      }
    }
    if (!found && fromCount && toCount) {
      fx = froms[0].x; fy = froms[0].y;
      tx = tos[0].x; ty = tos[0].y;
      found = true;
      Serial.println("Fallback: first from -> first to");
    }
  } else if (fromCount == 0 && toCount == 1) {
    Serial.println("Case: fromCount==0 && toCount==1 -> look up lost piece");
    int lostIdx = -1;
    for (uint8_t y = 0; y < 8 && lostIdx < 0; y++) for (uint8_t x = 0; x < 8; x++)
      if (prevBoard[y][x] != 0 && board[y][x] == 0) { lostIdx = y*8 + x; break; }
    if (lostIdx >= 0) {
      fx = lostIdx % 8; fy = lostIdx / 8;
      tx = tos[0].x; ty = tos[0].y;
      found = true;
      Serial.print("Found lostIdx = "); Serial.println(lostIdx);
    } else {
      found = false;
    }
  } else {
    if (fromCount >= 1 && toCount >= 1) {
      fx = froms[0].x; fy = froms[0].y;
      tx = tos[0].x; ty = tos[0].y;
      found = true;
      Serial.println("General fallback: first from & first to");
    } else found = false;
  }

  if (!found) {
    Serial.println("Could not detect a valid move from physical board.");
    sync_board_from_mcumax();
    return false;
  }

  char fromAlg[2], toAlg[2];
  coord_to_alg(fx, fy, fromAlg);
  coord_to_alg(tx, ty, toAlg);
  humanMove[0] = fromAlg[0];
  humanMove[1] = fromAlg[1];
  humanMove[2] = toAlg[0];
  humanMove[3] = toAlg[1];
  humanMove[4] = '\0';

  Serial.print("Detected move: ");
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
  setup_all_pins();
  initMuxPins();        
  mcumax_init();
  sync_board_from_mcumax();

  // FIX: Updated digitalRead to A8
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
  
  // FIX: Declare this here so it can be seen by the checkmate check at the bottom
  mcumax_move valid_moves[GAME_VALID_MOVES_NUM_MAX]; 

  Serial.println("=== YOUR TURN ===");
  Serial.println("Move a piece and press the button.");

  // Retry loop: Keep asking for a move until a valid legal one is played
  while (!validHumanMovePlayed) {
    
    // 1. Wait for physical move and button press
    if (!get_human_move()) {
      Serial.println("Sensor logic failed to identify a move. Please try pressing button again.");
      continue; // Go back to top of while loop (wait for button again)
    }
    Serial.println("");

    // 2. Convert string to Engine Move
    mcumax_move move = {
      get_square(humanMove),
      get_square(humanMove + 2)
    };

    // 3. Search Legal Moves to validate
    // We reuse the array declared at the top of the function
    uint32_t n = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
    bool isLegal = false;
    for (uint32_t i = 0; i < n; i++) {
      if (valid_moves[i].from == move.from && valid_moves[i].to == move.to) { isLegal = true; break; }
    }

    if (!isLegal) {
      Serial.print("ILLEGAL MOVE detected: ");
      Serial.println(humanMove);
      Serial.println("Move rejected. Please correct board and press button again.");
      
      // Reset internal state to what engine thinks, so we can detect the correction
      sync_board_from_mcumax(); 
      print_board();
      continue; // Go back to top of while loop
    }

    // 4. Play the move in the engine
    if (!mcumax_play_move(move)) {
      Serial.println("Engine rejected move (internal error). Resyncing.");
      sync_board_from_mcumax();
      print_board();
      continue; // Go back to top of while loop
    }

    // If we are here, the move was valid and played successfully
    validHumanMovePlayed = true;
  }

  // --- AI TURN ---
  sync_board_from_mcumax();

  // Check if AI is checkmated immediately
  mcumax_move opp_moves[GAME_VALID_MOVES_NUM_MAX];
  uint32_t opp_n = mcumax_search_valid_moves(opp_moves, GAME_VALID_MOVES_NUM_MAX);
  if (opp_n == 0) {
    Serial.println("Game over: opponent has no legal moves (checkmate or stalemate).");
    print_board();
    while(1); // Stop execution
  }

  // Calculate and Play AI Move
  mcumax_move reply = mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);
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
  }

  // Check if Human is checkmated (Now valid_moves is accessible here!)
  uint32_t you_n = mcumax_search_valid_moves(valid_moves, GAME_VALID_MOVES_NUM_MAX);
  if (you_n == 0) {
    Serial.println("Game over: you have no legal moves (checkmate or stalemate).");
    print_board();
    while(1);
  }

  print_board();
}