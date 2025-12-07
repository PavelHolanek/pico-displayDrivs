#ifndef gfx_H
#define gfx_H

#include "pico/stdlib.h"
#include "gfxfont.h"
#include "ili9488.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_MAX_SIZE 120000 //120 kB 

//There can be anything in the memory after initialization, so make it sure that all buffer is used or you risk random pixels
void GFX_createFramebuf(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void GFX_destroyFramebuf();
void GFX_flush();

void GFX_drawPixel(int16_t x, int16_t y, struct Color color);

void addExtraCharacter(wchar_t c);
char getCharForWideChar(wchar_t wc);

void GFX_drawChar(int16_t x, int16_t y, unsigned char c, struct Color color, struct Color bg, uint8_t size_x, uint8_t size_y);
void GFX_write(uint8_t c, uint8_t textsize);
void GFX_setCursor(int16_t x, int16_t y);
void GFX_setTextColor(struct Color color);
void GFX_setTextBack(struct Color color);
void GFX_setFont(const GFXfont *f);

void GFX_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, struct Color color);
void GFX_drawFastVLine(int16_t x, int16_t y, int16_t h, struct Color color);
void GFX_drawFastHLine(int16_t x, int16_t y, int16_t l, struct Color color);

void GFX_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, struct Color color);
void GFX_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, struct Color color);
void GFX_fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, struct Color color);

void GFX_fillScreen(struct Color color);
void GFX_setClearColor(struct Color color);
void GFX_clearScreen();

void GFX_drawCircle(int16_t x0, int16_t y0, int16_t r, struct Color color);
void GFX_fillCircle(int16_t x0, int16_t y0, int16_t r, struct Color color);

void GFX_printf(uint8_t textsize, const char *format, ...);


uint GFX_getWidth();
uint GFX_getHeight();

#ifdef __cplusplus
}
#endif

#endif
