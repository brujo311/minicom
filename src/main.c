
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
//#include <stdlib.h>
//#include <stdint.h> // console
//#include <string.h> // console
#include "mcu_driver.h"
#include "lcd_driver.h"
//#include "FAT32.h"
//#include "SD_routines.h"
#include "ningen_ui.h"
//#include "colors.h"
//#include "ram_driver.h"
//#include "kb_driver.h"
#include "console.h"

int main()
{
    mcu_set_32mhz();

    lcd_init_controller();

    //SD_check_alive();

    wish_init();
    wish_run();

    lcd_write_debug("Console exit");

    while (1) {
        // Your main application code here (if any)
		if(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
		;
		if(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))
		;
    }

    return 0;
}
