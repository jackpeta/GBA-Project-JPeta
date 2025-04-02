#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gba.h"

// images
#include "images/title_screen.h"
#include "images/initial_background.h"
#include "images/minesweeper_board_after_init.h"
#include "images/minesweeper_tile_flag_nocursor.h"
#include "images/minesweeper_tile_flag_cursor.h"
#include "images/minesweeper_tile_noflag_cursor.h"
#include "images/minesweeper_tile_noflag_nocursor.h"
#include "images/minesweeper_gba_board_start_of_play.h"
#include "images/minesweeper_win_screen.h"
#include "images/minesweeper_lose_screen.h"

//images for animated title screen
#include "images/titleframe1.h"
#include "images/titleframe2.h"
#include "images/titleframe3.h"
#include "images/titleframe4.h"
#include "images/titleframe5.h"
#include "images/titleframe6.h"
#include "images/titleframe7.h"



// Add any additional states you need for your app. You are not required to use
// these specific provided states.
enum gba_state {
  START,
  INIT,
  PLAY,
  WIN,
  LOSE,
};

volatile int seconds = 0;
volatile int frames = 0;
const int FRAMES_PER_SECOND = 60;

Game game = {
  .mines = {
      {1,1,0,0,0,0,0,0,0,0},
      {1,2,0,0,0,0,0,0,0,0},
      {1,1,0,0,0,0,0,0,0,0},
      {1,1,0,0,0,0,0,2,1,2},
      {1,1,2,0,0,0,2,1,1,1},
      {1,1,2,0,0,0,1,1,1,1},
      {1,1,1,1,1,2,2,2,1,1},
      {1,1,1,2,1,1,1,1,1,1}
  },
  .mines_flagged = 0
};

int original_mines[][10] = {
  {1,1,0,0,0,0,0,0,0,0},
  {1,2,0,0,0,0,0,0,0,0},
  {1,1,0,0,0,0,0,0,0,0},
  {1,1,0,0,0,0,0,2,1,2},
  {1,1,2,0,0,0,2,1,1,1},
  {1,1,2,0,0,0,1,1,1,1},
  {1,1,1,1,1,2,2,2,1,1},
  {1,1,1,2,1,1,1,1,1,1}
};


void drawCursor(int cursor_row, int cursor_col) {
  drawRectDMA(CURSOR_OFFSET_LENGTH(cursor_row), CURSOR_OFFSET_WIDTH(cursor_col), 3, 3, WHITE);
}


void undrawFlag(int cursor_row, int cursor_col) {
  undrawTileDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_gba_board_start_of_play, 150, 120, 1);
  drawImageDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_tile_noflag_nocursor);
  drawCursor(cursor_row, cursor_col);
}

// draws the correct tile state based on cursor row and cursor column
void drawTileState(int r, int c, int draw_cursor) {
  // out of bounds check
  if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS) {
      return;
  }

  int state = game.mines[r][c];
  const u16* image_to_draw = NULL;

  switch (state) {
      case TILE_REVEALED:
          // if revealed, draw from background board
          undrawTileDMA(TILE_OFFSET_LENGTH(r), TILE_OFFSET_WIDTH(c), TILE_SIZE, TILE_SIZE, minesweeper_board_after_init, WIDTH, HEIGHT, 0);
          break; 

      case TILE_FLAG_MINE:
      case TILE_FLAG_NOMINE:
          image_to_draw = minesweeper_tile_flag_nocursor;
          break;

      case TILE_NOFLAG_MINE:
      case TILE_NOFLAG_NOMINE:
      default: // default: non-flagged, if state is unexpected
          image_to_draw = minesweeper_tile_noflag_nocursor;
          break;
  }

  // draw base tile image if tile is selected but not revealed
  if (image_to_draw) {
      drawImageDMA(TILE_OFFSET_LENGTH(r), TILE_OFFSET_WIDTH(c), TILE_SIZE, TILE_SIZE, image_to_draw);
  }

  if (draw_cursor) {
      drawCursor(r, c); // always draw our cursor, our 3x3 rectangle
  }
  // -----------
}

// called when we move our cursor
void updateCursorDraw(int prev_r, int prev_c, int r, int c) {
drawTileState(prev_r, prev_c, 0); // restore tile state

drawTileState(r, c, 1); 
}

// when unflagging/flagging, cursor remains on the tile
void updateTileDraw(int r, int c) {
  drawTileState(r, c, 1);
}

// used to assert win condition
int checkWinCondition(void) {
  int allMinesFlagged = 1; // assume true until iteratively proven otherwise
  int anyNonMineFlagged = 0;
  
  for (int i = 0; i < BOARD_ROWS; i++) {
    for (int j = 0; j < BOARD_COLS; j++) {
      if (original_mines[i][j] == TILE_NOFLAG_MINE && 
          game.mines[i][j] != TILE_FLAG_MINE) {
        allMinesFlagged = 0;
      }
      
      // if there's a non-mine that's flagged, we haven't won yet
      if (original_mines[i][j] != TILE_NOFLAG_MINE && 
          (game.mines[i][j] == TILE_FLAG_MINE || game.mines[i][j] == TILE_FLAG_NOMINE)) {
        anyNonMineFlagged = 1;
      }
    }
  }
  
  return allMinesFlagged && !anyNonMineFlagged;
}

int firstFrame = 1;
int endingTimer;
static volatile int personal_record = 0;
static volatile int firstPlay = 1;
static volatile int hasWon = 0;
static volatile int oldRecord = 1000000;
static volatile int cursor_row = 1;
static volatile int cursor_column = 4;
static volatile int prev_cursor_row, prev_cursor_column;


int main(void) {
  // Manipulate REG_DISPCNT here to set Mode 3. //
  REG_DISPCNT = MODE3 | BG2_ENABLE;

  // Save current and previous state of button input.
  u32 previousButtons = BUTTONS;
  u32 currentButtons = BUTTONS;

  videoBuffer = BACK_BUFFER;

  // Load initial application state
  enum gba_state state = START;

  int moveIndex = 0; //used for animation
  int firstTime = 1; // also used for animation

  while (1) {
    currentButtons = BUTTONS; 

          switch (state) {
              case START:
              if (firstTime == 1) {
                drawFullScreenImageDMA(jackanim1);
                firstTime = 0;
              }
            // Draw the current moving image based on moveIndex
            switch (moveIndex) {
              case 0:
                drawFullScreenImageDMA(jackanim2);
                break;
              case 1:
                drawFullScreenImageDMA(jackanim3);
                break;
              case 2:
                drawFullScreenImageDMA(jackanim4);
                break;
              case 3:
                drawFullScreenImageDMA(jackanim5);
                break;
              case 4:
                drawFullScreenImageDMA(jackanim6);
                break;
              case 5:
                drawFullScreenImageDMA(jackanim7);
                break;
            }
            vBlankCounter++;
            if (vBlankCounter >= VBLANK_DELAY) {
              moveIndex = (moveIndex + 1) % TOTAL_MOVES;  // loop through images (0 -> 1 -> 2 -> ... -> 0)
              vBlankCounter = 0;
            }

            if (KEY_JUST_PRESSED(BUTTON_START, currentButtons, previousButtons)) {
              // reset gamestate whenever we go to INIT from START
              personal_record = 0;
              firstPlay = 1;
              hasWon = 0;
              oldRecord = 1000000;
              state = INIT;
              cursor_row = 1;
              cursor_column = 4;
            } 

            break;

    
        case PLAY: {
          if (KEY_JUST_PRESSED(BUTTON_SELECT, currentButtons, previousButtons)) {
            state = START;
            firstFrame = 1; 
          }
        
          if (firstFrame) {
              // draw base background
              drawFullScreenImageDMA(minesweeper_board_after_init);
        
              // iterative loop to draw all non-revealed tiles (initial state)
              for (int i = 0; i < BOARD_ROWS; i++) {
                  for (int j = 0; j < BOARD_COLS; j++) {
                      if (game.mines[i][j] != TILE_REVEALED) {
                          drawTileState(i, j, 0); 
                      }
                  }
              }

              // draw initial cursor position
              drawTileState(cursor_row, cursor_column, 1); 
        
              firstFrame = 0; 
        
              // draw initial timer text
              char timerString[20];
              sprintf(timerString, "Timer: 0");

              // erase potential old text 
              drawRectDMA(24, 95, 100, 8, MINESWEEPER_GREEN);
              drawString(24, 95, timerString, GREEN);
          }
        
          frames++;
          if (frames >= FRAMES_PER_SECOND) { 
              seconds++;
              frames = 0;
              drawRectDMA(24, 95, 100, 8, MINESWEEPER_GREEN);
              char timerString[20];
              sprintf(timerString, "Timer: %d", seconds);
              drawString(24, 95, timerString, GREEN);
          }
        
        
          // store previous cursor positions
          prev_cursor_row = cursor_row; 
          prev_cursor_column = cursor_column;

          int moved = 0; // flag to check if cursor moved
        
          if (KEY_JUST_PRESSED(BUTTON_RIGHT, currentButtons, previousButtons)) {
              cursor_column = (cursor_column + 1) % BOARD_COLS;
              moved = 1;
          }
          if (KEY_JUST_PRESSED(BUTTON_LEFT, currentButtons, previousButtons)) {
              cursor_column = (cursor_column - 1 + BOARD_COLS) % BOARD_COLS;
              moved = 1;
          }
          if (KEY_JUST_PRESSED(BUTTON_UP, currentButtons, previousButtons)) {
              cursor_row = (cursor_row - 1 + BOARD_ROWS) % BOARD_ROWS;
              moved = 1;
          }
          if (KEY_JUST_PRESSED(BUTTON_DOWN, currentButtons, previousButtons)) {
              cursor_row = (cursor_row + 1) % BOARD_ROWS;
              moved = 1;
          }
        
          // if cursor moved, update drawing
          if (moved) {
              updateCursorDraw(prev_cursor_row, prev_cursor_column, cursor_row, cursor_column);
          }
        
          // A button - reveal a tile
          if (KEY_JUST_PRESSED(BUTTON_A, currentButtons, previousButtons)) {
              int r = cursor_row;
              int c = cursor_column;
              if (game.mines[r][c] == TILE_NOFLAG_MINE) {
                  endingTimer = seconds;
                  state = LOSE; 
              } else if (game.mines[r][c] == TILE_NOFLAG_NOMINE) {
                  // reveal tile
                  game.mines[r][c] = TILE_REVEALED;
                  updateTileDraw(r, c); 
                  // TODO: Implement flood fill reveal for empty tiles if needed
              }
          }
        
          // B button - flag/unflag a tile
          if (KEY_JUST_PRESSED(BUTTON_B, currentButtons, previousButtons)) {
              int r = cursor_row;
              int c = cursor_column;
              int stateChanged = 0;
              if (game.mines[r][c] == TILE_NOFLAG_MINE || game.mines[r][c] == TILE_NOFLAG_NOMINE) {
                  game.mines[r][c] = (game.mines[r][c] == TILE_NOFLAG_MINE) ? TILE_FLAG_MINE : TILE_FLAG_NOMINE;
                  stateChanged = 1;
                  if (game.mines[r][c] == TILE_FLAG_MINE && checkWinCondition()) {
                      endingTimer = seconds;
                      state = WIN; // always check win condition after flagging
                  }
              } else if (game.mines[r][c] == TILE_FLAG_MINE || game.mines[r][c] == TILE_FLAG_NOMINE) {
                  // unflag the tile
                  game.mines[r][c] = (game.mines[r][c] == TILE_FLAG_MINE) ? TILE_NOFLAG_MINE : TILE_NOFLAG_NOMINE;
                  stateChanged = 1;
              }
        
              if (stateChanged && state == PLAY) { //uUpdate draw only if state changed and game is still playing
                  updateTileDraw(r, c); 
              }
          }
          break;
        } 

        case INIT:

          if (KEY_JUST_PRESSED(BUTTON_SELECT, currentButtons, previousButtons)) {
            state = START;
        }

          waitForVBlank();
          while (DMA[3].cnt & DMA_ON);
          fillScreenDMA(BLACK);
          waitForVBlank();
          drawFullScreenImageDMA(minesweeper_initial_background);
  
          // switch our buffers
          videoBuffer = BACK_BUFFER;
      
          for (int i = 0; i < BOARD_ROWS; i++) {
              for (int j = 0; j < BOARD_COLS; j++) {
                  game.mines[i][j] = original_mines[i][j];
              }
          }

          // reset cursor state every time we go again
          if (!firstPlay) {
            cursor_row = 1;
            cursor_column = 4;
          }

      
          if (KEY_JUST_PRESSED(BUTTON_START, currentButtons, previousButtons)) {
              seconds = 0;
              frames = 0;
              firstFrame = 1;
              state = PLAY;
          }
          break;

        case WIN:
        if (KEY_JUST_PRESSED(BUTTON_SELECT, currentButtons, previousButtons)) {
          state = START;
      }

        waitForVBlank();
        fillScreenDMA(BLACK);
        waitForVBlank();
        drawFullScreenImageDMA(minesweeper_win_screen);
        waitForVBlank();

        char beatRecord[40];
        int player_beat_record = 0;

        // if it's the first play, simply set the record without comparing.
        if (firstPlay || !hasWon) {
            personal_record = seconds;
        } else {
              if (seconds <= personal_record) {
                  personal_record = seconds;
                  player_beat_record = 1;
                  strcpy(beatRecord, "You beat your personal best!");
              } else {
                  strcpy(beatRecord, "You didn't beat your personal best.");
              }
          }

          char timerString[20];
          sprintf(timerString, "YOUR TIME: %d", seconds);
          drawString(60, 80, timerString, BLACK);

          // display record info if it's not the first play.
          if (!firstPlay && hasWon) {
              char recordString[30];

              // this if statement is necessary because the strings are of different lengths
              // and as such we need to center them differently based on the outcome
              if (player_beat_record) {
                sprintf(recordString, "YOUR PREVIOUS BEST: %d", oldRecord);
                drawString(75, 80, recordString, BLACK);
                drawString(90, 30, beatRecord, BLACK);
              } else {
                sprintf(recordString, "YOUR BEST: %d", personal_record);
                drawString(75, 80, recordString, BLACK);
                drawString(90, 10, beatRecord, BLACK);
              }
          }

          

        if (KEY_JUST_PRESSED(BUTTON_START, currentButtons, previousButtons)) {
          waitForVBlank();
          fillScreenDMA(BLACK);  
          firstPlay = 0;
          hasWon = 1;
          oldRecord = oldRecord < seconds ? oldRecord : seconds;
          state = INIT;
        }
            break;
            
        case LOSE:
        if (KEY_JUST_PRESSED(BUTTON_SELECT, currentButtons, previousButtons)) {
          state = START;
        }

            waitForVBlank();
            fillScreenDMA(BLACK);
            waitForVBlank();
            drawFullScreenImageDMA(minesweeper_lose_screen);
            waitForVBlank();

           if (KEY_JUST_PRESSED(BUTTON_START, currentButtons, previousButtons)) {
            waitForVBlank();
            fillScreenDMA(BLACK);
            firstPlay = 0;  
            state = INIT;
           }
            break;
    }

    // flip the buffers to display what we just drew
    flipBuffers();

    previousButtons = currentButtons; // store the current state of the buttons
}

return 0;
}
