#pragma once

#include "waveshare_it8951.h"

class Device {
public:
    Device() {}

    bool begin();
    void process();

private:
    void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);

    WaveshareIT8951 _display{};
    WaveshareIT8951Frame _frame{};
    bool _flushing{false};
};
