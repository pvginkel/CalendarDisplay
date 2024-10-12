#include "includes.h"

#include "Buttons.h"

void IRAM_ATTR Buttons::gpio_isr_handler(void* arg) {
    auto button = (Button*)arg;
    auto pin = (uint32_t)button->pin;

    xQueueSendFromISR(button->parent->_queue, &pin, nullptr);
}

void Buttons::begin() {
    gpio_config_t i_conf = {
        .pin_bit_mask = 1ull << CONFIG_DEVICE_BUTTON1_PIN | 1ull << CONFIG_DEVICE_BUTTON2_PIN |
                        1ull << CONFIG_DEVICE_BUTTON3_PIN | 1ull << CONFIG_DEVICE_BUTTON4_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));

    _queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(
        [](void* arg) {
            auto self = (Buttons*)arg;
            uint32_t pin;

            while (true) {
                if (xQueueReceive(self->_queue, &pin, portMAX_DELAY)) {
                    auto level = gpio_get_level((gpio_num_t)pin);
                    auto clicked = !level;
                    auto current_millis = esp_get_millis();

                    if (clicked && current_millis - self->_last_millis > 10) {
                        switch (pin) {
                            case CONFIG_DEVICE_BUTTON1_PIN:
                                self->_next_page.call();
                                break;
                            case CONFIG_DEVICE_BUTTON2_PIN:
                                self->_previous_page.call();
                                break;
                            case CONFIG_DEVICE_BUTTON3_PIN:
                                self->_home.call();
                                break;
                            case CONFIG_DEVICE_BUTTON4_PIN:
                                self->_off.call();
                                break;
                        }
                    }

                    self->_last_millis = current_millis;
                }
            }
        },
        "buttons_gpio_task", 2048, this, 10, nullptr);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)CONFIG_DEVICE_BUTTON1_PIN, gpio_isr_handler,
                                         new Button{.parent = this, .pin = CONFIG_DEVICE_BUTTON1_PIN}));

    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)CONFIG_DEVICE_BUTTON2_PIN, gpio_isr_handler,
                                         new Button{.parent = this, .pin = CONFIG_DEVICE_BUTTON2_PIN}));

    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)CONFIG_DEVICE_BUTTON3_PIN, gpio_isr_handler,
                                         new Button{.parent = this, .pin = CONFIG_DEVICE_BUTTON3_PIN}));

    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)CONFIG_DEVICE_BUTTON4_PIN, gpio_isr_handler,
                                         new Button{.parent = this, .pin = CONFIG_DEVICE_BUTTON4_PIN}));
}
