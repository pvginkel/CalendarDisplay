#pragma once

#include "it8951.h"

class Device {
public:
    Device() {}

    bool begin();
    void process();

private:
    void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
    uint8_t convert_3bpp_to_4bpp(uint8_t value);

    IT8951 _display{};
    bool _flushing{false};
};
