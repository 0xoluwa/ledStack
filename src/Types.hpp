#pragma once
#include <Arduino.h>

enum PowerStatus {
    BATTERY_POWER = 0,  // GPIO LOW = battery
    MAIN_POWER = 1      // GPIO HIGH = main power
};

struct TimeData {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

enum LED_PANEL_ACTION {
    LED_P_OK,
    SET_HEADER_T,
    SET_HEADER_COL,
    SET_TIME_T,
    SET_TIME_COL,
    SET_BG_COL,
    SET_LED_BRIGHT,
    SET_TIME_DATA
};

struct LED_PANEL_REQUEST {
    LED_PANEL_ACTION action = LED_P_OK;
    union {
        char text[128];
        uint32_t color;
        uint8_t brightness;
        TimeData timeData;
    } data;
};

struct DisplaySettings {
    uint8_t brightness;
    uint32_t headerColor;
    uint32_t timeColor;
    uint32_t bgColor;
    char headerText[128];
};