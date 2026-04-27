



#define MAX_SLOTS 16

extern uint32_t ram_slot_addr[MAX_SLOTS];
extern uint32_t ram_slot_size[MAX_SLOTS];
extern uint8_t ram_slot_key[MAX_SLOTS];
extern uint8_t ram_status;

uint8_t ram_init();
void ram_get_slot(uint8_t slot, uint32_t *addr, uint32_t *size, uint8_t *key);
uint32_t get_ram_slot_size(uint8_t slot);
uint32_t get_ram_slot_addr(uint8_t slot);
uint8_t get_ram_slot_key(uint8_t slot);
void ram_write_byte(uint32_t add, uint8_t data);
uint8_t ram_read_byte(uint32_t add);
void ram_write_word(uint32_t add, uint16_t data);
uint16_t ram_read_word(uint32_t add);
void ram_write_dword(uint32_t add, uint32_t data);
uint32_t ram_read_dword(uint32_t add);
void ram_write(uint32_t add, uint8_t * data, uint16_t bytes);
void ram_read(uint32_t add, uint8_t * data, uint16_t bytes);
uint8_t ram_test(void);
uint32_t ram_allocate(uint32_t size);
void ram_free(uint32_t addr);
uint8_t ram_truncate(uint32_t addr, uint32_t size);
uint8_t ram_read_byte_safe(uint32_t addr, uint8_t key);
uint8_t ram_write_byte_safe(uint32_t addr, uint8_t data, uint8_t key);
uint8_t ram_read_safe(uint32_t addr, uint8_t *data, uint16_t count, uint8_t key);
uint8_t ram_write_safe(uint32_t addr, uint8_t *data, uint16_t count, uint8_t key);
uint32_t ram_allocate_safe(uint32_t size, uint8_t key);
uint8_t ram_check_key(uint32_t addr, uint8_t key);
uint8_t ram_free_safe(uint32_t addr, uint8_t key);