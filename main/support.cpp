#include "includes.h"

#include "support.h"

int getisoweek(tm& time_info) {
    char week_str[3];
    strftime(week_str, sizeof(week_str), "%V", &time_info);

    return atoi(week_str);
}
