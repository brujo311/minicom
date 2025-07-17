
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include "lcd_driver.h"
//#include "ningen_ui.h"
#include "colors.h"

int main() {

    OSC.CTRL |= OSC_RC32MEN_bm;    // Enabled the internal 32MHz oscillator
    while ((OSC.STATUS & OSC_RC32MRDY_bm) == 0)
      ;  // wait for oscillator to finish starting.
    CPU_CCP = CCP_IOREG_gc;  // tickle the Configuration Change Protection Register
    CLK.CTRL = CLK_SCLKSEL_RC32M_gc;   // select the 32MHz oscillator as system clock.

    //WDT.CTRL = WDT_DISABLE_gc; // Disable watchdog timer
	//Bluetooth_off();
    lcd_init_controller();

	//boot();

    lcd_set_position(0, 0);
    lcd_number(mem_check(), 10, WHITE, 3, NULL, NULL);
    _delay_ms(5000);

    lcd_clear_screen(BLACK);

    while(!touch_sense()) ;

    while(touch_sense()) ;

    ui_draw_form(0);

    _delay_ms(5000);

    ui_draw_list_box(0, 5, 0, "A ver\nQue onda\nCon texto\nDinamico\nLa concha");

    _delay_ms(5000);

    ui_draw_form(0);

    ui_draw_list_box(0, 5, 0, NULL);

    while(1 == 1) ui_handle_touch(0);

    lcd_set_position(200, 240);
    lcd_number(mem_check(), 10, WHITE, 3, NULL, NULL);

    while (1) {
        // Your main application code here (if any)
		if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
		;
		if(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))
		;
    }

    return 0;
}
