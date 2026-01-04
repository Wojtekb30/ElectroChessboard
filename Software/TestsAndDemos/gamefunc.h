#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

	// Nothing = 0
	// White = 1
	// Black = 2
	// White queen = 3
	// Black queen = 4

bool verifyMove(uint8_t board[8][8]) {
	
	return true;
}

void predictMove(const uint8_t board[8][8], bool LEDstatus[8][8], uint8_t pawnPrice, uint8_t queenPrice) {
    //Zero the LEDs
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            LEDstatus[i][j] = false;
        }
    }
}

