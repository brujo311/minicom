
cmake_minimum_required(VERSION 3.13)

# Your project name
project(MINICOM4 C)

# MCU and programmer config
set(MCU atxmega128a3u)
set(F_CPU 32000000UL)
set(PROGRAMMER avrispmkII)
set(BAUD 19200)

# Toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os -Wall")

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Include path
include_directories("include")

# Output
add_executable(${PROJECT_NAME}.elf ${SOURCES})

# Convert to HEX after build
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND avr-objcopy -O ihex -R .eeprom ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMENT "Generating HEX file"
)

# Flash with avrdude
add_custom_target(flash
    COMMAND avrdude -p ${MCU} -c ${PROGRAMMER} -P usb -U flash:w:${PROJECT_NAME}.hex:i
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "Flashing MCU..."
)
