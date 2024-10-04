#pragma once

#ifdef LV_SIMULATOR

extern "C" {
// Use the script in the tools folder to update the fonts.

LV_FONT_DECLARE(lv_font_regular_24);
LV_FONT_DECLARE(lv_font_regular_28);
LV_FONT_DECLARE(lv_font_medium_28);
LV_FONT_DECLARE(lv_font_medium_46);
}

static constexpr auto XSMALL_FONT = &lv_font_regular_24;
static constexpr auto SMALL_FONT = &lv_font_regular_28;
static constexpr auto SMALL_MEDIUM_FONT = &lv_font_medium_28;
static constexpr auto NORMAL_FONT = &lv_font_medium_46;

#else

extern "C" {
// Use the script in the tools folder to update the fonts.

LV_FONT_DECLARE(lv_font_regular_48);
LV_FONT_DECLARE(lv_font_regular_56);
LV_FONT_DECLARE(lv_font_medium_56);
LV_FONT_DECLARE(lv_font_medium_92);
}

static constexpr auto XSMALL_FONT = &lv_font_regular_48;
static constexpr auto SMALL_FONT = &lv_font_regular_56;
static constexpr auto SMALL_MEDIUM_FONT = &lv_font_medium_56;
static constexpr auto NORMAL_FONT = &lv_font_medium_92;

#endif
