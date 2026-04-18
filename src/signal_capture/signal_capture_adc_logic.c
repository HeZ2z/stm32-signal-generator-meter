/* signal_capture_adc_logic.c
 *
 * 纯逻辑层，无硬件依赖。
 * 主要函数 snapshot_bounds() 以 static inline 定义在对应的 .h 中，
 * 此文件保留用于未来可测试的非内联逻辑扩展。
 */

#include "signal_capture/signal_capture_adc_logic.h"
