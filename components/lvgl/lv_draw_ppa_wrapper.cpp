/**
 * @file lv_draw_ppa_wrapper.cpp
 * Wrapper to compile the fixed PPA C source files within ESPHome's build system.
 * ESPHome only auto-compiles .cpp files from the component directory.
 * This wrapper includes all PPA .c files so they get linked properly.
 */

#include "esphome/core/defines.h"

#ifdef USE_LVGL_PPA

extern "C" {
#include "ppa/lv_draw_ppa.c"
#include "ppa/lv_draw_ppa_fill.c"
#include "ppa/lv_draw_ppa_img.c"
#include "ppa/lv_draw_ppa_buf.c"
}

#endif /* USE_LVGL_PPA */
