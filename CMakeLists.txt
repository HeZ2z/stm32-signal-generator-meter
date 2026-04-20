cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain-arm-none-eabi.cmake" CACHE FILEPATH "")
endif()

project(stm32_signal_generator_meter C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_TARGET firmware)

set(STM32_DRIVERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/STM32F4xx_HAL_Driver")

set(PROJECT_SOURCES
    src/app/app.c
    src/display/display.c
    src/display/display_lcd.c
    src/display/display_lcd_hw.c
    src/display/display_lcd_scene.c
    src/display/display_lcd_scope.c
    src/display/display_lcd_xy.c
    src/display/display_lcd_xy_logic.c
    src/display/lcd_prim.c
    src/display/lcd_font.c
    src/display/display_uart.c
    src/scope/scope_render_logic.c
    src/system/main.c
    src/signal_capture/signal_capture_adc.c
    src/signal_capture/signal_capture_adc_logic.c
    src/signal_gen/signal_gen.c
    src/signal_gen/signal_gen_dac.c
    src/signal_gen/signal_gen_dac_logic.c
    src/signal_gen/signal_gen_logic.c
    src/signal_measure/signal_measure.c
    src/signal_measure/signal_measure_logic.c
    src/touch/touch.c
    src/board/stm32f4xx_hal_msp.c
    src/board/stm32f4xx_it.c
    src/system/syscalls.c
    src/system/system_stm32f4xx.c
    src/ui/ui_cmd.c
    src/ui/ui_ctrl.c
    src/ui/ui_touch_map.c
    src/ui/ui_actions.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_adc.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_adc_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_cortex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_dac.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_dac_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_dma.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_flash.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_flash_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_gpio.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_ltdc.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_ltdc_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_pwr.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_pwr_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_rcc.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_rcc_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_sdram.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_ll_fmc.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_tim.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_tim_ex.c
    ${STM32_DRIVERS_DIR}/Src/stm32f4xx_hal_uart.c
    startup/startup_stm32f429xx.s
)

add_executable(${PROJECT_TARGET}.elf ${PROJECT_SOURCES})

target_include_directories(${PROJECT_TARGET}.elf PRIVATE
    include
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmsis_device_f4/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMSIS_6/CMSIS/Core/Include
    ${STM32_DRIVERS_DIR}/Inc
    ${STM32_DRIVERS_DIR}/Inc/Legacy
)

target_compile_definitions(${PROJECT_TARGET}.elf PRIVATE
    STM32F429xx
    USE_HAL_DRIVER
)

target_compile_options(${PROJECT_TARGET}.elf PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
)

target_link_options(${PROJECT_TARGET}.elf PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/config/STM32F429IGTX_FLASH.ld
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_TARGET}.map
)

add_custom_command(TARGET ${PROJECT_TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_TARGET}.elf> ${CMAKE_BINARY_DIR}/${PROJECT_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_TARGET}.elf> ${CMAKE_BINARY_DIR}/${PROJECT_TARGET}.bin
    COMMENT "Generating HEX and BIN artifacts"
)
