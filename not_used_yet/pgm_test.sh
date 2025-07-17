echo "Making hex"
#avr-gcc -mmcu=atxmega128a3u -Os ili9488_x128a3u.c -o salida.o
avr-gcc -mmcu=atxmega128a3u -Os -c fonts.c -o salida1.o
avr-gcc -mmcu=atxmega128a3u -Os -c ll_x128a3u.c -o salida2.o
avr-gcc -mmcu=atxmega128a3u -Os -c x128_ili9488_driver.c -o salida3.o
avr-gcc -mmcu=atxmega128a3u -Os -c main.c -o salida4.o
avr-gcc -mmcu=atxmega128a3u salida1.o salida2.o salida3.o salida4.o -o poronga.elf
avr-objcopy -j .text -j .data -j .bootloader -O ihex poronga.elf salida.hex
echo "Programing MCU"
avrdude -p x128a3u -c avrispmkii -e -U flash:w:salida.hex
#avrdude -c avrispmkii -p atxmega128a3u -U fuse5:w:0xF7:m
echo "Done"
