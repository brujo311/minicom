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

//uint16_t test_pos = 80;

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

const uint8_t str_calibrating[] PROGMEM = "Calibrating... ";
const uint8_t str_touch_the_point[] PROGMEM = "Touch the point...";
const uint8_t str__OK[] PROGMEM = " OK";
const uint8_t str_got[] PROGMEM = "Got -> X : ";
const uint8_t str_Y[] PROGMEM = " Y : ";
const uint8_t str_cal_x[] PROGMEM = " Cal X : ";

const uint8_t str_0x[] PROGMEM = "0x";




/** @var array Chache memory char index row */
uint16_t lcd_index_y = 0;
/** @var array Chache memory char index column */
uint16_t lcd_index_x = 0;

void lcd_send_cmd(uint8_t data)
{
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
	PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select

    SPIE.DATA = data;

    while (!(SPIE.STATUS & SPI_IF_bm))
        ;

	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
	PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
}

void lcd_send_data(uint8_t * data, uint16_t data_bytes_count)
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

void lcd_get_data(uint8_t * data, uint16_t data_bytes_count)
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
		SPI_send(ILI9488_CMD_READ_ID4);
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
		PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
		readback[a] = SPI_send(0x00);
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
	/*DDR_LCD_CS |= (1 << PIN_LCD_CS);
	PORT_LCD_CS |= (1 << PIN_LCD_CS);

	DDR_LCD_CS |= (1 << PIN_LCD_DC);
	PORT_LCD_CS |= (1 << PIN_LCD_DC);

	DDR_TOUCH_CS |= (1 << PIN_TOUCH_CS);
	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS);

    PORTE_DIR |= PIN5_bm | PIN7_bm;

    SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80;
    SPIE.INTCTRL = SPI_INTLVL_LO_gc;*/
}

void lcd_spi_touch_set(void)
{
	SPI_set(4);
	/*DDR_LCD_CS |= (1 << PIN_LCD_CS);
	PORT_LCD_CS |= (1 << PIN_LCD_CS);

	DDR_LCD_CS |= (1 << PIN_LCD_DC);
	PORT_LCD_CS |= (1 << PIN_LCD_DC);

	DDR_TOUCH_CS |= (1 << PIN_TOUCH_CS);
	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS);

    PORTE_DIR |= PIN5_bm | PIN7_bm;

    SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV16_gc | 0x80;
    SPIE.INTCTRL = SPI_INTLVL_LO_gc;*/
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

void lcd_send_color_565(uint16_t color, uint32_t count)
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


void lcd_print(const uint8_t *str, uint16_t color, uint8_t size)
{
	while (pgm_read_byte(str) != '\0') {
		lcd_draw_char(pgm_read_byte(str++), color, size);
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

void lcd_print_number(uint32_t number, uint8_t base, uint16_t color, uint8_t size)
{
	uint8_t a[16];
	itoa(number, a, base); // MODIFICADO ACA
	lcd_write_dynamic(a, color, size); // MODIFICADO ACA
}

void lcd_print_HEX8(uint8_t data, uint16_t text_color, uint8_t text_size, uint8_t toggle0x)
{
	char a = 0;
	char b = 0;

	a = (data >> 4) & 0x0f;
	if(a > 9) a += (0x41 - 10);
	if(a < 10) a += 0x30;
	b = data & 0x0f;
	if(b > 9) b += (0x41 - 10);
	if(b < 10) b += 0x30;

	if(toggle0x == 1) lcd_print(str_0x, text_color, text_size);
	lcd_draw_char(a, text_color, text_size);
	lcd_draw_char(b, text_color, text_size);
}

void lcd_print_HEX16(uint16_t data, uint16_t text_color, uint8_t text_size, uint8_t toggle0x)
{
	char a = 0;
	char b = 0;
	char c = 0;
	char d = 0;

	a = (data >> 12) & 0x0f;
	if(a > 9) a += (0x41 - 10);
	if(a < 10) a += 0x30;
	b = (data >> 8) & 0x0f;
	if(b > 9) b += (0x41 - 10);
	if(b < 10) b += 0x30;
	c = (data >> 4) & 0x0f;
	if(c > 9) c += (0x41 - 10);
	if(c < 10) c += 0x30;
	d = data & 0x0f;
	if(d > 9) d += (0x41 - 10);
	if(d < 10) d += 0x30;

	if(toggle0x == 1) lcd_print(str_0x, text_color, text_size);
	lcd_draw_char(a, text_color, text_size);
	lcd_draw_char(b, text_color, text_size);
	lcd_draw_char(c, text_color, text_size);
	lcd_draw_char(d, text_color, text_size);
}

/*void lcd_get_char_size(uint8_t chr, uint8_t type, uint8_t *size)
{
	if(type == 4) {size[0] = 12; size[1] = 14; }
	if(type == 1) {size[0] = 6; size[1] = 8; }
	if(type == 2) {size[0] = 12; size[1] = 14; }
	if(type == 3) {size[0] = 12; size[1] = 14; }
}*/

void lcd_write_dynamic(uint8_t *text, uint16_t color, uint8_t size)
{
	uint8_t i = 0;
	while (text[i] != '\0') {
		lcd_draw_char(text[i++], color, size); }
}

void lcd_write_debug(uint8_t * d)
{
	uint8_t i = 0;
	lcd_draw_window(0, 479, 320 - 8, 319, BLACK);
	lcd_set_position(6, 320 - 8);
	while (d[i] != '\0') {
		lcd_draw_char(d[i++], RED, 1);
	}
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
			lcd_print_number(b, 10, RED, 1);
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

uint16_t touch_send_data(uint8_t cmd)
{
	uint16_t readback;
	PORT_TOUCH_CS &= ~(1 << PIN_TOUCH_CS); // Chip select

	readback = SPI_send(cmd);

	readback = (SPI_send(0x00)) << 8;

	readback |= SPI_send(0x00);

	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS); // Chip de-select
	return readback;
}

void touch_refresh()
{
	uint32_t readback = 0;
	uint16_t a = 0;

	lcd_spi_touch_set();
	for(a = 0; a < SAMPLES; a++)
		readback += touch_send_data(0b10000000 | ((XPT2046_MODE_D_XP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	readback_x = readback / SAMPLES;

	readback = 0;

	for(a = 0; a < SAMPLES; a++)
		readback += touch_send_data(0b10000000 | ((XPT2046_MODE_D_YP & 0b11111) << 2) | (XPT2046_PD_ON & 0b011)) >> 4;

	readback_y = readback / SAMPLES;

	touch_send_data(0b10000000 | ((XPT2046_MODE_S_BATT & 0b11111) << 2) | (XPT2046_PD_POWERDOWN & 0b011));

	cursor_x = (readback_x / 4) + cursor_calibration_X;
	cursor_y = (readback_y / 6) - cursor_calibration_Y;
	lcd_spi_set();
}

void touch_calibrate()
{
	uint8_t a[16];
	lcd_set_position(40, 26);
	lcd_print(str_calibrating, GREEN, 1);
	lcd_set_position(40, 42);
	lcd_print(str_touch_the_point, WHITE, 1);
	lcd_draw_line_x(239, 241, 159, MAGENTA);
	lcd_draw_line_x(239, 241, 160, MAGENTA);
	lcd_draw_line_x(239, 241, 161, MAGENTA);
	while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
		touch_refresh();
	while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
	lcd_print(str__OK, WHITE, 1);
	cursor_calibration_X = 240 - (readback_x / 4); // OK
	cursor_calibration_Y = 160 - (readback_y / 6); // OK
	lcd_write_debug("Got -> X : ");
	lcd_print_number(readback_x, 10, GREEN, 1);
	lcd_print(str_Y, RED, 1);
	lcd_print_number(readback_y, 10, GREEN, 1);
	lcd_print(str_cal_x, RED, 1);
	lcd_print_number(cursor_calibration_X, 10, GREEN, 1);
	lcd_print(str_Y, RED, 1);
	lcd_print_number(cursor_calibration_Y, 10, GREEN, 1);
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
