

void eeprom_init(void);
void eeprom_wait_ready(void);
void eeprom_erase_byte(uint16_t address);
void eeprom_write_byte(uint16_t address, uint8_t data);
uint8_t eeprom_read_byte(uint16_t address);
void eeprom_write_word(uint16_t address, uint16_t data);
uint16_t eeprom_read_word(uint16_t address);