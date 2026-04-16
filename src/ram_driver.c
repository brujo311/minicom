
#include <avr/io.h>
#include "mcu_driver.h"
#include "ram_driver.h"

uint8_t ram_status = 0;

static inline void ram_spi_send(uint8_t data)
{
    RAM_SPI_DATA = data;
    while(!RAM_SPI_IF);
}

static inline uint8_t ram_spi_transfer(uint8_t data)
{
    RAM_SPI_DATA = data;
    while(!RAM_SPI_IF);
    return RAM_SPI_DATA;
}

void ram_init()
{
    PORTC_DIR = 0b10111100;
    PORTC_OUT = 0b00011100;
    RAM_SPI_MAX_SET;
}

static inline void assert_ram(uint32_t add)
{
    uint8_t chip = (add >> 17) & 0x0f;  // each chip = 128KB

    switch(chip)
    {
        case 0:
            PORTC_OUT &= ~(0b00001000); // CS0 low
            break;
        case 1:
            PORTC_OUT &= ~(0b00000100); // CS1 low
            break;
        case 2:
            PORTC_OUT &= ~(0b00010000); // CS2 low
            break;
    }
}

static inline void deassert_ram()
{
    PORTC_OUT |= (0b00011100);
}

void ram_write_byte(uint32_t add, uint8_t data)
{
    assert_ram(add);
    ram_spi_transfer(0x02);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    ram_spi_transfer(data);
    deassert_ram();
}

uint8_t ram_read_byte(uint32_t add)
{
    uint8_t data;
    assert_ram(add);
    ram_spi_transfer(0x03);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    data = ram_spi_transfer(0x00);
    deassert_ram();
    return data;
}

void ram_write_word(uint32_t add, uint16_t data)
{
    assert_ram(add);
    ram_spi_transfer(0x02);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    ram_spi_transfer((uint8_t)(data >> 8));
    ram_spi_transfer((uint8_t)data);
    deassert_ram();
}

uint16_t ram_read_word(uint32_t add)
{
    uint16_t data;
    assert_ram(add);
    ram_spi_transfer(0x03);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    data = (uint16_t)(ram_spi_transfer(0x00) << 8);
    data |= (uint16_t)ram_spi_transfer(0x00);
    deassert_ram();
    return data;
}

void ram_write_dword(uint32_t add, uint32_t data)
{
    assert_ram(add);
    ram_spi_transfer(0x02);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    ram_spi_transfer((uint8_t)(data >> 24));
    ram_spi_transfer((uint8_t)(data >> 16));
    ram_spi_transfer((uint8_t)(data >> 8));
    ram_spi_transfer((uint8_t)data);
    deassert_ram();
}

uint32_t ram_read_dword(uint32_t add)
{
    uint32_t data;
    assert_ram(add);
    ram_spi_transfer(0x03);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    data = (uint32_t)(ram_spi_transfer(0x00) << 24);
    data |= (uint32_t)(ram_spi_transfer(0x00) << 16);
    data |= (uint32_t)(ram_spi_transfer(0x00) << 8);
    data |= (uint32_t)ram_spi_transfer(0x00);
    deassert_ram();
    return data;
}

void ram_write(uint32_t add, uint8_t * data, uint8_t bytes)
{
    assert_ram(add);
    ram_spi_transfer(0x02);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    for(uint8_t a = 0; a < bytes; a++) ram_spi_transfer(data[a]);
    deassert_ram();
}

void ram_read(uint32_t add, uint8_t * data, uint8_t bytes)
{
    assert_ram(add);
    ram_spi_transfer(0x03);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    for(uint8_t a = 0; a < bytes; a++) data[a] = ram_spi_transfer(0x00);
    deassert_ram();
}

uint8_t ram_test()
{
    uint32_t add;
    uint8_t a = 7;

    for(add = 0; add < 0x20000; add++)
    {
        ram_write_byte(add, (uint8_t)add);
        if(ram_read_byte(add) != (uint8_t)add) { a &= ~(1); break; }
        ram_write_byte(add, 0);
    }
    for(add = 0x20000; add < 0x40000; add++)
    {
        ram_write_byte(add, (uint8_t)add);
        if(ram_read_byte(add) != (uint8_t)add) { a &= ~(2); break; }
        ram_write_byte(add, 0);
    }
    for(add = 0x40000; add < 0x60000; add++)
    {
        ram_write_byte(add, (uint8_t)add);
        if(ram_read_byte(add) != (uint8_t)add) { a &= ~(4); break; }
        ram_write_byte(add, 0);
    }
    ram_status = a;
    return a;
}