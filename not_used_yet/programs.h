

/*typedef enum {
	HEX8 = 0,
	HEX16 = 1,
	HEX32 = 2,
    BIN = 3,
    FLOAT = 4,
    BOOL = 5,
	DEC8 = 6,
    DEC16 = 7
} DataType;*/

uint16_t mem_check();
void boot();

uint8_t store_setting(uint16_t start_address, uint8_t type, uint8_t *data, uint8_t *caption) ;
uint8_t read_setting(uint16_t start_address, uint8_t setting_number, uint8_t *readback) ;
uint8_t read_setting_caption(uint16_t start_address, uint8_t setting_number, uint8_t *readback) ;
uint8_t read_setting_data_type(uint16_t start_address, uint8_t setting_number, uint8_t *readback) ;
uint8_t write_setting(uint16_t start_address, uint8_t setting_number, uint8_t *data) ;
uint16_t get_setting_length(uint16_t start_address) ;
void display_console_buffer(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, uint8_t char_size_x, uint8_t char_size_y);

uint8_t gui_allocate_elements(uint8_t size);
void gui_free_elements();
void gui_load();
void gui_config();
void gui_status(uint8_t * status, uint16_t color);
void gui_form_main();
void gui_io_control();
void gui_port_operation(uint8_t port);
void gui_pwm_gen();
void gui_input_analyzer();

void console_allocate_buffer();
void console_free_buffer();

void console_write(uint8_t *text);
void console_update_input();
void console_update(uint8_t start_line, uint8_t redraw_keypad);
void console(uint8_t windowed, uint8_t keypad_on);
uint8_t console_make_prompt(uint8_t flag, uint8_t *dirname);

void console_BIN(uint8_t bin, uint8_t toggle_0b, uint8_t toggle_nl);
void console_HEX8(uint8_t hex8, uint8_t toggle_0x, uint8_t toggle_nl);
void console_number(uint32_t number, uint8_t base, uint8_t toggle_nl);

uint8_t macro_process(uint8_t *macro_data);
uint8_t compare_strings(uint8_t * str1, uint8_t * str2);
uint8_t compare_command(uint8_t * str1, uint8_t * str2);
uint8_t bin_to_int(uint8_t *binaryString);

void load_gui_settings();

void pgm_store(uint8_t **arguments);
void pgm_conf(uint8_t **arguments, uint8_t *filename);
void pgm_less(uint8_t **arguments);
void pgm_status();
void pgm_cls();
void pgm_paint();
void pgm_ls();
void pgm_cd(uint8_t *dirname);
void pgm_read_file(uint8_t *filename, uint8_t **arguments);
void pgm_bitmap(uint8_t *filename, uint8_t **arguments);
uint8_t pgm_pwm(uint8_t port, uint8_t bit, uint16_t freq, uint8_t duty, uint8_t status, uint8_t multiplier);

void system_error_hadle(uint8_t error);

uint8_t FAT_try();
