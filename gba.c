#include "gba.h"

u16 BACK_BUFFER[240 * 160] __attribute__((aligned(4))); // back buffer avoids screen tearing

volatile u16 *videoBuffer = FRONT_BUFFER;
u32 vBlankCounter = 0;

void flipBuffers(void) {
  waitForVBlank();
  
  DMA[3].src = BACK_BUFFER;
  DMA[3].dst = FRONT_BUFFER;
  DMA[3].cnt = (WIDTH * HEIGHT) | DMA_SOURCE_INCREMENT | DMA_DESTINATION_INCREMENT | DMA_16 | DMA_ON;
}

/*
  Wait until the start of the next VBlank. This is useful to avoid tearing.
  Completing this function is required.
*/
void waitForVBlank(void) {
   // (1)
  // Write a while loop that loops until we're NOT in vBlank anymore:
  // (This prevents counting one VBlank more than once if your app is too fast)
  while (SCANLINECOUNTER >= 160);
  
  // (2)
  // Write a while loop that keeps going until we're in vBlank:
  while (SCANLINECOUNTER < 160);
  
  // (3)
  // Finally, increment the vBlank counter:
  vBlankCounter++;
}

static int __qran_seed = 42;
static int qran(void) {
  __qran_seed = 1664525 * __qran_seed + 1013904223;
  return (__qran_seed >> 16) & 0x7FFF;
}

int randint(int min, int max) { return (qran() * (max - min) >> 15) + min; }

/*
  Sets a pixel in the video buffer to a given color.
  Using DMA is NOT recommended. (In fact, using DMA with this function would be really slow!)
*/
void setPixel(int row, int col, u16 color) {
  
  videoBuffer[OFFSET(row, col, WIDTH)] = color;

}

/*
  Draws a rectangle of a given color to the video buffer.
  The width and height, as well as the top left corner of the rectangle, are passed as parameters.
  This function can be completed using `height` DMA calls. 
*/
void drawRectDMA(int row, int col, int width, int height, volatile u16 color) {
  // loop through row
  for (int r = 0; r < height; r++) {
    // destination address for this row in the video buffer
    volatile u16 *dest = &videoBuffer[OFFSET(row + r, col, WIDTH)];
    
    // set up DMA transfer for this row
    DMA[DMA_CHANNEL_3].src = &color;          
    DMA[DMA_CHANNEL_3].dst = dest;            
    DMA[DMA_CHANNEL_3].cnt = width | DMA_SOURCE_FIXED | DMA_DESTINATION_INCREMENT | DMA_16 | DMA_ON;                     
  }
}

/*
  Draws a fullscreen image to the video buffer.
  The image passed in must be of size WIDTH * HEIGHT.
  This function can be completed using a single DMA call.
*/
void drawFullScreenImageDMA(const u16 *image) {
  DMA[3].src = image;
  DMA[3].dst = videoBuffer;
  DMA[3].cnt = (WIDTH * HEIGHT) | DMA_SOURCE_INCREMENT | DMA_DESTINATION_INCREMENT | DMA_ON;
}

/*
  Draws an image to the video buffer.
  The width and height, as well as the top left corner of the image, are passed as parameters.
  The image passed in must be of size width * height.
  Completing this function is required.
  This function can be completed using `height` DMA calls. Solutions that use more DMA calls will not get credit.
*/
void drawImageDMA(int row, int col, int width, int height, const u16 *image) {
  for (int r = 0; r < height; r++) {
    // calculate the source address for this row in the image data
    const u16 *src = &image[r * width];
    // calculate the destination address for this row in the video buffer
    volatile u16 *dest = &videoBuffer[OFFSET(row + r, col, WIDTH)];
    
    DMA[3].src = src;                // source is the current row in our image
    DMA[3].dst = dest;               // destination is the video buffer
    DMA[3].cnt = width | DMA_SOURCE_INCREMENT | DMA_DESTINATION_INCREMENT | DMA_16 | DMA_ON;                     
  }
}

/*
  Draws a rectangular chunk of a fullscreen image to the video buffer.
  The width and height, as well as the top left corner of the chunk to be drawn, are passed as parameters.
  The image passed in must be of size WIDTH * HEIGHT.
  This function can be completed using `height` DMA calls.
*/
void undrawImageDMA(int row, int col, int width, int height, const u16 *image) {
  for (int r = 0; r < height; r++) {
    // calculate the source address for this row in the full-screen image
    // find the correct row and column in the source image
    const u16 *src = &image[OFFSET(row + r, col, WIDTH)];
    
    // calculate the destination address for this row in the video buffer
    volatile u16 *dest = &videoBuffer[OFFSET(row + r, col, WIDTH)];
    
    DMA[3].src = src;                
    DMA[3].dst = dest;             
    DMA[3].cnt = width | DMA_SOURCE_INCREMENT | DMA_DESTINATION_INCREMENT | DMA_16 | DMA_ON;                    
  }
}


// i made this to selectively undraw parts of the background image
// used in cursor undrawing and tile undrawing!
void undrawTileDMA(int row, int col, int width, int height, const u16 *background_image, int image_width, int image_height, int is_board_image) {
  int source_row = row;
  int source_col = col;
  
  // If this is a board image (150x120), apply the offsets
  if (is_board_image) {
    source_row = row - 34; 
    source_col = col - 44;
    
    // Verify bounds for board images
    if (source_row < 0 || source_col < 0 || source_row + height > image_height || source_col + width > image_width) {
      return;
    }
  } else {
    // For full-screen images, just check basic bounds
    if (row < 0 || col < 0 || row + height > HEIGHT || col + width > WIDTH) {
      return;
    }
  }

  while (DMA[3].cnt & DMA_ON); // Wait for DMA

  for (int r = 0; r < height; r++) {
    // Use the provided image width for offset calculations
    const u16 *src = &background_image[OFFSET(source_row + r, source_col, image_width)];
    volatile u16 *dest = &videoBuffer[OFFSET(row + r, col, WIDTH)];

    DMA[3].src = src;
    DMA[3].dst = dest;
    DMA[3].cnt = width | DMA_SOURCE_INCREMENT | DMA_DESTINATION_INCREMENT | DMA_16 | DMA_ON;
  }
}

/*
  Fills the video buffer with a given color.
  This function can be completed using a single DMA call.
*/
void fillScreenDMA(volatile u16 color) {
  DMA[3].src = &color;
  DMA[3].dst = videoBuffer;
  DMA[3].cnt = (WIDTH * HEIGHT) | DMA_SOURCE_FIXED | DMA_DESTINATION_INCREMENT | DMA_ON;
}

/* STRING-DRAWING FUNCTIONS (provided) */
void drawChar(int row, int col, char ch, u16 color) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 8; j++) {
      if (fontdata_6x8[OFFSET(j, i, 6) + ch * 48]) {
        setPixel(row + j, col + i, color);
      }
    }
  }
}

void drawString(int row, int col, char *str, u16 color) {
  while (*str) {
    drawChar(row, col, *str++, color);
    col += 6;
  }
}

void drawCenteredString(int row, int col, int width, int height, char *str, u16 color) {
  u32 len = 0;
  char *strCpy = str;
  while (*strCpy) {
    len++;
    strCpy++;
  }

  u32 strWidth = 6 * len;
  u32 strHeight = 8;

  int new_row = row + ((height - strHeight) >> 1);
  int new_col = col + ((width - strWidth) >> 1);
  drawString(new_row, new_col, str, color);
}
