#include "DisplayManager.hpp"
#include "ui/ui.h"
#include "ui/screens.h"

// Static instance pointer for callbacks
DisplayManager* DisplayManager::instance = nullptr;

void DisplayManager::init() {
    instance = this;

    dmaDisplay = nullptr;
    virtualDisplay = nullptr;
    lvDisplay = nullptr;
    lvBuffer1 = nullptr;
    lvBuffer2 = nullptr;

    initHardwareDisplay();
    initLVGL();
    initUI();

    // Set default header color
    lv_obj_set_style_text_color(objects.head_lb__main_ctn, lv_color_hex(0x0000ff), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void DisplayManager::initHardwareDisplay() {
    HUB75_I2S_CFG::i2s_pins pins = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN,
        B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN
    };

    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X * 2,
        PANEL_RES_Y / 2,
        NUM_ROWS * NUM_COLS,
        pins
    );

    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    mxconfig.clkphase = false;

    dmaDisplay = new MatrixPanel_I2S_DMA(mxconfig);
    dmaDisplay->setLatBlanking(1);
    dmaDisplay->setBrightness(255);
    dmaDisplay->begin();

    virtualDisplay = new VirtualMatrixPanel_T<VIRTUAL_MATRIX_CHAIN_TYPE, ScanTypeMapping<PANEL_SCAN>, 1>(
        NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y
    );
    virtualDisplay->setDisplay(*dmaDisplay);
    virtualDisplay->setPixelBase(8);
    virtualDisplay->clearScreen();

    Serial.println("Display hardware initialized");
}

void DisplayManager::initLVGL() {
    lv_init();
    lv_tick_set_cb(lvglTickCallback);

    lvDisplay = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);

    size_t buf_bytes = LV_BUFFER_SIZE * sizeof(lv_color16_t);
    lvBuffer1 = (lv_color16_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    assert(lvBuffer1);

#if USE_DOUBLE_BUFFERING
    lvBuffer2 = (lv_color16_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    assert(lvBuffer2);
#else
    lvBuffer2 = nullptr;
#endif

    lv_display_set_buffers(lvDisplay, lvBuffer1, lvBuffer2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay, lvglFlushCallback);

    Serial.println("LVGL initialized");
}

void DisplayManager::initUI() {
    ui_init();
    ui_tick();
    Serial.println("UI initialized");
}

void DisplayManager::update() {
    lv_timer_handler();
    ui_tick();
}

void DisplayManager::lvglTick() {
    lv_tick_inc(1);
}

void DisplayManager::setHeaderText(const char* message) {
    lv_label_set_text(objects.head_lb__main_ctn, message);
}

void DisplayManager::setHeaderColor(uint32_t color) {
    lv_obj_set_style_text_color(objects.head_lb__main_ctn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void DisplayManager::setTimeText(const char* message) {
    lv_label_set_text(objects.time_lb__main_ctn, message);
}

void DisplayManager::setTimeColor(uint32_t color) {
    lv_obj_set_style_text_color(objects.time_lb__main_ctn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void DisplayManager::setBackgroundColor(uint32_t color) {
    lv_obj_set_style_bg_color(objects.main_ctn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    if (dmaDisplay) {
        dmaDisplay->setBrightness(brightness);
    }
}

void DisplayManager::handleRequest(LED_PANEL_REQUEST request) {
    switch (request.action) {
        case SET_HEADER_T:
            setHeaderText(request.data.text);
            break;
        case SET_HEADER_COL:
            setHeaderColor(request.data.color);
            break;
        case SET_TIME_T:
            setTimeText(request.data.text);
            break;
        case SET_TIME_COL:
            setTimeColor(request.data.color);
            break;
        case SET_BG_COL:
            setBackgroundColor(request.data.color);
            break;
        case SET_LED_BRIGHT:
            setBrightness(request.data.brightness);
            break;
        default:
            break;
    }
}

// Static callback implementations
uint32_t DisplayManager::lvglTickCallback() {
    return esp_timer_get_time() / 1000ULL;
}

void DisplayManager::lvglFlushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
    if (!instance || !instance->virtualDisplay) return;

    const int x = area->x1;
    const int y = area->y1;
    const int w = lv_area_get_width(area);
    const int h = lv_area_get_height(area);

    instance->virtualDisplay->drawRGBBitmap(x, y, (uint16_t*)px_map, w, h);

    lv_display_flush_ready(instance->lvDisplay);
}
