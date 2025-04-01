#ifndef MAIN_H
#define MAIN_H

#include "gba.h"

// Game board dimensions
#define BOARD_ROWS 8    
#define BOARD_COLS 10
#define NUM_MINES 10
#define TILE_SIZE 15 // size of each tile in pixels

// used to offset how we draw and interact with tiles - 15x15 grid area offset by the board's placement
#define TILE_OFFSET_LENGTH(i) (34 + (15 * i))
#define TILE_OFFSET_WIDTH(j) (44 + (15 * j))

// same for cursor, except 3x3 area offset by both the board and the tile
#define CURSOR_OFFSET_LENGTH(i) (34 + (15 * i) + 6)
#define CURSOR_OFFSET_WIDTH(j) (44 + (15 * j) + 6)

// mine states
#define TILE_REVEALED 0
#define TILE_NOFLAG_NOMINE 1
#define TILE_NOFLAG_MINE 2
#define TILE_FLAG_NOMINE 3
#define TILE_FLAG_MINE 4


typedef struct {
    int mines[8][10];
    int mines_flagged;
} Game;

extern volatile int seconds;
extern volatile int frames;


#endif
