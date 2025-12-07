#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "ili9488.h"

int16_t _xstart = 0; ///< Internal framebuffer X offset
int16_t _ystart = 0; ///< Internal framebuffer Y offset

uint8_t rotation;

spi_inst_t *ili9488_spi = spi_default;

uint16_t ili9488_pinCS = PICO_DEFAULT_SPI_CSN_PIN;
uint16_t ili9488_pinDC = 20;
int16_t ili9488_pinRST = 16;

uint16_t ili9488_pinSCK = PICO_DEFAULT_SPI_SCK_PIN;
uint16_t ili9488_pinTX = PICO_DEFAULT_SPI_TX_PIN;

uint16_t _width;  ///< Display width as modified by current rotation
uint16_t _height; ///< Display height as modified by current rotation

uint8_t initcmd[] = {
	15, //24 commands
	0xE0, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
	0XE1, 15 ,0x00 ,0x16 ,0x19 ,0x03 ,0x0F ,0x05 ,0x32 ,0x45 ,0x46 ,0x04 ,0x0E ,0x0D ,0x35 ,0x37 ,0x0F,
	0XC0, 2 , 0x17, 0x15,
	0xC1, 1, 0x41,
	0xC5, 3, 0x00, 0x12, 0x80,
	ILI9488_MADCTL, 1, 0x48,
	0x3A, 1, 0x77,
	0xB0, 1, 0x80,
	0xB1, 1, 0xA0,
	0xB4, 1, 0x02,
	0xB6, 3, 0x02, 0x02, 0x3B,
	0xB7, 1, 0xC6,
	0xF7, 4, 0xA9, 0x51, 0x2C, 0x82,
	ILI9488_SLPOUT, 0x80,
	ILI9488_DISPON, 0x80,
};

#ifdef USE_DMA
uint dma_tx;
dma_channel_config dma_cfg;
void waitForDMA()
{

	dma_channel_wait_for_finish_blocking(dma_tx);
}
#endif

void LCD_setPins(uint16_t dc, uint16_t cs, int16_t rst, uint16_t sck, uint16_t tx)
{
	ili9488_pinDC = dc;
	ili9488_pinCS = cs;
	ili9488_pinRST = rst;
	ili9488_pinSCK = sck;
	ili9488_pinTX = tx;
}

void LCD_setSPIperiph(spi_inst_t *s)
{
	ili9488_spi = s;
}

void initSPI()
{
	spi_init(ili9488_spi, 1000 * 40000);
	spi_set_format(ili9488_spi, 16, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	gpio_set_function(ili9488_pinSCK, GPIO_FUNC_SPI);
	gpio_set_function(ili9488_pinTX, GPIO_FUNC_SPI);

	gpio_init(ili9488_pinCS);
	gpio_set_dir(ili9488_pinCS, GPIO_OUT);
	gpio_put(ili9488_pinCS, 1);

	gpio_init(ili9488_pinDC);
	gpio_set_dir(ili9488_pinDC, GPIO_OUT);
	gpio_put(ili9488_pinDC, 1);

	if (ili9488_pinRST != -1)
	{
		gpio_init(ili9488_pinRST);
		gpio_set_dir(ili9488_pinRST, GPIO_OUT);
		gpio_put(ili9488_pinRST, 1);
	}

#ifdef USE_DMA
	dma_tx = dma_claim_unused_channel(true);
	dma_cfg = dma_channel_get_default_config(dma_tx);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&dma_cfg, spi_get_dreq(ili9488_spi, true));
#endif
}

void ILI9488_Reset()
{
	if (ili9488_pinRST != -1)
	{
		gpio_put(ili9488_pinRST, 0);
		sleep_ms(5);
		gpio_put(ili9488_pinRST, 1);
		sleep_ms(150);
	}
}

void ILI9488_Select()
{
	gpio_put(ili9488_pinCS, 0); 
}

void ILI9488_DeSelect()
{
	gpio_put(ili9488_pinCS, 1);
}

void ILI9488_RegCommand()
{
	gpio_put(ili9488_pinDC, 0);
}

void ILI9488_RegData()
{
	gpio_put(ili9488_pinDC, 1);
}

void ILI9488_WriteCommand(uint8_t cmd)
{
	ILI9488_RegCommand();
	spi_set_format(ili9488_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(ili9488_spi, &cmd, sizeof(cmd));
}

void ILI9488_WriteData(uint8_t *buff, size_t buff_size)
{
	ILI9488_RegData();
	spi_set_format(ili9488_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(ili9488_spi, buff, buff_size);
}

void write8data(uint8_t cmd)
{
	ILI9488_RegData();
	spi_set_format(ili9488_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(ili9488_spi, &cmd, sizeof(cmd));
}

void ILI9488_SendCommand(uint8_t commandByte, uint8_t *dataBytes,
						 uint8_t numDataBytes)
{
	ILI9488_Select();

	ILI9488_WriteCommand(commandByte);
	ILI9488_WriteData(dataBytes, numDataBytes);

	ILI9488_DeSelect();
}


void innt()
{
	ILI9488_WriteCommand(0xE0); // Positive Gamma Control
    write8data(0x00);
    write8data(0x03);
    write8data(0x09);
    write8data(0x08);
    write8data(0x16);
    write8data(0x0A);
    write8data(0x3F);
    write8data(0x78);
    write8data(0x4C);
    write8data(0x09);
    write8data(0x0A);
    write8data(0x08);
    write8data(0x16);
    write8data(0x1A);
    write8data(0x0F);

    ILI9488_WriteCommand(0XE1); // Negative Gamma Control
    write8data(0x00);
    write8data(0x16);
    write8data(0x19);
    write8data(0x03);
    write8data(0x0F);
    write8data(0x05);
    write8data(0x32);
    write8data(0x45);
    write8data(0x46);
    write8data(0x04);
    write8data(0x0E);
    write8data(0x0D);
    write8data(0x35);
    write8data(0x37);
    write8data(0x0F);

    ILI9488_WriteCommand(0XC0); // Power Control 1
    write8data(0x17);
    write8data(0x15);

    ILI9488_WriteCommand(0xC1); // Power Control 2
    write8data(0x41);

    ILI9488_WriteCommand(0xC5); // VCOM Control
    write8data(0x00);
    write8data(0x12);
    write8data(0x80);

    ILI9488_WriteCommand(0x36); // Memory Access Control
    write8data(0x48);          // MX, BGR

    ILI9488_WriteCommand(0x3A); // Pixel Interface Format
    write8data(0x66);  // 18-bit colour for SPI

    ILI9488_WriteCommand(0xB0); // Interface Mode Control
    write8data(0x00);

    ILI9488_WriteCommand(0xB1); // Frame Rate Control
    write8data(0xA0);

    ILI9488_WriteCommand(0xB4); // Display Inversion Control
    write8data(0x02);

    ILI9488_WriteCommand(0xB6); // Display Function Control
    write8data(0x02);
    write8data(0x02);
    write8data(0x3B);

    ILI9488_WriteCommand(0xB7); // Entry Mode Set
    write8data(0xC6);

    ILI9488_WriteCommand(0xF7); // Adjust Control 3
    write8data(0xA9);
    write8data(0x51);
    write8data(0x2C);
    write8data(0x82);

    ILI9488_WriteCommand(0x11);  //Exit Sleep
	sleep_ms(120);

    ILI9488_WriteCommand(0x29);  //Display on
	sleep_ms(25);
}

void LCD_initDisplay()
{
	initSPI();
	ILI9488_Select();

	if (ili9488_pinRST < 0)
	{										   // If no hardware reset pin...
		ILI9488_WriteCommand(ILI9488_SWRESET); // Engage software reset
		sleep_ms(150);
	}
	else
		ILI9488_Reset();
	innt();	
	
	_width = ILI9488_TFTHEIGHT;
	_height = ILI9488_TFTWIDTH;
}

void LCD_setRotation(uint8_t m)
{
	rotation = m % 4; // can't be higher than 3
	switch (rotation)
	{
	case 0:
		m = (MADCTL_MX | MADCTL_BGR);
		_width = ILI9488_TFTWIDTH;
		_height = ILI9488_TFTHEIGHT;
		break;
	case 1:
		m = (MADCTL_MV | MADCTL_BGR);
		_width = ILI9488_TFTHEIGHT;
		_height = ILI9488_TFTWIDTH;
		break;
	case 2:
		m = (MADCTL_MY | MADCTL_BGR);
		_width = ILI9488_TFTWIDTH;
		_height = ILI9488_TFTHEIGHT;
		break;
	case 3:
		m = (MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
		_width = ILI9488_TFTHEIGHT;
		_height = ILI9488_TFTWIDTH;
		break;
	}

	ILI9488_SendCommand(ILI9488_MADCTL, &m, 1);
}

void LCD_setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
	uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);

	xa = __builtin_bswap32(xa);
	ya = __builtin_bswap32(ya);

	ILI9488_WriteCommand(ILI9488_CASET);
	ILI9488_WriteData((uint8_t*)&xa, sizeof(xa));

	// row address set
	ILI9488_WriteCommand(ILI9488_PASET);
	ILI9488_WriteData((uint8_t*)&ya, sizeof(ya));

	// write to RAM
	ILI9488_WriteCommand(ILI9488_RAMWR);
}

void LCD_WriteBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
{
	ILI9488_Select();
	LCD_setAddrWindow(x, y, w, h); // Clipped area
	ILI9488_RegData();
	spi_set_format(ili9488_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
#ifdef USE_DMA
	dma_channel_configure(dma_tx, &dma_cfg,
						  &spi_get_hw(ili9488_spi)->dr, // write address
						  bitmap,						// read address
						  w * h *3,						// element count (each element is of size transfer_data_size)
						  true);						// start asap
	waitForDMA();
#else

	spi_write_blocking(ili9488_spi, bitmap, w * h * 3);
#endif

	ILI9488_DeSelect();
}

void LCD_WritePixel(int x, int y, struct Color col)
{
	ILI9488_Select();
	LCD_setAddrWindow(x, y, 1, 1); // Clipped area
	ILI9488_RegData();
	spi_set_format(ili9488_spi, 8, SPI_CPOL_1, SPI_CPOL_1, SPI_MSB_FIRST);
	spi_write_blocking(ili9488_spi, &(col.r), 1);
	spi_write_blocking(ili9488_spi, &(col.g), 1);
	spi_write_blocking(ili9488_spi, &(col.b), 1);
	ILI9488_DeSelect();
}
