
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
 #include <stdint.h> // console
 #include <string.h> // console
#include "mcu_driver.h"
#include "lcd_driver.h"
#include "FAT32.h"
#include "SD_routines.h"
#include "ningen_ui.h"
#include "colors.h"
#include "ram_driver.h"
#include "kb_driver.h"
#include "console.h"
//#include "awire_driver.h"







int main()
{
    mcu_set_32mhz();

    //WDT.CTRL = WDT_DISABLE_gc; // Disable watchdog timer
	//Bluetooth_off();
    lcd_init_controller();

    lcd_clear_screen(BLACK);

    _delay_ms(100);

    SD_check_alive();

    /*if((SD_status & 0x80))
    {
        lcd_write_debug("CID :");
        for(uint8_t a = 0; a < 16; a++)
        lcd_number(SD_cid[a], 16, GREEN, 1, " ", NULL);
    }*/

    //_delay_ms(2000);

    //lcd_write_debug("Probando RAM : ");
    //lcd_number(ram_test(), 2, GREEN, 1, " ", NULL);

    //_delay_ms(5000);

    wish_init();
    wish_run();
    
    lcd_write_debug("Keyboard :");

    for(uint8_t b = 0; b < 16; b++)
    {
        lcd_number(twi_read_byte(), 16, GREEN, 1, " ", NULL);
        _delay_ms(3000);
    }

    uint16_t file_count = 0;
    uint8_t file_data[512];
    uint8_t file_name[12];
    // FAT_unload_directory(void);
    // FAT_load_directory(uint8_t *dirname); // One at a time

    uint8_t x = 0, y = 0;
    for(uint8_t a = 0; a < 8; a++)
    {
        lcd_set_position(x * 6, y * 8);
        lcd_draw_string("File line ", WHITE, 1);
        lcd_number(a, 10, GREEN, 1, NULL, " : ");
        lcd_number(FAT_error_log, 10, RED, 1, NULL, " : ");
        lcd_number(FAT_read_file_line(NULL, file_data), 10, GREEN, 1, NULL, " -> ");
        lcd_draw_string(file_data, BLUE, 1);
        y++; x = 0;
        lcd_set_position(x * 6, y * 8);
        _delay_ms(2000);
        lcd_draw_string("Specific line ", WHITE, 1);
        lcd_number(a, 10, GREEN, 1, NULL, " : ");
        lcd_number(FAT_error_log, 10, RED, 1, NULL, " : ");
        lcd_number(FAT_read_specific_file_line("DIESEL1.TXT", file_data, a), 10, GREEN, 1, NULL, " -> ");
        lcd_draw_string(file_data, BLUE, 1);
        y++; x = 0;
        _delay_ms(2000);
    }

    lcd_write_debug("Loading directory... ");
    lcd_number(FAT_load_directory("PIJA\0"), 10, YELLOW, 1, NULL, " ");
    lcd_number(FAT_error_log, 10, YELLOW, 1, NULL, " ");
    _delay_ms(1000);

    lcd_write_debug("Counting files... ");
    lcd_number(FAT_count_files_in_directory(NULL, &file_count), 10, YELLOW, 1, " ", NULL); // NULL for ROOT
    _delay_ms(1000);
    lcd_write_debug("File count :");
    lcd_number(file_count, 10, GREEN, 1, " ", NULL);

    _delay_ms(2000);

    for(uint8_t a = 1; a <= file_count; a++)
    {
        lcd_write_debug("File ");
        lcd_number(a, 10, GREEN, 1, NULL, " : ");
        lcd_number(FAT_get_file_name(a, file_data), 10, GREEN, 1, NULL, " -> ");
        for(uint8_t b = 0; b < 11; b++)
            file_name[b] = file_data[b];
        file_name[11] = '\0';
        lcd_draw_string(file_name, BLUE, 1);
        _delay_ms(1000);
    }

    lcd_write_debug("Unloading directory... ");
    lcd_number(FAT_unload_directory(), 10, YELLOW, 1, " ", NULL);
    _delay_ms(1000);

    lcd_number(FAT_count_files_in_directory(NULL, &file_count), 10, YELLOW, 1, " ", NULL); // NULL for ROOT
    lcd_write_debug("File count :");
    lcd_number(file_count, 10, GREEN, 1, " ", NULL);

    _delay_ms(2000);

    for(uint8_t a = 1; a <= file_count; a++)
    {
        lcd_write_debug("File ");
        lcd_number(a, 10, GREEN, 1, NULL, " : ");
        lcd_number(FAT_get_file_name(a, file_data), 10, GREEN, 1, NULL, " -> ");
        for(uint8_t b = 0; b < 11; b++)
            file_name[b] = file_data[b];
        file_name[11] = '\0';
        lcd_draw_string(file_name, BLUE, 1);
        _delay_ms(500);
    }
    //findFilesInDirectory(NULL);

    lcd_set_position(200, 240);
    lcd_number(mem_check(), 10, WHITE, 3, NULL, NULL);

    //ui_draw_form(0);

    //ui_draw_list_box(0, 5, "A ver\nQue onda\nCon texto\nDinamico\nLa concha\0");

    //ui_draw_form(0);

    //ui_draw_list_box(0, 5, NULL);

    while (1) {
        // Your main application code here (if any)
		if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
		;
		if(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))
		;
    }

    return 0;
}
