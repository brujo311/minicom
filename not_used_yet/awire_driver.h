

#define AWIRE_SYNC 0x55
#define AWIRE_ACK 0x40
#define AWIRE_NACK 0x41
#define AWIRE_IDCODE 0xC0
#define AWIRE_MEMDATA 0xC1
#define AWIRE_MEM_STATUS 0xC2
#define AWIRE_BAUD_RATE 0xC3
#define AWIRE_STATUS_INFO 0xC4
#define AWIRE_MEMORY_SPEED 0xC5
#define AWIRE_INS_AYA 0x01
#define AWIRE_INS_JTAG_ID 0x02
#define AWIRE_INS_STATUS_REQUEST 0x03
#define AWIRE_INS_TUNE 0x04
#define AWIRE_INS_MEMORY_SPEED_REQUEST 0x05
#define AWIRE_INS_CHIP_ERASE 0x06
#define AWIRE_INS_DISABLE 0x07
#define AWIRE_INS_2_PIN_MODE 0x08
#define AWIRE_INS_MEMORY_WRITE 0x80
#define AWIRE_INS_MEMORY_READ 0x81
#define AWIRE_INS_HALT 0x82
#define AWIRE_INS_RESET 0x83
#define AWIRE_INS_SET_GUARD_TIME 0x84

#define AWIRE_ERROR_SYNC 1
#define AWIRE_ERROR_NACK 2

extern uint8_t awire_pgm_mode;
extern uint8_t awire_timeout;
extern uint16_t awire_rx_index;
extern uint8_t awire_rx_buffer[];
extern uint32_t awire_rxd_timeout;

void awire_reset(void);
void awire_enter_progmode(void);
void awire_send(const uint8_t *data, uint16_t byte_count);
uint8_t awire_check_alive();
uint8_t awire_read_jtag_id(uint8_t *idcode);
uint8_t awire_read_status(uint8_t *status);
uint8_t awire_chip_erase();
uint8_t awire_write_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type);
uint8_t awire_read_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type);