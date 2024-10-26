#pragma once

#include "Buttons.h"
#include "CalendarEventsDto.h"
#include "Device.h"
#include "LvglUI.h"

class CalendarUI : public LvglUI {
    enum class orientation_t { horizontal, vertical };
    enum class week_column_t { left, right };

public:
    CalendarUI(Device* device, Buttons* buttons) : _device(device), _buttons(buttons) {}

#ifdef LV_SIMULATOR
    CalendarEventsDto& get_data() { return _data; }
#endif

protected:
    void do_begin() override;
    void do_render(lv_obj_t* parent) override;

private:
    lv_color_t color_make(int color);
    const char* get_weekday(int weekday);
    const char* get_month(int month);
    void create_event(lv_obj_t* parent, const CalendarEventDto& value);
    void create_day(lv_obj_t* parent, int weekday, uint8_t col, uint8_t row, week_column_t week_column);
    lv_obj_t* create_line(lv_obj_t* parent, orientation_t orientation, uint8_t col, uint8_t col_span, uint8_t row,
                          uint8_t row_span);
    lv_obj_t* create_line(lv_obj_t* parent, orientation_t orientation, uint8_t col, uint8_t row) {
        return create_line(parent, orientation, col, 1, row, 1);
    }
    uint32_t calculate_hash(const char* str);
    bool scroll_content(lv_obj_t* cont, week_column_t week_column);
    lv_obj_t* create_ellipsis_obj(lv_obj_t* parent, uint8_t col, uint8_t row);

#ifndef LV_SIMULATOR
    void do_update() override;
    void update_data();
    void set_offset(int offset);
#endif

    CalendarEventsDto _data{};
    Device* _device;
    Buttons* _buttons;
    uint32_t _last_hash;
    int _offset{0};
    lv_obj_t* _scroll_to_cont{nullptr};
    week_column_t _scroll_to_cont_week_column{};
#ifndef LV_SIMULATOR
    time_t _next_update{0};
    esp_timer_handle_t _home_timer;
#endif
};
