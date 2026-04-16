

#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "mcu_driver.h"
#include "lcd_driver.h"
#include "FAT32.h"
#include "SD_routines.h"
#include "ningen_ui.h"
#include "colors.h"
#include "ram_driver.h"
#include "kb_driver.h"
#include "console.h"
#include "programs.h"


static const char PSTR_BMP1[]      PROGMEM = "bitmap: usage: file_reader <filename>\n";
static const char PSTR_BMP2[]      PROGMEM = "bitmap: error opening file\n";
static const char PSTR_BMP3[]      PROGMEM = "Bitmap info loaded -> size : ";
static const char PSTR_BMP4[]      PROGMEM = "The file is not a bitmap\n";
static const char PSTR_BMP5[]      PROGMEM = "File format error or data missing\n";
static const char PSTR_BMP6[]      PROGMEM = "Only 24 bits BMP suported (for now)\n";
//static const char PSTR_BMP7[]      PROGMEM = " 128 KB\n"; // not used
//static const char PSTR_BMP8[]      PROGMEM = " 256 KB\n"; // not used
//static const char PSTR_BMP9[]      PROGMEM = " 384 KB\n"; // not used

void pgm_show_bitmap(uint8_t argc, uint8_t *argv[])
{
   if (argc < 2) {
       console_write_f(PSTR_BMP1);
       return;
   }

   uint8_t *filename = argv[1];
   
   // Open the file
   if (!FAT_open_file(filename)) {
       console_write_f(PSTR_BMP2);
       return;
   }

   uint8_t bitmap_data[128];
   if(!FAT_open_file(filename)) return;
   if(!FAT_read_bytes(NULL, bitmap_data, 14)) return;
   if((bitmap_data[0] != 'B') || (bitmap_data[1] != 'M')) { console_write_f(PSTR_BMP4); FAT_close_file(); return; }
   uint16_t start_offset = (uint16_t)(bitmap_data[11] << 8) | bitmap_data[10];
   uint32_t bitmap_size = (uint32_t)(bitmap_data[5] << 24) | (uint32_t)(bitmap_data[4] << 16) | (uint32_t)(bitmap_data[3] << 8) | bitmap_data[2];
   if(!FAT_read_bytes(NULL, bitmap_data, start_offset - 14)) { console_write_f(PSTR_BMP5); FAT_close_file(); return; }
   console_write_f(PSTR_BMP3);
   uint16_t size_x = (uint16_t)(bitmap_data[5] << 8) | bitmap_data[4];
   uint16_t size_y = (uint16_t)(bitmap_data[9] << 8) | bitmap_data[8];
   uint16_t bits_per_pixel = (uint16_t)(bitmap_data[15] << 8) | bitmap_data[14];
   console_number(size_x, 10, NULL, " x ");
   console_number(size_y, 10, NULL, " ");
   console_number(bits_per_pixel, 10, NULL, " bits\n");
   
   // Only support 24bpp for now
   if(bits_per_pixel != 24) { console_write_f(PSTR_BMP6); FAT_close_file(); return; }

   // Clamp to LCD size
   uint16_t draw_x = (size_x > lcd_size_x) ? lcd_size_x : size_x;
   uint16_t draw_y = (size_y > lcd_size_y) ? lcd_size_y : size_y;

   // BMP rows are padded to 4-byte boundary
   // 24bpp = 3 bytes per pixel
   uint16_t row_bytes = size_x * 3;
   uint8_t  padding   = 4 - (row_bytes - ((row_bytes / 4) * 4));

   // BMP is stored bottom-up, so we read from last row to first
   // Set the window once for the whole image

   lcd_clear_screen(BLACK);
   console_visible = 0;

   uint8_t  row_buf[480 * 3]; // max row size for 480px wide 24bpp
   uint16_t y;
   uint16_t x;

   for(y = 0; y < draw_y; y++)
   {
       if(!FAT_read_bytes(NULL, row_buf, row_bytes))
       { console_write_f(PSTR_BMP5); FAT_close_file(); console_visible = 1; wish_redraw(); return; }

       if(padding)
       {
           uint8_t pad_buf[3]; // max padding is 3 bytes
           if(!FAT_read_bytes(NULL, pad_buf, padding))
           { console_write_f(PSTR_BMP5); FAT_close_file(); console_visible = 1; wish_redraw(); return; }
       }

       for(x = 0; x < draw_x; x++)
       {
           uint8_t b = row_buf[x * 3 + 0];
           uint8_t g = row_buf[x * 3 + 1];
           uint8_t r = row_buf[x * 3 + 2];
           //lcd_draw_pixel(x, draw_y - y, _24bits_to_565(r, g, b));
           lcd_draw_pixel_24bit(x, y, r, g, b);
       }
   }

   while(1)
   {
       if(keyboard_get_key() == 'q')
       {
           FAT_close_file();
           console_visible = 1;
           wish_redraw();
           return;
       }
   }
   console_visible = 1;
   wish_redraw();
   FAT_close_file();
   return;
}

static const char PSTR_FR1[]  PROGMEM = "file reader: usage: file_reader <filename>\n";
static const char PSTR_FR2[]  PROGMEM = "file reader: file not found\n";
static const char PSTR_FR3[]  PROGMEM = "file reader: file is too large (>32 KB)\n";
static const char PSTR_FR4[]  PROGMEM = "file reader: read error\n";
static const char PSTR_FR5[]  PROGMEM = "File loading succeed\n";
static const char PSTR_FR6[]  PROGMEM = "Press 'q' to exit the file viewer";

void pgm_file_reader(uint8_t argc, uint8_t *argv[])
{
    if (argc < 2) {
        console_write_f(PSTR_FR1); // Using the same error message as bitmap function
        return;
    }

    uint8_t *filename = argv[1];
    
    // Get directory entry to check if file exists and get size
    struct dir_Structure dir_entry;
    
    // Search for file in directory
    if(!FAT_find_file(filename, (uint8_t *)&dir_entry))
    {
       console_write_f(PSTR_FR2);
       return;
    }

    uint32_t ram_addr = (console_ram_start_addr + console_ram_buffer_size + 32768);
    uint32_t max_file_size = 32768;
    
    // Check if file is too large
    if (dir_entry.fileSize > max_file_size) {
        console_write_f(PSTR_FR3);
        return;
    }
    
    // Open the file
    if (!FAT_open_file(filename)) {
        console_write_f(PSTR_FR2); // "error opening file"
        return;
    }
    
    // Read file into RAM
    uint32_t bytes_read = 0;
    uint8_t buffer[64];
    uint16_t remaining_bytes = dir_entry.fileSize;
    
    while (remaining_bytes > 0) {
        uint16_t to_read = (remaining_bytes > 64) ? 64 : remaining_bytes;
        
        if (!FAT_read_bytes(NULL, buffer, to_read)) {
            console_write_f(PSTR_FR4);
            FAT_close_file();
            return;
        }
        
        ram_write(ram_addr + bytes_read, buffer, to_read);
        bytes_read += to_read;
        remaining_bytes -= to_read;
    }
    
    // Show success message
    console_write_f(PSTR_FR5);
    FAT_close_file();
    
    // Prepare display
    lcd_clear_screen(console_back_color);
    console_visible = 0;
    
    // Draw title bar (GRAY)
    uint16_t title_bar_height = char_size_y * 2;
    lcd_draw_window(0, lcd_size_x - 1, 0, title_bar_height - 1, GRAY);
    
    // Draw status bar
    uint16_t status_bar_y = lcd_size_y - char_size_y * 2;
    lcd_draw_window(0, lcd_size_x - 1, status_bar_y, lcd_size_y - 1, GRAY);
    
    // Display file name in title bar
    lcd_set_position(10, 5);
    lcd_draw_string(filename, WHITE, console_char_size);
    
    // Display file size in status bar
    lcd_set_position(10, status_bar_y + 5);
    lcd_number(dir_entry.fileSize, 10, WHITE, 1, NULL, " bytes");
    
    // Draw exit instruction
    lcd_set_position(lcd_size_x - (50 * char_size_x), status_bar_y + 5);
    lcd_draw_string_f(PSTR_FR6, WHITE, 1);
    
    // Display file content (simple text display)
    uint8_t line_buffer[64];
    uint16_t line_count = 0;
    uint16_t max_lines = (lcd_size_y - title_bar_height * 2 - char_size_y) / char_size_y;
    uint16_t scroll_offset = 0;
    uint16_t current_line = 0;
    
    // Get the total number of lines by reading all content
    uint32_t file_pos = ram_addr;
    uint16_t line_count_calc = 0;
    uint8_t byte;
    while (file_pos < ram_addr + dir_entry.fileSize) {
        byte = ram_read_byte(file_pos++);
        if (byte == '\n' || byte == '\0') {
            line_count_calc++;
        }
    }
    
    // Display content with scrolling
    uint16_t display_lines = 0;
    uint16_t current_line_start = 0;
    uint16_t chars_in_line = 0;
    uint16_t current_x = 0;
    uint16_t current_y = title_bar_height + 5;
    
    // Draw initial lines
    file_pos = ram_addr;
    uint8_t char_buffer[128];
    uint16_t char_index = 0;
    
    while (file_pos < ram_addr + dir_entry.fileSize) {
        byte = ram_read_byte(file_pos++);
        
        if (byte == '\n' || byte == '\0') {
            char_buffer[char_index] = '\0';
            if (current_y >= title_bar_height && current_y < lcd_size_y - char_size_y * 2) {
                lcd_set_position(10, current_y);
                lcd_draw_string(char_buffer, console_text_color, console_char_size);
                display_lines++;
            }
            char_index = 0;
            current_y += char_size_y;
        } else if (byte == '\r') {
            // Skip carriage returns
            continue;
        } else {
            char_buffer[char_index++] = byte;
        }
    }
    
    // Main loop for scrolling and interaction
    uint8_t key;
    while (1) {
        key = keyboard_wait_key();
        
        if (key == 'q') {
            break;
        } else if (key == KEY_UP) {
            scroll_offset = (scroll_offset > 0) ? scroll_offset - 1 : 0;
        } else if (key == KEY_DOWN) {
            scroll_offset = (scroll_offset < line_count_calc) ? scroll_offset + 1 : line_count_calc;
        } else if (key == KEY_LEFT) {
            // Horizontal scrolling not implemented for text files
        } else if (key == KEY_RIGHT) {
            // Horizontal scrolling not implemented for text files
        }
        
        // Redraw display with new scroll position
        lcd_draw_window(0, lcd_size_x - 1, title_bar_height, lcd_size_y - (title_bar_height * 2) - 1, console_back_color);
        
        // Redraw file content
        uint32_t display_pos = ram_addr;
        uint16_t line_num = 0;
        uint8_t current_line_char = 0;
        uint16_t y_pos = title_bar_height + 5;
        uint8_t in_line = 0;
        
        // Reset position for display
        file_pos = ram_addr;
        char_index = 0;
        current_y = title_bar_height + 5;
        
        while (file_pos < ram_addr + dir_entry.fileSize && y_pos < lcd_size_y - char_size_y * 2) {
            byte = ram_read_byte(file_pos++);
            
            if (byte == '\n' || byte == '\0') {
                char_buffer[char_index] = '\0';
                if (line_num >= scroll_offset && line_num < scroll_offset + max_lines) {
                    lcd_set_position(10, y_pos);
                    lcd_draw_string(char_buffer, WHITE, console_char_size);
                }
                char_index = 0;
                line_num++;
                y_pos += char_size_y;
            } else if (byte == '\r') {
                // Skip carriage returns
                continue;
            } else {
                char_buffer[char_index++] = byte;
            }
        }
    }
    // Clean up and exit
    console_visible = 1;
    wish_redraw();
}
