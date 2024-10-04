#include "includes.h"

#include "gpiopin.h"

bool GPIOPin::digital_read() { return gpio_get_level((gpio_num_t)pin_) == 0 ? inverted_ : !inverted_; }

void GPIOPin::digital_write(bool value) { gpio_set_level((gpio_num_t)pin_, value ? !inverted_ : inverted_); }

void GPIOPin::setup() {
    gpio_config_t i_conf = {
        .pin_bit_mask = 1ull << pin_,
        .mode = mode_,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));
}
