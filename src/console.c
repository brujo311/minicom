/**
 * @file wish.c
 * @brief Wizard Shell (wish) — Core implementation
 *
 * Target : ATxmega128A3U @ 32 MHz
 * Toolchain: avr-gcc + avr-libc
 *
 * ── External RAM layout ──────────────────────────────────────────────────────
 *
 *  [ console_ram_start_addr ]
 *  ┌──────────────────────────────────────────────────┐
 *  │  line slot 0   : [len:1B][data:console_ram_line_size-1 B]  │
 *  │  line slot 1   : [len:1B][data:...]                        │
 *  │  ...                                              │
 *  │  line slot N-1                                    │
 *  └──────────────────────────────────────────────────┘
 *   N = console_ram_buffer_size / console_ram_line_size
 *
 *  console_stored_lines  : how many slots are currently occupied (0..N)
 *  console_view_top_line : index of the first line rendered on screen
 *
 * ── Screen layout ────────────────────────────────────────────────────────────
 *
 *  Row 0 .. (rows_on_screen-2) : output lines
 *  Row      (rows_on_screen-1) : prompt + input   (prompt_row)
 *
 *  rows_on_screen = lcd_size_y / char_size_y
 *  cols_per_row   = lcd_size_x / char_size_x
 *
 * ── Scrolling ────────────────────────────────────────────────────────────────
 *
 *  Vertical   : UP/DOWN arrows change console_view_top_line and redraw.
 *  Horizontal : LEFT/RIGHT arrows change console_input_col_offset and
 *               redraw the prompt row only.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 */

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
 
 /* =========================================================================
  * PROGMEM string table
  * All string literals the shell itself needs live here so SRAM is not used.
  * ========================================================================= */
 static const char PSTR_SHELL_NAME[]  PROGMEM = "wish";
 static const char PSTR_PROMPT_SEP[]  PROGMEM = " ";
 static const char PSTR_ROOT_CHAR[]   PROGMEM = "# ";
 static const char PSTR_USER_CHAR[]   PROGMEM = "$ ";
 static const char PSTR_ROOT_DIR[]    PROGMEM = "/";
 static const char PSTR_NEWLINE[]     PROGMEM = "\n";
 static const char PSTR_WELCOME1[]    PROGMEM = "########################################\n";
 static const char PSTR_WELCOME2[]    PROGMEM = "#                                      #\n";
 static const char PSTR_WELCOME3[]    PROGMEM = "#      WISH [ Wizard Shell ] v1.0      #\n";
 static const char PSTR_WELCOME4[]    PROGMEM = "#                                      #\n";
 static const char PSTR_WELCOME5[]    PROGMEM = "########################################\n";
 static const char PSTR_WELCOME6[]    PROGMEM = " \n";
 static const char PSTR_WELCOME7[]    PROGMEM = "Atmel ATxmega128A3U @ 32 Mhz\n";
 static const char PSTR_WELCOME8[]    PROGMEM = " \n";

 static const char PSTR_WELCOME13[]   PROGMEM = " \n";
 static const char PSTR_WELCOME14[]   PROGMEM = "Done...\n";
 static const char PSTR_WELCOME15[]   PROGMEM = "Use 'help' to see available command list\n";
 static const char PSTR_CMD_NOTFOUND[]PROGMEM = ": command not found\n";
 static const char PSTR_UNKNOWN_KEY[] PROGMEM = "";   /* suppress unknown keys silently */
 
 /* ls / cd strings */
 static const char PSTR_LS_NO_SD[]    PROGMEM = "ls: no SD card\n";
 static const char PSTR_LS_ERR[]      PROGMEM = "ls: error reading directory\n";
 static const char PSTR_LS_EMPTY[]    PROGMEM = "(empty)\n";
 static const char PSTR_LS_DIR_TAG[]  PROGMEM = "/";   /* appended after dir names */
 static const char PSTR_CD_USAGE[]    PROGMEM = "cd: usage: cd [dirname]\n";
 static const char PSTR_CD_NO_SD[]    PROGMEM = "cd: no SD card\n";
 static const char PSTR_CD_NOTFOUND[] PROGMEM = "cd: no such directory: ";
 static const char PSTR_CD_NOTADIR[]  PROGMEM = "cd: not a directory: ";
 static const char PSTR_CD_MAXDEPTH[] PROGMEM = "cd: max directory depth reached\n";
 static const char PSTR_CD_ERR[]      PROGMEM = "cd: error\n";
 
 /* =========================================================================
  * Configurable globals  (caller loads from EEPROM before wish_init)
  * ========================================================================= */
 uint16_t console_back_color      = BLACK;
 uint16_t console_text_color      = WHITE;
 uint16_t console_prompt_color    = MAGENTA;
 uint8_t  console_char_size       = 1;         /* scale factor for lcd_draw_char */
 uint32_t console_ram_start_addr  = 0;
 uint32_t console_ram_buffer_size = 65536UL;   /* 64 KB default (can be up to 256 KB) */
 uint8_t  console_ram_scroll_delete = 50;      /* lines to evict when buffer full */
 uint8_t  char_size_x             = 8;         /* pixel width  of one glyph cell  */
 uint8_t  char_size_y             = 10;        /* pixel height of one glyph cell  */
 
 /* Derived at init — do NOT set manually */
 uint8_t  console_ram_line_size   = 0;         /* 1 + cols_per_row                */
 uint16_t console_stored_lines    = 0;
 
 /* State flags */
 volatile uint8_t  console_active  = 0;
 volatile uint8_t  console_visible = 0;
 
 /* Prompt / input */
 uint8_t  console_prompt[64]        = "wish / $ ";
 uint8_t  console_prompt_size       = 9;
 uint8_t  console_input_buffer[256];
 uint8_t  console_input_index       = 0;
 uint8_t  console_root_mode         = 0;
 
 /* Scroll state */
 volatile uint8_t  prompt_row            = 0;   /* screen row of the prompt line */
 uint16_t          console_view_top_line = 0;   /* first RAM line visible         */
 uint8_t           console_input_col_offset = 0;/* horizontal scroll (chars)      */
 
 uint16_t _cursor_x = 0;
 uint16_t _cursor_y = 0;
 uint8_t _char_under_cursor = 0;
 
 /* =========================================================================
  * Path stack — tracks the current directory nesting for prompt building.
  *
  * Each cd into a directory pushes a name onto the stack; cd/cd.. pops one.
  * The actual FAT cluster navigation is handled by FAT_load_directory /
  * FAT_unload_directory — we only track the string names here.
  *
  * Storage strategy: the names live in the external RAM at a dedicated region
  * separate from the console output buffer so the two never interfere.
  *
  * External RAM path-stack layout (starts at wish_path_ram_addr):
  *   slot 0 : [len:1B][name: WISH_PATH_NAME_MAX bytes]
  *   slot 1 : ...
  *   ...
  *   slot WISH_PATH_MAX_DEPTH-1
  *
  * wish_path_depth tracks how many slots are occupied.
  * ========================================================================= */
 
 /* Maximum directory nesting supported (hard-coded, easy to raise). */
 #define WISH_PATH_MAX_DEPTH     8
 
 /* Max characters stored for a single directory name segment (FAT 8+3 = 11,
  * but leave headroom for longer names if the FAT layer ever supports them). */
 #define WISH_PATH_NAME_MAX      12   /* 11 chars + 1 length byte per slot  */
 
 /* External RAM base address for the path stack.
  * Place it immediately after the console output buffer.
  * Total size = WISH_PATH_MAX_DEPTH * WISH_PATH_NAME_MAX = 8*12 = 96 bytes.
  * Caller may override before wish_init() if the memory map differs.        */
 uint32_t wish_path_ram_addr =
     0 + 65536UL;   /* console_ram_start_addr + console_ram_buffer_size default */
 
 /* Current nesting depth (0 = root). */
 static uint8_t wish_path_depth = 0;
 
 /* ── Path-stack RAM helpers ─────────────────────────────────────────────── */
 
 /** Write a path name segment to the given stack slot in external RAM. */
 static void _path_stack_write(uint8_t slot, const uint8_t *name, uint8_t len)
 {
     uint32_t addr = wish_path_ram_addr + (uint32_t)slot * WISH_PATH_NAME_MAX;
     if (len > WISH_PATH_NAME_MAX - 1) len = WISH_PATH_NAME_MAX - 1;
     ram_write_byte(addr, len);
     if (len > 0) ram_write(addr + 1, (uint8_t *)name, len);
 }
 
 /** Read a path name segment from the given stack slot into out (null-terminated). */
 static uint8_t _path_stack_read(uint8_t slot, uint8_t *out)
 {
     uint32_t addr = wish_path_ram_addr + (uint32_t)slot * WISH_PATH_NAME_MAX;
     uint8_t  len  = ram_read_byte(addr);
     if (len > WISH_PATH_NAME_MAX - 1) len = WISH_PATH_NAME_MAX - 1;
     if (len > 0) ram_read(addr + 1, out, len);
     out[len] = 0;
     return len;
 }
 
 /** Push a directory name onto the path stack. Returns 0 on depth overflow. */
 static uint8_t _path_push(const uint8_t *name, uint8_t len)
 {
     if (wish_path_depth >= WISH_PATH_MAX_DEPTH) return 0;
     _path_stack_write(wish_path_depth, name, len);
     wish_path_depth++;
     return 1;
 }
 
 /** Pop the top directory name from the path stack. Returns 0 if already root. */
 static uint8_t _path_pop(void)
 {
     if (wish_path_depth == 0) return 0;
     wish_path_depth--;
     return 1;
 }
 
 /**
  * Rebuild wish_cwd and the console prompt from the current path stack.
  * Result looks like  "/"  at root  or  "/home/docs/"  after two cd's.
  */
 static void _path_rebuild_prompt(void)
 {
     static uint8_t path_buf[128];
     uint8_t        seg[WISH_PATH_NAME_MAX];
     uint8_t        pos = 0;
 
     path_buf[pos++] = '/';   /* always start with root slash */
 
     for (uint8_t i = 0; i < wish_path_depth; i++)
     {
         uint8_t slen = _path_stack_read(i, seg);
         if (pos + slen + 1 < sizeof(path_buf) - 1)
         {
             memcpy(path_buf + pos, seg, slen);
             pos += slen;
             path_buf[pos++] = '/';
         }
     }
     path_buf[pos] = 0;
 
     wish_set_prompt(path_buf);
 }
 
 /* =========================================================================
  * Static helpers — forward declarations
  * ========================================================================= */
 static uint16_t  _cols_per_row      (void);
 static uint16_t  _rows_on_screen    (void);
 static uint16_t  _max_stored_lines  (void);
 
 static void      _ram_write_line    (uint16_t slot, const uint8_t *data, uint8_t len);
 static uint8_t   _ram_read_line     (uint16_t slot, uint8_t *out_buf);
 
 static void      _evict_lines       (void);
 static void      _append_line       (const uint8_t *data, uint8_t len);
 static void      _commit_pending    (void);
 static void      _pending_append    (const uint8_t *data, uint8_t len);
 
 static void      _redraw_output     (void);
 static void      _redraw_prompt_row (void);
 static void      _clear_row         (uint8_t row);
 static void      _draw_char_at      (uint8_t col, uint8_t row, uint8_t c,
                                      uint16_t color);
 static void      _draw_str_at       (uint8_t col, uint8_t row,
                                      const uint8_t *str, uint8_t len,
                                      uint16_t color);
 
 static void      _scroll_view_up    (uint8_t n);
 static void      _scroll_view_down  (uint8_t n);
 static void      _scroll_view_to_bottom(void);
 
 static void      _handle_key        (uint8_t key);
 static void      _handle_enter      (void);
 static void      _process_command   (uint8_t *line);
 
 static void      _pgm_to_sram       (const char *pgm_src, uint8_t *dst,
                                      uint8_t max_len);
 
 /* =========================================================================
  * Inline geometry helpers
  * ========================================================================= */
 static inline uint16_t _cols_per_row(void)
 {
     return (uint16_t)(lcd_size_x / char_size_x);
 }
 
 static inline uint16_t _rows_on_screen(void)
 {
     return (uint16_t)(lcd_size_y / char_size_y);
 }
 
 static inline uint16_t _max_stored_lines(void)
 {
     if (console_ram_line_size == 0) return 0;
     return (uint16_t)(console_ram_buffer_size / console_ram_line_size);
 }
 
 /* =========================================================================
  * PROGMEM copy helper
  * ========================================================================= */
 static void _pgm_to_sram(const char *pgm_src, uint8_t *dst, uint8_t max_len)
 {
     uint8_t i = 0;
     uint8_t c;
     while (i < (max_len - 1))
     {
         c = pgm_read_byte(pgm_src + i);
         if (c == 0) break;
         dst[i] = c;
         i++;
     }
     dst[i] = 0;
 }
 
 /* =========================================================================
  * RAM line storage
  *
  * Slot address = console_ram_start_addr + slot * console_ram_line_size
  * Layout per slot:
  *   byte 0           : actual data length in bytes (0 = empty line)
  *   bytes 1 .. N-1   : character data (not null-terminated in RAM)
  * ========================================================================= */
 static void _ram_write_line(uint16_t slot, const uint8_t *data, uint8_t len)
 {
     uint32_t addr = console_ram_start_addr
                   + (uint32_t)slot * console_ram_line_size;
     ram_write_byte(addr, len);
     if (len > 0)
     {
         ram_write(addr + 1, (uint8_t *)data, len);
     }
 }
 
 /* Returns the actual string length stored in the slot. */
 static uint8_t _ram_read_line(uint16_t slot, uint8_t *out_buf)
 {
     uint32_t addr = console_ram_start_addr
                   + (uint32_t)slot * console_ram_line_size;
     uint8_t  len  = ram_read_byte(addr);
     if (len > 0)
     {
         ram_read(addr + 1, out_buf, len);
     }
     out_buf[len] = 0; /* null-terminate for callers */
     return len;
 }
 
 /* =========================================================================
  * Line eviction  (called when buffer is full)
  *
  * Deletes the oldest console_ram_scroll_delete lines by shifting all
  * remaining lines towards slot 0.
  * ========================================================================= */
 static void _evict_lines(void)
 {
     uint16_t del   = console_ram_scroll_delete;
     uint8_t  line_data_size = (uint8_t)(console_ram_line_size - 1);
     uint8_t  tmp[256];          /* max line data length = cols_per_row ≤ 255 */
 
     if (del > console_stored_lines) del = console_stored_lines;
 
     /* Shift slots [del .. stored_lines-1] to [0 .. stored_lines-del-1] */
     uint16_t dst_slot = 0;
     uint16_t src_slot = del;
 
     while (src_slot < console_stored_lines)
     {
         uint8_t len = _ram_read_line(src_slot, tmp);
         _ram_write_line(dst_slot, tmp, len);
         dst_slot++;
         src_slot++;
     }
 
     console_stored_lines -= del;
 
     /* Adjust view so it stays valid */
     if (console_view_top_line >= del)
         console_view_top_line -= del;
     else
         console_view_top_line = 0;
 }
 
 /* =========================================================================
  * Pending line buffer
  *
  * Fragments from console_write_d / console_write_f accumulate in a single
  * dedicated slot in external RAM until a '\n' is received.  On '\n' the
  * slot is committed to the permanent line store and the screen is updated.
  * A '\0' (end of a fragment call) does NOT flush — the caller sends '\n'
  * explicitly when the logical line is complete.
  *
  * Slot layout (identical to every other line slot):
  *   byte 0          : current accumulated length  (SRAM mirror: console_pending_len)
  *   bytes 1 .. N-1  : character data
  *
  * The pending slot occupies the LAST slot of the output buffer region so it
  * never collides with committed lines.  Effective committed-line capacity is
  * (console_ram_buffer_size / console_ram_line_size) - 1.
  *
  * Writing strategy: each console_write call assembles its fragment into a
  * local SRAM buffer and issues a single ram_write() call per segment —
  * no byte-at-a-time serial RAM traffic.
  * ========================================================================= */

 /* Pending slot base address — calculated in wish_init() once geometry is known. */
 static uint32_t _pending_slot_addr = 0;

 /* SRAM mirror of the pending slot length byte (avoids a RAM read each call). */
 static uint8_t  console_pending_len = 0;

 /* =========================================================================
  * _commit_pending  —  flush the pending slot into the permanent line store
  *
  * Reads the pending slot from external RAM in one ram_read() call, passes
  * the data to _append_line(), then clears the slot.
  * Called only when '\n' is encountered.
  * ========================================================================= */
 static void _commit_pending(void)
 {
     uint8_t staging[256];
     uint8_t len = console_pending_len;

     if (len > 0)
         ram_read(_pending_slot_addr + 1, staging, len);

     staging[len] = 0;
     _append_line(staging, len);

     console_pending_len = 0;
     ram_write_byte(_pending_slot_addr, 0);
 }

 /* =========================================================================
  * _pending_append  —  append a SRAM chunk to the pending slot
  *
  * Writes up to `len` bytes at the current pending offset using a single
  * ram_write() call.  If the pending slot would overflow the display width,
  * it is auto-committed first (visual line wrap without '\n'), then writing
  * continues.  No screen update — that only happens on '\n'.
  * ========================================================================= */
 static void _pending_append(const uint8_t *data, uint8_t len)
 {
     if (len == 0) return;

     uint8_t capacity = (uint8_t)(console_ram_line_size - 1);
     uint8_t cols     = (uint8_t)(_cols_per_row() & 0xFF);
     if (capacity > cols) capacity = cols;

     while (len > 0)
     {
         uint8_t room = (uint8_t)(capacity - console_pending_len);

         if (room == 0)
         {
             /* Slot full — auto-commit visual row, no screen update yet. */
             _commit_pending();
             room = capacity;
         }

         uint8_t take = (len <= room) ? len : room;

         ram_write(_pending_slot_addr + 1 + console_pending_len, (uint8_t *)data, take);
         console_pending_len += take;
         ram_write_byte(_pending_slot_addr, console_pending_len);

         data += take;
         len  -= take;
     }
 }

 /* =========================================================================
  * _append_line  —  write one fully-formed line segment into the slot store
  *
  * Splits at cols_per_row if wider than one display row.
  * Does NOT touch the screen.
  * ========================================================================= */
 static void _append_line(const uint8_t *data, uint8_t len)
 {
     if (!console_active) return;

     uint16_t cols   = _cols_per_row();
     uint16_t max    = _max_stored_lines();
     uint8_t  data_capacity = (uint8_t)(console_ram_line_size - 1);

     if (cols > data_capacity) cols = data_capacity;

     uint8_t offset = 0;

     /* Always store at least one slot (even for an empty line) */
     do
     {
         if (console_stored_lines >= max)
             _evict_lines();

         uint8_t chunk = (uint8_t)(len - offset);
         if (chunk > (uint8_t)cols) chunk = (uint8_t)cols;

         _ram_write_line(console_stored_lines, data + offset, chunk);
         console_stored_lines++;
         offset += chunk;

     } while (offset < len);
 }

 /* =========================================================================
  * console_write_d  —  write a SRAM string fragment to the console output
  *
  * Scans for '\n' boundaries.  Each segment between boundaries is written to
  * the pending slot in one ram_write() call.  A '\n' commits the pending
  * slot and triggers a screen update.  '\r' is silently stripped.  The
  * trailing '\0' does NOT flush — include '\n' in the string to close the
  * line.
  * ========================================================================= */
 void console_write_d(uint8_t *text)
 {
     if (!console_active) return;

     uint8_t *p   = text;
     uint8_t *seg = p;

     while (*p)
     {
         uint8_t c = *p;

         if (c == '\r')
         {
             uint8_t seg_len = (uint8_t)(p - seg);
             if (seg_len > 0) _pending_append(seg, seg_len);
             p++;
             seg = p;
             continue;
         }

         if (c == '\n')
         {
             uint8_t seg_len = (uint8_t)(p - seg);
             if (seg_len > 0) _pending_append(seg, seg_len);
             p++;
             seg = p;

             _commit_pending();

             if (console_visible)
             {
                 _scroll_view_to_bottom();
                 _redraw_output();
                 _redraw_prompt_row();
             }
             continue;
         }

         p++;
     }

     /* Append tail after last '\n' (or whole string if no '\n') — no flush. */
     uint8_t tail_len = (uint8_t)(p - seg);
     if (tail_len > 0) _pending_append(seg, tail_len);
 }

 /* =========================================================================
  * console_write_f  —  write a PROGMEM string fragment to the console output
  *
  * Same '\n' semantics as console_write_d.  Copies PROGMEM into a local
  * SRAM buffer (up to 255 bytes at a time) then delegates to console_write_d
  * so the bulk-write logic is not duplicated.  Handles PROGMEM strings of
  * any length through chunking.
  * ========================================================================= */
 void console_write_f(const char *text)
 {
     if (!console_active) return;

     uint8_t  sram_buf[256];
     uint16_t offset = 0;

     while (1)
     {
         uint8_t chunk = 0;
         while (chunk < 255)
         {
             uint8_t c = pgm_read_byte(text + offset + chunk);
             sram_buf[chunk] = c;
             chunk++;
             if (c == '\0') break;
         }

         console_write_d(sram_buf);

         if (sram_buf[chunk - 1] == '\0') break;

         offset += chunk;
     }
 }

 void console_number(uint32_t number, uint8_t base, uint8_t * prefix, uint8_t * sufix)
{
	uint8_t a[16];
	uint8_t b;

	if(prefix != NULL) console_write_d(prefix);

	ultoa(number, a, base);
	console_write_d(a);

	if(sufix != NULL) console_write_d(sufix);
}
 
 /* =========================================================================
  * Prompt construction
  * ========================================================================= */
 void wish_set_prompt(uint8_t *cwd)
 {
     /* Update internal CWD copy */
     //strncpy((char *)wish_cwd, (char *)cwd, sizeof(wish_cwd) - 1);
     //wish_cwd[sizeof(wish_cwd) - 1] = 0;
 
     /*
      * Build:  "wish <cwd> $ "  or  "wish <cwd> # "
      *
      * Using only progmem fragments + strcat-style assembly so we do not
      * embed any string literals directly in .data/.bss.
      */
     uint8_t tmp[8];
     uint8_t idx = 0;
 
     /* "wish" */
     _pgm_to_sram(PSTR_SHELL_NAME, tmp, sizeof(tmp));
     uint8_t name_len = (uint8_t)strlen((char *)tmp);
     memcpy(console_prompt + idx, tmp, name_len);
     idx += name_len;
 
     /* " " */
     console_prompt[idx++] = ' ';
 
     /* cwd */
     uint8_t cwd_len = (uint8_t)strlen((char *)cwd);
     if (idx + cwd_len >= 60) cwd_len = (uint8_t)(60 - idx); /* clamp */
     memcpy(console_prompt + idx, cwd, cwd_len);
     idx += cwd_len;
 
     /* " " */
     console_prompt[idx++] = ' ';
 
     /* "$ " or "# " */
     if (console_root_mode)
     {
         console_prompt[idx++] = '#';
     }
     else
     {
         console_prompt[idx++] = '$';
     }
     console_prompt[idx++] = ' ';
     console_prompt[idx]   = 0;
 
     console_prompt_size = idx;
 
     if (console_visible) _redraw_prompt_row();
 }
 
 /* =========================================================================
  * Low-level screen drawing helpers
  * ========================================================================= */
 
 /** Clear one character-row to the background colour. */
 static void _clear_row(uint8_t row)
 {
     uint16_t y0 = (uint16_t)row * char_size_y;
     uint16_t y1 = y0 + char_size_y - 1;
     lcd_draw_window(0, lcd_size_x - 1, y0, y1, console_back_color);
 }
 
 /** Draw a single character at (col, row) in the given colour. */
 static void _draw_char_at(uint8_t col, uint8_t row, uint8_t c, uint16_t color)
 {
     uint16_t x = (uint16_t)col * char_size_x;
     uint16_t y = (uint16_t)row * char_size_y;
     lcd_set_position(x, y);
     lcd_draw_char(c, color, console_char_size);
 }
 
 /**
  * Draw up to len characters of str starting at (col, row).
  * Stops at the right edge of the screen automatically.
  */
 static void _draw_str_at(uint8_t col, uint8_t row, const uint8_t *str,
                          uint8_t len, uint16_t color)
 {
     uint16_t cols = _cols_per_row();
     uint8_t  c    = col;
 
     for (uint8_t i = 0; i < len; i++)
     {
         if (c >= cols) break;
         _draw_char_at(c, row, str[i], color);
         c++;
     }
 }
 
 /* =========================================================================
  * Scroll helpers
  * ========================================================================= */
 
 static void _scroll_view_up(uint8_t n)
 {
     if (console_view_top_line == 0) return;
     if (n > console_view_top_line)
         console_view_top_line = 0;
     else
         console_view_top_line -= n;
 
     if (console_visible)
     {
         _redraw_output();
         _redraw_prompt_row();
     }
 }
 
 static void _scroll_view_down(uint8_t n)
 {
     uint16_t output_rows = (uint16_t)(_rows_on_screen() - 1);
     uint16_t max_top;
 
     if (console_stored_lines > output_rows)
         max_top = console_stored_lines - output_rows;
     else
         max_top = 0;
 
     console_view_top_line += n;
     if (console_view_top_line > max_top)
         console_view_top_line = max_top;
 
     if (console_visible)
     {
         _redraw_output();
         _redraw_prompt_row();
     }
 }
 
 /** Snap the view so that the very last output line is visible. */
 static void _scroll_view_to_bottom(void)
 {
     uint16_t output_rows = (uint16_t)(_rows_on_screen() - 1);
     if (console_stored_lines > output_rows)
         console_view_top_line = console_stored_lines - output_rows;
     else
         console_view_top_line = 0;
 }
 
 /* =========================================================================
  * Full output redraw
  *
  * Renders rows 0 .. (rows_on_screen-2) from RAM starting at
  * console_view_top_line.
  * ========================================================================= */
 static void _redraw_output(void)
 {
     if (!console_visible) return;
 
     uint16_t output_rows = (uint16_t)(_rows_on_screen() - 1);
     uint8_t  line_buf[256];
 
     for (uint16_t row = 0; row < output_rows; row++)
     {
         _clear_row((uint8_t)row);
 
         uint16_t line_idx = console_view_top_line + row;
         if (line_idx >= console_stored_lines) break; /* no more lines */
 
         uint8_t len = _ram_read_line(line_idx, line_buf);
         if (len > 0)
         {
             _draw_str_at(0, (uint8_t)row, line_buf, len, console_text_color);
         }
     }
 }
 
 /* =========================================================================
  * Prompt row redraw
  *
  * The last screen row always shows:
  *   [prompt][input_visible_window]
  *
  * Horizontal scrolling of the input is handled via console_input_col_offset.
  * The prompt is always drawn from column 0; the input follows immediately
  * after.  When the combined prompt+input exceeds the screen width the input
  * portion scrolls.
  * ========================================================================= */

 static void _redraw_cursor(uint8_t cursor)
 {
    if(cursor)
    {
        lcd_draw_window(_cursor_x, _cursor_x + char_size_x - 1, _cursor_y, _cursor_y + char_size_y - 1, console_text_color);
        lcd_set_position(_cursor_x, _cursor_y);
        if(_char_under_cursor) lcd_draw_char(_char_under_cursor, console_back_color, console_char_size);
    }
    if(!cursor)
    {
        lcd_draw_window(_cursor_x, _cursor_x + char_size_x - 1, _cursor_y, _cursor_y + char_size_y - 1, console_back_color);
        lcd_set_position(_cursor_x, _cursor_y);
        if(_char_under_cursor) lcd_draw_char(_char_under_cursor, console_text_color, console_char_size);
    }
 }

 static void _redraw_prompt_row(void)
 {
     if (!console_visible) return;
 
     uint16_t cols = _cols_per_row();
     uint8_t  row  = (uint8_t)(_rows_on_screen() - 1);
     prompt_row    = row;
 
     /* Clear the whole row */
     _clear_row(row);
 
     /* Draw prompt (never scrolls — always anchored at left) */
     uint8_t  col  = 0;
     uint8_t  plen = console_prompt_size;
     if (plen > cols) plen = (uint8_t)cols;
 
     _draw_str_at(col, row, console_prompt, plen, console_prompt_color);
     col += plen;
 
     if (col >= cols) return; /* no room for input */
 
     /* Available columns for the input text */
     uint8_t  avail = (uint8_t)(cols - col);
 
     /* Apply horizontal scroll offset to input */
     uint8_t  istart = console_input_col_offset;
     uint8_t  ilen   = console_input_index;
 
     if (istart > ilen) istart = ilen; /* clamp */
 
     uint8_t visible_len = (uint8_t)(ilen - istart);
     if (visible_len > avail) visible_len = avail;
 
     if (visible_len > 0)
     {
         _draw_str_at(col, row, console_input_buffer + istart,
                      visible_len, console_text_color);
     }

    /* Draw cursor block at current cursor position */
     uint8_t cursor_screen_col = (uint8_t)(col + (console_input_index - istart));
     if (cursor_screen_col < cols)
     {
         /* Draw a filled block as cursor — invert colors at cursor cell */
         _cursor_x = (uint16_t)cursor_screen_col * char_size_x;
         _cursor_y = (uint16_t)row * char_size_y - 1;
 
         /* If there is a character under the cursor draw it in background color */
         if (console_input_index < ilen)
         {
             _char_under_cursor = console_input_buffer[console_input_index];
         }
     }
 }
 
 /* =========================================================================
  * Key handling
  * ========================================================================= */
 
 /** Ensure the cursor stays within the visible input window. */
 static void _ensure_cursor_visible(void)
 {
     uint16_t cols  = _cols_per_row();
     uint8_t  avail = (uint8_t)(cols - console_prompt_size);
     if (avail == 0) avail = 1;
 
     /* Scroll right if cursor is beyond the visible window */
     while (console_input_index > (uint8_t)(console_input_col_offset + avail - 1))
         console_input_col_offset++;
 
     /* Scroll left if cursor is before the visible window */
     while (console_input_col_offset > 0
            && console_input_index < console_input_col_offset)
         console_input_col_offset--;
 }
 
 static void _handle_key(uint8_t key)
 {
     /* --- Arrow keys --- */
     if (key == KEY_UP)
     {
         _scroll_view_up(1);
         return;
     }
     if (key == KEY_DOWN)
     {
         _scroll_view_down(1);
         return;
     }
     if (key == KEY_LEFT)
     {
         if (console_input_index > 0)
         {
             console_input_index--;
             _ensure_cursor_visible();
             if (console_visible) _redraw_prompt_row();
         }
         return;
     }
     if (key == KEY_RIGHT)
     {
         uint8_t ilen = (uint8_t)strlen((char *)console_input_buffer);
         if (console_input_index < ilen)
         {
             console_input_index++;
             _ensure_cursor_visible();
             if (console_visible) _redraw_prompt_row();
         }
         return;
     }
 
     /* --- Enter --- */
     if (key == KEY_ENTER || key == KEY_ENTER2)
     {
         _handle_enter();
         return;
     }
 
     /* --- Backspace --- */
     if (key == KEY_BACKSPACE)
     {
         _redraw_cursor(0);
         if (console_input_index > 0)
         {
             /* Shift everything left of cursor */
             uint8_t end = (uint8_t)strlen((char *)console_input_buffer);
             for (uint8_t i = console_input_index - 1; i < end - 1; i++)
                 console_input_buffer[i] = console_input_buffer[i + 1];
             console_input_buffer[end - 1] = 0;
             console_input_index--;
             _ensure_cursor_visible();
             if (console_visible) _redraw_prompt_row();
         }
         return;
     }
 
     /* --- Delete --- */
     if (key == KEY_DEL)
     {
         _redraw_cursor(0);
         uint8_t end = (uint8_t)strlen((char *)console_input_buffer);
         if (console_input_index < end)
         {
             for (uint8_t i = console_input_index; i < end - 1; i++)
                 console_input_buffer[i] = console_input_buffer[i + 1];
             console_input_buffer[end - 1] = 0;
             if (console_visible) _redraw_prompt_row();
         }
         return;
     }
 
     /* --- Printable ASCII --- */
     if (key >= 0x20 && key <= 0x7E)
     {
         _redraw_cursor(0);
         uint8_t end = (uint8_t)strlen((char *)console_input_buffer);
         if (end < 254) /* leave 1 byte for null terminator */
         {
             /* Insert at cursor position */
             for (uint8_t i = end; i > console_input_index; i--)
                 console_input_buffer[i] = console_input_buffer[i - 1];
             console_input_buffer[console_input_index] = key;
             console_input_buffer[end + 1] = 0;
             console_input_index++;
             _ensure_cursor_visible();
             if (console_visible) _redraw_prompt_row();
         }
     }
 }
 
 /* =========================================================================
  * Enter key — echo the line to output, process command, reset input
  * ========================================================================= */
 static void _handle_enter(void)
 {
     /* Echo the full prompt + input line to the output RAM */
     if (console_active)
     {
         /* Build echo line: prompt + input */
         uint8_t echo[256 + 64];
         uint8_t elen = 0;
 
         memcpy(echo, console_prompt, console_prompt_size);
         elen = console_prompt_size;
 
         uint8_t ilen = (uint8_t)strlen((char *)console_input_buffer);
         if (elen + ilen < sizeof(echo))
         {
             memcpy(echo + elen, console_input_buffer, ilen);
             elen += ilen;
         }
         echo[elen] = '\n';
         echo[elen + 1] = 0;
         console_write_d(echo);
     }
 
     /* Process the command */
     _process_command(console_input_buffer);
 
     /* Clear input buffer */
     memset(console_input_buffer, 0, sizeof(console_input_buffer));
     console_input_index      = 0;
     console_input_col_offset = 0;
 
     /* Snap view to bottom and refresh */
     if (console_visible)
     {
         _scroll_view_to_bottom();
         _redraw_output();
         _redraw_prompt_row();
     }
 }
 
 /* =========================================================================
  * Command dispatcher
  *
  * Parses argv[0] and dispatches.  All built-in commands live here; external
  * commands are expected to be registered via function pointers (see table at
  * end of file) or implemented by the caller linking this translation unit.
  *
  * ─── Argument parsing ───────────────────────────────────────────────────────
  * argv[0] is the command name; argv[1..WISH_MAX_ARGS] are the arguments.
  * Fields are space-separated.  Quoted strings are not yet supported.
  * ========================================================================= */
 #define WISH_MAX_ARGS   8
 
 /* Typedef for a command handler */
 typedef void (*wish_cmd_fn_t)(uint8_t argc, uint8_t *argv[]);
 
 /* Forward declarations of built-in handlers */
 static void _cmd_clear (uint8_t argc, uint8_t *argv[]);
 static void _cmd_echo  (uint8_t argc, uint8_t *argv[]);
 static void _cmd_help  (uint8_t argc, uint8_t *argv[]);
 static void _cmd_ls    (uint8_t argc, uint8_t *argv[]);
 static void _cmd_cd    (uint8_t argc, uint8_t *argv[]);
 static void _cmd_mem_check (uint8_t argc, uint8_t *argv[]);
 static void _cmd_ram_stat(uint8_t argc, uint8_t *argv[]);
 
 /* ── Command table ──────────────────────────────────────────────────────── */
 typedef struct
 {
     const char      *name_pgm;   /* PROGMEM command name string */
     wish_cmd_fn_t    handler;
 } wish_cmd_entry_t;
 
 static const char CMD_CLEAR[]      PROGMEM = "clear";
 static const char CMD_ECHO[]       PROGMEM = "echo";
 static const char CMD_HELP[]       PROGMEM = "help";
 static const char CMD_LS[]         PROGMEM = "ls";
 static const char CMD_CD[]         PROGMEM = "cd";
 static const char CMD_MEM[]        PROGMEM = "memchk";
 static const char CMD_BITMAP[]     PROGMEM = "bitmap";
 static const char CMD_TEXTVW[]     PROGMEM = "text";
 static const char CMD_RAMSTAT[]    PROGMEM = "ramstat";
 static const char CMD_MKFILE[]     PROGMEM = "mkfile";
 static const char CMD_WFILE[]      PROGMEM = "wfile";
 
 /* Sentinel-terminated.  Add new built-ins here. */
 static const wish_cmd_entry_t wish_cmd_table[] PROGMEM =
 {
     { CMD_CLEAR,   _cmd_clear            },
     { CMD_ECHO,    _cmd_echo             },
     { CMD_HELP,    _cmd_help             },
     { CMD_LS,      _cmd_ls               },
     { CMD_CD,      _cmd_cd               },
     { CMD_MEM,     _cmd_mem_check        },
     { CMD_BITMAP,  pgm_show_bitmap       },
     { CMD_TEXTVW,  pgm_file_reader       },
     { CMD_RAMSTAT, _cmd_ram_stat         },
     { CMD_MKFILE,  pgm_mk_file           },
     { CMD_WFILE,   pgm_app_file          },
     { 0,         0                       }  /* sentinel */
 };
 
 /**
  * @brief  Parse line into argc/argv and dispatch to handler.
  *         argv[i] point into a local copy — do NOT store across frames.
  */
 static void _process_command(uint8_t *line)
 {
     static uint8_t  line_copy[256];
     static uint8_t *argv[WISH_MAX_ARGS + 1];
 
     /* Copy line so we can tokenise in-place */
     strncpy((char *)line_copy, (char *)line, sizeof(line_copy) - 1);
     line_copy[sizeof(line_copy) - 1] = 0;
 
     uint8_t argc = 0;
     uint8_t *p   = line_copy;
 
     /* Tokenise on spaces */
     while (*p && argc <= WISH_MAX_ARGS)
     {
         /* Skip leading spaces */
         while (*p == ' ') p++;
         if (!*p) break;
 
         argv[argc++] = p;
 
         /* Find end of token */
         while (*p && *p != ' ') p++;
         if (*p == ' ') { *p = 0; p++; }
     }
     argv[argc] = 0;
 
     if (argc == 0) return; /* empty line */
 
     /* Lookup in PROGMEM table */
     uint8_t entry_idx = 0;
     while (1)
     {
         /*
          * pgm_read_word() reads a 16-bit pointer from PROGMEM on AVR.
          * Cast to the pointer type after reading.
          */
         const char *name_pgm = (const char *)(uint16_t)pgm_read_word(
                                     &wish_cmd_table[entry_idx].name_pgm);
         if (name_pgm == 0) break; /* sentinel */
 
         wish_cmd_fn_t fn = (wish_cmd_fn_t)(uint16_t)pgm_read_word(
                                     &wish_cmd_table[entry_idx].handler);
 
         /* Compare argv[0] against PROGMEM name */
         uint8_t match = 1;
         uint8_t i     = 0;
         while (1)
         {
             uint8_t nc = pgm_read_byte(name_pgm + i);
             uint8_t ac = argv[0][i];
             if (nc != ac) { match = 0; break; }
             if (nc == 0 && ac == 0) break;
             i++;
         }
 
         if (match)
         {
             fn(argc, argv);
             return;
         }
         entry_idx++;
     }
 
     /* Unknown command — print error */
     uint8_t err[280];
     uint8_t epos = 0;
     uint8_t name_len = (uint8_t)strlen((char *)argv[0]);
     memcpy(err, argv[0], name_len);
     epos = name_len;
 
     /* Append PROGMEM " : command not found" */
     uint8_t suffix[32];
     _pgm_to_sram(PSTR_CMD_NOTFOUND, suffix, sizeof(suffix));
     uint8_t slen = (uint8_t)strlen((char *)suffix);
     memcpy(err + epos, suffix, slen);
     epos += slen;
     err[epos] = 0;
 
     console_write_d(err);
 }
 
 /* =========================================================================
  * Built-in command implementations
  * ========================================================================= */
 
 static void _cmd_clear(uint8_t argc, uint8_t *argv[])
 {
     (void)argc; (void)argv;
     console_stored_lines    = 0;
     console_view_top_line   = 0;
     console_pending_len     = 0;
     ram_write_byte(_pending_slot_addr, 0);
     if (console_visible)
     {
         lcd_clear_screen(console_back_color);
         _redraw_prompt_row();
     }
 }
 
 static void _cmd_echo(uint8_t argc, uint8_t *argv[])
 {
     if (argc < 2) { console_write_d((uint8_t *)""); return; }
 
     /* Re-join argv[1..] with spaces into a single output line */
     uint8_t buf[256];
     uint8_t pos = 0;
 
     for (uint8_t i = 1; i < argc && pos < 254; i++)
     {
         uint8_t alen = (uint8_t)strlen((char *)argv[i]);
         if (pos + alen >= 254) alen = (uint8_t)(254 - pos);
         memcpy(buf + pos, argv[i], alen);
         pos += alen;
         if (i < argc - 1 && pos < 254) buf[pos++] = ' ';
     }
     buf[pos] = 0;
     console_write_d(buf);
 }
 
 static const char HELP_LINE1[] PROGMEM = "wish - Wizard Shell\n";
 static const char HELP_LINE2[] PROGMEM = "Built-in commands: clear, echo, help, ls, cd\n";
 static const char HELP_LINE3[] PROGMEM = "UP/DOWN: scroll output  LEFT/RIGHT: move cursor\n";
 static const char HELP_LINE4[] PROGMEM = "cd [dir] : enter dir  |  cd : go up one level\n";
 
 static void _cmd_help(uint8_t argc, uint8_t *argv[])
 {
     (void)argc; (void)argv;
     console_write_f(HELP_LINE1);
     console_write_f(HELP_LINE2);
     console_write_f(HELP_LINE3);
     console_write_f(HELP_LINE4);
 }
 
 /* =========================================================================
  * FAT helpers
  * ========================================================================= */
 
 /**
  * Convert a raw FAT 8.3 name (11 bytes, space-padded, no dot) into a
  * printable null-terminated string.
  *
  * FAT stores "README  TXT" — we want "README.TXT".
  * Directories have no extension so we just strip trailing spaces.
  *
  * @param raw   11-byte FAT name field (NOT null-terminated).
  * @param out   Output buffer, must be >= 13 bytes  (8 + '.' + 3 + NUL).
  * @return      Length of the resulting string.
  */
 static uint8_t _fat_name_to_str(const uint8_t *raw, uint8_t *out)
 {
     uint8_t pos = 0;
 
     /* Base name: up to 8 chars, strip trailing spaces */
     uint8_t base_end = 8;
     while (base_end > 0 && raw[base_end - 1] == ' ') base_end--;
 
     for (uint8_t i = 0; i < base_end; i++)
         out[pos++] = raw[i];
 
     /* Extension: up to 3 chars, strip trailing spaces */
     uint8_t ext_end = 3;
     while (ext_end > 0 && raw[8 + ext_end - 1] == ' ') ext_end--;
 
     if (ext_end > 0)
     {
         out[pos++] = '.';
         for (uint8_t i = 0; i < ext_end; i++)
             out[pos++] = raw[8 + i];
     }
 
     out[pos] = 0;
     return pos;
 }
 
 /**
  * Write a small decimal number into a buffer.
  * Returns the number of characters written (no null terminator added).
  */
 static uint8_t _uint32_to_str(uint32_t val, uint8_t *out)
 {
     uint8_t  tmp[10];
     uint8_t  i = 0;
 
     if (val == 0) { out[0] = '0'; return 1; }
 
     while (val > 0)
     {
         tmp[i++] = (uint8_t)('0' + (val % 10));
         val /= 10;
     }
 
     /* Reverse */
     for (uint8_t j = 0; j < i; j++)
         out[j] = tmp[i - 1 - j];
 
     return i;
 }
 
 /* =========================================================================
  * ls — list current directory
  *
  * Calls FAT_count_files_in_directory(NULL) then iterates with
  * FAT_get_file_name(index, buf).  Each 32-byte entry is interpreted as a
  * dir_Structure; attribute byte is at offset 11 (0-based).
  *
  * Output per entry (one line each):
  *   Directories :  "  [DIRNAME/]"
  *   Files       :  "  FILENAME.EXT        12345"
  * ========================================================================= */
 
 /* Attribute bit for subdirectory (standard FAT). */
 #define FAT_ATTR_DIRECTORY  0x10
 
 /* Byte offset of the attrib field inside dir_Structure. */
 #define FAT_DIRENTRY_ATTRIB_OFFSET   11
 
 /* Byte offset of fileSize field (unsigned long, 4 bytes, little-endian). */
 #define FAT_DIRENTRY_FILESIZE_OFFSET 28
 
 static void _cmd_ls(uint8_t argc, uint8_t *argv[])
 {
     (void)argc; (void)argv;
 
     uint16_t file_count = 0;
     uint8_t  entry_buf[32];          /* raw dir_Structure from FAT */
     uint8_t  name_str[14];           /* formatted printable name   */
     uint8_t  line_buf[64];           /* assembled output line      */
 
     /* Count entries in the current working directory. */
     if (!FAT_count_files_in_directory(NULL, &file_count))
     {
         /* Check for the no-SD-card error specifically. */
         if (FAT_error_log == FAT_ERR_NO_SD_CARD)
             console_write_f(PSTR_LS_NO_SD);
         else
             console_write_f(PSTR_LS_ERR);
         return;
     }
 
     if (file_count == 0)
     {
         console_write_f(PSTR_LS_EMPTY);
         return;
     }
 
     /*
      * FAT_get_file_name uses 1-based indexing (as seen in the test code).
      * Iterate from 1 to file_count inclusive.
      */
     for (uint16_t idx = 1; idx <= file_count; idx++)
     {
         if (!FAT_get_file_name((uint16_t)idx, entry_buf))
             continue;   /* skip unreadable entries silently */
 
         uint8_t attrib   = entry_buf[FAT_DIRENTRY_ATTRIB_OFFSET];
         uint8_t is_dir   = (attrib & FAT_ATTR_DIRECTORY) ? 1 : 0;
         uint8_t name_len = _fat_name_to_str(entry_buf, name_str);
 
         uint8_t lpos = 0;
 
         /* Leading two spaces for readability. */
         line_buf[lpos++] = ' ';
         line_buf[lpos++] = ' ';
 
         if (is_dir)
         {
             /* Directories: show as  "  DIRNAME/"  in a distinct colour
              * would be nice, but console_write_d uses a single colour.
              * We prefix with '[' and suffix with ']' to visually separate. */
             line_buf[lpos++] = '[';
             memcpy(line_buf + lpos, name_str, name_len);
             lpos += name_len;
             line_buf[lpos++] = ']';
         }
         else
         {
             /* Files: show name then right-aligned file size.
              * Format: "  NAME.EXT       <size>"
              * We pad with spaces to a fixed column then append the size.  */
             memcpy(line_buf + lpos, name_str, name_len);
             lpos += name_len;
 
             /* Pad name out to column 16 (2 indent + 13 name = 15 max, pad to 18). */
             while (lpos < 18) line_buf[lpos++] = ' ';
 
             /* Read file size (uint32, little-endian, offset 28 in dir_Structure). */
             uint32_t fsize =  (uint32_t)entry_buf[FAT_DIRENTRY_FILESIZE_OFFSET]
                            | ((uint32_t)entry_buf[FAT_DIRENTRY_FILESIZE_OFFSET + 1] << 8)
                            | ((uint32_t)entry_buf[FAT_DIRENTRY_FILESIZE_OFFSET + 2] << 16)
                            | ((uint32_t)entry_buf[FAT_DIRENTRY_FILESIZE_OFFSET + 3] << 24);
 
             uint8_t size_str[12];
             uint8_t size_len = _uint32_to_str(fsize, size_str);
             memcpy(line_buf + lpos, size_str, size_len);
             lpos += size_len;
         }
 
         line_buf[lpos] = 0;
         console_write_d(line_buf);
         console_write_f(PSTR_NEWLINE);
     }
 }
 
 /* =========================================================================
  * cd — change directory
  *
  * Usage:
  *   cd          — go up one level  (equivalent to cd ..)
  *   cd ..       — go up one level
  *   cd dirname  — enter dirname inside the current directory
  *
  * The FAT layer keeps its own working-cluster state; we only need to
  * call FAT_load_directory / FAT_unload_directory and keep the name stack
  * in sync for prompt reconstruction.
  * ========================================================================= */
 static void _cmd_cd(uint8_t argc, uint8_t *argv[])
 {
     /* ── No argument or ".." → go up one level ──────────────────────── */
     if (argc < 2 || (argv[1][0] == '.' && argv[1][1] == '.' && argv[1][2] == 0))
     {
         if (wish_path_depth == 0)
             return;   /* already at root, silently do nothing like bash */
 
         if (!FAT_unload_directory())
         {
             console_write_f(PSTR_CD_NO_SD);
             return;
         }
 
         _path_pop();
         _path_rebuild_prompt();
         return;
     }
 
     /* ── Argument is "." → stay put ─────────────────────────────────── */
     if (argv[1][0] == '.' && argv[1][1] == 0)
         return;
 
     /* ── Normal directory name ───────────────────────────────────────── */
     uint8_t *dirname = argv[1];
 
     /* Check depth limit before doing any FAT work. */
     if (wish_path_depth >= WISH_PATH_MAX_DEPTH)
     {
         console_write_f(PSTR_CD_MAXDEPTH);
         return;
     }
 
     /*
      * Verify the target exists and is a directory by scanning the current
      * directory's file list and matching the name.
      *
      * We compare the requested name against the formatted printable name
      * (_fat_name_to_str output) so the user types "home" not "HOME       ".
      * The comparison is case-insensitive to be forgiving on FAT volumes
      * where everything is stored upper-case.
      */
     uint16_t file_count = 0;
     uint8_t  entry_buf[32];
     uint8_t  name_str[14];
     uint8_t  found       = 0;
     uint8_t  found_is_dir = 0;
 
     if (!FAT_count_files_in_directory(NULL, &file_count))
     {
         console_write_f((FAT_error_log == FAT_ERR_NO_SD_CARD)
                         ? PSTR_CD_NO_SD : PSTR_CD_ERR);
         return;
     }
 
     for (uint16_t idx = 1; idx <= file_count; idx++)
     {
         if (!FAT_get_file_name((uint16_t)idx, entry_buf))
             continue;
 
         _fat_name_to_str(entry_buf, name_str);
 
         /* Case-insensitive comparison. */
         uint8_t match = 1;
         uint8_t i     = 0;
         while (1)
         {
             uint8_t a = dirname[i];
             uint8_t b = name_str[i];
 
             /* Convert both to upper-case for comparison. */
             if (a >= 'a' && a <= 'z') a -= 32;
             if (b >= 'a' && b <= 'z') b -= 32;
 
             if (a != b) { match = 0; break; }
             if (a == 0 && b == 0)    break;   /* both ended — equal */
             i++;
         }
 
         if (match)
         {
             found        = 1;
             found_is_dir = (entry_buf[FAT_DIRENTRY_ATTRIB_OFFSET] & FAT_ATTR_DIRECTORY) ? 1 : 0;
             break;
         }
     }
 
     /* Entry not found at all. */
     if (!found)
     {
         /* Build error message: "cd: no such directory: <dirname>" */
         uint8_t  msg[80];
         uint8_t  mpos = 0;
         uint8_t  pfx[32];
 
         _pgm_to_sram(PSTR_CD_NOTFOUND, pfx, sizeof(pfx));
         uint8_t pfx_len = (uint8_t)strlen((char *)pfx);
         memcpy(msg, pfx, pfx_len);
         mpos = pfx_len;
 
         uint8_t dlen = (uint8_t)strlen((char *)dirname);
         if (mpos + dlen < sizeof(msg) - 1)
         {
             memcpy(msg + mpos, dirname, dlen);
             mpos += dlen;
         }
         msg[mpos] = 0;
         console_write_d(msg);
         return;
     }
 
     /* Found but it's a file, not a directory. */
     if (!found_is_dir)
     {
         uint8_t  msg[80];
         uint8_t  mpos = 0;
         uint8_t  pfx[32];
 
         _pgm_to_sram(PSTR_CD_NOTADIR, pfx, sizeof(pfx));
         uint8_t pfx_len = (uint8_t)strlen((char *)pfx);
         memcpy(msg, pfx, pfx_len);
         mpos = pfx_len;
 
         uint8_t dlen = (uint8_t)strlen((char *)dirname);
         if (mpos + dlen < sizeof(msg) - 1)
         {
             memcpy(msg + mpos, dirname, dlen);
             mpos += dlen;
         }
         msg[mpos] = 0;
         console_write_d(msg);
         return;
     }
 
     /*
      * Entry exists and is a directory — ask FAT to load it.
      * FAT_load_directory expects the raw uppercase name that was passed as
      * argv[1]; based on the test code it accepts a plain null-terminated
      * string (e.g. "PIJA\0").  We upper-case the argument before passing.
      */
     uint8_t fat_name[14];
     uint8_t i = 0;
     while (dirname[i] && i < 13)
     {
         uint8_t c = dirname[i];
         if (c >= 'a' && c <= 'z') c -= 32;
         fat_name[i] = c;
         i++;
     }
     fat_name[i] = 0;
 
     if (!FAT_load_directory(fat_name))
     {
         console_write_f((FAT_error_log == FAT_ERR_NO_SD_CARD)
                         ? PSTR_CD_NO_SD : PSTR_CD_ERR);
         return;
     }
 
     /*
      * FAT cluster is now updated in the FAT globals.
      * Push the directory name onto our path stack and rebuild the prompt.
      * We store the user-supplied name (not the upper-cased FAT name) so
      * the prompt looks friendlier if the SD card has mixed-case names.
      */
     uint8_t dlen = (uint8_t)strlen((char *)dirname);
     _path_push(dirname, dlen);
     _path_rebuild_prompt();
 }

 static const char PSTR_MEM1[]      PROGMEM = "               Checking MCU RAM : ";
 static const char PSTR_MEM2[]      PROGMEM = "   Checking external serial RAM : ";
 static const char PSTR_MEM3[]      PROGMEM = "               Checking SD card :";
 static const char PSTR_MEM13[]     PROGMEM = "                          Total : ";
 static const char PSTR_MEM4[]      PROGMEM = "SD card root volume information : ";
 static const char PSTR_MEM5[]      PROGMEM = " bytes free\n"; // not used
 static const char PSTR_MEM6[]      PROGMEM = " 128 KB\n"; // not used
 static const char PSTR_MEM7[]      PROGMEM = " 256 KB\n"; // not used
 static const char PSTR_MEM8[]      PROGMEM = " 384 KB\n"; // not used
 static const char PSTR_MEM9[]      PROGMEM = "slot ";
 static const char PSTR_MEM10[]     PROGMEM = " detected\n";
 static const char PSTR_MEM11[]     PROGMEM = " not";
 static const char PSTR_MEM12[]     PROGMEM = "                                  ";

 static void _cmd_mem_check(uint8_t argc, uint8_t *argv[])
 {
    (void)argc; (void)argv;
	uint16_t free_mem = 1;
	uint8_t *test;
	test = (uint8_t *)malloc(free_mem);
	while(test)
	{
		test = (uint8_t *)malloc(free_mem);
		free(test);
		free_mem++;
	}
	// MCU RAM
    console_write_f(PSTR_MEM1);
    uint8_t out[16];
    console_number(free_mem, 10, NULL, " Bytes free\n");
    
    free_mem = 384;
    // External Serial RAM
    console_write_f(PSTR_MEM2);
    console_write_f(PSTR_MEM9);
    console_number(0, 10, NULL, NULL);
    if(!(ram_status & 0x01)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM12);
    console_write_f(PSTR_MEM9);
    console_number(1, 10, NULL, NULL);
    if(!(ram_status & 0x02)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM12);
    console_write_f(PSTR_MEM9);
    console_number(2, 10, NULL, NULL);
    if(!(ram_status & 0x04)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM13);
    console_number(free_mem, 10, NULL, " KB\n");

    // SD card
    console_write_f(PSTR_MEM3);
    if(!SD_check_alive()) console_write_f(PSTR_MEM11); 
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM4);
    FAT_get_volume_id(out);
    out[11] = '\n';
    out[12] = '\0';
    console_write_d(out);
 }

 static void _cmd_ram_stat(uint8_t argc, uint8_t *argv[])
 {
    (void)argc; (void)argv;
	uint16_t free_mem = 1;
	uint8_t *test;
	test = (uint8_t *)malloc(free_mem);
	while(test)
	{
		test = (uint8_t *)malloc(free_mem);
		free(test);
		free_mem++;
	}
	// MCU RAM
    console_write_f(PSTR_MEM1);
    uint8_t out[16];
    console_number(free_mem, 10, NULL, " Bytes free\n");
    
    free_mem = 384;
    // External Serial RAM
    console_write_f(PSTR_MEM2);
    console_write_f(PSTR_MEM9);
    console_number(0, 10, NULL, NULL);
    if(!(ram_status & 0x01)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM12);
    console_write_f(PSTR_MEM9);
    console_number(1, 10, NULL, NULL);
    if(!(ram_status & 0x02)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM12);
    console_write_f(PSTR_MEM9);
    console_number(2, 10, NULL, NULL);
    if(!(ram_status & 0x04)) { console_write_f(PSTR_MEM11); free_mem -= 128; }
    console_write_f(PSTR_MEM10);
    console_write_f(PSTR_MEM13);
    console_number(free_mem, 10, NULL, " KB\n");

    // RAM allocation check
    console_write_f(PSTR("External RAM allocation table\n"));
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
    {
        console_write_f(PSTR("  slot["));
        console_number(a, 10, NULL, "] addr : 0x");
        console_number(ram_slot_addr[a], 16, NULL, " size : ");
        console_number(ram_slot_size[a], 10, NULL, " bytes\n");
    }
 }


 
 /* =========================================================================
  * wish_redraw — public full-screen refresh
  * ========================================================================= */
 void wish_redraw(void)
 {
     if (!console_visible) return;
     lcd_clear_screen(console_back_color);
     _redraw_output();
     _redraw_prompt_row();
 }
 
 /* =========================================================================
  * wish_init
  * ========================================================================= */
 void wish_init(void)
 {
     /* ── Derive geometry from globals ─────────────────────────────────── */
     uint16_t cols = _cols_per_row();
 
     /*
      * Each RAM slot stores 1 length byte + cols data bytes.
      * cols_per_row must fit in uint8_t (max 255); the LCD at 8px/char and
      * 480px wide gives 60 columns which fits fine.
      */
     console_ram_line_size = (uint8_t)(1 + (cols & 0xFF));
 
     /*
      * Reserve the last slot of the output buffer region as the pending line
      * buffer.  Effective usable line slots = (buffer_size / line_size) - 1.
      */
     lcd_write_debug("Loading...");
     ram_init();
     ram_test();

     console_ram_start_addr = ram_allocate(console_ram_buffer_size);
     if(!console_ram_start_addr) { lcd_write_debug("Console buffer allocation error"); return; }

     _pending_slot_addr  = console_ram_start_addr
                         + console_ram_buffer_size
                         - console_ram_line_size;
     console_pending_len = 0;
     ram_write_byte(_pending_slot_addr, 0);
 
     /* ── Clear runtime state ──────────────────────────────────────────── */
     console_stored_lines        = 0;
     console_view_top_line       = 0;
     console_input_index         = 0;
     console_input_col_offset    = 0;
     memset(console_input_buffer, 0, sizeof(console_input_buffer));
 
     /* ── Hardware init ────────────────────────────────────────────────── */
     
     keyboard_init();
     lcd_clear_screen(console_back_color);
    
     /* ── Build initial prompt (root, depth=0) ────────────────────────── */
     wish_path_depth = 0;
     _path_rebuild_prompt();
 
     /* ── Activate ─────────────────────────────────────────────────────── */
     
     console_active  = 1;
     console_visible = 1;
 
     /* ── Welcome banner ───────────────────────────────────────────────── */
     console_write_f(PSTR_WELCOME1);
     console_write_f(PSTR_WELCOME2);
     console_write_f(PSTR_WELCOME3);
     console_write_f(PSTR_WELCOME4);
     console_write_f(PSTR_WELCOME5);
     console_write_f(PSTR_WELCOME6);
     console_write_f(PSTR_WELCOME7);
     console_write_f(PSTR_WELCOME8);

     _cmd_mem_check(0, NULL);

     console_write_f(PSTR_WELCOME13);
     console_write_f(PSTR_WELCOME14);
 
     /* ── First draw ───────────────────────────────────────────────────── */
     //wish_redraw();
 }
 
 /* =========================================================================
  * wish_run  —  main blocking event loop
  * ========================================================================= */
 void wish_run(void)
 {
    if(!console_active) return;

     uint16_t count_1 = 0;
     uint16_t count_2 = 0;
     uint8_t cursor_old = 0, cursor = 0, key = 0;
     while (1)
     {
         if(count_1 > 50)
         {
            count_2++;
            count_1 = 0;
         }
         if(count_2 > 20)
         {
            count_2 = 0;
            count_1 = 0;
            if(cursor) { _redraw_cursor(0); cursor_old = 0; }
            if(!cursor) { _redraw_cursor(1); cursor_old = 1; }
            cursor = cursor_old;
         }
         key = keyboard_get_key(); /* blocks until key pressed */
         if (key) _handle_key(key);
         count_1++;
     }
 }

