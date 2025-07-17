
#define F_CPU 32000000UL // CPU frequency in Hz (32MHz)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "strings.h"

const uint8_t str0[] PROGMEM = " ";
const uint8_t str1[] PROGMEM = "\n";
const uint8_t str2[] PROGMEM = "X";
const uint8_t str3[] PROGMEM = "No";
const uint8_t str4[] PROGMEM = "Yes";
const uint8_t str5[] PROGMEM = "Close";
const uint8_t str6[] PROGMEM = "Discard changes\nand exit?";
const uint8_t str7[] PROGMEM = "Save changes\nand exit?";
const uint8_t str8[] PROGMEM = "Exit";
const uint8_t str9[] PROGMEM = "Save";
const uint8_t str10[] PROGMEM = "0x";
const uint8_t str11[] PROGMEM = "Esc";
const uint8_t str12[] PROGMEM = "<-";
const uint8_t str13[] PROGMEM = "OK";
const uint8_t str14[] PROGMEM = "CAPS";
const uint8_t str15[] PROGMEM = "Space";
const uint8_t str16[] PROGMEM = "Ctrl";
const uint8_t str17[] PROGMEM = "Sym";
const uint8_t str18[] PROGMEM = "^";
const uint8_t str19[] PROGMEM = "<";
const uint8_t str20[] PROGMEM = ">";
const uint8_t str21[] PROGMEM = "Port";
const uint8_t str22[] PROGMEM = "EDIT";
const uint8_t str23[] PROGMEM = "DIR";
const uint8_t str24[] PROGMEM = "OUT";
const uint8_t str25[] PROGMEM = "PIN";
const uint8_t str26[] PROGMEM = "FREE FLASH MEMORY";
const uint8_t str27[] PROGMEM = "START";
const uint8_t str28[] PROGMEM = "STOP";
const uint8_t str29[] PROGMEM = "FREE RAM MEMORY";
const uint8_t str30[] PROGMEM = "User Interface";
const uint8_t str31[] PROGMEM = "SD Card";
const uint8_t str32[] PROGMEM = "Bluetooth";
const uint8_t str33[] PROGMEM = "NRF wireless";
const uint8_t str34[] PROGMEM = "I/O Control Program";
const uint8_t str35[] PROGMEM = "PWM Wave Generator";
const uint8_t str36[] PROGMEM = "Input data analyzer";
const uint8_t str37[] PROGMEM = "Paint";
const uint8_t str38[] PROGMEM = "Settings";
const uint8_t str39[] PROGMEM = " ";
const uint8_t str40[] PROGMEM = " ";
const uint8_t str41[] PROGMEM = " ";
const uint8_t str42[] PROGMEM = " ";
const uint8_t str43[] PROGMEM = " ";
const uint8_t str44[] PROGMEM = " ";
const uint8_t str45[] PROGMEM = " ";
const uint8_t str46[] PROGMEM = " ";
const uint8_t str47[] PROGMEM = " ";
const uint8_t str48[] PROGMEM = " ";
const uint8_t str49[] PROGMEM = "Loading...";
const uint8_t str50[] PROGMEM = "Load complete... Free memory : ";
const uint8_t str51[] PROGMEM = "DIR OUT PIN";
const uint8_t str52[] PROGMEM = "Edit PORT P status";
const uint8_t str53[] PROGMEM = "Enable DIR";
const uint8_t str54[] PROGMEM = "SD status changed";
const uint8_t str55[] PROGMEM = "Enable OUT";
const uint8_t str56[] PROGMEM = "Are you sure you want\nto modify AAA bit B ?";
const uint8_t str57[] PROGMEM = "PIN is read-only...";
const uint8_t str58[] PROGMEM = "DDR write disabled...";
const uint8_t str59[] PROGMEM = "OUT write disabled...";
const uint8_t str60[] PROGMEM = "Element count : ";
const uint8_t str61[] PROGMEM = "Frequency";
const uint8_t str62[] PROGMEM = "Duty [%]";
const uint8_t str63[] PROGMEM = "Disable DIR after STOP";
const uint8_t str64[] PROGMEM = "Keep output HIGH after STOP";
const uint8_t str65[] PROGMEM = " ";
const uint8_t str66[] PROGMEM = "Parameters not set";
const uint8_t str67[] PROGMEM = "Input PWM frequency";
const uint8_t str68[] PROGMEM = "Input PWM duty";
const uint8_t str69[] PROGMEM = "Max duty value is 100 %";
const uint8_t str70[] PROGMEM = "Port A";
const uint8_t str71[] PROGMEM = "Port B";
const uint8_t str72[] PROGMEM = "Port C";
const uint8_t str73[] PROGMEM = "Port D";
const uint8_t str74[] PROGMEM = "Port E";
const uint8_t str75[] PROGMEM = "Port F";
const uint8_t str76[] PROGMEM = "Hz";
const uint8_t str77[] PROGMEM = "kHz";
const uint8_t str78[] PROGMEM = "0b";
const uint8_t str79[] PROGMEM = "BN";
const uint8_t str80[] PROGMEM = "ADC input data analyzer";
const uint8_t str81[] PROGMEM = "Memory allocation error: emergency buffer size = 256 Bytes\n";
const uint8_t str82[] PROGMEM = "Memory allocation error: emergency buffer size = 32 Bytes\n";
const uint8_t str83[] PROGMEM = " ";
const uint8_t str84[] PROGMEM = " ";
const uint8_t str85[] PROGMEM = " ";
const uint8_t str86[] PROGMEM = " ";
const uint8_t str87[] PROGMEM = " ";
const uint8_t str88[] PROGMEM = " ";
const uint8_t str89[] PROGMEM = " ";
const uint8_t str90[] PROGMEM = "Welcome";
const uint8_t str91[] PROGMEM = "Graphic interface";
const uint8_t str92[] PROGMEM = "Command line";
const uint8_t str93[] PROGMEM = " ";
const uint8_t str94[] PROGMEM = " ";
const uint8_t str95[] PROGMEM = " ";
const uint8_t str96[] PROGMEM = "Item 1\nItem 2\nItem 3\nItem 4";
const uint8_t str97[] PROGMEM = " ";
const uint8_t str98[] PROGMEM = " ";
const uint8_t str99[] PROGMEM = " ";
const uint8_t str100[] PROGMEM = "ATMEL ATxmega128A3U @ 32 Mhz\n\nnIngen Shell V1.0\n\n";
const uint8_t str101[] PROGMEM = "Error reading setting:\n";
const uint8_t str102[] PROGMEM = "Console buffer size set to 1024 bytes (default)\n";
const uint8_t str103[] PROGMEM = "Console input buffer size set to 128 bytes (default)\n";
const uint8_t str104[] PROGMEM = "Console buffer allocated : ";
const uint8_t str105[] PROGMEM = " Bytes\n\0";
const uint8_t str106[] PROGMEM = "Error reading setting: ";
const uint8_t str107[] PROGMEM = "Console Keypad Size set to 30 (default)\n";
const uint8_t str108[] PROGMEM = "Console Back Color set to default\n";
const uint8_t str109[] PROGMEM = "Console Text Color set to default\n";
const uint8_t str110[] PROGMEM = "Console Text Type set 1 (default)\n";
const uint8_t str111[] PROGMEM = "Touch cursor calibration X set to -24 (default)\n";
const uint8_t str112[] PROGMEM = "Touch cursor calibration Y set to -4 (default)\n";
const uint8_t str113[] PROGMEM = "Console buffer size : ";
const uint8_t str114[] PROGMEM = "Console input buffer size : ";
const uint8_t str115[] PROGMEM = "Console max lines : ";
const uint8_t str116[] PROGMEM = "Console max cols : ";
const uint8_t str117[] PROGMEM = "Console buffer used (index_b) : ";
const uint8_t str118[] PROGMEM = "Need `CRsysop`CN\n";
const uint8_t str119[] PROGMEM = "Store address -> ";
const uint8_t str120[] PROGMEM = "Address points to ";
const uint8_t str121[] PROGMEM = "IO ";
const uint8_t str122[] PROGMEM = "EEPROM ";
const uint8_t str123[] PROGMEM = "RAM ";
const uint8_t str124[] PROGMEM = "Data to write -> ";
const uint8_t str125[] PROGMEM = "Missing parameter -a address\n";
const uint8_t str126[] PROGMEM = "Missing parameter -v value\n";
const uint8_t str127[] PROGMEM = "Memory updated\n";
const uint8_t str128[] PROGMEM = "Settings start address ";
const uint8_t str129[] PROGMEM = "Setting number ";
const uint8_t str130[] PROGMEM = "conf program help\n\n1. To read a setting call\nconf -a [addr] -n [number]\n\n2. To update a setting call\nconf -a [addr] -n [number] -v [new val]\n\n3. To create a setting call\nconf -a [addr] -n [number] -m [caption] -v [val] -t [type]\n\n4. If you dont know what the fuck to do call\nconf -?\n\nThanks\n\n";
const uint8_t str131[] PROGMEM = "Missing parameter -a address\n";
const uint8_t str132[] PROGMEM = "Missing parameter -n setting number\n";
const uint8_t str133[] PROGMEM = "Setting not found\n";
const uint8_t str134[] PROGMEM = "Setting data type error\n";
const uint8_t str135[] PROGMEM = "Setting reading error\n";
const uint8_t str136[] PROGMEM = "Sysop, updating setting -> ";
const uint8_t str137[] PROGMEM = " ";
const uint8_t str138[] PROGMEM = " ";
const uint8_t str139[] PROGMEM = " ";
const uint8_t str140[] PROGMEM = "guix";
const uint8_t str141[] PROGMEM = "paint";
const uint8_t str142[] PROGMEM = "less";
const uint8_t str143[] PROGMEM = "conf";
const uint8_t str144[] PROGMEM = "store";
const uint8_t str145[] PROGMEM = "gamma";
const uint8_t str146[] PROGMEM = "status";
const uint8_t str147[] PROGMEM = "cls";
const uint8_t str148[] PROGMEM = "FAT";
const uint8_t str149[] PROGMEM = "sysop";
const uint8_t str150[] PROGMEM = "exit";
const uint8_t str151[] PROGMEM = "ls";
const uint8_t str152[] PROGMEM = "cd";
const uint8_t str153[] PROGMEM = "jiwonbday";
const uint8_t str154[] PROGMEM = "fread";
const uint8_t str155[] PROGMEM = "bitmap";
const uint8_t str156[] PROGMEM = " ";
const uint8_t str157[] PROGMEM = " ";
const uint8_t str158[] PROGMEM = " ";
const uint8_t str159[] PROGMEM = " ";
const uint8_t str160[] PROGMEM = " ";
const uint8_t str161[] PROGMEM = " ";
const uint8_t str162[] PROGMEM = " ";
const uint8_t str163[] PROGMEM = " ";
const uint8_t str164[] PROGMEM = " ";
const uint8_t str165[] PROGMEM = " ";
const uint8_t str166[] PROGMEM = " ";
const uint8_t str167[] PROGMEM = " ";
const uint8_t str168[] PROGMEM = " ";
const uint8_t str169[] PROGMEM = " ";
const uint8_t str170[] PROGMEM = " HEX8 ";
const uint8_t str171[] PROGMEM = " HEX16 ";
const uint8_t str172[] PROGMEM = " HEX32 ";
const uint8_t str173[] PROGMEM = " BIN ";
const uint8_t str174[] PROGMEM = " FLOAT ";
const uint8_t str175[] PROGMEM = " COLOR ";
const uint8_t str176[] PROGMEM = " DEC8 ";
const uint8_t str177[] PROGMEM = " DEC16 ";
const uint8_t str178[] PROGMEM = "Type not defined\n";
const uint8_t str179[] PROGMEM = "Value not defined\n";
const uint8_t str180[] PROGMEM = "Sysop, making -> ";
const uint8_t str181[] PROGMEM = "Type -> ";
const uint8_t str182[] PROGMEM = "Are you sure? y/n\n";
const uint8_t str183[] PROGMEM = "Ok, lets go then\n";
const uint8_t str184[] PROGMEM = " Data -> ";
const uint8_t str185[] PROGMEM = "Less address ";
const uint8_t str186[] PROGMEM = "Line elements (default 12) ";
const uint8_t str187[] PROGMEM = "Display item count (default 64) ";
const uint8_t str188[] PROGMEM = "No SD card found\n";
const uint8_t str189[] PROGMEM = "FAT32 filesystem not available on the SD card\n";
const uint8_t str190[] PROGMEM = "Directory error\n";
const uint8_t str191[] PROGMEM = "Directory not found\n";
const uint8_t str192[] PROGMEM = "Missing filename...\n";
const uint8_t str193[] PROGMEM = "Not enaugh free memory...\n";
const uint8_t str194[] PROGMEM = " ";
const uint8_t str195[] PROGMEM = " ";
const uint8_t str196[] PROGMEM = " ";
const uint8_t str197[] PROGMEM = " ";
const uint8_t str198[] PROGMEM = " ";
const uint8_t str199[] PROGMEM = "Its on 7th Febrero, Asshole\n";

const uint8_t err0[] PROGMEM = "Memory allocation error\nNot enaugh memory";
const uint8_t err1[] PROGMEM = "\n";
const uint8_t err2[] PROGMEM = "X";
const uint8_t err3[] PROGMEM = "No";
const uint8_t err4[] PROGMEM = "Yes";
const uint8_t err5[] PROGMEM = "Close";
const uint8_t err6[] PROGMEM = "Discard changes\nand exit?";
const uint8_t err7[] PROGMEM = "Save changes\nand exit?";
const uint8_t err8[] PROGMEM = "Exit";
const uint8_t err9[] PROGMEM = "Save";
const uint8_t err10[] PROGMEM = "0x";
const uint8_t err11[] PROGMEM = "Esc";
const uint8_t err12[] PROGMEM = "<-";
const uint8_t err13[] PROGMEM = "OK";
const uint8_t err14[] PROGMEM = "CAPS";
const uint8_t err15[] PROGMEM = "Space";
const uint8_t err16[] PROGMEM = "Ctrl";
const uint8_t err17[] PROGMEM = "Sym";
const uint8_t err18[] PROGMEM = "^";
const uint8_t err19[] PROGMEM = "<";
const uint8_t err20[] PROGMEM = ">";
const uint8_t err21[] PROGMEM = "Port";
const uint8_t err22[] PROGMEM = "EDIT";
const uint8_t err23[] PROGMEM = "DIR";
const uint8_t err24[] PROGMEM = "OUT";
const uint8_t err25[] PROGMEM = "PIN";
const uint8_t err26[] PROGMEM = "BN";
const uint8_t err27[] PROGMEM = "START";
const uint8_t err28[] PROGMEM = "STOP";
const uint8_t err29[] PROGMEM = "FREE MEMORY";
const uint8_t err30[] PROGMEM = "User Interface";
const uint8_t err31[] PROGMEM = "SD Card";
const uint8_t err32[] PROGMEM = "Bluetooth";
const uint8_t err33[] PROGMEM = "NRF wireless";
const uint8_t err34[] PROGMEM = "I/O Control Program";
const uint8_t err35[] PROGMEM = "PWM Wave Generator";
const uint8_t err36[] PROGMEM = "Input data analyzer";
const uint8_t err37[] PROGMEM = "Paint";
const uint8_t err38[] PROGMEM = "Settings";
const uint8_t err39[] PROGMEM = " ";
const uint8_t err40[] PROGMEM = " ";
const uint8_t err41[] PROGMEM = " ";
const uint8_t err42[] PROGMEM = " ";
const uint8_t err43[] PROGMEM = " ";
const uint8_t err44[] PROGMEM = " ";
const uint8_t err45[] PROGMEM = " ";
const uint8_t err46[] PROGMEM = " ";
const uint8_t err47[] PROGMEM = " ";
const uint8_t err48[] PROGMEM = " ";
const uint8_t err49[] PROGMEM = "Loading...";
const uint8_t err50[] PROGMEM = "Load complete... Free memory : ";


const uint8_t * const strings[] PROGMEM = { str0, str1, str2, str3, str4, str5, str6, str7, str8, str9, str10,
                                            str11, str12, str13, str14, str15, str16, str17, str18, str19, str20,
                                            str21, str22, str23, str24, str25, str26, str27, str28, str29, str30,
                                            str31, str32, str33, str34, str35, str36, str37, str38, str39, str40,
                                            str41, str42, str43, str44, str45, str46, str47, str48, str49, str50,
                                            str51, str52, str53, str54, str55, str56, str57, str58, str59, str60,
                                            str61, str62, str63, str64, str65, str66, str67, str68, str69, str70,
                                            str71, str72, str73, str74, str75, str76, str77, str78, str79, str80,
                                            str81, str82, str83, str84, str85, str86, str87, str88, str89, str90,
                                            str91, str92, str93, str94, str95, str96, str97, str98, str99, str100,
                                            str101, str102, str103, str104, str105, str106, str107, str108, str109, str110,
                                            str111, str112, str113, str114, str115, str116, str117, str118, str119, str120,
                                            str121, str122, str123, str124, str125, str126, str127, str128, str129, str130,
                                            str131, str132, str133, str134, str135, str136, str137, str138, str139, str140,
                                            str141, str142, str143, str144, str145, str146, str147, str148, str149, str150,
                                            str151, str152, str153, str154, str155, str156, str157, str158, str159, str160,
                                            str161, str162, str163, str164, str165, str166, str167, str168, str169, str170,
                                            str171, str172, str173, str174, str175, str176, str177, str178, str179, str180,
                                            str181, str182, str183, str184, str185, str186, str187, str188, str189, str190,
                                            str191, str192, str193, str194, str195, str196, str197, str198, str199
};

const uint8_t * const errors[] PROGMEM = { err0, err1, err2, err3, err4, err5, err6, err7, err8, err9, err10,
                                            err11, err12, err13, err14, err15, err16, err17, err18, err19, err20,
                                            err21, err22, err23, err24, err25, err26, err27, err28, err29, err30,
                                            err31, err32, err33, err34, err35, err36, err37, err38, err39, err40,
                                            err41, err42, err43, err44, err45, err46, err47, err48, err49, err50
};

uint8_t *get_string(uint8_t index)
{
    uint16_t a = 0, len = 0;

    const uint8_t* string_address = (const uint8_t*) pgm_read_word(&strings[index]);

    while(pgm_read_byte(string_address + len) != '\0') len++;

    len++;

    uint8_t* ram_str = (uint8_t*) malloc(len);

    if (ram_str != NULL)
        for(a = 0; a < len; a++)
            ram_str[a] = pgm_read_byte(string_address + a);

    return ram_str;
}

uint8_t *get_error_msg(uint8_t index)
{
    uint16_t a = 0, len = 0;

    const uint8_t* string_address = (const uint8_t*) pgm_read_word(&errors[index]);

    while(pgm_read_byte(string_address + len) != '\0') len++;

    len++;

    uint8_t* ram_str = (uint8_t*) malloc(len);

    if (ram_str != NULL)
        for(a = 0; a < len; a++)
            ram_str[a] = pgm_read_byte(string_address + a);

    return ram_str;
}

void free_string(uint8_t* ram_str)
{
    if (ram_str != NULL) {
        free(ram_str);
    }
}
