//**************************************************************
// ****** FUNCTIONS FOR SD RAW DATA TRANSFER *******
//**************************************************************
//Controller: ATmega32 (8 Mhz internal)
//Compiler	: AVR-GCC
//Version	: 2.2
//Author	: CC Dharmani, Chennai (India)
// 			  www.dharmanitech.com
//Date		: 15 July 2009
//**************************************************************

//**************************************************
// ***** SOURCE FILE : SD_routines.c ******
//**************************************************

#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "SD_routines.h"
#include "mcu_driver.h"

uint8_t SD_status = 0;
uint8_t SD_cid[16];
volatile uint32_t startBlock, totalBlocks;
volatile uint8_t buffer[512];

static inline uint8_t sd_spi_transfer(uint8_t data)
{
    uint8_t r;
    SPI_DATA = data;
    while(!SPI_IF);
    r = SPI_DATA;
    return r;
}



uint8_t SD_check_alive()
{
  uint8_t cid[16];

  if(!(SD_status & SD_STATUS_INIT_OK))
  {
    SD_init();
    if(!(SD_status & SD_STATUS_INIT_OK)) return 0;
  }

  if(!SD_read_CID(cid))
  {
    SD_status = SD_ERR_CID_TIMEOUT;
    SPI_MAX_SET;
    return 0;
  }

  for(uint8_t a = 0; a < 16; a++)
  {
    if(cid[a] != SD_cid[a])
    {
      SD_status = SD_ERR_CID_MISMATCH;
      return 0;
    }
  }

  return 1;
}

uint8_t SD_init(void)
{
    uint8_t r;
    uint8_t i;
    uint8_t ocr[4];
    uint8_t cid[16];

    SD_status = 0;

    SPI_LOW_SET;
    SD_CS_DIRSET;

    // --- 1. 160 clock cycles with CS high ---
    SD_CS_DEASSERT;
    for (uint8_t a = 0; a < 20; a++) sd_spi_transfer(0xFF);

    // --- 2. CMD0 ---
    for (i = 0; i < 200; i++)
    {
        r = SD_send_cmd(GO_IDLE_STATE, 0, 0x95);
        SD_cmd_end();
        if (r == 0x01) break;
    }

    if (r != 0x01)
    {
        SD_status = SD_ERR_CMD0;
        SPI_MAX_SET;
        return r;
    }

    // --- 3. CMD8 (check SD v2) ---
    r = SD_send_cmd(8, 0x1AA, 0x87);

    if (r == 0x01)
    {
        // Read R7 (4 bytes)
        for (i = 0; i < 4; i++)
            ocr[i] = sd_spi_transfer(0xFF);

        SD_cmd_end();

        if (ocr[2] != 0x01 || ocr[3] != 0xAA)
        {
            SD_status = SD_ERR_CMD8;
            SPI_MAX_SET;
            return r;
        }

        // --- 4. ACMD41 (SD v2) ---
        for (i = 0; i < 255; i++)
        {
            r = SD_send_cmd(55, 0, 0xFF);
            SD_cmd_end();

            if (r > 0x01) continue;

            r = SD_send_cmd(41, 0x40000000, 0xFF);
            SD_cmd_end();

            if (r == 0x00) break;
        }

        if (r != 0x00)
        {
            SD_status = SD_ERR_ACMD41;
            SPI_MAX_SET;
            return r;
        }

        // --- 5. CMD58 (read OCR) ---
        r = SD_send_cmd(58, 0, 0xFF);

        if (r != 0x00)
        {
            SD_cmd_end();
            SD_status = SD_ERR_CMD58;
            SPI_MAX_SET;
            return r;
        }

        for (i = 0; i < 4; i++)
            ocr[i] = sd_spi_transfer(0xFF);

        SD_cmd_end();

        // Check CCS (SDHC)
        if (ocr[0] & 0x40)
            SD_status |= SD_STATUS_TYPE_V2;
        else
            SD_status |= SD_STATUS_TYPE_V1;

        SD_status |= SD_STATUS_INIT_OK;
    }
    else
    {
        // --- Old card (SDSC or MMC) ---
        for (i = 0; i < 255; i++)
        {
            r = SD_send_cmd(1, 0, 0xFF);
            SD_cmd_end();

            if (r == 0x00) break;
        }

        if (r != 0x00)
        {
            SD_status = SD_ERR_TIMEOUT;
            SPI_MAX_SET;
            return r;
        }

        SD_status |= SD_STATUS_TYPE_V1;
        SD_status |= SD_STATUS_INIT_OK;
    }

    // --- 6. Set block length (SDSC only) ---
    if (SD_status & SD_STATUS_TYPE_V1)
    {
        r = SD_send_cmd(SET_BLOCK_LEN, 512, 0xFF);
        SD_cmd_end();

        if (r != 0x00)
        {
            SD_status = SD_ERR_TIMEOUT;
            SPI_MAX_SET;
            return r;
        }
    }

    SPI_SD_SET;

    if(!SD_read_CID(cid))
    {
      SD_status = SD_ERR_CID_TIMEOUT;
      SPI_MAX_SET;
      return r;
    }

    for(uint8_t a = 0; a < 16; a++) SD_cid[a] = cid[a];

    // --- 7. Increase SPI speed ---
    SPI_MAX_SET;
    return 0;
}

static uint8_t SD_read_CID(uint8_t * cid)
{
    uint8_t r;
    uint8_t i;

    SPI_SD_SET;
    r = SD_send_cmd(READ_CID, 0, 0xFF);
    if (r != 0x00)
    {
        SD_cmd_end();
        SPI_MAX_SET; 
        return 0;
    }

    // Wait for data token 0xFE
    i = 0;
    do {
        r = sd_spi_transfer(0xFF);
        if (++i > 200) { SD_cmd_end(); SPI_MAX_SET; return 0; }
    } while (r != 0xFE);

    // Read 16 bytes CID
    for (i = 0; i < 16; i++)
        cid[i] = sd_spi_transfer(0xFF);

    // 2 CRC bytes
    sd_spi_transfer(0xFF);
    sd_spi_transfer(0xFF);

    sd_spi_transfer(0xFF);
    SD_cmd_end();

    SPI_MAX_SET; 
    return 1;
}

static uint8_t SD_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response;
    uint8_t retry = 0;

    SD_CS_ASSERT;

    sd_spi_transfer(0xFF); // Ncr

    sd_spi_transfer(cmd | 0x40);
    sd_spi_transfer(arg >> 24);
    sd_spi_transfer(arg >> 16);
    sd_spi_transfer(arg >> 8);
    sd_spi_transfer(arg);
    sd_spi_transfer(crc);

    do {
        response = sd_spi_transfer(0xFF);
    } while ((response & 0x80) && (++retry < 100));

    return response;
}

static void SD_cmd_end(void)
{
    SD_CS_DEASSERT;
    sd_spi_transfer(0xFF);
}

uint8_t SD_erase(uint32_t startBlock, uint32_t totalBlocks)
{
uint8_t response;

if(!(SD_status & SD_STATUS_INIT_OK)) { SD_status = SD_ERR_NO_CARD; return 0xff; }

SPI_SD_SET;

response = SD_send_cmd(ERASE_BLOCK_START_ADDR, startBlock<<9, 0xff); //send starting block address
SD_cmd_end();
if(response != 0x00) //check for SD status: 0x00 - OK (No flags set)
  { SPI_MAX_SET; return response; }

response = SD_send_cmd(ERASE_BLOCK_END_ADDR,(startBlock + totalBlocks - 1)<<9, 0xff); //send end block address
SD_cmd_end();
if(response != 0x00)
  { SPI_MAX_SET; return response; }

response = SD_send_cmd(ERASE_SELECTED_BLOCKS, 0, 0xff); //erase all selected blocks
SD_cmd_end();
if(response != 0x00)
  { SPI_MAX_SET; return response; }

  SPI_MAX_SET;
return 0; //normal return
}

uint8_t SD_readSingleBlock(uint32_t startBlock)
{
uint8_t response;
uint16_t i, retry=0;

if(!(SD_status & SD_STATUS_INIT_OK)) { SD_status = SD_ERR_NO_CARD; return 0xff; }

SPI_SD_SET;

response = SD_send_cmd(READ_SINGLE_BLOCK, startBlock<<9, 0xff); //read a Block command
//block address converted to starting address of 512 byte Block
//SD_cmd_end();
if(response != 0x00) //check for SD status: 0x00 - OK (No flags set)
  {
    SD_cmd_end();
    SPI_MAX_SET;
    return response;
  }

//SD_CS_ASSERT;

retry = 0;
while(sd_spi_transfer(0xff) != 0xfe) //wait for start block token 0xfe (0x11111110)
  if(retry++ > 0xfffe){SD_cmd_end(); SPI_MAX_SET; return 1;} //return if time-out

for(i=0; i<512; i++) //read 512 bytes
  buffer[i] = sd_spi_transfer(0xff);

sd_spi_transfer(0xff); //receive incoming CRC (16-bit), CRC is ignored here
sd_spi_transfer(0xff);

sd_spi_transfer(0xff); //extra 8 clock pulses
SD_cmd_end();

SPI_MAX_SET;
return 0;
}

uint8_t SD_writeSingleBlock(uint32_t startBlock)
{
uint8_t response;
uint16_t i, retry = 0;

if(!(SD_status & SD_STATUS_INIT_OK)) { SD_status = SD_ERR_NO_CARD; return 0xff; }

SPI_SD_SET;

response = SD_send_cmd(WRITE_SINGLE_BLOCK, startBlock<<9, 0xff); //write a Block command
if(response != 0x00) //check for SD status: 0x00 - OK (No flags set)
{ SD_cmd_end(); SPI_MAX_SET; return response; }

sd_spi_transfer(0xfe);     //Send start block token 0xfe (0x11111110)

for(i=0; i<512; i++)    //send 512 bytes data
  sd_spi_transfer(buffer[i]);

sd_spi_transfer(0xff);     //transmit dummy CRC (16-bit), CRC is ignored here
sd_spi_transfer(0xff);

response = sd_spi_transfer(0xff);

if( (response & 0x1f) != 0x05) //response= 0xXXX0AAA1 ; AAA='010' - data accepted
{                              //AAA='101'-data rejected due to CRC error
  SD_cmd_end(); 
  SPI_MAX_SET;               //AAA='110'-data rejected due to write error
  return response;
}

while(!sd_spi_transfer(0xff)) //wait for SD card to complete writing and get idle
if(retry++ > 0xfffe){SD_CS_DEASSERT; SPI_MAX_SET; return 1;}

SD_CS_DEASSERT;
sd_spi_transfer(0xff);   //just spend 8 clock cycle delay before reasserting the CS line
SD_CS_ASSERT;         //re-asserting the CS line to verify if card is still busy

while(!sd_spi_transfer(0xff)) //wait for SD card to complete writing and get idle
   if(retry++ > 0xfffe){SD_CS_DEASSERT; SPI_MAX_SET; return 1;}

   SD_CS_DEASSERT;
SPI_MAX_SET;
return 0;
}

#ifndef FAT_TESTING_ONLY
//***************************************************************************
//Function: to read multiple blocks from SD card & send every block to UART
//Arguments: none
//return: uint8_t; will be 0 if no error,
// otherwise the response byte will be sent
//****************************************************************************
uint8_t SD_readMultipleBlock (uint32_t startBlock, uint32_t totalBlocks)
{
uint8_t response;
uint16_t i, retry=0;

retry = 0;

response = SD_send_cmd(READ_MULTIPLE_BLOCKS, startBlock <<9); //read a Block command
//block address converted to starting address of 512 byte Block
if(response != 0x00) //check for SD status: 0x00 - OK (No flags set)
return response;

SD_CS_ASSERT;

while( totalBlocks )
{
  retry = 0;
  while(sd_spi_transfer(0xff) != 0xfe) //wait for start block token 0xfe (0x11111110)
  if(retry++ > 0xfffe){SD_CS_DEASSERT; return 1;} //return if time-out

  for(i=0; i<512; i++) //read 512 bytes
    buffer[i] = sd_spi_transfer(0xff);

  sd_spi_transfer(0xff); //receive incoming CRC (16-bit), CRC is ignored here
  sd_spi_transfer(0xff);

  sd_spi_transfer(0xff); //extra 8 cycles
  TX_NEWLINE;
  transmitString_F(PSTR(" --------- "));
  TX_NEWLINE;

  for(i=0; i<512; i++) //send the block to UART
  {
    if(buffer[i] == '~') break;
    transmitByte ( buffer[i] );
  }

  TX_NEWLINE;
  transmitString_F(PSTR(" --------- "));
  TX_NEWLINE;
  totalBlocks--;
}

SD_send_cmd(STOP_TRANSMISSION, 0); //command to stop transmission
SD_CS_DEASSERT;
sd_spi_transfer(0xff); //extra 8 clock pulses

return 0;
}

//***************************************************************************
//Function: to receive data from UART and write to multiple blocks of SD card
//Arguments: none
//return: uint8_t; will be 0 if no error,
// otherwise the response byte will be sent
//****************************************************************************
uint8_t SD_writeMultipleBlock(uint32_t startBlock, uint32_t totalBlocks)
{
uint8_t response, data;
uint16_t i, retry=0;
uint32_t blockCounter=0, size;

response = SD_send_cmd(WRITE_MULTIPLE_BLOCKS, startBlock<<9); //write a Block command
if(response != 0x00) //check for SD status: 0x00 - OK (No flags set)
  return response;

SD_CS_ASSERT;

TX_NEWLINE;
transmitString_F(PSTR(" Enter text (End with ~): "));
TX_NEWLINE;

while( blockCounter < totalBlocks )
{
   i=0;
   do
   {
     data = receiveByte();
     if(data == 0x08)	//'Back Space' key pressed
	 { 
	   if(i != 0)
	   { 
	     transmitByte(data);
	     transmitByte(' '); 
	     transmitByte(data); 
	     i--; 
		 size--;
	   } 
	   continue;     
	 }
     transmitByte(data);
     buffer[i++] = data;
     if(data == 0x0d)
     {
        transmitByte(0x0a);
        buffer[i++] = 0x0a;
     }
	 if(i == 512) break;
   }while (data != '~');

   TX_NEWLINE;
   transmitString_F(PSTR(" ---- "));
   TX_NEWLINE;

   sd_spi_transfer(0xfc); //Send start block token 0xfc (0x11111100)

   for(i=0; i<512; i++) //send 512 bytes data
     sd_spi_transfer( buffer[i] );

   sd_spi_transfer(0xff); //transmit dummy CRC (16-bit), CRC is ignored here
   sd_spi_transfer(0xff);

   response = sd_spi_transfer(0xff);
   if( (response & 0x1f) != 0x05) //response= 0xXXX0AAA1 ; AAA='010' - data accepted
   {                              //AAA='101'-data rejected due to CRC error
      SD_CS_DEASSERT;             //AAA='110'-data rejected due to write error
      return response;
   }

   while(!sd_spi_transfer(0xff)) //wait for SD card to complete writing and get idle
     if(retry++ > 0xfffe){SD_CS_DEASSERT; return 1;}

   sd_spi_transfer(0xff); //extra 8 bits
   blockCounter++;
}

sd_spi_transfer(0xfd); //send 'stop transmission token'

retry = 0;

while(!sd_spi_transfer(0xff)) //wait for SD card to complete writing and get idle
   if(retry++ > 0xfffe){SD_CS_DEASSERT; return 1;}

SD_CS_DEASSERT;
sd_spi_transfer(0xff); //just spend 8 clock cycle delay before reasserting the CS signal
SD_CS_ASSERT; //re assertion of the CS signal is required to verify if card is still busy

while(!sd_spi_transfer(0xff)) //wait for SD card to complete writing and get idle
   if(retry++ > 0xfffe){SD_CS_DEASSERT; return 1;}
SD_CS_DEASSERT;

return 0;
}
//*********************************************
#endif

//******** END ****** www.dharmanitech.com *****
