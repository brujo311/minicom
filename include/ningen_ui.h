
uint16_t color_form_back;
uint16_t color_border;
uint16_t color_border_selected;
uint16_t color_border_disabled;
uint16_t color_text;
uint16_t color_text_selected;
uint16_t color_text_disabled;
uint16_t color_form;
uint16_t color_cmd_back;
uint16_t color_cmd_disabled;
uint16_t color_cmd_selected;

uint8_t *ui_control_status;

// Command button structure definition
typedef struct {
    uint16_t pos_x;
    uint16_t pos_y;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t back_color;
    uint16_t text_color;
    uint16_t border_color;
    uint16_t back_color_sel;
    uint16_t text_color_sel;
    uint16_t border_color_sel;
    uint16_t back_color_dis;
    uint16_t text_color_dis;
    uint16_t border_color_dis;
	uint8_t ctrl_type;
    uint8_t text_size;
    uint8_t thickness;
    const uint8_t *text;
    //void (*callback)(void);  // Function pointer for button press handler
} ui_static_control;

// Modified form structure to include command buttons
typedef struct {
    uint16_t pos_x;
    uint16_t pos_y;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t back_color;
    uint16_t text_color;
    uint16_t border_color;
    uint8_t text_size;
    uint8_t thickness;
    uint8_t ctrl_type;
    const uint8_t *title;
    uint8_t num_controls;           // Number of buttons in this form
    const ui_static_control *controls; // Pointer to array of buttons
} ui_static_form;

typedef enum {
	NoControlBox = 1,
	TitleOnly = 2,
	TitleClose = 3,
    TitleShowRam = 0x80
} CtrlBoxType;

typedef enum {
	CtrlSelected = 1,
	CtrlDisabled = 2,
	CtrlNormal = 0
} ControlStatus;

typedef enum {
	CmdButton = 1,
	Label = 2,
	TextBox = 3,
	CheckBox = 4,
	StaticListBox = 5,
	UseSystemColors = 0x80
} ControlType;

uint16_t mem_check();

void ui_char(uint16_t pos_x, uint16_t pos_y, uint8_t character, uint16_t color, uint8_t size);
void ui_write(uint16_t pos_x, uint16_t pos_y, const uint8_t * text, uint16_t color, uint8_t size);
void ui_draw_control(uint8_t form_index, uint8_t control_index);
void ui_draw_list_box(uint8_t form_index, uint8_t control_index, const uint8_t *dynamic_content);
void ui_text_update(uint8_t form_index, uint8_t control_index, const uint8_t *new_text);
void ui_draw_form(uint8_t form_index);
void ui_form_unload();

uint8_t ui_handle_touch();

