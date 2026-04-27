//**************************************************************
// ****** FUNCTIONS FOR SD RAW DATA TRANSFER *******
//**************************************************************
//Controller: ATmega32 (Clock: 8 Mhz-internal)
//Compiler	: AVR-GCC
//Version 	: 2.2
//Author	: CC Dharmani, Chennai (India)
//			  www.dharmanitech.com
//Date		: 15 July 2009
//**************************************************************

//**************************************************
// ***** HEADER FILE : SD_routines.h ******
//**************************************************
#ifndef _SD_ROUTINES_H_
#define _SD_ROUTINES_H_

//Use following macro if you don't want to activate the multiple block access functions
//those functions are not required for FAT32

#define FAT_TESTING_ONLY         

//use following macros if PF1 pin is used for Chip Select of SD
#define SD_CS_ASSERT     PORTF_OUT &= ~0x02
#define SD_CS_DEASSERT   PORTF_OUT |= 0x02
#define SD_CS_DIRSET     PORTF_DIR |= 0x02

// SD_status layout:
// [7]    = initialized (1 = OK)
// [6:4]  = card type
// [3:0]  = error code

#define SD_STATUS_INIT_OK   0x80
#define SD_STATUS_TYPE_V2   0x20   // SDHC/SDv2
#define SD_STATUS_TYPE_V1   0x10   // SDSC/old

// Errors (LS4 bits)
#define SD_ERR_NONE         0x00
#define SD_ERR_CMD0         0x01
#define SD_ERR_CMD8         0x02
#define SD_ERR_ACMD41       0x03
#define SD_ERR_TIMEOUT      0x04
#define SD_ERR_CMD58        0x05
#define SD_ERR_CID_TIMEOUT  0x06
#define SD_ERR_NO_CARD      0x07
#define SD_ERR_CID_MISMATCH 0x08


#define GO_IDLE_STATE            0
#define SEND_OP_COND             1
#define SEND_CSD                 9
#define READ_CID                 10
#define STOP_TRANSMISSION        12
#define SEND_STATUS              13
#define SET_BLOCK_LEN            16
#define READ_SINGLE_BLOCK        17
#define READ_MULTIPLE_BLOCKS     18
#define WRITE_SINGLE_BLOCK       24
#define WRITE_MULTIPLE_BLOCKS    25
#define ERASE_BLOCK_START_ADDR   32
#define ERASE_BLOCK_END_ADDR     33
#define ERASE_SELECTED_BLOCKS    38
#define CRC_ON_OFF               59

#define ON     1
#define OFF    0

#define SD_ON 0
#define SD_INSERTED 1
#define FAT32_ACTIVE 2

extern volatile uint32_t startBlock, totalBlocks;
extern volatile uint8_t buffer[512];
extern uint8_t SD_status;
extern uint8_t SD_cid[16];

void console_write(uint8_t * text);

uint8_t SD_check_alive();
uint8_t SD_init();
//static uint8_t SD_read_CID(uint8_t * cid);
//static uint8_t SD_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
//static void SD_cmd_end(void);
uint8_t SD_erase (uint32_t startBlock, uint32_t totalBlocks);
uint8_t SD_readSingleBlock(uint32_t startBlock);
uint8_t SD_writeSingleBlock(uint32_t startBlock);

//unsigned char SD_init();
//unsigned char SD_sendCommand(unsigned char cmd, unsigned long arg);
//unsigned char SD_readSingleBlock(unsigned long startBlock);
//unsigned char SD_writeSingleBlock(unsigned long startBlock);
//unsigned char SD_readMultipleBlock (unsigned long startBlock, unsigned long totalBlocks);
//unsigned char SD_writeMultipleBlock(unsigned long startBlock, unsigned long totalBlocks);
//unsigned char SD_erase (unsigned long startBlock, unsigned long totalBlocks);

#endif
