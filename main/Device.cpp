#include "includes.h"

#include "Device.h"

#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS 2

#define PIN_HRDY 8
#define PIN_RST 9
#define PIN_CS 10
#define PIN_SCLK 12
#define PIN_MISO 13
#define PIN_MOSI 11

LOG_TAG(Device);

void Device::process() {
    // raise the task priority of LVGL and/or reduce the handler period can improve the
    // performance

    vTaskDelay(pdMS_TO_TICKS(10));

    // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
    lv_timer_handler();
}

void Device::flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    ESP_ERROR_ASSERT(LV_COLOR_DEPTH == 8);

    ESP_LOGD(TAG, "Updating display %dx%d %dx%d", area->x1, area->y1, area->x2, area->y2);

    if (!_flushing) {
        _flushing = true;

        _frame = {
            .area{
                .x = 0,
                .y = 0,
                .w = _display.get_width(),
                .h = _display.get_height(),
            },
            .target_memory_address = _display.get_memory_address(),
            .bpp = 4,
            .hold = true,
        };

        _display.update_start(_frame);
    }

    const auto width = _display.get_width();
    const auto height = (area->y2 - area->y1) + 1;
    const auto buffer = _display.get_buffer();
    const auto buffer_len = _display.get_buffer_len();
    size_t buffer_offset = 0;

    for (lv_coord_t y = 0; y < height; y++) {
        for (lv_coord_t x = width - 2; x >= 0; x -= 2) {
            const auto b1 = color_p[y * width + x].ch.red;
            const auto b2 = color_p[y * width + x + 1].ch.red;

            const auto b = b1 << 1 | b2 << 5;

            buffer[buffer_offset++] = b;

            if (buffer_offset >= buffer_len) {
                _display.update_write_buffer(buffer_offset);
                buffer_offset = 0;
            }
        }
    }

    if (buffer_offset > 0) {
        _display.update_write_buffer(buffer_offset);
    }

    if (lv_disp_flush_is_last(disp_drv)) {
        _display.update_end(_frame);

        _flushing = false;
    }

    lv_disp_flush_ready(disp_drv);
}

bool Device::begin() {
    _display.set_cs_pin(new GPIOPin(PIN_CS, GPIO_MODE_OUTPUT));
    _display.set_sclk_pin(new GPIOPin(PIN_SCLK, GPIO_MODE_OUTPUT));
    _display.set_miso_pin(new GPIOPin(PIN_MISO, GPIO_MODE_INPUT));
    _display.set_mosi_pin(new GPIOPin(PIN_MOSI, GPIO_MODE_OUTPUT));
    _display.set_busy_pin(new GPIOPin(PIN_HRDY, GPIO_MODE_INPUT, true /* inverted */));
    _display.set_reset_pin(new GPIOPin(PIN_RST, GPIO_MODE_OUTPUT));

    _display.setup(-1.15f);

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

    const int draw_buffer_lines = 100;
    const size_t draw_buffer_pixels = sizeof(lv_color_t) * _display.get_width() * draw_buffer_lines;
    const size_t draw_buffer_size = sizeof(lv_color_t) * draw_buffer_pixels;

    ESP_LOGI(TAG, "Allocating %d Kb for draw buffer", (draw_buffer_size) / 1024);

    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

    static lv_disp_draw_buf_t draw_buffer_dsc;
    auto draw_buffer = (lv_color_t*)heap_caps_malloc(draw_buffer_size, MALLOC_CAP_INTERNAL);
    if (!draw_buffer) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer");
        esp_restart();
    }

    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

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

    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Device initialization complete");

    return true;
}
