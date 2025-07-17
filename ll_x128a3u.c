
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "x128_ili9488_driver.h"
#include "ningen_ui.h"
#include "ll_x128a3u.h"

ISR(TWIE_TWIM_vect) { cli(); lcd_write_debug("Vector 46"); return; }
ISR(TCC1_CCA_vect) { cli();  lcd_write_debug("Vector 22"); return; }
ISR(PORTD_INT1_vect) { cli();  lcd_write_debug("Vector 65"); return; }
ISR(PORTR_INT0_vect) { cli();  lcd_write_debug("Vector 4"); return; }
ISR(TCC1_CCB_vect) { cli();  lcd_write_debug("Vector 23"); return; }
ISR(TCE1_OVF_vect) { cli();  lcd_write_debug("Vector 53"); return; }
ISR(USARTC1_TXC_vect) { cli();  lcd_write_debug("Vector 30"); return; }
ISR(RTC_COMP_vect) { cli();  lcd_write_debug("Vector 11"); return; }
ISR(TCE1_ERR_vect) { cli();  lcd_write_debug("Vector 54"); return; }
ISR(TCE0_ERR_vect) { cli();  lcd_write_debug("Vector 48"); return; }
ISR(ACB_AC0_vect) { cli();  lcd_write_debug("Vector 36"); return; }
ISR(TCD0_ERR_vect) { cli();  lcd_write_debug("Vector 78"); return; }
ISR(NVM_SPM_vect) { cli();  lcd_write_debug("Vector 33"); return; }
ISR(PORTR_INT1_vect) { cli();  lcd_write_debug("Vector 5"); return; }
ISR(TWIC_TWIS_vect) { cli();  lcd_write_debug("Vector 12"); return; }
ISR(TCE2_LUNF_vect) { cli();  lcd_write_debug("Vector 47"); return; }
//ISR(TCC0_CCA_vect) { cli();  lcd_write_debug("Vector 16"); return; } // USING VECTOR DOWN
ISR(USARTE0_RXC_vect) { cli();  lcd_write_debug("Vector 58"); return; }
// ISR(TCC2_LCMPB_vect) { cli();  lcd_write_debug("Vector 17"); return; } // USING VECTOR DOWN
ISR(TCD0_CCD_vect) { cli();  lcd_write_debug("Vector 82"); return; }
ISR(USARTD1_DRE_vect) { cli();  lcd_write_debug("Vector 92"); return; }
ISR(PORTA_INT1_vect) { cli();  lcd_write_debug("Vector 67"); return; }
//ISR(TCC2_LCMPA_vect) { cli();  lcd_write_debug("Vector 16"); return; }
//ISR(TCE0_OVF_vect) { cli();  lcd_write_debug("Vector 47"); return; }
ISR(PORTC_INT0_vect) { cli();  lcd_write_debug("Vector 2"); return; }
ISR(USARTE1_TXC_vect) { cli();  lcd_write_debug("Vector 63"); return; }
//ISR(TCC0_CCB_vect) { cli();  lcd_write_debug("Vector 17"); return; }
ISR(USARTD0_TXC_vect) { cli();  lcd_write_debug("Vector 90"); return; }
ISR(ACA_ACW_vect) { cli();  lcd_write_debug("Vector 70"); return; }
ISR(ACA_AC0_vect) { cli();  lcd_write_debug("Vector 68"); return; }
ISR(USARTD0_RXC_vect) { cli();  lcd_write_debug("Vector 88"); return; }
//ISR(TCD2_LCMPD_vect) { cli();  lcd_write_debug("Vector 82"); return; }
ISR(TCF2_LCMPA_vect) { cli();  lcd_write_debug("Vector 110"); return; }
ISR(TCC0_CCC_vect) { cli();  lcd_write_debug("Vector 18"); return; }
//ISR(TCF0_CCA_vect) { cli();  lcd_write_debug("Vector 110"); return; }
ISR(TCD1_OVF_vect) { cli();  lcd_write_debug("Vector 83"); return; }
ISR(TCC0_OVF_vect) { cli();  lcd_write_debug("Vector 14"); return; }
ISR(SPID_INT_vect) { cli();  lcd_write_debug("Vector 87"); return; }
ISR(TCF2_LUNF_vect) { cli();  lcd_write_debug("Vector 108"); return; }
ISR(USARTC1_DRE_vect) { cli();  lcd_write_debug("Vector 29"); return; }
ISR(TCF2_LCMPB_vect) { cli();  lcd_write_debug("Vector 111"); return; }
//ISR(TCF0_CCB_vect) { cli();  lcd_write_debug("Vector 111"); return; }
ISR(TCE0_CCD_vect) { cli();  lcd_write_debug("Vector 52"); return; }
ISR(USARTC1_RXC_vect) { cli();  lcd_write_debug("Vector 28"); return; }
//ISR(TCC2_LCMPC_vect) { cli();  lcd_write_debug("Vector 18"); return; }
ISR(TCC0_CCD_vect) { cli();  lcd_write_debug("Vector 19"); return; }
ISR(OSC_OSCF_vect) { cli();  lcd_write_debug("Vector 1"); return; }
ISR(TCE1_CCA_vect) { cli();  lcd_write_debug("Vector 55"); return; }
ISR(PORTF_INT1_vect) { cli();  lcd_write_debug("Vector 105"); return; }
ISR(TCD1_CCA_vect) { cli();  lcd_write_debug("Vector 85"); return; }
ISR(TCD0_OVF_vect) { cli();  lcd_write_debug("Vector 77"); return; }
ISR(TCC0_ERR_vect) { cli();  lcd_write_debug("Vector 15"); return; }
ISR(TCD2_LCMPC_vect) { cli();  lcd_write_debug("Vector 81"); return; }
//ISR(TCC2_LCMPD_vect) { cli();  lcd_write_debug("Vector 19"); return; }
ISR(TCF2_LCMPC_vect) { cli();  lcd_write_debug("Vector 112"); return; }
ISR(TCF2_HUNF_vect) { cli();  lcd_write_debug("Vector 109"); return; }
ISR(SPIC_INT_vect) { cli();  lcd_write_debug("Vector 24"); return; }
//ISR(TCC2_LUNF_vect) { cli();  lcd_write_debug("Vector 14"); return; }
ISR(TCE1_CCB_vect) { cli();  lcd_write_debug("Vector 56"); return; }
ISR(PORTB_INT1_vect) { cli();  lcd_write_debug("Vector 35"); return; }
ISR(DMA_CH0_vect) { cli();  lcd_write_debug("Vector 6"); return; }
//ISR(TCF0_CCC_vect) { cli();  lcd_write_debug("Vector 112"); return; }
//ISR(TCC2_HUNF_vect) { cli();  lcd_write_debug("Vector 15"); return; }
ISR(ADCB_CH1_vect) { cli();  lcd_write_debug("Vector 40"); return; }
ISR(AES_INT_vect) { cli();  lcd_write_debug("Vector 31"); return; }
ISR(USARTF0_DRE_vect) { cli();  lcd_write_debug("Vector 120"); return; }
ISR(ADCA_CH0_vect) { cli();  lcd_write_debug("Vector 71"); return; }
ISR(TWIC_TWIM_vect) { cli();  lcd_write_debug("Vector 13"); return; }
ISR(USARTC0_RXC_vect) { cli();  lcd_write_debug("Vector 25"); return; }
ISR(TWIE_TWIS_vect) { cli();  lcd_write_debug("Vector 45"); return; }
ISR(TCF2_LCMPD_vect) { cli();  lcd_write_debug("Vector 113"); return; }
ISR(TCE2_LCMPA_vect) { cli();  lcd_write_debug("Vector 49"); return; }
//ISR(TCE0_CCA_vect) { cli();  lcd_write_debug("Vector 49"); return; }
ISR(DMA_CH1_vect) { cli();  lcd_write_debug("Vector 7"); return; }
ISR(RTC_OVF_vect) { cli();  lcd_write_debug("Vector 10"); return; }
ISR(TCC1_ERR_vect) { cli();  lcd_write_debug("Vector 21"); return; }
ISR(USARTC0_TXC_vect) { cli();  lcd_write_debug("Vector 27"); return; }
//ISR(TCF0_CCD_vect) { cli();  lcd_write_debug("Vector 113"); return; }
ISR(TCC1_OVF_vect) { cli();  lcd_write_debug("Vector 20"); return; }
ISR(TCD1_ERR_vect) { cli();  lcd_write_debug("Vector 84"); return; }
ISR(TCD2_LCMPB_vect) { cli();  lcd_write_debug("Vector 80"); return; }
ISR(USB_TRNCOMPL_vect) { cli();  lcd_write_debug("Vector 126"); return; }
ISR(ADCB_CH2_vect) { cli();  lcd_write_debug("Vector 41"); return; }
ISR(USARTE0_TXC_vect) { cli();  lcd_write_debug("Vector 60"); return; }
ISR(PORTF_INT0_vect) { cli();  lcd_write_debug("Vector 104"); return; }
ISR(PORTE_INT0_vect) { cli();  lcd_write_debug("Vector 43"); return; }
ISR(ADCA_CH1_vect) { cli();  lcd_write_debug("Vector 72"); return; }
ISR(USARTD1_RXC_vect) { cli();  lcd_write_debug("Vector 91"); return; }
ISR(USARTE1_DRE_vect) { cli();  lcd_write_debug("Vector 62"); return; }
ISR(TCE2_LCMPB_vect) { cli();  lcd_write_debug("Vector 50"); return; }
//ISR(TCE0_CCB_vect) { cli();  lcd_write_debug("Vector 50"); return; }
ISR(ADCB_CH0_vect) { cli();  lcd_write_debug("Vector 39"); return; }
ISR(DMA_CH2_vect) { cli();  lcd_write_debug("Vector 8"); return; }
ISR(ACA_AC1_vect) { cli();  lcd_write_debug("Vector 69"); return; }
ISR(USARTD1_TXC_vect) { cli();  lcd_write_debug("Vector 93"); return; }
ISR(USARTF0_RXC_vect) { cli();  lcd_write_debug("Vector 119"); return; }
ISR(ADCB_CH3_vect) { cli();  lcd_write_debug("Vector 42"); return; }
//ISR(TCD0_CCC_vect) { cli();  lcd_write_debug("Vector 81"); return; }
ISR(PORTE_INT1_vect) { cli();  lcd_write_debug("Vector 44"); return; }
ISR(USARTF0_TXC_vect) { cli();  lcd_write_debug("Vector 121"); return; }
//ISR(TCD2_LUNF_vect) { cli();  lcd_write_debug("Vector 77"); return; }
ISR(PORTB_INT0_vect) { cli();  lcd_write_debug("Vector 34"); return; }
ISR(PORTA_INT0_vect) { cli();  lcd_write_debug("Vector 66"); return; }
ISR(USB_BUSEVENT_vect) { cli();  lcd_write_debug("Vector 125"); return; }
ISR(ADCA_CH2_vect) { cli();  lcd_write_debug("Vector 73"); return; }
ISR(TCE2_LCMPC_vect) { cli();  lcd_write_debug("Vector 51"); return; }
//ISR(TCE0_CCC_vect) { cli();  lcd_write_debug("Vector 51"); return; }
ISR(USARTE0_DRE_vect) { cli();  lcd_write_debug("Vector 59"); return; }
ISR(TCD1_CCB_vect) { cli();  lcd_write_debug("Vector 86"); return; }
//ISR(TCD2_HUNF_vect) { cli();  lcd_write_debug("Vector 78"); return; }
//ISR(TCE2_HUNF_vect) { cli();  lcd_write_debug("Vector 48"); return; }
ISR(PORTC_INT1_vect) { cli();  lcd_write_debug("Vector 3"); return; }
ISR(USARTD0_DRE_vect) { cli();  lcd_write_debug("Vector 89"); return; }
ISR(DMA_CH3_vect) { cli();  lcd_write_debug("Vector 9"); return; }
ISR(USARTE1_RXC_vect) { cli();  lcd_write_debug("Vector 61"); return; }
ISR(USARTC0_DRE_vect) { cli();  lcd_write_debug("Vector 26"); return; }
ISR(ACB_AC1_vect) { cli();  lcd_write_debug("Vector 37"); return; }
ISR(ACB_ACW_vect) { cli();  lcd_write_debug("Vector 38"); return; }
ISR(ADCA_CH3_vect) { cli();  lcd_write_debug("Vector 74"); return; }
ISR(TCD0_CCA_vect) { cli();  lcd_write_debug("Vector 79"); return; }
ISR(SPIE_INT_vect) { cli();  lcd_write_debug("Vector 57"); return; }
//ISR(TCE2_LCMPD_vect) { cli();  lcd_write_debug("Vector 52"); return; }
//ISR(TCD2_LCMPA_vect) { cli();  lcd_write_debug("Vector 79"); return; }
//ISR(TCF0_OVF_vect) { cli();  lcd_write_debug("Vector 108"); return; }
ISR(PORTD_INT0_vect) { cli();  lcd_write_debug("Vector 64"); return; }
//ISR(TCF0_ERR_vect) { cli();  lcd_write_debug("Vector 109"); return; }
//ISR(TCD0_CCB_vect) { cli();  lcd_write_debug("Vector 80"); return; }
ISR(NVM_EE_vect) { cli();  lcd_write_debug("Vector 32"); return; }





uint8_t ll_timer0_pwm = 0;
uint8_t ll_timer0_pwm_port = 0;
uint8_t ll_timer0_pwm_pin = 0;




static inline void NVM_EXEC(void)
{
        void *z = (void *)&NVM_CTRLA;

        __asm__ volatile("out %[ccp], %[ioreg]"  "\n\t"
        "st z, %[cmdex]"
        :
        : [ccp] "I" (_SFR_IO_ADDR(CCP)),
        [ioreg] "d" (CCP_IOREG_gc),
                     [cmdex] "r" (NVM_CMDEX_bm),
                     [z] "z" (z)
                     );
}

void EEPROM_WriteByte(uint16_t address, uint8_t value)//( uint8_t pageAddr, uint8_t byteAddr, uint8_t value )
{
	/*  Flush buffer to make sure no unintetional data is written and load
	 *  the "Page Load" command into the command register.
	 */
	EEPROM_FlushBuffer();
	NVM.CMD = NVM_CMD_LOAD_EEPROM_BUFFER_gc;

	/* Calculate address */
	//uint16_t address = (uint16_t)(pageAddr*EEPROM_PAGESIZE)
	//                            |(byteAddr & (EEPROM_PAGESIZE-1));

	/* Set address to write to. */
	NVM.ADDR0 = address & 0xFF;
	NVM.ADDR1 = (address >> 8) & 0x1F;
	NVM.ADDR2 = 0x00;

	/* Load data to write, which triggers the loading of EEPROM page buffer. */
	NVM.DATA0 = value;

	/*  Issue EEPROM Atomic Write (Erase&Write) command. Load command, write
	 *  the protection signature and execute command.
	 */
	NVM.CMD = NVM_CMD_ERASE_WRITE_EEPROM_PAGE_gc;
	NVM_EXEC();
}


/*! \brief Read one byte from EEPROM using IO mapping.
 *
 *  This function reads one byte from EEPROM using IO-mapped access.
 *  Please note that the memory mapped EERPROM can not be used when using this function.
 *
 *  \param  pageAddr  EEPROM Page address, between 0 and EEPROM_SIZE/EEPROM_PAGESIZE
 *  \param  byteAddr  EEPROM Byte address, between 0 and EEPROM_PAGESIZE.
 *
 *  \return  Byte value read from EEPROM.
 */
uint8_t EEPROM_ReadByte(uint16_t address)//( uint8_t pageAddr, uint8_t byteAddr )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Calculate address */
	//uint16_t address = (uint16_t)(pageAddr*EEPROM_PAGESIZE)
	//                            |(byteAddr & (EEPROM_PAGESIZE-1));

	/* Set address to read from. */
	NVM.ADDR0 = address & 0xFF;
	NVM.ADDR1 = (address >> 8) & 0x1F;
	NVM.ADDR2 = 0x00;

	/* Issue EEPROM Read command. */
	NVM.CMD = NVM_CMD_READ_EEPROM_gc;
	NVM_EXEC();

	return NVM.DATA0;
}


/*! \brief Wait for any NVM access to finish, including EEPROM.
 *
 *  This function is blcoking and waits for any NVM access to finish,
 *  including EEPROM. Use this function before any EEPROM accesses,
 *  if you are not certain that any previous operations are finished yet,
 *  like an EEPROM write.
 */
void EEPROM_WaitForNVM( void )
{
	do {
		/* Block execution while waiting for the NVM to be ready. */
	} while ((NVM.STATUS & NVM_NVMBUSY_bm) == NVM_NVMBUSY_bm);
}


/*! \brief Flush temporary EEPROM page buffer.
 *
 *  This function flushes the EEPROM page buffers. This function will cancel
 *  any ongoing EEPROM page buffer loading operations, if any.
 *  This function also works for memory mapped EEPROM access.
 *
 *  \note The EEPROM write operations will automatically flush the buffer for you.
 */
void EEPROM_FlushBuffer( void )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Flush EEPROM page buffer if necessary. */
	if ((NVM.STATUS & NVM_EELOAD_bm) != 0) {
		NVM.CMD = NVM_CMD_ERASE_EEPROM_BUFFER_gc;
		NVM_EXEC();
	}
}


/*! \brief Load single byte into temporary page buffer.
 *
 *  This function loads one byte into the temporary EEPROM page buffers.
 *  If memory mapped EEPROM is enabled, this function will not work.
 *  Make sure that the buffer is flushed before starting to load bytes.
 *  Also, if multiple bytes are loaded into the same location, they will
 *  be ANDed together, thus 0x55 and 0xAA will result in 0x00 in the buffer.
 *
 *  \note Only one page buffer exist, thus only one page can be loaded with
 *        data and programmed into one page. If data needs to be written to
 *        different pages, the loading and writing needs to be repeated.
 *
 *  \param  byteAddr  EEPROM Byte address, between 0 and EEPROM_PAGESIZE.
 *  \param  value     Byte value to write to buffer.
 */
void EEPROM_LoadByte( uint8_t byteAddr, uint8_t value )
{
	/* Wait until NVM is not busy and prepare NVM command.*/
	EEPROM_WaitForNVM();
	NVM.CMD = NVM_CMD_LOAD_EEPROM_BUFFER_gc;

	/* Set address. */
	NVM.ADDR0 = byteAddr & 0xFF;
	NVM.ADDR1 = 0x00;
	NVM.ADDR2 = 0x00;

	/* Set data, which triggers loading of EEPROM page buffer. */
	NVM.DATA0 = value;
}


/*! \brief Load entire page into temporary EEPROM page buffer.
 *
 *  This function loads an entire EEPROM page from an SRAM buffer to
 *  the EEPROM page buffers. Please note that the memory mapped EERPROM can not be used when using this function.
 *  Make sure that the buffer is flushed before
 *  starting to load bytes.
 *
 *  \note Only the lower part of the address is used to address the buffer.
 *        Therefore, no address parameter is needed. In the end, the data
 *        is written to the EEPROM page given by the address parameter to the
 *        EEPROM write page operation.
 *
 *  \param  values   Pointer to SRAM buffer containing an entire page.
 */
void EEPROM_LoadPage( const uint8_t * values )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();
	NVM.CMD = NVM_CMD_LOAD_EEPROM_BUFFER_gc;

	/*  Set address to zero, as only the lower bits matters. ADDR0 is
	 *  maintained inside the loop below.
	 */
	NVM.ADDR1 = 0x00;
	NVM.ADDR2 = 0x00;

	/* Load multible bytes into page buffer. */
	for (uint8_t i = 0; i < EEPROM_PAGESIZE; ++i) {
		NVM.ADDR0 = i;
		NVM.DATA0 = *values;
		++values;
	}
}

/*! \brief Write already loaded page into EEPROM.
 *
 *  This function writes the contents of an already loaded EEPROM page
 *  buffer into EEPROM memory.
 *
 *  As this is an atomic write, the page in EEPROM will be erased
 *  automatically before writing. Note that only the page buffer locations
 *  that have been loaded will be used when writing to EEPROM. Page buffer
 *  locations that have not been loaded will be left untouched in EEPROM.
 *
 *  \param  pageAddr  EEPROM Page address, between 0 and EEPROM_SIZE/EEPROM_PAGESIZE
 */
void EEPROM_AtomicWritePage( uint8_t pageAddr )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Calculate page address */
	uint16_t address = (uint16_t)(pageAddr*EEPROM_PAGESIZE);

	/* Set address. */
	NVM.ADDR0 = address & 0xFF;
	NVM.ADDR1 = (address >> 8) & 0x1F;
	NVM.ADDR2 = 0x00;

	/* Issue EEPROM Atomic Write (Erase&Write) command. */
	NVM.CMD = NVM_CMD_ERASE_WRITE_EEPROM_PAGE_gc;
	NVM_EXEC();
}


/*! \brief Erase EEPROM page.
 *
 *  This function erases one EEPROM page, so that every location reads 0xFF.
 *
 *  \param  pageAddr  EEPROM Page address, between 0 and EEPROM_SIZE/EEPROM_PAGESIZE
 */
void EEPROM_ErasePage( uint8_t pageAddr )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Calculate page address */
	uint16_t address = (uint16_t)(pageAddr*EEPROM_PAGESIZE);

	/* Set address. */
	NVM.ADDR0 = address & 0xFF;
	NVM.ADDR1 = (address >> 8) & 0x1F;
	NVM.ADDR2 = 0x00;

	/* Issue EEPROM Erase command. */
	NVM.CMD = NVM_CMD_ERASE_EEPROM_PAGE_gc;
	NVM_EXEC();
}


/*! \brief Write (without erasing) EEPROM page.
 *
 *  This function writes the contents of an already loaded EEPROM page
 *  buffer into EEPROM memory.
 *
 *  As this is a split write, the page in EEPROM will _not_ be erased
 *  before writing.
 *
 *  \param  pageAddr  EEPROM Page address, between 0 and EEPROM_SIZE/EEPROM_PAGESIZE
 */
void EEPROM_SplitWritePage( uint8_t pageAddr )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Calculate page address */
	uint16_t address = (uint16_t)(pageAddr*EEPROM_PAGESIZE);

	/* Set address. */
	NVM.ADDR0 = address & 0xFF;
	NVM.ADDR1 = (address >> 8) & 0x1F;
	NVM.ADDR2 = 0x00;

	/* Issue EEPROM Split Write command. */
	NVM.CMD = NVM_CMD_WRITE_EEPROM_PAGE_gc;
	NVM_EXEC();
}

/*! \brief Erase entire EEPROM memory.
 *
 *  This function erases the entire EEPROM memory block to 0xFF.
 */
void EEPROM_EraseAll( void )
{
	/* Wait until NVM is not busy. */
	EEPROM_WaitForNVM();

	/* Issue EEPROM Erase All command. */
	NVM.CMD = NVM_CMD_ERASE_EEPROM_gc;
	NVM_EXEC();
}

// Function to wait for NVM to be ready
static inline void wait_for_nvm(void) {
    while (NVM.STATUS & NVM_NVMBUSY_bm);
}

// Function to load the flash page buffer
static void flash_load_page_buffer(const uint8_t* data, uint16_t size) {
    // Wait for NVM to be ready
    wait_for_nvm();

    // Load multiple bytes into page buffer
    for (uint16_t i = 0; i < size; i++) {
        NVM.CMD = NVM_CMD_LOAD_FLASH_BUFFER_gc;
        NVM_ADDR0 = i;
        NVM_DATA0 = data[i];
    }
}

// Function to write page buffer to flash
static void flash_write_page(uint32_t address) {
    // Wait for NVM to be ready
    wait_for_nvm();

    // Calculate the page address
    uint32_t page_addr = address - (address % SPM_PAGESIZE);

    // Erase and write page
    NVM.CMD = NVM_CMD_ERASE_WRITE_APP_PAGE_gc;
    NVM.ADDR0 = page_addr & 0xFF;
    NVM.ADDR1 = (page_addr >> 8) & 0xFF;
    NVM.ADDR2 = (page_addr >> 16) & 0xFF;

    // Execute command
    CCP = CCP_SPM_gc;
    NVM.CTRLA = NVM_CMDEX_bm;

    // Wait for completion
    wait_for_nvm();
}

void SPI_set(uint8_t speed)
{
	DDR_LCD_CS |= (1 << PIN_LCD_CS);
	PORT_LCD_CS |= (1 << PIN_LCD_CS);

	DDR_LCD_CS |= (1 << PIN_LCD_DC);
	PORT_LCD_CS |= (1 << PIN_LCD_DC);

	DDR_TOUCH_CS |= (1 << PIN_TOUCH_CS);
	PORT_TOUCH_CS |= (1 << PIN_TOUCH_CS);

	DDR_NRF_CSN |= (1 << PIN_NRF_CSN);
	PORT_NRF_CSN |= (1 << PIN_NRF_CSN);

	DDR_NRF_CE |= (1 << PIN_NRF_CE);
	PORT_NRF_CE |= (1 << PIN_NRF_CE);

	DDR_SD_CS |= (1 << PIN_SD_CS);
	PORT_SD_CS |= (1 << PIN_SD_CS);

    PORTE_DIR |= PIN5_bm | PIN7_bm;

    SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80;
	if(speed == 8) SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc;
	if(speed == 4) SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV16_gc | 0x80;
	if(speed == 2) SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV16_gc;
    //SPIE.INTCTRL = SPI_INTLVL_LO_gc; // Carefull this is enabling interrupts that fuck the system.
}

uint8_t SPI_send(uint8_t data)
{
	SPIE.DATA = data;

    while (!(SPIE.STATUS & SPI_IF_bm))
        ;

	return SPIE.DATA;
}

void Bluetooth_off()
{
	DDR_Bluetooth_PWR &= ~(1 << PIN_Bluetooth_PWR);
	//PORT_Bluetooth_PWR |= (1 << PIN_Bluetooth_PWR);
}

void Bluetooth_on()
{
	PORT_Bluetooth_PWR &= ~(1 << PIN_Bluetooth_PWR);
	DDR_Bluetooth_PWR |= (1 << PIN_Bluetooth_PWR);
}

// Timer0 initialization function
void ll_timer0_init(uint16_t value_a, uint16_t value_b, uint8_t status, uint8_t prescaler) {
    // Set timer clock source (assuming using system clock)
    if(status)
	{

		//TCC0.INTFLAGS = TC0_CCAIF_bm | TC0_CCBIF_bm;
		//TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
		switch(prescaler)
		{
			case 1:
				TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
				break;
			case 2:
				TCC0.CTRLA = TC_CLKSEL_DIV2_gc;
				break;
			case 3:
				TCC0.CTRLA = TC_CLKSEL_DIV4_gc;
				break;
			case 4:
				TCC0.CTRLA = TC_CLKSEL_DIV8_gc;
				break;
			case 5:
				TCC0.CTRLA = TC_CLKSEL_DIV64_gc;
				break;
			case 6:
				TCC0.CTRLA = TC_CLKSEL_DIV256_gc;
				break;
			case 7:
				TCC0.CTRLA = TC_CLKSEL_DIV1024_gc;
				break;
		}

		// Enable Compare/Capture channels A and B
		TCC0.CTRLB |= TC0_CCAEN_bm | TC0_CCBEN_bm;

		// Set Waveform Generation Mode to Normal
		TCC0.CTRLB |= TC_WGMODE_NORMAL_gc;

		// Enable Compare Match A and B interrupts
		TCC0.INTCTRLB |= TC_CCAINTLVL_LO_gc | TC_CCBINTLVL_LO_gc;

		// Set period (TOP value)
		TCC0.PER = 0xFFFF;  // Maximum period

		TCC0.CCA = value_a;
		TCC0.CCB = value_b;

		// Enable low-level interrupts globally
		PMIC.CTRL |= PMIC_LOLVLEN_bm;
		//sei();  // Enable global interrupts
		TCC0.INTFLAGS = 0xff;
		sei();
	}
	if(!status)
	{
		TCC0.CTRLA = 0;
		TCC0.INTCTRLB &= ~(TC_CCAINTLVL_LO_gc | TC_CCBINTLVL_LO_gc);
	}
}

// Compare Match A Interrupt Service Routine
ISR(TCC0_CCA_vect) {
    if(ll_timer0_pwm) BSP(ll_timer0_pwm_port, ll_timer0_pwm_pin);
	//lcd_write_debug("Interrupt 1");
	return;
		//PORTA.OUTSET = PIN0_bm;  // Set PORTA pin 0 high
}

// Compare Match B Interrupt Service Routine
ISR(TCC0_CCB_vect) {
	if(ll_timer0_pwm) BCP(ll_timer0_pwm_port, ll_timer0_pwm_pin);
	//lcd_write_debug("Interrupt 2");
	TCC0.CNT = 0;
	return;
    //PORTA.OUTCLR = PIN0_bm;  // Set PORTA pin 0 low
}

void BSP(uint8_t port, uint8_t pin)
{
	switch(port)
	{
		case 1:
			PORTA_OUT |= (1 << pin);
			break;
		case 2:
			PORTB_OUT |= (1 << pin);
			break;
		case 3:
			PORTC_OUT |= (1 << pin);
			break;
		case 4:
			PORTD_OUT |= (1 << pin);
			break;
		case 5:
			PORTE_OUT |= (1 << pin);
			break;
		case 6:
			PORTF_OUT |= (1 << pin);
			break;
	}
}

void BCP(uint8_t port, uint8_t pin)
{
	switch(port)
	{
		case 1:
			PORTA_OUT &= ~(1 << pin);
			break;
		case 2:
			PORTB_OUT &= ~(1 << pin);
			break;
		case 3:
			PORTC_OUT &= ~(1 << pin);
			break;
		case 4:
			PORTD_OUT &= ~(1 << pin);
			break;
		case 5:
			PORTE_OUT &= ~(1 << pin);
			break;
		case 6:
			PORTF_OUT &= ~(1 << pin);
			break;
	}
}

void BSD(uint8_t port, uint8_t pin)
{
	switch(port)
	{
		case 1:
			PORTA_DIR |= (1 << pin);
			break;
		case 2:
			PORTB_DIR |= (1 << pin);
			break;
		case 3:
			PORTC_DIR |= (1 << pin);
			break;
		case 4:
			PORTD_DIR |= (1 << pin);
			break;
		case 5:
			PORTE_DIR |= (1 << pin);
			break;
		case 6:
			PORTF_DIR |= (1 << pin);
			break;
	}
}

void BCD(uint8_t port, uint8_t pin)
{
	switch(port)
	{
		case 1:
			PORTA_DIR &= ~(1 << pin);
			break;
		case 2:
			PORTB_DIR &= ~(1 << pin);
			break;
		case 3:
			PORTC_DIR &= ~(1 << pin);
			break;
		case 4:
			PORTD_DIR &= ~(1 << pin);
			break;
		case 5:
			PORTE_DIR &= ~(1 << pin);
			break;
		case 6:
			PORTF_DIR &= ~(1 << pin);
			break;
	}
}

uint8_t get_PIN(uint8_t port)
{
	switch(port)
	{
		case 1:
			return PORTA_IN;
			break;
		case 2:
			return PORTB_IN;
			break;
		case 3:
			return PORTC_IN;
			break;
		case 4:
			return PORTD_IN;
			break;
		case 5:
			return PORTE_IN;
			break;
		case 6:
			return PORTF_IN;
			break;
	}
}

uint8_t get_PORT(uint8_t port)
{
	switch(port)
	{
		case 1:
			return PORTA_OUT;
			break;
		case 2:
			return PORTB_OUT;
			break;
		case 3:
			return PORTC_OUT;
			break;
		case 4:
			return PORTD_OUT;
			break;
		case 5:
			return PORTE_OUT;
			break;
		case 6:
			return PORTF_OUT;
			break;
	}
}

uint8_t get_DIR(uint8_t port)
{
	switch(port)
	{
		case 1:
			return PORTA_DIR;
			break;
		case 2:
			return PORTB_DIR;
			break;
		case 3:
			return PORTC_DIR;
			break;
		case 4:
			return PORTD_DIR;
			break;
		case 5:
			return PORTE_DIR;
			break;
		case 6:
			return PORTF_DIR;
			break;
	}
}

void set_PORT(uint8_t port, uint8_t data)
{
	switch(port)
	{
		case 1:
			PORTA_OUT = data;
			break;
		case 2:
			PORTB_OUT = data;
			break;
		case 3:
			PORTC_OUT = data;
			break;
		case 4:
			PORTD_OUT = data;
			break;
		case 5:
			PORTE_OUT = data;
			break;
		case 6:
			PORTF_OUT = data;
			break;
	}
}

void set_DIR(uint8_t port, uint8_t data)
{
	switch(port)
	{
		case 1:
			PORTA_DIR = data;
			break;
		case 2:
			PORTB_DIR = data;
			break;
		case 3:
			PORTC_DIR = data;
			break;
		case 4:
			PORTD_DIR = data;
			break;
		case 5:
			PORTE_DIR = data;
			break;
		case 6:
			PORTF_DIR = data;
			break;
	}
}

uint8_t ll_get_port(uint8_t port)
{
	if(port > 6 || port < 1) return 0xee;
	return get_PORT(port);
}

uint8_t ll_get_ddr(uint8_t port)
{
	if(port > 6 || port < 1) return 0xee;
	return get_DIR(port);
}

uint8_t ll_get_pin(uint8_t port)
{
	if(port > 6 || port < 1) return 0xee;
	return get_PIN(port);
}

void ll_set_port(uint8_t port, uint8_t data)
{
	if(port > 6 || port < 1) return;
	set_PORT(port, data);
}

void ll_set_ddr(uint8_t port, uint8_t data)
{
	if(port > 6 || port < 1) return;
	set_DIR(port, data);
}
