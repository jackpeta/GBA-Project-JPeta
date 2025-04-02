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

void drawFlag(int cursor_row, int cursor_col) {
  undrawTileDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_gba_board_start_of_play, 150, 120, 1);
  drawImageDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_tile_flag_nocursor);
  drawCursor(cursor_row, cursor_col);
}

// use when board changed
void undrawCursorPressed(int cursor_row, int cursor_col) {
  undrawTileDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_board_after_init, 240, 160, 0);
  drawCursor(cursor_row, cursor_col);
}

// use when board not changed
void undrawCursorMoved(int prev_cursor_row, int prev_cursor_col, int cursor_row, int cursor_col) {
  if (game.mines[prev_cursor_row][prev_cursor_col] != original_mines[prev_cursor_row][prev_cursor_col]) {
    undrawTileDMA(TILE_OFFSET_LENGTH(prev_cursor_row), TILE_OFFSET_WIDTH(prev_cursor_col), 15, 15, minesweeper_board_after_init, 240, 160, 0);
    drawCursor(cursor_row, cursor_col);
  } else {
    undrawTileDMA(CURSOR_OFFSET_LENGTH(prev_cursor_row), CURSOR_OFFSET_WIDTH(prev_cursor_col), 3, 3, minesweeper_gba_board_start_of_play, 150, 120, 1);
    drawCursor(cursor_row, cursor_col);
  }
}

void undrawFlag(int cursor_row, int cursor_col) {
  undrawTileDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_gba_board_start_of_play, 150, 120, 1);
  drawImageDMA(TILE_OFFSET_LENGTH(cursor_row), TILE_OFFSET_WIDTH(cursor_col), 15, 15, minesweeper_tile_noflag_nocursor);
  drawCursor(cursor_row, cursor_col);
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

  while (1) {
    currentButtons = BUTTONS; 

    switch (state) {
        case START:
            drawFullScreenImageDMA(minesweeper_title_10);

            if (KEY_JUST_PRESSED(BUTTON_START, currentButtons, previousButtons)) {
                state = INIT;
            }        
            break;

    
        case PLAY: {
          if (KEY_JUST_PRESSED(BUTTON_SELECT, currentButtons, previousButtons)) {
            state = START;
          }

            if (firstFrame) {
                drawFullScreenImageDMA(minesweeper_board_after_init);
                firstFrame = 0;

                char timerString[20];

                sprintf(timerString, "Timer: 0");
                drawString(24, 95, timerString, GREEN);
            }
            
            // update timer counter
            frames++;
            if (frames >= FRAMES_PER_SECOND / 2) {
                seconds++;
                frames = 0;
                
                // clear previous timer text area
                drawRectDMA(24, 95, 100, 8, MINESWEEPER_GREEN);
                
                // draw new timer text
                char timerString[20];
                sprintf(timerString, "Timer: %d", seconds);
                drawString(24, 95, timerString, GREEN);
            }
            
            for (int i = 0; i < BOARD_ROWS; i++) {
              for (int j = 0; j < BOARD_COLS; j++) {

                switch (game.mines[i][j]) {
                  case TILE_REVEALED:

                    continue;

                  break;
                  case TILE_NOFLAG_NOMINE:

                    drawImageDMA(TILE_OFFSET_LENGTH(i), TILE_OFFSET_WIDTH(j), 15, 15, minesweeper_tile_noflag_nocursor);

                  break;
                  case TILE_NOFLAG_MINE:

                    drawImageDMA(TILE_OFFSET_LENGTH(i), TILE_OFFSET_WIDTH(j), 15, 15, minesweeper_tile_noflag_nocursor);

                  break;
                  case TILE_FLAG_NOMINE:
                    
                    drawImageDMA(TILE_OFFSET_LENGTH(i), TILE_OFFSET_WIDTH(j), 15, 15, minesweeper_tile_flag_nocursor);


                  break;

                  case TILE_FLAG_MINE:

                    drawImageDMA(TILE_OFFSET_LENGTH(i), TILE_OFFSET_WIDTH(j), 15, 15, minesweeper_tile_flag_nocursor);

                  break;
                  default:
                  break;
    
                }
              }
            }
            
            // 0-indexed arrays mean row, col off by 1

            drawCursor(cursor_row, cursor_column);

            // check for button presses
            if (KEY_JUST_PRESSED(BUTTON_RIGHT, currentButtons, previousButtons)) {
              prev_cursor_column = cursor_column;
              cursor_column = (cursor_column + 1) % BOARD_COLS;
              undrawCursorMoved(cursor_row, prev_cursor_column, cursor_row, cursor_column);
          }
          if (KEY_JUST_PRESSED(BUTTON_LEFT, currentButtons, previousButtons)) {
              prev_cursor_column = cursor_column;
              cursor_column = (cursor_column - 1 + BOARD_COLS) % BOARD_COLS;
              undrawCursorMoved(cursor_row, prev_cursor_column, cursor_row, cursor_column);
          }
          if (KEY_JUST_PRESSED(BUTTON_UP, currentButtons, previousButtons)) {
              prev_cursor_row = cursor_row;
              cursor_row = (cursor_row - 1 + BOARD_ROWS) % BOARD_ROWS;
              undrawCursorMoved(prev_cursor_row, cursor_column, cursor_row, cursor_column);
          }
          if (KEY_JUST_PRESSED(BUTTON_DOWN, currentButtons, previousButtons)) {
              prev_cursor_row = cursor_row;
              cursor_row = (cursor_row + 1) % BOARD_ROWS;
              undrawCursorMoved(prev_cursor_row, cursor_column, cursor_row, cursor_column);
          }
      
          // A Button - reveal a tile
          if (KEY_JUST_PRESSED(BUTTON_A, currentButtons, previousButtons)) {
            if (game.mines[cursor_row][cursor_column] == TILE_NOFLAG_MINE) {
              endingTimer = seconds;
              state = LOSE;
            } else if (game.mines[cursor_row][cursor_column] == TILE_FLAG_MINE) {
              game.mines[cursor_row][cursor_column] = TILE_FLAG_MINE;
            } else if (game.mines[cursor_row][cursor_column] == TILE_FLAG_NOMINE) {
                game.mines[cursor_row][cursor_column] = TILE_FLAG_NOMINE;
            } else {
              undrawCursorPressed(cursor_row, cursor_column);
              game.mines[cursor_row][cursor_column] = TILE_REVEALED;
            }
          }
      
          // B Button - flag a tile
          if (KEY_JUST_PRESSED(BUTTON_B, currentButtons, previousButtons)) {
            if (game.mines[cursor_row][cursor_column] == TILE_NOFLAG_MINE) {

              drawFlag(cursor_row, cursor_column);
              game.mines[cursor_row][cursor_column] = TILE_FLAG_MINE;
              if (checkWinCondition()) {
                endingTimer = seconds;
                state = WIN;
              }

            } else if (game.mines[cursor_row][cursor_column] == TILE_FLAG_MINE) {

                undrawFlag(cursor_row, cursor_column);
                game.mines[cursor_row][cursor_column] = TILE_NOFLAG_MINE;

            } else if (game.mines[cursor_row][cursor_column] == TILE_FLAG_NOMINE) {

                undrawFlag(cursor_row, cursor_column);
                game.mines[cursor_row][cursor_column] = TILE_NOFLAG_NOMINE;

            } else if (game.mines[cursor_row][cursor_column] == TILE_NOFLAG_NOMINE) {

                drawFlag(cursor_row, cursor_column);
                game.mines[cursor_row][cursor_column] = TILE_FLAG_NOMINE;

            } else {
              break; // revealed tile or random edge case? 
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
