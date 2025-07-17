#!/bin/bash

# Function to reset MCU
reset_mcu() {
  echo "Resetting MCU"
  avrdude -p x128a3u -c avrispmkii -R reset || { echo "Error resetting MCU"; exit 1; }
  echo "MCU Reset"
}

# Check if the argument is '-r' or 'r'
if [[ "$1" == "-r" || "$1" == "r" ]]; then
  reset_mcu
  exit 0
fi

echo "Making hex"

# Compile each source file, and if any fails, exit the script
avr-gcc -mmcu=atxmega128a3u -Os -c fonts.c -o salida1.o || { echo "Error compiling fonts.c"; exit 1; }
avr-gcc -mmcu=atxmega128a3u -Os -c strings.c -o salida2.o || { echo "Error compiling strings.c"; exit 1; }
avr-gcc -mmcu=atxmega128a3u -Os -c ll_x128a3u.c -o salida3.o || { echo "Error compiling ll_x128a3u.c"; exit 1; }
avr-gcc -mmcu=atxmega128a3u -Os -c x128_ili9488_driver.c -o salida4.o || { echo "Error compiling x128_ili9488_driver.c"; exit 1; }
avr-gcc -mmcu=atxmega128a3u -Os -c ningen_ui.c -o salida5.o || { echo "Error compiling ningen_ui.c"; exit 1; }
avr-gcc -mmcu=atxmega128a3u -Os -c main.c -o salida6.o || { echo "Error compiling main.c"; exit 1; }

# Link object files into the final ELF
avr-gcc -mmcu=atxmega128a3u salida1.o salida2.o salida3.o salida4.o salida5.o salida6.o -o poronga.elf || { echo "Error linking files"; exit 1; }

# Convert ELF to HEX format
avr-objcopy -j .text -j .data -j .bootloader -O ihex poronga.elf salida.hex || { echo "Error creating HEX file"; exit 1; }

echo "Programming MCU"

# Program the MCU only if the previous steps were successful
avrdude -p x128a3u -c avrispmkii -e -U flash:w:salida.hex || { echo "Error programming MCU"; exit 1; }

echo "Done"


