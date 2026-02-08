/**
 * @file lv_draw_ppa.c
 * Fixed PPA draw unit for LVGL 9.4 on ESP32-P4
 * Backported from https://github.com/lvgl/lvgl/pull/9162
 * Adapted for C++ compilation (ESPHome build system)
 */

#include "sdkconfig.h"
#ifdef CONFIG_SOC_PPA_SUPPORTED

#include "lv_draw_ppa_private.h"
#include "lv_draw_ppa.h"

/*********************
 *      DEFINES
 *********************/
#define DRAW_UNIT_ID_PPA 120
#define PPA_BUF_ALIGN     16  /* PPA needs at least 16-byte aligned buffers (128-bit burst) */

/* Check if a draw buffer is suitable for PPA (non-NULL, aligned, has data) */
static inline bool ppa_buf_usable(lv_draw_buf_t * buf)
{
    if(buf == NULL || buf->data == NULL || buf->data_size == 0) return false;
    if(((uintptr_t)buf->data) % PPA_BUF_ALIGN != 0) return false;
    return true;
}

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t ppa_evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t ppa_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static int32_t ppa_delete(lv_draw_unit_t * draw_unit);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_ppa_init(void)
{
    lv_draw_ppa_unit_t * draw_ppa_unit = (lv_draw_ppa_unit_t *)lv_draw_create_unit(sizeof(lv_draw_ppa_unit_t));
    draw_ppa_unit->base_unit.evaluate_cb = ppa_evaluate;
    draw_ppa_unit->base_unit.dispatch_cb = ppa_dispatch;
    draw_ppa_unit->base_unit.delete_cb = ppa_delete;

    LV_LOG_INFO("PPA draw unit registered (diagnostic mode - no PPA ops)");

    /*
     * DIAGNOSTIC: Skip PPA client registration for now.
     * If the crash stops, the issue is in PPA operations.
     * If the crash persists, the issue is in lv_draw_create_unit() or
     * how LVGL handles the new draw unit with LV_USE_OS=LV_OS_FREERTOS.
     */
}

void lv_draw_ppa_deinit(void)
{
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* DIAGNOSTIC: Always return 0 - never claim any task */
static int32_t ppa_evaluate(lv_draw_unit_t * u, lv_draw_task_t * t)
{
    LV_UNUSED(u);
    LV_UNUSED(t);
    return 0;
}

/* DIAGNOSTIC: Always return IDLE - never dispatch */
static int32_t ppa_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    LV_UNUSED(draw_unit);
    LV_UNUSED(layer);
    return LV_DRAW_UNIT_IDLE;
}

static int32_t ppa_delete(lv_draw_unit_t * draw_unit)
{
    LV_UNUSED(draw_unit);
    return 0;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
