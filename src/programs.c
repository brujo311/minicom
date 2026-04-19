

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

void pgm_file_reader(uint8_t argc, uint8_t *argv[])
{
    if (argc < 2) {
        console_write_f(PSTR("file reader: usage: file_reader <filename>\n"));
        return;
    }

    uint8_t *filename = argv[1];
    struct dir_Structure dir_entry;

    if (!FAT_find_file(filename, (uint8_t *)&dir_entry)) {
        console_write_f(PSTR("file reader: file not found\n"));
        return;
    }

    uint32_t ram_addr = ram_allocate(dir_entry.fileSize);
    if (!ram_addr) { console_write_f(PSTR("Error allocating file in RAM, not enaugh memory\n")); return; }
    console_write_f(PSTR("File allocated to RAM -> "));
    console_number(dir_entry.fileSize, 10, NULL, " KB\n");

    if (!FAT_open_file(filename)) {
        console_write_f(PSTR("file reader: file not found\n"));
        ram_free(ram_addr);
        return;
    }

    uint32_t bytes_read = 0;
    uint8_t buffer[64];
    uint16_t remaining_bytes = dir_entry.fileSize;
    while (remaining_bytes > 0) {
        uint16_t to_read = (remaining_bytes > 64) ? 64 : remaining_bytes;
        if (!FAT_read_bytes(NULL, buffer, to_read)) {
            console_write_f(PSTR("file reader: read error\n"));
            FAT_close_file();
            ram_free(ram_addr);
            return;
        }
        ram_write(ram_addr + bytes_read, buffer, to_read);
        bytes_read += to_read;
        remaining_bytes -= to_read;
    }

    console_write_f(PSTR("File loading succeed\n"));
    FAT_close_file();

    // --- Display setup ---
    lcd_clear_screen(console_back_color);
    console_visible = 0;

    uint16_t title_bar_height = char_size_y * 2;
    uint16_t status_bar_y     = lcd_size_y - char_size_y * 2;

    // FIX: max_lines uses status_bar_y directly instead of double-subtracting title_bar_height
    uint16_t content_top    = title_bar_height + 5;
    uint16_t content_bottom = status_bar_y;
    uint16_t max_lines      = (content_bottom - content_top) / char_size_y;

    lcd_draw_window(0, lcd_size_x - 1, 0, title_bar_height - 1, GRAY);
    lcd_draw_window(0, lcd_size_x - 1, status_bar_y, lcd_size_y - 1, GRAY);

    lcd_set_position(10, 5);
    lcd_draw_string(filename, WHITE, console_char_size);

    lcd_set_position(10, status_bar_y + 5);
    lcd_number(dir_entry.fileSize, 10, WHITE, 1, NULL, " bytes");

    lcd_set_position(lcd_size_x - (33 * char_size_x), status_bar_y + 5);
    lcd_draw_string_f(PSTR("Press 'q' to exit the file viewer"), WHITE, 1);

    // --- Count total lines ---
    uint16_t total_lines = 0;
    uint32_t file_pos = ram_addr;
    uint8_t  byte;
    uint8_t  last_was_newline = 1;   // treat start-of-file as a fresh line

    while (file_pos < ram_addr + dir_entry.fileSize) {
        byte = ram_read_byte(file_pos++);
        if (byte == '\n') {
            total_lines++;
            last_was_newline = 1;
        } else if (byte != '\r') {
            last_was_newline = 0;
        }
    }
    // If the file doesn't end with '\n', the last line still counts
    if (!last_was_newline) total_lines++;

    // --- Redraw helper lambda (inline via goto-free block) ---
    // We'll use a local function-style macro to avoid repetition.
    // Called once before the loop and then after every keypress.

    uint16_t scroll_offset = 0;
    uint8_t  char_buffer[128];
    uint16_t char_index;

    #define REDRAW_CONTENT() do {                                                   \
        /* Clear content area */                                                    \
        lcd_draw_window(0, lcd_size_x - 1,                                         \
                        title_bar_height, content_bottom - 1,                      \
                        console_back_color);                                        \
        /* Walk the file, skip lines before scroll_offset,                         \
           draw lines in [scroll_offset, scroll_offset + max_lines) */             \
        uint32_t _pos   = ram_addr;                                                 \
        uint16_t _lnum  = 0;          /* logical line number being assembled */    \
        uint16_t _drawn = 0;          /* how many lines rendered so far */         \
        uint16_t _y     = content_top;                                              \
        char_index = 0;                                                             \
        while (_pos < ram_addr + dir_entry.fileSize) {                             \
            uint8_t _b = ram_read_byte(_pos++);                                    \
            if (_b == '\r') continue;                                               \
            if (_b == '\n' || _b == '\0') {                                        \
                char_buffer[char_index] = '\0';                                    \
                if (_lnum >= scroll_offset && _drawn < max_lines) {                \
                    lcd_set_position(10, _y);                                      \
                    lcd_draw_string(char_buffer, console_text_color,               \
                                    console_char_size);                            \
                    _y += char_size_y;   /* FIX: advance Y only for drawn lines */ \
                    _drawn++;                                                       \
                }                                                                  \
                char_index = 0;                                                    \
                _lnum++;                                                            \
                if (_drawn >= max_lines) break;  /* no need to read further */     \
            } else {                                                                \
                if (char_index < sizeof(char_buffer) - 1)                          \
                    char_buffer[char_index++] = _b;                                \
            }                                                                      \
        }                                                                          \
        /* Handle last line if file doesn't end with '\n' */                       \
        if (char_index > 0 && _lnum >= scroll_offset && _drawn < max_lines) {     \
            char_buffer[char_index] = '\0';                                        \
            lcd_set_position(10, _y);                                              \
            lcd_draw_string(char_buffer, console_text_color, console_char_size);  \
        }                                                                          \
    } while (0)

    // Initial draw
    REDRAW_CONTENT();

    // --- Input / scroll loop ---
    uint8_t key;
    uint16_t max_scroll = (total_lines > max_lines) ? (total_lines - max_lines) : 0;

    while (1) {
        key = keyboard_wait_key();

        if (key == 'q') {
            break;
        } else if (key == KEY_UP) {
            if (scroll_offset > 0) {
                scroll_offset--;
                REDRAW_CONTENT();
            }
        } else if (key == KEY_DOWN) {
            if (scroll_offset < max_scroll) {
                scroll_offset++;
                REDRAW_CONTENT();
            }
        }
        // KEY_LEFT / KEY_RIGHT: horizontal scroll not implemented
    }

    #undef REDRAW_CONTENT

    // --- Cleanup ---
    console_visible = 1;
    ram_free(ram_addr);
    wish_redraw();
}

void pgm_mk_file(uint8_t argc, uint8_t *argv[])
{
    if (argc < 2) {
        console_write_f(PSTR("file maker: usage: mkfile <filename>\n"));
        return;
    }

    uint8_t *filename = argv[1];
    console_write_f(PSTR("Creating file... "));
    if(FAT_create_file(filename))
    {
        console_write_f(PSTR("done\n"));
        return;
    }
    console_write_f(PSTR("exit with error status : "));
    console_number(FAT_error_log, 10, NULL, "\n");
    return;
}

void pgm_app_file(uint8_t argc, uint8_t *argv[])
{
    if (argc < 3) {
        console_write_f(PSTR("file writer: usage: wfile <filename> <data>\n"));
        return;
    }

    uint8_t *filename = argv[1];
    uint8_t *data = argv[2];

    console_write_f(PSTR("Opening file... "));
    if(FAT_append_data(filename, data, strlen(data)))
    {
        console_write_f(PSTR("done\n"));
        return;
    }
    console_write_f(PSTR("exit with error status : "));
    console_number(FAT_error_log, 10, NULL, "\n");
    return;
}
