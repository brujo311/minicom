

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
   //uint32_t bitmap_size = ((uint32_t)bitmap_data[5] << 24) | ((uint32_t)bitmap_data[4] << 16) | ((uint32_t)bitmap_data[3] << 8) | bitmap_data[2];
   if(!FAT_read_bytes(NULL, bitmap_data, start_offset - 14)) { console_write_f(PSTR_BMP5); FAT_close_file(); return; }
   console_write_f(PSTR_BMP3);
   uint16_t size_x = (uint16_t)(bitmap_data[5] << 8) | bitmap_data[4];
   uint16_t size_y = (uint16_t)(bitmap_data[9] << 8) | bitmap_data[8];
   uint16_t bits_per_pixel = (uint16_t)(bitmap_data[15] << 8) | bitmap_data[14];
   console_number(size_x, 10, NULL, (uint8_t *)" x ");
   console_number(size_y, 10, NULL, (uint8_t *)" ");
   console_number(bits_per_pixel, 10, NULL, (uint8_t *)" bits\n");
   
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
    console_number(dir_entry.fileSize, 10, NULL, (uint8_t *)" bytes\n");

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
    lcd_number(dir_entry.fileSize, 10, WHITE, 1, NULL, (uint8_t *)" bytes");

    lcd_set_position(lcd_size_x - (33 * char_size_x), status_bar_y + 5);
    lcd_draw_string_f((const uint8_t*)PSTR("Press 'q' to exit the file viewer"), WHITE, 1);

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
    console_number(FAT_error_log, 10, NULL, (uint8_t *)"\n");
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
    if(FAT_append_data(filename, data, strlen((const char *)data)))
    {
        console_write_f(PSTR("done\n"));
        return;
    }
    console_write_f(PSTR("exit with error status : "));
    console_number(FAT_error_log, 10, NULL, (uint8_t *)"\n");
    return;
}

// ============================================================
//  pgm_text_editor  –  nano-like editor
//
//  Special keys:
//    fn+s  (0x9B)  Save
//    fn+q  (0x8D)  Quit
//    Arrow keys    Move cursor / scroll
//    Backspace     Delete char before cursor
//    Delete(0x7F)  Delete char at cursor
//    Enter         Insert newline
//    Printable     Insert character
//
//  RAM layout  (base = ram_addr):
//    [0 .. file_size-1]   file bytes  (grows/shrinks as edited)
//  A separate uint32_t  `file_size`  tracks the live byte count.
//  Max buffer size is capped at MAX_FILE_SIZE bytes.
// ============================================================

#define MAX_FILE_SIZE   49152u   /* 48 KB hard cap                      */
#define BLINK_TICKS     1000u   /* ~poll iterations between blink tog  */
#define LINE_BUF_SIZE   256u     /* max visible chars per line          */

// Key codes
#define KEY_FN_S        0x9B
#define KEY_FN_Q        0x8D
//#define KEY_BACKSPACE   0x08
#define KEY_DELETE      0x7F
//#define KEY_ENTER       0x0D

// ----------------------------------------------------------------
//  Forward-declared helpers (defined as static below)
// ----------------------------------------------------------------
static uint32_t  editor_line_start   (uint32_t base, uint32_t size, uint16_t line);
static uint16_t  editor_line_length  (uint32_t base, uint32_t size, uint32_t line_start);
static uint16_t  editor_count_lines  (uint32_t base, uint32_t size);
static void      editor_insert_byte  (uint32_t base, uint32_t *size, uint32_t pos, uint8_t b);
static void      editor_delete_byte  (uint32_t base, uint32_t *size, uint32_t pos);

// ----------------------------------------------------------------
void pgm_text_editor(uint8_t argc, uint8_t *argv[])
{
    if (argc < 2) {
        console_write_f(PSTR("editor: usage: edit <filename>\n"));
        return;
    }

    uint8_t *filename = argv[1];
    uint8_t  file_exists = 0;

    // ---- Allocate RAM buffer ----
    uint32_t ram_addr = ram_allocate(MAX_FILE_SIZE);
    if (!ram_addr) {
        console_write_f(PSTR("editor: not enough RAM\n"));
        return;
    }

    uint32_t file_size = 0;

    // ---- Load file if it exists ----
    struct dir_Structure dir_entry;
    if (FAT_find_file(filename, (uint8_t *)&dir_entry)) {
        file_exists = 1;
        uint32_t fsize = dir_entry.fileSize;
        if (fsize > MAX_FILE_SIZE) fsize = MAX_FILE_SIZE;

        if (FAT_open_file(filename)) {
            uint32_t remaining = fsize;
            uint8_t  buf[64];
            while (remaining > 0) {
                uint16_t to_read = (remaining > 64) ? 64 : (uint16_t)remaining;
                if (!FAT_read_bytes(NULL, buf, to_read)) {
                    console_write_f(PSTR("editor: read error\n"));
                    FAT_close_file();
                    ram_free(ram_addr);
                    return;
                }
                ram_write(ram_addr + file_size, buf, (uint8_t)to_read);
                file_size += to_read;
                remaining -= to_read;
            }
            FAT_close_file();
        }
    }
    // (if not found, we start with an empty buffer – file created on save)

    // ---- Display setup ----
    lcd_clear_screen(console_back_color);
    console_visible = 0;

    const uint16_t title_bar_height = char_size_y + 6;
    const uint16_t status_bar_y     = lcd_size_y - char_size_y * 2;
    const uint16_t content_top      = title_bar_height + 2;
    const uint16_t content_bottom   = status_bar_y;
    const uint16_t max_lines        = (content_bottom - content_top) / char_size_y;
    const uint16_t max_cols         = (lcd_size_x - 10) / char_size_x; /* visible columns */

    // Title bar
    lcd_draw_window(0, lcd_size_x - 1, 0, title_bar_height - 1, GRAY);
    lcd_set_position(10, 5);
    lcd_draw_string(filename, WHITE, console_char_size);
    // Status bar (static part)
    lcd_draw_window(0, lcd_size_x - 1, status_bar_y, lcd_size_y - 1, GRAY);
    lcd_set_position(lcd_size_x - (33 * char_size_x), status_bar_y + 5);
    lcd_draw_string_f((const uint8_t*)PSTR("fn+S save  fn+Q quit"), WHITE, 1);

    // ---- Editor state ----
    uint16_t cursor_row   = 0;   /* logical line  (0-based)    */
    uint16_t cursor_col   = 0;   /* char offset within line    */
    uint16_t scroll_row   = 0;   /* first visible line         */
    uint16_t scroll_col   = 0;   /* first visible column       */
    uint8_t  cursor_vis   = 1;   /* current blink state        */
    uint8_t  dirty        = 0;   /* unsaved changes flag       */
    uint32_t blink_count  = 0;

    // ---- Local macros ----

    // Draw the fixed chrome (title + status) that may get clobbered
#define DRAW_CHROME() do {                                                      \
    lcd_draw_window(0, lcd_size_x-1, 0, title_bar_height-1, GRAY);            \
    lcd_set_position(10, 4);                                                    \
    lcd_draw_string(filename, WHITE, console_char_size);                        \
    if (dirty) {                                                                \
        lcd_set_position(lcd_size_x/2, 4);                                     \
        lcd_draw_string_f((const uint8_t*)PSTR("[modified]"), YELLOW, 1);      \
    }                                                                           \
    lcd_draw_window(0, lcd_size_x-1, status_bar_y, lcd_size_y-1, GRAY);       \
    lcd_set_position(10, status_bar_y + 4);                                       \
    lcd_number(cursor_row+1, 10, WHITE, 1, (uint8_t*)"Ln ", (uint8_t*)"  Col ");\
    lcd_number(cursor_col+1, 10, WHITE, 1, NULL, NULL);                         \
    lcd_set_position(lcd_size_x - (20 * char_size_x), status_bar_y + 4);      \
    lcd_draw_string_f((const uint8_t*)PSTR("fn+S save  fn+Q quit"), WHITE, 1); \
} while(0)

    // Redraw the entire content area
#define REDRAW_CONTENT() do {                                                   \
    lcd_draw_window(0, lcd_size_x-1, content_top, content_bottom-1,            \
                    console_back_color);                                         \
    uint16_t _total = editor_count_lines(ram_addr, file_size);                  \
    for (uint16_t _r = 0; _r < max_lines; _r++) {                              \
        uint16_t _line = scroll_row + _r;                                       \
        if (_line >= _total) break;                                             \
        uint32_t _ls  = editor_line_start(ram_addr, file_size, _line);         \
        uint16_t _len = editor_line_length(ram_addr, file_size, _ls);          \
        uint16_t _y   = content_top + _r * char_size_y;                        \
        /* copy visible portion of line into a local buffer */                  \
        uint8_t  _lb[LINE_BUF_SIZE];                                            \
        uint16_t _ci  = 0;                                                      \
        for (uint16_t _c = scroll_col;                                          \
             _c < _len && _ci < max_cols && _ci < LINE_BUF_SIZE-1; _c++) {     \
            _lb[_ci++] = ram_read_byte(_ls + _c);                              \
        }                                                                       \
        _lb[_ci] = '\0';                                                        \
        if (_ci > 0) {                                                          \
            lcd_set_position(5, _y);                                           \
            lcd_draw_string(_lb, console_text_color, console_char_size);        \
        }                                                                       \
    }                                                                           \
    DRAW_CHROME();                                                              \
} while(0)

    // Draw or erase the cursor character at current position
    // cursor_vis==1 → draw a block; ==0 → redraw the underlying char
#define DRAW_CURSOR(_vis) do {                                                  \
    if (cursor_row >= scroll_row && cursor_row < scroll_row + max_lines) {     \
        uint16_t _scr_row = cursor_row - scroll_row;                            \
        uint16_t _scr_col = cursor_col - scroll_col; /* may be neg-wrap safe */\
        if (cursor_col >= scroll_col && _scr_col < max_cols) {                 \
            uint16_t _cx = 5 + _scr_col * char_size_x;                       \
            uint16_t _cy = content_top + _scr_row * char_size_y;              \
            uint32_t _ls = editor_line_start(ram_addr, file_size, cursor_row); \
            uint16_t _ll = editor_line_length(ram_addr, file_size, _ls);       \
            uint8_t  _ch = (cursor_col < _ll)                                  \
                           ? ram_read_byte(_ls + cursor_col) : ' ';            \
            uint8_t  _cs[2]; _cs[0] = _ch; _cs[1] = '\0';                     \
            if (_vis) {                                                         \
                lcd_draw_window(_cx, _cx + char_size_x - 1,                    \
                                _cy, _cy + char_size_y - 1, WHITE);            \
                lcd_set_position(_cx, _cy);                                     \
                lcd_draw_string(_cs, console_back_color, console_char_size);   \
            } else {                                                            \
                lcd_draw_window(_cx, _cx + char_size_x - 1,                    \
                                _cy, _cy + char_size_y - 1, console_back_color);\
                if (_ch != ' ') {                                               \
                    lcd_set_position(_cx, _cy);                                 \
                    lcd_draw_string(_cs, console_text_color, console_char_size);\
                }                                                               \
            }                                                                   \
        }                                                                       \
    }                                                                           \
} while(0)

    // Initial draw
    REDRAW_CONTENT();
    DRAW_CURSOR(1);

    // ----------------------------------------------------------------
    //  Main editor loop
    // ----------------------------------------------------------------
    uint8_t running = 1;
    while (running) {

        // ---- Blink cursor ----
        blink_count++;
        if (blink_count >= BLINK_TICKS) {
            blink_count  = 0;
            cursor_vis  ^= 1;
            DRAW_CURSOR(cursor_vis);
        }

        uint8_t key = keyboard_get_key();
        if (!key) continue;   /* no key – keep blinking */

        // Erase cursor before modifying state
        DRAW_CURSOR(0);
        cursor_vis = 1;
        blink_count = 0;

        // ---- Navigation ----
        if (key == KEY_UP) {
            if (cursor_row > 0) {
                cursor_row--;
                uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
                uint16_t len = editor_line_length(ram_addr, file_size, ls);
                if (cursor_col > len) cursor_col = len;
                if (cursor_row < scroll_row) {
                    scroll_row = cursor_row;
                    REDRAW_CONTENT();
                }
            }
        }
        else if (key == KEY_DOWN) {
            uint16_t total = editor_count_lines(ram_addr, file_size);
            if (cursor_row + 1 < total) {
                cursor_row++;
                uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
                uint16_t len = editor_line_length(ram_addr, file_size, ls);
                if (cursor_col > len) cursor_col = len;
                if (cursor_row >= scroll_row + max_lines) {
                    scroll_row = cursor_row - max_lines + 1;
                    REDRAW_CONTENT();
                }
            }
        }
        else if (key == KEY_LEFT) {
            if (cursor_col > 0) {
                cursor_col--;
                if (cursor_col < scroll_col) {
                    scroll_col = cursor_col;
                    REDRAW_CONTENT();
                }
            } else if (cursor_row > 0) {
                // wrap to end of previous line
                cursor_row--;
                uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
                uint16_t len = editor_line_length(ram_addr, file_size, ls);
                cursor_col = len;
                if (cursor_row < scroll_row) {
                    scroll_row = cursor_row;
                    REDRAW_CONTENT();
                }
                if (cursor_col >= scroll_col + max_cols) {
                    scroll_col = (cursor_col >= max_cols) ? cursor_col - max_cols + 1 : 0;
                    REDRAW_CONTENT();
                }
            }
        }
        else if (key == KEY_RIGHT) {
            uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
            uint16_t len = editor_line_length(ram_addr, file_size, ls);
            if (cursor_col < len) {
                cursor_col++;
                if (cursor_col >= scroll_col + max_cols) {
                    scroll_col = cursor_col - max_cols + 1;
                    REDRAW_CONTENT();
                }
            } else {
                // wrap to start of next line
                uint16_t total = editor_count_lines(ram_addr, file_size);
                if (cursor_row + 1 < total) {
                    cursor_row++;
                    cursor_col = 0;
                    scroll_col = 0;
                    if (cursor_row >= scroll_row + max_lines) {
                        scroll_row = cursor_row - max_lines + 1;
                    }
                    REDRAW_CONTENT();
                }
            }
        }

        // ---- Backspace ----
        else if (key == KEY_BACKSPACE) {
            if (cursor_col > 0) {
                // Delete char before cursor on same line
                uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
                uint32_t del_pos = ls + cursor_col - 1;
                editor_delete_byte(ram_addr, &file_size, del_pos);
                cursor_col--;
                if (cursor_col < scroll_col) {
                    scroll_col = cursor_col;
                }
                dirty = 1;
                REDRAW_CONTENT();
            } else if (cursor_row > 0) {
                // Merge current line onto end of previous line (delete the '\n')
                cursor_row--;
                uint32_t ls_prev = editor_line_start(ram_addr, file_size, cursor_row);
                uint16_t len_prev= editor_line_length(ram_addr, file_size, ls_prev);
                cursor_col = len_prev;
                // The '\n' is right after the previous line's text
                editor_delete_byte(ram_addr, &file_size, ls_prev + len_prev);
                if (cursor_row < scroll_row) scroll_row = cursor_row;
                scroll_col = (cursor_col >= max_cols) ? cursor_col - max_cols + 1 : 0;
                dirty = 1;
                REDRAW_CONTENT();
            }
        }

        // ---- Delete (forward) ----
        else if (key == KEY_DELETE) {
            uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
            uint16_t len = editor_line_length(ram_addr, file_size, ls);
            if (cursor_col < len) {
                editor_delete_byte(ram_addr, &file_size, ls + cursor_col);
                dirty = 1;
                REDRAW_CONTENT();
            } else {
                // Delete the newline → merge with next line
                uint16_t total = editor_count_lines(ram_addr, file_size);
                if (cursor_row + 1 < total) {
                    editor_delete_byte(ram_addr, &file_size, ls + len); /* '\n' */
                    dirty = 1;
                    REDRAW_CONTENT();
                }
            }
        }

        // ---- Enter ----
        else if (key == KEY_ENTER) {
            if (file_size < MAX_FILE_SIZE) {
                uint32_t ls  = editor_line_start(ram_addr, file_size, cursor_row);
                editor_insert_byte(ram_addr, &file_size, ls + cursor_col, '\n');
                cursor_row++;
                cursor_col  = 0;
                scroll_col  = 0;
                if (cursor_row >= scroll_row + max_lines) {
                    scroll_row = cursor_row - max_lines + 1;
                }
                dirty = 1;
                REDRAW_CONTENT();
            }
        }

        // ---- Save  fn+s ----
        else if (key == KEY_FN_S) {
            // Flash status bar
            lcd_draw_window(0, lcd_size_x-1, status_bar_y, lcd_size_y-1, GRAY);
            lcd_set_position(10, status_bar_y+5);
            lcd_draw_string_f((const uint8_t*)PSTR("Saving..."), YELLOW, 1);

            uint8_t save_ok = 1;

            if (file_exists) {
                FAT_delete_file(filename);
            }

            if (!FAT_create_file(filename)) {
                save_ok = 0;
            }

            if (save_ok) {
                // Write in 64-byte chunks
                uint32_t remaining = file_size;
                uint32_t offset    = 0;
                uint8_t  buf[64];
                while (remaining > 0) {
                    uint8_t chunk = (remaining > 64) ? 64 : (uint8_t)remaining;
                    ram_read(ram_addr + offset, buf, chunk);
                    if (!FAT_append_data(filename, buf, chunk)) {
                        save_ok = 0;
                        break;
                    }
                    offset    += chunk;
                    remaining -= chunk;
                }
            }

            file_exists = 1;   /* now it definitely exists on disk */
            dirty       = 0;

            // Show result
            lcd_draw_window(0, lcd_size_x-1, status_bar_y, lcd_size_y-1, GRAY);
            lcd_set_position(10, status_bar_y+5);
            if (save_ok)
                lcd_draw_string_f((const uint8_t*)PSTR("Saved!"), GREEN, 1);
            else
                lcd_draw_string_f((const uint8_t*)PSTR("Save FAILED!"), RED, 1);

            lcd_set_position(lcd_size_x - (20 * char_size_x), status_bar_y + 5);
            lcd_draw_string_f((const uint8_t*)PSTR("fn+S save  fn+Q quit"), WHITE, 1);

            DRAW_CHROME();
        }

        // ---- Quit  fn+q ----
        else if (key == KEY_FN_Q) {
            if (dirty) {
                // Ask for confirmation in the status bar
                lcd_draw_window(0, lcd_size_x-1, status_bar_y, lcd_size_y-1, GRAY);
                lcd_set_position(10, status_bar_y+5);
                lcd_draw_string_f((const uint8_t*)PSTR("Unsaved changes! fn+Q again to quit"), RED, 1);
                dirty = 0;  /* second fn+Q will pass through the else branch */
            } else {
                running = 0;
            }
        }

        // ---- Printable character insert ----
        else if (key >= 0x20 && key < 0x80) {
            if (file_size < MAX_FILE_SIZE) {
                uint32_t ls = editor_line_start(ram_addr, file_size, cursor_row);
                editor_insert_byte(ram_addr, &file_size, ls + cursor_col, key);
                cursor_col++;
                if (cursor_col >= scroll_col + max_cols) {
                    scroll_col = cursor_col - max_cols + 1;
                    REDRAW_CONTENT();
                } else {
                    // Fast path: only redraw current line
                    uint16_t _scr_row = cursor_row - scroll_row;
                    uint16_t _y = content_top + _scr_row * char_size_y;
                    uint32_t _ls = editor_line_start(ram_addr, file_size, cursor_row);
                    uint16_t _len= editor_line_length(ram_addr, file_size, _ls);
                    // clear line first
                    lcd_draw_window(10, lcd_size_x-1, _y, _y + char_size_y - 1, console_back_color);
                    uint8_t  _lb[LINE_BUF_SIZE];
                    uint16_t _ci = 0;
                    for (uint16_t _c = scroll_col;
                         _c < _len && _ci < max_cols && _ci < LINE_BUF_SIZE-1; _c++) {
                        _lb[_ci++] = ram_read_byte(_ls + _c);
                    }
                    _lb[_ci] = '\0';
                    if (_ci > 0) {
                        lcd_set_position(10, _y);
                        lcd_draw_string(_lb, console_text_color, console_char_size);
                    }
                }
                dirty = 1;
                DRAW_CHROME();
            }
        }

        // Draw cursor at new position
        DRAW_CURSOR(cursor_vis);
    }

    // ---- Cleanup ----
    #undef DRAW_CHROME
    #undef REDRAW_CONTENT
    #undef DRAW_CURSOR

    console_visible = 1;
    ram_free(ram_addr);
    wish_redraw();
}

// ================================================================
//  Helper implementations
// ================================================================

// Returns the RAM address of the start of the given logical line.
// If the line is beyond the last line, returns ram_addr + file_size.
static uint32_t editor_line_start(uint32_t base, uint32_t size, uint16_t line)
{
    uint32_t pos  = base;
    uint32_t end  = base + size;
    uint16_t cur  = 0;

    while (cur < line && pos < end) {
        if (ram_read_byte(pos++) == '\n')
            cur++;
    }
    return pos;
}

// Returns the number of characters on the line (excluding '\n').
static uint16_t editor_line_length(uint32_t base, uint32_t size, uint32_t line_start)
{
    uint32_t end = base + size;
    uint32_t pos = line_start;
    uint16_t len = 0;

    while (pos < end) {
        uint8_t b = ram_read_byte(pos++);
        if (b == '\n') break;
        len++;
    }
    return len;
}

// Returns total number of logical lines (minimum 1).
static uint16_t editor_count_lines(uint32_t base, uint32_t size)
{
    if (size == 0) return 1;

    uint32_t end   = base + size;
    uint32_t pos   = base;
    uint16_t lines = 1;

    while (pos < end) {
        if (ram_read_byte(pos++) == '\n')
            lines++;
    }
    // If the last byte IS a newline, that trailing empty line is real.
    return lines;
}

// Insert a single byte at `pos` (absolute RAM address).
// Shifts everything from pos..base+size one byte forward.
static void editor_insert_byte(uint32_t base, uint32_t *size, uint32_t pos, uint8_t b)
{
    if (*size >= MAX_FILE_SIZE) return;

    uint32_t end = base + *size;

    // Shift bytes one position to the right (walk backwards to avoid overlap)
    for (uint32_t i = end; i > pos; i--) {
        ram_write_byte(i, ram_read_byte(i - 1));
    }
    ram_write_byte(pos, b);
    (*size)++;
}

// Delete a single byte at `pos` (absolute RAM address).
// Shifts everything after pos one byte backward.
static void editor_delete_byte(uint32_t base, uint32_t *size, uint32_t pos)
{
    if (*size == 0) return;
    uint32_t end = base + *size;

    for (uint32_t i = pos; i < end - 1; i++) {
        ram_write_byte(i, ram_read_byte(i + 1));
    }
    (*size)--;
}
