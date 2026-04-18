



#define MAX_SLOTS 8

extern uint32_t ram_slot_addr[MAX_SLOTS];
extern uint32_t ram_slot_size[MAX_SLOTS];
extern uint8_t ram_status;

void ram_init();
void ram_write_byte(uint32_t add, uint8_t data);
uint8_t ram_read_byte(uint32_t add);
void ram_write_word(uint32_t add, uint16_t data);
uint16_t ram_read_word(uint32_t add);
void ram_write_dword(uint32_t add, uint32_t data);
uint32_t ram_read_dword(uint32_t add);
void ram_write(uint32_t add, uint8_t * data, uint8_t bytes);
void ram_read(uint32_t add, uint8_t * data, uint8_t bytes);
uint8_t ram_test(void);
uint32_t ram_allocate(uint32_t size);
void ram_free(uint32_t addr);
uint8_t ram_truncate(uint32_t addr, uint32_t size);