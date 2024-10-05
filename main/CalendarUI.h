#pragma once

#include "CalendarEventsDto.h"
#include "Device.h"
#include "LvglUI.h"

class CalendarUI : public LvglUI {
public:
    CalendarUI(Device* device) : _device(device) {}

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
    void create_day(lv_obj_t* parent, int weekday, uint8_t col, uint8_t row);
    uint32_t calculate_hash(const char* str);

#ifndef LV_SIMULATOR
    void do_update() override;
    void update_data();
#endif

    CalendarEventsDto _data{};
    Device* _device;
    uint32_t _last_hash;
#ifndef LV_SIMULATOR
    time_t _next_update{0};
#endif
};
