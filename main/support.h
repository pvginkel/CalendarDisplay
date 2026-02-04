#pragma once

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define ESP_TIMER_MS(v) ((v) * 1000)
#define ESP_TIMER_SECONDS(v) ESP_TIMER_MS((v) * 1000)

int getisoweek(tm& time_info);

#define esp_get_millis() uint32_t(esp_timer_get_time() / 1000ull)
