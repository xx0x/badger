cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_GXX arm-none-eabi-g++ REQUIRED)
find_program(ARM_ASM arm-none-eabi-gcc REQUIRED)
# gcc-ar/gcc-ranlib load the LTO plugin, which plain ar/ranlib do not. Without them a static
# library built with -flto (tinyusb) archives fine but links with "undefined reference".
find_program(ARM_AR arm-none-eabi-gcc-ar REQUIRED)
find_program(ARM_RANLIB arm-none-eabi-gcc-ranlib REQUIRED)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(PYTHON3 NAMES python3 python REQUIRED)

set(CMAKE_C_COMPILER ${ARM_GCC})
set(CMAKE_CXX_COMPILER ${ARM_GXX})
set(CMAKE_ASM_COMPILER ${ARM_ASM})
set(CMAKE_AR ${ARM_AR})
set(CMAKE_RANLIB ${ARM_RANLIB})
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(badger C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(MCU_TYPE "PY32F071xB")
set(MCU_TYPE_LOWER "py32f071xb")
set(JLINK_DEVICE "PY32F071xB")
set(PROBE_RS_CHIP "${MCU_TYPE}")

set(CPU_FLAGS
  -mcpu=cortex-m0plus
  -mthumb
)

set(COMMON_WARNINGS
  -Wall
  -Wextra
  -Wpedantic
)

option(BADGER_STRICT_FLOAT_LITERALS "Treat float/double conversion warnings as errors in C++" OFF)

# The TinyUF2 bootloader lives in the first 16K of flash and starts the
# application at 0x08004000. Keep this in sync with the FLASH origin in
# lib/LDScripts/${MCU_TYPE_LOWER}.ld.
set(APP_FLASH_OFFSET 0x4000)
set(FLASH_BASE_ADDRESS 0x08000000)
set(FLASH_SIZE 0x20000)

# Region owned by the application, used to erase only the app when flashing it
# so that the bootloader below it survives.
math(EXPR APP_FLASH_START "${FLASH_BASE_ADDRESS} + ${APP_FLASH_OFFSET}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_FLASH_END "${FLASH_BASE_ADDRESS} + ${FLASH_SIZE} - 1" OUTPUT_FORMAT HEXADECIMAL)

set(COMMON_DEFINES
  USE_HAL_DRIVER
  ${MCU_TYPE}
  VECT_TAB_OFFSET=${APP_FLASH_OFFSET}
)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/lib/LDScripts/${MCU_TYPE_LOWER}.ld")
set(BOOTLOADER_HEX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/bootloader/tinyuf2-badger_py32f071.hex")
set(UF2CONV "${CMAKE_CURRENT_SOURCE_DIR}/lib/uf2/utils/uf2conv.py")
set(UF2_UPLOAD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/misc/upload_uf2.py")
set(UF2_FAMILY_ID "0x7d7a66ef" CACHE STRING "UF2 family ID accepted by the bootloader")
set(UF2_DRIVE_NAME "badgerboot" CACHE STRING "Name of the drive exposed by the bootloader")
set(STARTUP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/lib/CMSIS/Device/PY32F071/Source/gcc/startup_py32f071xx.s")

if(NOT EXISTS "${LINKER_SCRIPT}")
  message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()

if(NOT EXISTS "${BOOTLOADER_HEX_FILE}")
  message(WARNING "Bootloader image not found: ${BOOTLOADER_HEX_FILE}")
endif()

if(NOT EXISTS "${UF2CONV}")
  message(FATAL_ERROR "uf2conv.py not found: ${UF2CONV}\n"
                      "The uf2 submodule is missing, fetch it with:\n"
                      "  git submodule update --init")
endif()

set(APP_SOURCES
  src/main.cpp
  src/Badger.cpp
  src/app/Application.cpp
  src/app/Presets.cpp
  src/app/SaveFileHandler.cpp
  src/common/Clock.cpp
  src/common/MidiMerger.cpp
  src/common/MidiNoteSender.cpp
  src/common/MidiParser.cpp
  src/common/TapTempoHandler.cpp
  src/common/clocksources/InternalClockSource.cpp
  src/common/clocksources/MidiClockSource.cpp
  src/common/clocksources/ExternalClockSource.cpp
  src/dsp/sequencing/TrackSequencer.cpp
  src/dsp/math/HyperRandomness.cpp
  src/dsp/control/AdsrEnv.cpp
  src/dsp/filters/Svf.cpp
  src/dsp/sampling/EightBitSamplePlayer.cpp
  src/dsp/synthesis/OscillatorQ15.cpp
  src/dsp/synthesis/LoFiFm.cpp
  src/dsp/synthesis/Oscillator.cpp
  src/dsp/synthesis/MultiOsc.cpp
  src/dsp/synthesis/KarplusStrong.cpp
  src/dsp/effects/SimpleHardClipper.cpp
  src/hardware/AudioInterface.cpp
  src/hardware/AT24C.cpp
  src/hardware/AnalogInputs.cpp
  src/hardware/Buttons.cpp
  src/hardware/Connectivity.cpp
  src/hardware/Layers.cpp
  src/hardware/Leds.cpp
  src/hardware/MidiHandler.cpp
  src/hardware/UsbMidiHandler.cpp
  src/usb/usb_descriptors.c
  src/py32f071_it.c
  src/py32f071_hal_msp.c
  src/syscalls.c
)

set(CMSIS_SOURCES
  lib/CMSIS/Device/PY32F071/Source/system_py32f071.c
  ${STARTUP_FILE}
)

set(HAL_SOURCES
  lib/PY32F071_HAL_Driver/Src/py32f071_hal.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_adc.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_adc_ex.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_cortex.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_dac.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_dac_ex.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_dma.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_flash.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_gpio.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_i2c.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_pwr.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_rcc.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_rcc_ex.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_tim.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_tim_ex.c
  lib/PY32F071_HAL_Driver/Src/py32f071_hal_uart.c
)

# TinyUSB device stack, MIDI class only, on the Mentor MUSB controller the PY32F071 uses.
set(TINYUSB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/tinyusb")

if(NOT EXISTS "${TINYUSB_DIR}/src/tusb.c")
  message(FATAL_ERROR "TinyUSB not found in ${TINYUSB_DIR}. Run: git submodule update --init --recursive")
endif()

set(TINYUSB_SOURCES
  ${TINYUSB_DIR}/src/tusb.c
  ${TINYUSB_DIR}/src/common/tusb_fifo.c
  ${TINYUSB_DIR}/src/device/usbd.c
  ${TINYUSB_DIR}/src/class/midi/midi_device.c
  ${TINYUSB_DIR}/src/portable/mentor/musb/dcd_musb.c
)

add_library(tinyusb STATIC ${TINYUSB_SOURCES})

target_include_directories(tinyusb PUBLIC
  ${TINYUSB_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb
)

target_include_directories(tinyusb PRIVATE
  src
  lib/CMSIS/Core/Include
  lib/CMSIS/Device/PY32F071/Include
  lib/PY32F071_HAL_Driver/Inc
)

target_compile_definitions(tinyusb PUBLIC ${COMMON_DEFINES})

target_compile_options(tinyusb PRIVATE
  ${CPU_FLAGS}
  -ffunction-sections
  -fdata-sections
  -fno-common
  $<$<CONFIG:Debug>:-Og>
  $<$<NOT:$<CONFIG:Debug>>:-Os>
  -g3
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -flto=auto
)

add_executable(${PROJECT_NAME}.elf
  ${APP_SOURCES}
  ${CMSIS_SOURCES}
  ${HAL_SOURCES}
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE tinyusb)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
  OUTPUT_NAME ${PROJECT_NAME}
  SUFFIX ".elf"
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
  src
  lib/CMSIS/Core/Include
  lib/CMSIS/Device/PY32F071/Include
  lib/PY32F071_HAL_Driver/Inc
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE ${COMMON_DEFINES} NDEBUG)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS}
  ${COMMON_WARNINGS}
  -ffunction-sections
  -fdata-sections
  -fno-common
  $<$<CONFIG:Debug>:-Og>
  $<$<NOT:$<CONFIG:Debug>>:-Os>
  -g3
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
  $<$<COMPILE_LANGUAGE:CXX>:-Wdouble-promotion>
  $<$<COMPILE_LANGUAGE:CXX>:-Wfloat-conversion>
  $<$<COMPILE_LANGUAGE:CXX>:$<$<BOOL:${BADGER_STRICT_FLOAT_LITERALS}>:-Werror=double-promotion>>
  $<$<COMPILE_LANGUAGE:CXX>:$<$<BOOL:${BADGER_STRICT_FLOAT_LITERALS}>:-Werror=float-conversion>>
  -flto=auto
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS}
  -specs=nano.specs
  -specs=nosys.specs
  -T${LINKER_SCRIPT}
  -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  -flto=auto
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
  target_link_options(${PROJECT_NAME}.elf PRIVATE -Wl,--no-warn-rwx-segments)
endif()

# These two pull in tinyusb headers, which use flexible array members and anonymous structs.
set_source_files_properties(src/usb/usb_descriptors.c
  PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
set_source_files_properties(src/hardware/UsbMidiHandler.cpp
  PROPERTIES COMPILE_OPTIONS "-Wno-pedantic;-Wno-volatile")

set(HEX_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex")
set(BIN_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")
set(UF2_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.uf2")

# The UF2 is converted from the HEX so that it picks up the load addresses from
# the image itself rather than relying on a separately specified base address.
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
  COMMAND ${ARM_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
  COMMAND ${ARM_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
  COMMAND ${PYTHON3} ${UF2CONV} ${HEX_FILE} --convert --family ${UF2_FAMILY_ID} --output ${UF2_FILE}
  COMMENT "Generating HEX, BIN and UF2 images"
)

set(JLINKEXE "JLinkExe" CACHE FILEPATH "Path to SEGGER JLinkExe")
set(PROBE_RS "probe-rs" CACHE STRING "Path to probe-rs executable")
set(PROBE_RS_PROBE "" CACHE STRING "Optional probe selector for probe-rs, for example 'VID:PID:SERIAL'")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/misc/jlink-command.in ${CMAKE_BINARY_DIR}/jlink-command.jlink @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/misc/jlink-bootloader-command.in ${CMAKE_BINARY_DIR}/jlink-bootloader-command.jlink @ONLY)

add_custom_target(flash-jlink
  COMMAND ${JLINKEXE}
    -noGui 1
    -device ${JLINK_DEVICE}
    -if swd
    -speed 4000
    -JLinkScriptFile ${CMAKE_CURRENT_SOURCE_DIR}/misc/jlink-script
    -CommanderScript ${CMAKE_BINARY_DIR}/jlink-command.jlink
  DEPENDS ${PROJECT_NAME}.elf
  COMMENT "Flashing ${PROJECT_NAME}.hex with J-Link"
)

set(PROBE_RS_FLASH_COMMAND
  ${PROBE_RS}
  download
  --chip
  ${PROBE_RS_CHIP}
  --protocol
  swd
  --non-interactive
  --speed
  1800
)

if(PROBE_RS_PROBE)
  list(APPEND PROBE_RS_FLASH_COMMAND --probe ${PROBE_RS_PROBE})
endif()

set(PROBE_RS_FLASH_BOOTLOADER_COMMAND
  ${PROBE_RS_FLASH_COMMAND}
  --binary-format
  hex
  ${BOOTLOADER_HEX_FILE}
)

list(APPEND PROBE_RS_FLASH_COMMAND $<TARGET_FILE:${PROJECT_NAME}.elf>)

set(PROBE_RS_RESET_COMMAND
  ${PROBE_RS}
  reset
  --chip
  ${PROBE_RS_CHIP}
  --protocol
  swd
  --non-interactive
  --speed
  1800
)

if(PROBE_RS_PROBE)
  list(APPEND PROBE_RS_RESET_COMMAND --probe ${PROBE_RS_PROBE})
endif()

add_custom_target(flash-probe-rs
  COMMAND ${PROBE_RS_FLASH_COMMAND}
  COMMAND ${PROBE_RS_RESET_COMMAND}
  DEPENDS ${PROJECT_NAME}.elf
  COMMENT "Flashing and resetting $<TARGET_FILE_NAME:${PROJECT_NAME}.elf> with probe-rs (${PROBE_RS_CHIP})"
)

add_custom_target(flash-bootloader-jlink
  COMMAND ${JLINKEXE}
    -noGui 1
    -device ${JLINK_DEVICE}
    -if swd
    -speed 4000
    -JLinkScriptFile ${CMAKE_CURRENT_SOURCE_DIR}/misc/jlink-script
    -CommanderScript ${CMAKE_BINARY_DIR}/jlink-bootloader-command.jlink
  COMMENT "Flashing the TinyUF2 bootloader with J-Link (erases the whole chip)"
)

add_custom_target(flash-bootloader-probe-rs
  COMMAND ${PROBE_RS_FLASH_BOOTLOADER_COMMAND}
  COMMAND ${PROBE_RS_RESET_COMMAND}
  COMMENT "Flashing the TinyUF2 bootloader with probe-rs (${PROBE_RS_CHIP})"
)

add_custom_target(upload-uf2
  COMMAND ${PYTHON3} ${UF2_UPLOAD_SCRIPT} --drive ${UF2_DRIVE_NAME} ${UF2_FILE}
  DEPENDS ${PROJECT_NAME}.elf
  COMMENT "Uploading ${PROJECT_NAME}.uf2 to the '${UF2_DRIVE_NAME}' drive"
)
