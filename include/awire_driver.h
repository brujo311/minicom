







#define AWIRE_PORT PORTA_OUT
#define AWIRE_PIN  PORTA_IN
#define AWIRE_DDR  PORTA_DIR

#define AWIRE_PIN_RST  (1 << 6)

//#define AWIRE_DELAY_TX 100
//#define AWIRE_DELAY_RX1 40
//#define AWIRE_DELAY_RX2 60

#define AWIRE_DELAY_TX 15
#define AWIRE_DELAY_RX1 7
#define AWIRE_DELAY_RX2 7

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

#define CHUNK_SIZE              16      // max chunk we use

#define FLASHC_GENERAL_ADD      0x05
#define FLASHC_BASE_ADDR_UC     0xFFFE1400UL // AT32UC128D4
#define FLASHC_BASE_ADDR_UC3    0xFFFE0000UL // AT32UC3C
#define FLASHC_CTRL_OFFSET      0x00          // CONTROL reg
#define FLASHC_STATUS_OFFSET    0x08          // STATUS reg
#define FLASHC_CMD_OFFSET       0x04          // COMMAND / TRIGGER reg
#define FLASHC_PARAMETER_OFFSET 0x0C
#define FLASHC_VERSION_OFFSET   0x10
#define FLASHC_GPFUSE_H_OFFSET  0x14
#define FLASHC_GPFUSE_L_OFFSET  0x18
#define FLASHC_PAGEBUF_OFFSET   0x100         // PAGE BUFFER base address (if device exposes one)
#define FLASHC_KEY_VALUE        0xA5          // magic KEY to start operation (placeholder)
#define FLASHC_BUSY_MASK        0x01          // bit mask in STATUS for busy (placeholder)
#define FLASHC_ERR_MASK         0x02          // bit mask for error (placeholder)

#define FLASH_CMD_NOP           0
#define FLASH_CMD_WRITE_PAGE    1
#define FLASH_CMD_ERASE_PAGE    2
#define FLASH_CMD_CLEAR_BUFFER  3
#define FLASH_CMD_LOCK_REGION   4
#define FLASH_CMD_UNLOCK_REGION 5
#define FLASH_CMD_ERASE_ALL     6
#define FLASH_CMD_WRITE_GP_FUSE 7
#define FLASH_CMD_ERASE_GP_FUSE 8
#define FLASH_CMD_SET_SEC_BIT   9
#define FLASH_CMD_PGM_FUSE_BYTE 10
#define FLASH_CMD_ERASE_ALL_GPF 11
#define FLASH_CMD_QUICK_PAGE_RD 12
#define FLASH_CMD_WRITE_USR_PG  13
#define FLASH_CMD_ERASE_USR_PG  14
#define FLASH_CMD_QREAD_USR_PG  15
 
#define AWIRE_BYTE 0
#define AWIRE_HALF_WORD 1
#define AWIRE_WORD 2

#define AWIRE_OK 0xa0
#define AWIRE_ERROR_SYNC 0xa1
#define AWIRE_ERROR_NACK 0xa2
#define AWIRE_ERROR_NVM_BUSY 0xa3
#define AWIRE_ERROR_RAM_OVF 0xa4
#define AWIRE_ERROR_DEVICE_NOT_ALIVE 0xa5
#define AWIRE_ERROR_ADDR 0xa6
#define AWIRE_ERROR_1 0xa7
#define AWIRE_ERROR_2 0xa8
#define AWIRE_ERROR_PARAMETERS 0xa8
#define AWIRE_ERROR_MISSING_PARAMETERS 0xa9
#define AWIRE_ERROR_CHECKING_STATUS 0xaa
#define AWIRE_ERROR_TIMEOUT 0xab
#define AWIRE_ERROR_PGM 0xac
#define AWIRE_ERROR_RCV_LESS 0xad
#define AWIRE_ERROR_SECTION_LOCKED 0xae
#define AWIRE_MISSMATCH 0xaf
#define AWIRE_ERROR_READ_GOT_MEM_STATUS 0xeffe
#define AWIRE_ERROR_READ_GOT_UNKNOW 0xefff

extern uint8_t awire_pgm_mode;
extern uint8_t awire_timeout;
extern uint16_t page_size;
extern uint16_t flash_size;
extern uint32_t flash_controller_base_address;

void awire_reset(void);
void awire_enter_progmode(void);
void awire_send(uint8_t *data, uint16_t byte_count);
uint16_t awire_recv(uint8_t *data, uint16_t byte_count);
uint8_t awire_check_alive();
uint8_t awire_read_jtag_id(uint8_t *idcode);
uint8_t awire_read_status(uint8_t *status);
uint8_t awire_chip_erase();
uint8_t awire_write_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type);
uint16_t awire_read_mem(uint8_t * address, uint8_t * data, uint16_t byte_count, uint8_t data_type);
uint8_t awire_get_parameters(uint32_t controller_base_addr);
uint8_t write_reg32(uint32_t target_addr, uint32_t value);
uint8_t read_reg32(uint32_t target_addr, uint32_t *value_out);
uint8_t awire_write_page(uint8_t *page_address);
uint8_t awire_erase_page(uint8_t *page_address);
uint8_t awire_clear_page_buffer(uint8_t *page_address);
uint8_t awire_load_page(uint8_t *page_address, uint8_t *data);
uint8_t awire_read_fuses(uint8_t * data);
uint8_t awire_erase_fuse_bit(uint8_t fuse);
uint8_t awire_write_fuse_bit(uint8_t fuse);
uint16_t crc16_ccitt(uint8_t *data, uint16_t length);
uint16_t crc16_xmodem(uint8_t *data, uint16_t length);
uint16_t crc16_kermit(uint8_t *data, uint16_t length);
uint16_t crc16_fcs(uint8_t *data, uint16_t length);
uint16_t crc16_fcs_rev_bitwise(const uint8_t *data, uint16_t len);
uint16_t calculate_crc16(const uint8_t *data, uint16_t len);