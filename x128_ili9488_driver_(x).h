 
#ifndef LCD_DRIVER_H_
#define LCD_DRIVER_H_

//#define PORT_SPI PORTB
//#define DDR_SPI DDRB
//#define PIN_SPI_SCK 5
//#define PIN_SPI_MISO 4
//#define PIN_SPI_MOSI 3
//#define PIN_SPI_SS 2

/*#define PORT_LCD_BCK PORTD
#define DDR_LCD_BCK DDRD
#define PIN_LCD_BCK 5
#define PORT_LCD_CS PORTD
#define DDR_LCD_CS DDRD
#define PIN_LCD_CS 6
#define PORT_LCD_RST PORTD
#define DDR_LCD_RST DDRD
#define PIN_LCD_RST 7
#define PORT_LCD_DC PORTB
#define DDR_LCD_DC DDRB
#define PIN_LCD_DC 0

#define PORT_TOUCH_IRQ PIND // INPUT
#define DDR_TOUCH_IRQ DDRD
#define PIN_TOUCH_IRQ 2
#define PORT_TOUCH_CS PORTC
#define DDR_TOUCH_CS DDRC
#define PIN_TOUCH_CS 4*/

// Define LCD control pins
/*#define PORT_LCD_RST PORTE_OUT
#define DDR_LCD_RST PORTE_DIR
#define PIN_LCD_RST 2 //PIN3_bm

#define PORT_LCD_CS PORTE_OUT
#define DDR_LCD_CS PORTE_DIR
#define PIN_LCD_CS 3 //PIN4_bm

#define PORT_LCD_DC PORTE_OUT
#define DDR_LCD_DC PORTE_DIR
#define PIN_LCD_DC 1 //PIN2_bm

#define PORT_TOUCH_CS PORTE_OUT
#define DDR_TOUCH_CS PORTE_DIR
#define PIN_TOUCH_CS 4 //PIN0_bm

#define PORT_TOUCH_IRQ PORTE_IN
#define DDR_TOUCH_IRQ PORTE_DIR
#define PIN_TOUCH_IRQ 0 //PIN1_bm*/

#define X1 1
#define X2 1
#define X3 1
#define X4 2

// LCD commands
#define DELAY   0x80

#define NOP     0x00
#define SWRESET 0x01
#define RDDID   0x04
#define RDDST   0x09

#define SLPIN   0x10
#define SLPOUT  0x11
#define PTLON   0x12
#define NORON   0x13

#define INVOFF  0x20
#define INVON   0x21
#define DISPOFF 0x28
#define DISPON  0x29
#define RAMRD   0x2E
#define CASET   0x2A
#define RASET   0x2B
#define RAMWR   0x2C

#define PTLAR   0x30
#define MADCTL  0x36
#define COLMOD  0x3A

#define FRMCTR1 0xB1
#define FRMCTR2 0xB2
#define FRMCTR3 0xB3
#define INVCTR  0xB4
#define DISSET5 0xB6

#define PWCTR1  0xC0
#define PWCTR2  0xC1
#define PWCTR3  0xC2
#define PWCTR4  0xC3
#define PWCTR5  0xC4
#define VMCTR1  0xC5

#define RDID1   0xDA
#define RDID2   0xDB
#define RDID3   0xDC
#define RDID4   0xDD

#define GMCTRP1 0xE0
#define GMCTRN1 0xE1

#define PWCTR6  0xFC

#define YELLOW ~0xf800
#define MAGENTA ~0x0780
#define CYAN ~0x007f
#define WHITE ~0x0000
#define VIOLET 0b1110110000011101
#define ORCHID 0b1101101110011011
#define BLACK ~0xffff
#define RED ~0xff80
#define BLUE ~0x07ff
#define GREEN ~0xf87f

#define GRAY1 0b0010000100000100
#define GRAY2 0b0100001000001000
#define GRAY3 0b0110001100001100
#define GRAY4 0b1000010000010000
#define GRAY5 0b1010010100010100
#define GRAY6 0b1100011000011000 // 0xc618
#define GRAY7 0b1110011100011100
#define BBLUE1 0b0010010000010000
#define BBLUE2 0b0100010100010100
#define BBLUE3 0b1010011000011000
#define BBLUE4 0b1100011000011100

#define HALF_B 0b0111100000000000
#define HALF_G 0b0000001111100000
#define HALF_R 0b0000000000001111

#define DELAY   0x80

#define NOP     0x00
#define SWRESET 0x01
#define RDDID   0x04
#define RDDST   0x09

#define SLPIN   0x10
#define SLPOUT  0x11
#define PTLON   0x12
#define NORON   0x13

#define INVOFF  0x20
#define INVON   0x21
#define DISPOFF 0x28
#define DISPON  0x29
#define RAMRD   0x2E
#define CASET   0x2A
#define RASET   0x2B
#define RAMWR   0x2C

#define PTLAR   0x30
#define MADCTL  0x36
#define COLMOD  0x3A

#define FRMCTR1 0xB1
#define FRMCTR2 0xB2
#define FRMCTR3 0xB3
#define INVCTR  0xB4
#define DISSET5 0xB6

#define PWCTR1  0xC0
#define PWCTR2  0xC1
#define PWCTR3  0xC2
#define PWCTR4  0xC3
#define PWCTR5  0xC4
#define VMCTR1  0xC5

#define RDID1   0xDA
#define RDID2   0xDB
#define RDID3   0xDC
#define RDID4   0xDD

#define GMCTRP1 0xE0
#define GMCTRN1 0xE1

#define PWCTR6  0xFC

 /* Level 1 Commands (from the display Datasheet) */
#define ILI9488_CMD_NOP                             0x00
#define ILI9488_CMD_SOFTWARE_RESET                  0x01
#define ILI9488_CMD_READ_DISP_ID                    0x04
#define ILI9488_CMD_READ_ERROR_DSI                  0x05
#define ILI9488_CMD_READ_DISP_STATUS                0x09
#define ILI9488_CMD_READ_DISP_POWER_MODE            0x0A
#define ILI9488_CMD_READ_DISP_MADCTRL               0x0B
#define ILI9488_CMD_READ_DISP_PIXEL_FORMAT          0x0C
#define ILI9488_CMD_READ_DISP_IMAGE_MODE            0x0D
#define ILI9488_CMD_READ_DISP_SIGNAL_MODE           0x0E
#define ILI9488_CMD_READ_DISP_SELF_DIAGNOSTIC       0x0F
#define ILI9488_CMD_ENTER_SLEEP_MODE                0x10
#define ILI9488_CMD_SLEEP_OUT                       0x11
#define ILI9488_CMD_PARTIAL_MODE_ON                 0x12
#define ILI9488_CMD_NORMAL_DISP_MODE_ON             0x13
#define ILI9488_CMD_DISP_INVERSION_OFF              0x20
#define ILI9488_CMD_DISP_INVERSION_ON               0x21
#define ILI9488_CMD_PIXEL_OFF                       0x22
#define ILI9488_CMD_PIXEL_ON                        0x23
#define ILI9488_CMD_DISPLAY_OFF                     0x28
#define ILI9488_CMD_DISPLAY_ON                      0x29
#define ILI9488_CMD_COLUMN_ADDRESS_SET              0x2A
#define ILI9488_CMD_PAGE_ADDRESS_SET                0x2B
#define ILI9488_CMD_MEMORY_WRITE                    0x2C
#define ILI9488_CMD_MEMORY_READ                     0x2E
#define ILI9488_CMD_PARTIAL_AREA                    0x30
#define ILI9488_CMD_VERT_SCROLL_DEFINITION          0x33
#define ILI9488_CMD_TEARING_EFFECT_LINE_OFF         0x34
#define ILI9488_CMD_TEARING_EFFECT_LINE_ON          0x35
#define ILI9488_CMD_MEMORY_ACCESS_CONTROL           0x36
#define ILI9488_CMD_VERT_SCROLL_START_ADDRESS       0x37
#define ILI9488_CMD_IDLE_MODE_OFF                   0x38
#define ILI9488_CMD_IDLE_MODE_ON                    0x39
#define ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET         0x3A
#define ILI9488_CMD_WRITE_MEMORY_CONTINUE           0x3C
#define ILI9488_CMD_READ_MEMORY_CONTINUE            0x3E
#define ILI9488_CMD_SET_TEAR_SCANLINE               0x44
#define ILI9488_CMD_GET_SCANLINE                    0x45
#define ILI9488_CMD_WRITE_DISPLAY_BRIGHTNESS        0x51
#define ILI9488_CMD_READ_DISPLAY_BRIGHTNESS         0x52
#define ILI9488_CMD_WRITE_CTRL_DISPLAY              0x53
#define ILI9488_CMD_READ_CTRL_DISPLAY               0x54
#define ILI9488_CMD_WRITE_CONTENT_ADAPT_BRIGHTNESS  0x55
#define ILI9488_CMD_READ_CONTENT_ADAPT_BRIGHTNESS   0x56
#define ILI9488_CMD_WRITE_MIN_CAB_LEVEL             0x5E
#define ILI9488_CMD_READ_MIN_CAB_LEVEL              0x5F
#define ILI9488_CMD_READ_ABC_SELF_DIAG_RES          0x68
#define ILI9488_CMD_READ_ID1                        0xDA
#define ILI9488_CMD_READ_ID2                        0xDB
#define ILI9488_CMD_READ_ID3                        0xDC

/* Level 2 Commands (from the display Datasheet) */
#define ILI9488_CMD_INTERFACE_MODE_CONTROL          0xB0
#define ILI9488_CMD_FRAME_RATE_CONTROL_NORMAL       0xB1
#define ILI9488_CMD_FRAME_RATE_CONTROL_IDLE_8COLOR  0xB2
#define ILI9488_CMD_FRAME_RATE_CONTROL_PARTIAL      0xB3
#define ILI9488_CMD_DISPLAY_INVERSION_CONTROL       0xB4
#define ILI9488_CMD_BLANKING_PORCH_CONTROL          0xB5
#define ILI9488_CMD_DISPLAY_FUNCTION_CONTROL        0xB6
#define ILI9488_CMD_ENTRY_MODE_SET                  0xB7
#define ILI9488_CMD_BACKLIGHT_CONTROL_1             0xB9
#define ILI9488_CMD_BACKLIGHT_CONTROL_2             0xBA
#define ILI9488_CMD_HS_LANES_CONTROL                0xBE
#define ILI9488_CMD_POWER_CONTROL_1                 0xC0
#define ILI9488_CMD_POWER_CONTROL_2                 0xC1
#define ILI9488_CMD_POWER_CONTROL_NORMAL_3          0xC2
#define ILI9488_CMD_POWER_CONTROL_IDEL_4            0xC3
#define ILI9488_CMD_POWER_CONTROL_PARTIAL_5         0xC4
#define ILI9488_CMD_VCOM_CONTROL_1                  0xC5
#define ILI9488_CMD_CABC_CONTROL_1                  0xC6
#define ILI9488_CMD_CABC_CONTROL_2                  0xC8
#define ILI9488_CMD_CABC_CONTROL_3                  0xC9
#define ILI9488_CMD_CABC_CONTROL_4                  0xCA
#define ILI9488_CMD_CABC_CONTROL_5                  0xCB
#define ILI9488_CMD_CABC_CONTROL_6                  0xCC
#define ILI9488_CMD_CABC_CONTROL_7                  0xCD
#define ILI9488_CMD_CABC_CONTROL_8                  0xCE
#define ILI9488_CMD_CABC_CONTROL_9                  0xCF
#define ILI9488_CMD_NVMEM_WRITE                     0xD0
#define ILI9488_CMD_NVMEM_PROTECTION_KEY            0xD1
#define ILI9488_CMD_NVMEM_STATUS_READ               0xD2
#define ILI9488_CMD_READ_ID4                        0xD3
#define ILI9488_CMD_ADJUST_CONTROL_1                0xD7
#define ILI9488_CMD_READ_ID_VERSION                 0xD8
#define ILI9488_CMD_POSITIVE_GAMMA_CORRECTION       0xE0
#define ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION       0xE1
#define ILI9488_CMD_DIGITAL_GAMMA_CONTROL_1         0xE2
#define ILI9488_CMD_DIGITAL_GAMMA_CONTROL_2         0xE3
#define ILI9488_CMD_SET_IMAGE_FUNCTION              0xE9
#define ILI9488_CMD_ADJUST_CONTROL_2                0xF2
#define ILI9488_CMD_ADJUST_CONTROL_3                0xF7
#define ILI9488_CMD_ADJUST_CONTROL_4                0xF8
#define ILI9488_CMD_ADJUST_CONTROL_5                0xF9
#define ILI9488_CMD_SPI_READ_SETTINGS               0xFB
#define ILI9488_CMD_ADJUST_CONTROL_6                0xFC
#define ILI9488_CMD_ADJUST_CONTROL_7                0xFF

// MV = 0 in MADCTL
// max columns
#define MAX_X 480
// max rows
#define MAX_Y 320
// columns max counter
#define SIZE_X MAX_X - 1
// rows max counter
#define SIZE_Y MAX_Y - 1
// whole pixels
#define CACHE_SIZE_MEM 153600
// number of columns for chars
#define CHARS_COLS_LEN 5
// number of rows for chars
#define CHARS_ROWS_LEN 8

/** @const Command list ILI9341 */
extern const uint8_t INIT_ILI9341[];

/** @const Characters */


/** @enum Font sizes */
/*typedef enum {
	// normal font size: 1x high & 1x wide
	X1 = 0x00,
	// bigger font size: 2x higher & 1x wide
	X2 = 0x01,
	// the biggest font size: font 2x higher & 2x wider
	// 0x0A is set cause offset 5 position to right only for
	//      this case and no offset for previous cases X1, X2
	//      when draw the characters of string in DrawString()
	X3 = 0x0A,
	X4 = 0x0b
} ESizes;*/


uint8_t spi_send(uint8_t data);
void lcd_spi_set(void);
void lcd_spi_touch_set(void);
void lcd_reset(void);
void lcd_init_controller(void);
void lcd_send_init_commands(const uint8_t *);
void lcd_send_cmd(uint8_t);
void lcd_send_data(uint8_t *, uint16_t);
void lcd_send_color_565(uint16_t, uint32_t);
uint8_t lcd_set_partial_area(uint16_t, uint16_t);
uint8_t lcd_set_window(uint16_t, uint16_t, uint16_t, uint16_t);
uint8_t lcd_set_position(uint16_t, uint16_t);
void lcd_draw_window(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
void lcd_draw_pixel(uint16_t, uint16_t, uint16_t);
void lcd_draw_char(uint8_t, uint16_t, uint8_t);
void lcd_get_char_size(uint8_t chr, uint8_t type, uint8_t *size_x, uint8_t *size_y);
void lcd_print(const uint8_t *, uint16_t, uint8_t);
void lcd_print_number(uint32_t, uint8_t, uint16_t, uint8_t);
uint8_t lcd_draw_line(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
void lcd_draw_line_x(uint16_t, uint16_t, uint16_t, uint16_t);
void lcd_draw_line_y(uint16_t, uint16_t, uint16_t, uint16_t);
void lcd_clear_screen(uint16_t);
void lcd_on(void);
void lcd_off(void);
void lcd_print_HEX8(uint8_t, uint16_t, uint8_t, uint8_t);
void lcd_print_HEX16(uint16_t, uint16_t, uint8_t, uint8_t);
void lcd_write_debug(uint8_t *);
void lcd_write_dynamic(uint8_t *text, uint16_t color, uint8_t size);
void lcd_draw_command(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, uint16_t line_col, uint16_t fill_col);
void lcd_gamma_correction();
void lcd_char(uint8_t ch, uint16_t color, uint8_t font) ;
void lcd_string(uint8_t * str, uint16_t color, uint8_t font) ;

#define XPT2046_MODE_S_TEMP0 0b00001
#define XPT2046_MODE_S_XPYM  0b00101
#define XPT2046_MODE_S_BATT  0b01001
#define XPT2046_MODE_S_XPZ1  0b01101
#define XPT2046_MODE_S_YNZ2  0b10001
#define XPT2046_MODE_S_YPXM  0b10101
#define XPT2046_MODE_S_AUX   0b11001
#define XPT2046_MODE_S_TEMP1 0b11101

#define XPT2046_MODE_D_XP    0b00100   // REF+ YP  REF- YN
#define XPT2046_MODE_D_XPYP  0b01100   // REF+ YP  REF- XN
#define XPT2046_MODE_D_YNYP  0b10000   // REF+ YP  REF- XN
#define XPT2046_MODE_D_YP    0b10100   // REF+ XP  REF- XN

#define XPT2046_PD_POWERDOWN 0b00
#define XPT2046_PD_REFOFF    0b01
#define XPT2046_PD_ADCOFF    0b10
#define XPT2046_PD_ON        0b11      // Interrupt disabled!

#define SAMPLES 200

uint16_t readback_x;
uint16_t readback_y;
int16_t cursor_calibration_X;
int16_t cursor_calibration_Y;
uint16_t cursor_x;
uint16_t cursor_y;
uint16_t cursor_x_old;
uint16_t cursor_y_old;

uint16_t touch_send_data(uint8_t);
void touch_refresh(void);
void touch_calibrate(void);
uint8_t touch_sense();
void touch_wait_press();
void touch_wait_release();

#endif /* LCD_DRIVER_H_ */
