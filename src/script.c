/* =============================================================================
 * wish_script.c  —  _cmd_script() implementation
 *
 * Syntax summary
 * --------------
 *   $var = <expr>              integer/float/string assignment
 *   $var = "hello world"       string assignment
 *   echo $var                  variable expansion before dispatch
 *   if(<expr>) {               conditional  (single-line condition)
 *   } elif(<expr>) {
 *   } else {
 *   }
 *   while(<expr>) {            loop
 *   }
 *   for($i=0;$i<10;$i++) {    C-style for  (no spaces around ; required)
 *   }
 *   script myscript.sh         nested call (up to SCRIPT_MAX_DEPTH)
 *   --buffer-size <bytes>      must be argv before filename (optional)
 *   #                          comment
 *
 * Memory layout (serial RAM)
 * --------------------------
 *   One global variable table  : up to SCRIPT_VAR_MAX entries
 *     Each entry               : name (16 B) + type tag (1 B) + payload
 *       INT  payload           : 4 B  (int32_t)
 *       FLT  payload           : 4 B  (float, via memcpy)
 *       STR  payload           : 2 B  address of separate string cell
 *     String cells             : variable length, null-terminated
 *   Per-call block buffer      : up to cfg.block_buf_size bytes
 *     Holds the raw text of one { … } block for if/while/for bodies.
 *
 * All serial-RAM addresses are obtained via ram_allocate_safe().
 * The RAM key used throughout is SCRIPT_RAM_KEY.
 * =========================================================================== */

 #include <stdint.h>
 #include <string.h>
 #include <stdlib.h>          /* strtol, strtof */
 #include <avr/pgmspace.h>
// #include <avr/io.h>
// #include <stdarg.h>
 #include "script.h"
// #include "mcu_driver.h"
// #include "lcd_driver.h"
 #include "FAT32.h"
// #include "SD_routines.h"
 #include "ningen_ui.h"
 #include "colors.h"
 #include "ram_driver.h"
// #include "nvm_driver.h"
// #include "kb_driver.h"
 #include "console.h"
 #include "programs.h"
 
 /* ── tunables ─────────────────────────────────────────────────────────────── */
 #define SCRIPT_RAM_KEY        0xA7u   /* arbitrary non-zero key                */
 #define SCRIPT_MAX_DEPTH      4       /* max nested script() calls             */
 #define SCRIPT_VAR_MAX        32      /* max live variables                    */
 #define SCRIPT_LINE_MAX       256     /* max chars in one script line          */
 #define SCRIPT_NAME_MAX       16      /* max variable name length (with '\0')  */
 #define SCRIPT_DEFAULT_BUF    8192u   /* default block-body buffer (bytes)     */
 #define SCRIPT_TOTAL_BUDGET   49152u  /* 48 KB hard ceiling for all allocs     */
 
 /* ── variable table ───────────────────────────────────────────────────────── */
 #define VTYPE_INT  0u
 #define VTYPE_FLT  1u
 #define VTYPE_STR  2u
 #define VTYPE_NONE 0xFFu
 
 /*
  * Each slot in the global variable table lives in serial RAM at a fixed layout:
  *
  *   Offset  Size  Field
  *   ------  ----  -----
  *        0    16  name (null-padded)
  *       16     1  type  (VTYPE_INT / VTYPE_FLT / VTYPE_STR / VTYPE_NONE)
  *       17     4  payload
  *                   INT : int32_t  (little-endian)
  *                   FLT : float    (IEEE-754 little-endian)
  *                   STR : uint32_t address of string cell in serial RAM
  *       21     –  (total 21 bytes per slot)
  */
 #define VAR_SLOT_BYTES  21u
 #define VAR_NAME_OFF     0u
 #define VAR_TYPE_OFF    16u
 #define VAR_PAY_OFF     17u
 
 /* ── script engine state ──────────────────────────────────────────────────── */
 typedef struct {
     uint32_t var_table_addr;   /* base address of var table in serial RAM     */
     uint32_t block_buf_addr;   /* base address of block-body buffer            */
     uint32_t block_buf_size;   /* size of block-body buffer                   */
     uint32_t total_used;       /* running total of bytes allocated             */
     uint8_t  depth;            /* current recursion depth                     */
     uint8_t  abort;            /* set to 1 on any fatal error                 */
     /* one-line put-back — lets exec_file peek without FAT_read_specific_*   */
     uint8_t  pb_valid;
     uint8_t  pb_buf[SCRIPT_LINE_MAX];
 } ScriptState;
 
 static ScriptState g_ss;      /* single global instance                       */
 
 /*
  * line_clean – strip ALL leading and trailing \r \n from a raw FAT line.
  *
  * FAT_read_file_line stores the newline byte into data[] before it sets
  * find_nl=1, so the first byte of the returned buffer is often '\n'.
  * Without stripping this the first real character of every line is lost.
  */
  static void line_clean(uint8_t *buf)
  {
      /* strip trailing \r \n */
      uint16_t len = (uint16_t)strlen((char *)buf);
      while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
          buf[--len] = 0;
  
      /* strip leading \r \n — this is the critical one */
      uint16_t start = 0;
      while (buf[start] == '\r' || buf[start] == '\n') start++;
      if (start > 0) {
          uint16_t i = 0;
          while ((buf[i] = buf[i + start]) != 0) i++;
      }
  }

 /*
  * fat_read_line / fat_unread_line
  * --------------------------------
  * Thin sequential wrapper around FAT_read_file_line with a one-line put-back
  * slot.  All reads inside exec_file and collect_block_from_file go through
  * fat_read_line so that the put-back is always honoured.
  * FAT_read_specific_file_line is never called — it has an off-by-one bug
  * that drops the first character of the target line.
  */
 static uint8_t fat_read_line(uint8_t *file_name, uint8_t *buf)
 {
     if (g_ss.pb_valid) {
         memcpy(buf, g_ss.pb_buf, SCRIPT_LINE_MAX);
         g_ss.pb_valid = 0;
         return 1;
     }
     uint8_t r = FAT_read_file_line(file_name, buf);
     if (r) line_clean(buf);
     return r;
 }
 
 static void fat_unread_line(const uint8_t *buf)
 {
     /* only one level of put-back is ever needed */
     memcpy(g_ss.pb_buf, buf, SCRIPT_LINE_MAX);
     g_ss.pb_valid = 1;
 }
 
 /* ── helpers: serial-RAM byte-array read/write ────────────────────────────── */
 
 static void ram_write_buf(uint32_t addr, const uint8_t *src, uint16_t len)
 {
     if(!ram_write_safe(addr, src, len, SCRIPT_RAM_KEY))
        console_write_f(PSTR("script : permission denied\n"));

     //for (uint16_t i = 0; i < len; i++)
         //ram_write_byte_safe(addr + i, src[i], SCRIPT_RAM_KEY);
 }
 
 static void ram_read_buf(uint32_t addr, uint8_t *dst, uint16_t len)
 {
    if(!ram_read_safe(addr, dst, len, SCRIPT_RAM_KEY))
        console_write_f(PSTR("script : permission denied\n"));
     //for (uint16_t i = 0; i < len; i++)
         //dst[i] = ram_read_byte_safe(addr + i, SCRIPT_RAM_KEY);
 }
 
 static void ram_write_str(uint32_t addr, const uint8_t *s)
 {
     uint16_t len = strlen((char *)s);
     ram_write_byte_safe(addr, (uint8_t)(len >> 8), SCRIPT_RAM_KEY);
     ram_write_byte_safe(addr + 1, (uint8_t)len, SCRIPT_RAM_KEY);
     if(!ram_write_safe(addr + 2, s, len, SCRIPT_RAM_KEY))
        console_write_f(PSTR("script : permission denied\n"));
     /*uint16_t i = 0;
     do {
         ram_write_byte_safe(addr + i, s[i], SCRIPT_RAM_KEY);
     } while (s[i++]);*/
 }
 
 static void ram_read_str(uint32_t addr, uint8_t *dst, uint16_t max_len)
 {
     uint16_t len = (uint16_t)ram_read_byte_safe(addr, SCRIPT_RAM_KEY) << 8;
     len |= ram_read_byte_safe(addr + 1, SCRIPT_RAM_KEY);
     if(!ram_read_safe(addr + 2, dst, len, SCRIPT_RAM_KEY))
        console_write_f(PSTR("script : permission denied\n"));
     /*uint16_t i = 0;
     while (i < max_len - 1) {
         uint8_t c = ram_read_byte_safe(addr + i, SCRIPT_RAM_KEY);
         dst[i++] = c;
         if (!c) return;
     }
     dst[i] = 0;*/
 }

 static void ram_free_mem(uint32_t addr)
 {
    if(!ram_free_safe(addr, SCRIPT_RAM_KEY))
        console_number_f(addr, 16, PSTR("script : mem free addr 0x"), PSTR(" failed, permission denied\n"));

 }
 
 /* ── safe allocator (tracks budget) ──────────────────────────────────────── */
 
 static uint32_t script_alloc(uint32_t size)
 {
     if (g_ss.total_used + size > SCRIPT_TOTAL_BUDGET) 
     {
         console_write_f(PSTR("script: RAM budget exceeded\n"));
         g_ss.abort = 1;
         return 0;
     }
     uint32_t addr = ram_allocate_safe(size, SCRIPT_RAM_KEY);
     if (!addr) 
     {
         console_write_f(PSTR("script: RAM allocation failed\n"));
         g_ss.abort = 1;
         return 0;
     }
     g_ss.total_used += size;
     return addr;
 }
 
 /* ── variable table operations ────────────────────────────────────────────── */

 static void var_table_free(void)
 {
    ram_free_mem(g_ss.var_table_addr);
 }

 /* Mark all slots VTYPE_NONE */
 static void var_table_init(void)
 {
     for (uint8_t i = 0; i < SCRIPT_VAR_MAX; i++) 
     {
         uint32_t base = g_ss.var_table_addr + (uint32_t)i * VAR_SLOT_BYTES;
         ram_write_byte_safe(base + VAR_TYPE_OFF, VTYPE_NONE, SCRIPT_RAM_KEY);
         ram_write_byte_safe(base + VAR_NAME_OFF, 0,          SCRIPT_RAM_KEY);
     }
 }
 
 /* Returns slot index (0‥SCRIPT_VAR_MAX-1) or 0xFF if not found */
 static uint8_t var_find(const uint8_t *name)
 {
     uint8_t nlen = (uint8_t)strlen((const char *)name);
     if (nlen >= SCRIPT_NAME_MAX) nlen = SCRIPT_NAME_MAX - 1;
 
     for (uint8_t i = 0; i < SCRIPT_VAR_MAX; i++) {
         uint32_t base = g_ss.var_table_addr + (uint32_t)i * VAR_SLOT_BYTES;
         if (ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY) == VTYPE_NONE)
             continue;
         uint8_t match = 1;
         for (uint8_t j = 0; j <= nlen; j++) {
             if (ram_read_byte_safe(base + VAR_NAME_OFF + j, SCRIPT_RAM_KEY)
                     != name[j]) { match = 0; break; }
         }
         if (match) return i;
     }
     return 0xFF;
 }
 
 /* Returns first free slot index or 0xFF if table full */
 static uint8_t var_alloc_slot(void)
 {
     for (uint8_t i = 0; i < SCRIPT_VAR_MAX; i++) {
         uint32_t base = g_ss.var_table_addr + (uint32_t)i * VAR_SLOT_BYTES;
         if (ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY) == VTYPE_NONE)
             return i;
     }
     return 0xFF;
 }
 
 static void var_write_name(uint8_t slot, const uint8_t *name)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     uint8_t i = 0;
     do {
         ram_write_byte_safe(base + VAR_NAME_OFF + i, name[i], SCRIPT_RAM_KEY);
     } while (name[i++] && i < SCRIPT_NAME_MAX);
     ram_write_byte_safe(base + VAR_NAME_OFF + (SCRIPT_NAME_MAX - 1),
                         0, SCRIPT_RAM_KEY);
 }
 
 static void var_set_int(uint8_t slot, int32_t val)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     ram_write_byte_safe(base + VAR_TYPE_OFF, VTYPE_INT, SCRIPT_RAM_KEY);
     uint8_t b[4];
     memcpy(b, &val, 4);
     ram_write_buf(base + VAR_PAY_OFF, b, 4);
 }
 
 static void var_set_flt(uint8_t slot, float val)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     ram_write_byte_safe(base + VAR_TYPE_OFF, VTYPE_FLT, SCRIPT_RAM_KEY);
     uint8_t b[4];
     memcpy(b, &val, 4);
     ram_write_buf(base + VAR_PAY_OFF, b, 4);
 }
 
 /* For strings: allocate a fresh cell in serial RAM and store the address */
 static void var_set_str(uint8_t slot, const uint8_t *s)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     uint8_t  prev_type = ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY);
 
     uint16_t slen = (uint16_t)strlen((const char *)s) + 1u;
     uint32_t cell = script_alloc(slen);
     if (!cell) return;
 
     (void)prev_type; /* old string cell is leaked – acceptable for small scripts */
 
     ram_write_str(cell, s);
     ram_write_byte_safe(base + VAR_TYPE_OFF, VTYPE_STR, SCRIPT_RAM_KEY);
     uint8_t b[4];
     memcpy(b, &cell, 4);
     ram_write_buf(base + VAR_PAY_OFF, b, 4);
 }
 
 static int32_t var_get_int(uint8_t slot)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     uint8_t  type = ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY);
     uint8_t  b[4];
     ram_read_buf(base + VAR_PAY_OFF, b, 4);
 
     if (type == VTYPE_INT) { int32_t v; memcpy(&v, b, 4); return v; }
     if (type == VTYPE_FLT) { float   f; memcpy(&f, b, 4); return (int32_t)f; }
     if (type == VTYPE_STR) {
         uint32_t addr; memcpy(&addr, b, 4);
         uint8_t tmp[32]; ram_read_str(addr, tmp, sizeof(tmp));
         return (int32_t)strtol((char *)tmp, NULL, 0);
     }
     return 0;
 }
 
 static float var_get_flt(uint8_t slot)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     uint8_t  type = ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY);
     uint8_t  b[4];
     ram_read_buf(base + VAR_PAY_OFF, b, 4);
 
     if (type == VTYPE_FLT) { float f; memcpy(&f, b, 4); return f; }
     if (type == VTYPE_INT) { int32_t i; memcpy(&i, b, 4); return (float)i; }
     if (type == VTYPE_STR) {
         uint32_t addr; memcpy(&addr, b, 4);
         uint8_t tmp[32]; ram_read_str(addr, tmp, sizeof(tmp));
         return strtof((char *)tmp, NULL);
     }
     return 0.0f;
 }
 
 /* Read a variable's string representation into dst (max dst_len bytes) */
 static void var_get_str(uint8_t slot, uint8_t *dst, uint16_t dst_len)
 {
     uint32_t base = g_ss.var_table_addr + (uint32_t)slot * VAR_SLOT_BYTES;
     uint8_t  type = ram_read_byte_safe(base + VAR_TYPE_OFF, SCRIPT_RAM_KEY);
     uint8_t  b[4];
     ram_read_buf(base + VAR_PAY_OFF, b, 4);
 
     if (type == VTYPE_STR) {
         uint32_t addr; memcpy(&addr, b, 4);
         ram_read_str(addr, dst, dst_len);
         return;
     }
     if (type == VTYPE_INT) {
         int32_t v; memcpy(&v, b, 4);
         /* simple itoa – negative support */
         char tmp[16]; ltoa(v, tmp, 10);
         strncpy((char *)dst, tmp, dst_len - 1); dst[dst_len-1] = 0;
         return;
     }
     if (type == VTYPE_FLT) {
         float f; memcpy(&f, b, 4);
         /* dtostrf: width=1, precision=4 */
         dtostrf((double)f, 1, 4, (char *)dst);
         return;
     }
     dst[0] = 0;
 }
 
 /* ── variable expansion ───────────────────────────────────────────────────── */
 /*
  * Expand all $name occurrences in `src` into `dst`.
  * dst_max must be >= SCRIPT_LINE_MAX.
  */
 static void expand_vars(const uint8_t *src, uint8_t *dst, uint16_t dst_max)
 {
     uint16_t si = 0, di = 0;
     while (src[si] && di < dst_max - 1) {
         if (src[si] != '$') {
             dst[di++] = src[si++];
             continue;
         }
         si++; /* skip $ */
         /* collect variable name: alphanumeric + _ */
         uint8_t name[SCRIPT_NAME_MAX];
         uint8_t ni = 0;
         while (src[si] && (src[si] == '_' ||
                (src[si] >= 'a' && src[si] <= 'z') ||
                (src[si] >= 'A' && src[si] <= 'Z') ||
                (src[si] >= '0' && src[si] <= '9')) &&
                ni < SCRIPT_NAME_MAX - 1)
         {
             name[ni++] = src[si++];
         }
         name[ni] = 0;
         if (ni == 0) { dst[di++] = '$'; continue; }
 
         uint8_t slot = var_find(name);
         if (slot == 0xFF) {
             /* unknown variable – expand to empty string */
             continue;
         }
         uint8_t val[64];
         var_get_str(slot, val, sizeof(val));
         uint8_t vi = 0;
         while (val[vi] && di < dst_max - 1)
             dst[di++] = val[vi++];
     }
     dst[di] = 0;
 }
 
 /* ── expression evaluator ─────────────────────────────────────────────────── */
 /*
  * Evaluates a numeric expression string and returns the result as float.
  * Supports: integer and float literals, $variables, +, -, *, /, unary -,
  *           ==, !=, <, <=, >, >=, &&, ||, !
  * Precedence (low→high): || → && → comparison → +/- → * / → unary → atom
  *
  * The expression is first variable-expanded, then parsed recursively.
  * All arithmetic is done in float; booleans are float 0.0 / 1.0.
  */
 
 /* parser cursor – points into a static expanded buffer */
 static const uint8_t *g_ep;
 
 static void  ep_skip_ws(void) { while (*g_ep == ' ' || *g_ep == '\t') g_ep++; }
 static float ep_parse_expr(void);  /* forward */
 
 static float ep_parse_atom(void)
 {
     ep_skip_ws();
     /* unary ! */
     if (*g_ep == '!') { g_ep++; return ep_parse_atom() == 0.0f ? 1.0f : 0.0f; }
     /* unary - */
     if (*g_ep == '-') { g_ep++; return -ep_parse_atom(); }
     /* parenthesised sub-expression */
     if (*g_ep == '(') {
         g_ep++;
         float v = ep_parse_expr();
         ep_skip_ws();
         if (*g_ep == ')') g_ep++;
         return v;
     }
     /* string literal  "…"  – not numeric, evaluate as 0 in expressions */
     if (*g_ep == '"') {
         g_ep++;
         while (*g_ep && *g_ep != '"') g_ep++;
         if (*g_ep == '"') g_ep++;
         return 0.0f;
     }
     /* float / int literal */
     if ((*g_ep >= '0' && *g_ep <= '9') || *g_ep == '.') {
         char *end;
         float v = strtof((const char *)g_ep, &end);
         g_ep = (const uint8_t *)end;
         return v;
     }
     /* bare word (already-expanded variable placeholder shouldn't appear,
        but handle gracefully) */
     while (*g_ep && *g_ep != ' ' && *g_ep != ')' &&
            *g_ep != '&' && *g_ep != '|') g_ep++;
     return 0.0f;
 }
 
 static float ep_parse_muldiv(void)
 {
     float l = ep_parse_atom();
     while (1) {
         ep_skip_ws();
         if (*g_ep == '*') { g_ep++; l *= ep_parse_atom(); }
         else if (*g_ep == '/') {
             g_ep++;
             float r = ep_parse_atom();
             l = (r != 0.0f) ? l / r : 0.0f;
         } else break;
     }
     return l;
 }
 
 static float ep_parse_addsub(void)
 {
     float l = ep_parse_muldiv();
     while (1) {
         ep_skip_ws();
         if      (*g_ep == '+') { g_ep++; l += ep_parse_muldiv(); }
         else if (*g_ep == '-') { g_ep++; l -= ep_parse_muldiv(); }
         else break;
     }
     return l;
 }
 
 static float ep_parse_cmp(void)
 {
     float l = ep_parse_addsub();
     ep_skip_ws();
     if (g_ep[0]=='=' && g_ep[1]=='=') { g_ep+=2; return l == ep_parse_addsub() ? 1.0f:0.0f; }
     if (g_ep[0]=='!' && g_ep[1]=='=') { g_ep+=2; return l != ep_parse_addsub() ? 1.0f:0.0f; }
     if (g_ep[0]=='<' && g_ep[1]=='=') { g_ep+=2; return l <= ep_parse_addsub() ? 1.0f:0.0f; }
     if (g_ep[0]=='>' && g_ep[1]=='=') { g_ep+=2; return l >= ep_parse_addsub() ? 1.0f:0.0f; }
     if (g_ep[0]=='<')                 { g_ep++;  return l <  ep_parse_addsub() ? 1.0f:0.0f; }
     if (g_ep[0]=='>')                 { g_ep++;  return l >  ep_parse_addsub() ? 1.0f:0.0f; }
     return l;
 }
 
 static float ep_parse_and(void)
 {
     float l = ep_parse_cmp();
     while (g_ep[0]=='&' && g_ep[1]=='&') {
         g_ep+=2; float r = ep_parse_cmp();
         l = (l != 0.0f && r != 0.0f) ? 1.0f : 0.0f;
     }
     return l;
 }
 
 static float ep_parse_expr(void)
 {
     float l = ep_parse_and();
     while (g_ep[0]=='|' && g_ep[1]=='|') {
         g_ep+=2; float r = ep_parse_and();
         l = (l != 0.0f || r != 0.0f) ? 1.0f : 0.0f;
     }
     return l;
 }
 
 /* Public: expand variables in expr_in, then evaluate */
 static float eval_expr(const uint8_t *expr_in)
 {
     static uint8_t expbuf[SCRIPT_LINE_MAX];
     expand_vars(expr_in, expbuf, sizeof(expbuf));
     g_ep = expbuf;
     return ep_parse_expr();
 }
 
 /* ── assignment parser ────────────────────────────────────────────────────── */
 /*
  * Handles:  $name = <rhs>
  * rhs is a string literal "…", or a numeric/expression.
  * Returns 1 if the line was an assignment, 0 otherwise.
  */
 static uint8_t try_assignment(uint8_t *line)
 {
     uint8_t *p = line;
     while (*p == ' ') p++;
     if (*p != '$') return 0;
     p++; /* skip $ */
 
     /* collect name */
     uint8_t name[SCRIPT_NAME_MAX];
     uint8_t ni = 0;
     while (*p && *p != ' ' && *p != '=' && ni < SCRIPT_NAME_MAX - 1)
         name[ni++] = *p++;
     name[ni] = 0;
     if (ni == 0) return 0;
 
     while (*p == ' ') p++;
     if (*p != '=') return 0;
     p++;
     while (*p == ' ') p++;
 
     /* find or create slot */
     uint8_t slot = var_find(name);
     if (slot == 0xFF) {
         slot = var_alloc_slot();
         if (slot == 0xFF) {
             console_write_f(PSTR("script: variable table full\n"));
             g_ss.abort = 1;
             return 1;
         }
         var_write_name(slot, name);
     }
 
     /* string literal? */
     if (*p == '"') {
         p++;
         uint8_t strval[SCRIPT_LINE_MAX];
         uint16_t si = 0;
         while (*p && *p != '"' && si < sizeof(strval) - 1)
             strval[si++] = *p++;
         strval[si] = 0;
         /* expand variables inside the string */
         uint8_t expstr[SCRIPT_LINE_MAX];
         expand_vars(strval, expstr, sizeof(expstr));
         var_set_str(slot, expstr);
         return 1;
     }
 
     /* numeric / expression – try float vs int */
     static uint8_t expbuf[SCRIPT_LINE_MAX];
     expand_vars(p, expbuf, sizeof(expbuf));
 
     /* if no '.', 'e', 'E' in expanded string treat as integer */
     uint8_t has_dot = 0;
     for (uint8_t *q = expbuf; *q; q++)
         if (*q == '.' || *q == 'e' || *q == 'E') { has_dot = 1; break; }
 
     if (has_dot) {
         float fv = eval_expr(p);
         var_set_flt(slot, fv);
     } else {
         int32_t iv = (int32_t)eval_expr(p);
         var_set_int(slot, iv);
     }
     return 1;
 }
 
 /* ── for-loop init/increment parser ──────────────────────────────────────── */
 /*
  * Parses the init or increment clause of a for statement.
  * Handles:  $var = expr   and   $var++  $var--  $var+=n  $var-=n
  */
 static void exec_for_clause(uint8_t *clause)
 {
     uint8_t *p = clause;
     while (*p == ' ') p++;
     if (*p != '$') return;
     p++;
 
     uint8_t name[SCRIPT_NAME_MAX]; uint8_t ni = 0;
     while (*p && *p != ' ' && *p != '=' && *p != '+' && *p != '-'
            && ni < SCRIPT_NAME_MAX - 1)
         name[ni++] = *p++;
     name[ni] = 0;
 
     while (*p == ' ') p++;
 
     uint8_t slot = var_find(name);
 
     /* $var++ */
     if (p[0]=='+' && p[1]=='+') {
         if (slot != 0xFF) var_set_int(slot, var_get_int(slot) + 1);
         return;
     }
     /* $var-- */
     if (p[0]=='-' && p[1]=='-') {
         if (slot != 0xFF) var_set_int(slot, var_get_int(slot) - 1);
         return;
     }
     /* $var += n */
     if (p[0]=='+' && p[1]=='=') {
         p+=2; while (*p==' ') p++;
         if (slot != 0xFF) var_set_int(slot, var_get_int(slot) + (int32_t)eval_expr(p));
         return;
     }
     /* $var -= n */
     if (p[0]=='-' && p[1]=='=') {
         p+=2; while (*p==' ') p++;
         if (slot != 0xFF) var_set_int(slot, var_get_int(slot) - (int32_t)eval_expr(p));
         return;
     }
     /* $var = expr */
     if (*p == '=') {
         p++; while (*p==' ') p++;
         if (slot == 0xFF) {
             slot = var_alloc_slot();
             if (slot == 0xFF) { g_ss.abort = 1; return; }
             var_write_name(slot, name);
         }
         var_set_int(slot, (int32_t)eval_expr(p));
     }
 }
 
 /* ── block-body helpers ───────────────────────────────────────────────────── */
 /*
  * A { … } block body is stored as raw text in the serial-RAM block buffer.
  * Lines are separated by '\n'. The buffer holds everything between the
  * opening { line and the matching closing } line (exclusive).
  *
  * Nesting: each '{' seen increments a depth counter; '}' decrements it.
  * We only stop collecting when depth reaches 0.
  */
 
 typedef struct {
     uint32_t addr;   /* base in serial RAM */
     uint32_t size;   /* capacity           */
     uint32_t used;   /* bytes written      */
 } BlockBuf;
 
 static uint8_t bb_append(BlockBuf *bb, const uint8_t *line)
 {
     uint16_t len = (uint16_t)strlen((const char *)line);
     if (bb->used + len + 1u >= bb->size) {
         console_write_f(PSTR("script: block buffer overflow\n"));
         g_ss.abort = 1;
         return 0;
     }
     ram_write_buf(bb->addr + bb->used, line, len);
     bb->used += len;
     ram_write_byte_safe(bb->addr + bb->used++, '\n', SCRIPT_RAM_KEY);
     return 1;
 }
 
 /* Reads one '\n'-terminated line from the block buffer.
    Returns 0 at end of buffer. */
 static uint8_t bb_read_line(BlockBuf *bb, uint32_t *pos,
                             uint8_t *out, uint16_t out_max)
 {
     if (*pos >= bb->used) return 0;
     uint16_t i = 0;
     while (*pos < bb->used && i < out_max - 1) {
         uint8_t c = ram_read_byte_safe(bb->addr + (*pos)++, SCRIPT_RAM_KEY);
         if (c == '\n') break;
         out[i++] = c;
     }
     out[i] = 0;
     return 1;
 }
 
 /* ── forward declaration of the line executor ─────────────────────────────── */
 static void exec_block(BlockBuf *bb);
 
 /* ── collect a { … } block from a file into a BlockBuf ─────────────────── */
 /*
  * Called immediately after a line ending with '{' has been read.
  * file_name and *line_no track the current file read position so we can
  * continue reading after the closing '}'.
  * Returns 1 on success, 0 on error.
  */
 /*
  * collect_block_from_file
  * -----------------------
  * Called immediately after a keyword line (if/while/for) has been consumed
  * by exec_file via FAT_read_file_line.  The FAT cursor is already positioned
  * at the line after the keyword.
  *
  * Opening '{':
  *   May be on the same line as the keyword (already stripped by caller) OR
  *   on its own next line.  We peek at the first line: if it is solely '{'
  *   we discard it and start collecting the body.  Otherwise the body starts
  *   immediately (keyword line ended with '{').
  *
  * Closing '}':
  *   The matching '}' line is consumed but NOT stored in the buffer.
  *   Inner '{'/'}' pairs increment/decrement a depth counter.
  *
  * Uses FAT_read_file_line (sequential) — no re-seek.
  * line_no is passed in/out so exec_file knows where we left the cursor for
  * elif/else lookahead using FAT_read_specific_file_line.
  */
 static uint8_t collect_block_from_file(uint8_t *file_name, uint16_t *line_no,
                                         BlockBuf *bb)
 {
     bb->used = 0;
     uint8_t  raw[SCRIPT_LINE_MAX];
     uint8_t  depth    = 1;
     uint8_t  got_open = 0;
 
     while (!g_ss.abort) {
         if (!fat_read_line(file_name, raw)) {
             console_write_f(PSTR("script: unterminated block\n"));
             g_ss.abort = 1;
             return 0;
         }
         (*line_no)++;
 
         /* line_clean() already stripped \r\n in fat_read_line */
         uint8_t *trim = raw;
         while (*trim == ' ' || *trim == '\t') trim++;
 
         if (!got_open) {
             if (trim[0] == '{' && trim[1] == '\0') {
                 got_open = 1;
                 continue;
             }
             got_open = 1;
         }
 
         if (trim[0] == '}' && trim[1] == '\0') {
             depth--;
             if (depth == 0) return 1;
             bb_append(bb, raw);
             continue;
         }
 
         if (trim[0] == '{' && trim[1] == '\0') {
             depth++;
             bb_append(bb, raw);
             continue;
         }
 
         bb_append(bb, raw);
         if (g_ss.abort) return 0;
     }
     return 0;
 }
 
 /* Same but reading from a parent BlockBuf (for nested structures) */
 static uint8_t collect_block_from_buf(BlockBuf *src, uint32_t *src_pos,
                                        BlockBuf *dst)
 {
     dst->used = 0;
     uint8_t raw[SCRIPT_LINE_MAX];
     uint8_t depth = 1;
 
     while (!g_ss.abort) {
         if (!bb_read_line(src, src_pos, raw, sizeof(raw))) break;
 
         uint8_t *trim = raw;
         while (*trim == ' ' || *trim == '\t') trim++;
 
         uint16_t rlen = (uint16_t)strlen((char *)raw);
         if (rlen > 0 && raw[rlen-1] == '\r') raw[--rlen] = 0;
 
         if (trim[0] == '}') {
             depth--;
             if (depth == 0) return 1;
             bb_append(dst, raw);
         } else {
             if (rlen > 0 && raw[rlen-1] == '{') depth++;
             bb_append(dst, raw);
         }
     }
     console_write_f(PSTR("script: unterminated nested block\n"));
     g_ss.abort = 1;
     return 0;
 }
 
 /* ── strip trailing '{' and extract condition ─────────────────────────────── */
 /*
  * Given a keyword line like  "if($x > 0) {"  or  "while($x) {"
  * returns a pointer into the line to the condition text (inside the
  * outermost parentheses) and strips the trailing '{'.
  */
 static uint8_t *extract_condition(uint8_t *line)
 {
     uint8_t *p = line;
     while (*p && *p != '(') p++;
     if (!*p) return (uint8_t *)"0";
     p++; /* skip '(' */
     uint8_t *start = p;
 
     /* find matching ')' */
     uint8_t depth = 1;
     while (*p) {
         if (*p == '(') depth++;
         if (*p == ')') { depth--; if (!depth) { *p = 0; break; } }
         p++;
     }
     return start;
 }
 
 /* ── single-line executor ─────────────────────────────────────────────────── */
 /*
  * Dispatches one fully-expanded line that is NOT a block opener.
  */
 static void exec_single_line(uint8_t *line)
 {
     /* expand variables */
     static uint8_t expanded[SCRIPT_LINE_MAX];
     expand_vars(line, expanded, sizeof(expanded));
     _process_command(expanded);
 }
 
 /* ── block executor ───────────────────────────────────────────────────────── */
 /*
  * Iterates over lines stored in a BlockBuf and executes them.
  * Handles if/elif/else, while, for natively; everything else goes
  * through exec_single_line → _process_command.
  *
  * To avoid recursive heap allocation of BlockBufs we use a single shared
  * block buffer (g_ss.block_buf_addr) and stack-allocate only the metadata
  * (BlockBuf structs are small: 12 bytes).  Nested blocks use the same
  * backing store by temporarily splitting it.  Maximum nesting is
  * SCRIPT_MAX_DEPTH levels for both file and block nesting combined.
  */
 static void exec_block(BlockBuf *bb)
 {
     uint32_t pos = 0;
     uint8_t  raw[SCRIPT_LINE_MAX];
 
     while (!g_ss.abort && bb_read_line(bb, &pos, raw, sizeof(raw))) {
         /* strip trailing \r */
         uint16_t rlen = (uint16_t)strlen((char *)raw);
         if (rlen > 0 && raw[rlen-1] == '\r') raw[--rlen] = 0;
 
         /* skip blank lines and comments */
         uint8_t *trim = raw;
         while (*trim == ' ' || *trim == '\t') trim++;
         if (!*trim || *trim == '#') continue;
 
         /* standalone { or } at block level — skip */
         if ((trim[0] == '{' || trim[0] == '}') && trim[1] == '\0') continue;
 
         /* ── assignment: $var = … ───────────────────────────────────────── */
         if (*trim == '$') {
             uint8_t *eq = (uint8_t *)strchr((char *)trim, '=');
             if (eq && eq > trim &&
                 eq[-1] != '!' && eq[-1] != '<' && eq[-1] != '>' && eq[1] != '=') {
                 try_assignment(trim);
                 continue;
             }
         }
 
         /*
          * For all child block allocations we carve space from AFTER the
          * current bb->used watermark so we don't overwrite the parent body.
          * child.addr = bb->addr + bb->used  (already past what we've read/kept)
          * child.size = bb->size - bb->used
          * This is safe because bb_read_line advances only `pos`, not bb->used,
          * so bb->used remains the total collected size of this block.
          */
 #define CHILD_BUF(child)                            \
         (child).addr = bb->addr + bb->used;         \
         (child).size = (bb->size > bb->used)        \
                        ? bb->size - bb->used : 0;   \
         (child).used = 0
 
         /* ── if ─────────────────────────────────────────────────────────── */
         if (strncmp((char *)trim, "if(", 3) == 0 ||
             strncmp((char *)trim, "if (", 4) == 0)
         {
             uint8_t  cond_buf[SCRIPT_LINE_MAX];
             strncpy((char *)cond_buf, (char *)trim, sizeof(cond_buf)-1);
             cond_buf[sizeof(cond_buf)-1] = 0;
             uint8_t *cond   = extract_condition(cond_buf);
             float    result = eval_expr(cond);
 
             BlockBuf child; CHILD_BUF(child);
             if (!collect_block_from_buf(bb, &pos, &child)) return;
 
             if (result != 0.0f) {
                 exec_block(&child);
             } else {
                 uint32_t saved_pos = pos;
                 uint8_t  next[SCRIPT_LINE_MAX];
                 while (!g_ss.abort && bb_read_line(bb, &pos, next, sizeof(next))) {
                     uint8_t *nt = next;
                     while (*nt == ' ' || *nt == '\t') nt++;
                     if (!*nt || *nt == '#') continue;
 
                     if (strncmp((char *)nt, "elif(", 5) == 0 ||
                         strncmp((char *)nt, "elif (", 6) == 0)
                     {
                         uint8_t cc[SCRIPT_LINE_MAX];
                         strncpy((char *)cc, (char *)nt, sizeof(cc)-1);
                         cc[sizeof(cc)-1] = 0;
                         float rv = eval_expr(extract_condition(cc));
                         BlockBuf child2; CHILD_BUF(child2);
                         collect_block_from_buf(bb, &pos, &child2);
                         if (rv != 0.0f) { exec_block(&child2); goto if_done; }
                         saved_pos = pos;
                     } else if (strncmp((char *)nt, "else", 4) == 0) {
                         BlockBuf child3; CHILD_BUF(child3);
                         collect_block_from_buf(bb, &pos, &child3);
                         exec_block(&child3);
                         goto if_done;
                     } else {
                         pos = saved_pos; /* put back non-elif/else line */
                         break;
                     }
                 }
             }
             if_done:;
             continue;
         }
 
         /* ── while ──────────────────────────────────────────────────────── */
         if (strncmp((char *)trim, "while(", 6) == 0 ||
             strncmp((char *)trim, "while (", 7) == 0)
         {
             uint8_t cond_buf[SCRIPT_LINE_MAX];
             strncpy((char *)cond_buf, (char *)trim, sizeof(cond_buf)-1);
             cond_buf[sizeof(cond_buf)-1] = 0;
             uint8_t *cond = extract_condition(cond_buf);
 
             BlockBuf body; CHILD_BUF(body);
             if (!collect_block_from_buf(bb, &pos, &body)) return;
 
             while (!g_ss.abort && eval_expr(cond) != 0.0f)
                 exec_block(&body);
             continue;
         }
 
         /* ── for ────────────────────────────────────────────────────────── */
         if (strncmp((char *)trim, "for(", 4) == 0 ||
             strncmp((char *)trim, "for (", 5) == 0)
         {
             uint8_t tmp[SCRIPT_LINE_MAX];
             strncpy((char *)tmp, (char *)trim, sizeof(tmp)-1);
             tmp[sizeof(tmp)-1] = 0;
 
             uint8_t *p = tmp;
             while (*p && *p != '(') p++; if (*p) p++;
             uint8_t *init_s = p;
             while (*p && *p != ';') p++; if (*p) *p++ = 0;
             uint8_t *cond_s = p;
             while (*p && *p != ';') p++; if (*p) *p++ = 0;
             uint8_t *incr_s = p;
             uint8_t *rp2 = (uint8_t *)strchr((char *)incr_s, ')');
             if (rp2) *rp2 = 0;
 
             BlockBuf body; CHILD_BUF(body);
             if (!collect_block_from_buf(bb, &pos, &body)) return;
 
             exec_for_clause(init_s);
             while (!g_ss.abort && eval_expr(cond_s) != 0.0f) {
                 exec_block(&body);
                 exec_for_clause(incr_s);
             }
             continue;
         }
 
 #undef CHILD_BUF
 
         /* ── ordinary command line ──────────────────────────────────────── */
         exec_single_line(trim);
     }
 }
 
 /* ── file-level executor ──────────────────────────────────────────────────── */
 static void exec_file(uint8_t *file_name)
 {
     uint16_t line_no = 0;   /* mirrors FAT cursor — used for error messages only */
     uint8_t  raw[SCRIPT_LINE_MAX];
 
     while (!g_ss.abort) 
     {
         if (!fat_read_line(file_name, raw)) break;
         line_no++;
 
         /* line_clean() already stripped \r\n in fat_read_line */
         uint8_t *trim = raw;
         while (*trim == ' ' || *trim == '\t') trim++;
         if (!*trim || *trim == '#') continue;
 
         if ((trim[0] == '{' || trim[0] == '}') && trim[1] == '\0') continue;
 
         /* ── assignment: $var = <expr> ──────────────────────────────────── */
         if (*trim == '$') {
             uint8_t *eq = (uint8_t *)strchr((char *)trim, '=');
             if (eq && eq > trim &&
                 eq[-1] != '!' && eq[-1] != '<' && eq[-1] != '>' && eq[1] != '=') {
                 try_assignment(trim);
                 continue;
             }
         }
 
         /* ── if(<cond>) ─────────────────────────────────────────────────── */
         if (strncmp((char *)trim, "if(", 3) == 0 ||
             strncmp((char *)trim, "if (", 4) == 0)
         {
             uint8_t cond_buf[SCRIPT_LINE_MAX];
             strncpy((char *)cond_buf, (char *)trim, sizeof(cond_buf) - 1);
             cond_buf[sizeof(cond_buf) - 1] = 0;
             float result = eval_expr(extract_condition(cond_buf));
 
             BlockBuf body;
             body.addr = g_ss.block_buf_addr;
             body.size = g_ss.block_buf_size;
             body.used = 0;
             if (!collect_block_from_file(file_name, &line_no, &body)) return;
 
             if (result != 0.0f) {
                 exec_block(&body);
                 goto file_skip_rest;    /* skip any following elif/else */
             }
 
             /* condition false — walk elif/else chain */
             while (!g_ss.abort) {
                 uint8_t nxt[SCRIPT_LINE_MAX];
                 if (!fat_read_line(file_name, nxt)) break;
                 line_no++;
 
                 uint8_t *nt = nxt;
                 while (*nt == ' ' || *nt == '\t') nt++;
 
                 if (!*nt || *nt == '#') continue;
 
                 if (strncmp((char *)nt, "elif(", 5) == 0 ||
                     strncmp((char *)nt, "elif (", 6) == 0)
                 {
                     uint8_t cc[SCRIPT_LINE_MAX];
                     strncpy((char *)cc, (char *)nt, sizeof(cc) - 1);
                     cc[sizeof(cc) - 1] = 0;
                     float rv = eval_expr(extract_condition(cc));
 
                     BlockBuf eb;
                     eb.addr = g_ss.block_buf_addr;
                     eb.size = g_ss.block_buf_size;
                     eb.used = 0;
                     if (!collect_block_from_file(file_name, &line_no, &eb)) return;
 
                     if (rv != 0.0f) {
                         exec_block(&eb);
                         goto file_skip_rest;
                     }
                     /* false — continue to next elif/else */
                 }
                 else if (strncmp((char *)nt, "else", 4) == 0 &&
                          (nt[4] == '\0' || nt[4] == ' ' || nt[4] == '{'))
                 {
                     BlockBuf eb;
                     eb.addr = g_ss.block_buf_addr;
                     eb.size = g_ss.block_buf_size;
                     eb.used = 0;
                     if (!collect_block_from_file(file_name, &line_no, &eb)) return;
                     exec_block(&eb);
                     goto file_if_done;
                 }
                 else {
                     /* Not elif/else — put line back and stop chain walk */
                     fat_unread_line(nxt);
                     line_no--;
                     break;
                 }
             }
             goto file_if_done;
 
             file_skip_rest:
             /* A branch was taken — discard all remaining elif/else bodies */
             while (!g_ss.abort) {
                 uint8_t nxt[SCRIPT_LINE_MAX];
                 if (!fat_read_line(file_name, nxt)) break;
                 line_no++;
 
                 uint8_t *nt = nxt;
                 while (*nt == ' ' || *nt == '\t') nt++;
                 if (!*nt || *nt == '#') continue;
 
                 if (strncmp((char *)nt, "elif(", 5) == 0 ||
                     strncmp((char *)nt, "elif (", 6) == 0 ||
                     (strncmp((char *)nt, "else", 4) == 0 &&
                      (nt[4] == '\0' || nt[4] == ' ' || nt[4] == '{')))
                 {
                     BlockBuf discard;
                     discard.addr = g_ss.block_buf_addr;
                     discard.size = g_ss.block_buf_size;
                     discard.used = 0;
                     if (!collect_block_from_file(file_name, &line_no, &discard))
                         return;
                 } else {
                     fat_unread_line(nxt);
                     line_no--;
                     break;
                 }
             }
 
             file_if_done:;
             continue;
         }
 
         /* ── while(<cond>) ──────────────────────────────────────────────── */
         if (strncmp((char *)trim, "while(", 6) == 0 ||
             strncmp((char *)trim, "while (", 7) == 0)
         {
             uint8_t cond_buf[SCRIPT_LINE_MAX];
             strncpy((char *)cond_buf, (char *)trim, sizeof(cond_buf) - 1);
             cond_buf[sizeof(cond_buf) - 1] = 0;
             uint8_t *cond = extract_condition(cond_buf);
 
             BlockBuf body;
             body.addr = g_ss.block_buf_addr;
             body.size = g_ss.block_buf_size;
             body.used = 0;
             if (!collect_block_from_file(file_name, &line_no, &body)) return;
 
             while (!g_ss.abort && eval_expr(cond) != 0.0f)
                 exec_block(&body);
             continue;
         }
 
         /* ── for($i=0; $i<N; $i++) ──────────────────────────────────────── */
         if (strncmp((char *)trim, "for(", 4) == 0 ||
             strncmp((char *)trim, "for (", 5) == 0)
         {
             uint8_t tmp[SCRIPT_LINE_MAX];
             strncpy((char *)tmp, (char *)trim, sizeof(tmp) - 1);
             tmp[sizeof(tmp) - 1] = 0;
 
             uint8_t *p = tmp;
             while (*p && *p != '(') p++;
             if (*p) p++;
             uint8_t *init_s = p;
             while (*p && *p != ';') p++; if (*p) *p++ = 0;
             uint8_t *cond_s = p;
             while (*p && *p != ';') p++; if (*p) *p++ = 0;
             uint8_t *incr_s = p;
             uint8_t *rp = (uint8_t *)strchr((char *)incr_s, ')');
             if (rp) *rp = 0;
 
             BlockBuf body;
             body.addr = g_ss.block_buf_addr;
             body.size = g_ss.block_buf_size;
             body.used = 0;
             if (!collect_block_from_file(file_name, &line_no, &body)) return;
 
             exec_for_clause(init_s);
             while (!g_ss.abort && eval_expr(cond_s) != 0.0f) {
                 exec_block(&body);
                 exec_for_clause(incr_s);
             }
             continue;
         }
 
         /* ── ordinary command ───────────────────────────────────────────── */
         exec_single_line(trim);
     }
     /* FAT_read_file_line closes the file automatically on EOF */
 }
 
 /* ── public command entry point ───────────────────────────────────────────── */
 /*
  * Usage:  script [--buffer-size <bytes>] <filename>
  */
 void _cmd_script(uint8_t argc, uint8_t *argv[])
 {
     if (argc < 2) {
         console_write_f(PSTR("usage: script [--buffer-size <bytes>] <file>\n"));
         return;
     }
 
     /* ── parse optional --buffer-size ──────────────────────────────────── */
     uint32_t block_buf_size = SCRIPT_DEFAULT_BUF;
     uint8_t  file_arg       = 1;
 
     if (argc >= 4 &&
         strcmp((char *)argv[1], "--buffer-size") == 0)
     {
         block_buf_size = (uint32_t)strtoul((char *)argv[2], NULL, 10);
         if (block_buf_size == 0) block_buf_size = SCRIPT_DEFAULT_BUF;
         file_arg = 3;
     }
 
     uint8_t *file_name = argv[file_arg];
 
     /* ── depth guard ────────────────────────────────────────────────────── */
     if (g_ss.depth >= SCRIPT_MAX_DEPTH) {
         console_write_f(PSTR("script: max recursion depth reached\n"));
         return;
     }
 
     uint8_t is_root = (g_ss.depth == 0);
 
     /* ── first call: initialise global state ────────────────────────────── */
     if (is_root) {
         g_ss.total_used     = 0;
         g_ss.abort          = 0;
         g_ss.var_table_addr = 0;
         g_ss.block_buf_addr = 0;
         g_ss.block_buf_size = block_buf_size;
         g_ss.pb_valid       = 0;
 
         /* allocate variable table */
         uint32_t vt_size = (uint32_t)SCRIPT_VAR_MAX * VAR_SLOT_BYTES;
         g_ss.var_table_addr = script_alloc(vt_size);
         if (!g_ss.var_table_addr) 
         {  
            console_write_f(PSTR("script : allocation error\n"));
            return;
         }
         var_table_init();
 
         /* allocate block buffer */
         g_ss.block_buf_addr = script_alloc(block_buf_size);
         if (!g_ss.block_buf_addr) 
         {
            console_write_f(PSTR("script : allocation error\n"));
            var_table_free();
            return;
         }
     }
 
     /* ── execute ────────────────────────────────────────────────────────── */
     g_ss.depth++;
     exec_file(file_name);
     g_ss.depth--;
 
     /* ── on root return: free all per-script variables ──────────────────── */
     if (is_root) {
         /* Mark all variable slots as free so the next top-level script call
            starts with a clean table.  String cells in serial RAM are not
            individually freed (no free() API), but total_used is reset so the
            next root call's script_alloc() calls will reuse the same range. */
         if (g_ss.var_table_addr)
             var_table_free();
 
         /* Reset budget accounting so next top-level call starts fresh.
            The actual serial-RAM slots are considered reused implicitly. */
         ram_free_mem(g_ss.var_table_addr);
         g_ss.total_used     = 0;
         g_ss.var_table_addr = 0;
         g_ss.block_buf_addr = 0;
         g_ss.depth          = 0;
         g_ss.abort          = 0;
         g_ss.pb_valid       = 0;
     }
 }