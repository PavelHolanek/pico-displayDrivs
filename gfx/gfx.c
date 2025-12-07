#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "malloc.h"
#include "stdarg.h"
#include "gfx.h"
#include "font.h"
#include "gfxfont.h"
#include "hardware/dma.h"

// Declare methods from the display drivers
extern void LCD_WritePixel(int x, int y, struct Color col);
extern void LCD_WriteBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *bitmap);
extern uint16_t _width;	 ///< Display width as modified by current rotation
extern uint16_t _height; ///< Display height as modified by current rotation

bool isEqual(struct Color c1, struct Color c2)
{
	return c1.b == c2.b && c1.r == c2.r && c1.g == c2.g;
}

bool isNotEqual(struct Color c1, struct Color c2)
{
	return c1.b != c2.b || c1.r != c2.r || c1.g != c2.g;
}

#ifndef swap
#define swap(a, b)     \
	{                  \
		int16_t t = a; \
		a = b;         \
		b = t;         \
	}
#endif

#define GFX_BLACK {0x00, 0x00, 0x00}
#define GFX_WHITE {0xFF, 0xFF, 0xFF}

static int memcpy_dma_chan;
static bool gfx_dma_init = false;

uint8_t  frameBufferX = 0;
uint8_t  frameBufferY = 0;
uint8_t  frameBufferWidth = 0;
uint8_t  frameBufferHeight = 0;

uint8_t *gfxFramebuffer = NULL;

static int16_t cursor_y = 0;
int16_t cursor_x = 0;
struct Color textcolor = GFX_WHITE;
struct Color textbgcolor = GFX_BLACK;
struct Color clearColour = GFX_BLACK;
uint8_t wrap = 1;

GFXfont *gfxFont = NULL;

wchar_t wideCharacters[128] = {};
uint8_t extraCharacters = 0;

char getCharForWideChar(wchar_t wc)
{
	for (uint8_t i = 0; i < 128; i++)
	{
		if (wc == wideCharacters[i])
		{
			return i + 128;
		}
	}
	return '?';
}

typedef enum
{
    DIACRITIC_NONE = 0,
    DIACRITIC_ACUTE,  		// ´
    DIACRITIC_CARON,  		// ˇ
    DIACRITIC_DOT,    		// .
} DIACRITIC;

void addExtraCharacter(wchar_t c)
{
	if(extraCharacters < 128)
	{
		wideCharacters[extraCharacters] = c;
		extraCharacters++;
	} 
}

uint GFX_getWidth()
{
	return _width;
}

uint GFX_getHeight()
{
	return _height;
}

void GFX_setClearColor(struct Color color)
{
	clearColour = color;
}

void GFX_clearScreen()
{
	GFX_fillRect(0, 0, _width, _height, clearColour);
}

void GFX_fillScreen(struct Color color)
{
	GFX_fillRect(0, 0, _width, _height, color);
}

void GFX_drawPixel(int16_t x, int16_t y, struct Color color)
{
	if (gfxFramebuffer != NULL)
	{
		if ((x < frameBufferX) || (y < frameBufferY) || (x >= frameBufferX + frameBufferWidth) || (y >= frameBufferY + frameBufferHeight))
		{
			LCD_WritePixel(x, y, color);
		}
		else
		{
			uint16_t relativeX = x - frameBufferX;
			uint16_t relativeY = y - frameBufferY;
			gfxFramebuffer[(relativeX + relativeY * frameBufferWidth) * 3] = color.r;
			gfxFramebuffer[(relativeX + relativeY * frameBufferWidth) * 3 + 1] = color.g;
			gfxFramebuffer[(relativeX + relativeY * frameBufferWidth) * 3 + 2] = color.b;
		}
	}
	else
	{
		LCD_WritePixel(x, y, color);
	}
		
}

void GFX_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, struct Color color)
{
	int16_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep)
	{
		swap(x0, y0);
		swap(x1, y1);
	}

	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y0 < y1)
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}

	for (; x0 <= x1; x0++)
	{
		if (steep)
		{
			GFX_drawPixel(y0, x0, color);
		}
		else
		{
			GFX_drawPixel(x0, y0, color);
		}
		err -= dy;
		if (err < 0)
		{
			y0 += ystep;
			err += dx;
		}
	}
}

void GFX_drawFastVLine(int16_t x, int16_t y, int16_t h, struct Color color)
{
	GFX_drawLine(x, y, x, y + h - 1, color);
}

void GFX_drawFastHLine(int16_t x, int16_t y, int16_t l, struct Color color)
{
	GFX_drawLine(x, y, x + l - 1, y, color);
}

void GFX_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, struct Color color)
{
	for (int16_t i = x; i < x + w; i++)
	{
		GFX_drawFastVLine(i, y, h, color);
	}
}

void fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
					  uint8_t corners, int16_t delta,
					  struct Color color)
{

	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;
	int16_t px = x;
	int16_t py = y;

	delta++; // Avoid some +1's in the loop

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
		f += ddF_x;
		// These checks avoid double-drawing certain lines, important
		// for the SSD1306 library which has an INVERT drawing mode.
		if (x < (y + 1))
		{
			if (corners & 1)
				GFX_drawFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
			if (corners & 2)
				GFX_drawFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
		}
		if (y != py)
		{
			if (corners & 1)
				GFX_drawFastVLine(x0 + py, y0 - px, 2 * px + delta, color);
			if (corners & 2)
				GFX_drawFastVLine(x0 - py, y0 - px, 2 * px + delta, color);
			py = y;
		}
		px = x;
	}
}

void GFX_fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, struct Color color)
{
	if (w <= 0 || h <= 0) return;

	if (r <= 0) {
		GFX_fillRect(x, y, w, h, color);
		return;
	}

	if (r > w / 2) r = w / 2;
	if (r > h / 2) r = h / 2;

	// Fill the central rectangle (between rounded corners)
	GFX_fillRect(x + r, y, w - 2 * r, h, color);

	// Fill the rounded corners using helper (extends vertical spans by delta)
	fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color); // Right corners
	fillCircleHelper(x + r,           y + r, r, 2, h - 2 * r - 1, color); // Left corners
}

void GFX_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, struct Color color)
{
	GFX_drawFastHLine(x, y, w, color);
	GFX_drawFastHLine(x, y + h - 1, w, color);
	GFX_drawFastVLine(x, y, h, color);
	GFX_drawFastVLine(x + w - 1, y, h, color);
}

unsigned char solveDiacritic(wchar_t wc, DIACRITIC* d) {
    // Map characters with diacritics to their base character and diacritic type
	int i = wc;
    switch (i)
	{
        case L'Á':
            *d = DIACRITIC_ACUTE;
            return 'A';
        case L'Ó':
            *d = DIACRITIC_ACUTE;
            return 'O';
        case L'Í':
            *d = DIACRITIC_ACUTE;
            return 'I';
        case L'É':
            *d = DIACRITIC_ACUTE;
            return 'E';
		case L'Ú':
            *d = DIACRITIC_ACUTE;
            return 'U';
        case L'Ů':
            *d = DIACRITIC_DOT;
            return 'U';
        case  L'Ě':
            *d = DIACRITIC_CARON;
            return 'E';
        case  L'Ř':
            *d = DIACRITIC_CARON;
            return 'R';
        case  L'Š':
            *d = DIACRITIC_CARON;
            return 'S';
        case  L'Č':
            *d = DIACRITIC_CARON;
            return 'C';
        case  L'Ž':
            *d = DIACRITIC_CARON;
            return 'Z';
        case  L'Ň':
            *d = DIACRITIC_CARON;
            return 'N';
        case  L'Ď':
            *d = DIACRITIC_CARON;
            return 'D';
        case  L'Ť':
            *d = DIACRITIC_CARON;
            return 'T';
		case  L'Ý':
            *d = DIACRITIC_ACUTE;
            return 'Y';
		case L'á':
        	*d = DIACRITIC_ACUTE;
        	return 'a';
		case L'ó':
			*d = DIACRITIC_ACUTE;
			return 'o';
		case L'í':
			*d = DIACRITIC_ACUTE;
			return 'i';
		case L'é':
			*d = DIACRITIC_ACUTE;
			return 'e';
		case L'ú':
			*d = DIACRITIC_ACUTE;
			return 'u';
		case L'ů':
			*d = DIACRITIC_DOT;
			return 'u';
		case L'ě':
			*d = DIACRITIC_CARON;
			return 'e';
		case L'ř':
			*d = DIACRITIC_CARON;
			return 'r';
		case L'š':
			*d = DIACRITIC_CARON;
			return 's';
		case L'č':
			*d = DIACRITIC_CARON;
			return 'c';
		case L'ž':
			*d = DIACRITIC_CARON;
			return 'z';
		case L'ň':
			*d = DIACRITIC_CARON;
			return 'n';
		case L'ď':
			*d = DIACRITIC_CARON;
			return 'd';
		case L'ť':
			*d = DIACRITIC_CARON;
			return 't';
		case L'ý':
			*d = DIACRITIC_ACUTE;
			return 'y';
        default:
            *d = DIACRITIC_NONE; 
           return (char)wc;
    }
}

void drawDiacritic(int16_t x, int16_t y, unsigned char c, DIACRITIC diacritic, struct Color color,
				  struct Color bg, uint8_t size_x, uint8_t size_y)
{
	// Support sizes that are even multiples of 2 (2*n). For other sizes, skip.
	if ((size_x % 2) != 0 || (size_y % 2) != 0)
	{
		return;
	}

	// Scale relative to the original size==2 pattern
	uint8_t kx = size_x / 2;
	uint8_t ky = size_y / 2;

	int o = (isupper(c) ? 0 : 3) * ky;

	if (diacritic == DIACRITIC_DOT)
	{
		// Original pixels replicated as rectangles scaled by kx x ky
		GFX_fillRect(x + 4 * kx, y + (-1) * ky + o, kx, ky, color);
		GFX_fillRect(x + 5 * kx, y + (-1) * ky + o, kx, ky, color);
		GFX_fillRect(x + 4 * kx, y + (-4) * ky + o, kx, ky, color);
		GFX_fillRect(x + 5 * kx, y + (-4) * ky + o, kx, ky, color);
		GFX_fillRect(x + 3 * kx, y + (-2) * ky + o, kx, ky, color);
		GFX_fillRect(x + 3 * kx, y + (-3) * ky + o, kx, ky, color);
		GFX_fillRect(x + 6 * kx, y + (-2) * ky + o, kx, ky, color);
		GFX_fillRect(x + 6 * kx, y + (-3) * ky + o, kx, ky, color);
	}
	else if (diacritic == DIACRITIC_CARON)
	{
		if ('t' == c)
		{
			GFX_fillRect(x + 7 * kx, y + (-1) * ky + o, kx, ky, color);
			GFX_fillRect(x + 7 * kx, y + (-2) * ky + o, kx, ky, color);
			GFX_fillRect(x + 8 * kx, y + (-2) * ky + o, kx, ky, color);
			GFX_fillRect(x + 8 * kx, y + (-3) * ky + o, kx, ky, color);
		}
		else if ('d' == c)
		{
			// Intentionally not implemented: would exceed char bounds and clash with next glyph
		}
		else
		{
			GFX_fillRect(x + 4 * kx, y + (-2) * ky + o, kx, ky, color);
			GFX_fillRect(x + 5 * kx, y + (-2) * ky + o, kx, ky, color);
			GFX_fillRect(x + 3 * kx, y + (-3) * ky + o, kx, ky, color);
			GFX_fillRect(x + 4 * kx, y + (-3) * ky + o, kx, ky, color);
			GFX_fillRect(x + 5 * kx, y + (-3) * ky + o, kx, ky, color);
			GFX_fillRect(x + 6 * kx, y + (-3) * ky + o, kx, ky, color);
			GFX_fillRect(x + 3 * kx, y + (-4) * ky + o, kx, ky, color);
			GFX_fillRect(x + 2 * kx, y + (-4) * ky + o, kx, ky, color);
			GFX_fillRect(x + 7 * kx, y + (-4) * ky + o, kx, ky, color);
			GFX_fillRect(x + 6 * kx, y + (-4) * ky + o, kx, ky, color);
		}
	}
	else if (diacritic == DIACRITIC_ACUTE)
	{
		// Background clear to refine shape, scaled
		GFX_fillRect(x + 4 * kx, y + (-3) * ky + o, kx, ky, bg);

		GFX_fillRect(x + 4 * kx, y + (-2) * ky + o, kx, ky, color);
		GFX_fillRect(x + 5 * kx, y + (-2) * ky + o, kx, ky, color);
		GFX_fillRect(x + 5 * kx, y + (-3) * ky + o, kx, ky, color);
		GFX_fillRect(x + 6 * kx, y + (-3) * ky + o, kx, ky, color);
		GFX_fillRect(x + 7 * kx, y + (-4) * ky + o, kx, ky, color);
		GFX_fillRect(x + 6 * kx, y + (-4) * ky + o, kx, ky, color);
	}
}

void GFX_drawChar(int16_t x, int16_t y, unsigned char c, struct Color color,
				  struct Color bg, uint8_t size_x, uint8_t size_y)
{
	if (!gfxFont)
	{
		if ((x >= _width) ||			  // Clip right
			(y >= _height) ||			  // Clip bottom
			((x + 6 * size_x - 1) < 0) || // Clip left
			((y + 8 * size_y - 1) < 0))	  // Clip top
			return;

		DIACRITIC diacritic = DIACRITIC_NONE;
		if( c >= 128)
		{
			wchar_t wc = wideCharacters[(uint8_t)c - 128];
			c = solveDiacritic(wc, &diacritic);
		}
		
		if (c >= 176)
			c++; // Handle 'classic' charset behavior

		// GFX_Select();
		for (int8_t i = 0; i < 5; i++)
		{ // Char bitmap = 5 columns
			uint8_t line = font[c * 5 + i];
			for (int8_t j = 0; j < 8; j++, line >>= 1)
			{
				if (line & 1)
				{
					if (size_x == 1 && size_y == 1)
						GFX_drawPixel(x + i, y + j, color);
					else
						GFX_fillRect(x + i * size_x, y + j * size_y, size_x,
									 size_y, color);
				}
				else if (isNotEqual(bg,color))
				{
					if (size_x == 1 && size_y == 1)
						GFX_drawPixel(x + i, y + j, bg);
					else
						GFX_fillRect(x + i * size_x, y + j * size_y, size_x,
									 size_y, bg);
				}
			}
		}
		drawDiacritic(x, y, c, diacritic, color, bg, size_x, size_y);

		if (isNotEqual(bg,color))
		{ // If opaque, draw vertical line for last column
			if (size_x == 1 && size_y == 1)
				GFX_drawFastVLine(x + 5, y, 8, bg);
			else
				GFX_fillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
		}
		// GFX_DeSelect();
	}
	else
	{
		c -= (uint8_t)gfxFont->first;
		GFXglyph *glyph = (gfxFont->glyph) + c;
		uint8_t *bitmap = gfxFont->bitmap;

		uint16_t bo = glyph->bitmapOffset;
		uint8_t w = glyph->width, h = glyph->height;
		int8_t xo = glyph->xOffset, yo = glyph->yOffset;
		uint8_t xx, yy, bits = 0, bit = 0;
		int16_t xo16 = 0, yo16 = 0;

		if (size_x > 1 || size_y > 1)
		{
			xo16 = xo;
			yo16 = yo;
		}

		// GFX_Select();
		for (yy = 0; yy < h; yy++)
		{
			for (xx = 0; xx < w; xx++)
			{
				if (!(bit++ & 7))
				{
					bits = bitmap[bo++];
				}
				if (bits & 0x80)
				{
					if (size_x == 1 && size_y == 1)
					{
						GFX_drawPixel(x + xo + xx, y + yo + yy, color);
					}
					else
					{
						GFX_fillRect(x + (xo16 + xx) * size_x,
									 y + (yo16 + yy) * size_y, size_x, size_y,
									 color);
					}
				}
				bits <<= 1;
			}
		}
		// GFX_DeSelect();
	}
}

void GFX_write(uint8_t c, uint8_t textsize)
{
	uint8_t textsize_y = textsize;
	uint8_t textsize_x = textsize;
	if (!gfxFont)
	{
		if (c == '\n')
		{								// Newline?
			cursor_x = 0;				// Reset x to zero,
			cursor_y += textsize_y * 8; // advance y one line
		}
		else if (c != '\r')
		{ // Ignore carriage returns
			if (wrap && ((cursor_x + textsize_x * 6) > _width))
			{								// Off right?
				cursor_x = 0;				// Reset x to zero,
				cursor_y += textsize_y * 8; // advance y one line
			}
			GFX_drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor,
						 textsize_x, textsize_y);
			cursor_x += textsize_x * 6; // Advance x one char
		}
	}
	else
	{
		if (c == '\n')
		{
			cursor_x = 0;
			cursor_y += (int16_t)textsize_y * (uint8_t)(gfxFont->yAdvance);
		}
		else if (c != '\r')
		{
			uint8_t first = gfxFont->first;
			if ((c >= first) && (c <= (uint8_t)gfxFont->last))
			{
				GFXglyph *glyph = (gfxFont->glyph) + (c - first);
				uint8_t w = glyph->width, h = glyph->height;
				if ((w > 0) && (h > 0))
				{										 // Is there an associated bitmap?
					int16_t xo = (int8_t)glyph->xOffset; // sic
					if (wrap && ((cursor_x + textsize_x * (xo + w)) > _width))
					{
						cursor_x = 0;
						cursor_y += (int16_t)textsize_y * (uint8_t)gfxFont->yAdvance;
					}
					GFX_drawChar(cursor_x, cursor_y, c, textcolor,
								 textbgcolor, textsize_x, textsize_y);
				}
				cursor_x += (uint8_t)glyph->xAdvance * (int16_t)textsize_x;
			}
		}
	}
}

void GFX_setCursor(int16_t x, int16_t y)
{
	cursor_x = x;
	cursor_y = y;
}

void GFX_setTextColor(struct Color color)
{
	textcolor = color;
}

void GFX_setTextBack(struct Color color)
{
	textbgcolor = color;
}

void GFX_setFont(const GFXfont *f)
{
	if (f)
	{ // Font struct pointer passed in?
		if (!gfxFont)
		{ // And no current font struct?
			// Switching from classic to new font behavior.
			// Move cursor pos down 6 pixels so it's on baseline.
			cursor_y += 6;
		}
	}
	else if (gfxFont)
	{ // NULL passed.  Current font struct defined?
		// Switching from new to classic font behavior.
		// Move cursor pos up 6 pixels so it's at top-left of char.
		cursor_y -= 6;
	}
	gfxFont = (GFXfont *)f;
}

void GFX_fillCircle(int16_t x0, int16_t y0, int16_t r,
					struct Color color)
{

	GFX_drawFastVLine(x0, y0 - r, 2 * r + 1, color);
	fillCircleHelper(x0, y0, r, 3, 0, color);
}

void GFX_drawCircle(int16_t x0, int16_t y0, int16_t r, struct Color color)
{

	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	GFX_drawPixel(x0, y0 + r, color);
	GFX_drawPixel(x0, y0 - r, color);
	GFX_drawPixel(x0 + r, y0, color);
	GFX_drawPixel(x0 - r, y0, color);

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
		f += ddF_x;

		GFX_drawPixel(x0 + x, y0 + y, color);
		GFX_drawPixel(x0 - x, y0 + y, color);
		GFX_drawPixel(x0 + x, y0 - y, color);
		GFX_drawPixel(x0 - x, y0 - y, color);
		GFX_drawPixel(x0 + y, y0 + x, color);
		GFX_drawPixel(x0 - y, y0 + x, color);
		GFX_drawPixel(x0 + y, y0 - x, color);
		GFX_drawPixel(x0 - y, y0 - x, color);
	}
}

char printBuf[100];
void printString(char s[], uint8_t textsize)
{
	uint8_t n = strlen(s);
	for (int i = 0; i < n; i++)
		GFX_write(s[i], textsize);
}

void GFX_printf(uint8_t textsize, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsprintf(printBuf, format, args);
	printString(printBuf, textsize);
	va_end(args);
}

void GFX_createFramebuf(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	if(w * h * 3 > BUFFER_MAX_SIZE) //e.g. 200p x 200p square
	{
		return;
	}
	frameBufferX = x;
	frameBufferY = y;
	frameBufferWidth = w;
	frameBufferHeight = h;
	gfxFramebuffer = malloc(w * h * sizeof(uint8_t) * 3);
}
void GFX_destroyFramebuf()
{
	frameBufferX = 0;
	frameBufferY = 0;
	frameBufferWidth = 0;
	frameBufferHeight = 0;
	free(gfxFramebuffer);
	gfxFramebuffer = NULL;
}
void GFX_flush()
{
	if (gfxFramebuffer != NULL)
	{
		LCD_WriteBitmap(frameBufferX, frameBufferY, frameBufferWidth, frameBufferHeight, gfxFramebuffer);
	}
}

void initGfxDmaChan()
{
	if (!gfx_dma_init)
	{
		memcpy_dma_chan = dma_claim_unused_channel(true);
		gfx_dma_init = true;
	}
}

void dma_memset(void *dest, uint8_t val, size_t num)
{
	initGfxDmaChan();

    dma_channel_config c = dma_channel_get_default_config(memcpy_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);

    dma_channel_configure(
        memcpy_dma_chan, // Channel to be configured
        &c,              // The configuration we just created
        dest,            // The initial write address
        &val,            // The initial read address
        num,             // Number of transfers; in this case each is 1 byte.
        true             // Start immediately.
    );

    // We could choose to go and do something else whilst the DMA is doing its
    // thing. In this case the processor has nothing else to do, so we just
    // wait for the DMA to finish.
    dma_channel_wait_for_finish_blocking(memcpy_dma_chan);
}

void dma_memcpy(void *dest, void *src, size_t num)
{
	initGfxDmaChan();

    dma_channel_config c = dma_channel_get_default_config(memcpy_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);

    dma_channel_configure(
        memcpy_dma_chan, // Channel to be configured
        &c,              // The configuration we just created
        dest,            // The initial write address
        src,             // The initial read address
        num,             // Number of transfers; in this case each is 1 byte.
        true             // Start immediately.
    );

    // We could choose to go and do something else whilst the DMA is doing its
    // thing. In this case the processor has nothing else to do, so we just
    // wait for the DMA to finish.
    dma_channel_wait_for_finish_blocking(memcpy_dma_chan);
}
