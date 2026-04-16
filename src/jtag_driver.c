#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include "jtag_driver.h"
#include "lcd_driver.h"

// JTAG implementation

// low-level pin ops
static inline void tck_high(void){ JTAG_PORT |= PIN_TCK; }
static inline void tck_low(void){  JTAG_PORT &= ~PIN_TCK; }
static inline void tms_high(void){ JTAG_PORT |= PIN_TMS; }
static inline void tms_low(void){  JTAG_PORT &= ~PIN_TMS; }
static inline void tdi_high(void){ JTAG_PORT |= PIN_TDI; }
static inline void tdi_low(void){  JTAG_PORT &= ~PIN_TDI; }
static inline void rst_high(void){ JTAG_PORT |= PIN_RST; }
static inline void rst_low(void){  JTAG_PORT &= ~PIN_RST; }
static inline uint8_t tdo_read(void){ return (JTAG_PIN & PIN_TDO) ? 1 : 0; }

// tiny delay to control TCK pulse width
static inline void tck_pulse(void){
    // TCK low -> high -> low
    tck_high();
    // short hold
    _delay_us(5);
    tck_low();
    _delay_us(5);
}

// Initialize pins
static void jtag_init_pins(void){
    // outputs: TCK,TMS,TDI
    JTAG_DDR |= (PIN_RST);
    // input: TDO
    JTAG_DDR &= ~PIN_TDO;
    // default states
    rst_low();
    _delay_ms(100);

    JTAG_DDR |= (PIN_TCK | PIN_TMS | PIN_TDI);

    tck_low();
    tms_high(); // leave TAP in reset if idle
    tdi_low();

    _delay_ms(10);

    rst_high();

    _delay_ms(10);
}

// TAP reset: hold TMS=1 for >=5 TCK cycles
static void jtag_tap_reset(void){

    tms_high();
    for(int i=0;i<5;i++){
        tck_pulse();
    }
    // stay in Test-Logic-Reset
}

// Move TAP to Run-Test/Idle from current state by TMS=0 then pulse once
static void jtag_go_to_run_idle(void){
    tms_low();
    tck_pulse();
}

// Generic: go to Shift-IR state
static void jtag_goto_shift_ir(void){
    // From reset: go to Run-Test/Idle first
    // Sequence: TMS=0 (-> Run-Test/Idle), then TMS=1 (Select-DR-Scan),
    // then TMS=1 (Select-IR-Scan), then TMS=0 (Capture-IR), then TMS=0 (Shift-IR)
    tms_low(); tck_pulse();            // RTI
    tms_high(); tck_pulse();           // Select-DR-Scan
    tms_high(); tck_pulse();           // Select-IR-Scan
    tms_low(); tck_pulse();            // Capture-IR
    tms_low(); tck_pulse();            // Shift-IR
}

// Generic: go to Shift-DR state
static void jtag_goto_shift_dr(void){
    // From RTI: TMS=1 (Select-DR-Scan), TMS=0 (Capture-DR), TMS=0 (Shift-DR)
    tms_low(); tck_pulse();            // RTI
    tms_high(); tck_pulse();           // Select-DR-Scan
    tms_low(); tck_pulse();            // Capture-DR
    tms_low(); tck_pulse();            // Shift-DR
}

// Shift bits LSB-first into IR, returning the TDO of the last bit shifted out (if needed).
// length bits; ir holds the bits LSB-first (bit0 first).
static uint32_t jtag_shift_ir(uint32_t ir, uint8_t length){
    uint32_t tdo_val = 0;
    // go to Shift-IR
    jtag_goto_shift_ir();
    for(uint8_t i=0;i<length;i++){
        // set TDI according to bit i
        if (ir & (1u<<i)) tdi_high(); else tdi_low();
        // if last bit => set TMS high to exit to Update-IR on next TCK
        if (i == length-1) tms_high(); else tms_low();
        // pulse
        tck_pulse();
        // read TDO (capture into tdo_val LSB-first)
        if (tdo_read()) tdo_val |= (1u<<i);
        // leave TMS as set for next iteration
    }
    // take one more TCK to move through Update-IR -> Run-Test/Idle
    tck_pulse();
    // ensure we end in Run-Test/Idle
    tms_low();
    tck_pulse();
    return tdo_val;
}

// Shift length bits LSB-first in DR; return assembled uint32_t (only up to 32 bits)
static uint32_t jtag_shift_dr(uint8_t length){
    uint32_t out = 0;
    // go to Shift-DR
    jtag_goto_shift_dr();
    for(uint8_t i=0;i<length;i++){
        // keep TDI low for read-only (or you may send zeros)
        tdi_low();
        // set TMS=1 on last bit to exit
        if (i == length-1) tms_high(); else tms_low();
        tck_pulse();
        if (tdo_read()) out |= (1u<<i);
    }
    // leave RTI
    tck_pulse();
    tms_low();
    tck_pulse();
    return out;
}

// A helper: try to select an IR (with given width) then read DR 32-bit
static uint32_t jtag_select_ir_and_read_dr(uint32_t ir, uint8_t ir_len){
    jtag_tap_reset();
    jtag_go_to_run_idle();
    jtag_shift_ir(ir, ir_len);
    // now shift DR and read 32 bits (IDCODE typically 32 bits)
    uint32_t r = jtag_shift_dr(32);
    return r;
}

void jtag_reset()
{
    JTAG_DDR |= (PIN_RST);

    JTAG_DDR &= ~(PIN_TDO | PIN_TCK | PIN_TMS | PIN_TDI);

    rst_low();
    _delay_ms(100);

    rst_high();

    _delay_ms(10);
}

void jtag_init()
{
    jtag_init_pins();
    jtag_tap_reset();
    jtag_go_to_run_idle();
}

uint8_t jtag_get_device_id(uint8_t find, uint8_t * readback)
{
    const uint8_t try_ir_len[] = {4, 5, 6, 8};
    const uint32_t try_ir_codes[] = {0x01, 0x09, 0x02, 0x00}; // examples
    for( uint8_t L_i = 0; L_i < sizeof(try_ir_len); L_i++ )
    {
        uint8_t irlen = try_ir_len[L_i];
        readback[8] = irlen;
        for( uint8_t c = 0; c < sizeof(try_ir_codes) / sizeof(try_ir_codes[0]); c++ )
        {
            uint32_t ir = try_ir_codes[c];
            memcpy(&readback[4], &ir, 4);
            // print up to two hex digits of IR (LSB-first representation)
            // show IR as MSB-first for readability: we print the value
            uint32_t r = jtag_select_ir_and_read_dr(ir, irlen);
            memcpy(&readback[0], &r, 4);

            for(uint8_t a = 0; a < 4; a++) if(readback[a] == find) return 1;
        }
    }
    return 0;
}