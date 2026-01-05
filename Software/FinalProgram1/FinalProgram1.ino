#include <mcu-max.h>
#include <SoftwareSerial.h>
#include <stdint.h>

// Define Pin 19 (PD2 / Pin 45) as TX
SoftwareSerial MyFix(10, 19);
#define Serial MyFix

// AI strength
#define MCUMAX_NODE_MAX 1000
#define MCUMAX_DEPTH_MAX 3
#define GAME_VALID_MOVES_NUM_MAX 181

/* -------------------------------------------------
 * PIECE ENCODING (user board)
 *
 * 0  = empty
 * 1  = white pawn
 * 2  = black pawn
 * 3  = white knight
 * 4  = black knight
 * 5  = white bishop
 * 6  = black bishop
 * 7  = white rook
 * 8  = black rook
 * 9  = white queen
 * 10 = black queen
 * 11 = white king
 * 12 = black king
 * ------------------------------------------------- */

uint8_t board[8][8];

// mcu-max piece symbols (official)
const char *mcumax_symbols = ".PPNKBRQ.ppnkbrq";

// symbols for printing *your* board
const char user_symbols[] = ".PpNnBbRrQqKk";

/* -------------------------------------------------
 * CONVERT mcu-max PIECE → uint8 ID
 * ------------------------------------------------- */

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

/* -------------------------------------------------
 * SYNC BOARD FROM mcu-max (SOURCE OF TRUTH)
 * ------------------------------------------------- */

void sync_board_from_mcumax() {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      mcumax_square sq = 0x10 * y + x;
      uint8_t mp = mcumax_get_piece(sq);
      board[y][x] = mcumax_piece_to_uint8(mp);
    }
  }
}

/* -------------------------------------------------
 * PRINTING
 * ------------------------------------------------- */

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

/* -------------------------------------------------
 * INPUT
 * ------------------------------------------------- */

mcumax_square get_square(char *s) {
  uint8_t file = s[0] - 'a';
  uint8_t rank = '8' - s[1];
  if (file > 7 || rank > 7)
    return MCUMAX_SQUARE_INVALID;
  return 0x10 * rank + file;
}

char serial_input[5];
int moveidx = 0;

const char *mymoves[] = {
  "e2e4",
  "g1f3",
  "f1c4",
  "b13b"
};

const int mymovessize = sizeof(mymoves) / sizeof(mymoves[0]);

bool get_serial_input() {
  if (moveidx >= mymovessize)
    return false;
  strcpy(serial_input, mymoves[moveidx++]);
  return true;
}

/* -------------------------------------------------
 * SETUP / LOOP
 * ------------------------------------------------- */

void setup() {
  Serial.begin(9600);

  mcumax_init();
  sync_board_from_mcumax();

  Serial.println("mcu-max serial port example");
  Serial.println("---------------------------");
  Serial.println("Enter moves like e2e4");

  print_board();
}

void loop() {
  if (!get_serial_input())
    return;

  Serial.println("");

  mcumax_move move = {
    get_square(serial_input),
    get_square(serial_input + 2)
  };

  mcumax_move valid_moves[GAME_VALID_MOVES_NUM_MAX];
  uint32_t n = mcumax_search_valid_moves(valid_moves,
                                        GAME_VALID_MOVES_NUM_MAX);

  bool ok = false;
  for (uint32_t i = 0; i < n; i++) {
    if (valid_moves[i].from == move.from &&
        valid_moves[i].to   == move.to) {
      ok = true;
      break;
    }
  }

  if (!ok || !mcumax_play_move(move)) {
    Serial.println("Invalid move.");
  } else {
    sync_board_from_mcumax();

    mcumax_move reply =
      mcumax_search_best_move(MCUMAX_NODE_MAX, MCUMAX_DEPTH_MAX);

    if (reply.from == MCUMAX_SQUARE_INVALID) {
      Serial.println("Game over.");
    } else if (mcumax_play_move(reply)) {
      sync_board_from_mcumax();
      Serial.print("Opponent moves: ");
      print_move(reply);
      Serial.println("");
    }
  }

  print_board();
}
