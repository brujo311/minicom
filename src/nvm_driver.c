
#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "nvm_driver.h"

void eeprom_init(void)
{
    eeprom_wait_ready();
    
    NVM.CMD = NVM_CMD_NO_OPERATION_gc;
}

void eeprom_wait_ready(void)
{
    // Wait while EEPROM is busy (NVMBUSY bit is set)
    while (NVM.STATUS & NVM_NVMBUSY_bm) ;
}

void eeprom_erase_byte(uint16_t address)
{
    uint8_t sreg_backup;
    
    // Backup SREG and disable interrupts
    sreg_backup = SREG;
    cli();
    
    // Wait for any ongoing EEPROM operation to complete
    eeprom_wait_ready();
    
    // Set up NVM command for EEPROM erase
    NVM.CMD = NVM_CMD_ERASE_EEPROM_PAGE_gc;
    
    // Set EEPROM address
    NVM.ADDR0 = address & 0xFF;        // Low byte
    NVM.ADDR1 = (address >> 8) & 0xFF; // High byte
    NVM.ADDR2 = 0x00;                  // Not used for EEPROM
    
    // Execute command with timed sequence
    CCP = CCP_IOREG_gc;  // Enable protected register write
    NVM.CTRLA |= NVM_CMDEX_bm;  // Execute command
    
    // Wait for erase operation to complete
    eeprom_wait_ready();
    
    // Clear NVM command
    NVM.CMD = NVM_CMD_NO_OPERATION_gc;
    
    // Restore SREG (re-enable interrupts if they were enabled)
    SREG = sreg_backup;
}

void eeprom_write_byte(uint16_t address, uint8_t data) {
    
    if(data == eeprom_read_byte(address)) return; // No changes to memmory not necessary to lose time and stress it up lool
    
    uint8_t sreg_backup;
    
    // Backup SREG and disable interrupts
    sreg_backup = SREG;
    cli();
    
    // Wait for any ongoing EEPROM operation to complete
    eeprom_wait_ready();
    
    // Set up NVM command for EEPROM write
    NVM.CMD = NVM_CMD_LOAD_EEPROM_BUFFER_gc;
    
    // Set EEPROM address
    NVM.ADDR0 = address & 0xFF;        // Low byte
    NVM.ADDR1 = (address >> 8) & 0xFF; // High byte
    NVM.ADDR2 = 0x00;                  // Not used for EEPROM
    
    // Load data into buffer
    NVM.DATA0 = data;
    
    // Change command to write EEPROM
    NVM.CMD = NVM_CMD_ERASE_WRITE_EEPROM_PAGE_gc;
    
    // Execute command with timed sequence
    CCP = CCP_IOREG_gc;  // Enable protected register write
    NVM.CTRLA |= NVM_CMDEX_bm;  // Execute command
    
    // Wait for write operation to complete
    eeprom_wait_ready();
    
    // Clear NVM command
    NVM.CMD = NVM_CMD_NO_OPERATION_gc;
    
    // Restore SREG (re-enable interrupts if they were enabled)
    SREG = sreg_backup;
}

uint8_t eeprom_read_byte(uint16_t address) {
    NVM.ADDR0 = (uint8_t)(address & 0xFF);
    NVM.ADDR1 = (uint8_t)((address >> 8) & 0xFF);
    NVM.CMD = NVM_CMD_READ_EEPROM_gc;
    CCP = CCP_IOREG_gc;
    NVM.CTRLA = NVM_CMDEX_bm;
    return NVM.DATA0;
}

void eeprom_write_word(uint16_t address, uint16_t data)
{
    eeprom_write_byte(address++, (uint8_t)(data >> 8));
    eeprom_write_byte(address, (uint8_t)data);
}

uint16_t eeprom_read_word(uint16_t address)
{
    uint16_t data = (uint16_t)(eeprom_read_byte(address++) << 8);
    data |= eeprom_read_byte(address);
    return data;
}