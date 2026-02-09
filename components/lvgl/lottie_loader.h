#pragma once

#ifdef USE_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstring>
#include <lvgl.h>

namespace esphome {
namespace lvgl {

static const char *const LOTTIE_TAG = "lottie";
static constexpr size_t LOTTIE_TASK_STACK_SIZE = 64 * 1024;

// Persistent context for each Lottie widget – tracks all PSRAM allocations
// and the render task, so resources are freed on screen unload and
// re-created on screen load.
struct LottieContext {
    // --- Config (set once, never freed) ---
    lv_obj_t *obj;
    const void *data;           // PROGMEM (embedded) or nullptr
    size_t data_size;
    const char *file_path;      // string literal or nullptr
    bool loop;
    bool auto_start;
    uint32_t width;
    uint32_t height;

    // --- Runtime state (freed on screen unload) ---
    uint8_t *pixel_buffer;      // PSRAM – width*height*4
    StackType_t *task_stack;    // PSRAM – 64 KB
    StaticTask_t *task_tcb;     // internal RAM
    TaskHandle_t task_handle;
    volatile bool stop_requested;
};

// --------------------------------------------------------------------------
// Render task: loads Lottie data, captures LVGL animation parameters,
// deletes the LVGL animation, and drives frame rendering on our 64 KB
// PSRAM stack.  Checks stop_requested each frame; suspends when done.
// --------------------------------------------------------------------------
inline void lottie_load_task(void *param) {
    LottieContext *ctx = (LottieContext *)param;

    // Wait for LVGL to be fully running
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(LOTTIE_TAG, "Loading lottie animation data...");

    // --- PHASE 1: Load data (ThorVG JSON parsing – needs large stack) ---
    lv_lock();

    if (ctx->data != nullptr) {
        lv_lottie_set_src_data(ctx->obj, ctx->data, ctx->data_size);
        ESP_LOGI(LOTTIE_TAG, "Data loaded from embedded source (%d bytes)", (int)ctx->data_size);
    } else if (ctx->file_path != nullptr) {
        lv_lottie_set_src_file(ctx->obj, ctx->file_path);
        ESP_LOGI(LOTTIE_TAG, "Data loaded from file: %s", ctx->file_path);
    }

    // --- PHASE 2: Capture animation parameters ---
    lv_anim_t *anim = lv_lottie_get_anim(ctx->obj);

    lv_anim_exec_xcb_t exec_cb = nullptr;
    void *anim_var = nullptr;
    int32_t start_frame = 0;
    int32_t end_frame = 0;
    uint32_t duration_ms = 0;

    if (anim != nullptr) {
        exec_cb      = anim->exec_cb;
        anim_var     = anim->var;
        start_frame  = anim->start_value;
        end_frame    = anim->end_value;
        duration_ms  = (uint32_t)lv_anim_get_time(anim);

        ESP_LOGI(LOTTIE_TAG, "Anim: frames %d..%d, duration %u ms",
                 (int)start_frame, (int)end_frame, (unsigned)duration_ms);

        // DELETE the LVGL animation – we drive rendering ourselves.
        lv_anim_delete(anim_var, exec_cb);
        ESP_LOGI(LOTTIE_TAG, "LVGL anim removed – rendering from PSRAM task");
    } else {
        ESP_LOGE(LOTTIE_TAG, "Animation INVALID – parsing may have failed!");
    }

    // Show widget
    lv_obj_remove_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    lv_unlock();

    // If parsing failed or auto_start is off, suspend
    if (exec_cb == nullptr || duration_ms == 0 || end_frame <= start_frame) {
        ESP_LOGW(LOTTIE_TAG, "No valid animation, task suspending");
        vTaskSuspend(NULL);
        return;
    }
    if (!ctx->auto_start) {
        ESP_LOGI(LOTTIE_TAG, "auto_start=false, task suspending");
        vTaskSuspend(NULL);
        return;
    }

    // --- PHASE 3: Frame render loop (64 KB PSRAM stack) ---
    int32_t total_frames = end_frame - start_frame;
    uint32_t frame_delay_ms = duration_ms / (uint32_t)total_frames;
    if (frame_delay_ms < 16)  frame_delay_ms = 16;
    if (frame_delay_ms > 100) frame_delay_ms = 100;

    ESP_LOGI(LOTTIE_TAG, "Render loop: %u ms/frame, loop=%d",
             (unsigned)frame_delay_ms, (int)ctx->loop);

    TickType_t start_tick = xTaskGetTickCount();

    while (!ctx->stop_requested) {
        uint32_t elapsed_ms = (uint32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);

        int32_t frame;
        if (ctx->loop) {
            uint32_t phase = elapsed_ms % duration_ms;
            frame = start_frame + (int32_t)((int64_t)total_frames * phase / duration_ms);
        } else {
            if (elapsed_ms >= duration_ms) {
                lv_lock();
                exec_cb(anim_var, end_frame);
                lv_unlock();
                ESP_LOGI(LOTTIE_TAG, "Animation complete");
                break;
            }
            frame = start_frame + (int32_t)((int64_t)total_frames * elapsed_ms / duration_ms);
        }

        lv_lock();
        exec_cb(anim_var, frame);
        lv_unlock();

        vTaskDelay(pdMS_TO_TICKS(frame_delay_ms));
    }

    if (ctx->stop_requested) {
        ESP_LOGI(LOTTIE_TAG, "Stop requested – task suspending");
    }

    // Suspend (NOT delete) – cleanup callback will delete us safely
    vTaskSuspend(NULL);
}

// --------------------------------------------------------------------------
// Free all PSRAM/internal-RAM resources for one Lottie widget.
// --------------------------------------------------------------------------
inline void lottie_free_resources(LottieContext *ctx) {
    // Signal the task to stop (in case it's still in the render loop)
    ctx->stop_requested = true;

    // Brief yield to let the task see the flag and suspend
    // (the task is either in vTaskDelay or blocked on lv_lock)
    // Since we hold lv_lock here (called from LVGL event), the task
    // will be blocked on lv_lock if it tries to acquire it.
    // vTaskDelete is safe: the task is either suspended, in vTaskDelay,
    // or blocked on lv_lock – none of these hold the lock.
    if (ctx->task_handle) {
        vTaskDelete(ctx->task_handle);
        ctx->task_handle = nullptr;
    }
    if (ctx->task_stack)    { heap_caps_free(ctx->task_stack);    ctx->task_stack = nullptr; }
    if (ctx->task_tcb)      { heap_caps_free(ctx->task_tcb);      ctx->task_tcb = nullptr; }
    if (ctx->pixel_buffer)  { heap_caps_free(ctx->pixel_buffer);  ctx->pixel_buffer = nullptr; }
    ctx->stop_requested = false;

    ESP_LOGI(LOTTIE_TAG, "Lottie PSRAM freed (%ux%u = %u KB + 64 KB stack)",
             (unsigned)ctx->width, (unsigned)ctx->height,
             (unsigned)(ctx->width * ctx->height * 4 / 1024));
}

// --------------------------------------------------------------------------
// (Re-)allocate pixel buffer and launch the render task.
// Must be called under lv_lock.
// --------------------------------------------------------------------------
inline bool lottie_launch(LottieContext *ctx) {
    // Allocate pixel buffer in PSRAM
    size_t buf_bytes = (size_t)ctx->width * ctx->height * 4;
    ctx->pixel_buffer = (uint8_t *)heap_caps_malloc(
        buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ctx->pixel_buffer) {
        ESP_LOGE(LOTTIE_TAG, "PSRAM alloc failed (%u bytes)", (unsigned)buf_bytes);
        return false;
    }
    memset(ctx->pixel_buffer, 0, buf_bytes);

    // Re-set the Lottie buffer
    lv_lottie_set_buffer(ctx->obj, ctx->width, ctx->height, ctx->pixel_buffer);

    // Hide until data is loaded by the task
    lv_obj_add_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    // Allocate task stack + TCB
    ctx->task_stack = (StackType_t *)heap_caps_malloc(
        LOTTIE_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ctx->task_tcb = (StaticTask_t *)heap_caps_malloc(
        sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ctx->task_stack || !ctx->task_tcb) {
        ESP_LOGE(LOTTIE_TAG, "Task alloc failed");
        lottie_free_resources(ctx);
        return false;
    }

    ctx->stop_requested = false;
    ctx->task_handle = xTaskCreateStatic(
        lottie_load_task, "lottie_anim",
        LOTTIE_TASK_STACK_SIZE / sizeof(StackType_t),
        ctx, 5, ctx->task_stack, ctx->task_tcb);

    if (!ctx->task_handle) {
        lottie_free_resources(ctx);
        return false;
    }

    ESP_LOGI(LOTTIE_TAG, "Lottie task launched (%u KB PSRAM stack)",
             (unsigned)(LOTTIE_TASK_STACK_SIZE / 1024));
    return true;
}

// --------------------------------------------------------------------------
// Screen event callbacks – two-phase unload to avoid drawing freed buffer
// during screen transition animation.
//
//   SCREEN_UNLOAD_START  → stop task + hide widget (LVGL still draws screen)
//   SCREEN_UNLOADED      → free PSRAM (screen no longer visible)
//   SCREEN_LOADED        → re-allocate and re-launch
// --------------------------------------------------------------------------
inline void lottie_screen_unload_start_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);

    // Stop the render task immediately
    ctx->stop_requested = true;
    if (ctx->task_handle) {
        vTaskDelete(ctx->task_handle);
        ctx->task_handle = nullptr;
    }

    // Hide widget so LVGL won't try to draw the image during transition
    lv_obj_add_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(LOTTIE_TAG, "Lottie task stopped, widget hidden (transition starting)");
}

inline void lottie_screen_unloaded_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);

    // Now safe to free – screen is no longer visible
    if (ctx->task_stack)    { heap_caps_free(ctx->task_stack);    ctx->task_stack = nullptr; }
    if (ctx->task_tcb)      { heap_caps_free(ctx->task_tcb);      ctx->task_tcb = nullptr; }
    if (ctx->pixel_buffer)  { heap_caps_free(ctx->pixel_buffer);  ctx->pixel_buffer = nullptr; }
    ctx->stop_requested = false;

    ESP_LOGI(LOTTIE_TAG, "Lottie PSRAM freed (%ux%u = %u KB + 64 KB stack)",
             (unsigned)ctx->width, (unsigned)ctx->height,
             (unsigned)(ctx->width * ctx->height * 4 / 1024));
}

inline void lottie_screen_loaded_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);
    if (ctx->pixel_buffer == nullptr) {
        lottie_launch(ctx);
    }
}

// --------------------------------------------------------------------------
// Public API: initialise Lottie widget – allocate buffer, register screen
// events, and launch the load/render task.
// Call under lv_lock (from LVGL init code).
// --------------------------------------------------------------------------
inline bool lottie_init(lv_obj_t *obj, const void *data, size_t data_size,
                         const char *file_path, uint32_t width, uint32_t height,
                         bool loop, bool auto_start) {
    LottieContext *ctx = (LottieContext *)heap_caps_malloc(
        sizeof(LottieContext), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ctx) return false;
    memset(ctx, 0, sizeof(LottieContext));

    ctx->obj       = obj;
    ctx->data      = data;
    ctx->data_size = data_size;
    ctx->file_path = file_path;
    ctx->loop      = loop;
    ctx->auto_start = auto_start;
    ctx->width     = width;
    ctx->height    = height;

    // Register screen events for PSRAM lifecycle (two-phase unload)
    lv_obj_t *screen = lv_obj_get_screen(obj);
    lv_obj_add_event_cb(screen, lottie_screen_unload_start_cb,
                        LV_EVENT_SCREEN_UNLOAD_START, ctx);
    lv_obj_add_event_cb(screen, lottie_screen_unloaded_cb,
                        LV_EVENT_SCREEN_UNLOADED, ctx);
    lv_obj_add_event_cb(screen, lottie_screen_loaded_cb,
                        LV_EVENT_SCREEN_LOADED, ctx);

    return lottie_launch(ctx);
}

}  // namespace lvgl
}  // namespace esphome

#endif  // USE_ESP32
