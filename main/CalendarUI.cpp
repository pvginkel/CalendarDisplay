#include "includes.h"

#include "CalendarUI.h"

#include <cassert>
#include <chrono>
#include <ctime>

#include "Fonts.h"
#include "Messages.h"
#include "lv_support.h"

LOG_TAG(CalendarUI);

void CalendarUI::do_begin() { LvglUI::do_begin(); }

#ifndef LV_SIMULATOR

void CalendarUI::do_update() {
    auto current_time = time(nullptr);

    if (_next_update == 0 || current_time >= _next_update) {
        if (_next_update == 0) {
            // Get the time_t for the start of the hour.
            auto start_hour_time_info = *localtime(&current_time);
            start_hour_time_info.tm_min = 0;
            start_hour_time_info.tm_sec = 0;
            auto start_hour_time = mktime(&start_hour_time_info);

            // Calculate the number of seconds from the current time to
            // the next update interval.
            auto diff = current_time - start_hour_time;
            auto rounded_up =
                ((diff + CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL - 1) / CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL) *
                CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL;

            // Set the next update to the correct time. After the initial
            // calculation, every other update will be calculated simply
            // by adding the update interval.
            _next_update = start_hour_time + rounded_up;

            // Subtract 10 seconds to better time the update on the interval.
            // The update completes in roughly 15 seconds.
            _next_update -= 10;
        } else {
            _next_update += CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL;
        }

        update_data();
    }
}

void CalendarUI::update_data() {
    esp_http_client_config_t config = {
        .url = CONFIG_INFRA_STATISTICS_ENDPOINT,
        .timeout_ms = CONFIG_INFRA_STATISTICS_ENDPOINT_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Downloading statistics from %s", config.url);

    string json;
    auto err = esp_http_download_string(config, json, 128 * 1024);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download statistics");
        return;
    }

    if (!CalendarEventsDto::from_json(json.c_str(), _data)) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    ESP_LOGI(TAG, "Updating screen");

    render();
}

#endif

void CalendarUI::do_render(lv_obj_t* parent) {
    auto outer_cont = lv_obj_create(parent);
    reset_outer_container_styles(outer_cont);
    static lv_coord_t outer_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t outer_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(outer_cont, outer_cont_col_desc, outer_cont_row_desc);

    auto left_header = format("%.3s", get_month(_data.start.month));
    if (_data.start.month != _data.end.month) {
        left_header += format(" - %.3s", get_month(_data.end.month));
    }

    auto left_header_label = lv_label_create(outer_cont);
    lv_label_set_text(left_header_label, left_header.c_str());
    lv_obj_set_style_text_font(left_header_label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(left_header_label, LV_GRID_ALIGN_START, 0, 3, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_style_pad_hor(left_header_label, lv_dpx(10), LV_PART_MAIN);

    tm start_time_info;
    _data.start.to_time_info(start_time_info);

    auto start_time = mktime(&start_time_info);
    localtime_r(&start_time, &start_time_info);

    char week_str[3];
    strftime(week_str, sizeof(week_str), "%V", &start_time_info);

    auto right_header = format(MSG_WEEK " %s", week_str);

    auto right_header_label = lv_label_create(outer_cont);
    lv_label_set_text(right_header_label, right_header.c_str());
    lv_obj_set_style_text_font(right_header_label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(right_header_label, LV_GRID_ALIGN_END, 0, 3, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_style_pad_hor(right_header_label, lv_dpx(10), LV_PART_MAIN);

    auto hor_line_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(hor_line_cont);
    lv_obj_set_style_pad_top(hor_line_cont, lv_dpx(6), LV_PART_MAIN);
    lv_obj_set_grid_cell(hor_line_cont, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_START, 1, 1);

    auto hor_line = lv_obj_create(hor_line_cont);
    lv_obj_remove_style_all(hor_line);
    lv_obj_set_style_bg_opa(hor_line, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hor_line, color_make(0.4f), LV_PART_MAIN);
    lv_obj_set_size(hor_line, LV_PCT(100), lv_dpx(4));

    auto left_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(left_cont);
    static lv_coord_t left_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t left_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1),   LV_GRID_CONTENT,
                                              LV_GRID_FR(1),   LV_GRID_CONTENT, LV_GRID_FR(1),
                                              LV_GRID_CONTENT, LV_GRID_FR(1),   LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(left_cont, left_cont_col_desc, left_cont_row_desc);
    lv_obj_set_grid_cell(left_cont, LV_GRID_ALIGN_STRETCH, 0, LV_GRID_ALIGN_STRETCH, 2);

    auto ver_line_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(ver_line_cont);
    lv_obj_set_style_pad_hor(ver_line_cont, lv_dpx(12), LV_PART_MAIN);
    lv_obj_set_grid_cell(ver_line_cont, LV_GRID_ALIGN_START, 1, LV_GRID_ALIGN_STRETCH, 2);

    auto ver_line = lv_obj_create(ver_line_cont);
    lv_obj_remove_style_all(ver_line);
    lv_obj_set_style_bg_opa(ver_line, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ver_line, color_make(0.4f), LV_PART_MAIN);
    lv_obj_set_size(ver_line, lv_dpx(4), LV_PCT(100));

    auto right_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(right_cont);
    static lv_coord_t right_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t right_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,      LV_GRID_FR(1),
                                               LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(right_cont, right_cont_col_desc, right_cont_row_desc);
    lv_obj_set_grid_cell(right_cont, LV_GRID_ALIGN_STRETCH, 2, LV_GRID_ALIGN_STRETCH, 2);

    create_day(left_cont, 0, 0, 0);
    create_day(left_cont, 1, 0, 2);
    create_day(left_cont, 2, 0, 4);
    create_day(left_cont, 3, 0, 6);
    create_day(right_cont, 4, 0, 0);
    create_day(right_cont, 5, 0, 2);
    create_day(right_cont, 6, 0, 4);
}

void CalendarUI::create_day(lv_obj_t* parent, int weekday, uint8_t col, uint8_t row) {
    // Format the date, correctly offset for the day we're creating.

    tm time_info;
    _data.start.to_time_info(time_info);

    auto time = mktime(&time_info);

    time += (time_t)weekday * 24 * 3600;

    localtime_r(&time, &time_info);

    auto weekday_name = get_weekday(weekday);

    auto year = time_info.tm_year + 1900;
    auto month = time_info.tm_mon + 1;
    auto day = time_info.tm_mday;

    auto header = format("%s %d", weekday_name, day);

    // Render the events.

    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_START, row);
    lv_obj_set_style_pad_row(cont, lv_dpx(8), LV_PART_MAIN);

    auto label = lv_label_create(cont);
    lv_label_set_text(label, header.c_str());
    lv_obj_set_style_text_font(label, SMALL_MEDIUM_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(label, lv_dpx(10), LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, lv_dpx(10), LV_PART_MAIN);

    auto had_all_day = false;

    for (auto& event : _data.events) {
        if (event.start.year == year && event.start.month == month && event.start.day == day && !event.start.has_time) {
            create_event(cont, event);
            had_all_day = true;
        }
    }

    for (auto& event : _data.events) {
        if (event.start.year == year && event.start.month == month && event.start.day == day && event.start.has_time) {
            if (had_all_day) {
                had_all_day = false;

                auto hor_line = lv_obj_create(cont);
                lv_obj_remove_style_all(hor_line);
                lv_obj_set_style_bg_opa(hor_line, LV_OPA_100, LV_PART_MAIN);
                lv_obj_set_style_bg_color(hor_line, color_make(0.7f), LV_PART_MAIN);
                lv_obj_set_size(hor_line, LV_PCT(100), lv_dpx(3));
            }

            create_event(cont, event);
        }
    }
}

void CalendarUI::create_event(lv_obj_t* parent, const CalendarEventDto& value) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_style_radius(cont, lv_dpx(8), LV_PART_MAIN);
    lv_obj_set_style_clip_corner(cont, true, LV_PART_MAIN);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, color_make(value.calendar.color), LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_color(cont, lv_color_white(), LV_PART_MAIN);

    auto event_text = value.summary;
#if 0
	event_text += format(" - %s", value.calendar.name.c_str());
#endif

    if (value.instance.instance != 0) {
        event_text += format(" " MSG_DAY_OF, value.instance.instance, value.instance.total);
    }

    auto summary_label = lv_label_create(cont);
    lv_label_set_text(summary_label, event_text.c_str());
    lv_obj_set_style_text_font(summary_label, XSMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(summary_label, lv_dpx(10), LV_PART_MAIN);
    lv_obj_set_style_pad_top(summary_label, lv_dpx(6), LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(summary_label, lv_dpx(3), LV_PART_MAIN);
    lv_obj_set_size(summary_label, LV_PCT(100), LV_SIZE_CONTENT);

    if (value.start.has_time) {
        auto start = format("%d:%02d", value.start.hour, value.start.minute);
        auto end = format("%d:%02d", value.end.hour, value.end.minute);
        string time_text;

        if (value.instance.instance == 0) {
            if (start == end) {
                time_text = start;
            } else {
                time_text = start + " - " + end;
            }
        } else if (value.instance.instance == 1) {
            time_text = start;
        } else if (value.instance.instance == value.instance.total) {
            time_text = MSG_UNTIL " " + end;
        }

        if (!time_text.empty()) {
            auto time_label = lv_label_create(cont);
            lv_label_set_text(time_label, time_text.c_str());
            lv_obj_set_style_text_font(time_label, XSMALL_FONT, LV_PART_MAIN);
            lv_obj_set_style_pad_hor(time_label, lv_dpx(10), LV_PART_MAIN);
            lv_obj_set_style_pad_bottom(time_label, lv_dpx(3), LV_PART_MAIN);
            lv_obj_set_size(time_label, LV_PCT(100), LV_SIZE_CONTENT);

            // Reset the bottom padding of the summary label.
            lv_obj_set_style_pad_bottom(summary_label, 0, LV_PART_MAIN);
        }
    }
}

lv_color_t CalendarUI::color_make(float color) {
    auto value = (uint8_t)(255 * (1.0f - color));

    return lv_color_make(value, value, value);
}

const char* CalendarUI::get_weekday(int weekday) {
    switch (weekday) {
        case 0:
            return MSG_MONDAY;
        case 1:
            return MSG_TUESDAY;
        case 2:
            return MSG_WEDNESDAY;
        case 3:
            return MSG_THURSDAY;
        case 4:
            return MSG_FRIDAY;
        case 5:
            return MSG_SATURDAY;
        case 6:
            return MSG_SUNDAY;
        default:
            assert(false);
            return nullptr;
    }
}

const char* CalendarUI::get_month(int month) {
    switch (month) {
        case 1:
            return MSG_JANUARY;
        case 2:
            return MSG_FEBRUARY;
        case 3:
            return MSG_MARCH;
        case 4:
            return MSG_APRIL;
        case 5:
            return MSG_MAY;
        case 6:
            return MSG_JUNE;
        case 7:
            return MSG_JULY;
        case 8:
            return MSG_AUGUST;
        case 9:
            return MSG_SEPTEMBER;
        case 10:
            return MSG_OCTOBER;
        case 11:
            return MSG_NOVEMBER;
        case 12:
            return MSG_DECEMBER;
        default:
            assert(false);
            return nullptr;
    }
}
