


void twi_init(void);
uint8_t twi_send_byte(uint8_t data);
uint8_t twi_read_byte(void);
uint8_t keyboard_init(void);
uint8_t keyboard_get_key(void);
uint8_t keyboard_wait_key(void);