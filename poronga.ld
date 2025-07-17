
/* Linker script for Atmel ATxmega128A3U
 * Memory Specifications:
 * Flash: 128KB
 * SRAM: 8KB
 * EEPROM: 2KB
 */

 /* Provide symbols for tracking data initialization */
PROVIDE(_data_flash = LOADADDR(.data));
PROVIDE(_data_start = ADDR(.data));
PROVIDE(_data_end = ADDR(.data) + SIZEOF(.data));

MEMORY
{
  FLASH (rx) : ORIGIN = 0x0000, LENGTH = 128K
  SRAM (rw!x) : ORIGIN = 0x2000, LENGTH = 8K
  EEPROM (rw!x) : ORIGIN = 0x1000, LENGTH = 2K
  LCD_CONFIG_FLASH (rx) : ORIGIN = 0x2000, LENGTH = 256
}

/* Section Definitions */
SECTIONS
{
    .text :
    {
        . = ALIGN(2);
        *(.vectors)
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(2);
    } > FLASH

    .data :
    {
        . = ALIGN(2);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(2);
        _edata = .;
    } > SRAM AT > FLASH

    .bss :
    {
        . = ALIGN(2);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(2);
        _ebss = .;
    } > SRAM

    /* EEPROM Section */
    .eeprom (NOLOAD) :
    {
        *(.eeprom*)
    } > EEPROM

    /* Stack space */
    .stack (NOLOAD) :
    {
        *(.stack)
        . = ALIGN(2);
        PROVIDE(_stack_top = .);
    } > SRAM
    .lcd_config_data :
    {
        KEEP(*(.lcd_config_data))
    } > LCD_CONFIG_FLASH
}

/* Entry point */
ENTRY(Reset_Handler)

