#pragma once

#include <nvs_flash.h>
#include <nvs.h>
#include "../Types.hpp"

class SettingsStorage {
public:
    void init();

    // Read operations
    bool loadSettings(DisplaySettings& settings);

    // Write operations
    bool saveSettings(const DisplaySettings& settings);

    // Individual parameter operations (convenience methods)
    bool saveBrightness(uint8_t brightness);
    bool saveHeaderText(const char* text);
    bool saveHeaderColor(uint32_t color);
    bool saveTimeColor(uint32_t color);
    bool saveBgColor(uint32_t color);

    // Clear all stored settings
    bool clearSettings();

private:
    nvs_handle_t nvsHandle;
    bool isInitialized;

    static constexpr const char* NVS_NAMESPACE = "ledstack";

    // NVS keys
    static constexpr const char* KEY_BRIGHTNESS = "brightness";
    static constexpr const char* KEY_HEADER_TEXT = "header_txt";
    static constexpr const char* KEY_HEADER_COLOR = "header_col";
    static constexpr const char* KEY_TIME_COLOR = "time_col";
    static constexpr const char* KEY_BG_COLOR = "bg_col";

    // Helper methods
    bool openNVS();
    void closeNVS();
};