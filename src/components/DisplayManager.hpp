#pragma once

#define USE_GFX_LITE
#define USE_DOUBLE_BUFFERING 0

#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <lvgl.h>
#include "../Config.hpp"
#include "../Types.hpp"

// Forward declarations for EEZ UI
extern "C" void ui_init();
extern "C" void ui_tick();

class DisplayManager {
public:
    void init();
    void update();

    // Display control methods
    void setHeaderText(const char* message);
    void setHeaderColor(uint32_t color);
    void setTimeText(const char* message);
    void setTimeColor(uint32_t color);
    void setBackgroundColor(uint32_t color);
    void setBrightness(uint8_t brightness);

    // Request handler
    void handleRequest(LED_PANEL_REQUEST request);

    // LVGL tick for task scheduling
    void lvglTick();

private:
    // Hardware display objects
    MatrixPanel_I2S_DMA* dmaDisplay;
    VirtualMatrixPanel_T<VIRTUAL_MATRIX_CHAIN_TYPE, ScanTypeMapping<PANEL_SCAN>, 1>* virtualDisplay;

    // LVGL objects
    lv_display_t* lvDisplay;
    lv_color16_t* lvBuffer1;
    lv_color16_t* lvBuffer2;

    // Display dimensions
    static constexpr uint16_t DISPLAY_WIDTH = PANEL_RES_X * NUM_COLS;
    static constexpr uint16_t DISPLAY_HEIGHT = PANEL_RES_Y * NUM_ROWS;
    static constexpr size_t LV_BUFFER_SIZE = DISPLAY_WIDTH * 40;

    // Initialization methods
    void initHardwareDisplay();
    void initLVGL();
    void initUI();

    // LVGL callbacks (need to be static for C compatibility)
    static uint32_t lvglTickCallback();
    static void lvglFlushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);

    // Static instance for callbacks
    static DisplayManager* instance;
};
