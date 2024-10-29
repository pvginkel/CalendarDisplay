#include "includes.h"

#include "CalendarUI.h"

#include <cassert>
#include <chrono>
#include <ctime>

#include "Fonts.h"
#include "Messages.h"
#include "lv_support.h"

LOG_TAG(CalendarUI);

void CalendarUI::do_begin() {
    LvglUI::do_begin();

#ifndef LV_SIMULATOR
    const esp_timer_create_args_t time_timer_args = {
        .callback = [](void* arg) { ((CalendarUI*)arg)->set_offset(0); },
        .arg = this,
        .name = "home_timer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&time_timer_args, &_home_timer));

    _buttons->on_next_page([this]() { set_offset(_offset + 1); });
    _buttons->on_previous_page([this]() { set_offset(_offset - 1); });
    _buttons->on_home([this]() { set_offset(0); });
    _buttons->on_off([]() { ESP_LOGI(TAG, "Turning off has not been implemented"); });
#endif
}

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
            auto rounded_up = ((diff + CONFIG_CALENDAR_UPDATE_INTERVAL - 1) / CONFIG_CALENDAR_UPDATE_INTERVAL) *
                              CONFIG_CALENDAR_UPDATE_INTERVAL;

            // Set the next update to the correct time. After the initial
            // calculation, every other update will be calculated simply
            // by adding the update interval.
            _next_update = start_hour_time + rounded_up;
        } else {
            _next_update += CONFIG_CALENDAR_UPDATE_INTERVAL;
        }

        update_data();
    }
}

void CalendarUI::update_data() {
    auto url = format(CONFIG_CALENDAR_ENDPOINT, _offset);

    esp_http_client_config_t config = {
        .url = url.c_str(),
        .timeout_ms = CONFIG_CALENDAR_ENDPOINT_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Downloading calendar from %s", config.url);

    string json;
    auto err = esp_http_download_string(config, json, 128 * 1024, "Bearer " CONFIG_CALENDAR_BEARER_TOKEN);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download calendar");
        return;
    }

    auto hash = calculate_hash(json.c_str());
    if (hash == _last_hash) {
        return;
    }

    _last_hash = hash;

    if (!CalendarEventsDto::from_json(json.c_str(), _data)) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    ESP_LOGI(TAG, "Updating screen");

    _device->standby_after_next_paint();

    render();
}

void CalendarUI::set_offset(int offset) {
    _offset = offset;

    update_data();

    if (esp_timer_is_active(_home_timer)) {
        ESP_ERROR_CHECK(esp_timer_stop(_home_timer));
    }

    if (_offset != 0) {
        ESP_ERROR_CHECK(esp_timer_start_once(_home_timer, ESP_TIMER_MS(CONFIG_DEVICE_RESET_HOME_INTERVAL * 1000)));
    }
}

#endif

void CalendarUI::do_render(lv_obj_t* parent) {
    _scroll_to_cont = nullptr;

    auto outer_cont = lv_obj_create(parent);
    reset_outer_container_styles(outer_cont);
    static lv_coord_t outer_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t outer_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(outer_cont, outer_cont_col_desc, outer_cont_row_desc);

    auto top_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(top_cont);
    static lv_coord_t top_cont_col_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t top_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(top_cont, top_cont_col_desc, top_cont_row_desc);
    lv_obj_set_grid_cell(top_cont, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_style_pad_hor(top_cont, lv_dpx(10), LV_PART_MAIN);

    auto left_header = format("%.3s", get_month(_data.start.month));
    if (_data.start.month != _data.end.month) {
        left_header += format("/%.3s", get_month(_data.end.month));
    }

    auto left_header_label = lv_label_create(top_cont);
    lv_label_set_text(left_header_label, left_header.c_str());
    lv_obj_set_style_text_font(left_header_label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(left_header_label, LV_GRID_ALIGN_START, 0, LV_GRID_ALIGN_START, 0);

    tm start_time_info;
    _data.start.to_time_info(start_time_info);
    tm end_time_info;
    _data.end.to_time_info(end_time_info);
    tm today_time_info;
    _data.today.to_time_info(today_time_info);

    auto start_week = getisoweek(start_time_info);
    auto end_week = getisoweek(end_time_info);

    auto right_header = format(MSG_WEEK " %d", start_week);
    if (start_week != end_week) {
        right_header += format("/%d", end_week);
    }

    auto is_second_half = ((start_time_info.tm_wday + 6) % 7) >= 4;

    auto right_header_label = lv_label_create(top_cont);
    lv_label_set_text(right_header_label, right_header.c_str());
    lv_obj_set_style_text_font(right_header_label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(right_header_label, LV_GRID_ALIGN_START, 2, LV_GRID_ALIGN_START, 0);

    if (_data.countdown.year) {
        tm countdown_time_info;
        _data.countdown.to_time_info(countdown_time_info);
        auto countdown = mktime(&countdown_time_info);
        auto today = mktime(&today_time_info);
        auto countdown_days = (int)((countdown - today) / (24ull * 3600));
        auto middle_header = format(MSG_COUNTDOWN, countdown_days);

        auto middle_header_label = lv_label_create(top_cont);
        lv_label_set_text(middle_header_label, middle_header.c_str());
        lv_obj_set_style_text_font(middle_header_label, SMALL_MEDIUM_FONT, LV_PART_MAIN);
        lv_obj_set_grid_cell(middle_header_label, LV_GRID_ALIGN_CENTER, 1, LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_hor(middle_header_label, lv_dpx(10), LV_PART_MAIN);
    }

    auto hor_line = create_line(outer_cont, orientation_t::horizontal, 0, 3, 1, 1);
    lv_obj_set_style_pad_top(hor_line, lv_dpx(6), LV_PART_MAIN);

    auto first_half_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(first_half_cont);
    static lv_coord_t first_half_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t first_half_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                                    LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(first_half_cont, first_half_cont_col_desc, first_half_cont_row_desc);
    lv_obj_set_grid_cell(first_half_cont, LV_GRID_ALIGN_STRETCH, is_second_half ? 2 : 0, LV_GRID_ALIGN_STRETCH, 2);

    auto first_half_top_ellipsis = create_ellipsis_obj(first_half_cont, 0, 0);
    auto first_half_bottom_ellipsis = create_ellipsis_obj(first_half_cont, 0, 2);

    auto first_half = lv_obj_create(first_half_cont);
    reset_layout_container_styles(first_half);
    static lv_coord_t first_half_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t first_half_row_desc[] = {
        LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
        LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(first_half, first_half_col_desc, first_half_row_desc);
    lv_obj_set_grid_cell(first_half, LV_GRID_ALIGN_STRETCH, 0, LV_GRID_ALIGN_STRETCH, 1);

    create_line(outer_cont, orientation_t::vertical, 1, 2);

    auto second_half_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(second_half_cont);
    static lv_coord_t second_half_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t second_half_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                                     LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(second_half_cont, second_half_cont_col_desc, second_half_cont_row_desc);
    lv_obj_set_grid_cell(second_half_cont, LV_GRID_ALIGN_STRETCH, is_second_half ? 0 : 2, LV_GRID_ALIGN_STRETCH, 2);

    auto second_half_top_ellipsis = create_ellipsis_obj(second_half_cont, 0, 0);
    auto second_half_bottom_ellipsis = create_ellipsis_obj(second_half_cont, 0, 2);

    auto second_half = lv_obj_create(second_half_cont);
    reset_layout_container_styles(second_half);
    static lv_coord_t second_half_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t second_half_row_desc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                                LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                                LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(second_half, second_half_col_desc, second_half_row_desc);
    lv_obj_set_grid_cell(second_half, LV_GRID_ALIGN_STRETCH, 0, LV_GRID_ALIGN_STRETCH, 1);

    create_line(first_half, orientation_t::horizontal, 0, 2);
    create_line(first_half, orientation_t::horizontal, 0, 5);
    create_line(first_half, orientation_t::horizontal, 0, 8);
    create_line(second_half, orientation_t::horizontal, 0, 2);
    create_line(second_half, orientation_t::horizontal, 0, 5);

    if (is_second_half) {
        create_day(second_half, 0, 0, 0, week_column_t::left);
        create_day(second_half, 1, 0, 3, week_column_t::left);
        create_day(second_half, 2, 0, 6, week_column_t::left);
        create_day(first_half, 3, 0, 0, week_column_t::right);
        create_day(first_half, 4, 0, 3, week_column_t::right);
        create_day(first_half, 5, 0, 6, week_column_t::right);
        create_day(first_half, 6, 0, 9, week_column_t::right);
    } else {
        create_day(first_half, 0, 0, 0, week_column_t::left);
        create_day(first_half, 1, 0, 3, week_column_t::left);
        create_day(first_half, 2, 0, 6, week_column_t::left);
        create_day(first_half, 3, 0, 9, week_column_t::left);
        create_day(second_half, 4, 0, 0, week_column_t::right);
        create_day(second_half, 5, 0, 3, week_column_t::right);
        create_day(second_half, 6, 0, 6, week_column_t::right);
    }

    // Check whether everything fits on the screen.

    lv_obj_update_layout(parent);

    // Scroll the content to hide any finished events. If we show the ellipsis, the panel
    // size changes so we need to scroll again.

    if (scroll_content(first_half, is_second_half ? week_column_t::right : week_column_t::left)) {
        lv_obj_clear_flag(first_half_top_ellipsis, LV_OBJ_FLAG_HIDDEN);

        lv_obj_update_layout(parent);
        scroll_content(first_half, is_second_half ? week_column_t::right : week_column_t::left);
    }
    if (scroll_content(second_half, is_second_half ? week_column_t::left : week_column_t::right)) {
        lv_obj_clear_flag(second_half_top_ellipsis, LV_OBJ_FLAG_HIDDEN);

        lv_obj_update_layout(parent);
        scroll_content(second_half, is_second_half ? week_column_t::left : week_column_t::right);
    }

    // If there still are events hidden at the bottom, show the bottom ellipsis also.

    lv_obj_update_layout(parent);

    if (lv_obj_get_scroll_bottom(first_half) > lv_dpx(5)) {
        lv_obj_clear_flag(first_half_bottom_ellipsis, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_obj_get_scroll_bottom(second_half) > lv_dpx(5)) {
        lv_obj_clear_flag(second_half_bottom_ellipsis, LV_OBJ_FLAG_HIDDEN);
    }
}

bool CalendarUI::scroll_content(lv_obj_t* cont, week_column_t week_column) {
    if (!_scroll_to_cont || _scroll_to_cont_week_column != week_column) {
        return false;
    }

    lv_area_t cont_area;
    lv_area_t obj_area;

    lv_obj_get_coords(cont, &cont_area);
    lv_obj_get_coords(_scroll_to_cont, &obj_area);

    auto scroll_bottom = lv_obj_get_scroll_bottom(cont);
    if (scroll_bottom < lv_dpx(5)) {
        return false;
    }

    auto offset = obj_area.y1 - cont_area.y1;

    lv_obj_scroll_by(cont, 0, -min<lv_coord_t>(offset, scroll_bottom), LV_ANIM_OFF);

    return true;
}

lv_obj_t* CalendarUI::create_ellipsis_obj(lv_obj_t* parent, uint8_t col, uint8_t row) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_column(cont, lv_dpx(6), LV_PART_MAIN);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_CENTER, col, LV_GRID_ALIGN_STRETCH, row);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);

    for (auto i = 0; i < 3; i++) {
        auto circle = lv_obj_create(cont);
        lv_obj_set_scrollbar_mode(circle, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_ver(cont, lv_dpx(7), LV_PART_MAIN);
        lv_obj_set_size(circle, lv_dpx(18), lv_dpx(18));
        lv_obj_set_style_bg_color(circle, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(circle, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }

    return cont;
}

lv_obj_t* CalendarUI::create_line(lv_obj_t* parent, orientation_t orientation, uint8_t col, uint8_t col_span,
                                  uint8_t row, uint8_t row_span) {
    auto line_cont = lv_obj_create(parent);
    reset_layout_container_styles(line_cont);
    if (orientation == orientation_t::horizontal) {
        lv_obj_set_grid_cell(line_cont, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_START, row, row_span);
    } else {
        lv_obj_set_grid_cell(line_cont, LV_GRID_ALIGN_START, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    }

    auto line = lv_obj_create(line_cont);
    lv_obj_remove_style_all(line);
    lv_obj_set_style_bg_opa(line, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(line, color_make(12), LV_PART_MAIN);
    if (orientation == orientation_t::horizontal) {
        lv_obj_set_size(line, LV_PCT(100), lv_dpx(4));
    } else {
        lv_obj_set_size(line, lv_dpx(4), LV_PCT(100));
    }

    return line_cont;
}

void CalendarUI::create_day(lv_obj_t* parent, int weekday, uint8_t col, uint8_t row, week_column_t week_column) {
    // Format the date, correctly offset for the day we're creating.

    tm time_info;
    _data.start.to_time_info(time_info);

    auto time = mktime(&time_info);

    time += (time_t)weekday * 24 * 3600;

    localtime_r(&time, &time_info);

    auto weekday_name = get_weekday((time_info.tm_wday + 6) % 7);

    auto year = time_info.tm_year + 1900;
    auto month = time_info.tm_mon + 1;
    auto day = time_info.tm_mday;

    auto header = format("%d. %s", day, weekday_name);

    // Render the events.

    auto outer_cont = lv_obj_create(parent);
    reset_layout_container_styles(outer_cont);
    static lv_coord_t outer_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t outer_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(outer_cont, outer_cont_col_desc, outer_cont_row_desc);
    lv_obj_set_grid_cell(outer_cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_START, row);

    auto header_label = lv_label_create(outer_cont);
    lv_label_set_text(header_label, header.c_str());
    lv_obj_set_grid_cell(header_label, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_style_text_font(header_label, SMALL_MEDIUM_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(header_label, lv_dpx(10), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(header_label, lv_dpx(10), LV_PART_MAIN);

    if (year == _data.today.year && month == _data.today.month && day == _data.today.day) {
        // This is fragile. Yes, the where checking here that the day isn't the
        // first day (because there's no point in scrolling if it's the first day).
        // However, this can break if the rows don't start at 0 anymore.
        if (row) {
            _scroll_to_cont = outer_cont;
            _scroll_to_cont_week_column = week_column;
        }

        auto today_label = lv_label_create(outer_cont);
        lv_label_set_text(today_label, MSG_BULLSEYE);
        lv_obj_set_grid_cell(today_label, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);
        lv_obj_set_style_text_font(today_label, SMALL_MEDIUM_FONT, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(today_label, lv_dpx(10), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(today_label, lv_dpx(10), LV_PART_MAIN);
    }

    auto cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 1, 1);
    lv_obj_set_style_pad_row(cont, lv_dpx(8), LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(cont, lv_dpx(8), LV_PART_MAIN);
    if (week_column == week_column_t::left) {
        lv_obj_set_style_pad_right(cont, lv_dpx(12), LV_PART_MAIN);
    } else {
        lv_obj_set_style_pad_left(cont, lv_dpx(12), LV_PART_MAIN);
    }

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
                lv_obj_set_style_bg_color(hor_line, color_make(7), LV_PART_MAIN);
                lv_obj_set_size(hor_line, LV_PCT(100), lv_dpx(3));
            }

            create_event(cont, event);
        }
    }
}

uint32_t CalendarUI::calculate_hash(const char* str) {
    uint32_t hash = 0x811C9DC5;  // FNV-1a magic.
    for (size_t i = 0;; i++) {
        auto c = str[i];
        if (!c) {
            break;
        }

        hash ^= (uint32_t)c << (i % 4) * 8;
    }

    return hash;
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
    if (value.calendar.color > 10) {
        lv_obj_set_style_text_color(cont, color_make(0), LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_color(cont, color_make(15), LV_PART_MAIN);
    }

    string event_text;
    if (!value.calendar.emoji.empty()) {
        event_text += value.calendar.emoji + " ";
    }

    if (value.summary.length() > 80) {
        event_text.append(value.summary, 0, 77);
        event_text += "...";
    } else {
        event_text += value.summary;
    }
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

lv_color_t CalendarUI::color_make(int color) {
    auto value = color << 4 | (color & 1) << 3 | (color & 1) << 2 | (color & 1) << 1 | (color & 1) << 0;

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
