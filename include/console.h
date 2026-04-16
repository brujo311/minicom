



/**
 * @file wish.h
 * @brief Wizard Shell (wish) - Header file
 *
 * Embedded shell for ATxmega128A3U @ 32MHz
 * Uses external 256KB RAM for output buffering and 480x320 color LCD.
 *
 * Architecture overview:
 *   - RAM layout: [line_count(2B)] [line_0_len(1B) | line_0_data(N)] [line_1...] ...
 *   - Each stored line occupies exactly console_ram_line_size bytes:
 *       byte 0     : actual string length (0..console_ram_line_size-1)
 *       bytes 1..N : string data (NOT null-terminated in RAM)
 *   - console_stored_lines tracks how many lines are currently stored.
 *   - When RAM is full the oldest console_ram_scroll_delete lines are discarded
 *     and all remaining lines are shifted down.
 */

 #ifndef WISH_H
 #define WISH_H
 
 #include <stdint.h>
 #include <avr/pgmspace.h>
 
 /* =========================================================================
  * Key code definitions  (keyboard driver values)
  * ========================================================================= */
 #define KEY_UP          0xB5
 #define KEY_DOWN        0xB6
 #define KEY_LEFT        0xB4
 #define KEY_RIGHT       0xB7
 #define KEY_ENTER       '\r'
 #define KEY_ENTER2      '\n'
 #define KEY_BACKSPACE   '\b'
 #define KEY_DEL         0x7F

 /* =========================================================================
  * Configurable globals (loaded from EEPROM by the caller before wish_init)
  * ========================================================================= */
 
 /* --- LCD geometry & colours --- */
 extern uint16_t lcd_size_x;            /* LCD width  in pixels  (default 480) */
 extern uint16_t lcd_size_y;            /* LCD height in pixels  (default 320) */
 extern uint16_t console_back_color;    /* Background colour (default BLACK)   */
 extern uint16_t console_text_color;    /* Output text colour (default GRAY)   */
 extern uint16_t console_prompt_color;  /* Prompt colour (default MAGENTA)     */
 
 /* --- Character cell dimensions --- */
 extern uint8_t  char_size_x;           /* Pixel width  of one character cell  */
 extern uint8_t  char_size_y;           /* Pixel height of one character cell  */
 extern uint8_t  console_char_size;     /* Scale factor passed to lcd_draw_char*/
 
 /* --- External RAM layout --- */
 extern uint32_t console_ram_start_addr;   /* Base address in external RAM     */
 extern uint32_t console_ram_buffer_size;  /* Bytes reserved for line storage  */
 
 /* --- Path stack RAM region ---
  * Placed right after the console output buffer by default.
  * Total size = WISH_PATH_MAX_DEPTH * WISH_PATH_NAME_MAX = 8 * 12 = 96 bytes.
  * Override before wish_init() if your memory map differs.                   */
 extern uint32_t wish_path_ram_addr;       /* Base address of path stack in RAM */
 
 /* --- Scroll-eviction policy --- */
 extern uint8_t  console_ram_scroll_delete;/* Lines to drop when buffer full   */
 
 /* --- Derived / runtime --- */
 extern uint8_t  console_ram_line_size;    /* Bytes per stored line slot        */
                                           /* = 1(len) + cols_per_row           */
 extern uint16_t console_stored_lines;     /* Total lines currently in RAM      */
 
 /* --- State flags --- */
 extern volatile uint8_t console_active;   /* 1 = write to RAM enabled         */
 extern volatile uint8_t console_visible;  /* 1 = render to screen enabled     */
 
 /* --- Input / prompt buffers --- */
 extern uint8_t  console_prompt[64];       /* Current prompt string (e.g. "wish / $ ") */
 extern uint8_t  console_prompt_size;      /* strlen(console_prompt)           */
 extern uint8_t  console_input_buffer[256];/* Current user input line          */
 extern uint8_t  console_input_index;      /* Cursor position in input buffer  */
 
 /* --- Prompt root-mode flag --- */
 extern uint8_t  console_root_mode;        /* 0 = '$', 1 = '#'                 */
 
 /* --- Scroll state --- */
 extern volatile uint8_t  prompt_row;      /* Row (in chars) where prompt sits */
 extern uint16_t console_view_top_line;    /* First RAM line currently shown   */
 extern uint8_t  console_input_col_offset; /* Horizontal scroll offset (chars) */
 
 /* =========================================================================
  * Public API
  * ========================================================================= */
 
 /**
  * @brief Initialise the console subsystem.
  *s
  * Must be called once after all globals have been loaded from EEPROM.
  * Calls ram_init(), keyboard_init(), clears the screen, and draws the
  * initial prompt.
  */
 void wish_init(void);
 
 /**
  * @brief Main blocking loop — call from main() after wish_init().
  *
  * Reads keys and processes them (printable chars, backspace, arrows, enter).
  */
 void wish_run(void);
 
 /**
  * @brief Write a RAM-resident string to the console output.
  *
  * The string is split into display-width chunks and stored as one or more
  * lines in external RAM.  No-op when console_active == 0.
  *
  * @param text  Pointer to null-terminated string in SRAM.
  */
 void console_write_d(uint8_t *text);
 
 /**
  * @brief Write a PROGMEM string to the console output.
  *
  * Copies the string from flash to a temporary SRAM buffer then calls
  * console_write_d().  No-op when console_active == 0.
  *
  * @param text  Pointer to null-terminated string in PROGMEM (use PSTR()).
  */
 void console_write_f(const char *text);
 
 /**
  * @brief Update the prompt string from the current working directory.
  *
  * Reconstructs console_prompt as  "wish <cwd> $ "  (or # in root mode).
  *
  * @param cwd  Null-terminated path string (RAM).
  */

 void console_number(uint32_t number, uint8_t base, uint8_t * prefix, uint8_t * sufix);
 
 void wish_set_prompt(uint8_t *cwd);
 
 /**
  * @brief Force a full redraw of the visible console area + prompt.
  *
  * Call this after programmatically changing console_visible or after a
  * resolution change.  No-op when console_visible == 0.
  */
 void wish_redraw(void);
 
 #endif /* WISH_H */