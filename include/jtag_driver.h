
// JTAG pin definitions on PORTB
#define JTAG_PORT PORTA_OUT
#define JTAG_PIN  PORTA_IN
#define JTAG_DDR  PORTA_DIR

#define PIN_TCK  (1<<7)
#define PIN_TMS  (1<<1)
#define PIN_TDI  (1<<5)
#define PIN_TDO  (1<<3)
#define PIN_RST  (1<<6)

/*static inline void tck_pulse(void);
static void jtag_init_pins(void);
static void jtag_tap_reset(void);
static void jtag_go_to_run_idle(void);
static void jtag_goto_shift_ir(void);
static void jtag_goto_shift_dr(void);
static uint32_t jtag_shift_ir(uint32_t ir, uint8_t length);
static uint32_t jtag_shift_dr(uint8_t length);
static uint32_t jtag_select_ir_and_read_dr(uint32_t ir, uint8_t ir_len);*/

void jtag_reset(void);
void jtag_init(void);
uint8_t jtag_get_device_id(uint8_t find, uint8_t * readback);