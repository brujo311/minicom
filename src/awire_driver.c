

#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include "awire_driver.h"

uint8_t awire_pgm_mode = 0;
uint8_t awire_timeout = 0;
uint32_t awire_timeout_max = 1060000UL;
uint16_t page_size = 0;
uint16_t flash_size = 0;
uint32_t flash_controller_base_address = 0;

static inline void awire_set_tx()
{
    AWIRE_DDR |= AWIRE_PIN_RST;
    AWIRE_PORT |= AWIRE_PIN_RST;
}

static inline void awire_set_rx()
{
    AWIRE_DDR &= ~AWIRE_PIN_RST;
}

static inline void awire_send_bit(uint8_t bit)
{
    if (bit) {
        AWIRE_PORT |= AWIRE_PIN_RST;
    } else {
        AWIRE_PORT &= ~AWIRE_PIN_RST;
    }
    _delay_us(AWIRE_DELAY_TX);
}

static void awire_send_byte(uint8_t data)
{
    if(!awire_pgm_mode) return;
    awire_set_tx();
    awire_send_bit(0);  // start bit
    for (uint8_t i = 0; i < 8; i++) {
        awire_send_bit((data >> i) & 1);
    }
    awire_send_bit(1);  // stop bit
    awire_set_rx();
    //_delay_us(AWIRE_DELAY_TX);
}

static inline uint8_t awire_recv_bit(void)
{
    _delay_us(AWIRE_DELAY_RX1);
    uint8_t bit = (AWIRE_PIN & AWIRE_PIN_RST) ? 1 : 0;
    _delay_us(AWIRE_DELAY_RX2);
    return bit;
}

static uint8_t awire_recv_byte(void)
{
    uint8_t byte = 0;
    uint32_t timeout = 0;
    while((AWIRE_PIN & AWIRE_PIN_RST) && (timeout < awire_timeout_max)) timeout++;
    awire_recv_bit(); // start bit
    for (uint8_t i = 0; i < 8; i++) {
        byte |= (awire_recv_bit() << i);
    }
    awire_recv_bit(); // stop bit
    if(timeout >= awire_timeout_max) { byte = 0xff; awire_timeout = 1; }
    timeout = 0;
    while((!(AWIRE_PIN & AWIRE_PIN_RST)) && (timeout < awire_timeout_max)) timeout++;
    if(timeout >= awire_timeout_max) { byte = 0xff; awire_pgm_mode = 0; awire_timeout = 1; }
    return byte;
}

static uint8_t awire_recv_byte_no_wait(void)
{
    uint8_t byte = 0;
    awire_recv_bit(); // start bit
    for (uint8_t i = 0; i < 8; i++) byte |= (awire_recv_bit() << i);
    awire_recv_bit(); // stop bit
    return byte;
}

void awire_reset(void)
{
    awire_set_tx();
    _delay_ms(100);
    AWIRE_PORT &= ~AWIRE_PIN_RST;
    _delay_ms(100);
    awire_set_tx();
    _delay_ms(20);
    awire_set_rx();
    awire_pgm_mode = 0;
}

void awire_enter_progmode(void)
{
    awire_reset();
    awire_set_tx();
    for (uint8_t i = 0; i < 5; i++)
    {
        AWIRE_PORT &= ~AWIRE_PIN_RST; // Clear 0
        _delay_us(1000);
        AWIRE_PORT |= AWIRE_PIN_RST; // Set 1
        _delay_us(1000);

    }
    awire_set_rx();
    _delay_ms(200);
}

void awire_send(uint8_t *data, uint16_t byte_count)
{
    for(uint16_t a = 0; a < byte_count; a++)
    {
        awire_send_byte(data[a]);
    }
}

uint16_t awire_recv(uint8_t *data, uint16_t byte_count)
{
    uint16_t bytes_received = 0;
    uint32_t timeout;

    for (uint16_t i = 0; i < byte_count; i++)
    {
        timeout = 0;
        data[i] = 0;

        // Wait for start bit (line low)
        while ((AWIRE_PIN & AWIRE_PIN_RST) && (timeout < awire_timeout_max))
            timeout++;

        // If timeout waiting for start bit
        if (timeout >= awire_timeout_max) {
            awire_timeout = 1;
            return bytes_received; // return bytes received so far
        }

        // Store received byte
        data[i] = awire_recv_byte_no_wait();
        bytes_received++;
    }

    return bytes_received; // all bytes received successfully
}

uint16_t crc16_ccitt(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}

uint16_t crc16_xmodem(uint8_t *data, uint16_t length) {
    uint16_t crc = 0x0000;   // ← changed

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}

uint16_t crc16_kermit(uint8_t *data, uint16_t length) {
    uint16_t crc = 0x0000;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0x8408;  // reversed 0x1021
            else
                crc >>= 1;
        }
    }
    return crc;
}

uint16_t crc16_fcs(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;  // Initial value

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0x8408;  // reversed 0x1021
            else
                crc >>= 1;
        }
    }

    crc = ~crc;  // Final XOR
    return crc;
}

uint16_t crc16_fcs_rev_bitwise(const uint8_t *data, uint16_t len) {
    const uint16_t poly = 0x8408;
    uint16_t crc = 0x0000; // initial value

    for (uint16_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
    }
    return crc & 0xFFFF;
}

uint16_t calculate_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0x0000;  // Starting value
    uint8_t i;
    
    for (uint16_t byte_idx = 0; byte_idx < len; byte_idx++) {
        crc ^= data[byte_idx];
        
        for (i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0x8408;  // Reversed polynomial
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

uint8_t awire_check_alive()
{
    if(!awire_pgm_mode) awire_enter_progmode();
    awire_pgm_mode = 1;
    uint8_t data[] = {AWIRE_SYNC, AWIRE_INS_AYA, 0x00, 0x00};
    awire_send(data, 4);
    uint8_t bytes = awire_recv(data, 4);
    if(bytes < 2 || data[0] != AWIRE_SYNC || data[1] != AWIRE_ACK) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }
    awire_pgm_mode = 1;
    return AWIRE_OK;
}

uint8_t awire_read_jtag_id(uint8_t *idcode)
{
    uint8_t bytes = 0;
    if(!awire_pgm_mode) 
        if(awire_check_alive()) return AWIRE_ERROR_DEVICE_NOT_ALIVE;
    uint8_t data[] = {AWIRE_SYNC, AWIRE_INS_JTAG_ID, 0x00, 0x00};
    awire_send(data, 4);
    bytes = awire_recv(data, 4);
    if(data[0] != AWIRE_SYNC || data[1] != AWIRE_IDCODE) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }
    bytes += awire_recv(idcode, 4);
    bytes += awire_recv(data, 2); // Discard CRC
    return AWIRE_OK;
}

uint8_t awire_read_status(uint8_t *status)
{
    uint8_t bytes = 0;
    if(!awire_pgm_mode) 
        if(awire_check_alive()) return AWIRE_ERROR_DEVICE_NOT_ALIVE;
    uint8_t data[] = {AWIRE_SYNC, AWIRE_INS_STATUS_REQUEST, 0x00, 0x00};
    awire_send(data, 4);
    bytes = awire_recv(data, 2);
    bytes += awire_recv(status, 4);
    if(bytes < 6 || data[0] != AWIRE_SYNC || data[1] != AWIRE_STATUS_INFO) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }
    return AWIRE_OK;
}

uint8_t awire_chip_erase()
{
    if(!awire_pgm_mode)
        if(awire_check_alive()) return AWIRE_ERROR_DEVICE_NOT_ALIVE;
    uint8_t data[] = {AWIRE_SYNC, AWIRE_INS_CHIP_ERASE, 0x00, 0x00};
    awire_send(data, 4);
    uint8_t bytes = awire_recv(data, 2);
    if(bytes < 2 || data[0] != AWIRE_SYNC || data[1] != AWIRE_ACK) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }
    return AWIRE_OK;
}

// { SYNC, CMD, LENGTH (2 bytes), ADDRESS (4 bytes), DATA (n bytes), CRC MSB, CRC LSB }
uint8_t awire_write_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type) // 0 -> byte, 1 -> 2 bytes, 2 -> word
{
    if(byte_count > 512) return AWIRE_ERROR_RAM_OVF;
    if(!awire_pgm_mode)
        if(awire_check_alive()) return AWIRE_ERROR_DEVICE_NOT_ALIVE;
    uint8_t snd[530];
    byte_count += 5;
    snd[0] = AWIRE_SYNC;
    snd[1] = AWIRE_INS_MEMORY_WRITE;
    snd[2] = (uint8_t)(byte_count >> 8);
    snd[3] = (uint8_t)(byte_count);
    byte_count -= 5;
    snd[4] = FLASHC_GENERAL_ADD | (data_type << 4);
    snd[5] = address[0];
    snd[6] = address[1];
    snd[7] = address[2];
    snd[8] = address[3];
    snd[9 + byte_count] = 0; // CRC
    snd[10 + byte_count] = 0; // CRC
    for(uint8_t a = 0; a < byte_count; a++) snd[9 + a] = data[a];
    awire_send(snd, byte_count + 11);
    uint8_t bytes = awire_recv(snd, 4);
    if(bytes < 2 || snd[0] != AWIRE_SYNC) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }
    if(snd[1] == AWIRE_MEM_STATUS) bytes += awire_recv(&snd[4], (snd[2] << 8 | snd[3])); // Get the status
    bytes += awire_recv(&snd[8], 2); // Discard CRC
    if(snd[4] == 0) return AWIRE_OK; // Memory write succed
    if(snd[4] == 1) return AWIRE_ERROR_NVM_BUSY; // Memory controller busy (but bus alive)
    if(snd[4] == 2) return AWIRE_ERROR_ADDR; // Address error
    return bytes; // Non of the above will return just the amount of bytes read
}

// { SYNC, CMD, LENGTH (2 bytes), ADDRESS (4 bytes), BYTES TO READ (2 bytes), CRC MSB, CRC LSB }
uint16_t awire_read_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type) // 0 -> byte, 1 -> 2 bytes, 2 -> word
{
    if(byte_count > 512) return AWIRE_ERROR_RAM_OVF;
    if(!awire_pgm_mode)
        if(awire_check_alive()) return AWIRE_ERROR_DEVICE_NOT_ALIVE;
    uint8_t snd[16];
    snd[0] = AWIRE_SYNC;
    snd[1] = AWIRE_INS_MEMORY_READ;
    snd[2] = 0;
    snd[3] = 7;
    snd[4] = FLASHC_GENERAL_ADD | (data_type << 4);
    snd[5] = address[0];
    snd[6] = address[1];
    snd[7] = address[2];
    snd[8] = address[3];
    snd[9] = (uint8_t)(byte_count >> 8);
    snd[10] = (uint8_t)(byte_count);
    snd[11] = 0; // CRC
    snd[12] = 0; // CRC
    awire_send(snd, 13);
    
    // Get first 4 bytes: SYNC, CMD, BYTE_COUNT_MSB, BYTE_COUNT_LSB
    uint8_t bytes = awire_recv(snd, 4);
    if(snd[0] != AWIRE_SYNC) { awire_pgm_mode = 0; return AWIRE_ERROR_DEVICE_NOT_ALIVE; }

    // Handle for normal memory read
    if(snd[1] == AWIRE_MEMDATA)
    {
        // Get the data bytes
        bytes += awire_recv(data, ((snd[2] << 8) | snd[3]) - 3);
        // Get the tail: STATUS, REMAINING_BYTES_MSB, REM_BYTES_LSB, CRC_MSB, CRC_LSB
        bytes += awire_recv(&snd[4], 5);
        // Return amount of bytes read
        return ((snd[2] << 8) | snd[3]) - 3;
    }

    // Handle for memory error
    if(snd[1] == AWIRE_MEM_STATUS)
    {
        // Get the status dummy bytes
        bytes += awire_recv(&snd[4], 3);
        return AWIRE_ERROR_READ_GOT_MEM_STATUS;
    }
    return AWIRE_ERROR_READ_GOT_UNKNOW;
}

uint8_t awire_get_parameters(uint32_t controller_base_addr)
{
    uint8_t address[4];
    address[0] = (uint8_t)(controller_base_addr >> 24);
    address[1] = (uint8_t)(controller_base_addr >> 16);
    address[2] = (uint8_t)(controller_base_addr >> 8);
    address[3] = FLASHC_PARAMETER_OFFSET;
    uint8_t parameters[4];
    if(awire_read_mem(address, parameters, 4, AWIRE_BYTE) != 4) return AWIRE_ERROR_PARAMETERS;
    flash_controller_base_address = controller_base_addr;
    switch(parameters[3]) // flash size in KB
    {
        case 0:
            flash_size = 4;
            break;
        case 1:
            flash_size = 8;
            break;
        case 2:
            flash_size = 16;
            break;
        case 3:
            flash_size = 32;
            break;
        case 4:
            flash_size = 48;
            break;
        case 5:
            flash_size = 64;
            break;
        case 6:
            flash_size = 96;
            break;
        case 7:
            flash_size = 128;
            break;
        case 8:
            flash_size = 192;
            break;
        case 9:
            flash_size = 256;
            break;
        case 10:
            flash_size = 384;
            break;
        case 11:
            flash_size = 512;
            break;
        case 12:
            flash_size = 768;
            break;
        case 13:
            flash_size = 1024;
            break;
        case 14:
            flash_size = 2048;
            break;
        case 15:
            flash_size = 4096;
            break;
    }
    switch(parameters[2]) // page size in bytes
    {
        case 0:
            page_size = 32;
            break;
        case 1:
            page_size = 64;
            break;
        case 2:
            page_size = 128;
            break;
        case 3:
            page_size = 256;
            break;
        case 4:
            page_size = 512;
            break;
        case 5:
            page_size = 1024;
            break;
        case 6:
            page_size = 2048;
            break;
        case 7:
            page_size = 4096;
            break;
    }
    return AWIRE_OK;
}

static void clean_page(uint8_t *address)
{
    // Convert 4 bytes (MSB first) into 32-bit integer
    uint32_t addr = ((uint32_t)address[0] << 24) |
                    ((uint32_t)address[1] << 16) |
                    ((uint32_t)address[2] << 8)  |
                    ((uint32_t)address[3]);

    // Clear lower bits inside the page
    addr &= ~((uint32_t)page_size - 1);

    // Store result back into address[4] (MSB first)
    address[0] = (uint8_t)(addr >> 24);
    address[1] = (uint8_t)(addr >> 16);
    address[2] = (uint8_t)(addr >> 8);
    address[3] = (uint8_t)(addr);
}

uint8_t awire_write_page(uint8_t *page_address)
{
    if(!page_size || !awire_pgm_mode) return AWIRE_ERROR_MISSING_PARAMETERS;
    uint8_t address[4];
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_CMD_OFFSET;
    uint8_t parameters[4];
    parameters[0] = FLASHC_KEY_VALUE;
    parameters[1] = page_address[1];
    parameters[2] = page_address[2];
    parameters[3] = page_address[3];
    clean_page(parameters);
    parameters[3] |= FLASH_CMD_WRITE_PAGE;
    if(awire_write_mem(address, parameters, 4, AWIRE_WORD) != AWIRE_OK) return 0;
    _delay_us(200);

    // Check for NVM controller to be ready
    address[3] = FLASHC_STATUS_OFFSET;
    uint8_t ready = 0;
    uint32_t timeout = 0;
    while(!ready && (timeout < awire_timeout_max))
    {
        timeout++;
        if(awire_read_mem(address, parameters, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;
        if(parameters[3] & 0x08) return AWIRE_ERROR_PGM; // Program error flag
        if(parameters[3] & 0x04) return AWIRE_ERROR_SECTION_LOCKED; // Program error flag
        if(parameters[3] & 0x01) ready = 1; // Ready flag
        if(timeout >= awire_timeout_max) return AWIRE_ERROR_TIMEOUT;
    }

    return AWIRE_OK;
}

uint8_t awire_erase_page(uint8_t *page_address)
{
    if(!page_size || !awire_pgm_mode) return AWIRE_ERROR_MISSING_PARAMETERS;
    uint8_t address[4];
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_CMD_OFFSET;
    uint8_t parameters[4];
    parameters[0] = FLASHC_KEY_VALUE;
    parameters[1] = page_address[1];
    parameters[2] = page_address[2];
    parameters[3] = page_address[3];
    clean_page(parameters);
    parameters[3] |= FLASH_CMD_ERASE_PAGE;
    if(awire_write_mem(address, parameters, 4, AWIRE_WORD) != AWIRE_OK) return 0;
    _delay_us(200);

    // Check for NVM controller to be ready
    address[3] = FLASHC_STATUS_OFFSET;
    uint8_t ready = 0;
    uint32_t timeout = 0;
    while(!ready && (timeout < awire_timeout_max))
    {
        timeout++;
        if(awire_read_mem(address, parameters, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;
        if(parameters[3] & 0x08) return AWIRE_ERROR_PGM; // Program error flag
        if(parameters[3] & 0x04) return AWIRE_ERROR_SECTION_LOCKED; // Program error flag
        if(parameters[3] & 0x01) ready = 1; // Ready flag
        if(timeout >= awire_timeout_max) return AWIRE_ERROR_TIMEOUT;
    }

    return AWIRE_OK;
}

uint8_t awire_clear_page_buffer(uint8_t *page_address)
{
    if(!page_size || !awire_pgm_mode) return AWIRE_ERROR_MISSING_PARAMETERS;
    uint8_t address[4];
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_CMD_OFFSET;
    uint8_t parameters[4];
    parameters[0] = FLASHC_KEY_VALUE;
    parameters[1] = page_address[1];
    parameters[2] = page_address[2];
    parameters[3] = page_address[3];
    clean_page(parameters);
    parameters[3] |= FLASH_CMD_CLEAR_BUFFER;
    if(awire_write_mem(address, parameters, 4, AWIRE_WORD) != AWIRE_OK) return AWIRE_ERROR_RCV_LESS;
    _delay_us(200);

    // Check for NVM controller to be ready
    address[3] = FLASHC_STATUS_OFFSET;
    uint8_t ready = 0;
    uint32_t timeout = 0;
    while(!ready && (timeout < awire_timeout_max))
    {
        timeout++;
        if(awire_read_mem(address, parameters, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;
        if(parameters[3] & 0x08) return AWIRE_ERROR_PGM; // Program error flag
        if(parameters[3] & 0x04) return AWIRE_ERROR_SECTION_LOCKED; // Program error flag
        if(parameters[3] & 0x01) ready = 1; // Ready flag
        if(timeout >= awire_timeout_max) return AWIRE_ERROR_TIMEOUT;
    }

    return AWIRE_OK;
}

uint8_t awire_load_page(uint8_t *page_address, uint8_t *data)
{
    if(!page_size || !awire_pgm_mode) return AWIRE_ERROR_MISSING_PARAMETERS;
    
    uint16_t word_addr = 0;
    uint8_t address[4];

    address[0] = page_address[0];
    address[1] = page_address[1];

    uint16_t chunk_count = page_size / 4; // write word by word
    
    for(uint16_t a = 0; a < chunk_count; a++)
    {
        word_addr = a * 4;
        address[2] = page_address[2] | (uint8_t)(word_addr >> 8);
        address[3] = page_address[3] | (uint8_t)(word_addr);
        
        if(awire_write_mem(address, &data[word_addr], 4, AWIRE_WORD) != AWIRE_OK) return AWIRE_ERROR_RCV_LESS;
    }

    return AWIRE_OK;
}

uint8_t awire_read_fuses(uint8_t * data)
{
    if(!page_size || !awire_pgm_mode) return AWIRE_ERROR_MISSING_PARAMETERS;
    
    uint8_t address[4];
    
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_GPFUSE_H_OFFSET;

    if(awire_read_mem(address, data, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;

    address[3] = FLASHC_GPFUSE_L_OFFSET;

    if(awire_read_mem(address, &data[4], 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;

    return AWIRE_OK;
}

uint8_t awire_erase_fuse_bit(uint8_t fuse)
{
    if(!page_size || !awire_pgm_mode || (fuse > 63)) return AWIRE_ERROR_MISSING_PARAMETERS;

    uint8_t address[4];
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_CMD_OFFSET;
    uint8_t parameters[4];
    parameters[0] = FLASHC_KEY_VALUE;
    parameters[1] = 0;
    parameters[2] = fuse; // 0 ~ 63
    parameters[3] = FLASH_CMD_ERASE_GP_FUSE;
    if(awire_write_mem(address, parameters, 4, AWIRE_WORD) != AWIRE_OK) return 0;

    // Check for NVM controller to be ready
    address[3] = FLASHC_STATUS_OFFSET;
    uint8_t ready = 0;
    uint32_t timeout = 0;
    while(!ready && (timeout < awire_timeout_max))
    {
        timeout++;
        if(awire_read_mem(address, parameters, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;
        if(parameters[3] & 0x08) return AWIRE_ERROR_PGM; // Program error flag
        if(parameters[3] & 0x01) ready = 1; // Ready flag
        if(timeout >= awire_timeout_max) return AWIRE_ERROR_TIMEOUT;
    }

    return AWIRE_OK;
}

uint8_t awire_write_fuse_bit(uint8_t fuse)
{
    if(!page_size || !awire_pgm_mode || (fuse > 63)) return AWIRE_ERROR_MISSING_PARAMETERS;

    uint8_t address[4];
    address[0] = (uint8_t)(flash_controller_base_address >> 24);
    address[1] = (uint8_t)(flash_controller_base_address >> 16);
    address[2] = (uint8_t)(flash_controller_base_address >> 8);
    address[3] = FLASHC_CMD_OFFSET;
    uint8_t parameters[4];
    parameters[0] = FLASHC_KEY_VALUE;
    parameters[1] = 0;
    parameters[2] = fuse; // 0 ~ 63
    parameters[3] = FLASH_CMD_WRITE_GP_FUSE;
    if(awire_write_mem(address, parameters, 4, AWIRE_WORD) != AWIRE_OK) return AWIRE_ERROR_RCV_LESS;

    // Check for NVM controller to be ready
    address[3] = FLASHC_STATUS_OFFSET;
    uint8_t ready = 0;
    uint32_t timeout = 0;
    while(!ready && (timeout < awire_timeout_max))
    {
        timeout++;
        if(awire_read_mem(address, parameters, 4, AWIRE_WORD) != 4) return AWIRE_ERROR_RCV_LESS;
        if(parameters[3] & 0x08) return AWIRE_ERROR_PGM; // Program error flag
        if(parameters[3] & 0x01) ready = 1; // Ready flag
        if(timeout >= awire_timeout_max) return AWIRE_ERROR_TIMEOUT;
    }

    return AWIRE_OK;
}
