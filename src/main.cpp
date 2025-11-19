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
QueueHandle_t storageQueue;
SemaphoreHandle_t nvsMutex;

// Display state
bool displayPowerOn = true;

// FreeRTOS task handles
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t timeUpdateTaskHandle = NULL;
TaskHandle_t storageTaskHandle = NULL;


void displayTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(10); 

    while (true) {
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

            xQueueSend(storageQueue, &req, 0); 
        }

        displayManager.update();
        displayManager.lvglTick();

        vTaskDelay(xDelay);
    }
}

void storageTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000);

    while (true) {
        LED_PANEL_REQUEST req;

        if (xQueueReceive(storageQueue, &req, 0) == pdTRUE) {
            if (xSemaphoreTake(nvsMutex, portMAX_DELAY) == pdTRUE) {
                switch (req.action) {
                    case SET_HEADER_T:
                        settingsStorage.saveHeaderText(req.data.text);
                        Serial.println("Storage: Saved header text");
                        break;
                    case SET_HEADER_COL:
                        settingsStorage.saveHeaderColor(req.data.color);
                        Serial.println("Storage: Saved header color");
                        break;
                    case SET_TIME_COL:
                        settingsStorage.saveTimeColor(req.data.color);
                        Serial.println("Storage: Saved time color");
                        break;
                    case SET_BG_COL:
                        settingsStorage.saveBgColor(req.data.color);
                        Serial.println("Storage: Saved background color");
                        break;
                    case SET_LED_BRIGHT:
                        settingsStorage.saveBrightness(req.data.brightness);
                        Serial.println("Storage: Saved brightness");
                        break;
                    default:
                        break;
                }
                xSemaphoreGive(nvsMutex);
            }
        }
        vTaskDelay(xDelay);
    }
}

void webServerTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(2); 

    while (true) {
        webServer.handleClient();
        vTaskDelay(xDelay);
    }
}

void timeUpdateTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000); 
    char timeBuffer[16];
    bool showColon = true;

    while (true) {
        int gpio_level = rtc_gpio_get_level(GPIO_NUM_32);
        PowerStatus powerStatus = gpio_level ? MAIN_POWER : BATTERY_POWER;

        TimeData currentTime = timeKeeper.getCurrentTime();

        if (powerStatus == BATTERY_POWER) {
            Serial.println("Battery detected - entering deep sleep (ULP will handle time/wake)");
            timeKeeper.enterDeepSleep();
        }

        // Convert to 12-hour format for display
        uint8_t displayHour = currentTime.hour;
        if (displayHour == 0) {
            displayHour = 12; // Midnight
        } else if (displayHour > 12) {
            displayHour -= 12; // PM hours
        }

        // Format time with pulsing colon (alternates every second)
        const char* separator = showColon ? ":" : " ";
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d%s%02d",
                 displayHour, separator, currentTime.minute);

        showColon = !showColon; // Toggle for next iteration

        LED_PANEL_REQUEST req;
        req.action = SET_TIME_T;
        strncpy(req.data.text, timeBuffer, sizeof(req.data.text) - 1);
        req.data.text[sizeof(req.data.text) - 1] = '\0';
        xQueueSend(displayQueue, &req, portMAX_DELAY);

        vTaskDelay(xDelay);
    }
}

void webServerDisplayCallback(LED_PANEL_REQUEST req) {
    xQueueSend(displayQueue, &req, portMAX_DELAY);
}


void setup() {
#ifdef DEBUG_LEDSTACK
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("ledStack Initializing...");
    Serial.println("========================================");
#endif

    displayQueue = xQueueCreate(10, sizeof(LED_PANEL_REQUEST));
    storageQueue = xQueueCreate(10, sizeof(LED_PANEL_REQUEST)); 
    nvsMutex = xSemaphoreCreateMutex();

    if (displayQueue == NULL || storageQueue == NULL || nvsMutex == NULL) {
#ifdef DEBUG_LEDSTACK
        Serial.println("Failed to create queue/mutex");
#endif
        while (1) { delay(1000); }
    }

#ifdef DEBUG_LEDSTACK
    Serial.println("Initializing Settings Storage...");
#endif
    settingsStorage.init();

#ifdef DEBUG_LEDSTACK
    Serial.println("Initializing TimeKeeper...");
#endif
    timeKeeper.init();

#ifdef DEBUG_LEDSTACK
    if (timeKeeper.wasWokenByULP()) {
        Serial.println("Woken by ULP - Main power restored");
    }
#endif

    PowerStatus powerStatus = timeKeeper.getPowerStatus();
#ifdef DEBUG_LEDSTACK
    Serial.printf("Power status check in main: %d (0=battery, 1=main)\n", powerStatus);
#endif
    if (powerStatus == BATTERY_POWER) {
#ifdef DEBUG_LEDSTACK
        Serial.println("Running on battery - entering deep sleep");
#endif
        timeKeeper.enterDeepSleep();
    }

#ifdef DEBUG_LEDSTACK
    Serial.println("Running on main power");
    Serial.println("Initializing Display...");
#endif

    displayManager.init();

#ifdef DEBUG_LEDSTACK
    Serial.println("Loading saved settings...");
#endif

    DisplaySettings settings;
    if (settingsStorage.loadSettings(settings)) {
        displayManager.setBrightness(settings.brightness);
        displayManager.setHeaderText(settings.headerText);
        displayManager.setHeaderColor(settings.headerColor);
        displayManager.setTimeColor(settings.timeColor);
        displayManager.setBackgroundColor(settings.bgColor);
#ifdef DEBUG_LEDSTACK
        Serial.println("Settings loaded and applied");
#endif
    } 
#ifdef DEBUG_LEDSTACK
    else {
        Serial.println("No saved settings, using defaults");
    }
    Serial.println("Initializing WebServer...");
#endif

    webServer.init();
    webServer.setDisplayControlCallback(webServerDisplayCallback);
    webServer.begin();

#ifdef DEBUG_LEDSTACK
    Serial.println("Creating FreeRTOS tasks...");
#endif

    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        8192,  
        NULL,
        2,  
        &displayTaskHandle,
        1     
    );

    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServerTask",
        8192,
        NULL,
        1,
        &webServerTaskHandle,
        0     
    );

    xTaskCreatePinnedToCore(
        timeUpdateTask,
        "TimeUpdateTask",
        4096,
        NULL,
        1,
        &timeUpdateTaskHandle,
        1     
    );

    xTaskCreatePinnedToCore(
        storageTask,
        "StorageTask",
        4096,
        NULL,
        1,    
        &storageTaskHandle,
        1      
    );

#ifdef DEBUG_LEDSTACK
    Serial.println("========================================");
    Serial.println("ledStack Initialized Successfully");
    Serial.println("========================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
#endif
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}