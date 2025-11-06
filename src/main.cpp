#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "components/TimeKeeper.hpp"
#include "components/DisplayManager.hpp"
#include "components/WebServer.hpp"
#include "components/SettingsStorage.hpp"
#include "Types.hpp"

// Component instances
TimeKeeper timeKeeper;
DisplayManager displayManager;
WebServerManager webServer;
SettingsStorage settingsStorage;

// FreeRTOS queues and semaphores
QueueHandle_t displayQueue;
SemaphoreHandle_t nvsMutex;

// Display state
bool displayPowerOn = true;

// FreeRTOS task handles
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t timeUpdateTaskHandle = NULL;

// Task: Display rendering and LVGL updates
void displayTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(10); // 10ms tick for LVGL

    while (true) {
        // Handle display requests from queue
        LED_PANEL_REQUEST req;
        if (xQueueReceive(displayQueue, &req, 0) == pdTRUE) {
            displayManager.handleRequest(req);

            // Handle time sync separately
            if (req.action == SET_TIME_DATA) {
                timeKeeper.setTime(req.data.timeData.hour,
                                  req.data.timeData.minute,
                                  req.data.timeData.second);
                Serial.printf("Time synced: %02d:%02d:%02d\n",
                             req.data.timeData.hour,
                             req.data.timeData.minute,
                             req.data.timeData.second);
            }

            // Save to NVS based on action
            if (xSemaphoreTake(nvsMutex, portMAX_DELAY) == pdTRUE) {
                switch (req.action) {
                    case SET_HEADER_T:
                        settingsStorage.saveHeaderText(req.data.text);
                        break;
                    case SET_HEADER_COL:
                        settingsStorage.saveHeaderColor(req.data.color);
                        break;
                    case SET_TIME_COL:
                        settingsStorage.saveTimeColor(req.data.color);
                        break;
                    case SET_BG_COL:
                        settingsStorage.saveBgColor(req.data.color);
                        break;
                    case SET_LED_BRIGHT:
                        settingsStorage.saveBrightness(req.data.brightness);
                        break;
                    default:
                        break;
                }
                xSemaphoreGive(nvsMutex);
            }
        }

        // Update LVGL
        displayManager.update();
        displayManager.lvglTick();

        vTaskDelay(xDelay);
    }
}

// Task: WebServer client handling
void webServerTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(2); // Fast response

    while (true) {
        webServer.handleClient();
        vTaskDelay(xDelay);
    }
}

// Task: Time updates from ULP to display
void timeUpdateTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000); // Update every second
    char timeBuffer[16];

    while (true) {
        TimeData currentTime = timeKeeper.getCurrentTime();

        // Format time as HH:MM:SS
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d",
                 currentTime.hour, currentTime.minute, currentTime.second);

        // Send to display queue
        LED_PANEL_REQUEST req;
        req.action = SET_TIME_T;
        strncpy(req.data.text, timeBuffer, sizeof(req.data.text) - 1);
        req.data.text[sizeof(req.data.text) - 1] = '\0';
        xQueueSend(displayQueue, &req, portMAX_DELAY);

        vTaskDelay(xDelay);
    }
}

// Callback for WebServer to send display requests
void webServerDisplayCallback(LED_PANEL_REQUEST req) {
    xQueueSend(displayQueue, &req, portMAX_DELAY);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("ledStack Initializing...");
    Serial.println("========================================");

    // Create FreeRTOS communication primitives
    displayQueue = xQueueCreate(10, sizeof(LED_PANEL_REQUEST));
    nvsMutex = xSemaphoreCreateMutex();

    if (displayQueue == NULL || nvsMutex == NULL) {
        Serial.println("Failed to create queue/mutex");
        while (1) { delay(1000); }
    }

    // Initialize components
    Serial.println("Initializing Settings Storage...");
    settingsStorage.init();

    Serial.println("Initializing TimeKeeper...");
    timeKeeper.init();

    // Power monitoring disabled for testing
    // if (timeKeeper.wasWokenByULP()) {
    //     Serial.println("Woken by ULP - Main power restored");
    // }

    // PowerStatus powerStatus = timeKeeper.getPowerStatus();
    // if (powerStatus == BATTERY_POWER) {
    //     Serial.println("Running on battery - entering deep sleep");
    //     timeKeeper.enterDeepSleep();
    // }

    Serial.println("Running on main power (power monitoring disabled for testing)");

    Serial.println("Initializing Display...");
    displayManager.init();

    // Load saved settings
    Serial.println("Loading saved settings...");
    DisplaySettings settings;
    if (settingsStorage.loadSettings(settings)) {
        displayManager.setBrightness(settings.brightness);
        displayManager.setHeaderText(settings.headerText);
        displayManager.setHeaderColor(settings.headerColor);
        displayManager.setTimeColor(settings.timeColor);
        displayManager.setBackgroundColor(settings.bgColor);
        Serial.println("Settings loaded and applied");
    } else {
        Serial.println("No saved settings, using defaults");
    }

    Serial.println("Initializing WebServer...");
    webServer.init();
    webServer.setDisplayControlCallback(webServerDisplayCallback);
    webServer.begin();

    // Create FreeRTOS tasks
    Serial.println("Creating FreeRTOS tasks...");

    // Display task - Core 1, High priority
    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        8192,  // Stack size
        NULL,
        2,     // Priority
        &displayTaskHandle,
        1      // Core 1
    );

    // WebServer task - Core 0, Medium priority
    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServerTask",
        8192,
        NULL,
        1,
        &webServerTaskHandle,
        0      // Core 0
    );

    // Time update task - Core 1, Medium priority
    xTaskCreatePinnedToCore(
        timeUpdateTask,
        "TimeUpdateTask",
        4096,
        NULL,
        1,
        &timeUpdateTaskHandle,
        1      // Core 1
    );

    Serial.println("========================================");
    Serial.println("ledStack Initialized Successfully");
    Serial.println("========================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Empty - all work done in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}