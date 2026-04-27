
#include <avr/io.h>
#include <stdlib.h>
#include "mcu_driver.h"
#include "ram_driver.h"
#include "console.h"

uint32_t ram_data_addr = 0;
uint32_t ram_data_size = 0;
uint32_t ram_data_key = 0;

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

static void _w_ram_slot_size(uint8_t slot, uint32_t data)
{
    ram_write_dword(ram_data_size + (slot * 4), data);
}

static void _w_ram_slot_addr(uint8_t slot, uint32_t data)
{
    ram_write_dword(ram_data_addr + (slot * 4), data);
}

static void _w_ram_slot_key(uint8_t slot, uint8_t data)
{
    ram_write_byte(ram_data_key + slot, data);
}

uint8_t ram_init()
{
    PORTC_DIR = 0b10111100;
    PORTC_OUT = 0b00011100;
    RAM_SPI_MAX_SET;
    ram_test();
    if(ram_status)
    {
        uint32_t base = 0;
        if(ram_status & 0x04) base = 0x80040000;
        if(ram_status & 0x02) base = 0x80020000;
        if(ram_status & 0x01) base = 0x80000000;
        ram_data_addr = base;
        ram_data_size = (MAX_SLOTS * 4) + base;
        ram_data_key = (MAX_SLOTS * 8) + base;
        _w_ram_slot_size(0, (MAX_SLOTS * 4));
        _w_ram_slot_addr(0, ram_data_addr);
        _w_ram_slot_key(0, 0x01);
        _w_ram_slot_size(1, (MAX_SLOTS * 4));
        _w_ram_slot_addr(1, ram_data_size);
        _w_ram_slot_key(1, 0x01);
        _w_ram_slot_size(2, MAX_SLOTS);
        _w_ram_slot_addr(2, ram_data_key);
        _w_ram_slot_key(2, 0x01);
        for(uint8_t a = 3; a < MAX_SLOTS; a++)
        {
            _w_ram_slot_size(a, 0);
            _w_ram_slot_addr(a, 0);
            _w_ram_slot_key(a, 0);
        }
        return 1;
    }
    return 0;
}

void ram_get_slot(uint8_t slot, uint32_t *addr, uint32_t *size, uint8_t *key)
{
    uint32_t addr_buff = get_ram_slot_addr(slot);
    uint32_t size_buff = get_ram_slot_size(slot);
    uint8_t key_buff = get_ram_slot_key(slot);
    addr = &addr_buff;
    size = &size_buff;
    key = &key_buff;
}

uint32_t get_ram_slot_size(uint8_t slot)
{
    return ram_read_dword(ram_data_size + (slot * 4));
}

uint32_t get_ram_slot_addr(uint8_t slot)
{
    return ram_read_dword(ram_data_addr + (slot * 4));
}

uint8_t get_ram_slot_key(uint8_t slot)
{
    return ram_read_byte(ram_data_key + slot);
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
    data = (uint32_t)ram_spi_transfer(0x00) << 24;
    data |= (uint32_t)ram_spi_transfer(0x00) << 16;
    data |= (uint32_t)ram_spi_transfer(0x00) << 8;
    data |= (uint32_t)ram_spi_transfer(0x00);
    deassert_ram();
    return data;
}

void ram_write(uint32_t add, uint8_t * data, uint16_t bytes)
{
    assert_ram(add);
    ram_spi_transfer(0x02);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    for(uint16_t a = 0; a < bytes; a++) ram_spi_transfer(data[a]);
    deassert_ram();
}

void ram_read(uint32_t add, uint8_t * data, uint16_t bytes)
{
    assert_ram(add);
    ram_spi_transfer(0x03);
    ram_spi_transfer((uint8_t)(add >> 16));
    ram_spi_transfer((uint8_t)(add >> 8));
    ram_spi_transfer((uint8_t)add);
    for(uint16_t a = 0; a < bytes; a++) data[a] = ram_spi_transfer(0x00);
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

// Allocate memory block
uint32_t ram_allocate(uint32_t size)
{
    if(size == 0) return 0;

    const uint32_t chip_base[3] = { 0x000000, 0x020000, 0x040000 };
    const uint32_t chip_size    = 0x020000;

    uint8_t chip = 0;
    while(chip < 3)
    {
        if(!(ram_status & (1 << chip))) { chip++; continue; }

        uint8_t run_start = chip;
        while(chip < 3 && (ram_status & (1 << chip))) chip++;
        uint8_t run_end = chip;

        uint32_t region_start = chip_base[run_start];
        uint32_t region_end   = chip_base[run_end - 1] + chip_size;

        if(size > (region_end - region_start)) continue;

        uint32_t slot_start[MAX_SLOTS];
        uint32_t slot_end[MAX_SLOTS];
        uint8_t  slot_count = 0;

        for(uint8_t a = 0; a < MAX_SLOTS; a++)
        {
            if(!get_ram_slot_size(a)) continue;
            uint32_t s = get_ram_slot_addr(a) & 0x00FFFFFF;
            uint32_t e = s + get_ram_slot_size(a);

            if(e <= region_start || s >= region_end) continue;
            slot_start[slot_count] = s;
            slot_end[slot_count]   = e;
            slot_count++;
        }

        // sort
        for(uint8_t i = 1; i < slot_count; i++)
        {
            uint32_t ks = slot_start[i], ke = slot_end[i];
            int8_t j = i - 1;
            while(j >= 0 && slot_start[j] > ks)
            {
                slot_start[j+1] = slot_start[j];
                slot_end[j+1]   = slot_end[j];
                j--;
            }
            slot_start[j+1] = ks;
            slot_end[j+1]   = ke;
        }

        uint32_t search = region_start;

        for(uint8_t i = 0; i <= slot_count; i++)
        {
            uint32_t gap_end = (i < slot_count) ? slot_start[i] : region_end;

            if(gap_end > search && (gap_end - search) >= size)
            {
                for(uint8_t a = 0; a < MAX_SLOTS; a++)
                {
                    if(!get_ram_slot_size(a))
                    {
                        _w_ram_slot_addr(a, 0x80000000 | search);
                        _w_ram_slot_size(a, size);
                        _w_ram_slot_key(a, 0x01); // System KEY
                        return get_ram_slot_addr(a);
                    }
                }
                return 0;
            }

            if(i < slot_count && slot_end[i] > search)
                search = slot_end[i];
        }
    }

    return 0;
}

// Free memory block
void ram_free(uint32_t addr)
{
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
        if(addr == get_ram_slot_addr(a))
        {
            _w_ram_slot_addr(a, 0);
            _w_ram_slot_size(a, 0);
            _w_ram_slot_key(a, 0);
        }
}

// Free memory block
uint8_t ram_truncate(uint32_t addr, uint32_t size)
{
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
        if(addr == get_ram_slot_addr(a))
        {
            if(get_ram_slot_size(a) >= size) { _w_ram_slot_size(a, size); return 1; }
        }

    return 0;
}

uint8_t ram_read_byte_safe(uint32_t addr, uint8_t key)
{
    if(!ram_check_key(addr, key)) return 0;
    return ram_read_byte(addr);
}

uint8_t ram_write_byte_safe(uint32_t addr, uint8_t data, uint8_t key)
{
    if(!ram_check_key(addr, key)) return 0;
    ram_write_byte(addr, data);
    return 1;
}

uint8_t ram_read_safe(uint32_t addr, uint8_t *data, uint16_t count, uint8_t key)
{
    if(!ram_check_key(addr, key)) return 0;
    ram_read(addr, data, count);
    return 1;
}

uint8_t ram_write_safe(uint32_t addr, uint8_t *data, uint16_t count, uint8_t key)
{
    if(!ram_check_key(addr, key)) return 0;
    ram_write(addr, data, count);
    return 1;
}

uint32_t ram_allocate_safe(uint32_t size, uint8_t key)
{
    if(key == 0x00 || key == 0x01) return 0;
    uint32_t addr = ram_allocate(size);
    if(!addr) return 0;
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
    {
        if(get_ram_slot_size(a) == size && get_ram_slot_addr(a) == addr)
        {
            _w_ram_slot_key(a, key);
            return addr;
        }
    }
    return 0;
}

uint8_t ram_check_key(uint32_t addr, uint8_t key)
{
    if(key == 0x00 || key == 0x01) return 0;
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
    {
        if(get_ram_slot_size(a) != 0 && addr >= get_ram_slot_addr(a) && addr < (get_ram_slot_addr(a) + get_ram_slot_size(a)))
        {
            if(key == get_ram_slot_key(a)) return 1;
            return 0;
        }
    }
    return 0;
}

// Free memory block
uint8_t ram_free_safe(uint32_t addr, uint8_t key)
{
    if(!ram_check_key(addr, key)) return 0;
    for(uint8_t a = 0; a < MAX_SLOTS; a++)
        if(addr == get_ram_slot_addr(a))
        {
            _w_ram_slot_addr(a, 0);
            _w_ram_slot_size(a, 0);
            _w_ram_slot_key(a, 0);
        }
    return 1;
}