
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "x128_ili9488_driver.h"
#include "ningen_ui.h"
#include "ll_x128a3u.h"
#include "fonts.h"
#include "strings.h"

uint16_t color_back = BLACK;
uint16_t color_border = BLUE;
uint16_t color_border_selected = BLUE;
uint16_t color_border_disabled = GRAY1;
uint16_t color_text = WHITE;
uint16_t color_text_selected = WHITE;
uint16_t color_text_disabled = GRAY4;
uint16_t color_form = GRAY4;
uint16_t color_cmd = GRAY6;
uint16_t color_cmd_disabled = BLACK;
uint16_t color_cmd_selected = HALF_B;

/*uint8_t matrix_status[MATRIX_SIZE];
uint16_t matrix_x0[MATRIX_SIZE];
uint16_t matrix_x1[MATRIX_SIZE];
uint16_t matrix_y0[MATRIX_SIZE];
uint16_t matrix_y1[MATRIX_SIZE];*/

#define CAPS_ON 0
#define SYM_ON 1
#define CTRL_ON 2

uint8_t keypad_status = 0;

void ui_text(uint16_t pos_x, uint16_t pos_y, uint16_t text, uint16_t color, uint8_t size)
{
    ui_text(pos_x, pos_y, get_string(text), color, size);
    free_string(text);
}

void ui_text(uint16_t pos_x, uint16_t pos_y, uint8_t * text, uint16_t color, uint8_t size)
{
    uint8_t c = 0, size_y = 16;
    lcd_set_position(pos_x, pos_y);
	while (text[c] != '\0')
    {
		lcd_draw_char(text[c++], color, size);
        if(text[c] == '\n')
        {
            get_char_size('A', size, NULL, &size_y);
            lcd_set_position(pos_x, pos_y + size_y);
            c++;
        }
	}
}

void ui_char(uint16_t pos_x, uint16_t pos_y, uint8_t character, uint16_t color, uint8_t size)
{
    lcd_set_position(pos_x, pos_y);
    lcd_draw_char(character, color, size);
}

/*uint8_t ui_check_touch()
{
    uint8_t a = 0;
    touch_refresh();
    for(a = 0; a < MATRIX_SIZE; a++)
    {
        if((matrix_status[a] & 0x80) &&
            cursor_x > matrix_x0[a] &&
            cursor_x < matrix_x1[a] &&
            cursor_y > matrix_y0[a] &&
            cursor_y < matrix_y1[a])
            return a;
    }
    return MATRIX_SIZE;
}*/

/*uint8_t ui_wait_touch()
{
    uint8_t a = 0;
    while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
        ;
    return ui_check_touch();
}*/

value_list* allocate_value_list(uint8_t *elements)
{
    uint8_t element_count = 0, a = 0;

    value_list *list_data = (value_list *)malloc(sizeof(value_list));
    if (!list_data) {
        return NULL; // Allocation failed
    }

    while(elements[a] != '\0')
    {
        if(elements[a] == '\n') element_count++;
        a++;
    }
    element_count++;
    lcd_write_debug(get_string(60));
    lcd_number(element_count, 10, RED, 1, NULL, NULL);

    // Set initial values
    list_data->position_x = 0;
    list_data->position_y = 0;
    list_data->size_x = 100; // Default size
    list_data->size_y = 100; // Default size
    list_data->text_size = 1; // Default type

    list_data->element_count = element_count;

    // Allocate memory for elements
    list_data->elements = (uint8_t *)malloc(strlen((uint8_t *)elements) + 1);
    if (!list_data->elements) {
        free(list_data);
        return NULL;
    }

    strcpy((uint8_t *)list_data->elements, (uint8_t *)elements);

    // Allocate memory for element_type and element_value arrays
    list_data->element_type = (uint8_t *)malloc(element_count * sizeof(uint8_t));
    list_data->element_value = (uint32_t *)malloc(element_count * sizeof(uint32_t));

    if (!list_data->element_type || !list_data->element_value) {
        free(list_data->elements);
        free(list_data);
        return NULL;
    }

    // Initialize element_type and element_value (default values)
    for (uint8_t i = 0; i < element_count; ++i) {
        list_data->element_type[i] = 0; // Default type
        list_data->element_value[i] = 0; // Default value
    }

    return list_data;
}

uint8_t ui_value_list_add(value_list *list_data, uint8_t *new_element, uint8_t type, uint32_t value)
{
    if (!list_data || !new_element) {
        return 1; // Invalid parameters
    }

    // Reallocate memory for new elements string (append with '\n' and the new element)
    size_t new_size = strlen((uint8_t *)list_data->elements) + strlen((uint8_t *)new_element) + 2; // +2 for '\n' and '\0'
    list_data->elements = (uint8_t *)realloc(list_data->elements, new_size);
    if (!list_data->elements) {
        return 1; // Reallocation failed
    }

    // Append the new element to the elements string
    strcat((uint8_t *)list_data->elements, get_string(1)); // ORDEN MODIFICADO
    strcat((uint8_t *)list_data->elements, (uint8_t *)new_element);

    // Reallocate memory for element_type and element_value arrays
    list_data->element_type = (uint8_t *)realloc(list_data->element_type, (list_data->element_count + 1) * sizeof(uint8_t));
    list_data->element_value = (uint32_t *)realloc(list_data->element_value, (list_data->element_count + 1) * sizeof(uint32_t));

    if (!list_data->element_type || !list_data->element_value) {
        return 1; // Reallocation failed
    }

    // Add the new element type and value
    list_data->element_type[list_data->element_count] = type;
    list_data->element_value[list_data->element_count] = value;

    // Increment the element count
    list_data->element_count++;

    return 0; // Success
}

uint8_t ui_value_list_remove_element(value_list *list_data, uint8_t index)
{
    if (!list_data || index >= list_data->element_count) {
        return 1; // Invalid index
    }

    // Remove the element by shifting remaining elements
    uint8_t *temp_elements = (uint8_t *)malloc(strlen((uint8_t *)list_data->elements) + 1); // Temporary memory for reassembly
    if (!temp_elements) {
        return -1; // Allocation failed
    }

    uint8_t *current_element = list_data->elements;
    uint8_t *next_element = strstr((uint8_t *)list_data->elements, get_string(1));

    for (uint8_t i = 0; i < list_data->element_count; ++i) {
        if (i == index) {
            // Skip the element at index
            current_element = next_element + 1;
            if (next_element) {
                next_element = strstr((uint8_t *)current_element, get_string(1));
            }
            continue;
        }

        // Copy the element to temp_elements
        if (i != 0) {
            strcat((uint8_t *)temp_elements, get_string(1));
        }
        strncat((uint8_t *)temp_elements, (uint8_t *)current_element, next_element - current_element);
        current_element = next_element + 1;
        if (next_element) {
            next_element = strstr((uint8_t *)current_element, get_string(1));
        }
    }

    // Free old elements string and update with new one
    free(list_data->elements);
    list_data->elements = temp_elements;

    // Adjust the element_type and element_value arrays
    for (uint8_t i = index; i < list_data->element_count - 1; ++i) {
        list_data->element_type[i] = list_data->element_type[i + 1];
        list_data->element_value[i] = list_data->element_value[i + 1];
    }

    // Reallocate memory for the reduced arrays
    list_data->element_type = (uint8_t *)realloc(list_data->element_type, (list_data->element_count - 1) * sizeof(uint8_t));
    list_data->element_value = (uint32_t *)realloc(list_data->element_value, (list_data->element_count - 1) * sizeof(uint32_t));

    // Decrement element count
    list_data->element_count--;

    return 0; // Success
}

void ui_value_list_update(value_list *list_data, uint8_t selected, uint8_t show_index)
{
    uint8_t *buffer;
    uint16_t text_x, text_y;
    uint8_t max_text_length, max_show_items;
    uint8_t text_size_x, text_size_y;

    lcd_draw_window(list_data->position_x, list_data->position_x + list_data->size_x, list_data->position_y, list_data->position_y + list_data->size_y, list_data->back_color);

    for(uint8_t a = 0; a < list_data->thickness; a++)
    {
        lcd_draw_line_x(list_data->position_x, list_data->position_x + list_data->size_x, list_data->position_y + a, list_data->border_color);
        lcd_draw_line_x(list_data->position_x, list_data->position_x + list_data->size_x, list_data->position_y + list_data->size_y - a, list_data->border_color);
        lcd_draw_line_y(list_data->position_x + a, list_data->position_y, list_data->position_y + list_data->size_y, list_data->border_color);
        lcd_draw_line_y(list_data->position_x + list_data->size_x - a, list_data->position_y, list_data->position_y + list_data->size_y, list_data->border_color);
    }

    get_char_size('A', list_data->text_size, &text_size_x, &text_size_y);
    max_text_length = ((list_data->size_x - (list_data->thickness * 2) - 6) / text_size_x) - 8;
    max_show_items = (list_data->size_y - (list_data->thickness * 2) - 6) / text_size_y;

    //for(uint8_t a = 0; a < list_data->element_count; a++)
    //{
        ui_write(list_data->position_x, list_data->position_y, list_data->elements, list_data->text_color, list_data->text_size);
        //list_data->title = (uint8_t *)malloc(strlen((uint8_t *)title) + 1);
        //if((a + 1) != selected) ui_write(list_data->position_x, list_data->position_y, list_data->elements, list_data->text_color, list_data->text_size);
    //}


}

value_list *ui_value_list(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *contents, uint16_t border, uint16_t back, uint16_t text, uint16_t color_selected, uint16_t color_sel_txt, uint8_t thickness, uint8_t text_type)
{
    if(size_x < 150 || size_y < 150) { size_x = 150, size_y = 150; }

    // Create ListBoxItem with title and initial elements
    value_list *list_data = allocate_value_list(contents);

    if (!list_data) {
        return NULL; // Allocation failed
    }

    list_data->position_x = pos_x;
    list_data->position_y = pos_y;
    list_data->size_x = size_x;
    list_data->size_x = size_y;
    list_data->back_color = back;
    list_data->text_color = text;
    list_data->border_color = border;
    list_data->sel_color = color_selected;
    list_data->sel_txt_color = color_sel_txt;
    list_data->text_size = text_type;
    list_data->thickness = thickness;

    ui_value_list_update(list_data, 0, 0);

    return list_data;
}

void ui_value_list_clear(value_list *list_data)
{
    free(list_data->elements);
    free(list_data->element_type);
    free(list_data->element_value);
    free(list_data);
}


void ui_command(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type)
{
    uint8_t a = 0, b = 0;
    uint8_t char_width = 0, char_height = 0;

    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back);

    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border);
    }

    while(caption[b] != '\0') b++; // Get text length

    lcd_get_char_size('A', type, &char_width, &char_height);
    ui_write(pos_x + ((size_x - (b * char_width)) / 2) + 1,  pos_y + ((size_y - char_height) / 2) + 1, caption, text, type);
}

void ui_check_box(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type)
{
    uint8_t a = 0, b = 0;
    uint8_t char_width = 0, char_height = 0;

    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back);

    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border);
    }

    //while(caption[b] != '\0') b++; // Get text length

    lcd_get_char_size('A', type, &char_width, &char_height);
    ui_write(pos_x + size_x + 5,  pos_y + ((size_y - char_height) / 2) + 1, caption, text, type);
}

void ui_check_box_update(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint16_t sel_color, uint8_t thickness, uint8_t status)
{
    uint16_t color_0 = 0, color_1 = 0, color_2 = 0;
    uint8_t a = 0;

    if(status == 1) // Deselected, enabled
    {
        color_0 = color_border;
        color_1 = color_back;
        color_2 = color_back;
    }
    if(status == 2) // Selected, enabled
    {
        color_0 = color_border;
        color_1 = color_back;
        color_2 = sel_color;
    }
    if(status == 3) // Deselected, disabled
    {
        color_0 = color_border_disabled;
        color_1 = color_back;
        color_2 = color_back;
    }
    if(status == 4) // Selected, disabled
    {
        color_0 = color_border_disabled;
        color_1 = color_back;
        color_2 = color_text_disabled;
    }

    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, color_1);
    lcd_draw_window(pos_x + thickness + 3, pos_x + size_x - thickness - 3, pos_y + thickness + 3, pos_y + size_y - thickness - 3, color_2);

    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, color_0);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, color_0);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, color_0);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, color_0);
    }
}

void ui_check_box_fill(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint16_t sel_color, uint8_t thickness)
{
    lcd_draw_window(pos_x + thickness + 3, pos_x + size_x - thickness - 3, pos_y + thickness + 3, pos_y + size_y - thickness - 3, sel_color);
}

void ui_text_box(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type)
{
    uint8_t a = 0, b = 0;
    uint8_t char_width = 0, char_height = 0;

    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back);

    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border);
    }

    uint16_t char_pos_x = pos_x + thickness + 2;
    //if(thickness == 0) char_width -= 2;

    lcd_get_char_size('A', type, &char_width, &char_height);
    ui_write(char_pos_x,  pos_y + ((size_y - char_height) / 2) + 1, caption, text, type);
}

void ui_text_box_update(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *caption, uint8_t *caption_old, uint16_t border, uint16_t back, uint16_t text, uint8_t thickness, uint8_t type, uint8_t max_length)
{
    uint8_t a = 0;
    uint8_t char_width, char_height; // Variables to store the width and height based on character type.

    uint16_t char_x;
    uint16_t char_y;

    lcd_get_char_size('A', type, &char_width, &char_height);

    while (caption[a] != '\0')
    {
        if (caption_old[a] != caption[a])
        {
            char_x = pos_x + thickness + 2 + (a * char_width);
            //if(thickness == 0) char_x -= 2;
            char_y = pos_y + ((size_y - char_height) / 2) + 2;

            if(type == 1) char_y = pos_y + ((size_y - char_height) / 2) + 1;

            lcd_draw_window(char_x, char_x + char_width, char_y, char_y + char_height, back);
            ui_char(char_x, char_y, caption[a], text, type);
        }
        a++;
    }

    // Check if caption_old is longer than caption and remove the extra characters.
    while (caption_old[a] != '\0' && a < max_length) {

        char_x = pos_x + thickness + 2 + (a * char_width);
        char_y = pos_y + ((size_y - char_height) / 2) + 2;

        if(type == 1) char_y = pos_y + ((size_y - char_height) / 2) + 1;

        lcd_draw_window(char_x, char_x + char_width, char_y, char_y + char_height, back);

        caption_old[a] = '\0';
        a++;
    }
}

void ui_form(uint16_t pos_x, uint16_t pos_y, uint16_t size_x, uint16_t size_y, uint8_t *title, uint16_t border, uint16_t back, uint16_t text, uint16_t title_col, uint8_t thickness, uint8_t text_type, CtrlBoxType ctrl_type)
{
    uint8_t a = 0, b = 0;
    uint16_t c = 0;
    uint32_t d = 0;
    uint8_t char_width, char_height;

    lcd_draw_window(pos_x, pos_x + size_x, pos_y, pos_y + size_y, back);

    for(a = 0; a < thickness; a++)
    {
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + a, border);
        lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + size_y - a, border);
        lcd_draw_line_y(pos_x + a, pos_y, pos_y + size_y, border);
        lcd_draw_line_y(pos_x + size_x - a, pos_y, pos_y + size_y, border);
    }

    lcd_get_char_size('A', text_type, &char_width, &char_height);

    if(ctrl_type != NoControlBox)
    {
        lcd_draw_window(pos_x + thickness, pos_x + size_x - thickness, pos_y + thickness, pos_y + thickness + 9 + char_height, title_col);
        ui_write(pos_x + 5 + thickness,  pos_y + 5 + thickness, title, text, text_type);

        for(a = 0; a < thickness; a++)
            lcd_draw_line_x(pos_x, pos_x + size_x, pos_y + thickness + 9 + char_height + a, border);
    }

    if(ctrl_type == TitleClose)
    {
        ui_command(pos_x + size_x - thickness - 24, pos_y + thickness + 2, 22, 19, get_string(2), color_border, color_cmd, color_text, 1, 3);
    }
}

uint8_t ui_message_yes_no(uint8_t *message) {
    uint16_t size_x, size_y;
    uint16_t pos_x, pos_y;
    uint8_t length = 0, lines = 0, length_old = 0;
    uint8_t a = 0, b = 0;
    uint16_t cmd_1_pos_x, cmd_1_pos_y, cmd_2_pos_x, cmd_2_pos_y;
    uint8_t cmd_width = 80, cmd_height = 40;

    // Calculate the length and number of lines
    while (b != 1) {
        if ((message[a] != '\n') && (message[a] != '\0'))
            length++;
        if (message[a] == '\n') {
            lines++;
            if (length > length_old)
                length_old = length;
            length = 0;
        }
        if (message[a] == '\0') {
            lines++;
            if (length > length_old)
                length_old = length;
            b = 1;
        }
        a++;
    }

    // Calculate dimensions and positions for the message box
    size_x = (length_old * 12) + 40;  // Adjusted width
    size_y = (lines * 16) + cmd_height + 60;      // Adjusted height with more space for buttons

    if(size_x < 210) size_x = 210;
    if(size_y < 100) size_y = 100;
    if(size_x > 479) size_x = 479;
    if(size_y > 319) size_y = 319;

    pos_x = (480 - size_x) / 2;
    pos_y = (320 - size_y) / 2;

    // Draw the message box
    lcd_draw_command(pos_x, pos_x + size_x, pos_y, pos_y + size_y, color_border, color_form);

    // Draw get_string(4) and get_string(3) buttons with space between them
    cmd_1_pos_x = pos_x + 20; cmd_1_pos_y = pos_y + size_y - cmd_height - 10;
    cmd_2_pos_x = pos_x + size_x - cmd_width - 20; cmd_2_pos_y = pos_y + size_y - cmd_height - 10;
    ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(3), color_border, color_cmd, color_text, 2, 3);
    ui_command(cmd_2_pos_x, cmd_2_pos_y, cmd_width, cmd_height, get_string(4), color_border, color_cmd, color_text, 2, 3);

    // Draw the message text within the box
    //lcd_draw_string_10x14(pos_x + 20, pos_y + 20, message, color_text);
    ui_text(pos_x + 20, pos_y + 20, message, color_text, 3);

    b = 0;
    while(b == 0)
    {
        while((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
        touch_refresh();

        if((cursor_x > cmd_1_pos_x) && (cursor_x < (cmd_1_pos_x + cmd_width)) && (cursor_y > cmd_1_pos_y) && (cursor_y < (cmd_1_pos_y + cmd_height)))
        {
            ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(3), color_border_selected, color_cmd_selected, color_text_selected, 3, 3);
            b = 1;
        }
        if((cursor_x > cmd_2_pos_x) && (cursor_x < (cmd_2_pos_x + cmd_width)) && (cursor_y > cmd_2_pos_y) && (cursor_y < (cmd_2_pos_y + cmd_height)))
        {
            ui_command(cmd_2_pos_x, cmd_2_pos_y, cmd_width, cmd_height, get_string(4), color_border_selected, color_cmd_selected, color_text_selected, 3, 3);
            b = 2;
        }
        while(!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ))) ;
        if(b == 1)
        {
            ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(3), color_border, color_cmd, color_text, 2, 3);
        }
        if(b == 2)
        {
            ui_command(cmd_2_pos_x, cmd_2_pos_y, cmd_width, cmd_height, get_string(4), color_border, color_cmd, color_text, 2, 3);
        }
    }

    _delay_ms(500);
    return b - 1;
}

void ui_message_close(uint8_t *message) {
    uint16_t size_x, size_y;
    uint16_t pos_x, pos_y;
    uint8_t length = 0, lines = 0, length_old = 0;
    uint8_t a = 0, b = 0;
    uint16_t cmd_1_pos_x, cmd_1_pos_y;
    uint8_t cmd_width = 80, cmd_height = 40;

    // Calculate the length and number of lines
    while (b != 1) {
        if ((message[a] != '\n') && (message[a] != '\0'))
            length++;
        if (message[a] == '\n') {
            lines++;
            if (length > length_old)
                length_old = length;
            length = 0;
        }
        if (message[a] == '\0') {
            lines++;
            if (length > length_old)
                length_old = length;
            b = 1;
        }
        a++;
    }

    // Calculate dimensions and positions for the message box
    size_x = (length_old * 12) + 40;  // Adjusted width
    size_y = (lines * 16) + cmd_height + 60;  // Adjusted height with more space for the get_string(5) button

    if (size_x < 210)
        size_x = 210;  // Minimum width
    if (size_y < 100)
        size_y = 100;  // Minimum height
    if (size_x > 479)
        size_x = 479;  // Maximum width
    if (size_y > 319)
        size_y = 319;  // Maximum height

    pos_x = (480 - size_x) / 2;
    pos_y = (320 - size_y) / 2;

    // Draw the message box
    lcd_draw_command(pos_x, pos_x + size_x, pos_y, pos_y + size_y, color_border, color_form);

    // Draw the get_string(5) button
    cmd_1_pos_x = pos_x + (size_x - cmd_width) / 2;
    cmd_1_pos_y = pos_y + size_y - cmd_height - 10;
    ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(5), color_border, color_cmd, color_text, 2, 3);

    // Draw the message text within the box
    //lcd_draw_string_10x14(pos_x + 20, pos_y + 20, message, color_text);
    ui_text(pos_x + 20, pos_y + 20, message, color_text, 3);

    b = 0;
    while (b == 0) {
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();

        if ((cursor_x > cmd_1_pos_x) && (cursor_x < (cmd_1_pos_x + cmd_width)) && (cursor_y > cmd_1_pos_y) && (cursor_y < (cmd_1_pos_y + cmd_height))) {
            ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(5), color_border_selected, color_cmd_selected, color_text_selected, 3, 3);
            b = 1;
        }
        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        ui_command(cmd_1_pos_x, cmd_1_pos_y, cmd_width, cmd_height, get_string(5), color_border, color_cmd, color_text, 2, 3);
    }
    _delay_ms(500);
}

void numpad_draw_cmd(uint16_t pos_x, uint16_t pos_y, uint8_t * caption, uint8_t status)
{
    if(status == 1) ui_command(pos_x, pos_y, 40, 40, caption, color_border, color_cmd, color_text, 2, 3);
    if(status == 2) ui_command(pos_x, pos_y, 40, 40, caption, color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
    if(status == 3) ui_command(pos_x, pos_y, 40, 40, caption, color_border_disabled, color_cmd_disabled, color_text_disabled, 2, 3);
}

void ui_numpad(uint8_t *caption, uint8_t *data_n, uint8_t *data_s, uint8_t type)
{
    uint8_t a = 0, b = 0, c = 1, d = 0x80;
    uint16_t pos_x = 100, pos_y = 15;
    uint16_t cmd_pos_x[21];
    uint16_t cmd_pos_y[21];
    uint32_t cmd_enabled = 0;
    uint8_t txt[3*21];
    uint8_t status[21];
    uint8_t buffer[32];
    uint8_t buffer_old[32];
    uint8_t buffer_length = 0;
    uint16_t b_1 = 0;
    uint32_t b_2 = 0;
    double b_3 = 0.0f;

    if(type == DEC8) {a = data_n[0]; itoa(a, buffer, 10); }
    if(type == DEC16) {b_1 = (data_n[0] << 8) | data_n[1]; itoa(b_1, buffer, 10); }
    //if(type == DEC32) {memcpy(&b_2, data_n, 4); itoa(b_2, buffer, 10); }
    if(type == FLOAT) {memcpy(&b_3, data_n, 4); dtostrf(b_3, 2, 3, buffer); }
    if(type == HEX8) {a = data_n[0]; itoa(a, buffer, 16); }
    if(type == HEX16) {b_1 = (data_n[0] << 8) | data_n[1]; itoa(b_1, buffer, 16); }
    if(type == HEX32) {memcpy(&b_2, data_n, 4); itoa(b_2, buffer, 16); }

    for(a = 0; a < 21; a++) { txt[(3*a) + 1] = '\0'; txt[(3*a) + 2] = '\0'; }
    status[20] = 1;

    cmd_pos_x[20] = pos_x + 250;
    cmd_pos_y[20] = pos_y - 5;

    ui_form(pos_x, pos_y, 280, 290, caption, color_border, color_form, color_text, color_cmd_disabled, 2, 3, TitleClose);

    for(a = 0; a < 3; a++)
        for(b = 0; b < 3; b++)
        {
            cmd_pos_x[c] = pos_x + 20 + (50 * a);
            cmd_pos_y[c] = pos_y + 80 + (50 * b);
            txt[3*c] = '0' + c;
            status[c] = 1;
            c++;
        }
    if((type == HEX8) || (type == HEX16) || (type == HEX32))
        for(a = 0; a < 2; a++)
            for(b = 0; b < 3; b++)
            {
                cmd_pos_x[c] = pos_x + 20 + 150 + (50 * a);
                cmd_pos_y[c] = pos_y + 80 + (50 * b);
                txt[3*c] = 'A' + c - 10;
                status[c] = 1;
                c++;
            }
    if((type != HEX8) && (type != HEX16) && (type != HEX32))
        for(a = 0; a < 2; a++)
            for(b = 0; b < 3; b++)
            {
                cmd_pos_x[c] = pos_x + 20 + 150 + (50 * a);
                cmd_pos_y[c] = pos_y + 80 + (50 * b);
                txt[3*c] = 'A' + c - 10;
                status[c] = 3;
                c++;
            }
    cmd_pos_x[0] = pos_x + 20 + 50;
    cmd_pos_y[0] = pos_y + 80 + 150;
    txt[0] = '0';
    status[0] = 1;

    if(type == FLOAT)
    {
        cmd_pos_x[c] = pos_x + 20;
        cmd_pos_y[c] = pos_y + 80 + 150;
        txt[3*c] = '.';
        status[c] = 1;
        c++;

        cmd_pos_x[c] = pos_x + 20 + 100;
        cmd_pos_y[c] = pos_y + 80 + 150;
        txt[3*c] = '-';
        status[c] = 1;
        c++;
    }
    if(type != FLOAT)
    {
        cmd_pos_x[c] = pos_x + 20;
        cmd_pos_y[c] = pos_y + 80 + 150;
        txt[3*c] = '.';
        status[c] = 3;
        c++;

        cmd_pos_x[c] = pos_x + 20 + 100;
        cmd_pos_y[c] = pos_y + 80 + 150;
        txt[3*c] = '-';
        status[c] = 3;
        c++;
    }

    cmd_pos_x[c] = pos_x + 20 + 150;
    cmd_pos_y[c] = pos_y + 80 + 150;
    txt[3*c] = 'C'; txt[(3*c) + 1] = 'E';
    status[c] = 1;
    c++;

    cmd_pos_x[c] = pos_x + 20 + 200;
    cmd_pos_y[c] = pos_y + 80 + 150;
    txt[3*c] = 'O'; txt[(3*c) + 1] = 'K';
    status[c] = 1;

    a = 0;
    while(buffer[a++] != '\0') buffer_length++;
    ui_text_box(pos_x + 20, pos_y + 38, 240, 32, buffer, color_border, BLACK, color_text, 2, 3);

    for(a = 0; a < 20; a++)
        numpad_draw_cmd(cmd_pos_x[a], cmd_pos_y[a], &txt[3*a], status[a]);


    c = 0;
    while (c == 0) {
        d = 0x80;
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();
        for(a = 0; a < 21; a++) {
            if ((cursor_x > cmd_pos_x[a] - 5) && (cursor_x < (cmd_pos_x[a] + 45)) && (cursor_y > cmd_pos_y[a] - 5) && (cursor_y < (cmd_pos_y[a] + 45)) && (status[a] == 1)) d = a;
        }
        if(d < 20) numpad_draw_cmd(cmd_pos_x[d], cmd_pos_y[d], &txt[3*d], 2);
        if(d == 20) ui_command(pos_x + 280 - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border_selected, color_cmd_selected, color_text_selected, 1, 3);
        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        if(d < 20) numpad_draw_cmd(cmd_pos_x[d], cmd_pos_y[d], &txt[3*d], 1);
        if(d == 20) {
            ui_command(pos_x + 280 - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border, color_cmd, color_text, 1, 3);
            if(ui_message_yes_no(get_string(6)) == 1) return;
            lcd_draw_window(pos_x + 2, pos_x + 280 - 4, pos_y + 72, pos_y + 290 - 4, color_form);
            for(a = 0; a < 20; a++) numpad_draw_cmd(cmd_pos_x[a], cmd_pos_y[a], &txt[3*a], status[a]);
        }
        if((d >=0) && (d < 10))
        {
            for(a = 0; a < buffer_length; a++) buffer_old[a] = buffer[a];
            if(buffer_length == 1 && buffer[0] == '0') buffer_length = 0;
            buffer[buffer_length] = d + '0';
            buffer[buffer_length + 1] = '\0';
            buffer_length++;
            ui_text_box_update(pos_x + 20, pos_y + 38, 240, 32, buffer, buffer_old, color_border, BLACK, color_text, 2, 3, 32);
        }
        if((d > 9) && (d < 16))
        {
            for(a = 0; a < buffer_length; a++) buffer_old[a] = buffer[a];
            if(buffer_length == 1 && buffer[0] == '0') buffer_length = 0;
            buffer[buffer_length] = d + 'A' - 10;
            buffer[buffer_length + 1] = '\0';
            buffer_length++;
            ui_text_box_update(pos_x + 20, pos_y + 38, 240, 32, buffer, buffer_old, color_border, BLACK, color_text, 2, 3, 32);
        }
        if(d == 16)
        {
            for(a = 0; a < buffer_length; a++) {buffer_old[a] = buffer[a]; if(buffer[a] == '.') d = 50; }
            if(buffer_length == 0) { buffer_length = 2; buffer[0] = '0'; buffer[1] = '.'; d = 50; }
            if(d != 50)
            {
                buffer[buffer_length] = '.';
                buffer[buffer_length + 1] = '\0';
                buffer_length++;
                ui_text_box_update(pos_x + 20, pos_y + 38, 240, 32, buffer, buffer_old, color_border, BLACK, color_text, 2, 3, 32);
            }
        }
        if(d == 17)
        {
            //for(a = 0; a < buffer_length; a++) {buffer_old[a] = buffer[a]; if(buffer[a] == '.') d = 50; }
            if(buffer_length == 1 && buffer[0] == '0') buffer_length = 0;
            if(buffer_length == 0)
            {
                buffer[buffer_length] = '-';
                buffer[buffer_length + 1] = '\0';
                buffer_length++;
                ui_text_box_update(pos_x + 20, pos_y + 38, 240, 32, buffer, buffer_old, color_border, BLACK, color_text, 2, 3, 32);
            }
        }
        if(d == 18)
        {
            if(buffer_length > 0)
            {
                for(a = 0; a < buffer_length; a++) buffer_old[a] = buffer[a];
                buffer[buffer_length - 1] = '\0';
                buffer_length--;
                ui_text_box_update(pos_x + 20, pos_y + 38, 240, 32, buffer, buffer_old, color_border, BLACK, color_text, 2, 3, 32);
            }
        }
        if(d == 19)
        {
            if(ui_message_yes_no(get_string(7)))
            {
                memcpy(data_s, buffer, 16);
                if(type == DEC8) data_n[0] = atoi(buffer);
                if(type == DEC16) {b_1 = atoi(buffer); data_n[0] = (uint8_t)(b_1 >> 8); data_n[1] = (uint8_t)b_1; }
                if(type == FLOAT) {b_3 = atof(buffer); memcpy(data_n, &b_3, 4); }
                if(type == HEX8) {data_n[0] = (uint8_t)strtol(buffer, NULL, 16); }
                if(type == HEX16) {b_1 = (uint16_t)strtol(buffer, NULL, 16); data_n[0] = (uint8_t)(b_1 >> 8); data_n[1] = (uint8_t)b_1; }
                if(type == HEX32) {uint32_t b_2 = (uint32_t)strtol(buffer, NULL, 16); memcpy(&data_n[0], &b_2, sizeof(uint32_t)); }
                return;
            }
            lcd_draw_window(pos_x + 2, pos_x + 280 - 4, pos_y + 72, pos_y + 290 - 4, color_form);
            for(a = 0; a < 20; a++) numpad_draw_cmd(cmd_pos_x[a], cmd_pos_y[a], &txt[3*a], status[a]);
        }

    }

}

void ui_byte_pad(uint8_t *caption, uint8_t *data, uint8_t mask)
{
    uint16_t a = 0, b = 0;
    uint8_t c = 0, d = data[0], e = 0;
    uint16_t pos_x = 100, pos_y = 15;
    uint8_t buffer[32];

    while(b == 0)
    {
        buffer[a] = caption[a];
        if((caption[a] == '\n') || (caption[a] == '\t')) { b = a; buffer[a] = '\0'; }
        if(caption[a] == '\0') { b = 0x80; }
        a++;
    }
    a = b + 1;

    ui_form(pos_x, pos_y, 280, 290, buffer, color_border, color_form, color_text, color_cmd_disabled, 2, 3, TitleClose);

    for(c = 0; c < 8; c++)
    {
        if(b != 0x80)
        {
            b = 0;
            while(b == 0)
            {
                buffer[e] = caption[a];
                if((caption[a] == '\n') || (caption[a] == '\t')) { b = a; buffer[e] = '\0'; }
                if((caption[a] == '\0') && (c < 7)) { b = 0x80; }
                if((caption[a] == '\0') && (c == 7)) { b = 5; }
                a++; e++;
            }
        }
        if(b == 0x80)
        {
            buffer[0] = c + '0';
            buffer[1] = '\0';
        }
        ui_check_box(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, buffer, color_border, color_back, color_text, 2, 3);
        if((mask & (1 << c)) && (d & (1 << c))) ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 2);
        if((mask & (1 << c)) && !(d & (1 << c))) ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 1);
        if(!(mask & (1 << c)) && (d & (1 << c))) ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 4);
        if(!(mask & (1 << c)) && !(d & (1 << c))) ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 3);
        e = 0;
    }
    b = 0; e = 0;
    while(b == 0)
    {
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();
        if((cursor_x > (pos_x + 240)) && (cursor_x < (pos_x + 290)) && (cursor_y > pos_y - 5) && (cursor_y < pos_y + 35)) { ui_command(pos_x + 280 - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border_selected, color_cmd_selected, color_text_selected, 1, 3); e = 2; }
        if((cursor_x > (pos_x + 30)) && (cursor_x < (pos_x + 60)))
        {
            for(c = 0; c < 8; c++)
            {
                if((cursor_y > (pos_y + 40 + (c * 30))) && (cursor_y < (pos_y + 65 + (c * 30))))
                    if(mask & (1 << c))
                    {
                        if(d & (1 << c)) { d &= ~(1 << c); e = 1; ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 1); }
                        if(!(d & (1 << c)) && (e != 1)) { d |= (1 << c); ui_check_box_update(pos_x + 30, pos_y + 40 + (c * 30), 25, 25, BLUE, 2, 2); }
                        e = 0;
                    }
            }
        }
        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        if(e == 2)
        {
            ui_command(pos_x + 280 - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border, color_cmd, color_text, 1, 3);
            if(ui_message_yes_no(get_string(7)) == 1)
            {
                data[0] = d;
                return ;
            }
            return ;
        }
        e = 0;
    }
}

void ui_byte_pad_8(uint8_t *title, uint8_t *byte_name, uint8_t *data, uint8_t mask)
{
    uint8_t b = 0, c = 0, d = data[0], e = 0;
    uint16_t pos_x = 20, pos_y = 40;
    uint16_t pos_cx = 60, pos_cy = 70;
    uint16_t size_x = 440, size_y = 240;
    uint8_t buffer[1];

    buffer[0] = '\0';

    ui_form(pos_x, pos_y, size_x, size_y, title, color_border, color_form, color_text, color_cmd_disabled, 2, 3, TitleClose);
    ui_text(pos_x + 10, pos_y + pos_cy + 6, byte_name, YELLOW, SIZE3);

    ui_command(pos_x + size_x - 90, pos_y + size_y - 50, 80, 40, get_string(8), color_border, color_cmd, color_text, 2, 3);
    ui_command(pos_x + size_x - 90, pos_y + size_y - 100, 80, 40, get_string(9), color_border, color_cmd, color_text, 2, 3);

    for(c = 0; c < 8; c++)
    {
        //lcd_draw_char_10x14(pos_x + pos_cx + (c * 30) + 8, pos_y + pos_cy - 24, '7' - c, color_cmd_selected);
        ui_char(pos_x + pos_cx + (c * 30) + 8, pos_y + pos_cy - 24, '7' - c, color_cmd_selected,3);
        ui_check_box(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, buffer, color_border, color_back, color_text, 2, 3);
        if((mask & (1 << (7 - c))) && (d & (1 << (7 - c)))) ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 2);
        if((mask & (1 << (7 - c))) && !(d & (1 << (7 - c)))) ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 1);
        if(!(mask & (1 << (7 - c))) && (d & (1 << (7 - c)))) ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 4);
        if(!(mask & (1 << (7 - c))) && !(d & (1 << (7 - c)))) ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 3);
    }
    lcd_set_position(pos_x + pos_cx + 245, pos_y + pos_cy + 6);
    lcd_number(d, 16, YELLOW, SIZE3, get_string(10), NULL);
    b = 0; e = 0;
    while(b == 0)
    {
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();
        if((cursor_x > (pos_x + size_x - 30)) && (cursor_x < (pos_x + size_x)) && (cursor_y > pos_y - 5) && (cursor_y < pos_y + 35)) { ui_command(pos_x + size_x - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border_selected, color_cmd_selected, color_text_selected, 1, 3); e = 2; }
        if((cursor_y > (pos_y + pos_cy)) && (cursor_y < (pos_y + pos_cy + 25)))
        {
            for(c = 0; c < 8; c++)
            {
                if((cursor_x > (pos_x + pos_cx + (c * 30))) && (cursor_x < (pos_x + pos_cx + 25 + (c * 30))))
                    if(mask & (1 << (7 - c)))
                    {
                        if(d & (1 << (7 - c))) { d &= ~(1 << (7 - c)); e = 1; ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 1); }
                        if(!(d & (1 << (7 - c))) && (e != 1)) { d |= (1 << (7 - c)); ui_check_box_update(pos_x + pos_cx + (c * 30), pos_y + pos_cy, 25, 25, BLUE, 2, 2); }
                        lcd_draw_window(pos_x + pos_cx + 245, pos_x + pos_cx + 270, pos_y + pos_cy + 6, pos_y + pos_cy + 22, color_form);
                        lcd_set_position(pos_x + pos_cx + 245, pos_y + pos_cy + 6);
                        lcd_number(d, 16, YELLOW, SIZE3, get_string(10), NULL);
                        e = 0;
                    }
            }
        }
        if((cursor_x > (pos_x + size_x - 90)) && (cursor_x < (pos_x + size_x)) && (cursor_y > (pos_y + size_y - 50)) && (cursor_y < (pos_y + size_y)))
        {
            ui_command(pos_x + size_x - 90, pos_y + size_y - 50, 80, 40, get_string(8), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
            e = 3;
        }
        if((cursor_x > (pos_x + size_x - 90)) && (cursor_x < (pos_x + size_x)) && (cursor_y > (pos_y + size_y - 100)) && (cursor_y < (pos_y + size_y - 50)))
        {
            ui_command(pos_x + size_x - 90, pos_y + size_y - 100, 80, 40, get_string(9), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
            e = 4;
        }
        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        if(e == 2)
        {
            ui_command(pos_x + 280 - 2 - 24, pos_y + 2 + 2, 22, 19, get_string(2), color_border, color_cmd, color_text, 1, 3);
            if(ui_message_yes_no(get_string(7)) == 1)
            {
                data[0] = d;
                return ;
            }
            return ;
        }
        if(e == 3)
        {
            ui_command(pos_x + size_x - 90, pos_y + size_y - 50, 80, 40, get_string(8), color_border, color_cmd, color_text, 2, 3);
            _delay_ms(500);
            return ;
        }
        if(e == 4)
        {
            ui_command(pos_x + size_x - 90, pos_y + size_y - 100, 80, 40, get_string(9), color_border, color_cmd, color_text, 2, 3);
            data[0] = d;
            _delay_ms(500);
            return ;
        }
        e = 0;
    }
}

/*uint8_t ui_keypad(uint8_t * data, uint8_t max_length, uint8_t type, uint8_t cmd_size, uint8_t text_box_on)
{
    uint16_t keypad_x0[] = { 20, 200, 120, 100, 80, 140, 180, 220, 280, 260, 300, 340, 280, 240, 320, 360, 0, 120, 60, 160, 240, 160, 40, 80, 200, 40,
        400, 400, 400, 20, 100, 300, 380 };
    uint16_t keypad_y0[] = { 200, 240, 240, 200, 160, 200, 200, 200, 160, 200, 200, 200, 240, 240, 160, 160, 160, 160, 200, 160, 160, 240, 160, 240, 160, 240,
        160, 200, 240, 280, 280, 280, 280 };
    uint8_t keypad_x1[] = { 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
        79, 79, 79, 80, 200, 80, 80, 180, 80 };
    uint8_t symbols1[] = {'!', '[', '{', '#', '3', '.', '/', '=', '8', '&', ':', '-', '+', ']', '9', '0', '1', '4', ',', '5', '7', '}', '2', ')', '6', '('};
    uint8_t symbols2[] = {'~', '?', '<', '%', '3', '$', '^', '*', '8', '&', ';', '_', '"', '|', '9', '0', '1', '4', '@', '5', '7', '>', '2', ')', '6', '('};

    uint8_t a = 0, c = 0;
    uint8_t b[8];
    uint8_t index = 0;
    uint16_t b_1, b_2, b_3, b_4;
    uint8_t data_old[256];
    uint8_t backup[256];
    uint8_t original_length;

    keypad_status = 0;

    cmd_size = (cmd_size < 20) ? 20 : (cmd_size > 40) ? 40 : cmd_size;

    for(a = 0; a < 33; a++) keypad_y0[a] = (((keypad_y0[a] - 160) * cmd_size) / 40) + (320 - (4 * cmd_size)) ;

    for(a = 0; a < max_length; a++)
    {
        data_old[a] = data[a];
        backup[a] = data[a];
        if(data[a] == '\0') original_length = a;
    }
    index = original_length;

    lcd_draw_window(0, 40, keypad_y0[0], keypad_y0[0] + (cmd_size * 3) - 1, color_back);
    lcd_draw_window(keypad_x0[14], keypad_x0[14] + 80, keypad_y0[9], keypad_y0[9] + (cmd_size * 2), color_back);
    lcd_draw_window(440, 479, 320 - cmd_size, 319, color_back);
    for(a = 0; a < 26; a++)
    {
        b[0] = 'a' + a; b[1] = '\0';
        ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
    }
    ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(11), color_border, color_cmd, color_text, 2, 3); // 26
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(12), color_border, color_cmd, color_text, 2, 3); // 27
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(13), color_border, color_cmd, color_text, 2, 3); // 28
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(14), color_border, color_cmd, color_text, 2, 3); // 29
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(15), color_border, color_cmd, color_text, 2, 3); // 30
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(16), color_border, color_cmd, color_text, 2, 3); // 31
    a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(17), color_border, color_cmd, color_text, 2, 3); // 32

    b_1 = 0; b_3 = 479; b_2 = 130 + (160 - (cmd_size * 4)); b_4 = 30;
    if(type == T6x8_X1) {b_2 = 140 + (160 - (cmd_size * 4)); b_4 = 20; }
    if(text_box_on) ui_text_box(b_1, b_2, b_3, b_4, data, color_border, color_back, color_text, 2, type);

    while(1 == 1)
    {
        c = 0x80;
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();
        for(a = 0; a <= 32; a++)
            if((cursor_x > keypad_x0[a]) && (cursor_x < (keypad_x0[a] + keypad_x1[a])) && (cursor_y > keypad_y0[a]) && (cursor_y < (keypad_y0[a] + cmd_size - 1))) c = a;
        if((keypad_status &  (1 << CAPS_ON))) b[0] = 'A' + c;
        if(!(keypad_status &  (1 << CAPS_ON))) b[0] = 'a' + c;
        b[1] = '\0';
        if(c == 26)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(11), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 27)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(12), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 28)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(13), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 29)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(14), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 30)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(15), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 31)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(16), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 32)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(17), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c < 26)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, b, color_border_selected, color_cmd_selected, color_text_selected, 2, 3);


        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        if(c < 26)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            if(index < max_length)
            {
                if((keypad_status &  (1 << CAPS_ON))) data[index] = 'A' + c;
                if(!(keypad_status &  (1 << CAPS_ON))) data[index] = 'a' + c;
                if(((keypad_status &  (1 << SYM_ON))) && (!(keypad_status &  (1 << CTRL_ON)))) data[index] = symbols1[c];
                if(((keypad_status &  (1 << SYM_ON))) && ((keypad_status &  (1 << CTRL_ON)))) data[index] = symbols2[c];
                index++;
                data[index] = '\0';
                if(text_box_on) ui_text_box_update(b_1, b_2, b_3, b_4, data, data_old, color_border, color_back, color_text, 2, type, max_length);
                for(a = 0; a <= index; a++) data_old[a] = data[a];
            }
        }
        if(c == 26)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(11), color_border, color_cmd, color_text, 2, 3);
            for(a = 0; a < original_length; a++)
                data[a] = backup[a];
            return original_length;
        }
        if(c == 27)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(12), color_border, color_cmd, color_text, 2, 3);
            if(index > 0)
            {
                index--;
                data[index] = '\0';
                if(text_box_on) ui_text_box_update(b_1, b_2, b_3, b_4, data, data_old, color_border, color_back, color_text, 2, type, max_length);
                for(a = 0; a <= index; a++) data_old[a] = data[a];
            }
        }
        if(c == 28)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(13), color_border, color_cmd, color_text, 2, 3);
            return index;
        }
        if((c == 29) && ((keypad_status &  (1 << CAPS_ON))))
        {
            keypad_status &= ~(1 << CAPS_ON);
            c = 0x90;
            for(a = 0; a < 26; a++)
            {
                b[0] = 'a' + a; b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }
        if(c == 30)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(15), color_border, color_cmd, color_text, 2, 3);
            if(index < max_length)
            {
                data[index] = ' ';
                index++;
                data[index] = '\0';
                if(text_box_on) ui_text_box_update(b_1, b_2, b_3, b_4, data, data_old, color_border, color_back, color_text, 2, type, max_length);
                for(a = 0; a <= index; a++) data_old[a] = data[a];
            }
        }
        if((c == 31) && ((keypad_status &  (1 << CTRL_ON))))
        {
            keypad_status &= ~(1 << CTRL_ON);
            c = 0xa0;
        }
        if((c == 32) && ((keypad_status &  (1 << SYM_ON))))
        {
            keypad_status &= ~(1 << SYM_ON);
            c = 0xb0;
            for(a = 0; a < 26; a++)
            {
                if((keypad_status &  (1 << CAPS_ON))) b[0] = 'A' + a;
                if(!(keypad_status &  (1 << CAPS_ON))) b[0] = 'a' + a;
                b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }

        if((c == 29) && (!(keypad_status &  (1 << CAPS_ON))))
        {

            keypad_status &= ~(1 << SYM_ON);
            keypad_status &= ~(1 << CTRL_ON);
            keypad_status |= (1 << CAPS_ON);
            for(a = 0; a < 26; a++)
            {
                b[0] = 'A' + a; b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
            c = 0x91;

        }
        if((c == 31) && (!(keypad_status &  (1 << CTRL_ON))))
        {
            keypad_status |= (1 << CTRL_ON);
            keypad_status &= ~(1 << CAPS_ON);
            c = 0xa1;
        }
        if((c == 32) && (!(keypad_status &  (1 << SYM_ON))))
        {
            keypad_status &= ~(1 << CAPS_ON);
            keypad_status |= (1 << SYM_ON);
            c = 0xb1;
            for(a = 0; a < 26; a++)
            {
                if(!(keypad_status & (1 << CTRL_ON))) b[0] = symbols1[a];
                if((keypad_status & (1 << CTRL_ON))) b[0] = symbols2[a];
                b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }
        if(!(keypad_status &  (1 << SYM_ON))) ui_command(keypad_x0[32], keypad_y0[32], keypad_x1[32], cmd_size - 1, get_string(17), color_border, color_cmd, color_text, 2, 3);
        if(!(keypad_status &  (1 << CTRL_ON))) ui_command(keypad_x0[31], keypad_y0[31], keypad_x1[31], cmd_size - 1, get_string(16), color_border, color_cmd, color_text, 2, 3);
        if(!(keypad_status &  (1 << CAPS_ON))) ui_command(keypad_x0[29], keypad_y0[29], keypad_x1[29], cmd_size - 1, get_string(14), color_border, color_cmd, color_text, 2, 3);

    }

    return 0;
}*/

uint8_t ui_keypad(uint8_t * data, uint8_t max_length, uint8_t type, uint8_t cmd_size, uint8_t text_box_on, uint8_t toggle_nl)
{

    uint8_t a = 0, c = 0;
    uint8_t b[8];
    uint8_t index = 0;
    uint16_t b_1, b_2, b_3, b_4;
    uint8_t data_old[256];
    uint8_t backup[256];
    uint8_t original_length;

    keypad_status = 0;

    cmd_size = (cmd_size < 20) ? 20 : (cmd_size > 40) ? 40 : cmd_size;

    for(a = 0; a < max_length; a++)
    {
        data_old[a] = data[a];
        backup[a] = data[a];
        if(data[a] == '\0') original_length = a;
    }
    index = original_length;

    b_1 = 0; b_3 = 479; b_2 = 130 + (160 - (cmd_size * 4)); b_4 = 30;
    if(type == 1) {b_2 = 140 + (160 - (cmd_size * 4)); b_4 = 20; }
    if(text_box_on) ui_text_box(b_1, b_2, b_3, b_4, data, color_border, color_back, color_text, 2, type);

    ui_get_key(cmd_size, 0, 1); // dont wait, just redraw

    while(1 == 1)
    {
        c = ui_get_key(cmd_size, 1, 0); // wait, dunno redraw

        if(c == 0x1b) // ESCAPE
        {
            for(a = 0; a < max_length; a++)
                data[a] = backup[a];
            return original_length;
        }
        if(c == 0x0a) // ENTER (line feed)
        {
            if(toggle_nl) data[index++] = '\n';
            data[index] = '\0';
            return index;
        }
        if(c == 0x08 && index > 0) // BACKSPACE
        {
            index--;
            data[index] = '\0';
            if(text_box_on) ui_text_box_update(b_1, b_2, b_3, b_4, data, data_old, color_border, color_back, color_text, 2, type, max_length);
            for(a = 0; a <= index; a++) data_old[a] = data[a];
        }
        if(c >= 0x20 && c < 0x7f) // Character
        {
            if(index < max_length)
            {
                data[index] = c;
                index++;
                data[index] = '\0';
                if(text_box_on) ui_text_box_update(b_1, b_2, b_3, b_4, data, data_old, color_border, color_back, color_text, 2, type, max_length);
                for(a = 0; a <= index; a++) data_old[a] = data[a];
            }
        }
    }
}

uint8_t ui_get_key(uint8_t cmd_size, uint8_t wait, uint8_t redraw)
{
    uint16_t keypad_x0[] = { 20, 200, 120, 100, 80, 140, 180, 220, 280, 260, 300, 340, 280, 240, 320, 360, 0, 120, 60, 160, 240, 160, 40, 80, 200, 40,
        420, 400, 400, 20, 100, 320, 260, 380, 340, 380, 420 };
    uint16_t keypad_y0[] = { 200, 240, 240, 200, 160, 200, 200, 200, 160, 200, 200, 200, 240, 240, 160, 160, 160, 160, 200, 160, 160, 240, 160, 240, 160, 240,
        240, 160, 200, 280, 280, 240, 280, 240, 280, 280, 280 };
    uint8_t keypad_x1[] = { 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
        59, 79, 79, 80, 160, 60, 80, 40, 40, 40, 40 };
    uint8_t symbols1[] = {'!', '[', '{', '#', '3', '.', '/', '=', '8', '&', ':', '-', '+', ']', '9', '0', '1', '4', ',', '5', '7', '}', '2', ')', '6', '('};
    uint8_t symbols2[] = {'~', '?', '<', '%', '3', '$', '^', '*', '8', '&', ';', '_', '"', '|', '9', '0', '1', '4', '@', '5', '7', '>', '2', ')', '6', '('};

    uint8_t a = 0, c = 0, d = 0;
    uint8_t b[8];

    cmd_size = (cmd_size < 20) ? 20 : (cmd_size > 40) ? 40 : cmd_size;

    for(a = 0; a < 37; a++) keypad_y0[a] = (((keypad_y0[a] - 160) * cmd_size) / 40) + (320 - (4 * cmd_size)) ;

    if(redraw)
    {
        lcd_draw_window(0, 19, keypad_y0[0], keypad_y0[0] + (cmd_size * 3) - 1, color_form);
        lcd_draw_window(20, 39, keypad_y0[1], keypad_y0[1] + cmd_size - 1, color_form);
        lcd_draw_window(keypad_x0[33], keypad_x0[33] + 19, keypad_y0[0], keypad_y0[0] + cmd_size - 1, color_form);
        lcd_draw_window(460, 479, keypad_y0[34], keypad_y0[34] + cmd_size - 1, color_form);
        for(a = 0; a < 26; a++)
        {
            b[0] = 'a' + a; b[1] = '\0';
            ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
        }
        ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(11), color_border, color_cmd, color_text, 2, 3); // 26
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(12), color_border, color_cmd, color_text, 2, 3); // 27
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(13), color_border, color_cmd, color_text, 2, 3); // 28
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(14), color_border, color_cmd, color_text, 2, 3); // 29
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(15), color_border, color_cmd, color_text, 2, 3); // 30
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(16), color_border, color_cmd, color_text, 2, 3); // 31
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(17), color_border, color_cmd, color_text, 2, 3); // 32
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, get_string(18), color_border, color_cmd, color_text, 2, 3); // 29
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(19), color_border, color_cmd, color_text, 2, 3); // 30
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(0), color_border, color_cmd, color_text, 2, 3); // 31
        a++; ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size - 1, get_string(20), color_border, color_cmd, color_text, 2, 3); // 32
    }

    while(1 == 1)
    {
        if((!wait) && ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))) return 0xff; // No key pressed
        c = 0xff;
        while ((PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        touch_refresh();
        if(cursor_y < keypad_y0[4]) return 0xfe; // Key pressed above the keypad
        for(a = 0; a <= 36; a++)
            if((cursor_x > keypad_x0[a]) && (cursor_x < (keypad_x0[a] + keypad_x1[a])) && (cursor_y > keypad_y0[a]) && (cursor_y < (keypad_y0[a] + cmd_size - 1))) c = a;
        if((keypad_status &  (1 << CAPS_ON))) b[0] = 'A' + c;
        if(!(keypad_status &  (1 << CAPS_ON))) b[0] = 'a' + c;
        if(((keypad_status &  (1 << SYM_ON))) && (!(keypad_status &  (1 << CTRL_ON)))) b[0] = symbols1[c];
        if(((keypad_status &  (1 << SYM_ON))) && ((keypad_status &  (1 << CTRL_ON)))) b[0] = symbols2[c];
        b[1] = '\0';
        if(c == 26)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(11), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 27)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(12), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 28)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(13), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 29)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(14), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 30)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(15), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 31)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(16), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 32)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(17), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c < 26)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, b, color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 33)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(18), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 34)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(19), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 35)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(0), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);
        if(c == 36)
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(20), color_border_selected, color_cmd_selected, color_text_selected, 2, 3);


        while (!(PORT_TOUCH_IRQ & (1 << PIN_TOUCH_IRQ)))
            ;
        if(c < 26)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            if((keypad_status &  (1 << CAPS_ON))) d = 'A' + c;
            if(!(keypad_status &  (1 << CAPS_ON))) d = 'a' + c;
            if(((keypad_status &  (1 << SYM_ON))) && (!(keypad_status &  (1 << CTRL_ON)))) d = symbols1[c];
            if(((keypad_status &  (1 << SYM_ON))) && ((keypad_status &  (1 << CTRL_ON)))) d = symbols2[c];
            return d;
        }
        if(c == 26)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(11), color_border, color_cmd, color_text, 2, 3);
            return 0x1b; // escape
        }
        if(c == 27)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(12), color_border, color_cmd, color_text, 2, 3);
            return 0x08; // backspace
        }
        if(c == 28)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(13), color_border, color_cmd, color_text, 2, 3);
            return 0x0a; // line feed (enter)
        }
        if((c == 29) && ((keypad_status &  (1 << CAPS_ON))))
        {
            keypad_status &= ~(1 << CAPS_ON);
            c = 0x90;
            for(a = 0; a < 26; a++)
            {
                b[0] = 'a' + a; b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }
        if(c == 30)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(15), color_border, color_cmd, color_text, 2, 3);
            return ' ';
        }
        if((c == 31) && ((keypad_status &  (1 << CTRL_ON))))
        {
            keypad_status &= ~(1 << CTRL_ON);
            c = 0xa0;
        }
        if((c == 32) && ((keypad_status &  (1 << SYM_ON))))
        {
            keypad_status &= ~(1 << SYM_ON);
            c = 0xb0;
            for(a = 0; a < 26; a++)
            {
                if((keypad_status &  (1 << CAPS_ON))) b[0] = 'A' + a;
                if(!(keypad_status &  (1 << CAPS_ON))) b[0] = 'a' + a;
                b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }

        if((c == 29) && (!(keypad_status &  (1 << CAPS_ON))))
        {

            keypad_status &= ~(1 << SYM_ON);
            keypad_status &= ~(1 << CTRL_ON);
            keypad_status |= (1 << CAPS_ON);
            for(a = 0; a < 26; a++)
            {
                b[0] = 'A' + a; b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
            c = 0x91;
        }
        if((c == 31) && (!(keypad_status &  (1 << CTRL_ON))))
        {
            keypad_status |= (1 << CTRL_ON);
            keypad_status &= ~(1 << CAPS_ON);
            c = 0xa1;
        }
        if((c == 32) && (!(keypad_status &  (1 << SYM_ON))))
        {
            keypad_status &= ~(1 << CAPS_ON);
            keypad_status |= (1 << SYM_ON);
            c = 0xb1;
            for(a = 0; a < 26; a++)
            {
                if(!(keypad_status & (1 << CTRL_ON))) b[0] = symbols1[a];
                if((keypad_status & (1 << CTRL_ON))) b[0] = symbols2[a];
                b[1] = '\0';
                ui_command(keypad_x0[a], keypad_y0[a], keypad_x1[a], cmd_size, b, color_border, color_cmd, color_text, 2, 3);
            }
        }
        if(c == 33)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size, get_string(18), color_border, color_cmd, color_text, 2, 3); // 29
            return 0xf1;
        }
        if(c == 34)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(19), color_border, color_cmd, color_text, 2, 3); // 30
            return 0xf2;
        }
        if(c == 35)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(0), color_border, color_cmd, color_text, 2, 3); // 31
            return 0xf3;
        }
        if(c == 36)
        {
            ui_command(keypad_x0[c], keypad_y0[c], keypad_x1[c], cmd_size - 1, get_string(20), color_border, color_cmd, color_text, 2, 3); // 32
            return 0xf4;
        }


        if(!(keypad_status &  (1 << SYM_ON))) ui_command(keypad_x0[32], keypad_y0[32], keypad_x1[32], cmd_size - 1, get_string(17), color_border, color_cmd, color_text, 2, 3);
        if(!(keypad_status &  (1 << CTRL_ON))) ui_command(keypad_x0[31], keypad_y0[31], keypad_x1[31], cmd_size, get_string(16), color_border, color_cmd, color_text, 2, 3);
        if(!(keypad_status &  (1 << CAPS_ON))) ui_command(keypad_x0[29], keypad_y0[29], keypad_x1[29], cmd_size - 1, get_string(14), color_border, color_cmd, color_text, 2, 3);


    }

    return 0;
}
