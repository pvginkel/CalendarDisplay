#pragma once

#include "Device.h"
#include "LvglUI.h"
#include "CalendarEventsDto.h"

class CalendarUI : public LvglUI {
    CalendarEventsDto _data;
#ifndef LV_SIMULATOR
    time_t _next_update = 0;
#endif

public:
    CalendarUI() = default;

#ifdef LV_SIMULATOR
    CalendarEventsDto& get_data() { return _data; }
#endif

protected:
    void do_begin() override;
    void do_render(lv_obj_t* parent) override;

    lv_color_t color_make(float color);
    const char* get_weekday(int weekday);
    const char* get_month(int month);
    void create_event(lv_obj_t* parent, const CalendarEventDto& value);
    void create_day(lv_obj_t* parent, int weekday, uint8_t col, uint8_t row);

#ifndef LV_SIMULATOR
    void do_update() override;
    void update_data();
#endif
};
