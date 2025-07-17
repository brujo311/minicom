#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "x128_ili9488_driver.h"
#include "ningen_ui.h"
#include "lcd_driver.h"
#include "programs.h"
#include "SD_routines.h"
#include "FAT32.h"
#include "strings.h"

#define CONSOLE_ON 0
#define CONSOLE_WINDOWED 1
#define CONSOLE_KEYPAD_ON 2
#define CONSOLE_INPUT_OVERFLOW 3
#define CONSOLE_KEYPAD_SIZE 30
#define CONSOLE_SETTINGS_START_ADD 0x200
#define TOUCH_SETTINGS_START_ADD 0x100

//unsigned long startBlock;
//unsigned long totalBlocks;
//unsigned char buffer[512];
//unsigned long firstDataSector, rootCluster, totalClusters;
//unsigned int  bytesPerSector, sectorPerCluster, reservedSectorCount;



uint8_t *console_buffer = NULL;
uint16_t console_buffer_size = 1024;
uint8_t *console_input_buffer = NULL;
uint8_t *console_input_buffer_old = NULL;
uint8_t console_input_buffer_size = 128;
uint8_t console_input_index;
uint8_t console_status = 0;
uint8_t console_root = '$';
uint8_t console_keypad_size;
uint16_t console_back_color;
uint16_t console_text_color;
uint8_t console_text_type;
uint8_t console_max_lines;
uint8_t console_max_cols;
uint16_t index_b = 0;
uint16_t index_x = 0;
uint16_t index_y = 0;

uint8_t pwm_port = 0;
uint8_t pwm_pin = 0;
uint8_t pwm_run = 0;
uint8_t pwm_sel_3 = 0;
uint8_t pwm_sel_4 = 0;
uint8_t pwm_sel_5 = 0;
uint16_t pwm_freq = 0;
uint8_t pwm_duty = 0;

//float ing_offset[8];
//float ing_factor[8];

uint8_t *form_elements_x1 = NULL;
uint8_t *form_elements_x2 = NULL;
uint8_t *form_elements_y1 = NULL;
uint8_t *form_elements_y2 = NULL;

uint16_t mem_check()
{
	uint16_t free_mem = 1;
	uint8_t *test;
	test = (uint8_t *)malloc(free_mem);
	while(test)
	{
		test = (uint8_t *)malloc(free_mem);
		free(test);
		free_mem++;
	}
	return free_mem;
}

void boot()
{
	uint8_t a = 0;
	uint16_t b = 1;
	uint8_t *test;

	ui_write(0, 0, get_string(100), WHITE, SIZE3);
	ui_write(0, 16, get_string(29), WHITE, SIZE3);

	while(a == 0)
	{
		test = (uint8_t *)malloc(b);
		lcd_draw_window(160, 220, 16, 32, BLACK);
		lcd_set_position(160, 16);
		lcd_number(b, 10, WHITE, SIZE3, NULL, NULL);
		if(!test)
		{
			_delay_ms(2000);
			a = 1;
		}
		//_delay_us(100);
		free(test);
		b++;
	}
	/*ui_write(0, 24, get_string(26), WHITE, SIZE3);
	a = 0;
	while(a == 0)
	{
		test = (uint8_t *)malloc(b);
		lcd_draw_window(180, 240, 24, 32, BLACK);
		lcd_set_position(180, 0);
		lcd_number(b, 10, WHITE, SIZE3, NULL, NULL);
		if(!test)
		{
			_delay_ms(2000);
			a = 1;
		}
		//_delay_us(100);
		free(test);
		b++;
	}*/

	a = 0;

	ui_form(0, 0, 479, 319, get_string(0), color_border, color_back, color_text, 0, 2, 1, NoControlBox);
	ui_write(20, 20, get_string(90), MAGENTA, 3);
	ui_command(20, 50, 200, 30, get_string(91), color_border, color_cmd, color_text,  1, 3);
	ui_command(20, 90, 200, 30, get_string(92), color_border, color_cmd, color_text,  1, 3);

	while(a == 0)
	{
		touch_wait_press();
		touch_refresh();
		if(cursor_x > 20 && cursor_x < 220 && cursor_y > 50 && cursor_y < 80)
		{
			ui_command(20, 50, 200, 30, get_string(91), color_border, color_cmd_selected, color_text,  2, 3);
			a = 1;
			touch_wait_release();
			ui_command(20, 50, 200, 30, get_string(91), color_border, color_cmd, color_text,  1, 3);
		}
		if(cursor_x > 20 && cursor_x < 220 && cursor_y > 90 && cursor_y < 120)
		{
			ui_command(20, 90, 200, 30, get_string(92), color_border, color_cmd_selected, color_text,  2, 3);
			a = 2;
			touch_wait_release();
			ui_command(20, 90, 200, 30, get_string(92), color_border, color_cmd, color_text,  1, 3);
		}
	}
	_delay_ms(1000);
	if(a == 1)
	{
		gui_load();
	}
	if(a == 2)
	{
		console(0, 0);
		console_write(get_string(100));
		load_gui_settings();
		SD_init();
		console_update(0, 0);
		_delay_ms(5000);
		console(0, 1);
	}
}

// Function to store a new setting
static uint8_t calculate_string_length(uint8_t *str) {
    uint8_t length = 0;
    while (str[length] != '\0' && length < 32) {
        length++;
    }
    return length;
}

uint8_t store_setting(uint16_t start_address, uint8_t type, uint8_t *data, uint8_t *caption) {
    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Handle the case where EEPROM is uninitialized
    if (num_settings == 0xFF) {
        num_settings = 0;
    }

    // Calculate the control byte
    uint8_t control_byte = ((type & 0x07) | ((calculate_string_length(caption) & 0x1F) << 3));

    // Calculate the length of the setting
    uint8_t setting_length = (type == HEX8 || type == BIN || type == DEC8 || type == BOOL) ? 1 :
                            (type == HEX16 || type == DEC16) ? 2 :
                            (type == HEX32 || type == FLOAT) ? 4 : 0;

    // Find the position to store the new setting
    uint16_t settings_address = start_address + 1;
    for (uint8_t i = 0; i < num_settings; i++) {
        uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
        uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
        uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
                            (stored_type == HEX16 || stored_type == DEC16) ? 2 :
                            (stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;

        settings_address += 1 + stored_setting_length;
        settings_address += (stored_control_byte & 0xf8) >> 3;
    }

    // Store the control byte
    EEPROM_WriteByte(settings_address, control_byte);
    settings_address++;

    // Store the setting data
    for (uint8_t i = 0; i < setting_length; i++) {
        EEPROM_WriteByte(settings_address, data[i]);
        settings_address++;
    }

    // Store the caption
    for (uint8_t i = 0; i < calculate_string_length(caption); i++) {
        EEPROM_WriteByte(settings_address, caption[i]);
        settings_address++;
    }

    // Increment the setting count
    num_settings++;
    EEPROM_WriteByte(start_address, num_settings);

    return num_settings; // Return the new setting number
}

// Function to read a setting
uint8_t write_setting(uint16_t start_address, uint8_t setting_number, uint8_t *data) {
    // Determine the start address for the settings data
    uint16_t settings_address = start_address + 1;

    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Ensure the requested setting number is within bounds
    if (setting_number < 1 || setting_number > num_settings || num_settings == 0xff) {
        return 0; // Setting doesn't exist
    }

    // Loop through the settings
    for (uint8_t i = 1; i <= num_settings; i++) {
        // Read the data type and name length
		uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
		uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
		uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
								(stored_type == HEX16 || stored_type == DEC16) ? 2 :
								(stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;
		if(i != setting_number)
		{
			settings_address += 1 + stored_setting_length;
			settings_address += (stored_control_byte & 0xf8) >> 3;
		}
		if(i == setting_number)
		{
			for(uint8_t a = 0; a < stored_setting_length; a++)
				EEPROM_WriteByte(settings_address + a + 1, data[a]);
			return stored_setting_length;
		}
    }

    return 0; // Setting not found
}

// Function to read a setting
uint16_t get_setting_length(uint16_t start_address) {
    // Determine the start address for the settings data
    uint16_t settings_address = start_address + 1;

    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Ensure the requested setting number is within bounds
    if (num_settings == 0 || num_settings == 0xff) {
        return 0; // Setting doesn't exist
    }

    // Loop through the settings
    for (uint8_t i = 1; i <= num_settings; i++) {
        // Read the data type and name length
		uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
		uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
		uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
								(stored_type == HEX16 || stored_type == DEC16) ? 2 :
								(stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;
		settings_address += 1 + stored_setting_length;
		settings_address += (stored_control_byte & 0xf8) >> 3;
    }

    return settings_address;
}

// Function to read a setting
uint8_t read_setting(uint16_t start_address, uint8_t setting_number, uint8_t *readback) {
    // Determine the start address for the settings data
    uint16_t settings_address = start_address + 1;

    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Ensure the requested setting number is within bounds
    if (setting_number < 1 || setting_number > num_settings || num_settings == 0xff) {
        return 0; // Setting doesn't exist
    }

    // Loop through the settings
    for (uint8_t i = 1; i <= num_settings; i++) {
        // Read the data type and name length
		uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
		uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
		uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
								(stored_type == HEX16 || stored_type == DEC16) ? 2 :
								(stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;
		if(i != setting_number)
		{
			settings_address += 1 + stored_setting_length;
			settings_address += (stored_control_byte & 0xf8) >> 3;
		}
		if(i == setting_number)
		{
			for(uint8_t a = 0; a < stored_setting_length; a++)
				readback[a] = EEPROM_ReadByte(settings_address + a + 1);
			return stored_setting_length;
		}
    }

    return 0; // Setting not found
}

// Function to read a setting
uint8_t read_setting_caption(uint16_t start_address, uint8_t setting_number, uint8_t *readback) {
    // Determine the start address for the settings data
    uint16_t settings_address = start_address + 1;

    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Ensure the requested setting number is within bounds
    if (setting_number < 1 || setting_number > num_settings || num_settings == 0xff) {
        return 0; // Setting doesn't exist
    }

    // Loop through the settings
    for (uint8_t i = 1; i <= num_settings; i++) {
        // Read the data type and name length
		uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
		uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
		uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
								(stored_type == HEX16 || stored_type == DEC16) ? 2 :
								(stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;
		uint8_t stored_caption_length = (stored_control_byte & 0xf8) >> 3;
		if(i != setting_number)
			settings_address += 1 + stored_setting_length + stored_caption_length;
		if(i == setting_number)
		{
			for(uint8_t a = 0; a < stored_caption_length; a++)
				readback[a] = EEPROM_ReadByte(settings_address + a + 1 + stored_setting_length);
			readback[stored_caption_length] = '\0';
			return stored_caption_length;
		}
    }

    return 0; // Setting not found
}

uint8_t read_setting_data_type(uint16_t start_address, uint8_t setting_number, uint8_t *readback) {
    // Determine the start address for the settings data
    uint16_t settings_address = start_address + 1;

    // Read the total number of settings
    uint8_t num_settings = EEPROM_ReadByte(start_address);

    // Ensure the requested setting number is within bounds
    if (setting_number < 1 || setting_number > num_settings || num_settings == 0xff) {
        return 0; // Setting doesn't exist
    }

    // Loop through the settings
    for (uint8_t i = 1; i <= num_settings; i++) {
        // Read the data type and name length
		uint8_t stored_control_byte = EEPROM_ReadByte(settings_address);
		uint8_t stored_type = stored_control_byte & 0x07; // Bits 0 to 2
		uint8_t stored_setting_length = (stored_type == HEX8 || stored_type == BIN || stored_type == DEC8 || stored_type == BOOL) ? 1 :
								(stored_type == HEX16 || stored_type == DEC16) ? 2 :
								(stored_type == HEX32 || stored_type == FLOAT) ? 4 : 0;
		uint8_t stored_caption_length = (stored_control_byte & 0xf8) >> 3;
		if(i != setting_number)
			settings_address += 1 + stored_setting_length + stored_caption_length;
		if(i == setting_number) { readback[0] = stored_type; return 1; }
    }

    return 0; // Setting not found
}

uint8_t gui_allocate_elements(uint8_t size)
{
	if(!size) return 1;

	if(form_elements_x1 && form_elements_x2 && form_elements_y1 && form_elements_y2)
		return 0; // Buffer already allocated

	gui_free_elements();

	form_elements_x1 = (uint8_t *)malloc(size);
	form_elements_x2 = (uint8_t *)malloc(size);
	form_elements_y1 = (uint8_t *)malloc(size);
	form_elements_y2 = (uint8_t *)malloc(size);

	if(form_elements_x1 == NULL || form_elements_x2 == NULL || form_elements_y1 == NULL || form_elements_y2 == NULL)
	{
		gui_free_elements();
		return 1;
	}
	return 0;
}

void gui_free_elements()
{

	if (form_elements_x1) {
        // Free the existing buffer if it was allocated
        free(form_elements_x1);
        form_elements_x1 = NULL;
    }

    if (form_elements_x2) {
        // Free the existing buffer if it was allocated
        free(form_elements_x2);
        form_elements_x2 = NULL;
    }

    if (form_elements_y1) {
        // Free the existing buffer if it was allocated
        free(form_elements_y1);
        form_elements_y1 = NULL;
    }

	if (form_elements_y2) {
        // Free the existing buffer if it was allocated
        free(form_elements_y2);
        form_elements_y2 = NULL;
    }
}

void gui_form_main()
{
	ui_form(0, 0, 479, 319, get_string(0), color_border, color_back, color_text, 0, 2, 1, NoControlBox);
	ui_write(20, 20, get_string(30), MAGENTA, 3);
	ui_check_box(380, 5, 12, 12, get_string(31), color_border, BLACK, color_text, 1, 1);
	ui_check_box(380, 20, 12, 12, get_string(32), color_border, BLACK, color_text, 1, 1);
	ui_check_box(380, 35, 12, 12, get_string(33), color_border, BLACK, color_text, 1, 1);
	if((SD_status & (1 << (SD_INSERTED)))) ui_check_box_update(380, 5, 12, 12, GREEN, 1, 2);
	lcd_draw_line_x(0, 479, 51, color_border);
	lcd_draw_line_x(0, 479, 52, color_border);
	lcd_draw_line_x(0, 479, 304, color_border);
	lcd_draw_line_x(0, 479, 303, color_border);

	ui_command(10, 60, 300, 30, get_string(34), BLACK, BLACK, WHITE, 1, 3);
	ui_command(10, 90, 300, 30, get_string(35), BLACK, BLACK, WHITE, 1, 3);
	ui_command(10, 120, 300, 30, get_string(36), BLACK, BLACK, WHITE, 1, 3);
	ui_command(10, 150, 300, 30, get_string(37), BLACK, BLACK, WHITE, 1, 3);
	ui_command(10, 180, 300, 30, get_string(38), BLACK, BLACK, WHITE, 1, 3);
	//ui_command(10, 150, 300, 30, get_string(34), BLACK, BLACK, WHITE, 1, 3);

	gui_status(get_string(50), GREEN);
	lcd_number(mem_check(), 10, GREEN, 1, NULL, NULL);

}

void gui_load()
{
	uint16_t timer1;
	uint16_t timer2;

	console_status &= ~(1 << CONSOLE_ON);
	console_free_buffer();

	lcd_clear_screen(BLACK);
	ui_char(210, 130, 'n', BLUE, 3);
	ui_char(220, 130, 'I', CYAN, 3);
	ui_char(230, 130, 'n', BLUE, 3);
	ui_char(240, 130, 'g', BLUE, 3);
	ui_char(250, 130, 'e', BLUE, 3);
	ui_char(260, 130, 'n', BLUE, 3);
	ui_write(210, 170, get_string(49), WHITE, 1);

	SD_init();
	_delay_ms(1000);

	gui_form_main();

	while(1)
	{
		timer1++; timer2++;
		if(timer2 == 60000)
		{
			uint8_t c = SD_status;
			SD_init();
			if(c != SD_status)
			{
				if(!(SD_status & (1 << (SD_INSERTED)))) ui_check_box_update(380, 5, 12, 12, GREEN, 1, 1);
				if((SD_status & (1 << (SD_INSERTED)))) ui_check_box_update(380, 5, 12, 12, GREEN, 1, 2);
				gui_status(get_string(54), YELLOW);
			}
			timer2 = 0;
		}
		if(touch_sense())
		{
			touch_refresh();
			if(cursor_x > 10 && cursor_x < 300 && cursor_y > 60 && cursor_y < 90)
			{
				ui_command(10, 60, 300, 30, get_string(34), BLACK, BLUE, WHITE, 1, 3);
				touch_wait_release();
				ui_command(10, 60, 300, 30, get_string(34), BLACK, BLACK, WHITE, 1, 3);
				gui_io_control();
				gui_form_main();
			}
			if(cursor_x > 10 && cursor_x < 300 && cursor_y > 90 && cursor_y < 120)
			{
				ui_command(10, 90, 300, 30, get_string(35), BLACK, BLUE, WHITE, 1, 3);
				touch_wait_release();
				ui_command(10, 90, 300, 30, get_string(35), BLACK, BLACK, WHITE, 1, 3);
				gui_pwm_gen();
				gui_form_main();
			}
			if(cursor_x > 10 && cursor_x < 300 && cursor_y > 120 && cursor_y < 150)
			{
				ui_command(10, 120, 300, 30, get_string(36), BLACK, BLUE, WHITE, 1, 3);
				touch_wait_release();
				ui_command(10, 120, 300, 30, get_string(36), BLACK, BLACK, WHITE, 1, 3);
				gui_input_analyzer();
				gui_form_main();
			}
			if(cursor_x > 10 && cursor_x < 300 && cursor_y > 150 && cursor_y < 180)
			{
				ui_command(10, 150, 300, 30, get_string(37), BLACK, BLUE, WHITE, 1, 3);
				touch_wait_release();
				ui_command(10, 150, 300, 30, get_string(37), BLACK, BLACK, WHITE, 1, 3);
				pgm_paint();
				gui_form_main();
			}
			if(cursor_x > 10 && cursor_x < 300 && cursor_y > 180 && cursor_y < 210)
			{
				ui_command(10, 180, 300, 30, get_string(38), BLACK, BLUE, WHITE, 1, 3);
				touch_wait_release();
				ui_command(10, 180, 300, 30, get_string(38), BLACK, BLACK, WHITE, 1, 3);
				gui_form_main();
			}
		}
	}
}

void gui_status(uint8_t * status, uint16_t color)
{
	lcd_draw_window(5, 475, 308, 316, color_back);
	ui_write(5, 308, status, color, 1);
}

void gui_config()
{

}

void gui_io_control_redraw_form()
{
	uint8_t a, b;

	ui_form(0, 0, 479, 319, get_string(34), color_border, color_form, color_text, 0, 2, 3, TitleClose);

	for(b = 0; b < 6; b++)
	{
		ui_write(30 + (b * 78), 40, get_string(21), WHITE, 1);
		ui_char(30 + (b * 78) + 30, 40, 'A' + b, WHITE, 1);
		ui_write(12 + (b * 78), 55, get_string(51), WHITE, 1);
		ui_command(10 + (b * 78), 270, 70, 30, get_string(22), RED, color_cmd, RED, 1, 3);
		for(a = 0; a < 8; a++)
		{
			ui_check_box(10 + (b * 78), 65 + (a * 25), 20, 20, get_string(0), color_border, BLACK, color_text, 1, 1);
			ui_check_box(35 + (b * 78), 65 + (a * 25), 20, 20, get_string(0), color_border, BLACK, color_text, 1, 1);
			ui_check_box(60 + (b * 78), 65 + (a * 25), 20, 20, get_string(0), color_border, BLACK, color_text, 1, 1);
		}
	}
}

void gui_io_control()
{
	uint8_t a, b, c;
	//uint8_t edit_enabled[6];
	uint8_t port_old[6];
	uint8_t ddr_old[6];
	uint8_t pin_old[6];
    uint8_t redraw = 1;
	uint16_t timer1 = 0, timer2 = 0;

	gui_io_control_redraw_form();

	while(1)
	{
		timer1++;
		if(timer1 > 50 || redraw)
		{
			timer1 = 0;
			timer2++;
			for(b = 0; b < 6; b++)
			{
				if(ddr_old[b] != ll_get_ddr(b + 1) || redraw)
				{
					ddr_old[b] = ll_get_ddr(b + 1);
					for(a = 0; a < 8; a++)
					{
						if(((ddr_old[b] >> a) & 0x01))
							ui_check_box_fill(10 + (b * 78), 65 + (a * 25), 20, 20, MAGENTA, 1);
						if(!((ddr_old[b] >> a) & 0x01))
							ui_check_box_fill(10 + (b * 78), 65 + (a * 25), 20, 20, BLACK, 1);
					}
				}
				if(port_old[b] != ll_get_port(b + 1) || redraw)
				{
					port_old[b] = ll_get_port(b + 1);
					for(a = 0; a < 8; a++)
					{
						if(((port_old[b] >> a) & 0x01))
							ui_check_box_fill(35 + (b * 78), 65 + (a * 25), 20, 20, MAGENTA, 1);
						if(!((port_old[b] >> a) & 0x01))
							ui_check_box_fill(35 + (b * 78), 65 + (a * 25), 20, 20, BLACK, 1);
					}
				}
			}
		}
		if(timer2 > 1000 || redraw)
		{
			timer2 = 0;
			for(b = 0; b < 6; b++)
			{
				if(pin_old[b] != ll_get_pin(b + 1) || redraw)
				{
					pin_old[b] = ll_get_pin(b + 1);
					for(a = 0; a < 8; a++)
					{
						if(((pin_old[b] >> a) & 0x01))
							ui_check_box_fill(60 + (b * 78), 65 + (a * 25), 20, 20, GREEN, 1);
						if(!((pin_old[b] >> a) & 0x01))
							ui_check_box_fill(60 + (b * 78), 65 + (a * 25), 20, 20, BLACK, 1);
					}
				}
			}
		}
		redraw = 0;
		a = 0;
		if(touch_sense())
		{
			touch_refresh();
			if(cursor_x > 420 && cursor_y < 50) { gui_free_elements(); return; }
			for(b = 0; b < 6; b++)
			{
				if(cursor_x > (10 + (b * 78)) && cursor_x < (80 + (b * 78)) && cursor_y > 270)
				{
                    a = b + 1;
                    c = b;
				}
			}
		}
		if(a)
        {
            ui_command(10 + (c * 78), 270, 70, 30, get_string(22), RED, color_cmd_selected, RED, 2, 3);
            touch_wait_release();
            ui_command(10 + (c * 78), 270, 70, 30, get_string(22), RED, color_cmd, RED, 1, 3);
            _delay_ms(500);
            gui_port_operation(a);
            touch_wait_release();
            gui_io_control_redraw_form();
            redraw = 1;
            timer1 = 0;
            timer2 = 0;
        }
	}
}

void gui_port_operation_redraw_form(uint8_t port, uint8_t ddr_ena, uint8_t port_ena)
{
    uint8_t a;
    uint8_t *form_caption = get_string(52);
    uint16_t color_ddr = color_border;
    uint16_t color_port = color_border;

    if(ddr_ena) color_ddr = RED;
    if(port_ena) color_port = RED;

    form_caption[10] = 'A' + port - 1;

	ui_form(0, 0, 479, 319, form_caption, color_border, color_form, color_text, 0, 2, 3, TitleClose);

	ui_write(10, 55, get_string(23), WHITE, 3);
    ui_write(10, 105, get_string(24), WHITE, 3);
    ui_write(10, 155, get_string(25), WHITE, 3);

	//ui_command(10 + (b * 78), 270, 70, 30, get_string(22), RED, color_cmd, RED, 1, 3);
	for(a = 0; a < 8; a++)
	{
		ui_check_box(50 + (a * 50), 40, 40, 40, get_string(0), color_ddr, BLACK, color_text, 2, 1);
		ui_check_box(50 + (a * 50), 90, 40, 40, get_string(0), color_port, BLACK, color_text, 2, 1);
		ui_check_box(50 + (a * 50), 140, 40, 40, get_string(0), color_border, BLACK, color_text, 2, 1);
	}

    ui_check_box(10, 240, 30, 30, get_string(53), color_border, BLACK, color_text, 2, 3);
    if(ddr_ena) ui_check_box_fill(10, 240, 30, 30, YELLOW, 2);
    ui_check_box(10, 280, 30, 30, get_string(55), color_border, BLACK, color_text, 2, 3);
    if(port_ena) ui_check_box_fill(10, 280, 30, 30, YELLOW, 2);
}

void gui_port_operation(uint8_t port)
{
	uint8_t a, b, c, d;
	uint8_t ddr_enabled = 0;
    uint8_t port_enabled = 0;
	uint8_t port_old;
	uint8_t ddr_old;
	uint8_t pin_old;
    uint8_t redraw = 1;
	uint16_t timer1 = 0, timer2 = 0;
    uint16_t color_ddr = color_border;
    uint16_t color_port = color_border;
    uint8_t *message = get_string(56);

    gui_port_operation_redraw_form(port, 0, 0);

    while(1)
	{
		timer1++;
		if(timer1 > 50 || redraw)
		{
			timer1 = 0;
			timer2++;
            if(ddr_old != ll_get_ddr(port) || redraw)
            {
                ddr_old = ll_get_ddr(port);
                for(a = 0; a < 8; a++)
                {
                    if(((ddr_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 40, 40, 40, MAGENTA, 2);
                    if(!((ddr_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 40, 40, 40, BLACK, 2);
                }
            }
            if(port_old != ll_get_port(port) || redraw)
            {
                port_old = ll_get_port(port);
                for(a = 0; a < 8; a++)
                {
                    if(((port_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 90, 40, 40, MAGENTA, 2);
                    if(!((port_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 90, 40, 40, BLACK, 2);
                }
            }
		}
		if(timer2 > 1000 || redraw)
		{
			timer2 = 0;
            if(pin_old != ll_get_pin(port) || redraw)
            {
                pin_old = ll_get_pin(port);
                for(a = 0; a < 8; a++)
                {
                    if(((pin_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 140, 40, 40, GREEN, 2);
                    if(!((pin_old >> a) & 0x01))
                        ui_check_box_fill(50 + (a * 50), 140, 40, 40, BLACK, 2);
                }
            }
		}
		redraw = 0;
		a = 0;
		if(touch_sense())
		{
			touch_refresh();
			if(cursor_x > 420 && cursor_y < 50) { gui_free_elements(); return; }
			for(b = 0; b < 3; b++)
			{
                for(c = 0; c < 8; c++)
                {
                    if(cursor_x > (45 + (c * 50)) && cursor_x < (50 + (c * 50) + 45) && cursor_y > (35 + (b * 50)) && cursor_y < (40 + (b * 50) + 45))
                    {
                        a = b + 1;
                        d = c;
                    }
                }
			}
			if(cursor_x > 0 && cursor_x < 80 && cursor_y > 230 && cursor_y < 280) a = 4;
            if(cursor_x > 0 && cursor_x < 80 && cursor_y > 280) a = 5;
		}
        if(a) redraw = 1;
		if(a == 5)
        {
            a = port_enabled;
            if(a) { port_enabled = 0; ui_check_box_fill(10, 280, 30, 30, BLACK, 2); }
            if(!a) { port_enabled = 1; ui_check_box_fill(10, 280, 30, 30, YELLOW, 2); }
            if(ddr_enabled) color_ddr = RED;
            if(port_enabled) color_port = RED;
            if(!ddr_enabled) color_ddr = color_border;
            if(!port_enabled) color_port = color_border;
            for(a = 0; a < 8; a++)
				ui_check_box(50 + (a * 50), 90, 40, 40, get_string(0), color_port, BLACK, color_text, 2, 1);
            a = 0;
        }
		if(a == 4)
        {
            a = ddr_enabled;
            if(a) { ddr_enabled = 0; ui_check_box_fill(10, 240, 30, 30, BLACK, 2); }
            if(!a) { ddr_enabled = 1; ui_check_box_fill(10, 240, 30, 30, YELLOW, 2); }
            if(ddr_enabled) color_ddr = RED;
            if(port_enabled) color_port = RED;
            if(!ddr_enabled) color_ddr = color_border;
            if(!port_enabled) color_port = color_border;
            for(a = 0; a < 8; a++)
				ui_check_box(50 + (a * 50), 40, 40, 40, get_string(0), color_ddr, BLACK, color_text, 2, 1);
            a = 0;
        }
		if(a == 3)
        {
            ui_message_close(get_string(57));
            gui_port_operation_redraw_form(port, ddr_enabled, port_enabled);
            a = 0;
        }
        if(a == 1 && ddr_enabled == 0)
        {
            ui_message_close(get_string(58));
            gui_port_operation_redraw_form(port, ddr_enabled, port_enabled);
            a = 0;
        }
        if(a == 2 && port_enabled == 0)
        {
            ui_message_close(get_string(59));
            gui_port_operation_redraw_form(port, ddr_enabled, port_enabled);
            a = 0;
        }
		if(a)
        {
            touch_wait_release();
            if(a == 1) { message[32] = 'D'; message[33] = 'I'; message[34] = 'R'; }
            if(a == 2) { message[32] = 'O'; message[33] = 'U'; message[34] = 'T'; }
            message[40] = '0' + d;
            if(!ui_message_yes_no(message)) a = 0;
            if(a == 1)
            {
                if(((ddr_old >> d) & 0x01)) BCD(port, d);
                if(!((ddr_old >> d) & 0x01)) BSD(port, d);
            }
            if(a == 2)
            {
                if(((port_old >> d) & 0x01)) BCP(port, d);
                if(!((port_old >> d) & 0x01)) BSP(port, d);
            }
            gui_port_operation_redraw_form(port, ddr_enabled, port_enabled);
            timer1 = 0;
            timer2 = 0;
        }
	}
}

void gui_pwm_gen_redraw_form()
{
	uint8_t a;
	uint8_t* caption;

	ui_form(0, 0, 479, 319, get_string(35), color_border, color_form, color_text, 0, 2, SIZE3, TitleClose);

	if(pwm_run)
	{
		ui_command(280, 250, 80, 50, get_string(27), color_border_disabled, color_cmd_disabled, color_text_disabled, 1, SIZE3);
		ui_command(380, 250, 80, 50, get_string(28), color_border, color_cmd, color_text, 1, SIZE3);
	}
	if(!pwm_run)
	{
		ui_command(280, 250, 80, 50, get_string(27), color_border, color_cmd, color_text, 1, SIZE3);
		ui_command(380, 250, 80, 50, get_string(28), color_border_disabled, color_cmd_disabled, color_text_disabled, 1, SIZE3);
	}

	if(pwm_port != 1) ui_command(20, 40, 90, 30, get_string(70), color_border, color_cmd, color_text, 1, SIZE3);
	if(pwm_port != 2) ui_command(120, 40, 90, 30, get_string(71), color_border, color_cmd, color_text, 1, SIZE3);
	if(pwm_port != 3) ui_command(220, 40, 90, 30, get_string(72), color_border, color_cmd, color_text, 1, SIZE3);

	if(pwm_port == 1) ui_command(20, 40, 90, 30, get_string(70), color_border_selected, color_cmd_selected, color_text_selected, 2, SIZE3);
	if(pwm_port == 2) ui_command(120, 40, 90, 30, get_string(71), color_border_selected, color_cmd_selected, color_text_selected, 2, SIZE3);
	if(pwm_port == 3) ui_command(220, 40, 90, 30, get_string(72), color_border_selected, color_cmd_selected, color_text_selected, 2, SIZE3);

	for(a = 0; a < 8; a++)
	{
		caption = get_string(79);
		caption[1] = '0' + a;
		if(pwm_pin == (a + 1)) ui_command(20 + (a * 40), 80, 30, 30, caption, color_border_selected, color_cmd_selected, color_text_selected, 2, SIZE3);
		if(pwm_pin != (a + 1)) ui_command(20 + (a * 40), 80, 30, 30, caption, color_border, color_cmd, color_text, 1, SIZE3);
	}



	ui_write(20, 130, get_string(61), WHITE, SIZE3);
	ui_write(20, 170, get_string(62), WHITE, SIZE3);

	caption = (uint8_t*) malloc(16);
	if(caption == NULL) { ui_message_close(get_error_msg(0)); return; }

	itoa(pwm_freq, caption, 10);
	ui_text_box(120, 120, 120, 30, caption, color_border, BLACK, color_text, 1, SIZE3);

	caption = (uint8_t*) malloc(16);
	if(caption == NULL) { ui_message_close(get_error_msg(0)); return; }

	itoa(pwm_duty, caption, 10);
	ui_text_box(120, 160, 120, 30, caption, color_border, BLACK, color_text, 1, SIZE3);

	ui_check_box(20, 210, 30, 30, get_string(63), color_border, BLACK, color_text, 2, SIZE1);
	if(pwm_sel_3) ui_check_box_fill(20, 210, 30, 30, GREEN, 2);

	if(pwm_sel_3) { ui_check_box(20, 250, 30, 30, get_string(64), color_border_disabled, BLACK, color_text, 2, SIZE1); pwm_sel_5 = 0; }
	if(!pwm_sel_3) ui_check_box(20, 250, 30, 30, get_string(64), color_border, BLACK, color_text, 2, SIZE1);
	if(pwm_sel_5) ui_check_box_fill(20, 250, 30, 30, GREEN, 2);

	ui_check_box(280, 120, 30, 30, get_string(76), color_border, BLACK, color_text, 2, SIZE3);
	ui_check_box(280, 160, 30, 30, get_string(77), color_border, BLACK, color_text, 2, SIZE3);

	if(pwm_sel_4) ui_check_box_fill(280, 160, 30, 30, GREEN, 2);
	if(!pwm_sel_4) ui_check_box_fill(280, 120, 30, 30, GREEN, 2);

	if(caption) free(caption);
}

void gui_pwm_gen()
{
	uint8_t a, b;
	uint8_t freq2[2];
	uint8_t* caption;

	gui_pwm_gen_redraw_form();

	while(1)
	{
		b = 0;
		if(touch_sense())
		{
			touch_refresh();
			if(cursor_x > 420 && cursor_y < 50) { gui_free_elements(); return; }
			for(a = 0; a < 8; a++)
			{
				if(cursor_x > (20 + (a * 40)) && cursor_x < (50 + (a * 40)) && cursor_y > 80 && cursor_y < 110)
				{
                    b = a + 1;
				}
			}
			if(cursor_x > 280 && cursor_x < (280 + 80) && cursor_y > 250 && cursor_y < (250 + 50)) b = 9; // Start
			if(cursor_x > 380 && cursor_x < (380 + 80) && cursor_y > 250 && cursor_y < (250 + 50)) b = 10; // Stop
			if(cursor_x > 20 && cursor_x < (20 + 90) && cursor_y > 40 && cursor_y < (40 + 30)) b = 11; // Port A
			if(cursor_x > 120 && cursor_x < (120 + 90) && cursor_y > 40 && cursor_y < (40 + 30)) b = 12; // Port B
			if(cursor_x > 220 && cursor_x < (220 + 90) && cursor_y > 40 && cursor_y < (40 + 30)) b = 13; // Port C
			if(cursor_x > 120 && cursor_x < (120 + 120) && cursor_y > 120 && cursor_y < (120 + 30)) b = 14; // Freq
			if(cursor_x > 120 && cursor_x < (120 + 120) && cursor_y > 160 && cursor_y < (160 + 30)) b = 15; // Duty
			if(cursor_x > 20 && cursor_x < (20 + 100) && cursor_y > 210 && cursor_y < (210 + 30)) b = 16; // Disa
			if(cursor_x > 280 && cursor_x < (280 + 80) && cursor_y > 120 && cursor_y < (120 + 30)) b = 17; // Hz
			if(cursor_x > 280 && cursor_x < (280 + 80) && cursor_y > 160 && cursor_y < (160 + 30)) b = 18; // kHz
			if(cursor_x > 20 && cursor_x < (20 + 100) && cursor_y > 250 && cursor_y < (250 + 30)) b = 19; // Keep ON
		}
		if(pwm_run && b != 10) b = 0;
		if(b > 0 && b < 9)
        {
			caption = get_string(79);
			caption[1] = '0' + b - 1;
			ui_command(20 + ((b - 1) * 40), 80, 30, 30, caption, color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
			touch_wait_release();
			if(pwm_pin > 0 && pwm_pin != b)
			{
				caption = get_string(79);
				caption[1] = '0' + pwm_pin - 1;
				ui_command(20 + ((pwm_pin - 1) * 40), 80, 30, 30, caption, color_border, color_cmd, color_text, 1, SIZE3);
				pwm_pin = b;
				b = 0;
			}
			if(!pwm_pin)
			{
				pwm_pin = b;
				b = 0;
			}
			if(pwm_pin > 0 && pwm_pin == b)
			{
				caption = get_string(79);
				caption[1] = '0' + pwm_pin - 1;
				ui_command(20 + ((pwm_pin - 1) * 40), 80, 30, 30, caption, color_border, color_cmd, color_text, 1, SIZE3);
				pwm_pin = 0;
				b = 0;
			}
			b = 0;
        }
        a = pwm_run;
        if(b == 9 && a == 0) // START
		{
			if(!pwm_port || !pwm_pin || !pwm_duty || !pwm_freq )
			{
				ui_message_close(get_string(66));
				gui_pwm_gen_redraw_form();
			}
			if(pwm_port && pwm_pin && pwm_duty && pwm_freq )
			{
				ui_command(280, 250, 80, 50, get_string(27), color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
				touch_wait_release();
				ui_command(380, 250, 80, 50, get_string(28), color_border, color_cmd, color_text, 1, SIZE3);
				ui_command(280, 250, 80, 50, get_string(27), color_border_disabled, color_cmd_disabled, color_text_disabled, 1, SIZE3);
				if(pgm_pwm(pwm_port, pwm_pin, pwm_freq, pwm_duty, pwm_sel_3, pwm_sel_4) == 4) gui_pwm_gen_redraw_form();
				pwm_run = 1;
				b = 0;
			}
		}
		if(b == 10 && a == 1) // STOP
		{
			pwm_run = 0;
			ui_command(380, 250, 80, 50, get_string(28), color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
			touch_wait_release();
			ui_command(280, 250, 80, 50, get_string(27), color_border, color_cmd, color_text, 1, SIZE3);
			ui_command(380, 250, 80, 50, get_string(28), color_border_disabled, color_cmd_disabled, color_text_disabled, 1, SIZE3);
			if(pgm_pwm(pwm_port, pwm_pin, 0, 0, pwm_sel_3, pwm_sel_4) == 4) gui_pwm_gen_redraw_form();
			pwm_run = 0;
			b = 0;
		}
		a = pwm_port;
		if(b == 11) // Port A
		{
			ui_command(20, 40, 90, 30, get_string(70), color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
			touch_wait_release();
			if(a == 1)
			{
				ui_command(20, 40, 90, 30, get_string(70), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 0;
			}
			if(a != 1)
			{
				ui_command(120, 40, 90, 30, get_string(71), color_border, color_cmd, color_text, 1, SIZE3);
				ui_command(220, 40, 90, 30, get_string(72), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 1;
			}
		}
		if(b == 12) // Port B
		{
			ui_command(120, 40, 90, 30, get_string(71), color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
			touch_wait_release();
			if(a == 2)
			{
				ui_command(120, 40, 90, 30, get_string(71), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 0;
			}
			if(a != 2)
			{
				ui_command(20, 40, 90, 30, get_string(70), color_border, color_cmd, color_text, 1, SIZE3);
				ui_command(220, 40, 90, 30, get_string(72), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 2;
			}
		}
		if(b == 13) // Port C
		{
			ui_command(220, 40, 90, 30, get_string(72), color_border_selected, color_cmd_selected, color_text_selected, 1, SIZE3);
			touch_wait_release();
			if(a == 3)
			{
				ui_command(220, 40, 90, 30, get_string(72), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 0;
			}
			if(a != 3)
			{
				ui_command(20, 40, 90, 30, get_string(70), color_border, color_cmd, color_text, 1, SIZE3);
				ui_command(120, 40, 90, 30, get_string(71), color_border, color_cmd, color_text, 1, SIZE3);
				pwm_port = 3;
			}
		}
		if(b == 14)
		{
			freq2[0] = (uint8_t)(pwm_freq >> 8); freq2[1] = (uint8_t)pwm_freq;
			touch_wait_release();
			ui_numpad(get_string(67), freq2, NULL, DEC16);
			pwm_freq = (freq2[0] << 8) | freq2[1];
			gui_pwm_gen_redraw_form();
		}
		if(b == 15)
		{
			touch_wait_release();
			ui_numpad(get_string(68), &pwm_duty, NULL, DEC8);
			if(pwm_duty > 100)
			{
				ui_message_close(get_string(69));
				pwm_duty = 100;
			}
			gui_pwm_gen_redraw_form();
		}
		if(b == 19 && pwm_sel_3 != 1)
		{
			a = pwm_sel_5;
			if(a) { ui_check_box_fill(20, 250, 30, 30, BLACK, 2); pwm_sel_5 = 0; pwm_sel_3 = 2; }
			if(!a) { ui_check_box_fill(20, 250, 30, 30, GREEN, 2); pwm_sel_5 = 1; pwm_sel_3 = 0; }
		}
		if(b == 16)
		{
			a = pwm_sel_3;
			if(a == 0)
			{
				ui_check_box_fill(20, 210, 30, 30, GREEN, 2);
				ui_check_box_update(20, 250, 30, 30, BLACK, 2, 3);
				pwm_sel_3 = 1;
				pwm_sel_5 = 0;
			}
			if(a == 1)
			{
				ui_check_box_fill(20, 210, 30, 30, BLACK, 2);
				ui_check_box_update(20, 250, 30, 30, BLACK, 2, 1);
				pwm_sel_3 = 0;
				pwm_sel_5 = 0;
			}
			if(a == 2)
			{
				ui_check_box_fill(20, 210, 30, 30, GREEN, 2);
				ui_check_box_update(20, 250, 30, 30, BLACK, 2, 3);
				pwm_sel_3 = 1;
				pwm_sel_5 = 0;
			}
		}
		if(b == 17 && pwm_sel_4 != 0) // Hz
		{
			ui_check_box_fill(280, 160, 30, 30, BLACK, 2);
			ui_check_box_fill(280, 120, 30, 30, GREEN, 2);
			pwm_sel_4 = 0;
		}
		if(b == 18 && pwm_sel_4 != 1) // kHz
		{
			ui_check_box_fill(280, 120, 30, 30, BLACK, 2);
			ui_check_box_fill(280, 160, 30, 30, GREEN, 2);
			pwm_sel_4 = 1;
		}
		touch_wait_release();
	}
}

void gui_input_analyzer_form_redraw(value_list *list1)
{
	//if(gui_allocate_elements(10)) { ui_message_close("Error allocating buffer\nNot enaugh memory"); return; }
	ui_value_list_clear(list1);

	ui_form(0, 0, 479, 319, get_string(80), color_border, color_form, color_text, 0, 2, SIZE3, TitleClose);

	list1 = ui_value_list(20, 60, 200, 220, get_string(96), color_border, color_back, color_text, color_cmd_selected, color_text_selected, 2, SIZE3);
}

void gui_input_analyzer()
{
	value_list *list1;

	gui_input_analyzer_form_redraw(list1);

	while(1)
	{
		if(touch_sense())
		{
			touch_refresh();
			if(cursor_x > 420 && cursor_y < 50) { ui_value_list_clear(list1); gui_free_elements(); return; }
			touch_wait_release();
		}
	}
}

void console_allocate_buffer()
{
	uint8_t result = 0, result_b = 0;
	uint16_t a = 0;
	uint8_t data[4];

	    // Attempt to read console keypad size; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 5, data)) {
        console_buffer_size = 1024;
		result_b = 1;
    } else {
        console_buffer_size = (data[0] << 8) | data[1];
    }

    // Attempt to read console back color; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 6, data)) {
        console_input_buffer_size = 128;
		result_b |= 2;
    } else {
        console_input_buffer_size = data[0];
    }

    if (console_buffer) {
        // Free the existing buffer if it was allocated
        free(console_buffer);
        console_buffer = NULL;
    }

    if (console_input_buffer) {
        // Free the existing buffer if it was allocated
        free(console_input_buffer);
        console_input_buffer = NULL;
    }

    if (console_input_buffer_old) {
        // Free the existing buffer if it was allocated
        free(console_input_buffer_old);
        console_input_buffer_old = NULL;
    }

    if (console_buffer_size < 256) console_buffer_size = 256;
	if (console_input_buffer_size < 64) console_buffer_size = 64;

	console_buffer = (uint8_t *)malloc(console_buffer_size);
	if (console_buffer == NULL) {
		// Handle memory allocation failure
		// You can print an error message or take appropriate action
		console_buffer = (uint8_t *)malloc(256);
		for(a = 0; a < 256; a++) console_buffer[a]  = '\0';
		console_write(get_string(81));
		console_buffer_size = 256;
		result = 1;
	}

	console_input_buffer = (uint8_t *)malloc(console_input_buffer_size);
	if (console_input_buffer == NULL) {
		// Handle memory allocation failure
		// You can print an error message or take appropriate action
		console_input_buffer = (uint8_t *)malloc(32);
		for(a = 0; a < 32; a++) console_input_buffer[a]  = '\0';
		console_write(get_string(82));
		console_input_buffer_size = 32;
		result = 1;
	}

	console_input_buffer_old = (uint8_t *)malloc(console_input_buffer_size);
	if (console_input_buffer_old == NULL) {
		// Handle memory allocation failure
		// You can print an error message or take appropriate action
		console_input_buffer_old = (uint8_t *)malloc(32);
		for(a = 0; a < 32; a++) console_input_buffer_old[a]  = '\0';
		console_write(get_string(82));
		result = 1;
	}

	if(!result)
	{
		for(a = 0; a < console_buffer_size; a++) console_buffer[a]  = '\0';
		for(a = 0; a < console_input_buffer_size; a++) console_input_buffer[a]  = '\0';
		for(a = 0; a < console_input_buffer_size; a++) console_input_buffer_old[a]  = '\0';
		index_b = 0;
		if(result_b) console_write(get_string(101));
		if(result_b | 0x01) console_write(get_string(102));
		if(result_b | 0x02) console_write(get_string(103));
		console_write(get_string(104));
		console_number(console_buffer_size + (console_input_buffer_size * 2), 10, 0);
		console_write(get_string(105));
	}
}

void console_free_buffer()
{
    if (console_buffer) {
        // Free the existing buffer if it was allocated
        free(console_buffer);
        console_buffer = NULL;
    }

    if (console_input_buffer) {
        // Free the existing buffer if it was allocated
        free(console_input_buffer);
        console_input_buffer = NULL;
    }

    if (console_input_buffer_old) {
        // Free the existing buffer if it was allocated
        free(console_input_buffer_old);
        console_input_buffer_old = NULL;
    }
}


void console_BIN(uint8_t bin, uint8_t toggle_0b, uint8_t toggle_nl)
{
	uint8_t buffer[12];
	uint8_t a = 0, b = 0;
	if(toggle_0b) { buffer[0] = '0'; buffer[1] = 'b'; b = 2; }
	for(a = 0; a < 8; a++)
	{
		if((bin >> (7 - a)) & 0x01) buffer[a + b] = '1';
		if(!((bin >> (7 - a)) & 0x01)) buffer[a + b] = '0';
	}
	a = 8 + b;
	if(toggle_nl) buffer[a++] = '\n';
	buffer[a] = '\0';
	console_write(buffer);
}

void console_HEX8(uint8_t hex8, uint8_t toggle_0x, uint8_t toggle_nl)
{
	uint8_t buffer[8];
	uint8_t a = 0;
	if(toggle_0x) { buffer[0] = '0'; buffer[1] = 'x'; a = 2; }
	buffer[a] = (hex8 >> 4) & 0x0f;
	if(buffer[a] < 10) buffer[a] += '0';
	if(buffer[a] > 9 && buffer[a] < 16) buffer[a] += 'A' - 10;
	a++;
	buffer[a] = hex8 & 0x0f;
	if(buffer[a] < 10) buffer[a] += '0';
	if(buffer[a] > 9 && buffer[a] < 16) buffer[a] += 'A' - 10;
	a++;
	if(toggle_nl) buffer[a++] = '\n';
	buffer[a] = '\0';
	console_write(buffer);
}

void console_number(uint32_t number, uint8_t base, uint8_t toggle_nl)
{
    uint8_t a[16];
    itoa(number, a, base);

    if (toggle_nl > 0) {
        uint8_t len = strlen(a);
        if (len < 15) {
            a[len] = '\n';
            a[len + 1] = '\0'; // Null-terminate the string again
        }
    }

    console_write(a);
}

void console_write(uint8_t *text)
{
	if(!(console_status & (1 << CONSOLE_ON))) return;
    uint16_t text_length = strlen((const char *)text);

    // Check if the buffer has enough space to store the text
    if (index_b + text_length >= console_buffer_size) {
        // Calculate the remaining space in the buffer
        uint16_t remaining_space = console_buffer_size - index_b;

        // Find the first occurrence of '\0' or '\n' in the buffer
        uint16_t first_line_end = 0;
        while (first_line_end < index_b) {
            if (console_buffer[first_line_end] == '\0' || console_buffer[first_line_end] == '\n') {
                break;
            }
            first_line_end++;
        }

        // Calculate the length of the first line to remove
        uint16_t line_to_remove_length = first_line_end + 1;

        // Move the remaining content to the beginning of the buffer
        memmove(console_buffer, console_buffer + line_to_remove_length, index_b - line_to_remove_length);

        // Update index_b after removing the first line
        index_b -= line_to_remove_length;

        // Recalculate the remaining space in the buffer
        remaining_space = console_buffer_size - index_b;

        // Check if there's still not enough space after removing lines
        if (index_b + text_length >= console_buffer_size) {
            // Handle the case where the text is larger than the buffer
            // You can choose to truncate or ignore the text if it's too large
            // Here, we are truncating it.
            text_length = remaining_space;
        }
    }

    // Append the text to the buffer
    memcpy(console_buffer + index_b, text, text_length);
    index_b += text_length;

    // Null-terminate the buffer (optional)
    console_buffer[index_b] = '\0';
    free_string(text);
}

void console_update(uint8_t start_line, uint8_t redraw_keypad)
{
	if(!(console_status & (1 << CONSOLE_ON))) return;

	uint16_t color_old = console_text_color;
	uint8_t char_size_x;
	uint8_t char_size_y;
	uint8_t start_x;
	uint8_t start_y;
	uint16_t end_x;
	uint16_t end_y;
	//uint8_t char_size[2];

	lcd_get_char_size('a', console_text_type, &char_size_x, &char_size_y);
	//char_size_x = char_size[0]; char_size_y = char_size[1];

	if(console_status & (1 << CONSOLE_WINDOWED) && console_status & (1 << CONSOLE_KEYPAD_ON))
	{
		start_x = 5;
		start_y = 31;
		end_x = 474;
		end_y = 319 - (console_keypad_size * 4) - 6;
		console_max_lines = (end_y - start_y) / char_size_y;
		console_max_cols = (end_x - start_x) / char_size_x;
		if(redraw_keypad) ui_get_key(console_keypad_size, 0, 1);
		index_x = 5;
		index_y = (console_max_lines - 1) * char_size_y;
	}
	if(!(console_status & (1 << CONSOLE_WINDOWED)) && console_status & (1 << CONSOLE_KEYPAD_ON))
	{
		start_x = 0;
		start_y = 0;
		end_x = 479;
		end_y = 319 - (console_keypad_size * 4);
		console_max_lines = (end_y - start_y) / char_size_y;
		console_max_cols = (end_x - start_x) / char_size_x;
		if(redraw_keypad) ui_get_key(console_keypad_size, 0, 1);
		index_x = 0;
		index_y = (console_max_lines - 1) * char_size_y;
	}
	if(console_status & (1 << CONSOLE_WINDOWED) && !(console_status & (1 << CONSOLE_KEYPAD_ON)))
	{
		start_x = 5;
		start_y = 31;
		end_x = 474;
		end_y = 319 - 5;
		console_max_lines = (end_y - start_y) / char_size_y;
		console_max_cols = (end_x - start_x) / char_size_x;
	}
	if(!(console_status & (1 << CONSOLE_WINDOWED)) && !(console_status & (1 << CONSOLE_KEYPAD_ON)))
	{
		start_x = 0;
		start_y = 0;
		end_x = 479;
		end_y = 319;
		console_max_lines = (end_y - start_y) / char_size_y;
		console_max_cols = (end_x - start_x) / char_size_x;
	}

	if(start_line == 0 && console_max_lines > 0) console_max_lines--;

	lcd_draw_window(start_x, end_x, start_y, end_y, console_back_color);

	uint8_t a = 0, d = 0;
	uint16_t start_point = index_b;
	uint8_t display_lines = 0;
	uint8_t display_cols = 0;

	if(index_b == 0) return;

	while(!a)
	{
		if(console_buffer[start_point] == '\n') { display_lines++; display_cols = 0; }
		if(display_cols >= console_max_cols) { display_lines++; display_cols = 0; }
		if(display_lines > console_max_lines + start_line) { start_point++; a = 1; break; }
		start_point--; display_cols++;
		if(start_point == 0 && console_max_lines > display_lines) { console_max_lines = display_lines; a = 1; break; }
		if(start_point == 0 && console_max_lines <= display_lines) { a = 1; break; }
	}

	uint8_t col, line;

	for(line = 0; line < console_max_lines; line++)
	{
		lcd_set_position(start_x, start_y + (line * char_size_y));
		d = 0; col = 0;
		while(d != '\n' && col < console_max_cols)
		{
			d = console_buffer[start_point];
			//if(d == '\0') d = '\n';
		    if(d != '`' && d >= 0x20 && d < 0x7f) { lcd_draw_char(d, console_text_color, console_text_type); col++; }
		    if(d == '`') // Console codes
			{
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'R') console_text_color = RED;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'G') console_text_color = GREEN;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'B') console_text_color = BLUE;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'M') console_text_color = MAGENTA;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'Y') console_text_color = YELLOW;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'C') console_text_color = CYAN;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'W') console_text_color = WHITE;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'B') console_text_color = BLACK;
				if(console_buffer[start_point + 1] == 'C' && console_buffer[start_point + 2] == 'N') console_text_color = color_old;
				start_point += 2;
			}
			start_point++;
			if(start_point >= index_b) return;
		}
	}
}

void console_update_input()
{
    if (!(console_status & (1 << CONSOLE_ON))) return;

    uint8_t a = 0;
    //uint8_t char_size[2];
    uint8_t char_size_x;
    uint8_t char_size_y;

    lcd_get_char_size('a', console_text_type, &char_size_x, &char_size_y);
    //char_size_x = char_size[0];
    //char_size_y = char_size[1];

    // Calculate the starting character position for displaying text.
    uint8_t start_char = 0;

    if (console_input_index > console_max_cols) {
        start_char = console_input_index - console_max_cols;
		console_status |= (1 << CONSOLE_INPUT_OVERFLOW);
    }

    if(console_input_index <= console_max_cols && (console_status |= (1 << CONSOLE_INPUT_OVERFLOW)))
	{
		for(a = 0; a < console_input_buffer_size; a++) console_input_buffer_old[a] = 0xff;
		console_status &= ~(1 << CONSOLE_INPUT_OVERFLOW);
	}

	a = 0;

    // Modify the characters that are different within the displayable area.
    while (console_input_buffer[start_char] != '\0' && a < console_max_cols)
    {
        if (console_input_buffer_old[a] != console_input_buffer[start_char])
        {
            uint16_t char_x = index_x + (a * char_size_x);
            uint16_t char_y = index_y;

            lcd_draw_window(char_x, char_x + char_size_x - 1, char_y, char_y + char_size_y, console_back_color);

            lcd_set_position(char_x, char_y);

            lcd_draw_char(console_input_buffer[start_char], console_text_color, console_text_type);
        }
        a++;
        start_char++;
    }

    // Check if caption_old is longer than caption and remove the extra characters within the displayable area.

    while (console_input_buffer_old[start_char] != '\0' && a < console_max_cols)
    {
        uint16_t char_x = index_x + (a * char_size_x);
        uint16_t char_y = index_y;

        lcd_draw_window(char_x, char_x + char_size_x - 1, char_y, char_y + char_size_y, console_back_color);

        // Update the previous character.
        console_input_buffer_old[start_char] = '\0';
        a++;
        start_char++;
    }
}

void console(uint8_t windowed, uint8_t keypad_on)
{
	uint16_t timer1;
	uint16_t timer2;
	uint8_t buffer[4];
	uint8_t prompt_size = 0;

	if(windowed) console_status |= (1 << CONSOLE_WINDOWED);
	if(!windowed) console_status &= ~(1 << CONSOLE_WINDOWED);
	if(keypad_on) console_status |= (1 << CONSOLE_KEYPAD_ON);
	if(!keypad_on) console_status &= ~(1 << CONSOLE_KEYPAD_ON);

	if(!(console_status & (1  << CONSOLE_ON)))
	{
		console_status |= (1  << CONSOLE_ON);
		console_allocate_buffer();
	}

	uint8_t show_index = 0;
	uint8_t char_size_x, char_size_y;

	lcd_get_char_size('a', console_text_type, &char_size_x, &char_size_y);
	//char_size_x = buffer[0]; char_size_y = buffer[1];

	console_update(0, 1);

	if(!(console_status & (1 << CONSOLE_KEYPAD_ON))) return;

	uint8_t pos_x = 0;
	if((console_status & (1 << CONSOLE_WINDOWED))) pos_x = 3;

	prompt_size = console_make_prompt(0, 0);
	console_input_index = prompt_size;
	console_update_input();

	while(1)
	{
		timer1++; timer2++;
		uint8_t a = ui_get_key(console_keypad_size, 0, 0);
		if((show_index > 0) && (a < 0xf0)) {
			show_index = 0;
			console_update(0, 0);
			for(uint8_t c = 0; c < console_input_buffer_size; c++) console_input_buffer_old[a] = 0xff;
			console_update_input();
		}
		else if(a == '\n') // Enter
		{
			console_input_buffer[console_input_index] = '\n';
			console_input_buffer[console_input_index + 1] = '\0';
			console_write(console_input_buffer);
			if(macro_process(console_input_buffer) == 0xfe)
			{
				// Console exit sequence
				console_free_buffer();
				console_status &= ~(1  << CONSOLE_ON);
				return ;
			}
			console_update(0, 0);
			//for(uint8_t c = prompt_size; c < console_input_buffer_size; c++) console_input_buffer[c] = '\0';
			prompt_size = console_make_prompt(0, 0);
			console_input_index = prompt_size;
			console_update_input();
		}
		else if(a == 0x08) // Backspace
		{
			if(console_input_index > prompt_size)
			{
				for(uint8_t c = 0; c < console_input_buffer_size; c++) console_input_buffer_old[c] = console_input_buffer[c];
				console_input_index--;
				console_input_buffer[console_input_index] = '\0';
				console_update_input();
			}
		}
		else if(a == 0x1b) // Escape
		{
			for(uint8_t c = prompt_size; c < console_input_buffer_size; c++) console_input_buffer[c] = '\0';
			console_input_index = prompt_size;
			console_update_input();
		}
		else if((a >= 0x20) && (a < 0x7f))
		{
			for(uint8_t c = 0; c < console_input_buffer_size; c++) console_input_buffer_old[c] = console_input_buffer[c];
			console_input_buffer[console_input_index++] = a;
			console_input_buffer[console_input_index] = '\0';
			if(console_input_index > (console_input_buffer_size - 3)) console_input_index = (console_input_buffer_size - 3);
			console_update_input();
		}
		else if(a == 0xf1) // UP
		{
			show_index++;
			console_update(show_index, 0);
		}
		else if(a == 0xf3) // DN
		{
			if(show_index > 0) { show_index--; console_update(show_index, 0); }
		}
		else if(a == 0xf2) // LEFT
		{
			for(uint8_t c = 0; c < console_input_buffer_size; c++)
			{
				console_input_buffer[c] = console_input_buffer_old[c];
				console_input_buffer_old[c] = 0xff;
			}
			console_update_input();
		}
		if(timer1 == 3000)
		{
			if(show_index == 0)
			{
				console_input_buffer[console_input_index] = '_';
				console_input_buffer[console_input_index + 1] = '\0';
				console_update_input();
			}
		}
		if(timer1 == 6000)
		{
			if(show_index == 0)
			{
				console_input_buffer[console_input_index] = '\0';
				console_update_input();
				timer1 = 0;
			}
		}
		if(timer2 == 10000)
		{
			uint8_t c = SD_status;
			SD_init();
			if(show_index == 0 && c != SD_status)
			{
				prompt_size = console_make_prompt(0, 0);
				console_input_index = prompt_size;
				console_update(0, 0);
			}
			timer2 = 0;
		}
	}
	console_free_buffer();
	console_status &= ~(1  << CONSOLE_ON);
}

void load_gui_settings()
{
    uint8_t data[6];

    // Attempt to read console keypad size; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 1, data)) {
        console_keypad_size = 30;
		console_write(get_string(106));
        console_write(get_string(107));
    } else {
        console_keypad_size = data[0];
    }

    // Attempt to read console back color; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 3, data)) {
        console_back_color = color_back;
		console_write(get_string(106));
        console_write(get_string(108));
    } else {
        console_back_color = (data[0] << 8) | data[1];
    }

    // Attempt to read console text color; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 2, data)) {
        console_text_color = color_text;
		console_write(get_string(106));
        console_write(get_string(109));
    } else {
        console_text_color = (data[0] << 8) | data[1];
    }

    // Attempt to read console text type; use default if error
    if (!read_setting(CONSOLE_SETTINGS_START_ADD, 4, data)) {
        console_text_type = 1;
		console_write(get_string(106));
        console_write(get_string(110));
    } else {
        console_text_type = data[0];
    }

    // Attempt to read console text color; use default if error
    if (!read_setting(TOUCH_SETTINGS_START_ADD, 1, data)) {
		cursor_calibration_X = -24;
		console_write(get_string(106));
        console_write(get_string(111));
    } else {
        cursor_calibration_X = (data[0] << 8) | data[1];
    }

    // Attempt to read console text type; use default if error
    if (!read_setting(TOUCH_SETTINGS_START_ADD, 2, data)) {
        cursor_calibration_Y = -4;
		console_write(get_string(106));
        console_write(get_string(112));
    } else {
        cursor_calibration_Y = (data[0] << 8) | data[1];
    }
}

uint8_t console_make_prompt(uint8_t flag, uint8_t *dirname)
{
	uint8_t a = 0, b = 0;
	console_input_buffer[0] = 'a';
	console_input_buffer[1] = 's';
	console_input_buffer[2] = 'h';
	if(!(SD_status | (1 << SD_INSERTED)))
	{
		console_input_buffer[3] = console_root;
		console_input_buffer[4] = ' ';
		console_input_buffer[5] = '\0';
		return 5;
	}
	console_input_buffer[3] = '@';
	console_input_buffer[4] = '/';
	if(flag == 1) // Reset prompt
	{
		console_input_buffer[5] = console_root;
		console_input_buffer[6] = ' ';
		console_input_buffer[7] = '\0';
		return 7;
	}
	while(console_input_buffer[a] != console_root && console_input_buffer[a] != '\0') a++;
	if(a < 7 && !flag)
	{
		console_input_buffer[3] = '@';
		console_input_buffer[4] = '/';
		console_input_buffer[5] = console_root;
		console_input_buffer[6] = ' ';
		console_input_buffer[7] = '\0';
		return 7;
	}
	if(flag == 3) // Append directory
	{
		while(dirname[b] != '\0') console_input_buffer[a++] = dirname[b++];
		console_input_buffer[a++] = '/';
		console_input_buffer[a++] = console_root;
		console_input_buffer[a++] = ' ';
		console_input_buffer[a] = '\0';
		return a;
	}
	if(flag == 2 && a > 7) // Remove last directory
	{
		a -= 3;
		while(console_input_buffer[a] != '/' ) a--;
		console_input_buffer[a++] = '/';
		console_input_buffer[a++] = console_root;
		console_input_buffer[a++] = ' ';
		console_input_buffer[a] = '\0';
		return a;
	}
	return a + 2;
}

uint8_t macro_process(uint8_t *macro_data)
{
    uint8_t param_count = 0;
    uint8_t buffer[16];

	uint8_t ins[32];
	uint8_t short_parameters[7][16];
	uint8_t filename[64];
	uint8_t *ptr_short_parameters[7];

    uint8_t a = 0, b = 0, c = 0, d = 0xff, e = 0;

	for(a = 0; a < 7; a++) ptr_short_parameters[a] = &short_parameters[a][0];
	a = 0;

	for(uint8_t f = 0; f < 7; f++)
		for(uint8_t g = 0; g < 16; g++)
		{
			short_parameters[f][g] = '\0';
			buffer[g] = '\0';
		}

	for(uint8_t f = 0; f < 64; f++) filename[f] = '\0';

	while(d != '\0')
	{
		d = macro_data[a++];
		if(d == console_root && b == 0) b = 1;
		if(((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z')) && b == 1)
			{ ins[c++] = d; param_count = 1; }
		if(d == ' ' && c > 0 && b == 1)
			{
				ins[c] = '\n';
				ins[c + 1] = '\0';
				c = 0;
				b = 16;
			}
		if(((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <= '9') || (d == '.') || (d == '/')) && b == 16)
			{ filename[c++] = d; param_count = 1; }
		if(b == 16 && (d == '\n' || d == '\0'))
			{ filename[c] = '\n'; filename[c + 1] = '\0'; b = 17; }
		if(d == '-' && macro_data[a] != 'f')
			{
				if(b == 3) { buffer[c] = '\n'; buffer[c + 1] = '\0'; memcpy(short_parameters[e], buffer, 16);
					for(uint8_t f = 0; f < 16; f++) buffer[f] = 0; }
				if(b == 2) {filename[c] = '\n'; filename[c + 1] = '\0'; }
				if(b == 1) {ins[c] = '\n'; ins[c + 1] = '\0'; }
				b = 3; param_count++; c = 0;
				buffer[c++] = d;
				buffer[c++] = macro_data[a];
				e++; a += 2;
			}
		if(((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <='9') || d == '.' || d == ' ' || d == '?') && b == 3)
			buffer[c++] = d;
		if(d == '-' && macro_data[a] == 'f')
			{
				if(b == 3) { buffer[c] = '\n'; buffer[c + 1] = '\0'; memcpy(short_parameters[e], buffer, 16);
					for(uint8_t f = 0; f < 16; f++) buffer[f] = 0; }
				if(b == 1) {ins[c] = '\n'; ins[c + 1] = '\0'; }
				b = 2; param_count++; c = 0; a += 2;
			}
		if(((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <='9') || d == '/' || d == '.' || d == ' ' || d == '?') && b == 2)
			filename[c++] = d;
		if((d == '\n' || d == '\0') && b == 3)
			{ buffer[c] = '\n'; buffer[c + 1] = '\0'; memcpy(short_parameters[e], buffer, 16); for(uint8_t f = 0; f < 16; f++) buffer[f] = 0; }
		if((d == '\n' || d == '\0') && b == 2)
			{ filename[c] = '\n'; filename[c + 1] = '\0'; }
		if((d == '\n' || d == '\0') && b == 1)
			{ ins[c] = '\n'; ins[c + 1] = '\0'; }
		if(param_count > 9 || a >= 80) return param_count;
		if(d == '\n') d = '\0';
	}

	if(compare_command(ins, get_string(140))) { gui_load(); console_update(0, 1); }
	if(compare_command(ins, get_string(141))) { pgm_paint(); console_update(0, 1); }
	if(compare_command(ins, get_string(142))) pgm_less(ptr_short_parameters);
	if(compare_command(ins, get_string(143))) pgm_conf(ptr_short_parameters, filename);
	if(compare_command(ins, get_string(144))) pgm_store(ptr_short_parameters);
	//if(compare_command(ins, get_string(145))) { lcd_gamma_correction(); console_update(0, 1); }
	if(compare_command(ins, get_string(146))) pgm_status();
	if(compare_command(ins, get_string(147))) pgm_cls();
	//if(compare_command(ins, get_string(148))) FAT_try();
	if(compare_command(ins, get_string(149))) console_root = '#';
	if(compare_command(ins, get_string(150))) { if(console_root == '#') { console_root = '$'; } else if (console_root == '$') { param_count = 0xfe; } }
	if(compare_command(ins, get_string(151))) pgm_ls();
	if(compare_command(ins, get_string(152))) pgm_cd(filename);
	if(compare_command(ins, get_string(153))) { console_write(get_string(199)); console_update(0,0); }
	if(compare_command(ins, get_string(154))) pgm_read_file(filename, ptr_short_parameters);
	if(compare_command(ins, get_string(155))) pgm_bitmap(filename, ptr_short_parameters);

    return param_count;
}

uint8_t compare_strings(uint8_t * str1, uint8_t * str2)
{
	uint8_t a = 0, b = 0;
	while(b == 0)
	{
		if(str1[a] == '\n' || str1[a] == '\0' || str2[a] == '\n' || str2[a] == '\0') b = 1;
		if(!b && str1[a] != str2[a]) return 0;
		a++;
	}
	return 1;
}

uint8_t compare_command(uint8_t * str1, uint8_t * str2)
{
	uint8_t a = 0, b = 0;
	while(b == 0)
	{
		if(str2[a] == '\0') b = 1;
		if(!b && str1[a] != str2[a]) return 0;
		a++;
	}
	return 1;
}

uint8_t bin_to_int(uint8_t *binaryString)
{
	    // Skip the get_string(78) prefix if present
    if (strncmp(binaryString, get_string(78), 2) == 0) {
        binaryString += 2;
    }

    uint8_t result = 0;
    while (*binaryString) {
        result <<= 1;
        if (*binaryString == '1') {
            result |= 1;
        } else if (*binaryString != '0') {
            // Handle invalid characters if necessary
            // For simplicity, we'll skip them in this example
        }
        binaryString++;
    }

    return result;
}

void pgm_cls()
{
	for(uint16_t a = 0; a < console_buffer_size; a++) console_buffer[a] = 0;
	index_b = 0;
}

void pgm_status()
{
	console_write(get_string(113));
	console_number(console_buffer_size, 10, 1);
	console_write(get_string(114));
	console_number(console_input_buffer_size, 10, 1);
	console_write(get_string(115));
	console_number(console_max_lines, 10, 1);
	console_write(get_string(116));
	console_number(console_max_cols, 10, 1);
	console_write(get_string(117));
	console_number(index_b, 10, 1);
}

void pgm_store(uint8_t **arguments)
{
	uint8_t a = 0, b = 0;
	uint16_t address = 0;
	uint8_t *ptr = (uint8_t *)address;
	uint8_t data = 0xff;

	if(console_root != '#') { console_write(get_string(118)); return; }

	for(uint8_t f = 0; f < 7; f++)
	{
		if(arguments[f][0] == '-' && arguments[f][1] == 'a')
		{
			a = 1;
			address = (uint16_t)strtol(&arguments[f][2], NULL, 16);
			console_write(get_string(119));
			console_write(get_string(120));
			if(address < 0x1000) console_write(get_string(121));
			if(address >= 0x1000 || address < 0x2000) console_write(get_string(122));
			if(address >= 0x2000) console_write(get_string(123));
			console_HEX8((uint8_t)(address >> 8), 1, 0);
			console_HEX8((uint8_t)address, 0, 1);
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'v')
		{
			b = 1;
			data = (uint8_t)strtol(&arguments[f][2], NULL, 16);
			console_write(get_string(124));
			console_HEX8(data, 1, 0);
			console_write(get_string(0));
			console_number(data, 10, 1);
		}
	}

	if(!a) { console_write(get_string(125)); return; }
	if(!b) { console_write(get_string(126)); return; }

	if(address >= 0x1000 && address < 0x1800) { EEPROM_WriteByte(address - 0x1000, data); }
	else { memcpy(ptr, &data, 1); }

	console_write(get_string(127));
}

void pgm_conf(uint8_t **arguments, uint8_t * filename)
{
	uint8_t a = 0, v = 0, c = 0, m = 0, n = 0, t = 0;;
	uint8_t setting = 0;
	uint16_t address = 0;
	uint8_t a_1 = 0; uint16_t a_2 = 0; uint32_t a_3 = 0; double a_4 = 0.0f;
	uint8_t caption[32];
	uint8_t data[8];
	uint8_t type;

	for(uint8_t f = 0; f < 7; f++)
	{
		if(arguments[f][0] == '-' && arguments[f][1] == 'a')
		{
			a = 1;
			address = (uint16_t)strtol(&arguments[f][2], NULL, 16);
			if(address < 0x1000) address += 0x1000;
			console_write(get_string(128));
			console_HEX8((uint8_t)(address >> 8), 1, 0);
			console_HEX8((uint8_t)address, 0, 1);
			address -= 0x1000;
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'n')
		{
			setting = (uint8_t)atoi(&arguments[f][2]);
			console_write(get_string(129));
			console_number(setting, 10, 1);
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'v')
		{
			v = 1; c = f;
			if(console_root != '#') { console_write(get_string(118)); return; }
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'm')
		{
			m = 1; n = f;
			if(console_root != '#') { console_write(get_string(118)); return; }
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 't')
		{
			t = 1; type = f;
		}
		if(arguments[f][0] == '-' && arguments[f][1] == '?')
		{
			console_write(get_string(130));
			return;
		}
	}

	if(!a) { console_write(get_string(131)); return; }

	if(!m)
	{
		if(!setting) { console_write(get_string(132)); return; }

		if(!read_setting_caption(address, setting, caption)) { console_write(get_string(133)); return; }

		console_write(caption);

		if(!read_setting_data_type(address, setting, &a)) { console_write(get_string(134)); return; }

		if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }

		switch(a) {
			case 0:
				console_write(get_string(170));
				console_HEX8(caption[0], 1, 1);
				break;
			case 1:
				console_write(get_string(171));
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 2:
				console_write(get_string(172));
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 0);
				console_HEX8(caption[2], 0, 0);
				console_HEX8(caption[3], 0, 1);
				break;
			case 3:
				console_write(get_string(173));
				console_BIN(caption[0], 1, 1);
				break;
			case 4:
				console_write(get_string(174));
				memcpy(&a_4, caption, 4);
				dtostrf(a_4, 2, 3, caption);
				console_write(caption);
				console_write(get_string(1));
				break;
			case 5:
				console_write(get_string(175));
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 6:
				console_write(get_string(176));
				memcpy(&a_1, caption, 1);
				console_number(a_1, 10, 1);
				break;
			case 7:
				console_write(get_string(177));
				memcpy(&a_2, caption, 2);
				console_number(a_2, 10, 1);
				break;
		}
		if(!v) return;
	}

	if(v && !m)
	{
		console_write(get_string(136));
		for(uint8_t f = 0; f < 8; f++) data[f] = '\0';
		switch(a) {
			case 0: // HEX8
				data[0] = (uint8_t)strtol(&arguments[c][2], NULL, 16);
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 1);
				break;
			case 1: // HEX16
				a_2 = (uint16_t)strtol(&arguments[c][2], NULL, 16);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 2: // HEX32
				a_3 = (uint32_t)strtol(&arguments[c][2], NULL, 16);
				memcpy(&data[0], &a_3, sizeof(uint32_t));
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 0);
				console_HEX8(caption[2], 0, 0);
				console_HEX8(caption[3], 0, 1);
				break;
			case 3: // BIN
				a_1 = bin_to_int(&arguments[c][2]);
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_BIN(caption[0], 1, 1);
				break;
			case 4: // Float
				a_4 = atof(&arguments[c][2]);
				memcpy(data, &a_4, 4);
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				memcpy(&a_4, caption, 4);
				dtostrf(a_4, 2, 3, caption);
				console_write(caption);
				console_write(get_string(1));
				break;
			case 5: // Color
				a_2 = (uint16_t)strtol(&arguments[c][2], NULL, 16);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 6: // DEC8
				data[0] = atoi(&arguments[c][2]);
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_number(caption[0], 10, 1);
				break;
			case 7: // DEC16
				a_2 = atoi(&arguments[c][2]);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				write_setting(address, setting, data);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				memcpy(&a_2, caption, 2);
				console_number(a_2, 10, 1);
				break;
		}
	}

	if(!m) return;


		if(!t) { console_write(get_string(178)); return; }
		if(!v) { console_write(get_string(179)); return; }

		if(compare_strings(&arguments[type][2], "HEX8")) t = 0;
		if(compare_strings(&arguments[type][2], "hex8")) t = 0;
		if(compare_strings(&arguments[type][2], "HEX16")) t = 1;
		if(compare_strings(&arguments[type][2], "hex16")) t = 1;
		if(compare_strings(&arguments[type][2], "HEX32")) t = 2;
		if(compare_strings(&arguments[type][2], "hex32")) t = 2;
		if(compare_strings(&arguments[type][2], "BIN")) t = 3;
		if(compare_strings(&arguments[type][2], "bin")) t = 3;
		if(compare_strings(&arguments[type][2], "FLOAT")) t = 4;
		if(compare_strings(&arguments[type][2], "float")) t = 4;
		if(compare_strings(&arguments[type][2], "COLOR")) t = 5;
		if(compare_strings(&arguments[type][2], "color")) t = 5;
		if(compare_strings(&arguments[type][2], "DEC8")) t = 6;
		if(compare_strings(&arguments[type][2], "dec8")) t = 6;
		if(compare_strings(&arguments[type][2], "DEC16")) t = 7;
		if(compare_strings(&arguments[type][2], "dec16")) t = 7;

		console_write(get_string(180));
		console_write(filename);
		console_write(get_string(181));
		console_write(&arguments[type][2]);
		console_write(get_string(181));
		console_number(t, 10, 1);
		console_write(get_string(182));
		console_update(0, 0);
		v = 0;
		while(v == 0) { m = ui_get_key(console_keypad_size, 1, 0); if(m == 'y') v = 1; if(m == 'n') return; }
		console_write(get_string(183));
		console_update(0, 0);
		console_write(get_string(184));

		for(uint8_t f = 0; f < 8; f++) data[f] = '\0';
		switch(t) {
			case 0: // HEX8
				data[0] = (uint8_t)strtol(&arguments[c][2], NULL, 16);
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 1);
				break;
			case 1: // HEX16
				a_2 = (uint16_t)strtol(&arguments[c][2], NULL, 16);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 2: // HEX32
				a_3 = (uint32_t)strtol(&arguments[c][2], NULL, 16);
				memcpy(&data[0], &a_3, sizeof(uint32_t));
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 0);
				console_HEX8(caption[2], 0, 0);
				console_HEX8(caption[3], 0, 1);
				break;
			case 3: // BIN
				data[0] = bin_to_int(&arguments[c][2]);
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_BIN(caption[0], 1, 1);
				break;
			case 4: // Float
				a_4 = atof(&arguments[c][2]);
				memcpy(data, &a_4, 4);
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				memcpy(&a_4, caption, 4);
				dtostrf(a_4, 2, 3, caption);
				console_write(caption);
				console_write(get_string(1));
				break;
			case 5: // Color
				a_2 = (uint16_t)strtol(&arguments[c][2], NULL, 16);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_HEX8(caption[0], 1, 0);
				console_HEX8(caption[1], 0, 1);
				break;
			case 6: // DEC8
				data[0] = atoi(&arguments[c][2]);
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				console_number(caption[0], 10, 1);
				break;
			case 7: // DEC16
				a_2 = atoi(&arguments[c][2]);
				data[0] = (uint8_t)(a_2 >> 8);
				data[1] = (uint8_t)a_2;
				store_setting(address, t, data, filename);

				if(!read_setting(address, setting, caption)) { console_write(get_string(135)); return; }
				memcpy(&a_2, caption, 2);
				console_number(a_2, 10, 1);
				break;
		}

}

void pgm_less(uint8_t **arguments)
{
	uint8_t a = 0, b = 0, c = 0;
	uint8_t line_count = 12;
	uint8_t count = 64;
	uint16_t address = 0;
	uint8_t *ptr = (uint8_t *)address;

	for(uint8_t f = 0; f < 7; f++)
	{
		if(arguments[f][0] == '-' && arguments[f][1] == 'a')
		{
			a = 1;
			address = (uint16_t)strtol(&arguments[f][2], NULL, 16);
			console_write(get_string(185));
			console_HEX8((uint8_t)(address >> 8), 1, 0);
			console_HEX8((uint8_t)address, 0, 1);
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'l')
		{
			line_count = (uint8_t)strtol(&arguments[f][2], NULL, 16);
			console_write(get_string(186));
			console_number(line_count, 10, 1);
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'c')
		{
			count = (uint8_t)strtol(&arguments[f][2], NULL, 16);
			console_write(get_string(187));
			console_number(count, 10, 1);
		}
	}

	if(!a) { console_write(get_string(131)); return; }

	for(a = 0; a < count / line_count; a++)
	{
		for(b = 0; b < line_count; b++)
		{
			if(address >= 0x1000 && address < 0x1800) { console_HEX8(EEPROM_ReadByte(address - 0x1000 + c), 0, 0); }
			else { console_HEX8((uint8_t)*ptr + c, 0, 0); }
			console_write(get_string(0));
			c++;
		}
		console_write("\n\0");
	}
}

void pgm_paint()
{
	uint8_t a = 0;
	uint16_t color_set = WHITE;

	lcd_clear_screen(BLACK);
	lcd_draw_window(420, 460, 20, 60, GRAY1);
	lcd_draw_window(20, 60, 280, 319, RED);
	lcd_draw_window(60, 100, 280, 319, GREEN);
	lcd_draw_window(100, 140, 280, 319, BLUE);
	lcd_draw_window(140, 180, 280, 319, MAGENTA);
	lcd_draw_window(180, 220, 280, 319, YELLOW);
	lcd_draw_window(220, 260, 280, 319, CYAN);
	lcd_draw_window(260, 300, 280, 319, VIOLET);

    while (1) {
        // Your main application code here (if any)
		if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
		{
			a++;
			cursor_x_old = cursor_x;
			cursor_y_old = cursor_y;
			touch_refresh();

			if((cursor_x <= 460) && (cursor_x >= 420) && (cursor_y <= 60) && (cursor_y >= 20))
			{
				lcd_clear_screen(BLACK);
				lcd_draw_window(420, 460, 20, 60, GRAY1);
				lcd_draw_window(20, 60, 280, 319, RED);
				lcd_draw_window(60, 100, 280, 319, GREEN);
				lcd_draw_window(100, 140, 280, 319, BLUE);
				lcd_draw_window(140, 180, 280, 319, MAGENTA);
				lcd_draw_window(180, 220, 280, 319, YELLOW);
				lcd_draw_window(220, 260, 280, 319, CYAN);
				lcd_draw_window(260, 300, 280, 319, VIOLET);
				a = 0;
				return;
			}
			if((cursor_x <= 60) && (cursor_x >= 20) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = RED; a = 0; lcd_draw_window(420, 460, 280, 319, RED);}
			if((cursor_x <= 100) && (cursor_x >= 60) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = GREEN; a = 0; lcd_draw_window(420, 460, 280, 319, GREEN);}
			if((cursor_x <= 140) && (cursor_x >= 100) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = BLUE; a = 0; lcd_draw_window(420, 460, 280, 319, BLUE);}
			if((cursor_x <= 180) && (cursor_x >= 140) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = MAGENTA; a = 0; lcd_draw_window(420, 460, 280, 319, MAGENTA);}
			if((cursor_x <= 220) && (cursor_x >= 180) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = YELLOW; a = 0; lcd_draw_window(420, 460, 280, 319, YELLOW);}
			if((cursor_x <= 260) && (cursor_x >= 220) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = CYAN; a = 0; lcd_draw_window(420, 460, 280, 319, CYAN);}
			if((cursor_x <= 300) && (cursor_x >= 260) && (cursor_y <= 319) && (cursor_y >= 280)) {color_set = VIOLET; a = 0; lcd_draw_window(420, 460, 280, 319, VIOLET);}

			if(((cursor_x > (cursor_x_old + 3)) || (cursor_x < (cursor_x_old - 3)) || (cursor_y > (cursor_y_old + 3)) && (cursor_y < (cursor_y_old - 3))) && (a > 1)) {cursor_x = cursor_x_old; cursor_y = cursor_y_old; }
			if(a > 1)
			{
				lcd_draw_line_x(cursor_x - 1, cursor_x + 1, cursor_y - 1, color_set);
				lcd_draw_line_x(cursor_x - 1, cursor_x + 1, cursor_y, color_set);
				lcd_draw_line_x(cursor_x - 1, cursor_x + 1, cursor_y + 1, color_set);
			}
			if(a > 2) a = 2;
		}
		if(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))
		{
			cursor_x_old = cursor_x;
			cursor_y_old = cursor_y;
			a = 0;
		}
    }
}

void pgm_ls()
{
	uint8_t error, a;
	uint8_t dirname[9];

	if(!(SD_status & (1 << SD_INSERTED))) { console_write(get_string(188)); return; }
	if(!(SD_status & (1 << FAT32_ACTIVE)))
	{
		error = getBootSectorData();
		if(error) { console_write(get_string(189)); return; }
		SD_status |= (1 << FAT32_ACTIVE);
	}
	error = 0;
	while(console_input_buffer[error] != '/') { error++; if(error >= console_input_buffer_size) { console_write(get_string(190)); return; }}
	if(console_input_buffer[error + 1] == console_root) { findFilesInDirectory(0); /*findFiles(GET_LIST, 0);*/ return; }

	error++;
	a = error;
	while(console_input_buffer[error] != '/') { error++; if(error >= console_input_buffer_size) { console_write(get_string(190)); return; }}
	uint8_t c = 0;
	for(uint8_t b = a; b < error; b++) dirname[c++] = console_input_buffer[b];
	dirname[c] = '\0';
	findFilesInDirectory(dirname);
}

void pgm_cd(uint8_t *dirname)
{
	if(!(SD_status & (1 << SD_INSERTED))) { console_write(get_string(188)); return; }
	if(!(SD_status & (1 << FAT32_ACTIVE)))
	{
		uint8_t error = getBootSectorData();
		if(error) { console_write(get_string(189)); return; }
		SD_status |= (1 << FAT32_ACTIVE);
	}
	if(dirname[0] == '\0')
	{
		console_input_index = console_make_prompt(2, 0);
		return;
	}
	unsigned long error = findCluster(dirname);
	if(error == 0)
	{
		console_write(get_string(191));
		console_update(0, 0);
		return;
	}
	uint8_t formattedName[9];
    uint8_t k, l = 0;
    for(k = 0; k < 9; k++) formattedName[k] = '\0';
    k = 0;
    while(dirname[k] != '\0' && dirname[k] != '\n')
    {
        if (dirname[k] == '\n' || dirname[k] == '\0')
            break;
        if(dirname[k] >= 33 && dirname[k] <= 126 && dirname[k] != '/' && dirname[k] != '.')
          if(dirname[k] >= 'a' && dirname[k] <= 'z') { formattedName[l++] = dirname[k] - 'a' + 'A'; }// Convert to uppercase
          else { formattedName[l++] = dirname[k]; }
        k++;
    }
	console_input_index = console_make_prompt(3, formattedName);
}

void pgm_read_file(uint8_t *filename, uint8_t **arguments)
{
	uint16_t bytes = 256;
	uint8_t reset = 0;
	uint8_t *buffer;

	if(filename[0] == '\0') { console_write(get_string(191)); return ; }

	for(uint8_t f = 0; f < 7; f++)
	{
		if(arguments[f][0] == '-' && arguments[f][1] == 'n')
		{
			bytes = (uint16_t)strtol(&arguments[f][2], NULL, 16);
		}
		if(arguments[f][0] == '-' && arguments[f][1] == 'r')
		{
			reset = 1;
		}
	}

	buffer = (uint8_t *)malloc(bytes);
	if (buffer == NULL) { console_write(get_string(192)); return ; }

	if(reset) readFileData(filename, buffer, 0, bytes);
	else if(!reset) readFileData(filename, buffer, 1, bytes);

	console_write(buffer);
	console_write("\n\0");

	if(buffer) {free(buffer);
	buffer = NULL;}
}

void pgm_bitmap(uint8_t *filename, uint8_t **arguments)
{
    uint16_t bytes = 256;
    uint8_t *buffer;

    if (filename[0] == '\0')
    {
        console_write(get_string(191));
        return;
    }

    buffer = (uint8_t *)malloc(bytes);
    if (buffer == NULL)
    {
        console_write(get_string(193));
        return;
    }

    uint16_t bytesRead = 0;

    do
    {
		bytesRead = readFileData(filename, buffer, 1, bytes);

        if (bytesRead > 0)
        {
            // Process the bitmap header to get image dimensions
            uint16_t width = *((uint16_t *)(buffer + 18));
            uint16_t height = *((uint16_t *)(buffer + 22));

            // Extract the starting coordinates from the arguments
            uint16_t x = 0;
            uint16_t y = 0;

            for (uint8_t f = 0; f < 7; f++)
            {
                if (arguments[f][0] == '-' && arguments[f][1] == 'x')
                {
                    x = (uint16_t)strtol(&arguments[f][2], NULL, 10);
                }
                if (arguments[f][0] == '-' && arguments[f][1] == 'y')
                {
                    y = (uint16_t)strtol(&arguments[f][2], NULL, 10);
                }
            }

            // Draw the bitmap on the screen
            for (uint16_t i = 54; i < bytesRead; i += 3)
            {
                uint16_t color = ((uint16_t)buffer[i] << 8) | buffer[i + 1];
                lcd_draw_pixel(x, y, color);

                x++;

                if (x >= width)
                {
                    x = 0;
                    y++;
                }

                if (y >= height)
                {
                    break; // Stop if we've drawn the entire image
                }
            }
        }
    } while (bytesRead == bytes);

    if (buffer)
    {
        free(buffer);
        buffer = NULL;
    }
}

// Returns 1 if calculation successful, 0 if parameters invalid
uint8_t calculate_pwm_params(uint16_t freq, uint8_t duty, uint8_t multiplier,
                           uint16_t* value_a, uint16_t* value_b, uint8_t* presc) {
    // Convert frequency to Hz
    uint32_t freq_hz = freq;
    if (multiplier == 1) freq_hz *= 1000;
    else if (multiplier == 2) freq_hz *= 1000000;

    // Validate inputs
    if (freq_hz == 0 || duty > 100) return 0;

    // System clock frequency (32MHz)
    const uint32_t freq_cpu = 32000000UL;

    // Available prescaler values
    const uint16_t prescalers[] = {1, 2, 4, 8, 64, 256, 1024};

    // Calculate period in clock cycles
    uint32_t total_cycles = freq_cpu / freq_hz;

    // Find suitable prescaler
    uint8_t best_presc_idx = 0;
    uint32_t best_period = 0;

    for (uint8_t i = 0; i < 7; i++) {
        uint32_t period = total_cycles / prescalers[i];

        // Check if period fits in 16-bit timer
        if (period <= 0xFFFF && period >= 30) {  // Minimum 30 for a+b minimums
            best_presc_idx = i;
            best_period = period;
            break;
        }
    }

    // Check if we found a suitable prescaler
    if (best_period == 0) return 0;

    // Calculate a and b values based on duty cycle
    uint32_t total_time = best_period;
    uint32_t on_time = (total_time * duty) / 100;
    uint32_t off_time = total_time - on_time;

    // Ensure minimum values
    if (off_time < 10) off_time = 10;
    if (on_time < 20) on_time = 20;

    // Assign outputs
    *value_a = (uint16_t)off_time;
    *value_b = (uint16_t)best_period;
    *presc = best_presc_idx + 1;  // Convert to 1-7 range

    // Validate final values
    if (*value_b <= *value_a) return 0;
    if (*value_b > 0xFFFF) return 0;

    return 1;
}

// Example usage:
/*
uint16_t value_a, value_b;
uint8_t presc;
uint8_t result = calculate_pwm_params(1000, 50, 0, &value_a, &value_b, &presc);
if (result) {
    // Use the calculated values
    ll_timer0_init(value_a, value_b, 1, presc);
} else {
    // Handle error case
}
*/

uint8_t pgm_pwm(uint8_t port, uint8_t bit, uint16_t freq, uint8_t duty, uint8_t status, uint8_t multiplier)
{
	uint16_t value_a, value_b;
	uint8_t presc;

	if(!bit) { system_error_hadle(41); return 4; }
	if(bit > 8) { system_error_hadle(42); return 4; }
	if(!port) { system_error_hadle(43); return 4; }
	if(port > 6) { system_error_hadle(44); return 4; }

	bit--;

	if(!freq)
	{
		ll_timer0_init(0, 0, 0, 1);
		ll_timer0_pwm = 0;

		if(status == 1) BCD(port, bit);
		if(status == 2) BSP(port, bit);
		if(status == 0) BCP(port, bit);

		return 1;
	}

	BSD(port, bit);

	if(duty > 99)
	{
		ll_timer0_init(0, 0, 0, 1);
		ll_timer0_pwm = 0;
		BSP(port, bit);
		return 2;
	}

	if(!calculate_pwm_params(freq, duty, multiplier, &value_a, &value_b, &presc))
	{
		system_error_hadle(40);
		return 4;
	}

	ll_timer0_pwm = 1;
	ll_timer0_pwm_port = port;
	ll_timer0_pwm_pin = bit;

	ll_timer0_init(value_a, value_b, 1, presc);

	return 0;
}

void system_error_hadle(uint8_t error)
{
	if((console_status & (1 << CONSOLE_ON)))
	{
		console_write(get_error_msg(error));
		return;
	}
	ui_message_close(get_error_msg(error));
	return;
}



