// nIngen ili9488 480 x 320 LCD x128_ili9488_driver
// MCU : ATxmega128a3u @ 32 Mhz

#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "x128_ili9488_driver.h"
#include "ll_x128a3u.h"
#include "fonts.h"

// Define structures for different configuration types
struct touch_cal_data {
    uint16_t magic;      // Magic number (0xCAFE)
    uint16_t cal_x[CAL_POINTS * CAL_POINTS];
    uint16_t cal_y[CAL_POINTS * CAL_POINTS];
	uint16_t touch_samples;
	uint16_t touch_accuracy;
    uint16_t checksum;
};

struct lcd_settings {
    uint16_t magic;      // Different magic number (0xBEEF)
    uint8_t brightness;
    uint8_t contrast;
    uint8_t volume;
    uint8_t other_setting;
    uint16_t checksum;
};

// Combined structure that fits in one page
const struct {
    struct touch_cal_data touch_cal;  // 72 bytes
    struct other_config other_cfg;    // 8 bytes
    // Add more configurations here if needed
    // Remember: total size must be <= 256
} __attribute__((section(".calibration_data"))) stored_configs;

const uint8_t INIT_ILI9341[] PROGMEM = {
    // 11 commands in list:
    11,
    ILI9488_CMD_SOFTWARE_RESET, DELAY, 200,
    ILI9488_CMD_SLEEP_OUT, DELAY, 200,
    ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET, 1+DELAY, 0x05, 10,  // 10 ms
    ILI9488_CMD_FRAME_RATE_CONTROL_NORMAL, 3+DELAY,
         0x00,  // fastest refresh
         0x06,  // 6 lines front porch
         0x03,  // 3 lines back porch
           10,  // 10 ms delay
	ILI9488_CMD_DISP_INVERSION_OFF, DELAY, 10,
	ILI9488_CMD_MEMORY_ACCESS_CONTROL, 1, 0xE0,
    ILI9488_CMD_DISPLAY_FUNCTION_CONTROL, 2,
		 0x15,  // 1 clk cycle nonoverlap, 2 cycle gate
                // rise, 3 cycle osc equalize
         0x02,  // Fix on VTL
	ILI9488_CMD_DISPLAY_INVERSION_CONTROL, 1, 0x00,
    ILI9488_CMD_POSITIVE_GAMMA_CORRECTION, 15, 0x06, 0x08, 0x09, 0x08, 0x10, 0x0b, 0x22, 0x88, 0x35, 0x08, 0x10, 0x08, 0x14, 0x10, 0x05,
	//0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19, 0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, // PG 265
    ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION, 15+DELAY, 0x05, 0x10, 0x14, 0x08, 0x10, 0x08, 0x35, 0x88, 0x24, 0x0a, 0x04, 0x08, 0x06, 0x06, 0x02, 10,
	//0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E, 0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 10,
	ILI9488_CMD_NORMAL_DISP_MODE_ON, DELAY, 10,
};



/** @var array Chache memory char index row */
uint16_t lcd_index_y = 0;
/** @var array Chache memory char index column */
uint16_t lcd_index_x = 0;

// Optimized SPI send function for AVR
static inline uint8_t lcd_spi_transfer(uint8_t data) {
    SPIE.DATA = data;
    while(!(SPIE.STATUS & SPI_IF_bm));
    return SPIE.DATA;
}

static inline void lcd_send_cmd(uint8_t data)
{
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
	PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select

    SPIE.DATA = data;

    while (!(SPIE.STATUS & SPI_IF_bm))
        ;

	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
	PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
}

static inline void lcd_send_data(uint8_t * data, uint16_t data_bytes_count)
{
	uint8_t a;
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
	for(a = 0; a < data_bytes_count; a++) {

		SPIE.DATA = data[a];

		while (!(SPIE.STATUS & SPI_IF_bm))
			;
	}
	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
}

static inline void lcd_get_data(uint8_t * data, uint16_t data_bytes_count)
{
	uint8_t a;
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
	for(a = 0; a < data_bytes_count; a++) {

		SPIE.DATA = data[a];

		while (!(SPIE.STATUS & SPI_IF_bm))
			;

		data[a] = SPIE.DATA;
	}
	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
}

void lcd_read_register(uint8_t cmd, uint8_t * readback, uint8_t length)
{
	uint8_t a = 0;
	uint8_t reg1, reg2;

	reg1 = 0x81;
	reg2 = 0x00;

	lcd_spi_touch_set();

	for(a = 0; a < length; a++) {
		lcd_send_cmd(ILI9488_CMD_SPI_READ_SETTINGS);
		lcd_send_data(&reg1, 1);
		reg1++;
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select
		lcd_spi_transfer(ILI9488_CMD_READ_ID4);
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		readback[a] = lcd_spi_transfer(0x00);
		PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
		PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
		PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
		lcd_send_cmd(ILI9488_CMD_SPI_READ_SETTINGS);
		lcd_send_data(&reg2, 1);
		_delay_ms(10);
	}

	lcd_spi_set();
}

void lcd_reset(void)
{
	PORT_LCD_RST |= (1 << PIN_LCD_RST);
	DDR_LCD_RST  |= (1 << PIN_LCD_RST);
	_delay_ms(200);
	PORT_LCD_RST &= ~(1 << PIN_LCD_RST);
	_delay_ms(200);
	PORT_LCD_RST |=  (1 << PIN_LCD_RST);
}

void lcd_spi_set(void)
{
	SPI_set(16);
}

void lcd_spi_touch_set(void)
{
	SPI_set(4);
}

void lcd_init_controller(void)
{
	lcd_spi_set();
	lcd_reset();
	lcd_send_init_commands(INIT_ILI9341); // Send command list
	lcd_clear_screen(BLACK);
	lcd_on();
}

void lcd_send_init_commands(const uint8_t *commands)
{
	uint8_t milliseconds;
	uint8_t numOfCommands;
	uint8_t numOfArguments;
	uint8_t data;

	numOfCommands = pgm_read_byte(commands++);

	while (numOfCommands--) {
		lcd_send_cmd(pgm_read_byte(commands++));
		numOfArguments = pgm_read_byte(commands++);
		milliseconds = numOfArguments & DELAY; // Store the delay flag in milliseconds
		numOfArguments &= ~DELAY; // Remove the delay flag from the number of arguments
		while (numOfArguments--) {
			data = pgm_read_byte(commands++);
			lcd_send_data(&data, 1); // send arguments
		}
		if (milliseconds) {
			milliseconds = pgm_read_byte(commands++); // Read the last arg, value in milliseconds
			while (milliseconds--) {
				_delay_ms(1);
			}
		}
	}
}

static inline void lcd_send_color_565(uint16_t color, uint32_t count)
{
	uint8_t a[2];
	a[0] = (uint8_t)(color >> 8);
	a[1] = (uint8_t)(color);
	lcd_send_cmd(RAMWR);
	while (count--) {
		lcd_send_data(a, 2); // MODIFICADO ACA
	}
}

uint8_t lcd_set_partial_area(uint16_t sRow, uint16_t eRow)
{
	uint8_t a[2];

	if ((sRow > MAX_X) || (eRow > MAX_Y))
		return 0;

	a[0] = (uint8_t) (sRow >> 8);
	a[1] = (uint8_t) (sRow);

	lcd_send_cmd(PTLAR); // column address set

	lcd_send_data(a, 2); // start start Row MODIFICADO ACA

	a[0] = (uint8_t) (eRow >> 8);
	a[1] = (uint8_t) (eRow);

	lcd_send_data(a, 2); // end end Row // MODIFICADO ACA

	lcd_send_cmd(PTLON); // partial area on

	return 1;
}

uint8_t lcd_set_window(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1)
{
	uint8_t a[4];

	if ((x0 > x1) || (x1 > SIZE_X) || (y0 > y1) || (y1 > SIZE_Y)) {
		return 0;
	}

	a[0] = (uint8_t) (x0 >> 8);
	a[1] = (uint8_t) (x0);
	a[2] = (uint8_t) (x1 >> 8);
	a[3] = (uint8_t) (x1);

	lcd_send_cmd(CASET); // column address set
	lcd_send_data(&a[0], 1);
	lcd_send_data(&a[1], 1);
	lcd_send_data(&a[2], 1);
	lcd_send_data(&a[3], 1);

	a[0] = (uint8_t) (y0 >> 8);
	a[1] = (uint8_t) (y0);
	a[2] = (uint8_t) (y1 >> 8);
	a[3] = (uint8_t) (y1);

	lcd_send_cmd(RASET); // row address set
	lcd_send_data(&a[0], 1); // end y position // MODIFICADO ACA
	lcd_send_data(&a[1], 1);
	lcd_send_data(&a[2], 1);
	lcd_send_data(&a[3], 1);

	return 1;
}

uint8_t lcd_set_position(uint16_t x, uint16_t y)
{
	if ((x > SIZE_X - (CHARS_COLS_LEN + 1)) && (y > SIZE_Y - (CHARS_ROWS_LEN))) {
		return 0; // OUT OF RANGE
	}

	// check if x coordinates is out of range
	// and y is not out of range go to next line
	if ((x > SIZE_X - (CHARS_COLS_LEN + 1)) && (y < SIZE_Y - (CHARS_ROWS_LEN))) {
		lcd_index_y = y + CHARS_ROWS_LEN; // change position y
		lcd_index_x = x; // change position x
		}
	else {
		lcd_index_y = y; // set position y
		lcd_index_x = x; // set position x
	}

	return 1;
}

void lcd_draw_window(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, uint16_t color)
{
    uint16_t temp = 0;
    uint32_t d = 0;

	if(x0 > x1)
	{
		temp = x0;
		x0 = x1;
		x1 = temp;
	}
	if(y0 > y1)
	{
		temp = y0;
		y0 = y1;
		y1 = temp;
	}

	for(temp = 0; temp < (x1 - x0) + 1; temp++) d += (y1 - y0) + 1;

	lcd_set_window(x0, x1, y0, y1);
	lcd_send_color_565(color, d);
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
	lcd_set_window(x, x, y, y);
	lcd_send_color_565(color, 1);
}

void lcd_draw_char(uint8_t character, uint16_t color, uint8_t size)
{
    uint8_t data[90];
    uint8_t x, a, b;

    if(!get_char(character, data, size)) return;

	for(a = 0; a < data[1]; a++)
    {
        for(x = 0; x < data[0]; x++)
        {
            for(b = 0; b < 8; b++)
            {
                if((data[3 + (a * data[0]) + x] >> b) & 0x01)
                {
                    lcd_draw_pixel(lcd_index_x + x, lcd_index_y + ((a * 8) + b), color);
                }
            }
        }
    }

    lcd_set_position(lcd_index_x + data[0] + data[2], lcd_index_y);
}

void lcd_get_char_size(uint8_t chr, uint8_t type, uint8_t *size_x, uint8_t *size_y)
{
	get_char_size(chr, type, size_x, size_y);
}


void lcd_print(uint8_t *str, uint16_t color, uint8_t size)
{
	uint8_t i = 0;
	while (str[i] != '\0') {
		lcd_draw_char(str[i++], color, size);
	}
}

uint8_t lcd_draw_line(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t color)
{
	// determinant
	int16_t D;
	// deltas
	int16_t delta_x, delta_y;
	// steps
	int16_t trace_x = 1, trace_y = 1;

	// delta x
	delta_x = x2 - x1;
	// delta y
	delta_y = y2 - y1;

	// check if x2 > x1
	if (delta_x < 0) {
		// negate delta x
		delta_x = -delta_x;
		// negate step x
		trace_x = -trace_x;
	}

	// check if y2 > y1
	if (delta_y < 0) {
		// negate detla y
		delta_y = -delta_y;
		// negate step y
		trace_y = -trace_y;
	}

	// Bresenham condition for m < 1 (dy < dx)
	if (delta_y < delta_x) {
		// calculate determinant
		D = (delta_y << 1) - delta_x;
		// draw first pixel
		lcd_draw_pixel(x1, y1, color);
		// check if x1 equal x2
		while (x1 != x2) {
			// update x1
			x1 += trace_x;
			// check if determinant is positive
			if (D >= 0) {
				// update y1
				y1 += trace_y;
				// update determinant
				D -= (delta_x << 1);
			}
			// update deteminant
			D += (delta_y << 1);
			// draw next pixel
			lcd_draw_pixel(x1, y1, color);
		}
		// for m > 1 (dy > dx)
		} else {
		// calculate determinant
		D = delta_y - (delta_x << 1);
		// draw first pixel
		lcd_draw_pixel(x1, y1, color);
		// check if y2 equal y1
		while (y1 != y2) {
			// update y1
			y1 += trace_y;
			// check if determinant is positive
			if (D <= 0) {
				// update y1
				x1 += trace_x;
				// update determinant
				D += (delta_y << 1);
			}
			// update deteminant
			D -= (delta_x << 1);
			// draw next pixel
			lcd_draw_pixel(x1, y1, color);
		}
	}
	return 1;
}

void lcd_draw_line_x(uint16_t xs, uint16_t xe, uint16_t y, uint16_t color)
{
	uint8_t temp;
	// check if start is > as end
	if (xs > xe) {
		// temporary safe
		temp = xe;
		// start change for end
		xe = xs;
		// end change for start
		xs = temp;
	}
	lcd_set_window(xs, xe, y, y);
	lcd_send_color_565(color, xe - xs);
}

void lcd_draw_line_y(uint16_t x, uint16_t ys, uint16_t ye, uint16_t color)
{
	uint8_t temp;
	// check if start is > as end
	if (ys > ye) {
		// temporary safe
		temp = ye;
		// start change for end
		ye = ys;
		// end change for start
		ys = temp;
	}
	lcd_set_window(x, x, ys, ye);
	lcd_send_color_565(color, ye - ys);
}

void lcd_clear_screen(uint16_t color)
{
	lcd_set_window(0, SIZE_X, 0, SIZE_Y);
	lcd_send_color_565(color, CACHE_SIZE_MEM);
}

void lcd_on(void)
{
	lcd_send_cmd(DISPON);
	_delay_us(50);
}

void lcd_off(void)
{
	lcd_send_cmd(ILI9488_CMD_PIXEL_OFF);
}

void lcd_number(uint32_t number, uint8_t base, uint16_t color, uint8_t size, uint8_t * prefix, uint8_t * sufix)
{
	uint8_t a[16];
	uint8_t b;

	if(prefix != NULL) lcd_print(prefix, color, size);

	if(base == 16 && number < 256)
	{
		a[0] = (number >> 4) & 0x0f;
		if(a[0] > 9) a[0] += (0x41 - 10);
		if(a[0] < 10) a[0] += 0x30;
		a[1] = number & 0x0f;
		if(a[1] > 9) a[1] += (0x41 - 10);
		if(a[1] < 10) a[1] += 0x30;

		lcd_draw_char(a[0], color, size);
		lcd_draw_char(a[1], color, size);
	}

	if(base == 16 && number > 255 && number < 65536)
	{
		a[0] = (number >> 12) & 0x0f;
		if(a[0] > 9) a[0] += (0x41 - 10);
		if(a[0] < 10) a[0] += 0x30;
		a[1] = (number >> 8) & 0x0f;
		if(a[1] > 9) a[1] += (0x41 - 10);
		if(a[1] < 10) a[1] += 0x30;
		a[2] = (number >> 4) & 0x0f;
		if(a[2] > 9) a[2] += (0x41 - 10);
		if(a[2] < 10) a[2] += 0x30;
		a[3] = number & 0x0f;
		if(a[3] > 9) a[3] += (0x41 - 10);
		if(a[3] < 10) a[3] += 0x30;

		lcd_draw_char(a[0], color, size);
		lcd_draw_char(a[1], color, size);
		lcd_draw_char(a[2], color, size);
		lcd_draw_char(a[3], color, size);
	}

	if(base != 16)
	{
		itoa(number, a, base); // MODIFICADO ACA
		lcd_print(a, color, size); // MODIFICADO ACA
	}

	if(sufix != NULL) lcd_print(sufix, color, size);
}

void lcd_write_debug(uint8_t * d)
{
	lcd_draw_window(0, 479, 320 - 8, 319, BLACK);
	lcd_set_position(6, 320 - 8);
	lcd_print(d, RED, 1);
}

void lcd_draw_command(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, uint16_t line_col, uint16_t fill_col)
{
	lcd_draw_window(x0, x1, y0, y1, fill_col);
	lcd_draw_line_x(x0, x1, y0, line_col);
	lcd_draw_line_x(x0, x1, y1, line_col);
	lcd_draw_line_y(x0, y0, y1, line_col);
	lcd_draw_line_y(x1, y0, y1, line_col);
}

/*void lcd_gamma_correction()
{
	uint8_t a = 0, b = 0, c = 0, d = 0, a_1 = 0, b_1 = 0, c_1 = 0, e = 0;
	uint8_t data1 = 0, data2 = 0, data3 = 0;
	uint8_t gamma[32];
	uint8_t gamma_max[16] = { 0x0f, 0x3f, 0x3f, 0x0f, 0x1f, 0x0f, 0x7f, 0x0f, 0x0f, 0x7f, 0x0f, 0x1f, 0x0f, 0x3f, 0x3f, 0x0f };
	uint16_t cmd_x0[8];
	uint16_t cmd_y0[4];

	lcd_clear_screen(BLACK);
	lcd_draw_string_10x14(10, 10, "Gamma Correction Setup", WHITE);
	lcd_draw_window(20, 60, 280, 319, RED);
	lcd_draw_window(60, 100, 280, 319, GREEN);
	lcd_draw_window(100, 140, 280, 319, BLUE);
	lcd_draw_window(140, 180, 280, 319, MAGENTA);
	lcd_draw_window(180, 220, 280, 319, YELLOW);
	lcd_draw_window(220, 260, 280, 319, CYAN);
	lcd_draw_window(260, 300, 280, 319, HALF_R);
	lcd_draw_window(20, 60, 240, 279, GRAY1);
	lcd_draw_window(60, 100, 240, 279, GRAY2);
	lcd_draw_window(100, 140, 240, 279, GRAY3);
	lcd_draw_window(140, 180, 240, 279, GRAY4);
	lcd_draw_window(180, 220, 240, 279, GRAY5);
	lcd_draw_window(220, 260, 240, 279, GRAY6);
	lcd_draw_window(260, 300, 240, 279, GRAY7);
	lcd_draw_window(300, 340, 240, 279, HALF_G);
	lcd_draw_window(300, 340, 280, 319, HALF_B);

	lcd_draw_command(380, 460, 260, 300, WHITE, BLUE);
	lcd_set_position(408, 274);
	lcd_print("EXIT", WHITE, 1);

	lcd_draw_command(380, 460, 60, 140, WHITE, BLUE);
	lcd_set_position(410, 95);
	lcd_print("UP", WHITE, 1);

	lcd_draw_command(380, 460, 160, 240, WHITE, BLUE);
	lcd_set_position(410, 195);
	lcd_print("DN", WHITE, 1);

	while((data1 != ILI9488_CMD_POSITIVE_GAMMA_CORRECTION) || (data2 != 15) || (data3 != ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION))
	{
		data1 = pgm_read_byte(&INIT_ILI9341[0] + (b++));
		data2 = pgm_read_byte(&INIT_ILI9341[0] + b);
		data3 = pgm_read_byte(&INIT_ILI9341[0] + b + 16);
		if(b > 250)
		{
			lcd_write_debug("Didnt find data : ");
			lcd_print_HEX8(data1, GREEN, 1, 1);
			lcd_draw_char(' ', WHITE, 1);
			lcd_print_HEX8(data2, GREEN, 1, 1);
			lcd_draw_char(' ', WHITE, 1);
			lcd_print_HEX8(data3, GREEN, 1, 1);
			lcd_print(" cursor position : ", WHITE, 1);
			lcd_number(b, 10, RED, 1, NULL, NULL);
			while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
			while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
			return;
		}
	}

	for(a = 0; a < 7; a++)
	{
		gamma[a] = pgm_read_byte(&INIT_ILI9341[0] + (b + a + 1));
		gamma[a + 16] = pgm_read_byte(&INIT_ILI9341[0] + (b + a + 18));
	}
	gamma[7] = pgm_read_byte(&INIT_ILI9341[0] + (b + 7 + 1)) & 0x0f;
	gamma[8] = (pgm_read_byte(&INIT_ILI9341[0] + (b + 7 + 1)) >> 4) & 0x0f;
	gamma[7 + 16] = pgm_read_byte(&INIT_ILI9341[0] + (b + 7 + 18)) & 0x0f;
	gamma[8 + 16] = (pgm_read_byte(&INIT_ILI9341[0] + (b + 7 + 18)) >> 4) & 0x0f;
	for(a = 9; a < 16; a++)
	{
		gamma[a] = pgm_read_byte(&INIT_ILI9341[0] + (b + a));
		gamma[a + 16] = pgm_read_byte(&INIT_ILI9341[0] + (b + a + 17));
	}

	c = 0;
	for(b = 0; b < 4; b++)
	{
		cmd_y0[b] = 60 + (b * 40);
		for(a = 0; a < 8; a++)
		{
			cmd_x0[a] = 30 + (a * 40);
			lcd_draw_command(cmd_x0[a], cmd_x0[a] + 40, cmd_y0[b], cmd_y0[b] + 40, WHITE, BLACK);
			lcd_set_position(cmd_x0[a] + 15,cmd_y0[b] + 13);
			lcd_print_HEX8(gamma[c], YELLOW, 1, 0);
			c++;
		}
	}

	b = 0;
	while(e != 10)
	{
		while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
		touch_refresh();
		while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
		if((cursor_x >= 380) && (cursor_x <= 460) && (cursor_y >= 260) && (cursor_y <= 300)) e = 10;
		if((cursor_x >= 380) && (cursor_x <= 460) && (cursor_y >= 60) && (cursor_y <= 140)) e = 2;
		if((cursor_x >= 380) && (cursor_x <= 460) && (cursor_y >= 160) && (cursor_y <= 240)) e = 3;
		c = 0;
		for(b = 0; b < 4; b++)
		{
			for(a = 0; a < 8; a++)
			{
				if((cursor_x >= cmd_x0[a]) && (cursor_x <= (cmd_x0[a] + 40)) && (cursor_y >= cmd_y0[b]) && (cursor_y <= (cmd_y0[b] + 40)))
				{
					if(d == 1)
					{
						lcd_draw_command(cmd_x0[a_1], cmd_x0[a_1] + 40, cmd_y0[b_1], cmd_y0[b_1] + 40, WHITE, BLACK);
						lcd_set_position(cmd_x0[a_1] + 15,cmd_y0[b_1] + 13);
						lcd_print_HEX8(gamma[c_1], YELLOW, 1, 0);
					}
					d = 1; a_1 = a; b_1 = b; c_1 = c;
					lcd_draw_command(cmd_x0[a], cmd_x0[a] + 40, cmd_y0[b], cmd_y0[b] + 40, RED, BLACK);
					lcd_draw_command(cmd_x0[a] + 1, cmd_x0[a] + 40 - 1, cmd_y0[b] + 1, cmd_y0[b] + 40 - 1, RED, BLACK);
					lcd_draw_command(cmd_x0[a] + 2, cmd_x0[a] + 40 - 2, cmd_y0[b] + 2, cmd_y0[b] + 40 - 2, RED, BLACK);
					lcd_set_position(cmd_x0[a] + 15,cmd_y0[b] + 13);
					lcd_print_HEX8(gamma[c], YELLOW, 1, 0);
				}
				c++;
			}
		}
		if((d == 1) && (e == 2)) // UP
		{
			if(c_1 > 15) { if(gamma[c_1] < gamma_max[c_1 - 16]) gamma[c_1]++; }
			if(c_1 < 16) { if(gamma[c_1] < gamma_max[c_1]) gamma[c_1]++; }
			lcd_draw_window(cmd_x0[a_1] + 15, cmd_x0[a_1] + 15 + 12, cmd_y0[b_1] + 13, cmd_y0[b_1] + 13 + 16, BLACK);
			lcd_set_position(cmd_x0[a_1] + 15, cmd_y0[b_1] + 13);
			lcd_print_HEX8(gamma[c_1], YELLOW, 1, 0);
			e = 4;
		}
		if((d == 1) && (e == 3)) // DN
		{
			if(gamma[c_1] > 0) gamma[c_1]--;
			lcd_draw_window(cmd_x0[a_1] + 15, cmd_x0[a_1] + 15 + 12, cmd_y0[b_1] + 13, cmd_y0[b_1] + 13 + 16, BLACK);
			lcd_set_position(cmd_x0[a_1] + 15, cmd_y0[b_1] + 13);
			lcd_print_HEX8(gamma[c_1], YELLOW, 1, 0);
			e = 4;
		}
		if(e == 4)
		{
			if(c_1 < 16) lcd_send_cmd(ILI9488_CMD_POSITIVE_GAMMA_CORRECTION);
			if(c_1 > 15) lcd_send_cmd(ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION);
			for(a = 0; a < 7; a++)
			{
				if(c_1 < 16) {lcd_send_data(&gamma[a], 1); }
				if(c_1 > 15) {lcd_send_data(&gamma[a + 16], 1); }
			}
			if(c_1 < 16)
			{
				data1 = gamma[7] & 0x0f;
				data1 |= (gamma[8] << 4) & 0xf0;
			}
			if(c_1 > 15)
			{
				data1 = gamma[7 + 16] & 0x0f;
				data1 |= (gamma[8 + 16] << 4) & 0xf0;
			}
			lcd_send_data(&data1, 1);
			for(a = 8; a < 15; a++)
			{
				if(c_1 < 16) {lcd_send_data(&gamma[a + 1], 1); }
				if(c_1 > 15) {lcd_send_data(&gamma[a + 17], 1); }
			}
			_delay_ms(10);
			e = 0;
		}
	}
}*/

uint16_t touch_samples = 200;
uint8_t touch_accuracy = 10;

#define CAL_POINTS 4

const uint16_t cal_point_x[] PROGMEM = { 10, 160, 320, 470 };
const uint16_t cal_point_y[] PROGMEM = { 10, 110, 210, 310 };
uint16_t cal_x[CAL_POINTS * CAL_POINTS];
uint16_t cal_y[CAL_POINTS * CAL_POINTS];

static void __attribute__((noinline, section(".ramfunc")))
flash_write_page_from_ram(uint32_t address, const uint8_t* data, uint16_t size) {
    // Wait for NVM to be ready
    while (NVM.STATUS & NVM_NVMBUSY_bm);

    // Load page buffer
    for (uint16_t i = 0; i < size; i++) {
        NVM.CMD = NVM_CMD_LOAD_FLASH_BUFFER_gc;
        NVM_ADDR0 = i;
        NVM_DATA0 = data[i];
    }

    // Wait for NVM
    while (NVM.STATUS & NVM_NVMBUSY_bm);

    // Calculate page address
    uint32_t page_addr = address - (address % SPM_PAGESIZE);

    // Erase and write page
    NVM.CMD = NVM_CMD_ERASE_WRITE_APP_PAGE_gc;
    NVM.ADDR0 = page_addr & 0xFF;
    NVM.ADDR1 = (page_addr >> 8) & 0xFF;
    NVM.ADDR2 = (page_addr >> 16) & 0xFF;

    // Execute command
    CCP = CCP_SPM_gc;
    NVM.CTRLA = NVM_CMDEX_bm;

    // Wait for completion
    while (NVM.STATUS & NVM_NVMBUSY_bm);
}

// Calculate checksum
static uint16_t calculate_checksum(const uint16_t* data, uint16_t length) {
    uint16_t sum = 0;
    for (uint16_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// Save calibration with validation
bool save_calibration_to_progmem(uint16_t* cal_x, uint16_t* cal_y) {
    struct {
        uint16_t magic;
        uint16_t cal_x[CAL_POINTS * CAL_POINTS];
        uint16_t cal_y[CAL_POINTS * CAL_POINTS];
        uint16_t checksum;
    } temp_data;

    // Fill temporary structure
    temp_data.magic = 0xCAFE;  // Magic number for validation
    for (uint8_t i = 0; i < CAL_POINTS * CAL_POINTS; i++) {
        temp_data.cal_x[i] = cal_x[i];
        temp_data.cal_y[i] = cal_y[i];
    }

    // Calculate checksum
    temp_data.checksum = calculate_checksum((uint16_t*)&temp_data,
        (sizeof(temp_data) - sizeof(uint16_t)) / sizeof(uint16_t));

    // Get the address of our reserved section
    uint32_t cal_address = (uint32_t)&stored_cal_data;

    // Write to flash using RAM function
    flash_write_page_from_ram(cal_address, (uint8_t*)&temp_data, sizeof(temp_data));

    return true;
}

// Read and validate calibration data
bool read_calibration_from_progmem(uint8_t index_x, uint8_t index_y, uint16_t* cal_x, uint16_t* cal_y) {
    // Check magic number
    if (pgm_read_word(&stored_cal_data.magic) != 0xCAFE) {
        return false;  // Invalid data
    }

    // Read data into temporary buffer for checksum validation
    struct {
        uint16_t magic;
        uint16_t cal_x[CAL_POINTS * CAL_POINTS];
        uint16_t cal_y[CAL_POINTS * CAL_POINTS];
        uint16_t checksum;
    } temp_data;

    // Copy data from PROGMEM
    uint16_t* src = (uint16_t*)&stored_cal_data;
    uint16_t* dst = (uint16_t*)&temp_data;
    for (uint16_t i = 0; i < sizeof(temp_data)/sizeof(uint16_t); i++) {
        dst[i] = pgm_read_word(&src[i]);
    }

    // Validate checksum
    uint16_t calc_checksum = calculate_checksum((uint16_t*)&temp_data,
        (sizeof(temp_data) - sizeof(uint16_t)) / sizeof(uint16_t));
    if (calc_checksum != temp_data.checksum) {
        return false;  // Checksum mismatch
    }

    // Copy valid data to output arrays
    for (uint8_t i = 0; i < CAL_POINTS * CAL_POINTS; i++) {
        cal_x[i] = temp_data.cal_x[i];
        cal_y[i] = temp_data.cal_y[i];
    }

    return true;
}


// Function to save calibration data to PROGMEM
void save_calibration_to_progmem(uint16_t* cal_x, uint16_t* cal_y) {
    // Define a structure to hold all calibration data
    struct {
        uint16_t cal_x[CAL_POINTS * CAL_POINTS];
        uint16_t cal_y[CAL_POINTS * CAL_POINTS];
    } cal_data;

    // Copy calibration data to structure
    for (uint8_t i = 0; i < CAL_POINTS * CAL_POINTS; i++) {
        cal_data.cal_x[i] = cal_x[i];
        cal_data.cal_y[i] = cal_y[i];
    }

    // Choose a flash page address where to store the data
    // Make sure this address is in a section of flash you're not using for program code
    #define CALIBRATION_FLASH_PAGE 0x20000  // Adjust this address as needed

    // Load the page buffer with calibration data
    flash_load_page_buffer((uint8_t*)&cal_data, sizeof(cal_data));

    // Write the page buffer to flash
    flash_write_page(CALIBRATION_FLASH_PAGE);
}

// Function to read calibration data from PROGMEM
void read_calibration_from_progmem(uint16_t* cal_x, uint16_t* cal_y) {
    // Point to the stored calibration data
    const struct {
        uint16_t cal_x[CAL_POINTS * CAL_POINTS];
        uint16_t cal_y[CAL_POINTS * CAL_POINTS];
    } *cal_data = (void*)CALIBRATION_FLASH_PAGE;

    // Read the data from PROGMEM
    for (uint8_t i = 0; i < CAL_POINTS * CAL_POINTS; i++) {
        cal_x[i] = pgm_read_word(&cal_data->cal_x[i]);
        cal_y[i] = pgm_read_word(&cal_data->cal_y[i]);
    }
}

void print_xy(uint16_t pos_x, uint16_t pos_y, uint16_t val_x, uint16_t val_y, uint16_t color)
{
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 20 : pos_y);
	lcd_number(val_x, 10, color, 1, "X: ", NULL);
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 10 : pos_y + 10);
	lcd_number(val_y, 10, color, 1, "Y: ", NULL);
}

static inline void touch_select()
{
	PORT_TOUCH_CS &= ~(1 << PIN_TOUCH_CS); // Chip select
}

static inline void touch_deselect()
{
	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS); // Chip de-select
}

static inline uint16_t touch_send_data(uint8_t cmd)
{
	uint16_t readback;

	touch_select();

	SPI_send(cmd);

	readback = (SPI_send(0x00)) << 8;

	readback |= SPI_send(0x00);

	touch_deselect();

	return readback;
}

uint8_t get_biggest(uint16_t *data, uint8_t array_size) {
    if (array_size == 0) return 0;

    uint8_t max_index = 0;
    uint16_t max_value = data[0];

    for (uint8_t i = 1; i < array_size; i++) {
        if (data[i] > max_value) {
            max_value = data[i];
            max_index = i;
        }
    }

    return max_index;
}

uint8_t get_smallest(uint16_t *data, uint8_t array_size) {
    if (array_size == 0) return 0;

    uint8_t min_index = 0;
    uint16_t min_value = data[0];

    for (uint8_t i = 1; i < array_size; i++) {
        if (data[i] < min_value) {
            min_value = data[i];
            min_index = i;
        }
    }

    return min_index;
}

uint16_t touch_get_raw_old(uint16_t *x, uint16_t *y)
{
	uint32_t readback = 0;
	uint16_t readback_x = 0, readback_y = 0;
	uint16_t a = 0;

	lcd_spi_touch_set();

	for(a = 0; a < touch_samples; a++)
		readback += touch_send_data(0b10000000 | ((XPT2046_MODE_D_XP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	readback_x = readback / touch_samples;

	readback = 0;

	for(a = 0; a < touch_samples; a++)
		readback += touch_send_data(0b10000000 | ((XPT2046_MODE_D_YP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	readback_y = readback / touch_samples;

	touch_send_data(0b10000000 | ((XPT2046_MODE_S_BATT & 0b11111) << 2) | (XPT2046_PD_POWERDOWN & 0b011));

	*x = readback_x;
	*y = readback_y;

	lcd_spi_set();

	return touch_samples;
}

uint16_t accurate_average(uint16_t *data, uint16_t array_size, uint8_t max_deviation) {
    if (array_size == 0) return 0;
    if (array_size == 1) return data[0];

    // First calculate the simple average to use as reference
    uint32_t sum = 0;  // Using uint32_t to prevent overflow
    for (uint16_t i = 0; i < array_size; i++) {
        sum += data[i];
    }
    uint16_t simple_avg = (uint16_t)(sum / array_size);

    // Second pass: sum values within deviation range and count them
    sum = 0;
    uint8_t valid_count = 0;

    for (uint16_t i = 0; i < array_size; i++) {
        // Calculate absolute difference from average
        uint16_t diff = (data[i] > simple_avg) ?
                       (data[i] - simple_avg) :
                       (simple_avg - data[i]);

        // If within deviation range, include in accurate average
        if (diff <= max_deviation) {
            sum += data[i];
            valid_count++;
        }
    }

    // Return 0 if no valid values found
    if (valid_count == 0) return 0;

    // Calculate final average of valid values
    return (uint16_t)(sum / valid_count);
}

uint16_t touch_get_raw(uint16_t *x, uint16_t *y)
{
	uint16_t *readback;
	uint16_t a = 0;

	if(readback) free(readback);
	readback = (uint16_t *)malloc(touch_samples);
	if(!readback) return 0;

	if(touch_accuracy < 10) touch_accuracy = 10;

	lcd_spi_touch_set();

	for(a = 0; a < touch_samples; a++)
		readback[a] = touch_send_data(0b10000000 | ((XPT2046_MODE_D_XP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	*x = accurate_average(readback, touch_samples, touch_accuracy);


	for(a = 0; a < touch_samples; a++)
		readback[a] = touch_send_data(0b10000000 | ((XPT2046_MODE_D_YP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	*y = accurate_average(readback, touch_samples, touch_accuracy);

	touch_send_data(0b10000000 | ((XPT2046_MODE_S_BATT & 0b11111) << 2) | (XPT2046_PD_POWERDOWN & 0b011));

	lcd_spi_set();

	free(readback);

	return touch_samples;
}

uint8_t is_point_on_line(uint16_t x, uint16_t y,
                        uint16_t x1, uint16_t y1,
                        uint16_t x2, uint16_t y2) {
    // Check if point lies on line segment using cross product
    // and checking if point is within the bounding box of the line
    int32_t cross_product = (int32_t)(y - y1) * (x2 - x1) -
                           (int32_t)(x - x1) * (y2 - y1);

    if (cross_product != 0) return 0;

    // Check if point is within the bounding box of the line segment
    if (x1 > x2) {
        uint16_t temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        uint16_t temp = y1;
        y1 = y2;
        y2 = temp;
    }

    return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

uint8_t check_if_in(uint16_t x, uint16_t y, uint16_t *point_x, uint16_t *point_y) {
    // Check if point is within bounds of the polygon
    uint16_t min_x = point_x[0];
    uint16_t max_x = point_x[0];
    uint16_t min_y = point_y[0];
    uint16_t max_y = point_y[0];

    // Find bounding box
    for(int i = 1; i < 4; i++) {
        if(point_x[i] < min_x) min_x = point_x[i];
        if(point_x[i] > max_x) max_x = point_x[i];
        if(point_y[i] < min_y) min_y = point_y[i];
        if(point_y[i] > max_y) max_y = point_y[i];
    }

    // First quick check - if point is outside bounding box, it's definitely outside
    if(x < min_x || x > max_x || y < min_y || y > max_y) {
        return 0;
    }

    // Now do the actual point-in-polygon test using winding number algorithm
    int winding_number = 0;

    for(int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;

        if(point_y[i] <= y) {
            if(point_y[j] > y) {
                int32_t is_left = ((int32_t)point_x[j] - point_x[i]) * ((int32_t)y - point_y[i]) -
                                 ((int32_t)x - point_x[i]) * ((int32_t)point_y[j] - point_y[i]);
                if(is_left > 0) {
                    winding_number++;
                }
            }
        }
        else {
            if(point_y[j] <= y) {
                int32_t is_left = ((int32_t)point_x[j] - point_x[i]) * ((int32_t)y - point_y[i]) -
                                 ((int32_t)x - point_x[i]) * ((int32_t)point_y[j] - point_y[i]);
                if(is_left < 0) {
                    winding_number--;
                }
            }
        }
    }

    return winding_number != 0;
}

static inline void get_xy_pair(uint8_t index_x, uint8_t index_y, uint16_t *x, uint16_t *y)
{
	*x = pgm_read_word(&cal_point_x[index_x]);
	*y = pgm_read_word(&cal_point_y[index_y]);
}

static inline void get_xy_cal_pair(uint8_t index_x, uint8_t index_y, uint16_t *c_x, uint16_t *c_y)
{
	*c_x = cal_x[(index_x * CAL_POINTS) + index_y];
	*c_y = cal_y[(index_x * CAL_POINTS) + index_y];
}

void touch_refresh()
{
	uint8_t index_x = 0, index_y = 0;
	uint16_t x, y, x_old, y_old;
	uint16_t points_x[4];
	uint16_t points_y[4];
	uint8_t c = 0;

	touch_get_raw(&x, &y);

	x_old = x;
	y_old = y;

	// Enforce screen limits
	if(x < cal_x[0]) x = cal_x[0];
	if(x > cal_x[CAL_POINTS * CAL_POINTS - 1]) x = cal_x[CAL_POINTS * CAL_POINTS - 1];
	if(y < cal_y[0]) y = cal_y[0];
	if(y > cal_y[CAL_POINTS * CAL_POINTS - 1]) y = cal_y[CAL_POINTS * CAL_POINTS - 1];

	// Check location of the cursor
	for(uint8_t a = 0; a < (CAL_POINTS - 1); a++)
	{
		for(uint8_t b = 0; b < (CAL_POINTS - 1); b++)
		{
			get_xy_cal_pair(a, b, &points_x[0], &points_y[0]);
			get_xy_cal_pair(a + 1, b, &points_x[1], &points_y[1]);
			get_xy_cal_pair(a, b + 1, &points_x[3], &points_y[3]);
			get_xy_cal_pair(a + 1, b + 1, &points_x[2], &points_y[2]); // OJO el orden de los puntos es importante
			if(check_if_in(x, y, points_x, points_y)) { index_x = a + 1; index_y = b + 1; }
		}
	}

	get_xy_cal_pair(index_x - 1, index_y - 1, &points_x[0], &points_y[0]);
	get_xy_cal_pair(index_x, index_y - 1, &points_x[2], &points_y[2]);
	get_xy_cal_pair(index_x - 1, index_y, &points_x[1], &points_y[1]);
	get_xy_cal_pair(index_x, index_y, &points_x[3], &points_y[3]); // OJO el orden de los puntos es importante

	uint16_t avr_x_l = (points_x[0] + points_x[1]) / 2;
	uint16_t avr_x_h = (points_x[2] + points_x[3]) / 2;
	uint16_t avr_y_l = (points_y[0] + points_y[2]) / 2;
	uint16_t avr_y_h = (points_y[1] + points_y[3]) / 2;

	get_xy_pair(index_x - 1, index_y - 1, &points_x[0], &points_y[0]);
	get_xy_pair(index_x, index_y, &points_x[3], &points_y[3]);

	uint32_t diff_x = (points_x[3] - points_x[0]);
	uint32_t diff_y = (points_y[3] - points_y[0]);

	diff_x *= (x_old - avr_x_l);
	diff_y *= (y_old - avr_y_l);

	cursor_x = ((uint32_t)diff_x / (uint16_t)(avr_x_h - avr_x_l)) + points_x[0];
	cursor_y = ((uint32_t)diff_y / (uint16_t)(avr_y_h - avr_y_l)) + points_y[0];
}

void touch_calibrate()
{
	for(uint8_t a = 0; a < CAL_POINTS; a++)
	{
		for(uint8_t b = 0; b < CAL_POINTS; b++)
		{
			lcd_draw_line_x(pgm_read_word(&cal_point_x[a]) - 1, pgm_read_word(&cal_point_x[a]) + 1, pgm_read_word(&cal_point_y[b]) - 1, MAGENTA);
			lcd_draw_line_x(pgm_read_word(&cal_point_x[a]) - 1, pgm_read_word(&cal_point_x[a]) + 1, pgm_read_word(&cal_point_y[b]), MAGENTA);
			lcd_draw_line_x(pgm_read_word(&cal_point_x[a]) - 1, pgm_read_word(&cal_point_x[a]) + 1, pgm_read_word(&cal_point_y[b]) + 1, MAGENTA);

			touch_wait_press();

			touch_get_raw(&cal_x[(a * CAL_POINTS) + b], &cal_y[(a * CAL_POINTS) + b]);

			print_xy(pgm_read_word(&cal_point_x[a]), pgm_read_word(&cal_point_y[b]), cal_x[(a * CAL_POINTS) + b], cal_y[(a * CAL_POINTS) + b], RED);

			touch_wait_release();

			_delay_ms(100);
		}
	}
}

uint8_t touch_sense()
{
	if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) return 1;
	if((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) return 0;
}

void touch_wait_press()
{
	while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
}

void touch_wait_release()
{
	while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
}
