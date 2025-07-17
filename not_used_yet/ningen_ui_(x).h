

uint16_t color_back;
uint16_t color_border;
uint16_t color_border_selected;
uint16_t color_border_disabled;
uint16_t color_text;
uint16_t color_text_selected;
uint16_t color_text_disabled;
uint16_t color_form;
uint16_t color_cmd;
uint16_t color_cmd_disabled;
uint16_t color_cmd_selected;


#define	HEX8 0
#define	HEX16 1
#define	HEX32 2
#define BIN 3
#define FLOAT 4
#define BOOL 5
#define DEC8 6
#define DEC16 7

typedef enum {
	NoControlBox = 1,
	TitleOnly = 2,
	TitleClose = 3
} CtrlBoxType;

#define MATRIX_SIZE 128

#define SIZE1 1
#define SIZE2 2
#define SIZE3 3
#define SIZE4 4

struct ui_cmd {
		uint16_t pos_x;
		uint16_t pos_y;
		uint16_t size_x;
		uint16_t size_y;
		uint8_t caption[16];
};

typedef struct {
    uint16_t position_x;
    uint16_t position_y;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t back_color;
    uint16_t text_color;
    uint16_t border_color;
    uint16_t sel_color;
    uint16_t sel_txt_color;
    uint8_t text_size;
    uint8_t thickness;
    uint8_t element_count;        // Number of elements
    uint8_t *elements;            // Pointer to string containing all elements separated by '\n'
    uint8_t *element_type;        // Array of element types (size element_count)
    uint32_t *element_value;      // Array of element values (size element_count)

} value_list;

/*uint8_t matrix_status[MATRIX_SIZE];
uint16_t matrix_x0[MATRIX_SIZE];
uint16_t matrix_x1[MATRIX_SIZE];
uint16_t matrix_y0[MATRIX_SIZE];
uint16_t matrix_y1[MATRIX_SIZE];*/

//uint8_t ui_check_touch() ;
//uint8_t ui_wait_touch() ;

//void ui_write_dynamic(uint16_t pos_x,uint16_t pos_y, uint8_t * text, uint16_t color, uint8_t size);
void ui_write(uint16_t pos_x,uint16_t pos_y, uint8_t * text, uint16_t color, uint8_t size);
void ui_char(uint16_t pos_x,uint16_t pos_y, uint8_t character, uint16_t color, uint8_t size);

void ui_command(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text,  uint8_t thickness, uint8_t type);
void ui_check_box(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type);
void ui_check_box_update(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint16_t sel_color, uint8_t thickness, uint8_t status);
void ui_text_box(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type);
void ui_text_box_update(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint8_t *caption_old, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type, uint8_t max_length);
void ui_check_box_fill(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint16_t sel_color, uint8_t thickness);
value_list *ui_value_list(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *contents, uint16_t border, uint16_t back, uint16_t text, uint16_t color_selected, uint16_t color_sel_txt, uint8_t thickness, uint8_t text_type);
uint8_t ui_value_list_add(value_list *list_data, uint8_t *new_element, uint8_t type, uint32_t value);
uint8_t ui_value_list_remove_element(value_list *list_data, uint8_t index);
void ui_value_list_clear(value_list *list_data);
void ui_value_list_update(value_list *list_data, uint8_t selected, uint8_t show_index);
void ui_form(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *title, uint16_t border, uint16_t back, uint16_t text, uint16_t title_col, uint8_t thickness, uint8_t text_type, CtrlBoxType ctrl_type);
uint8_t ui_message_yes_no(uint8_t * message) ;
void ui_message_close(uint8_t *message) ;
void ui_numpad(uint8_t *caption, uint8_t *data_n, uint8_t *data_s, uint8_t type) ;
void ui_byte_pad(uint8_t *caption, uint8_t *data, uint8_t mask) ;
void ui_byte_pad_8(uint8_t *title, uint8_t *byte_name, uint8_t *data, uint8_t mask) ;
uint8_t ui_keypad(uint8_t * data, uint8_t max_length, uint8_t type, uint8_t cmd_size, uint8_t text_box_on, uint8_t toggle_nl) ;
uint8_t ui_get_key(uint8_t cmd_size, uint8_t wait, uint8_t redraw) ;


