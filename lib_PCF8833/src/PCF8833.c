/*
 *  PCF8833.c
 *
 *  Author: Kestutis Bivainis
 *
 */

#include "stm32f10x_conf.h"
#include "PCF8833.h"
#include "delay.h"
#include "font6x8.h"
#include "font8x8.h"
#include "font8x14.h"

static PCF8833_COLOR_MODE PCF8833_color_mode;
static PCF8833_ACCESS_MODE PCF8833_access_mode;
static PCF8833_ORIENTATION_MODE PCF8833_orientation_mode;
static uint8_t PCF8833_v,PCF8833_mirror;
static PCF8833_RGB_MODE PCF8833_rgb_mode;
static PCF8833_FONT_SIZE PCF8833_font_size;
static uint16_t PCF8833_text_foreground_color=0xFFFF;
static uint16_t PCF8833_text_background_color=0x0000;

static PCF8833_PIN pins_bb[]={
  {{PCF8833_CS_Pin, PCF8833_CS_Speed, PCF8833_CS_Mode_BB}, PCF8833_CS_Port, PCF8833_CS_Bus},
  {{PCF8833_SCLK_Pin,PCF8833_SCLK_Speed,PCF8833_SCLK_Mode_BB},PCF8833_SCLK_Port,PCF8833_SCLK_Bus},
  {{PCF8833_SDATA_Pin,PCF8833_SDATA_Speed,PCF8833_SDATA_Mode_BB},PCF8833_SDATA_Port,PCF8833_SDATA_Bus},
  {{PCF8833_RST_Pin,PCF8833_RST_Speed,PCF8833_RST_Mode},PCF8833_RST_Port,PCF8833_RST_Bus},
};

static PCF8833_PIN pins_spi[]={
  {{PCF8833_CS_Pin, PCF8833_CS_Speed, PCF8833_CS_Mode_SPI}, PCF8833_CS_Port, PCF8833_CS_Bus},
  {{PCF8833_SCLK_Pin,PCF8833_SCLK_Speed,PCF8833_SCLK_Mode_SPI},PCF8833_SCLK_Port,PCF8833_SCLK_Bus},
  {{PCF8833_SDATA_Pin,PCF8833_SDATA_Speed,PCF8833_SDATA_Mode_SPI},PCF8833_SDATA_Port,PCF8833_SDATA_Bus},
  {{PCF8833_RST_Pin,PCF8833_RST_Speed,PCF8833_RST_Mode},PCF8833_RST_Port,PCF8833_RST_Bus},
};

static unsigned char *FontTable[] = {
  (unsigned char *)FONT6x8,
  (unsigned char *)FONT8x8,
  (unsigned char *)FONT8x14
};

static uint8_t buf_in[9];
static uint8_t inbuf=0;

static void PCF8833_Reset(void);
static void PCF8833_GPIO_SPI_Config(void);
static void PCF8833_GPIO_Bitbang_Config(void);

// Color maps
//
// for Nokia 3100 6100 6030 LCD in 8bit mode
uint8_t RGB8ColorMap[20]= {
  0x00,0x02,0x05,0x07,0x09,0x0B,0x0E,0x0F, // red
  0x00,0x02,0x05,0x07,0x09,0x0B,0x0E,0x0F, // green
  0x00,0x06,0x0B,0x0F                      // blue
};

// only for Nokia 6020 LCD in 8bit mode
uint8_t RGB8ColorMap_Nokia6020[48]= {
  0x00,0x04,0x09,0x0D,0x12,0x16,0x1B,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // red
  0x00,0x09,0x12,0x1B,0x24,0x2D,0x36,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // green
  0x00,0x0A,0x15,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // blue
};

// only for Nokia 6020 LCD in 12bit mode
uint8_t RGB12ColorMap_Nokia6020[48]= {
  0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1F, // red
  0x00,0x07,0x0B,0x0F,0x13,0x17,0x1B,0x1F,0x23,0x27,0x2B,0x2F,0x33,0x37,0x3B,0x3F, // green
  0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1F  // blue
};


void PCF8833_Command_Bitbang(uint8_t command) {

  uint32_t Bit;

  PCF8833_SDATA_Port->BRR = PCF8833_SDATA_Pin;
  PCF8833_SCLK_Port->BSRR = PCF8833_SCLK_Pin;

  for (Bit = (1<<7); Bit; Bit>>=1) {

    PCF8833_SCLK_Port->BRR = PCF8833_SCLK_Pin;

    if(command&Bit) {
      PCF8833_SDATA_Port->BSRR = PCF8833_SDATA_Pin;
    }
    else {
      PCF8833_SDATA_Port->BRR = PCF8833_SDATA_Pin;
    }
    PCF8833_SCLK_Port->BSRR = PCF8833_SCLK_Pin;
  }
  PCF8833_SCLK_Port->BRR = PCF8833_SCLK_Pin;
}

void PCF8833_Data_Bitbang(uint8_t data) {

  uint32_t Bit;

  PCF8833_SDATA_Port->BSRR = PCF8833_SDATA_Pin;
  PCF8833_SCLK_Port->BSRR = PCF8833_SCLK_Pin;

  for (Bit = (1<<7); Bit; Bit>>=1) {

    PCF8833_SCLK_Port->BRR = PCF8833_SCLK_Pin;

    if(data&Bit) {
      PCF8833_SDATA_Port->BSRR = PCF8833_SDATA_Pin;
    }
    else {
      PCF8833_SDATA_Port->BRR = PCF8833_SDATA_Pin;
    }
    PCF8833_SCLK_Port->BSRR = PCF8833_SCLK_Pin;
  }
  PCF8833_SCLK_Port->BRR = PCF8833_SCLK_Pin;
}

void PCF8833_SPI9bits(uint16_t bits9) {

  buf_in[inbuf] |= bits9 >> (1+inbuf);
  buf_in[inbuf+1] = bits9 << (7-inbuf);
  inbuf++;
  if (inbuf==8) {

    uint32_t i;
    for(i=0;i<9;i++) {
      while (!(PCF8833_SPI->SR & SPI_I2S_FLAG_TXE)){};
      PCF8833_SPI->DR = buf_in[i];
      buf_in[i]=0;
    }
    inbuf=0;
  }
}

void PCF8833_SPI9bits_Flush(void) {

  uint32_t i;

  for(i=0;i<9;i++) {
    while (!(PCF8833_SPI->SR & SPI_I2S_FLAG_TXE)){};
    PCF8833_SPI->DR = buf_in[i];
    buf_in[i]=0;
  }
  inbuf=0;
}

void PCF8833_SetupColor(uint8_t *color_map,uint8_t size) {

  uint32_t i;
  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_RGBSET);
      for(i=0;i<size;i++) {
        PCF8833_Data_Bitbang(color_map[i]);
      }
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_RGBSET);
      for(i=0;i<size;i++) {
        PCF8833_SPI9bits(color_map[i]|0x100);
      }
      break;
  }
}

static void PCF8833_Reset(void) {

  GPIO_ResetBits(PCF8833_RST_Port, PCF8833_RST_Pin);
  DWT_Delay(1000);
  GPIO_SetBits(PCF8833_RST_Port, PCF8833_RST_Pin);
  DWT_Delay(10000);
}

static void PCF8833_GPIO_SPI_Config(void) {

  uint32_t i;

  SPI_InitTypeDef  SPI_InitStructure;

  for(i=0;i<sizeof(pins_spi)/sizeof(PCF8833_PIN);i++) {
    RCC_APB2PeriphClockCmd(pins_spi[i].GPIO_Bus,ENABLE);
    GPIO_Init(pins_spi[i].GPIOx,&pins_spi[i].GPIO_InitStructure);
  }

  PCF8833_SPI_Bus_Enable;

  SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(PCF8833_SPI, &SPI_InitStructure);
  SPI_SSOutputCmd(PCF8833_SPI,ENABLE);
  SPI_Cmd(PCF8833_SPI, ENABLE);
}

static void PCF8833_GPIO_Bitbang_Config(void) {

  uint32_t i;

  SPI_Cmd(PCF8833_SPI, DISABLE);
  PCF8833_SPI_Bus_Disable;

  for(i=0;i<sizeof(pins_bb)/sizeof(PCF8833_PIN);i++) {
    RCC_APB2PeriphClockCmd(pins_bb[i].GPIO_Bus,ENABLE);
    GPIO_Init(pins_bb[i].GPIOx,&pins_bb[i].GPIO_InitStructure);
  }
	PCF8833_SCLK_Port->BRR = PCF8833_SCLK_Pin;
}

void PCF8833_Init(PCF8833_ACCESS_MODE access_mode) {

  PCF8833_access_mode=access_mode;

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_GPIO_Bitbang_Config();
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_GPIO_SPI_Config();
      break;
  }

  PCF8833_Reset();

  GPIO_ResetBits(PCF8833_CS_Port, PCF8833_CS_Pin);

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_SLEEPOUT);
      DWT_Delay(10000);
      PCF8833_Command_Bitbang(PCF8833_DISPON);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_SLEEPOUT);
      PCF8833_SPI9bits_Flush();
      DWT_Delay(10000);
      PCF8833_SPI9bits(PCF8833_DISPON);
      PCF8833_SPI9bits_Flush();
      break;
  }
}

void PCF8833_DisplayInversion(PCF8833_DISPLAY_INVERSION display_inversion) {

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(display_inversion);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(display_inversion);
    break;
  }
}

void PCF8833_ColorMode(PCF8833_COLOR_MODE color_mode) {

  PCF8833_color_mode=color_mode;

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_COLMOD);
      PCF8833_Data_Bitbang(color_mode);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_COLMOD);
      PCF8833_SPI9bits(color_mode|0x100);
    break;
  }
}

void PCF8833_ClearScreen(uint16_t color) {

  uint16_t i;
  uint8_t data1,data2,data3;

  PCF8833_SetWindow(0,0,131,131);

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      switch(PCF8833_color_mode) {
        case PCF8833_COLOR_8BIT:
          data1=((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03);
          for(i=0;i<((132*132));i++) {
            PCF8833_Data_Bitbang(data1);
          }
          break;
        case PCF8833_COLOR_12BIT:
          data1=((color>>8)&0xF0)|((color>>7)&0x0F);
          data2=((color<<3)&0xF0)|(color>>12);
          data3=((color>>3)&0xF0)|((color>>1)&0x0F);
          for(i=0;i<((132*132)/2);i++) {
            PCF8833_Data_Bitbang(data1);
            PCF8833_Data_Bitbang(data2);
            PCF8833_Data_Bitbang(data3);
          }
          break;
        case PCF8833_COLOR_16BIT:
          data1=color>>8;
          data2=color;
          for(i=0;i<((132*132));i++) {
            PCF8833_Data_Bitbang(data1);
            PCF8833_Data_Bitbang(data2);
          }
          break;
      }
      break;
    case PCF8833_ACCESS_SPI9BITS:
      switch(PCF8833_color_mode) {
        case PCF8833_COLOR_8BIT:
          data1=((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03);
          for(i=0;i<((132*132));i++) {
            PCF8833_SPI9bits(data1|0x100);
          }
          break;
        case PCF8833_COLOR_12BIT:
          data1=((color>>8)&0xF0)|((color>>7)&0x0F);
          data2=((color<<3)&0xF0)|(color>>12);
          data3=((color>>3)&0xF0)|((color>>1)&0x0F);
          for(i=0;i<((132*132)/2);i++) {
            PCF8833_SPI9bits(data1|0x100);
            PCF8833_SPI9bits(data2|0x100);
            PCF8833_SPI9bits(data3|0x100);
          }
          break;
        case PCF8833_COLOR_16BIT:
          data1=color>>8;
          data2=color;
          for(i=0;i<((132*132));i++) {
            PCF8833_SPI9bits(data1|0x100);
            PCF8833_SPI9bits(data2|0x100);
          }
          break;
      }
      PCF8833_SPI9bits_Flush();
      break;
  }
}

void PCF8833_SetFont(PCF8833_FONT_SIZE font_size) {

  PCF8833_font_size=font_size;
}

void PCF8833_SetTextColours(uint16_t fColor, uint16_t bColor) {

  PCF8833_text_foreground_color = fColor;
  PCF8833_text_background_color = bColor;
}

void PCF8833_PutChar(char c, uint8_t x, uint8_t y) {

  uint32_t i,j;
  uint32_t nCols;
  uint32_t nRows;
  uint32_t nBytes;
  uint8_t PixelRow;
  uint8_t Mask;
  uint16_t Word0;
  uint16_t Word1;
  unsigned char *pFont;
  unsigned char *pChar;
  uint8_t data1,data2,data3;

  // get pointer to the beginning of the selected font table
  pFont = (unsigned char *)FontTable[PCF8833_font_size];

  // get the nColumns, nRows and nBytes
  nCols = *pFont;
  nRows = *(pFont + 1);
  nBytes = *(pFont + 2);

  // get pointer to the first byte of the desired character
  pChar = pFont + (nBytes * (c - 0x1F));

  PCF8833_SetWindow(x,y,x + nCols - 1,y + nRows - 1);

  // loop on each row
  for (i = 0; i < nRows; i++)
  {
    // copy pixel row from font table and then decrement row
    PixelRow = *pChar++;

    // loop on each pixel in the row (left to right)
    // Note: we do two pixels each loop
    Mask = 0x80;
    for (j = 0; j < nCols; j += 2)
    {
      // if pixel bit set, use foreground color; else use the background color
      // now get the pixel color for two successive pixels
      if (PixelRow & Mask)
        Word0 = PCF8833_text_foreground_color;
      else
        Word0 = PCF8833_text_background_color;
      Mask >>= 1;

      if (PixelRow & Mask)
        Word1 = PCF8833_text_foreground_color;
      else
        Word1 = PCF8833_text_background_color;
      Mask >>= 1;

      switch(PCF8833_access_mode) {
        case PCF8833_ACCESS_BITBANG:
          switch(PCF8833_color_mode) {
            case PCF8833_COLOR_8BIT:
              data1=((Word0>>8)&0xE0)|((Word0>>6)&0x1C)|((Word0>>3)&0x03);
              PCF8833_Data_Bitbang(data1);
              data1=((Word1>>8)&0xE0)|((Word1>>6)&0x1C)|((Word1>>3)&0x03);
              PCF8833_Data_Bitbang(data1);
              break;
            case PCF8833_COLOR_12BIT:
              data1=((Word0>>8)&0xF0)|((Word0>>7)&0x0F);
              data2=((Word0<<3)&0xF0)|(Word1>>12);
              data3=((Word1>>3)&0xF0)|((Word1>>1)&0x0F);
              PCF8833_Data_Bitbang(data1);
              PCF8833_Data_Bitbang(data2);
              PCF8833_Data_Bitbang(data3);
              break;
            case PCF8833_COLOR_16BIT:
              data1=Word0>>8;
              data2=Word0;
              PCF8833_Data_Bitbang(data1);
              PCF8833_Data_Bitbang(data1);
              data1=Word1>>8;
              data2=Word1;
              PCF8833_Data_Bitbang(data1);
              PCF8833_Data_Bitbang(data2);
              break;
          }
          break;
        case PCF8833_ACCESS_SPI9BITS:
          switch(PCF8833_color_mode) {
            case PCF8833_COLOR_8BIT:
              data1=((Word0>>8)&0xE0)|((Word0>>6)&0x1C)|((Word0>>3)&0x03);
              PCF8833_SPI9bits(data1|0x100);
              data1=((Word1>>8)&0xE0)|((Word1>>6)&0x1C)|((Word1>>3)&0x03);
              PCF8833_SPI9bits(data1|0x100);
              break;
            case PCF8833_COLOR_12BIT:
              data1=((Word0>>8)&0xF0)|((Word0>>7)&0x0F);
              data2=((Word0<<3)&0xF0)|(Word1>>12);
              data3=((Word1>>3)&0xF0)|((Word1>>1)&0x0F);
              PCF8833_SPI9bits(data1|0x100);
              PCF8833_SPI9bits(data2|0x100);
              PCF8833_SPI9bits(data3|0x100);
              break;
            case PCF8833_COLOR_16BIT:
              data1=Word0>>8;
              data2=Word0;
              PCF8833_SPI9bits(data1|0x100);
              PCF8833_SPI9bits(data2|0x100);
              data1=Word1>>8;
              data2=Word1;
              PCF8833_SPI9bits(data1|0x100);
              PCF8833_SPI9bits(data2|0x100);
              break;
          }
          break;
      }
    }
  }
  if(PCF8833_access_mode==PCF8833_ACCESS_SPI9BITS)
    PCF8833_SPI9bits_Flush();
}

void PCF8833_PutStr(char *pString, uint8_t x, uint8_t y) {

  if(y+FontTable[PCF8833_font_size][1]>PCF8833_GetHeight()-1)
    return;

  // loop until null-terminator is seen
  while (*pString) {
    if (x+FontTable[PCF8833_font_size][0]>PCF8833_GetWidth()-1) break;
    // draw the character
    PCF8833_PutChar(*pString++, x, y);
    x+=FontTable[PCF8833_font_size][0];
  }
}

void PCF8833_SetPixel(uint8_t x, uint8_t y, uint16_t color) {

  PCF8833_SetWindow(x,y,x,y);

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      switch(PCF8833_color_mode) {
        case PCF8833_COLOR_8BIT:
          PCF8833_Data_Bitbang(((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03));
          break;
        case PCF8833_COLOR_12BIT:
          PCF8833_Data_Bitbang(((color>>8)&0xF0)|((color>>7)&0x0F));
          PCF8833_Data_Bitbang((color<<3)&0xF0);
          PCF8833_Command_Bitbang(PCF8833_NOP);
          break;
        case PCF8833_COLOR_16BIT:
          PCF8833_Data_Bitbang(color>>8);
          PCF8833_Data_Bitbang(color);
          break;
      }
      break;
    case PCF8833_ACCESS_SPI9BITS:
     switch(PCF8833_color_mode) {
        case PCF8833_COLOR_8BIT:
          PCF8833_SPI9bits(((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03)|0x100);
          break;
        case PCF8833_COLOR_12BIT:
          PCF8833_SPI9bits(((color>>8)&0xF0)|((color>>7)&0x0F)|0x100);
          PCF8833_SPI9bits(((color<<3)&0xF0)|0x100);
          break;
        case PCF8833_COLOR_16BIT:
          PCF8833_SPI9bits((color>>8)|0x100);
          PCF8833_SPI9bits((color&0xFF)|0x100);
          break;
      }
      break;
  }
}

void PCF8833_Line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color) {

  int dy = y1 - y0;
  int dx = x1 - x0;
  int stepx, stepy;
  if (dy < 0) { dy = -dy; stepy = -1; } else { stepy = 1; }
  if (dx < 0) { dx = -dx; stepx = -1; } else { stepx = 1; }
  dy <<= 1; // dy is now 2*dy
  dx <<= 1; // dx is now 2*dx
  PCF8833_SetPixel(x0, y0, color);
  if (dx > dy)
  {
    int fraction = dy - (dx >> 1); // same as 2*dy - dx
    while (x0 != x1)
    {
      if (fraction >= 0)
      {
        y0 += stepy;
        fraction -= dx; // same as fraction -= 2*dx
      }
      x0 += stepx;
      fraction += dy; // same as fraction -= 2*dy
      PCF8833_SetPixel(x0, y0, color);
    }
  }
  else
  {
    int fraction = dx - (dy >> 1);
    while (y0 != y1)
    {
      if (fraction >= 0)
      {
        x0 += stepx;
        fraction -= dy;
      }
      y0 += stepy;
      fraction += dx;
      PCF8833_SetPixel(x0, y0, color);
    }
  }
}

void PCF8833_Rectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t fill, uint16_t color) {

  uint8_t xmin, xmax, ymin, ymax;
  uint32_t i,j;
  uint8_t data1,data2,data3;

  // check if the rectangle is to be filled
  if (fill) {
    // best way to create a filled rectangle is to define a drawing box
    // and loop two pixels at a time
    // calculate the min and max for x and y directions
    xmin = (x0 <= x1) ? x0 : x1;
    xmax = (x0 > x1) ? x0 : x1;
    ymin = (y0 <= y1) ? y0 : y1;
    ymax = (y0 > y1) ? y0 : y1;

    // specify the controller drawing box according to those limits
    PCF8833_SetWindow(xmin,ymin,xmax,ymax);

    j=(xmax-xmin+1)*(ymax-ymin+1);

    switch(PCF8833_access_mode) {
      case PCF8833_ACCESS_BITBANG:
        switch(PCF8833_color_mode) {
          case PCF8833_COLOR_8BIT:
            data1=((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03);
            for(i=0;i<j;i++) {
              PCF8833_Data_Bitbang(data1);
            }
            break;
          case PCF8833_COLOR_12BIT:
            data1=((color>>8)&0xF0)|((color>>7)&0x0F);
            data2=((color<<3)&0xF0)|(color>>12);
            data3=((color>>3)&0xF0)|((color>>1)&0x0F);
            for(i=0;i<j/2+1;i++) {
              PCF8833_Data_Bitbang(data1);
              PCF8833_Data_Bitbang(data2);
              PCF8833_Data_Bitbang(data3);
            }
            break;
          case PCF8833_COLOR_16BIT:
            data1=color>>8;
            data2=color;
            for(i=0;i<j;i++) {
              PCF8833_Data_Bitbang(data1);
              PCF8833_Data_Bitbang(data2);
            }
            break;
        }
        break;
      case PCF8833_ACCESS_SPI9BITS:
        switch(PCF8833_color_mode) {
          case PCF8833_COLOR_8BIT:
            data1=((color>>8)&0xE0)|((color>>6)&0x1C)|((color>>3)&0x03);
            for(i=0;i<j;i++) {
              PCF8833_SPI9bits(data1|0x100);
            }
            break;
          case PCF8833_COLOR_12BIT:
            data1=((color>>8)&0xF0)|((color>>7)&0x0F);
            data2=((color<<3)&0xF0)|(color>>12);
            data3=((color>>3)&0xF0)|((color>>1)&0x0F);
            for(i=0;i<j/2+1;i++) {
              PCF8833_SPI9bits(data1|0x100);
              PCF8833_SPI9bits(data2|0x100);
              PCF8833_SPI9bits(data3|0x100);
            }
            break;
          case PCF8833_COLOR_16BIT:
            data1=color>>8;
            data2=color;
            for(i=0;i<j;i++) {
              PCF8833_SPI9bits(data1|0x100);
              PCF8833_SPI9bits(data2|0x100);
            }
            break;
        }
        break;
    }
  }
  else {
    // best way to draw un unfilled rectangle is to draw four lines
    PCF8833_Line(x0, y0, x1, y0, color);
    PCF8833_Line(x0, y1, x1, y1, color);
    PCF8833_Line(x0, y0, x0, y1, color);
    PCF8833_Line(x1, y0, x1, y1, color);
  }
}

void PCF8833_Circle(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t color) {

  int f = 1 - radius;
  int ddF_x = 0;
  int ddF_y = -2 * radius;
  int x = 0;
  int y = radius;
  PCF8833_SetPixel(x0, y0 + radius, color);
  PCF8833_SetPixel(x0, y0 - radius, color);
  PCF8833_SetPixel(x0 + radius, y0, color);
  PCF8833_SetPixel(x0 - radius, y0, color);
  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x + 1;
    PCF8833_SetPixel(x0 + x, y0 + y, color);
    PCF8833_SetPixel(x0 - x, y0 + y, color);
    PCF8833_SetPixel(x0 + x, y0 - y, color);
    PCF8833_SetPixel(x0 - x, y0 - y, color);
    PCF8833_SetPixel(x0 + y, y0 + x, color);
    PCF8833_SetPixel(x0 - y, y0 + x, color);
    PCF8833_SetPixel(x0 + y, y0 - x, color);
    PCF8833_SetPixel(x0 - y, y0 - x, color);
  }
}

void PCF8833_SetOrientation(PCF8833_ORIENTATION_MODE orientation_mode,uint8_t mirror) {

  PCF8833_orientation_mode=orientation_mode;
  PCF8833_v=(orientation_mode&0x20)>>5;
  PCF8833_mirror=mirror;

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_MADCTL);
      PCF8833_Data_Bitbang(PCF8833_orientation_mode|PCF8833_rgb_mode);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_MADCTL);
      PCF8833_SPI9bits(PCF8833_orientation_mode|PCF8833_rgb_mode|0x100);
      PCF8833_SPI9bits_Flush();
      break;
  }
}

void PCF8833_SetRGB(PCF8833_RGB_MODE rgb_mode) {

  PCF8833_rgb_mode=rgb_mode;
  PCF8833_SetOrientation(PCF8833_orientation_mode,PCF8833_mirror);
}

void PCF8833_SetContrast(uint8_t contrast) {

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_SETCON);
      PCF8833_Data_Bitbang(contrast);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_SETCON);
      PCF8833_SPI9bits(contrast|0x100);
      PCF8833_SPI9bits_Flush();
      break;
  }
}

void PCF8833_Sleep(void) {

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_DISPOFF);
      PCF8833_Command_Bitbang(PCF8833_SLEEPIN);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_DISPOFF);
      PCF8833_SPI9bits(PCF8833_SLEEPIN);
      PCF8833_SPI9bits_Flush();
      break;
  }
}
void PCF8833_Wakeup(void) {

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      PCF8833_Command_Bitbang(PCF8833_SLEEPOUT);
      PCF8833_Command_Bitbang(PCF8833_DISPON);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      PCF8833_SPI9bits(PCF8833_SLEEPOUT);
      PCF8833_SPI9bits(PCF8833_DISPON);
      PCF8833_SPI9bits_Flush();
      break;
  }
}

void PCF8833_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {

  switch(PCF8833_access_mode) {
    case PCF8833_ACCESS_BITBANG:
      if(PCF8833_v^PCF8833_mirror) {
        PCF8833_Command_Bitbang(PCF8833_CASET);
      }
      else {
        PCF8833_Command_Bitbang(PCF8833_PASET);
      }

      PCF8833_Data_Bitbang(y0);
      PCF8833_Data_Bitbang(y1);
      if(PCF8833_v^PCF8833_mirror) {
        PCF8833_Command_Bitbang(PCF8833_PASET);
      }
      else {
        PCF8833_Command_Bitbang(PCF8833_CASET);
      }
      PCF8833_Data_Bitbang(x0);
      PCF8833_Data_Bitbang(x1);
      PCF8833_Command_Bitbang(PCF8833_RAMWR);
      break;
    case PCF8833_ACCESS_SPI9BITS:
      if(PCF8833_v^PCF8833_mirror) {
        PCF8833_SPI9bits(PCF8833_CASET);
      }
      else {
        PCF8833_SPI9bits(PCF8833_PASET);
      }
      PCF8833_SPI9bits(y0|0x100);
      PCF8833_SPI9bits(y1|0x100);
      if(PCF8833_v^PCF8833_mirror) {
        PCF8833_SPI9bits(PCF8833_PASET);
      }
      else {
        PCF8833_SPI9bits(PCF8833_CASET);
      }
      PCF8833_SPI9bits(x0|0x100);
      PCF8833_SPI9bits(x1|0x100);
      PCF8833_SPI9bits(PCF8833_RAMWR);
    break;
  }
}

uint8_t PCF8833_GetWidth(void) {

  return 132;
}

uint8_t PCF8833_GetHeight(void) {

  return 132;
}
