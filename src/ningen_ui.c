
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ningen_ui.h"
#include "lcd_driver.h"
#include "colors.h"

uint16_t color_form_back = BLACK;
uint16_t color_border = BLUE;
uint16_t color_border_selected = BLUE;
uint16_t color_border_disabled = GRAY1;
uint16_t color_text = WHITE;
uint16_t color_text_selected = WHITE;
uint16_t color_text_disabled = GRAY4;
uint16_t color_form = GRAY4;
uint16_t color_cmd_back = GRAY6;
uint16_t color_cmd_disabled = BLACK;
uint16_t color_cmd_selected = HALF_B;
uint16_t color_chk_selected = GREEN;
uint16_t color_chk_back = BLACK;
uint16_t color_list_selected = BLUE;
uint16_t color_list_back = BLACK;
uint16_t color_text_back = BLACK;

uint8_t list_control_count = 0;
uint8_t *list_start_index;
uint8_t text_box_count = 0;
uint8_t *ui_control_status;
uint8_t *ui_control_alias;
uint8_t ui_active_form = 0;
uint8_t **ui_list_contents;
uint8_t **ui_text_contents;


// Define buttons in PROGMEM
const ui_static_control form1_controls[] PROGMEM = {
    {
        .pos_x = 100, .pos_y = 40,
        .size_x = 80, .size_y = 30,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = RED,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 1,
        .thickness = 1,
        .ctrl_type = CmdButton,
        .text = "Special 1"
        //.callback = button1_callback
    },
    {
        .pos_x = 10, .pos_y = 40,
        .size_x = 80, .size_y = 30,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = RED,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 1,
        .thickness = 1,
        .ctrl_type = CmdButton | UseSystemColors,
        .text = "Command 1"
        //.callback = button1_callback
    },
    {
        .pos_x = 10, .pos_y = 80,
        .size_x = 0, .size_y = 0,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = RED,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 1,
        .thickness = 1,
        .ctrl_type = Label | UseSystemColors,
        .text = "Label 1"
        //.callback = button1_callback
    },
    {
        .pos_x = 190, .pos_y = 40,
        .size_x = 80, .size_y = 30,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = RED,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 1,
        .thickness = 1,
        .ctrl_type = TextBox | UseSystemColors,
        .text = "Text 1"
        //.callback = button1_callback
    },
    {
        .pos_x = 100, .pos_y = 80,
        .size_x = 20, .size_y = 20,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = RED,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 1,
        .thickness = 2,
        .ctrl_type = CheckBox | UseSystemColors,
        .text = "Check Box 1"
        //.callback = button1_callback
    },
    {
        .pos_x = 10, .pos_y = 120,
        .size_x = 150, .size_y = 100,
        .back_color = GRAY6,
        .text_color = WHITE,
        .border_color = GREEN,
        .back_color_sel = HALF_B,
        .text_color_sel = WHITE,
        .border_color_sel = BLUE,
        .back_color_dis = BLACK,
        .text_color_dis = WHITE,
        .border_color_dis = GRAY1,
        .text_size = 3,
        .thickness = 1,
        .ctrl_type = StaticListBox | UseSystemColors,
        //.text = "Item 1\nItem 2\nItem 3\nItem 4"
        .text = "Item 1\nElement 2\nMy dick\nChupame esta con ganas shijo de puta\nItem 5\nPoronga\nItem 7\nDe lunes a lunes\nLa que esta buena\nItem 10\nHayan\nItem 12"
        //.callback = button1_callback
    },
    // Add more buttons as needed
};

// Modified forms array with buttons
const ui_static_form forms[] PROGMEM = {
    {
        .pos_x = 10, .pos_y = 20,
        .size_x = 300, .size_y = 250,
        .back_color = GRAY4,
        .text_color = WHITE,
        .border_color = BLUE,
        .text_size = 3,
        .thickness = 2,
        .ctrl_type = TitleClose,
        .title = "Form 1",
        .num_controls = 6,
        .controls = form1_controls,
    },
    // Add more forms as needed
};

uint16_t mem_check()
{
	uint16_t free_mem = 1;
	uint8_t *test;
	test = (uint8_t *)malloc(free_mem);
	while(test)
	{
		test = (uint8_t *)malloc(free_mem);
		free(test);
		free_mem++;
	}
	return free_mem;
}

void ui_char(uint16_t pos_x, uint16_t pos_y, uint8_t character, uint16_t color, uint8_t size)
{
    lcd_set_position(pos_x, pos_y);
    lcd_draw_char(character, color, size);
}

void ui_write(uint16_t pos_x, uint16_t pos_y, const uint8_t * text, uint16_t color, uint8_t size)
{
    uint8_t c = 0, size_y = 16;
    lcd_set_position(pos_x, pos_y);
	while (text[c] != '\0')
    {
		lcd_draw_char(text[c++], color, size);
        if(text[c] == '\n')
        {
            lcd_get_char_size('A', size, NULL, &size_y);
            lcd_set_position(pos_x, pos_y + size_y);
            c++;
        }
	}
}

void ui_form_unload()
{
    if(ui_list_contents) {
        for(uint8_t i = 0; i < list_control_count; i++) {
            if(ui_list_contents[i]) {
                free(ui_list_contents[i]);
            }
        }
        free(ui_list_contents);
    }
    if(ui_text_contents) {
        for(uint8_t i = 0; i < text_box_count; i++) {
            if(ui_text_contents[i]) {
                free(ui_text_contents[i]);
            }
        }
        free(ui_text_contents);
    }
    list_control_count = 0;
    text_box_count = 0;
    if(ui_control_alias)
        free(ui_control_alias);
    if(ui_control_status)
        free(ui_control_status);
    if(list_start_index)
        free(list_start_index);
    ui_active_form = 0;
}

void ui_draw_form(uint8_t form_index)
{
    const ui_static_form *form = &forms[form_index];

    uint16_t x = pgm_read_word(&form->pos_x);
    uint16_t y = pgm_read_word(&form->pos_y);
    uint16_t width = pgm_read_word(&form->size_x);
    uint16_t height = pgm_read_word(&form->size_y);
    uint16_t back_color = pgm_read_word(&form->back_color);
    const uint8_t *title = (const uint8_t*)pgm_read_word(&form->title);
    uint8_t char_width, char_height;

    text_box_count = 0; list_control_count = 0;

    lcd_draw_window(x, x + width, y, y + height, back_color);

    for(uint8_t a = 0; a < pgm_read_byte(&form->thickness); a++)
    {
        lcd_draw_line_x(x, x + width, y + a, pgm_read_word(&form->border_color));
        lcd_draw_line_x(x, x + width, y + height - a, pgm_read_word(&form->border_color));
        lcd_draw_line_y(x + a, y, y + height, pgm_read_word(&form->border_color));
        lcd_draw_line_y(x + width - a, y, y + height, pgm_read_word(&form->border_color));
    }

    lcd_get_char_size('A', pgm_read_byte(&form->text_size), &char_width, &char_height);

    if(pgm_read_byte(&form->ctrl_type) != NoControlBox)
    {
        lcd_draw_window(x + pgm_read_byte(&form->thickness), x + width - pgm_read_byte(&form->thickness), y + pgm_read_byte(&form->thickness), y + pgm_read_byte(&form->thickness) + 9 + char_height, BLACK);
        ui_write(x + 5 + pgm_read_byte(&form->thickness),  y + 5 + pgm_read_byte(&form->thickness), title, pgm_read_byte(&form->text_color), pgm_read_byte(&form->text_size));

        for(uint8_t a = 0; a < pgm_read_byte(&form->thickness); a++)
            lcd_draw_line_x(x, x + width, y + pgm_read_byte(&form->thickness) + 9 + char_height + a, pgm_read_word(&form->border_color));
    }

    // Draw elements
    uint8_t num_controls = pgm_read_byte(&form->num_controls);

    // Allocate control memory
    if(ui_active_form != form_index + 1)
    {
        text_box_count = 0;
        list_control_count = 0;

        if(ui_control_status) { free(ui_control_status); }
        ui_control_status = (uint8_t *)malloc(num_controls * sizeof(uint8_t));
        if(!ui_control_status) return;

        if(ui_control_alias) { free(ui_control_alias); }
        ui_control_alias = (uint8_t *)malloc(num_controls * sizeof(uint8_t));
        if(!ui_control_alias) return;

        for(uint8_t a = 0; a < num_controls; a++)
        {
            ui_control_alias[a] = 0;
            ui_control_status[a] = CtrlNormal;

            const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
            const ui_static_control *static_ctrl = &ui_static_controls[a];
            uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type);

            if(ctrl_type == TextBox) { ui_control_alias[a] = text_box_count; text_box_count++; }
            if(ctrl_type == StaticListBox) { ui_control_alias[a] = list_control_count; list_control_count++; }
        }

        if(ui_text_contents) { free(ui_text_contents); }
        ui_text_contents = (uint8_t **)malloc(text_box_count * sizeof(uint8_t *));
        if(!ui_text_contents) return;

        for(uint8_t i = 0; i < text_box_count; i++) ui_text_contents[i] = NULL;

        if(ui_list_contents) { free(ui_list_contents); }
        ui_list_contents = (uint8_t **)malloc(list_control_count * sizeof(uint8_t *));
        if(!ui_list_contents) return;

        if(list_start_index) { free(list_start_index); }
        list_start_index = (uint8_t *)malloc(list_control_count * sizeof(uint8_t *));
        if(!list_start_index) return;

        for(uint8_t i = 0; i < list_control_count; i++) { ui_list_contents[i] = NULL; list_start_index[i] = 0; }
    }

    for(uint8_t i = 0; i < num_controls; i++)
    {
        if(pgm_read_byte(&form->ctrl_type) == TitleClose && i == 0) ui_draw_control(form_index, i);
        if(i > 0) ui_draw_control(form_index, i);
    }

    ui_active_form = form_index + 1;
}

void ui_draw_control(uint8_t form_index, uint8_t control_index)
{
    const ui_static_form *form = &forms[form_index];
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
    const ui_static_control *static_ctrl = &ui_static_controls[control_index];

    uint8_t a = 0;
    uint8_t char_width = 0, char_height = 0;
    uint16_t pos_x = pgm_read_word(&form->pos_x) + pgm_read_word(&static_ctrl->pos_x);
    uint16_t pos_y = pgm_read_word(&form->pos_y) + pgm_read_word(&static_ctrl->pos_y);
    uint16_t size_x = pgm_read_word(&static_ctrl->size_x);
    uint16_t size_y = pgm_read_word(&static_ctrl->size_y);
    uint16_t back_color;
    uint16_t text_color;
    uint16_t border_color;
    uint8_t text_size = pgm_read_byte(&static_ctrl->text_size);
    uint8_t thickness = pgm_read_byte(&static_ctrl->thickness);
    uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type);
    uint8_t use_system_colors = ctrl_type & UseSystemColors;
    ctrl_type &= ~(UseSystemColors);  // Clear the system colors flag to get actual control type
    const uint8_t *caption = (const uint8_t*)pgm_read_word(&static_ctrl->text);

    if(ui_control_status[control_index] == CtrlNormal)
    {
        if (use_system_colors) {
            back_color = (ctrl_type == CmdButton) ? color_cmd_back :
                        (ctrl_type == CheckBox) ? color_chk_back :
                        (ctrl_type == TextBox) ? color_text_back :
                        (ctrl_type == StaticListBox) ? color_list_back : color_form_back;
            text_color = color_text;
            border_color = color_border;
        } else {
            back_color = pgm_read_word(&static_ctrl->back_color);
            text_color = pgm_read_word(&static_ctrl->text_color);
            border_color = pgm_read_word(&static_ctrl->border_color);
        }
    }
    if(ui_control_status[control_index] == CtrlSelected)
    {
        if (use_system_colors) {
            back_color = (ctrl_type == CmdButton) ? color_cmd_selected :
                        (ctrl_type == CheckBox) ? color_chk_selected :
                        (ctrl_type == StaticListBox) ? color_list_selected : color_form_back;
            text_color = color_text_selected;
            border_color = color_border_selected;
        } else {
            back_color = pgm_read_word(&static_ctrl->back_color_sel);
            text_color = pgm_read_word(&static_ctrl->text_color_sel);
            border_color = pgm_read_word(&static_ctrl->border_color_sel);
        }
        if(ctrl_type == CmdButton) thickness++;
    }
    if(ui_control_status[control_index] == CtrlDisabled)
    {
        if (use_system_colors) {
            back_color = (ctrl_type == CmdButton) ? color_cmd_disabled : color_form_back;
            text_color = color_text_disabled;
            border_color = color_border_disabled;
        } else {
            back_color = pgm_read_word(&static_ctrl->back_color_dis);
            text_color = pgm_read_word(&static_ctrl->text_color_dis);
            border_color = pgm_read_word(&static_ctrl->border_color_dis);
        }
    }

    // Rest of the drawing code remains the same
    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back_color);
    if(ctrl_type != Label)
    {
        for(a = 0; a < thickness; a++)
        {
            lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border_color);
            lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border_color);
            lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border_color);
            lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border_color);
        }
    }

    a = 0;
    while(caption[a] != '\0') a++;
    lcd_get_char_size('A', text_size, &char_width, &char_height);
    if(ctrl_type == CmdButton) ui_write(pos_x + ((size_x - (a * char_width)) / 2) + 1,  pos_y + ((size_y - char_height) / 2) + 1, caption, text_color, text_size);
    if(ctrl_type == CheckBox) ui_write(pos_x + size_x + 5,  pos_y + ((size_y - char_height) / 2) + 1, caption, text_color, text_size);
    if(ctrl_type == Label) ui_write(pos_x,  pos_y, caption, text_color, text_size);

    if(ctrl_type == CheckBox)
    {
        lcd_draw_window(pos_x + thickness, pos_x + size_x - thickness, pos_y + thickness, pos_y + size_y - thickness, use_system_colors ? color_chk_back : back_color);
        if(ui_control_status[control_index] == CtrlSelected)
            lcd_draw_window(pos_x + thickness + 3, pos_x + size_x - thickness - 3,
                          pos_y + thickness + 3, pos_y + size_y - thickness - 3,
                          use_system_colors ? color_chk_selected : back_color);
    }

    if(ctrl_type == StaticListBox)
    {
        ui_draw_list_box(form_index, control_index, NULL);
    }

    if(ctrl_type == TextBox)
    {
        ui_text_update(form_index, control_index, NULL);
    }
}

void ui_draw_list_box(uint8_t form_index, uint8_t control_index, const uint8_t *dynamic_content)
{
    const ui_static_form *form = &forms[form_index];
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
    const ui_static_control *static_ctrl = &ui_static_controls[control_index];
    const uint8_t *content_to_use = NULL;
    uint8_t alias_index = ui_control_alias[control_index];

    // Determine which content source to use based on the four scenarios
    if (dynamic_content == NULL && ui_list_contents[alias_index] != NULL) {
        // Scenario 1: Use existing list content
        content_to_use = ui_list_contents[alias_index];
    }
    else if (dynamic_content != NULL && (ui_list_contents[alias_index] == NULL)) {
        // Scenario 2: Store dynamic content and use it
        uint16_t content_length = 0;
        while (dynamic_content[content_length] != '\0') content_length++;
        content_length++; // Account for null terminator

        ui_list_contents[alias_index] = (uint8_t*)malloc(content_length);
        if (ui_list_contents[alias_index] != NULL) {
            memcpy(ui_list_contents[alias_index], dynamic_content, content_length);
        }
        content_to_use = dynamic_content;
    }
    else if (dynamic_content != NULL && ui_list_contents[alias_index] != NULL) {
        // Scenario 3: Compare and update if different
        uint16_t old_len = 0, new_len = 0;
        uint8_t contents_different = 0;
        // Get lengths
        while (ui_list_contents[alias_index][old_len] != '\0') old_len++;
        while (dynamic_content[new_len] != '\0') new_len++;

        if (old_len != new_len) {
            contents_different = 1;
        } else {
            // Compare contents
            for (uint16_t i = 0; i <= new_len; i++) {
                if (ui_list_contents[alias_index][i] != dynamic_content[i]) {
                    contents_different = 1;
                    break;
                }
            }
        }

        if (contents_different) {
            // Free old content and allocate new
            free(ui_list_contents[alias_index]);
            ui_list_contents[alias_index] = (uint8_t*)malloc(new_len + 1);
            if (ui_list_contents[alias_index] != NULL) {
                memcpy(ui_list_contents[alias_index], dynamic_content, new_len + 1);
            }
        }
        content_to_use = dynamic_content;
    }
    else {
        // Scenario 4: Both NULL, load from PROGMEM
        content_to_use = (const uint8_t*)pgm_read_word(&static_ctrl->text);
        // Calculate length of PROGMEM content
        uint16_t prog_len = 0;
        while(content_to_use[prog_len++] != '\0') ;
        prog_len++;
        ui_list_contents[alias_index] = (uint8_t*)malloc(prog_len);
        if (ui_list_contents[alias_index] != NULL) {
            memcpy(ui_list_contents[alias_index], content_to_use, prog_len);
        }
    }

    // Continue with the existing drawing code, but use content_to_use instead of caption
    uint8_t a = 0, total_elements = 0;
    uint8_t char_width = 0, char_height = 0;
    uint16_t pos_x = pgm_read_word(&form->pos_x) + pgm_read_word(&static_ctrl->pos_x);
    uint16_t pos_y = pgm_read_word(&form->pos_y) + pgm_read_word(&static_ctrl->pos_y);
    uint16_t size_x = pgm_read_word(&static_ctrl->size_x);
    uint16_t size_y = pgm_read_word(&static_ctrl->size_y);
    uint16_t back_color, text_color, border_color, selected_color;
    uint8_t text_size = pgm_read_byte(&static_ctrl->text_size);
    uint8_t thickness = pgm_read_byte(&static_ctrl->thickness);

    // Get control type and system colors flag
    uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type);
    uint8_t use_system_colors = ctrl_type & UseSystemColors;
    ctrl_type &= ~(UseSystemColors);
    if(ctrl_type != StaticListBox) return;

    // First pass: count total string length needed
    uint16_t total_length = 0;
    const uint8_t *temp_caption = content_to_use;

    while (*temp_caption != '\0') {
        total_length++;
        temp_caption++;
    }
    total_length++;

    if (size_x < 100) size_x = 100;
    if (size_y < 80) size_y = 80;

    // Allocate buffer for drawing
    uint8_t *modified_text = (uint8_t*)malloc(total_length);
    if(modified_text == NULL)
    {
        // Handle allocation failure as before
        lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, color_list_back);
        for(a = 0; a < thickness; a++)
        {
            lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, color_border);
            lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, color_border);
            lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, color_border);
            lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, color_border);
        }
        return;
    }

    // Determine colors based on flag
    if (use_system_colors)
    {
        back_color = color_list_back;
        text_color = color_text;
        border_color = color_border;
        selected_color = color_list_selected;
    }
    else
    {
        back_color = pgm_read_word(&static_ctrl->back_color);
        text_color = pgm_read_word(&static_ctrl->text_color);
        border_color = pgm_read_word(&static_ctrl->border_color);
        selected_color = color_list_selected;  // Always use system color for selection highlight
    }

    uint16_t mod_idx = 0;
    // Copy from dynamic content
    while(content_to_use[a] != '\0')
    {
        if(content_to_use[a] == '\n')
        {
            modified_text[mod_idx++] = '\0';
            total_elements++;
        }
        else
        {
            modified_text[mod_idx++] = content_to_use[a];
        }
        a++;
    }

    modified_text[mod_idx++] = '\0';
    total_elements++;

    // Draw background and border
    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back_color);
    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border_color);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border_color);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border_color);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border_color);
    }

    // Calculate character size and max chars per line
    lcd_get_char_size('A', text_size, &char_width, &char_height);
    uint8_t max_chars = (size_x - 30 - 2 * thickness - char_width) / char_width;

    // Calculate how many items can fit in the visible area
    uint16_t item_height = char_height + 6;  // Height per item including spacing
    uint8_t visible_items = (size_y - 2 * thickness) / item_height;

    // Adjust list_start_index[alias_index] if it would result in empty space at the bottom
    if (list_start_index[alias_index] > total_elements - visible_items && total_elements > visible_items)
    {
        list_start_index[alias_index] = total_elements - visible_items;
    }

    // Calculate vertical spacing
    uint16_t vertical_padding = 3;  // Fixed padding for more consistent appearance
    uint16_t current_y = pos_y + thickness + vertical_padding;

    // Draw list elements
    a = 0;
    uint8_t current_element = 0;
    uint8_t displayed_items = 0;

    // Skip to list_start_index[alias_index]
    while(current_element < list_start_index[alias_index] && a < mod_idx)
    {
        while(modified_text[a] != '\0') a++;
        a++;  // Skip null terminator
        current_element++;
    }

    // Draw visible elements
    while(a < mod_idx && displayed_items < visible_items)
    {
        // Draw selected element with special background
        if(ui_control_status[control_index] > 0 && current_element + 1 == ui_control_status[control_index])
        {
            lcd_draw_window(pos_x + thickness, pos_x + size_x - 30 - thickness,
                          current_y - 2, current_y + char_height + 2, selected_color);
        }

        // Calculate text length and truncate if necessary
        uint16_t text_len = 0;
        while(modified_text[a + text_len] != '\0') text_len++;

        if(text_len > max_chars)
        {
            // Temporarily store the character at truncation point
            uint8_t temp = modified_text[a + max_chars];
            modified_text[a + max_chars] = '\0';

            // Draw truncated text
            ui_write(pos_x + thickness + char_width, current_y, &modified_text[a], text_color, text_size);

            // Restore the character
            modified_text[a + max_chars] = temp;
        }
        else
        {
            // Draw full text
            ui_write(pos_x + thickness + char_width, current_y, &modified_text[a], text_color, text_size);
        }

        // Move to next line
        current_y += item_height;
        current_element++;
        displayed_items++;

        // Move to next string
        while(modified_text[a] != '\0') a++;
        a++;  // Skip null terminator
        if(modified_text[a] == '\0') break;
    }

    // Draw scroll buttons if needed
    if (total_elements > visible_items)
    {
        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + 30, list_start_index[alias_index] > 0 ? border_color : color_border_disabled);
        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + 3, list_start_index[alias_index] > 0 ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 30, pos_y + 3, pos_y + 30, list_start_index[alias_index] > 0 ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 3, pos_y + 3, pos_y + 30, list_start_index[alias_index] > 0 ? border_color : color_border_disabled);

        for(a = 1; a < 11; a++)
        {
            lcd_draw_line_x(pos_x + size_x - 16 - a, pos_x + size_x - 16 + a, pos_y + 11 + a, list_start_index[alias_index] > 0 ? text_color : color_border_disabled);
        }

        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + size_y - 30, current_element < total_elements ? border_color : color_border_disabled);
        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + size_y - 3, current_element < total_elements ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 30, pos_y + size_y - 30, pos_y + size_y - 3, current_element < total_elements ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 3, pos_y + size_y - 30, pos_y + size_y - 3, current_element < total_elements ? border_color : color_border_disabled);

        for(a = 1; a < 11; a++)
        {
            lcd_draw_line_x(pos_x + size_x - 16 - a, pos_x + size_x - 16 + a, pos_y + size_y - 10 - a, current_element < total_elements ? text_color : color_border_disabled);
        }
    }

    // Free allocated memory
    free(modified_text);
}

void ui_text_update(uint8_t form_index, uint8_t control_index, const uint8_t *new_text)
{
    const ui_static_form *form = &forms[form_index];
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
    const ui_static_control *static_ctrl = &ui_static_controls[control_index];
    uint8_t char_width = 0, char_height = 0;
    uint16_t pos_x = pgm_read_word(&form->pos_x) + pgm_read_word(&static_ctrl->pos_x);
    uint16_t pos_y = pgm_read_word(&form->pos_y) + pgm_read_word(&static_ctrl->pos_y);
    uint16_t size_x = pgm_read_word(&static_ctrl->size_x);
    uint16_t size_y = pgm_read_word(&static_ctrl->size_y);
    uint16_t back_color, text_color, border_color;
    uint8_t text_size = pgm_read_byte(&static_ctrl->text_size);
    uint8_t thickness = pgm_read_byte(&static_ctrl->thickness);
    uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type);
    uint8_t use_system_colors = ctrl_type & UseSystemColors;

    if((ctrl_type & 0x7F) != TextBox) return;

    const uint8_t *caption = (const uint8_t*)pgm_read_word(&static_ctrl->text);
    uint8_t text_box_index = ui_control_alias[control_index];

    // Case 4 both are NULL, use caption
    if (new_text == NULL && (ui_text_contents[text_box_index] == NULL ||
        ui_text_contents[text_box_index][0] == '\0')) {
        // Get caption length
        uint8_t len = 0;
        while (pgm_read_byte(&caption[len]) != '\0') len++;

        // Allocate and store caption
        ui_text_contents[text_box_index] = (uint8_t*)malloc(len + 1);
        if (ui_text_contents[text_box_index] != NULL) {
            ui_text_contents[text_box_index] = (uint8_t *)caption;
            ui_text_contents[text_box_index][len] = '\0';
        }
        new_text = NULL;  // Force using ui_text_contents path
    }

    // Determine colors
    if (use_system_colors) {
        back_color = color_text_back;
        text_color = color_text;
        border_color = color_border;
    } else {
        back_color = pgm_read_word(&static_ctrl->back_color);
        text_color = pgm_read_word(&static_ctrl->text_color);
        border_color = pgm_read_word(&static_ctrl->border_color);
    }

    lcd_get_char_size('A', text_size, &char_width, &char_height);
    uint8_t max_length = (size_x - (2 * thickness) - 4) / char_width;

    // Case 1: new_text is NULL, use stored content
    if (new_text == NULL) {
        new_text = ui_text_contents[text_box_index];
    }
    // Case 2: stored content is NULL, initialize with new_text
    else if (ui_text_contents[text_box_index] == NULL) {
        uint8_t len = 0;
        while (new_text[len] != '\0') len++;

        ui_text_contents[text_box_index] = (uint8_t*)malloc(len + 1);
        if (ui_text_contents[text_box_index] != NULL) {
            for (uint8_t i = 0; i <= len; i++) {
                ui_text_contents[text_box_index][i] = new_text[i];
            }
        }
    }
    // Case 3: both exist, compare and update
    else {
        uint8_t len = 0;
        while (new_text[len] != '\0') len++;

        uint8_t *old_text = ui_text_contents[text_box_index];
        uint8_t a = 0;
        uint16_t char_x, char_y;

        // Update changed characters
        while (new_text[a] != '\0' && a < max_length) {
            char_x = pos_x + thickness + 2 + (a * char_width);
            char_y = pos_y + ((size_y - char_height) / 2);
            char_y += (text_size == 1) ? 1 : 2;

            if (old_text[a] != new_text[a] || old_text[a] == '\0') {
                lcd_draw_window(char_x, char_x + char_width - 1, char_y,
                              char_y + char_height - 1, back_color);
                ui_char(char_x, char_y, new_text[a], text_color, text_size);
            }
            a++;
        }

        // Clear remaining space
        while (old_text[a] != '\0' && a < max_length) {
            char_x = pos_x + thickness + 2 + (a * char_width);
            char_y = pos_y + ((size_y - char_height) / 2);
            char_y += (text_size == 1) ? 1 : 2;

            lcd_draw_window(char_x, char_x + char_width - 1, char_y,
                          char_y + char_height - 1, back_color);
            a++;
        }

        // Update stored content
        free(ui_text_contents[text_box_index]);
        ui_text_contents[text_box_index] = (uint8_t*)malloc(len + 1);
        if (ui_text_contents[text_box_index] != NULL) {
            for (uint8_t i = 0; i <= len; i++) {
                ui_text_contents[text_box_index][i] = new_text[i];
            }
        }
    }

    // Draw initial text if it hasn't been drawn yet
    if (new_text != NULL) {
        uint8_t a = 0;
        uint16_t char_x, char_y;
        while (new_text[a] != '\0' && a < max_length) {
            char_x = pos_x + thickness + 2 + (a * char_width);
            char_y = pos_y + ((size_y - char_height) / 2);
            char_y += (text_size == 1) ? 1 : 2;

            lcd_draw_window(char_x, char_x + char_width - 1, char_y,
                          char_y + char_height - 1, back_color);
            ui_char(char_x, char_y, new_text[a], text_color, text_size);
            a++;
        }
    }
}

// Handle touch events for a form
uint8_t ui_handle_touch()
{
    if(!ui_active_form) return 0; // No active form

    const ui_static_form *form = &forms[ui_active_form - 1]; // Active for 0 for none
    uint16_t form_x = pgm_read_word(&form->pos_x);
    uint16_t form_y = pgm_read_word(&form->pos_y);
    uint8_t num_static_ctrls = pgm_read_byte(&form->num_controls);
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);

    uint8_t ui_check_cursor_index(uint16_t form_pos_x, uint16_t form_pos_y, uint16_t ctrl_pos_x, uint16_t ctrl_pos_y, uint16_t ctrl_width, uint16_t ctrl_height)
    {
        uint16_t ctrl_x = form_pos_x + ctrl_pos_x;
        uint16_t ctrl_y = form_pos_y + ctrl_pos_y;
        return (cursor_x >= ctrl_x && cursor_x <= (ctrl_x + ctrl_width) &&
                cursor_y >= ctrl_y && cursor_y <= (ctrl_y + ctrl_height));
    }

    if (touch_sense())
    {
        touch_refresh();

        // Check basic controls
        for (uint8_t i = 0; i < num_static_ctrls; i++) {
            const ui_static_control *static_ctrl = &form1_controls[i];
            uint16_t ctrl_x = pgm_read_word(&static_ctrl->pos_x);
            uint16_t ctrl_y = pgm_read_word(&static_ctrl->pos_y);
            uint16_t ctrl_width = pgm_read_word(&static_ctrl->size_x);
            uint16_t ctrl_height = pgm_read_word(&static_ctrl->size_y);
            uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type) & (~(UseSystemColors)); // Remove use system colors flag

            if (ui_check_cursor_index(form_x, form_y, ctrl_x, ctrl_y, ctrl_width, ctrl_height))
            {
                // It is inside a control
                if(ctrl_type == CmdButton && ui_control_status[i] != CtrlDisabled)
                {
                    ui_control_status[i] = CtrlSelected;
                    ui_draw_control(ui_active_form - 1, i);
                    touch_wait_release();
                    ui_control_status[i] = CtrlNormal;
                    ui_draw_control(ui_active_form - 1, i);
                    if(i == 0 && (pgm_read_byte(&form->ctrl_type) == TitleClose)) // Form close button
                        ui_form_unload();
                    return i + 1;
                }
                if(ctrl_type == CheckBox && ui_control_status[i] != CtrlDisabled)
                {
                    uint8_t a = ui_control_status[i];
                    if(a == CtrlSelected) ui_control_status[i] = CtrlNormal;
                    if(a == CtrlNormal) ui_control_status[i] = CtrlSelected;
                    ui_draw_control(ui_active_form - 1, i);
                    touch_wait_release();
                    return i + 1;
                }
                if(ctrl_type == StaticListBox)
                {
                    // Calculate character dimensions and item heights like in draw function
                    uint8_t char_width = 0, char_height = 0;
                    uint8_t text_size = pgm_read_byte(&static_ctrl->text_size);
                    lcd_get_char_size('A', text_size, &char_width, &char_height);
                    uint16_t item_height = char_height + 6;  // Same spacing as in draw function
                    uint8_t thickness = pgm_read_byte(&static_ctrl->thickness);

                    // Calculate scroll button areas
                    uint16_t abs_x = form_x + ctrl_x;
                    uint16_t abs_y = form_y + ctrl_y;
                    uint16_t scroll_x = abs_x + ctrl_width - 30;

                    uint8_t alias_index = ui_control_alias[i];

                    // Check if touch is in up scroll button
                    if (cursor_x >= scroll_x && cursor_x <= (scroll_x + 27) &&
                        cursor_y >= (abs_y + 3) && cursor_y <= (abs_y + 30))
                    {
                        // Handle up scroll
                        if(list_start_index[alias_index] > 0) // If we have a selected item
                        {
                            list_start_index[alias_index]--;
                            ui_draw_control(ui_active_form - 1, i);
                        }
                        touch_wait_release();
                        return i + 1;
                    }

                    // Check if touch is in down scroll button
                    if (cursor_x >= scroll_x && cursor_x <= (scroll_x + 27) &&
                        cursor_y >= (abs_y + ctrl_height - 30) && cursor_y <= (abs_y + ctrl_height - 3))
                    {
                        // Handle down scroll
                        uint8_t total_elements = 0;

                        // Count total elements from current content
                        const uint8_t *content = ui_list_contents[alias_index];
                        if(!content) {
                            // If no dynamic content, use PROGMEM content
                            content = (const uint8_t*)pgm_read_word(&static_ctrl->text);
                        }

                        // Count elements by counting newlines
                        for(uint16_t idx = 0; content[idx] != '\0'; idx++) {
                            if(content[idx] == '\n') total_elements++;
                        }
                        total_elements++; // Account for last element

                        if(list_start_index[alias_index] < total_elements) {
                            list_start_index[alias_index]++;
                            ui_draw_control(ui_active_form - 1, i);
                        }
                        touch_wait_release();
                        return i + 1;
                    }

                    // Check if touch is in list area (excluding scroll buttons)
                    if (cursor_x >= (abs_x + thickness) && cursor_x <= (scroll_x - 1))
                    {
                        // Calculate which item was touched based on Y position
                        uint16_t rel_y = cursor_y - (abs_y + thickness);
                        uint16_t touched_index = rel_y / item_height;

                        // Calculate visible items
                        uint8_t visible_items = (ctrl_height - 2 * thickness) / item_height;

                        // If touch is within valid item area
                        if (touched_index < visible_items)
                        {
                            // Update selection
                            ui_control_status[i] = touched_index + 1; // +1 because 0 means no selection
                            ui_draw_control(ui_active_form - 1, i);
                            touch_wait_release();
                            return i + 1;
                        }
                        touch_wait_release();
                        return 0;
                    }
                }
                else
                {
                    touch_wait_release();
                    return i + 1;
                }
            }
        }
    }
    touch_wait_release();
    return 0; // No control pressed
}

