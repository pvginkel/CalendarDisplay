#include "includes.h"

#include "Device.h"

#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS 2

LOG_TAG(Device);

void Device::process() {
    // raise the task priority of LVGL and/or reduce the handler period can improve the
    // performance

    vTaskDelay(pdMS_TO_TICKS(10));

    // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
    lv_timer_handler();
}

void Device::set_on(bool on) {
    if (on == _on) {
        return;
    }

    _on = on;

    if (on) {
        ESP_LOGI(TAG, "Turning screen on");

        _display.set_system_run();
    } else {
        ESP_LOGI(TAG, "Turning screen off");

        _display.set_sleep();
    }
}

void Device::flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    ESP_ERROR_ASSERT(LV_COLOR_DEPTH == 16);

    ESP_LOGD(TAG, "Updating display %dx%d %dx%d", area->x1, area->y1, area->x2, area->y2);

    set_on(true);

    const uint16_t display_width = (area->x2 - area->x1) + 1;
    const uint16_t display_height = disp_drv->hor_res;

    if (!_flushing) {
        _flushing = true;
        _flush_start = esp_get_millis();

        IT8951Area area = {
            .x = 0,
            .y = 0,
            .w = display_width,
            .h = display_height,
        };

        _display.load_image_start(area, _display.get_memory_address(), IT8951_ROTATE_90, IT8951_PIXEL_FORMAT_4BPP);
    }

    const auto width = display_width;
    const auto height = (area->y2 - area->y1) + 1;
    const auto buffer_len = _display.get_buffer_len();

    auto buffer = _display.get_buffer();
    size_t buffer_offset = 0;

    for (lv_coord_t y = 0; y < height; y++) {
        for (lv_coord_t x = width - 2; x >= 0; x -= 2) {
            const auto b1 = color_p[y * width + x].ch.red >> 1;
            const auto b2 = color_p[y * width + x + 1].ch.red >> 1;

            buffer[buffer_offset++] = b1 | b2 << 4;

            if (buffer_offset >= buffer_len) {
                _display.load_image_flush_buffer(buffer_offset);
                buffer = _display.get_buffer();
                buffer_offset = 0;
            }
        }
    }

    if (buffer_offset > 0) {
        _display.load_image_flush_buffer(buffer_offset);
    }

    auto is_last = lv_disp_flush_is_last(disp_drv);

    if (is_last) {
        _display.load_image_end();

        IT8951Area area = {
            .x = 0,
            .y = 0,
            .w = display_height,
            .h = display_width,
        };

        _display.display_area(area, _display.get_memory_address(), IT8951_PIXEL_FORMAT_4BPP, IT8951_DISPLAY_MODE_GC16);

        _flushing = false;

        auto flush_end = esp_get_millis();
        ESP_LOGI(TAG, "Screen updated in %" PRIu32 " ms", flush_end - _flush_start);
    }

    lv_disp_flush_ready(disp_drv);

    if (is_last && _standby_after_next_paint) {
        _standby_after_next_paint = false;

        set_on(false);
    }
}

bool Device::begin() {
    _display.setup(-1.15f);

    _display.clear_screen();

    lv_init();

    ESP_LOGI(TAG, "Install LVGL tick timer");

    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void* arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); },
        .name = "lvgl_tick",
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, ESP_TIMER_MS(LVGL_TICK_PERIOD_MS)));

    const int draw_buffer_lines = 40;
    const size_t draw_buffer_pixels = _display.get_width() * draw_buffer_lines;
    const size_t draw_buffer_size = sizeof(lv_color_t) * draw_buffer_pixels;

    ESP_LOGI(TAG, "Allocating %dKb for draw buffer", (draw_buffer_size) / 1024);

    static lv_disp_draw_buf_t draw_buffer_dsc;
    auto draw_buffer = (lv_color_t*)heap_caps_malloc(draw_buffer_size, MALLOC_CAP_INTERNAL);
    if (!draw_buffer) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer");
        esp_restart();
    }

    lv_disp_draw_buf_init(&draw_buffer_dsc, draw_buffer, nullptr, draw_buffer_pixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = _display.get_width();
    disp_drv.ver_res = _display.get_height();

    disp_drv.flush_cb = [](lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
        ((Device*)disp_drv->user_data)->flush_cb(disp_drv, area, color_p);
    };
    disp_drv.user_data = this;

    disp_drv.draw_buf = &draw_buffer_dsc;

    disp_drv.full_refresh = 1;
    disp_drv.dpi = LV_DPI_DEF;
    disp_drv.rotated = LV_DISP_ROT_90;

    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Device initialization complete");

    return true;
}
