#pragma once

class Buttons {
    struct Button {
        Buttons* parent;
        int pin;
    };

    static void IRAM_ATTR gpio_isr_handler(void* arg);

    uint32_t _last_millis;
    QueueHandle_t _queue;
    Callback<void> _next_page;
    Callback<void> _previous_page;
    Callback<void> _home;
    Callback<void> _off;

public:
    void begin();

    void on_next_page(function<void(void)> func) { _next_page.add(func); }
    void on_previous_page(function<void(void)> func) { _previous_page.add(func); }
    void on_home(function<void(void)> func) { _home.add(func); }
    void on_off(function<void(void)> func) { _off.add(func); }
};
