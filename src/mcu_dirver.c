
#include <avr/io.h>
#include <stdint.h>
#include "mcu_driver.h"

uint16_t system_date = 0;
uint16_t system_time = 0;

void mcu_set_32mhz()
{
    OSC.CTRL |= OSC_RC32MEN_bm;    // Enabled the internal 32MHz oscillator
    while ((OSC.STATUS & OSC_RC32MRDY_bm) == 0)
      ;  // wait for oscillator to finish starting.
    CPU_CCP = CCP_IOREG_gc;  // tickle the Configuration Change Protection Register
    CLK.CTRL = CLK_SCLKSEL_RC32M_gc;   // select the 32MHz oscillator as system clock.

    //WDT.CTRL = WDT_DISABLE_gc; // Disable watchdog timer
}

uint8_t mcu_memory_read(uint16_t addr)
{
    // Cast the address to a pointer and dereference it
    // This works for most memory-mapped regions
    volatile uint8_t* mem_ptr = (volatile uint8_t*)addr;
    return *mem_ptr;
}

void mcu_memory_store(uint16_t addr, uint8_t value) // Super dangerous function
{
    // Cast the address to a pointer and store the value
    // This works for most memory-mapped regions
    volatile uint8_t* mem_ptr = (volatile uint8_t*)addr;
    *mem_ptr = value;
}

// Alternative implementation with more common RTC formats
void rtc_time_date_to_components(uint16_t time, uint16_t date, datetime_components_t* components)
{
    // Assuming time format: BCD HHMMSS (24-hour format)
    components->second = ((time & 0x003F) << 2) | ((time & 0x00C0) >> 6);
    components->minute = ((time & 0x03C0) >> 4) | ((time & 0x0C00) >> 10);
    components->hour = ((time & 0x3000) >> 12) | ((time & 0xC000) >> 14);

    // Assuming date format: BCD DDMMAAAA
    components->day = ((date & 0x00F0) >> 4) | ((date & 0x0300) >> 8);
    components->month = (date & 0x000F) | ((date & 0x00F0) >> 4);
    components->year = ((date & 0x0F00) >> 8) | ((date & 0xF000) >> 12);
}

void components_to_rtc_time_date(datetime_components_t* components, uint16_t* time, uint16_t* date)
{
    // Pack time as BCD HHMMSS
    *time = ((components->hour & 0x1F) << 12) |
    ((components->minute & 0x3F) << 4) |
    (components->second & 0x3F);

    // Pack date as BCD DDMMAAAA
    *date = ((components->day & 0x1F) << 10) |
    ((components->month & 0x0F) << 6) |
    ((components->year - 2000) & 0x3F);
}