#ifndef ILI9488_H
#define ILI9488_H
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Use DMA?
//#define USE_DMA 1

#define MADCTL_MY 0x80  ///< Bottom to top
#define MADCTL_MX 0x40  ///< Right to left
#define MADCTL_MV 0x20  ///< Reverse Mode
#define MADCTL_ML 0x10  ///< LCD refresh Bottom to top
#define MADCTL_RGB 0x00 ///< Red-Green-Blue pixel order
#define MADCTL_BGR 0x08 ///< Blue-Green-Red pixel order
#define MADCTL_MH 0x04  ///< LCD refresh right to left


#define ILI9488_TFTWIDTH 320  ///< ILI9488 max TFT width
#define ILI9488_TFTHEIGHT 480 ///< ILI9488 max TFT height

#define ILI9488_NOP 0x00     ///< No-op register
#define ILI9488_SWRESET 0x01 ///< Software reset register
#define ILI9488_RDDID 0x04   ///< Read display identification information
#define ILI9488_RDDST 0x09   ///< Read Display Status

#define ILI9488_SLPIN 0x10  ///< Enter Sleep Mode
#define ILI9488_SLPOUT 0x11 ///< Sleep Out
#define ILI9488_PTLON 0x12  ///< Partial Mode ON
#define ILI9488_NORON 0x13  ///< Normal Display Mode ON

#define ILI9488_RDMODE 0x0A     ///< Read Display Power Mode
#define ILI9488_RDMADCTL 0x0B   ///< Read Display MADCTL
#define ILI9488_RDPIXFMT 0x0C   ///< Read Display Pixel Format
#define ILI9488_RDIMGFMT 0x0D   ///< Read Display Image Format
#define ILI9488_RDSELFDIAG 0x0F ///< Read Display Self-Diagnostic Result

#define ILI9488_INVOFF 0x20   ///< Display Inversion OFF
#define ILI9488_INVON 0x21    ///< Display Inversion ON
#define ILI9488_GAMMASET 0x26 ///< Gamma Set
#define ILI9488_DISPOFF 0x28  ///< Display OFF
#define ILI9488_DISPON 0x29   ///< Display ON

#define ILI9488_CASET 0x2A ///< Column Address Set
#define ILI9488_PASET 0x2B ///< Page Address Set
#define ILI9488_RAMWR 0x2C ///< Memory Write
#define ILI9488_RAMRD 0x2E ///< Memory Read

#define ILI9488_PTLAR 0x30    ///< Partial Area
#define ILI9488_VSCRDEF 0x33  ///< Vertical Scrolling Definition
#define ILI9488_MADCTL 0x36   ///< Memory Access Control
#define ILI9488_VSCRSADD 0x37 ///< Vertical Scrolling Start Address
#define ILI9488_PIXFMT 0x3A   ///< COLMOD: Pixel Format Set

#define ILI9488_FRMCTR1                                                        \
  0xB1 ///< Frame Rate Control (In Normal Mode/Full Colors)
#define ILI9488_FRMCTR2 0xB2 ///< Frame Rate Control (In Idle Mode/8 colors)
#define ILI9488_FRMCTR3                                                        \
  0xB3 ///< Frame Rate control (In Partial Mode/Full Colors)
#define ILI9488_INVCTR 0xB4  ///< Display Inversion Control
#define ILI9488_DFUNCTR 0xB6 ///< Display Function Control

#define ILI9488_PWCTR1 0xC0 ///< Power Control 1
#define ILI9488_PWCTR2 0xC1 ///< Power Control 2
#define ILI9488_PWCTR3 0xC2 ///< Power Control 3
#define ILI9488_PWCTR4 0xC3 ///< Power Control 4
#define ILI9488_PWCTR5 0xC4 ///< Power Control 5
#define ILI9488_VMCTR1 0xC5 ///< VCOM Control 1
#define ILI9488_VMCTR2 0xC7 ///< VCOM Control 2

#define ILI9488_RDID1 0xDA ///< Read ID 1
#define ILI9488_RDID2 0xDB ///< Read ID 2
#define ILI9488_RDID3 0xDC ///< Read ID 3
#define ILI9488_RDID4 0xDD ///< Read ID 4

#define ILI9488_GMCTRP1 0xE0 ///< Positive Gamma Correction
#define ILI9488_GMCTRN1 0xE1 ///< Negative Gamma Correction

// Some ready-made 16-bit ('565') color settings:
#define ILI9488_BLACK 0x0000
#define ILI9488_WHITE 0xFFFF
#define ILI9488_RED 0xF800
#define ILI9488_GREEN 0x07E0
#define ILI9488_BLUE 0x001F
#define ILI9488_CYAN 0x07FF
#define ILI9488_MAGENTA 0xF81F
#define ILI9488_YELLOW 0xFFE0
#define ILI9488_ORANGE 0xFC00

#ifdef __cplusplus
extern "C" {
#endif
struct Color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

extern uint16_t _width;
extern uint16_t _height;

void LCD_setPins(uint16_t dc, uint16_t cs, int16_t rst, uint16_t sck, uint16_t tx);
void LCD_setSPIperiph(spi_inst_t *s);
void LCD_initDisplay();

void LCD_setRotation(uint8_t m);

void LCD_WritePixel(int x, int y, struct Color col);
void LCD_WriteBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *bitmap);

#ifdef __cplusplus
}
#endif

#endif
