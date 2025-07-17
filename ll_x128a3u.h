 

#define EEPROM_PAGESIZE 32



void EEPROM_WriteByte(uint16_t address, uint8_t value); //( uint8_t pageAddr, uint8_t byteAddr, uint8_t value );
uint8_t EEPROM_ReadByte(uint16_t address); //( uint8_t pageAddr, uint8_t byteAddr );
void EEPROM_WaitForNVM( void );
void EEPROM_FlushBuffer( void );
void EEPROM_LoadByte( uint8_t byteAddr, uint8_t value );
void EEPROM_LoadPage( const uint8_t * values );
void EEPROM_AtomicWritePage( uint8_t pageAddr );
void EEPROM_ErasePage( uint8_t pageAddr );
void EEPROM_SplitWritePage( uint8_t pageAddr );
void EEPROM_EraseAll( void );

#define PORT_LCD_RST PORTE_OUT
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
#define PIN_TOUCH_IRQ 0 //PIN1_bm

#define PORT_SD_CS PORTF_OUT
#define DDR_SD_CS PORTF_DIR
#define PIN_SD_CS 1 //PIN1_bm

#define PORT_NRF_CE PORTF_OUT
#define DDR_NRF_CE PORTF_DIR
#define PIN_NRF_CE 5 //PIN5_bm

#define PORT_NRF_CSN PORTF_OUT
#define DDR_NRF_CSN PORTF_DIR
#define PIN_NRF_CSN 0 //PIN0_bm

#define PORT_NRF_IRQ PORTF_IN
#define DDR_NRF_IRQ PORTF_DIR
#define PIN_NRF_IRQ 4 //PIN4_bm

#define PORT_Bluetooth_PWR PORTD_OUT
#define DDR_Bluetooth_PWR PORTD_DIR
#define PIN_Bluetooth_PWR 5 //PIN5_bm

uint8_t ll_timer0_pwm;
uint8_t ll_timer0_pwm_port;
uint8_t ll_timer0_pwm_pin;

uint8_t ll_get_port(uint8_t port);
uint8_t ll_get_ddr(uint8_t port);
uint8_t ll_get_pin(uint8_t port);
void ll_set_port(uint8_t port, uint8_t data);
void ll_set_ddr(uint8_t port, uint8_t data);

void SPI_set(uint8_t speed);
uint8_t SPI_send(uint8_t data);

void Bluetooth_off();
void Bluetooth_on();

void ll_timer0_init(uint16_t value_a, uint16_t value_b, uint8_t status, uint8_t prescaler);

void BSP(uint8_t port, uint8_t pin);
void BCP(uint8_t port, uint8_t pin);
void BSD(uint8_t port, uint8_t pin);
void BCD(uint8_t port, uint8_t pin);
uint8_t get_PIN(uint8_t port);
void set_PORT(uint8_t port, uint8_t data);
void set_DIR(uint8_t port, uint8_t data);


