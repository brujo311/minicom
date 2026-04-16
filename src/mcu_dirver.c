
#include <avr/io.h>


void mcu_set_32mhz()
{
    OSC.CTRL |= OSC_RC32MEN_bm;    // Enabled the internal 32MHz oscillator
    while ((OSC.STATUS & OSC_RC32MRDY_bm) == 0)
      ;  // wait for oscillator to finish starting.
    CPU_CCP = CCP_IOREG_gc;  // tickle the Configuration Change Protection Register
    CLK.CTRL = CLK_SCLKSEL_RC32M_gc;   // select the 32MHz oscillator as system clock.
}