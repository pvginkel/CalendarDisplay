// ReSharper disable CppClangTidyMiscUseAnonymousNamespace

#include "includes.h"

#include "CalendarEventsDto.h"

#include "cJSON.h"

LOG_TAG(CalendarEventsDto);

static bool parse_calendar_timestamp(cJSON* obj, CalendarTimestampDto& dto) {
    if (!cJSON_IsString(obj)) {
        ESP_LOGE(TAG, "Calendar timestamp is not a string");
        return false;
    }

    auto result = sscanf(obj->valuestring, "%d-%d-%dT%d:%d:%d", &dto.year, &dto.month, &dto.day, &dto.hour, &dto.minute,
                         &dto.second);
    if (result == 3) {
        dto.has_time = false;
        return true;
    }
    if (result == 6) {
        dto.has_time = true;
        return true;
    }

    ESP_LOGE(TAG, "Calendar timestamp is not of the correct format");
    return false;
}

static bool parse_calendar(cJSON* obj, CalendarDto& dto) {
    if (!cJSON_IsObject(obj)) {
        ESP_LOGE(TAG, "Calendar is not an object");
        return false;
    }

    auto name = cJSON_GetObjectItemCaseSensitive(obj, "name");
    if (!cJSON_IsString(name)) {
        ESP_LOGE(TAG, "Calendar name is not a string");
        return false;
    }
    dto.name = name->valuestring;

    auto emoji = cJSON_GetObjectItemCaseSensitive(obj, "emoji");
    if (cJSON_IsString(emoji)) {
        dto.emoji = emoji->valuestring;
    }

    auto color = cJSON_GetObjectItemCaseSensitive(obj, "color");
    if (!cJSON_IsNumber(color)) {
        ESP_LOGE(TAG, "Calendar color is not a number");
        return false;
    }
    dto.color = color->valueint;

    return true;
}

static bool parse_calendar_multi_day_instance(cJSON* obj, CalendarMultiDayInstanceDto& dto) {
    if (cJSON_IsNull(obj)) {
        dto = {};
        return true;
    }
    if (!cJSON_IsObject(obj)) {
        ESP_LOGE(TAG, "Calendar multi day instance is not an object");
        return false;
    }

    auto instance = cJSON_GetObjectItemCaseSensitive(obj, "instance");
    if (!cJSON_IsNumber(instance)) {
        ESP_LOGE(TAG, "Calendar multi day instance instance is not a number");
    }
    dto.instance = instance->valueint;

    auto total = cJSON_GetObjectItemCaseSensitive(obj, "total");
    if (!cJSON_IsNumber(instance)) {
        ESP_LOGE(TAG, "Calendar multi day instance total is not a number");
    }
    dto.total = total->valueint;

    return true;
}

static bool parse_calendar_event(cJSON* obj, CalendarEventDto& dto) {
    if (!cJSON_IsObject(obj)) {
        ESP_LOGE(TAG, "Calendar event object is not an object");
        return false;
    }

    auto calendar = cJSON_GetObjectItemCaseSensitive(obj, "calendar");
    if (!parse_calendar(calendar, dto.calendar)) {
        return false;
    }

    auto duration = cJSON_GetObjectItemCaseSensitive(obj, "duration");
    if (!cJSON_IsNumber(duration)) {
        ESP_LOGE(TAG, "Calendar event duration is not a number");
        return false;
    }
    dto.duration = duration->valueint;

    auto start = cJSON_GetObjectItemCaseSensitive(obj, "start");
    if (!parse_calendar_timestamp(start, dto.start)) {
        return false;
    }

    auto end = cJSON_GetObjectItemCaseSensitive(obj, "end");
    if (!parse_calendar_timestamp(end, dto.end)) {
        return false;
    }

    auto summary = cJSON_GetObjectItemCaseSensitive(obj, "summary");
    if (!cJSON_IsString(summary)) {
        ESP_LOGE(TAG, "Calendar event summary is not a string");
        return false;
    }
    dto.summary = summary->valuestring;

    auto instance = cJSON_GetObjectItemCaseSensitive(obj, "instance");
    if (!parse_calendar_multi_day_instance(instance, dto.instance)) {
        return false;
    }

    return true;
}

void CalendarEventsDto::clear() {
    start = {};
    end = {};
    events.clear();
}

bool CalendarEventsDto::from_json(const char* json_string, CalendarEventsDto& data) {
    data.clear();

    cJSON_Data root = {cJSON_Parse(json_string)};
    if (*root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse raw JSON string");
        return false;
    }

    const auto start = cJSON_GetObjectItemCaseSensitive(*root, "start");
    if (!parse_calendar_timestamp(start, data.start)) {
        return false;
    }

    const auto end = cJSON_GetObjectItemCaseSensitive(*root, "end");
    if (!parse_calendar_timestamp(end, data.end)) {
        return false;
    }

    const auto events = cJSON_GetObjectItemCaseSensitive(*root, "events");
    if (!cJSON_IsArray(events)) {
        ESP_LOGE(TAG, "Calendar events is not an array");
        return false;
    }

    cJSON* event;
    cJSON_ArrayForEach(event, events) {
        CalendarEventDto dto;
        if (!parse_calendar_event(event, dto)) {
            return false;
        }
        data.events.push_back(dto);
    }

    return true;
}

void CalendarTimestampDto::to_time_info(tm& time_info) {
    time_info = {};

    time_info.tm_year = year - 1900;
    time_info.tm_mon = month - 1;
    time_info.tm_mday = day;

    if (has_time) {
        time_info.tm_hour = hour;
        time_info.tm_min = minute;
        time_info.tm_sec = second;
    }
}
