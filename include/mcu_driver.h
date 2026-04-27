


#define SPI_DATA SPIE.DATA
#define SPI_STATUS SPIE.STATUS
#define SPI_IF (SPIE.STATUS & SPI_IF_bm)
#define SPI_MAX_SET SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80
#define SPI_LCD_SET SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80
#define SPI_TOUCH_SET SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV16_gc | 0x80
#define SPI_SD_SET SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80
#define SPI_LOW_SET SPIE.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV128_gc

#define RAM_SPI_DATA SPIC.DATA
#define RAM_SPI_STATUS SPIC.STATUS
#define RAM_SPI_IF (SPIC.STATUS & SPI_IF_bm)
#define RAM_SPI_MAX_SET SPIC.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc | 0x80

// Structure to hold date/time components
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} datetime_components_t;

extern uint16_t system_date;
extern uint16_t system_time;

void mcu_set_32mhz(void);
uint8_t mcu_memory_read(uint16_t addr);
void mcu_memory_store(uint16_t addr, uint8_t value);
void rtc_time_date_to_components(uint16_t time, uint16_t date, datetime_components_t* components);
void components_to_rtc_time_date(datetime_components_t* components, uint16_t* time, uint16_t* date);