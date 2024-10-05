#pragma once

struct CalendarTimestampDto {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    bool has_time;

    void to_time_info(tm& time_info);
};

struct CalendarDto {
    string name;
    string emoji;
    int color;
};

struct CalendarMultiDayInstanceDto {
    int instance;
    int total;
};

struct CalendarEventDto {
    CalendarDto calendar;
    CalendarTimestampDto start;
    CalendarTimestampDto end;
    string summary;
    int duration;
    CalendarMultiDayInstanceDto instance;
};

struct CalendarEventsDto {
    CalendarTimestampDto start;
    CalendarTimestampDto end;
    vector<CalendarEventDto> events;

    CalendarEventsDto() = default;
    CalendarEventsDto(const CalendarEventsDto&) = delete;
    CalendarEventsDto& operator=(const CalendarEventsDto&) = delete;
    CalendarEventsDto(CalendarEventsDto&&) = delete;
    CalendarEventsDto& operator=(CalendarEventsDto&&) = delete;

    void clear();

    static bool from_json(const char* json_string, CalendarEventsDto& data);
};
