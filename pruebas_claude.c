
This is my touch handle function

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
                    ui_draw_control(form_index, i);
                    touch_wait_release();
                    ui_control_status[i] = CtrlNormal;
                    ui_draw_control(form_index, i);
                    if(i == 0 && (pgm_read_byte(&form->ctrl_type) == TitleClose)) // Form close button
                        ui_form_unload();
                    return i + 1;
                }
                if(ctrl_type == CheckBox && ui_control_status[i] != CtrlDisabled)
                {
                    uint8_t a = ui_control_status[i];
                    if(a == CtrlSelected) ui_control_status[i] = CtrlNormal;
                    if(a == CtrlNormal) ui_control_status[i] = CtrlSelected;
                    ui_draw_control(form_index, i);
                    touch_wait_release();
                    return i + 1;
                }
                if(ctrl_type == StaticListBox)
                {

                }
                else
                {
                    return i + 1;
                }
            }
        }
    }
    return 0; // No control pressed
}

In the case the list is touched i need to calculate in which element of the list, or eventually scroll UP / DOWN botton did i touch. For this we have to take in consideration the "show index" wich will tell from all the elements wich one is the first to be displayed. Can we do this?

I need to do this inside this function:

void ui_draw_list_box(uint8_t form_index, uint8_t control_index, uint8_t show_index, const uint8_t *dynamic_content)
{
    const ui_static_form *form = &forms[form_index];
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
    const ui_static_control *static_ctrl = &ui_static_controls[control_index];
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
    ctrl_type &= ~(UseSystemColors);  // Clear the system colors flag to get actual control type
    if(ctrl_type != StaticListBox) return;

    // Select content source based on dynamic_content parameter
    const uint8_t *caption;

    if (dynamic_content) {
        caption = dynamic_content;
    } else {
        // For PROGMEM content - Need to read the pointer from PROGMEM first
        caption = (const uint8_t*)pgm_read_word(&static_ctrl->text);
    }

    // First pass: count total string length needed
    uint16_t total_length = 0;
    const uint8_t *temp_caption = caption;

    while (*temp_caption != '\0') {
        total_length++;
        temp_caption++;
    }
    total_length++; // Account for final null terminator

    // Allocate buffer
    uint8_t *modified_text = (uint8_t*)malloc(total_length);
    if(modified_text == NULL)
    {
        // If allocation fails, just draw an empty box with border
        if (size_x < 100) size_x = 100;
        if (size_y < 80) size_y = 80;
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

    // Enforce minimum size
    if (size_x < 100) size_x = 100;
    if (size_y < 80) size_y = 80;

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
    while(caption[a] != '\0')
    {
        if(caption[a] == '\n')
        {
            modified_text[mod_idx++] = '\0';
            total_elements++;
        }
        else
        {
            modified_text[mod_idx++] = caption[a];
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

    // Adjust show_index if it would result in empty space at the bottom
    if (show_index > total_elements - visible_items && total_elements > visible_items)
    {
        show_index = total_elements - visible_items;
    }

    // Calculate vertical spacing
    uint16_t vertical_padding = 3;  // Fixed padding for more consistent appearance
    uint16_t current_y = pos_y + thickness + vertical_padding;

    // Draw list elements
    a = 0;
    uint8_t current_element = 0;
    uint8_t displayed_items = 0;

    // Skip to show_index
    while(current_element < show_index && a < mod_idx)
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
        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + 30, show_index > 0 ? border_color : color_border_disabled);
        lcd_draw_line_x(pos_x + size_x - 30, pos_x + size_x - 3, pos_y + 3, show_index > 0 ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 30, pos_y + 3, pos_y + 30, show_index > 0 ? border_color : color_border_disabled);
        lcd_draw_line_y(pos_x + size_x - 3, pos_y + 3, pos_y + 30, show_index > 0 ? border_color : color_border_disabled);

        for(a = 1; a < 11; a++)
        {
            lcd_draw_line_x(pos_x + size_x - 16 - a, pos_x + size_x - 16 + a, pos_y + 11 + a, show_index > 0 ? text_color : color_border_disabled);
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

So we have 4 situations:
1. dynamic_content is NULL -> The list box should be filled with ui_list_contents[ui_control_alias[control_index]]
2. ui_list_contents[ui_control_alias[control_index]] is NULL or not initialized -> The list box should be filled with dynamic_content, ui_text_contents[ui_control_alias[control_index]] allocated with the size of new_text and the contents stored in ui_text_contents.
3. both dynamic_content and ui_list_contents are initialized -> Compare both and redraw the lines that are different with the contents of dynamic_content. Then free ui_list_contents, allocate with the new list string size and store it.
4. Both are NULL -> load the contents for the list from PROGMEM as its done now, and draw the list, then allocate ui_list_contents with the size of the string in progmem and store it there.

This are the globals:

uint8_t list_control_count = 0;
uint8_t *ui_control_alias; // This is already allocated and filled.
uint8_t **ui_list_contents;

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
    const uint8_t *caption = (const uint8_t*)pgm_read_word(&static_ctrl->text);

    if((ctrl_type & 0x7F) != TextBox) return;

    // Get the text box index from alias
    uint8_t text_box_index = ui_control_alias[control_index];

    // Case 4: Both are NULL
    if (new_text == NULL && (ui_text_contents[text_box_index] == NULL ||
        ui_text_contents[text_box_index][0] == '\0')) {

    }

    ui_write(0,0,ui_text_contents[text_box_index],RED,1);

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
}



void ui_draw_list_box(uint8_t form_index, uint8_t control_index, uint8_t status, uint8_t show_index)
{
    const ui_static_form *form = &forms[form_index];
    const ui_static_control *ui_static_controls = (const ui_static_control*)pgm_read_word(&form->controls);
    const ui_static_control *static_ctrl = &ui_static_controls[control_index];
    uint8_t a = 0, total_elements = 0;
    uint8_t char_width = 0, char_height = 0;
    uint16_t pos_x = pgm_read_word(&form->pos_x) + pgm_read_word(&static_ctrl->pos_x);
    uint16_t pos_y = pgm_read_word(&form->pos_y) + pgm_read_word(&static_ctrl->pos_y);
    uint16_t size_x = pgm_read_word(&static_ctrl->size_x);
    uint16_t size_y = pgm_read_word(&static_ctrl->size_y);
    uint16_t back_color, text_color, border_color, selected_color;
    uint8_t text_size = pgm_read_byte(&static_ctrl->text_size);
    uint8_t thickness = pgm_read_byte(&static_ctrl->thickness);
    const uint8_t *caption = (const uint8_t*)pgm_read_word(&static_ctrl->text);

    // First pass: count total string length needed
    uint16_t total_length = 0;
    const uint8_t *temp_caption = caption;
    while(*temp_caption != '\0')
    {
        total_length++;
        temp_caption++;
    }
    total_length++; // Account for final null terminator

    // Allocate buffer
    uint8_t *modified_text = (uint8_t*)malloc(total_length);
    if(modified_text == NULL)
    {
        // If allocation fails, just draw an empty box with border
        if (size_x < 100) size_x = 100;
        if (size_y < 80) size_y = 80;
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

    // Enforce minimum size
    if (size_x < 100) size_x = 100;
    if (size_y < 80) size_y = 80;

    // Get control type and system colors flag
    uint8_t ctrl_type = pgm_read_byte(&static_ctrl->ctrl_type);
    uint8_t use_system_colors = ctrl_type & UseSystemColors;
    ctrl_type &= ~(UseSystemColors);  // Clear the system colors flag to get actual control type

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

    // Copy and process text, counting elements
    uint16_t mod_idx = 0;
    while(caption[a] != '\0')
    {
        if(caption[a] == '\n')
        {
            modified_text[mod_idx++] = '\0';
            total_elements++;
        }
        else
        {
            modified_text[mod_idx++] = caption[a];
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

    // Adjust show_index if it would result in empty space at the bottom
    if (show_index > total_elements - visible_items && total_elements > visible_items)
    {
        show_index = total_elements - visible_items;
    }

    // Calculate vertical spacing
    uint16_t vertical_padding = 3;  // Fixed padding for more consistent appearance
    uint16_t current_y = pos_y + thickness + vertical_padding;

    // Draw list elements
    a = 0;
    uint8_t current_element = 0;
    uint8_t displayed_items = 0;

    // Skip to show_index
    while(current_element < show_index && a < mod_idx)
    {
        while(modified_text[a] != '\0') a++;
        a++;  // Skip null terminator
        current_element++;
    }

    // Draw visible elements
    while(a < mod_idx && displayed_items < visible_items)
    {
        // Draw selected element with special background
        if(status > 0 && current_element + 1 == status)
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
        // Draw up button
        uint16_t button_x = pos_x + size_x - 30 - thickness;
        uint16_t button_y = pos_y + thickness;

        // Button background
        lcd_draw_window(button_x, button_x + 30, button_y, button_y + 30,
                       show_index > 0 ? border_color : color_border_disabled);

        // Up arrow
        if (show_index > 0)
        {
            lcd_draw_line_x(button_x + 10, button_x + 20, button_y + 20, text_color);
            lcd_draw_line_y(button_x + 15, button_y + 10, button_y + 20, text_color);
        }

        // Draw down button
        button_y = pos_y + size_y - 30 - thickness;

        // Button background
        lcd_draw_window(button_x, button_x + 30, button_y, button_y + 30,
                       current_element < total_elements ? border_color : color_border_disabled);

        // Down arrow
        if (current_element < total_elements)
        {
            lcd_draw_line_x(button_x + 10, button_x + 20, button_y + 10, text_color);
            lcd_draw_line_y(button_x + 15, button_y + 10, button_y + 20, text_color);
        }
    }

    // Free allocated memory
    free(modified_text);
}
