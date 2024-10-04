#pragma once

#include "driver/spi_master.h"

class GPIOPin {
    int pin_;
    gpio_mode_t mode_;
    bool inverted_;

public:
    GPIOPin(int pin, gpio_mode_t mode, bool inverted = false) : pin_(pin), mode_(mode), inverted_(inverted) {}

    int get_pin() { return pin_; }
    gpio_mode_t get_mode() { return mode_; }
    bool get_inverted() { return inverted_; }
    bool digital_read();
    void digital_write(bool value);
    void setup();
};
