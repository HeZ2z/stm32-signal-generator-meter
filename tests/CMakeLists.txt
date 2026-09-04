cmake_minimum_required(VERSION 3.22)

project(stm32_host_tests C)

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
  -Wall
  -Wextra
  -Werror
)

add_executable(test_ui_cmd
  test_ui_cmd.c
  ../src/ui/ui_cmd.c
)
target_compile_definitions(test_ui_cmd PRIVATE HOST_TEST)
target_include_directories(test_ui_cmd PRIVATE ../include ../src)
add_test(NAME test_ui_cmd COMMAND test_ui_cmd)

add_executable(test_signal_gen_logic
  test_signal_gen_logic.c
  ../src/signal_gen/signal_gen_logic.c
)
target_compile_definitions(test_signal_gen_logic PRIVATE HOST_TEST)
target_include_directories(test_signal_gen_logic PRIVATE ../include ../src)
add_test(NAME test_signal_gen_logic COMMAND test_signal_gen_logic)

add_executable(test_signal_measure_logic
  test_signal_measure_logic.c
  ../src/signal_measure/signal_measure_logic.c
)
target_compile_definitions(test_signal_measure_logic PRIVATE HOST_TEST)
target_include_directories(test_signal_measure_logic PRIVATE ../include ../src)
add_test(NAME test_signal_measure_logic COMMAND test_signal_measure_logic)

add_executable(test_scope_render_logic
  test_scope_render_logic.c
  ../src/scope/scope_render_logic.c
  ../src/signal_capture/signal_capture_adc_logic.c
)
target_compile_definitions(test_scope_render_logic PRIVATE HOST_TEST)
target_include_directories(test_scope_render_logic PRIVATE ../include ../src)
add_test(NAME test_scope_render_logic COMMAND test_scope_render_logic)

add_executable(test_signal_capture_adc_logic
  test_signal_capture_adc_logic.c
  ../src/signal_capture/signal_capture_adc_logic.c
)
target_compile_definitions(test_signal_capture_adc_logic PRIVATE HOST_TEST)
target_include_directories(test_signal_capture_adc_logic PRIVATE ../include ../src)
add_test(NAME test_signal_capture_adc_logic COMMAND test_signal_capture_adc_logic)

add_executable(test_signal_gen_dac_logic
  test_signal_gen_dac_logic.c
  ../src/signal_gen/signal_gen_dac_logic.c
)
target_compile_definitions(test_signal_gen_dac_logic PRIVATE HOST_TEST)
target_include_directories(test_signal_gen_dac_logic PRIVATE ../include ../src)
add_test(NAME test_signal_gen_dac_logic COMMAND test_signal_gen_dac_logic)

add_executable(test_display_lcd_xy_logic
  test_display_lcd_xy_logic.c
  ../src/display/display_lcd_xy_logic.c
)
target_compile_definitions(test_display_lcd_xy_logic PRIVATE HOST_TEST)
target_include_directories(test_display_lcd_xy_logic PRIVATE ../include ../src)
add_test(NAME test_display_lcd_xy_logic COMMAND test_display_lcd_xy_logic)

add_library(test_ui_ctrl_compile_obj OBJECT ../src/ui/ui_ctrl.c)
target_compile_definitions(test_ui_ctrl_compile_obj PRIVATE HOST_TEST)
target_include_directories(test_ui_ctrl_compile_obj PRIVATE .. ../include ../src .)
target_compile_options(test_ui_ctrl_compile_obj PRIVATE -include hal_stubs.h)

add_executable(test_ui_ctrl_compile
  test_ui_ctrl_compile_stubs.c
)
target_compile_definitions(test_ui_ctrl_compile PRIVATE HOST_TEST)
target_link_libraries(test_ui_ctrl_compile PRIVATE test_ui_ctrl_compile_obj)
target_include_directories(test_ui_ctrl_compile PRIVATE . .. ../include ../src)
add_test(NAME test_ui_ctrl_compile COMMAND test_ui_ctrl_compile)
