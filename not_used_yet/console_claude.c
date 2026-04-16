

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

/*
 * wish_console.c — Wizard Shell (wish) console for LCD 480x320
 *
 * Layout (8px char width, 10px char height at size=1):
 *   Columns  : 480 / 8  = 60 chars per line
 *   Rows     : 320 / 10 = 32 rows total
 *              31 rows for output history  (rows 0-30)
 *               1 row  for the input prompt (row 31)
 *
 * External RAM map (64 KB, 0x0000–0xFFFF):
 *   Each line occupies a fixed SLOT of LINE_STRIDE bytes.
 *   Line n  →  base address  n * LINE_STRIDE
 *   Byte 0  of the slot = line length  (0–60)
 *   Bytes 1–60 of the slot = characters (not NUL-terminated in RAM)
 *   Max lines = 65536 / LINE_STRIDE = 1024 lines
 *
 * Scroll keys:
 *   0xB5  arrow-up   → scroll up   (view_top--)
 *   0xB6  arrow-down → scroll down (view_top++)
 *   0xB4  arrow-left  (reserved / future use)
 *   0xB7  arrow-right (reserved / future use)
 */

 /* ── colour constants ────────────────────────────────────────────────────── */
 //#define BLACK   0x0000u
 //#define WHITE   0xFFFFu
 //#define GREEN   0x07E0u   /* prompt / command text  */
 //#define CYAN    0x07FFu   /* output text            */
 //#define YELLOW  0xFFE0u   /* error text             */
 #define GRAY    0x8410u   /* scroll-bar / decorative*/

 /* ── display geometry ───────────────────────────────────────────────────── */
 #define LCD_W           480u
 #define LCD_H           320u
 #define CHAR_W          8u    /* pixels per char, size=1 */
 #define CHAR_H          10u   /* pixels per char, size=1 */
 #define COLS            (LCD_W  / CHAR_W)   /* 60 */
 #define DISPLAY_ROWS    (LCD_H  / CHAR_H)   /* 32 */
 #define OUTPUT_ROWS     (DISPLAY_ROWS - 1u) /* 31 — rows 0..30 */
 #define PROMPT_ROW      (DISPLAY_ROWS - 1u) /* row 31 */

 /* ── RAM / line-buffer constants ────────────────────────────────────────── */
 #define LINE_STRIDE     61u   /* 1 length byte + 60 char bytes */
 #define MAX_LINES       (65536u / LINE_STRIDE)  /* 1074 */
 #define MAX_INPUT       (COLS - 8u)             /* leave room for "wish # " */
 #define PROMPT          ((uint8_t *)"wish # ")
 #define PROMPT_LEN      7u

 /* ── key codes ──────────────────────────────────────────────────────────── */
 #define KEY_UP          0xB5u
 #define KEY_DOWN        0xB6u
 #define KEY_LEFT        0xB4u
 #define KEY_RIGHT       0xB7u
 #define KEY_ENTER       '\r'
 #define KEY_ENTER2      '\n'
 #define KEY_BACKSPACE   '\b'
 #define KEY_DEL         0x7Fu

 /* ══════════════════════════════════════════════════════════════════════════
  *  RAM helper — read/write a whole line slot
  * ══════════════════════════════════════════════════════════════════════════*/

 static void ram_write_line(uint16_t line_no, const uint8_t *buf, uint8_t len)
 {
     uint32_t base = (uint32_t)line_no * LINE_STRIDE;
     ram_write_byte(base, len);
     ram_write(base + 1, buf, len);
 }

 static uint8_t ram_read_line(uint16_t line_no, uint8_t *buf)
 {
     uint32_t base = (uint32_t)line_no * LINE_STRIDE;
     uint8_t  len  = ram_read_byte(base);
     if (len > COLS) len = COLS;          /* safety clamp */
     ram_read(base + 1, buf, len);
     buf[len] = '\0';
     return len;
 }

 /* ══════════════════════════════════════════════════════════════════════════
  *  Screen helpers
  * ══════════════════════════════════════════════════════════════════════════*/

 /* Clear a single text row (erase pixels) */
 static void clear_row(uint8_t row)
 {
     lcd_draw_window(0, LCD_W - 1u,
                     (uint16_t)(row * CHAR_H),
                     (uint16_t)(row * CHAR_H + CHAR_H - 1u),
                     BLACK);
 }

 /* Draw one RAM line onto a display row */
 static void draw_line(uint16_t line_no, uint8_t display_row, uint16_t color)
 {
     uint8_t buf[COLS + 1u];
     uint8_t len = ram_read_line(line_no, buf);

     clear_row(display_row);
     if (len == 0u) return;

     lcd_set_position((uint16_t)(0u),
                      (uint16_t)(display_row * CHAR_H));
     lcd_draw_string(buf, color, 1u);
 }

 /* Repaint the entire output area from view_top */
 static void redraw_output(uint16_t view_top, uint16_t total_lines)
 {
     for (uint8_t row = 0; row < OUTPUT_ROWS; row++)
     {
         uint16_t line_no = view_top + row;
         if (line_no < total_lines)
             draw_line(line_no, row, CYAN);
         else
             clear_row(row);
     }
 }

 /* Redraw only the prompt row */
 static void redraw_prompt(const uint8_t *input_buf, uint8_t input_len)
 {
     clear_row(PROMPT_ROW);
     lcd_set_position(0, (uint16_t)(PROMPT_ROW * CHAR_H));
     lcd_draw_string(PROMPT, GREEN, 1u);

     /* draw the input so far */
     uint8_t tmp[MAX_INPUT + 1u];
     for (uint8_t i = 0; i < input_len; i++) tmp[i] = input_buf[i];
     tmp[input_len] = '\0';
     lcd_set_position((uint16_t)(PROMPT_LEN * CHAR_W),
                      (uint16_t)(PROMPT_ROW * CHAR_H));
     lcd_draw_string(tmp, WHITE, 1u);

     /* draw a simple block cursor */
     lcd_draw_window(
         (uint16_t)((PROMPT_LEN + input_len) * CHAR_W),
         (uint16_t)((PROMPT_LEN + input_len) * CHAR_W + CHAR_W - 1u),
         (uint16_t)(PROMPT_ROW * CHAR_H),
         (uint16_t)(PROMPT_ROW * CHAR_H + CHAR_H - 1u),
         GREEN);
 }

 /* ══════════════════════════════════════════════════════════════════════════
  *  Append a line to the RAM buffer and optionally scroll to bottom
  * ══════════════════════════════════════════════════════════════════════════*/

 /* Append text to RAM.  If text is longer than COLS, it is hard-wrapped.
  * Returns updated total_lines. */
 static uint16_t append_text(uint16_t total_lines,
                              const uint8_t *text, uint16_t text_len,
                              uint16_t color_hint)
 {
     (void)color_hint; /* colour is chosen at draw time based on context */

     if (text_len == 0u)
     {
         /* store an empty line */
         if (total_lines < MAX_LINES)
         {
             ram_write_line(total_lines, (const uint8_t *)"", 0u);
             total_lines++;
         }
         return total_lines;
     }

     uint16_t offset = 0u;
     while (offset < text_len)
     {
         uint8_t  chunk = (uint8_t)(text_len - offset);
         if (chunk > COLS) chunk = COLS;

         if (total_lines < MAX_LINES)
         {
             ram_write_line(total_lines, text + offset, chunk);
             total_lines++;
         }
         offset += chunk;
     }
     return total_lines;
 }

 /* ══════════════════════════════════════════════════════════════════════════
  *  Command parser
  *
  *  Add new commands by extending the if/else chain in execute_command().
  * ══════════════════════════════════════════════════════════════════════════*/

 /* Simple string compare (NUL-terminated) */
 static int8_t streq(const uint8_t *a, const char *b)
 {
     while (*b)
     {
         if (*a != (uint8_t)*b) return 0;
         a++; b++;
     }
     return (*a == '\0');
 }

 /* Starts-with helper */
 static int8_t startswith(const uint8_t *a, const char *b)
 {
     while (*b)
     {
         if (*a != (uint8_t)*b) return 0;
         a++; b++;
     }
     return 1;
 }

 /*
  * execute_command — parse cmd_buf, write output lines to RAM.
  *
  * Returns updated total_lines.
  */
 static uint16_t execute_command(const uint8_t *cmd_buf,
                                 uint8_t        cmd_len,
                                 uint16_t       total_lines)
 {
     /* Echo the command with the prompt prefix */
     uint8_t echo[COLS + 1u];
     uint8_t echo_len = 0;
     const char *p = "wish # ";
     while (*p && echo_len < COLS) echo[echo_len++] = (uint8_t)*p++;
     for (uint8_t i = 0; i < cmd_len && echo_len < COLS; i++)
         echo[echo_len++] = cmd_buf[i];
     total_lines = append_text(total_lines, echo, echo_len, GREEN);

     if (cmd_len == 0u)
         return total_lines;   /* blank enter — just echo the prompt */

     /* ── built-in commands ─────────────────────────────────────────────── */

     if (streq(cmd_buf, "clear"))
     {
         /* Reset — wipe RAM lines (we just set total_lines back to 0) */
         /* We write a sentinel length=0 to line 0 so old data is logically gone */
         ram_write_byte(0, 0);
         total_lines = 0;
         return total_lines;
     }

     if (streq(cmd_buf, "help"))
     {
         static const char *help[] = {
             "  Wizard Shell - wish v1.0",
             "  Commands:",
             "    help      - show this message",
             "    clear     - clear the console",
             "    echo <t>  - print text",
             "    version   - firmware version",
             "    reboot    - software reset",
             NULL
         };
         for (uint8_t i = 0; help[i]; i++)
         {
             const char *s = help[i];
             uint8_t     buf[COLS];
             uint8_t     len = 0;
             while (*s && len < COLS) buf[len++] = (uint8_t)*s++;
             total_lines = append_text(total_lines, buf, len, CYAN);
         }
         return total_lines;
     }

     if (streq(cmd_buf, "version"))
     {
         total_lines = append_text(total_lines,
             (const uint8_t *)"  wish 1.0 - built " __DATE__, 28u, CYAN);
         return total_lines;
     }

     if (startswith(cmd_buf, "echo "))
     {
         const uint8_t *arg    = cmd_buf + 5u;
         uint8_t        arglen = (cmd_len >= 5u) ? (cmd_len - 5u) : 0u;
         if (arglen > 0u)
             total_lines = append_text(total_lines, arg, arglen, CYAN);
         else
             total_lines = append_text(total_lines, (const uint8_t *)"", 0u, CYAN);
         return total_lines;
     }

     if (streq(cmd_buf, "reboot"))
     {
         /* Platform-specific reset — insert your MCU reset call here */
         total_lines = append_text(total_lines,
             (const uint8_t *)"  Rebooting...", 14u, YELLOW);
         /* e.g. NVIC_SystemReset(); */
         return total_lines;
     }

     /* Unknown command */
     {
         uint8_t  msg[COLS];
         uint8_t  mlen = 0;
         const char *prefix = "  wish: command not found: ";
         while (*prefix && mlen < COLS) msg[mlen++] = (uint8_t)*prefix++;
         for (uint8_t i = 0; i < cmd_len && mlen < COLS; i++)
             msg[mlen++] = cmd_buf[i];
         total_lines = append_text(total_lines, msg, mlen, YELLOW);
     }
     return total_lines;
 }

 /* ══════════════════════════════════════════════════════════════════════════
  *  console() — main entry point
  * ══════════════════════════════════════════════════════════════════════════*/

 void console(void)
 {
     uint16_t total_lines = 0;   /* number of lines stored in RAM          */
     uint16_t view_top    = 0;   /* first RAM line shown in output area    */

     uint8_t  input_buf[MAX_INPUT + 1u];
     uint8_t  input_len  = 0;

     /* ── boot banner ─────────────────────────────────────────────────────*/
     lcd_clear_screen(BLACK);

     static const char *banner[] = {
         "  ################################",
         "  #  Wizard Shell  (wish v1.0)   #",
         "  ################################",
         "  Type 'help' for available commands.",
         NULL
     };
     for (uint8_t i = 0; banner[i]; i++)
     {
         const char *s   = banner[i];
         uint8_t     buf[COLS];
         uint8_t     len = 0;
         while (*s && len < COLS) buf[len++] = (uint8_t)*s++;
         total_lines = append_text(total_lines, buf, len, GREEN);
     }

     /* scroll to bottom and paint */
     if (total_lines > OUTPUT_ROWS)
         view_top = total_lines - OUTPUT_ROWS;
     else
         view_top = 0;

     redraw_output(view_top, total_lines);
     redraw_prompt(input_buf, input_len);

     /* ── main loop ───────────────────────────────────────────────────────*/
     for (;;)
     {
         uint8_t key = keyboard_wait_key();

         /* ── scroll keys ─────────────────────────────────────────────── */
         if (key == KEY_UP)
         {
             if (view_top > 0u)
             {
                 view_top--;
                 redraw_output(view_top, total_lines);
             }
             continue;
         }

         if (key == KEY_DOWN)
         {
             uint16_t max_top = (total_lines > OUTPUT_ROWS)
                                ? (total_lines - OUTPUT_ROWS) : 0u;
             if (view_top < max_top)
             {
                 view_top++;
                 redraw_output(view_top, total_lines);
             }
             continue;
         }

         /* Arrow-left / Arrow-right: reserved, ignore for now */
         if (key == KEY_LEFT || key == KEY_RIGHT)
             continue;

         /* ── editing keys ────────────────────────────────────────────── */
         if (key == KEY_BACKSPACE || key == KEY_DEL)
         {
             if (input_len > 0u)
             {
                 input_len--;
                 redraw_prompt(input_buf, input_len);
             }
             continue;
         }

         if (key == KEY_ENTER || key == KEY_ENTER2)
         {
             input_buf[input_len] = '\0';

             /* Execute and write output to RAM */
             total_lines = execute_command(input_buf, input_len, total_lines);

             /* Scroll to the last line so user sees the output */
             if (total_lines > OUTPUT_ROWS)
                 view_top = total_lines - OUTPUT_ROWS;
             else
                 view_top = 0;

             /* Clear input */
             input_len = 0;

             redraw_output(view_top, total_lines);
             redraw_prompt(input_buf, input_len);
             continue;
         }

         /* ── printable character ─────────────────────────────────────── */
         if (key >= 0x20u && key < 0x7Fu)
         {
             if (input_len < MAX_INPUT)
             {
                 input_buf[input_len++] = key;
                 redraw_prompt(input_buf, input_len);
             }
             /* If line is full, silently discard (no bell in this BSP) */
             continue;
         }

         /* Any other key code: ignore */
     }
 }
