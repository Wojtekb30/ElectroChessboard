#include "gamefunc.h"

#define PAWNPRICE 2
#define QUEENPRICE 5

int main() {

	// Nothing = 0
	// White = 1
	// Black = 2
	// White queen = 3
	// Black queen = 4
    uint8_t board[8][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 }
	};
	
	bool LEDstatus[8][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 }
	};



    printf("Board:\n");
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            printf("%d ", board[i][j]);
        }
        printf("\n");
    }

	bool validMove = verifyMove(board);
    predictMove(board, LEDstatus, PAWNPRICE, QUEENPRICE);
    
    printf("\nWas move valid?: ");
    printf("%d ", validMove);
    printf("\n\n");
    if (!validMove) { return 1; }

    printf("New LEDstatus:\n");
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            printf("%d ", LEDstatus[i][j]);
        }
        printf("\n");
    }

    return 0;
}
