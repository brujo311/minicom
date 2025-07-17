

#define FONT_COUNT 3

extern const uint8_t charset_1[];
extern const uint8_t charset_2[];
extern const uint8_t charset_3[];
extern const uint8_t charset_4[];

uint8_t get_char(uint8_t character, uint8_t * destination, uint8_t font_number);
uint8_t get_char_size(uint8_t character, uint8_t font_number, uint8_t * size_x, uint8_t * size_y);

