# Minimal board profile for RP2040 Zero
# This file can be included from the top-level CMakeLists to set board-specific defines

# Example: set(RP2040_BOARD "rp2040_zero")
# You can customize pins here or include a board-specific header

set(RP2040_BOARD "rp2040_zero" CACHE STRING "Target board name")
# Default to same pin assignments as current board; override as needed
set(PIN_MT6835_SCK 18 CACHE STRING "SCK pin for MT6835")
set(PIN_MT6835_MOSI 19 CACHE STRING "MOSI pin for MT6835")
set(PIN_MT6835_MISO 16 CACHE STRING "MISO pin for MT6835")
set(PIN_MT6835_CSN 17 CACHE STRING "CSN pin for MT6835")

message(STATUS "Loaded board profile: ${RP2040_BOARD}")
