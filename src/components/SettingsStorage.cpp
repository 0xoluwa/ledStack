#include "SettingsStorage.hpp"
#include <Arduino.h>

void SettingsStorage::init() {
    isInitialized = false;

    // Initialize NVS flash
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated, erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        isInitialized = true;
        Serial.println("NVS initialized");
    } else {
        Serial.printf("NVS init failed: %s\n", esp_err_to_name(err));
    }
}

bool SettingsStorage::loadSettings(DisplaySettings& settings) {
    if (!openNVS()) return false;

    // Load brightness
    uint8_t brightness = 255;
    nvs_get_u8(nvsHandle, KEY_BRIGHTNESS, &brightness);
    settings.brightness = brightness;

    // Load header color
    uint32_t headerColor = 0x0000FF;
    nvs_get_u32(nvsHandle, KEY_HEADER_COLOR, &headerColor);
    settings.headerColor = headerColor;

    // Load time color
    uint32_t timeColor = 0xFFFFFF;
    nvs_get_u32(nvsHandle, KEY_TIME_COLOR, &timeColor);
    settings.timeColor = timeColor;

    // Load background color
    uint32_t bgColor = 0x000000;
    nvs_get_u32(nvsHandle, KEY_BG_COLOR, &bgColor);
    settings.bgColor = bgColor;

    // Load header text
    size_t required_size = sizeof(settings.headerText);
    esp_err_t err = nvs_get_str(nvsHandle, KEY_HEADER_TEXT, settings.headerText, &required_size);
    if (err != ESP_OK) {
        // Default text if not found
        strncpy(settings.headerText, "ledStack", sizeof(settings.headerText) - 1);
        settings.headerText[sizeof(settings.headerText) - 1] = '\0';
    }

    closeNVS();

    Serial.println("Settings loaded from NVS");
    return true;
}

bool SettingsStorage::saveSettings(const DisplaySettings& settings) {
    if (!openNVS()) return false;

    bool success = true;

    // Save brightness
    if (nvs_set_u8(nvsHandle, KEY_BRIGHTNESS, settings.brightness) != ESP_OK) {
        success = false;
    }

    // Save header color
    if (nvs_set_u32(nvsHandle, KEY_HEADER_COLOR, settings.headerColor) != ESP_OK) {
        success = false;
    }

    // Save time color
    if (nvs_set_u32(nvsHandle, KEY_TIME_COLOR, settings.timeColor) != ESP_OK) {
        success = false;
    }

    // Save background color
    if (nvs_set_u32(nvsHandle, KEY_BG_COLOR, settings.bgColor) != ESP_OK) {
        success = false;
    }

    // Save header text
    if (nvs_set_str(nvsHandle, KEY_HEADER_TEXT, settings.headerText) != ESP_OK) {
        success = false;
    }

    // Commit changes
    if (nvs_commit(nvsHandle) != ESP_OK) {
        success = false;
    }

    closeNVS();

    if (success) {
        Serial.println("Settings saved to NVS");
    } else {
        Serial.println("Failed to save settings to NVS");
    }

    return success;
}

bool SettingsStorage::saveBrightness(uint8_t brightness) {
    if (!openNVS()) return false;

    bool success = (nvs_set_u8(nvsHandle, KEY_BRIGHTNESS, brightness) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();
    return success;
}

bool SettingsStorage::saveHeaderText(const char* text) {
    if (!openNVS()) return false;

    bool success = (nvs_set_str(nvsHandle, KEY_HEADER_TEXT, text) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();
    return success;
}

bool SettingsStorage::saveHeaderColor(uint32_t color) {
    if (!openNVS()) return false;

    bool success = (nvs_set_u32(nvsHandle, KEY_HEADER_COLOR, color) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();
    return success;
}

bool SettingsStorage::saveTimeColor(uint32_t color) {
    if (!openNVS()) return false;

    bool success = (nvs_set_u32(nvsHandle, KEY_TIME_COLOR, color) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();
    return success;
}

bool SettingsStorage::saveBgColor(uint32_t color) {
    if (!openNVS()) return false;

    bool success = (nvs_set_u32(nvsHandle, KEY_BG_COLOR, color) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();
    return success;
}

bool SettingsStorage::clearSettings() {
    if (!openNVS()) return false;

    bool success = (nvs_erase_all(nvsHandle) == ESP_OK);
    if (success) {
        success = (nvs_commit(nvsHandle) == ESP_OK);
    }

    closeNVS();

    if (success) {
        Serial.println("Settings cleared from NVS");
    }

    return success;
}

bool SettingsStorage::openNVS() {
    if (!isInitialized) {
        Serial.println("NVS not initialized");
        return false;
    }

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        Serial.printf("Failed to open NVS: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

void SettingsStorage::closeNVS() {
    nvs_close(nvsHandle);
}
