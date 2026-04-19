

// Driver for ILI9488 + XPT2046 LCD & TOUCH SCREEN
// Made by nIngen & Claude AI

#define LCD_SETTINGS_START_ADDRESS 0

#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "lcd_driver.h"
#include "fonts.h"
#include "colors.h"
#include "nvm_driver.h"
#include "mcu_driver.h"

/** @var array Chache memory char index row */
uint16_t lcd_index_y = 0;
/** @var array Chache memory char index column */
uint16_t lcd_index_x = 0;

#define DELAY   0x80 // Delay flag

// COMMANDS IN EEPROM ADDRESS 0X0002
/**/

extern uint16_t lcd_size_x = 480;
extern uint16_t lcd_size_y = 320;

uint16_t touch_samples = 250;
uint8_t touch_accuracy = 10;
uint8_t cal_points = 4;

uint16_t *cal_point_x;
uint16_t *cal_point_y;

uint16_t *cal_x;
uint16_t *cal_y;

uint16_t cursor_x = 0;
uint16_t cursor_y = 0;

// Optimized SPI send function for AVR
static inline void lcd_spi_send(uint8_t data)
{
    SPI_DATA = data;
    while(!SPI_IF);
}

static inline uint8_t lcd_spi_transfer(uint8_t data)
{
    SPI_DATA = data;
    while(!SPI_IF);
    return SPI_DATA;
}

static inline void lcd_send_cmd(uint8_t data)
{
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select
	PORT_LCD_DC &= ~(1 << PIN_LCD_DC); // Command select

    lcd_spi_send(data);

	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
	PORT_LCD_DC |= (1 << PIN_LCD_DC); // Data select
}

static inline void lcd_send_data(uint8_t * data, uint16_t data_bytes_count)
{
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select

	for(uint16_t a = 0; a < data_bytes_count; a++)
		lcd_spi_send(data[a]);

	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
}

static inline void lcd_get_data(uint8_t * data, uint16_t data_bytes_count)
{
	PORT_LCD_CS &= ~(1 << PIN_LCD_CS); // Chip select

	for(uint16_t a = 0; a < data_bytes_count; a++)
		data[a] = lcd_spi_transfer(data[a]);

	PORT_LCD_CS |= (1 << PIN_LCD_CS); // Chip de-select
}

void lcd_io()
{
    DDR_LCD_CS |= (1 << PIN_LCD_CS);
	PORT_LCD_CS |= (1 << PIN_LCD_CS);

	DDR_LCD_CS |= (1 << PIN_LCD_DC);
	PORT_LCD_CS |= (1 << PIN_LCD_DC);

	DDR_TOUCH_CS |= (1 << PIN_TOUCH_CS);
	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS);

    SPI_DIR |= SPI_SCK | SPI_MOSI;
    SPI_DIR &= ~(SPI_MISO);
}

void lcd_spi_set(void)
{
    SPI_LCD_SET;
}

static void lcd_reset(void)
{
	PORT_LCD_RST |= (1 << PIN_LCD_RST);
	DDR_LCD_RST  |= (1 << PIN_LCD_RST);
	_delay_ms(200);
	PORT_LCD_RST &= ~(1 << PIN_LCD_RST);
	_delay_ms(200);
	PORT_LCD_RST |=  (1 << PIN_LCD_RST);
}

/*static void lcd_copy_commands_to_eeprom()
{
	const uint8_t lcd_config_data[] =
	{
						11, // Command count
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
					ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION, 15+DELAY, 0x05, 0x10, 0x14, 0x08, 0x10, 0x08, 0x35, 0x88, 0x24, 0x0a, 0x04, 0x08, 0x06, 0x06, 0x02, 10,
					ILI9488_CMD_NORMAL_DISP_MODE_ON, DELAY, 10
	};

	eeprom_write_word(0, sizeof(lcd_config_data) + 2);

	uint16_t ee_a = 2;

	for(ee_a = 2; ee_a < (sizeof(lcd_config_data) + 2); ee_a++)
	{
		eeprom_write_byte(ee_a, lcd_config_data[ee_a - 2]);
	}
}*/

static void lcd_send_init_commands(uint16_t commands)
{
	uint8_t milliseconds;
	uint8_t numOfCommands;
	uint8_t numOfArguments;
	uint8_t data;

	//lcd_copy_commands_to_eeprom();

	uint16_t ee_a = 2;

	numOfCommands = eeprom_read_byte(commands++);
	//eeprom_write_byte(ee_a++, numOfCommands); //DEBUG

	while (numOfCommands--) {
		data = eeprom_read_byte(commands++);
		lcd_send_cmd(data);
		//eeprom_write_byte(ee_a++, data); //DEBUG
		numOfArguments = eeprom_read_byte(commands++);
		//eeprom_write_byte(ee_a++, numOfArguments); //DEBUG
		milliseconds = numOfArguments & DELAY; // Store the delay flag in milliseconds
		numOfArguments &= ~DELAY; // Remove the delay flag from the number of arguments
		while (numOfArguments--) {
			data = eeprom_read_byte(commands++);
			//eeprom_write_byte(ee_a++, data); //DEBUG
			lcd_send_data(&data, 1); // send arguments
		}
		if (milliseconds) {
			milliseconds = eeprom_read_byte(commands++); // Read the last arg, value in milliseconds
			//eeprom_write_byte(ee_a++, milliseconds); //DEBUG
			while (milliseconds--) {
				_delay_ms(1);
			}
		}
	}
	//eeprom_write_word(0, ee_a);
}

void lcd_on(void)
{
	lcd_send_cmd(ILI9488_CMD_DISPLAY_ON);
	_delay_us(50);
}

void lcd_off(void)
{
	lcd_send_cmd(ILI9488_CMD_PIXEL_OFF);
}

void lcd_init_controller(void)
{
    lcd_io();
	lcd_spi_set();
	lcd_reset();
	lcd_send_init_commands(LCD_SETTINGS_START_ADDRESS + 2);
	lcd_clear_screen(BLACK);
	lcd_on();
	load_touch_calibration();
}

static inline uint16_t _24bits_to_565(uint8_t r, uint8_t g, uint8_t b)
{
   return  (uint16_t)((r & 0xf8) << 5) |
		   (uint16_t)((g & 0x1c) << 11) |
		   (uint16_t)((g & 0xe0) >> 5) |
		   (uint16_t)(b & 0xf8);
}

static inline void lcd_send_color_565(uint16_t color, uint32_t count)
{
	lcd_send_cmd(ILI9488_CMD_MEMORY_WRITE);
	while (count--) {
		lcd_send_data((uint8_t *)(&color), 2); // MODIFICADO ACA
	}
}

static inline void lcd_send_color_565_diff(uint16_t *color, uint32_t count)
{
	lcd_send_cmd(ILI9488_CMD_MEMORY_WRITE);
	while (count--) {
		lcd_send_data((uint8_t *)color++, 2); // MODIFICADO ACA
	}
}

static uint8_t lcd_set_partial_area(uint16_t sRow, uint16_t eRow)
{
	//uint8_t a[2];
    uint16_t sRow_be = __builtin_bswap16(sRow);
    uint16_t eRow_be = __builtin_bswap16(eRow);

	if ((sRow > MAX_X) || (eRow > MAX_Y))
		return 0;

	//a[0] = (uint8_t) (sRow >> 8);
	//a[1] = (uint8_t) (sRow);

	lcd_send_cmd(ILI9488_CMD_PARTIAL_AREA); // column address set

	lcd_send_data((uint8_t *)(&sRow_be), 2); // start start Row MODIFICADO ACA

	//a[0] = (uint8_t) (eRow >> 8);
	//a[1] = (uint8_t) (eRow);

	lcd_send_data((uint8_t *)(&eRow_be), 2); // end end Row // MODIFICADO ACA

	lcd_send_cmd(ILI9488_CMD_PARTIAL_MODE_ON); // partial area on

	return 1;
}

uint8_t lcd_set_window(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1)
{
    //uint16_t x0_be = __builtin_bswap16(x0);
    //uint16_t x1_be = __builtin_bswap16(x1);
    //uint16_t y0_be = __builtin_bswap16(y0);
    //uint16_t y1_be = __builtin_bswap16(y1);
	uint8_t a[4];

	if ((x0 > x1) || (x1 > SIZE_X) || (y0 > y1) || (y1 > SIZE_Y)) {
		return 0;
	}

	a[0] = (uint8_t) (x0 >> 8);
	a[1] = (uint8_t) (x0);
	a[2] = (uint8_t) (x1 >> 8);
	a[3] = (uint8_t) (x1);

	lcd_send_cmd(ILI9488_CMD_COLUMN_ADDRESS_SET); // column address set
	lcd_send_data(&a[0], 1);
	lcd_send_data(&a[1], 1);
	lcd_send_data(&a[2], 1);
	lcd_send_data(&a[3], 1);
	//lcd_send_data((uint8_t *)(&x0_be), 2);
	//lcd_send_data((uint8_t *)(&x1_be), 2);

	a[0] = (uint8_t) (y0 >> 8);
	a[1] = (uint8_t) (y0);
	a[2] = (uint8_t) (y1 >> 8);
	a[3] = (uint8_t) (y1);

	lcd_send_cmd(ILI9488_CMD_PAGE_ADDRESS_SET); // row address set
	lcd_send_data(&a[0], 1); // end y position // MODIFICADO ACA
	lcd_send_data(&a[1], 1);
	lcd_send_data(&a[2], 1);
	lcd_send_data(&a[3], 1);
	//lcd_send_data((uint8_t *)(&y0_be), 2);
	//lcd_send_data((uint8_t *)(&y1_be), 2);

	return 1;
}

void lcd_set_position(uint16_t x, uint16_t y)
{
	if ((x > SIZE_X - (CHARS_COLS_LEN + 1)) && (y > SIZE_Y - (CHARS_ROWS_LEN))) return; // OUT OF RANGE

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

void lcd_draw_pixel_24bit(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
	lcd_set_window(x, x, y, y);
	lcd_send_color_565(_24bits_to_565(r, g, b), 1);
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

void lcd_draw_string(uint8_t *str, uint16_t color, uint8_t size)
{
	uint8_t i = 0;
	while ((str[i] != '\0') && (str[i] != '\n') && (str[i] != '\r')) {
		lcd_draw_char(str[i++], color, size);
	}
}

void lcd_draw_string_f(const uint8_t *str, uint16_t color, uint8_t size)
{
	uint8_t i;
	uint8_t a = 0;

	while (1)
	{
		i = pgm_read_byte(str + a);

		if ((i == '\0') || (i == '\n') || (i == '\r'))
			break;

		lcd_draw_char(i, color, size);
		a++;
	}
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

void lcd_number(uint32_t number, uint8_t base, uint16_t color, uint8_t size, uint8_t * prefix, uint8_t * sufix)
{
	uint8_t a[16];
	uint8_t b;

	if(prefix != NULL) lcd_draw_string(prefix, color, size);

	ultoa(number, a, base); // MODIFICADO ACA
	lcd_draw_string(a, color, size); // MODIFICADO ACA

	if(sufix != NULL) lcd_draw_string(sufix, color, size);
}

void lcd_write_debug(uint8_t * d)
{
	lcd_draw_window(0, 479, 320 - 8, 319, BLACK);
	lcd_set_position(6, 320 - 8);
	lcd_draw_string(d, RED, 1);
}

void lcd_print_xy(uint16_t pos_x, uint16_t pos_y, uint16_t val_x, uint16_t val_y, uint16_t color)
{
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 20 : pos_y);
	lcd_number(val_x, 10, color, 1, "X: ", NULL);
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 10 : pos_y + 10);
	lcd_number(val_y, 10, color, 1, "Y: ", NULL);
}

void lcd_draw_xy(uint16_t pos_x, uint16_t pos_y, uint16_t val_x, uint16_t val_y, uint16_t color_1, uint16_t color_2)
{
    lcd_draw_line_x(pos_x - 1, pos_x + 1, pos_y - 1, color_1);
    lcd_draw_line_x(pos_x - 1, pos_x + 1, pos_y, color_1);
    lcd_draw_line_x(pos_x - 1, pos_x + 1, pos_y + 1, color_1);
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 20 : pos_y);
	lcd_number(val_x, 10, color_2, 1, "X: ", NULL);
	lcd_set_position(pos_x > MAX_X - 45 ? pos_x - 45 : pos_x + 5, pos_y > MAX_Y - 20 ? pos_y - 10 : pos_y + 10);
	lcd_number(val_y, 10, color_2, 1, "Y: ", NULL);
}

void touch_spi_set(void)
{
	SPI_TOUCH_SET;
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

	lcd_spi_send(cmd);

	readback = (lcd_spi_transfer(0x00)) << 8;

	readback |= lcd_spi_transfer(0x00);

	touch_deselect();

	return readback;
}

uint8_t touch_sense()
{
	if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) return 1;
	if((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) return 0;
    return 0;
}

void touch_wait_press()
{
	while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
}

void touch_wait_release()
{
	while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
}

static uint16_t accurate_average(uint16_t *data, uint16_t array_size, uint8_t max_deviation) {
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
	uint16_t *readback = NULL;
	uint16_t a = 0;

	readback = (uint16_t *)malloc((size_t)touch_samples * sizeof(uint16_t));
	if(!readback) return 0;

	if(touch_accuracy < 10) touch_accuracy = 10;

	touch_spi_set();

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

static uint8_t check_if_in(uint16_t x, uint16_t y, uint16_t *point_x, uint16_t *point_y) {
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
	*x = cal_point_x[index_x];
	*y = cal_point_y[index_y];
}

static inline void get_xy_cal_pair(uint8_t index_x, uint8_t index_y, uint16_t *c_x, uint16_t *c_y)
{
	*c_x = cal_x[(index_x * cal_points) + index_y];
	*c_y = cal_y[(index_x * cal_points) + index_y];
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
	if(x > cal_x[cal_points * cal_points - 1]) x = cal_x[cal_points * cal_points - 1];
	if(y < cal_y[0]) y = cal_y[0];
	if(y > cal_y[cal_points * cal_points - 1]) y = cal_y[cal_points * cal_points - 1];

	// Check location of the cursor
	for(uint8_t a = 0; a < (cal_points - 1); a++)
	{
		for(uint8_t b = 0; b < (cal_points - 1); b++)
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

void allocation_error() { lcd_write_debug("Allocation error"); }

uint8_t load_touch_calibration()
{
	uint16_t addr = LCD_SETTINGS_START_ADDRESS + eeprom_read_word(LCD_SETTINGS_START_ADDRESS) + 2;
	
	cal_points = eeprom_read_byte(addr++);
    
	if((cal_points < 1) || (cal_points > 10)) 
	{
		cal_points = 4;

		if(cal_point_x) free(cal_point_x);
		if(cal_point_y) free(cal_point_y);

		cal_point_x = (uint16_t*)malloc(cal_points * sizeof(uint16_t));
		cal_point_y = (uint16_t*)malloc(cal_points * sizeof(uint16_t));

		if(!cal_point_x) { allocation_error(); return 1; }
		if(!cal_point_y) { allocation_error(); return 1; }

		cal_point_x[0] = 10; cal_point_y[0] = 10;
		cal_point_x[1] = 160; cal_point_y[1] = 110;
		cal_point_x[2] = 320; cal_point_y[2] = 210;
		cal_point_x[3] = 470; cal_point_y[3] = 310;
	
		touch_calibrate(); // No valid calibration data found
	}

	if(cal_x) free(cal_x);
	if(cal_y) free(cal_y);

	cal_x = (uint16_t*)malloc(cal_points * cal_points * sizeof(uint16_t));
	cal_y = (uint16_t*)malloc(cal_points * cal_points * sizeof(uint16_t));

	if(!cal_x) { allocation_error(); return 1; }
	if(!cal_y) { allocation_error(); return 1; }

	if(cal_point_x) free(cal_point_x);
	if(cal_point_y) free(cal_point_y);

	cal_point_x = (uint16_t*)malloc(cal_points * sizeof(uint16_t));
	cal_point_y = (uint16_t*)malloc(cal_points * sizeof(uint16_t));

	if(!cal_point_x) { allocation_error(); return 1; }
	if(!cal_point_y) { allocation_error(); return 1; }

	for (uint8_t c = 0; c < cal_points; c++)
	{
		cal_point_x[c]  = ((uint16_t)eeprom_read_byte(addr++) << 8);
		cal_point_x[c] |= eeprom_read_byte(addr++);
		cal_point_y[c]  = ((uint16_t)eeprom_read_byte(addr++) << 8);
		cal_point_y[c] |= eeprom_read_byte(addr++);
	}

	for (uint8_t a = 0; a < cal_points; a++)
	{
		for (uint8_t b = 0; b < cal_points; b++)
		{
			uint8_t index = (a * cal_points) + b;

			cal_x[index]  = ((uint16_t)eeprom_read_byte(addr++) << 8);
			cal_x[index] |= eeprom_read_byte(addr++);
			cal_y[index]  = ((uint16_t)eeprom_read_byte(addr++) << 8);
			cal_y[index] |= eeprom_read_byte(addr++);
		}
	}

    return 0;
}

void save_touch_calibration(uint16_t *cal_new_x, uint16_t *cal_new_y)
{
	uint16_t addr = LCD_SETTINGS_START_ADDRESS + eeprom_read_word(LCD_SETTINGS_START_ADDRESS) + 2;

	eeprom_write_byte(addr++, cal_points);

	for(uint8_t c = 0; c < cal_points; c++)
	{
		eeprom_write_byte(addr++, (uint8_t)(cal_point_x[c] >> 8));
		eeprom_write_byte(addr++, (uint8_t)cal_point_x[c]);
		eeprom_write_byte(addr++, (uint8_t)(cal_point_y[c] >> 8));
		eeprom_write_byte(addr++, (uint8_t)cal_point_y[c]);
	}

	for(uint8_t a = 0; a < cal_points; a++)
	{
		for(uint8_t b = 0; b < cal_points; b++)
		{
			eeprom_write_byte(addr++, (uint8_t)(cal_new_x[(a * cal_points) + b] >> 8));
			eeprom_write_byte(addr++, (uint8_t)cal_new_x[(a * cal_points) + b]);
			eeprom_write_byte(addr++, (uint8_t)(cal_new_y[(a * cal_points) + b] >> 8));
			eeprom_write_byte(addr++, (uint8_t)cal_new_y[(a * cal_points) + b]);
		}
	}

	eeprom_write_word(LCD_SETTINGS_START_ADDRESS + eeprom_read_word(LCD_SETTINGS_START_ADDRESS), LCD_SETTINGS_START_ADDRESS + eeprom_read_word(LCD_SETTINGS_START_ADDRESS) + addr);
}

void touch_calibrate()
{
    uint16_t cal_new_x[cal_points * cal_points];
    uint16_t cal_new_y[cal_points * cal_points];
	lcd_clear_screen(BLACK);
	for(uint8_t a = 0; a < cal_points; a++)
	{
		for(uint8_t b = 0; b < cal_points; b++)
		{
			lcd_draw_line_x(cal_point_x[a] - 1, cal_point_x[a] + 1, cal_point_y[b] - 1, MAGENTA);
			lcd_draw_line_x(cal_point_x[a] - 1, cal_point_x[a] + 1, cal_point_y[b], MAGENTA);
			lcd_draw_line_x(cal_point_x[a] - 1, cal_point_x[a] + 1, cal_point_y[b] + 1, MAGENTA);

			touch_wait_press();

			touch_get_raw(&cal_new_x[(a * cal_points) + b], &cal_new_y[(a * cal_points) + b]);

			lcd_print_xy(cal_point_x[a], cal_point_y[b], cal_new_x[(a * cal_points) + b], cal_new_y[(a * cal_points) + b], RED);

			touch_wait_release();

			_delay_ms(200);
		}
	}
	touch_wait_press();
    _delay_ms(1000);
    if(!touch_sense()) lcd_write_debug("Not saved");
    if(touch_sense()) {
        lcd_write_debug("Saving...");
        save_touch_calibration(cal_new_x, cal_new_y);
        lcd_write_debug("Saved"); }
    touch_wait_release();
}



