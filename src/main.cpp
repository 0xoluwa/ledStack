#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <time.h>

#include "components/DisplayManager.hpp"
#include "components/WebServer.hpp"
#include "components/SettingsStorage.hpp"
#include "Types.hpp"

// Your verified pinout
#define RTC_CE_PIN   33
#define RTC_IO_PIN   21
#define RTC_CLK_PIN  32

// Component instances
DisplayManager displayManager;
WebServerManager webServer;
SettingsStorage settingsStorage;

// FreeRTOS queues and semaphores
QueueHandle_t displayQueue;
QueueHandle_t storageQueue;
QueueHandle_t timeQueue;
SemaphoreHandle_t nvsMutex;

// Display state
bool displayPowerOn = true;

// FreeRTOS task handles
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t storageTaskHandle = NULL;

void displayTask(void* parameter);
void storageTask(void* parameter);

void webServerTask(void* parameter) 
{
    const TickType_t xDelay = pdMS_TO_TICKS(20); 

    while (true) {
        webServer.handleClient();
        vTaskDelay(xDelay);
    }
}

void webServerDisplayCallback(LED_PANEL_REQUEST req) 
{
#ifdef DEBUG_LEDSTACK
    Serial.println("Web server callback polled");
#endif
    xQueueSend(displayQueue, &req, 0);
}


void setup() 
{
#ifdef DEBUG_LEDSTACK
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("ledStack Initializing...");
    Serial.println("========================================");
#endif

    displayQueue = xQueueCreate(10, sizeof(LED_PANEL_REQUEST));
    storageQueue = xQueueCreate(10, sizeof(LED_PANEL_REQUEST)); 
    timeQueue = xQueueCreate(1, sizeof(TimeData));
    nvsMutex = xSemaphoreCreateMutex();

    if (displayQueue == NULL || storageQueue == NULL || nvsMutex == NULL || timeQueue == NULL) {
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
        16200,
        NULL,
        2,
        &webServerTaskHandle,
        0     
    );
    
    
    xTaskCreatePinnedToCore(
        storageTask,
        "StorageTask",
        4096,
        NULL,
        1,    
        &storageTaskHandle,
        0      
    );

#ifdef DEBUG_LEDSTACK
    Serial.println("========================================");
    Serial.println("ledStack Initialized Successfully");
    Serial.println("========================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
#endif
}

void loop() 
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}


void displayTask(void* parameter)
{
    const TickType_t xDelay = pdMS_TO_TICKS(2);

    // Software clock — runs entirely inside displayTask, no extra task needed
    struct tm currentTime = {};
    currentTime.tm_year = 2026 - 1900;
    currentTime.tm_mon = 0;
    currentTime.tm_mday = 27;
    currentTime.tm_hour = 12;
    currentTime.tm_min = 0;
    currentTime.tm_sec = 0;
    uint32_t lastTimeTick = millis();
    char timeStr[10];

    while (true) {
        LED_PANEL_REQUEST req;
        if (xQueueReceive(displayQueue, &req, 0) == pdTRUE) {
            displayManager.handleRequest(req);

            if (req.action == SET_TIME_DATA) {
                currentTime.tm_hour = req.data.timeData.hour;
                currentTime.tm_min = req.data.timeData.minute;
                currentTime.tm_sec = req.data.timeData.second;
                mktime(&currentTime);
                lastTimeTick = millis();
#ifdef DEBUG_LEDSTACK
                Serial.printf("Time synced: %02d:%02d:%02d\n",
                             req.data.timeData.hour,
                             req.data.timeData.minute,
                             req.data.timeData.second);
#endif
            } else if (req.action != SET_TIME_T && xQueueSend(storageQueue, &req, 0) == pdTRUE) {
#ifdef DEBUG_LEDSTACK
                Serial.println("Message sent to Storage from Display Manager");
#endif
            }
        }

        // Increment clock every second
        if (millis() - lastTimeTick >= 1000) {
            lastTimeTick += 1000;
            currentTime.tm_sec++;
            mktime(&currentTime);

            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", currentTime.tm_hour, currentTime.tm_min);
            displayManager.setTimeText(timeStr);
        }

        displayManager.update();
        displayManager.lvglTick();

        vTaskDelay(xDelay);
    }
}

void storageTask(void* parameter) 
{
    const TickType_t xDelay = pdMS_TO_TICKS(1000);

    while (true) {
        LED_PANEL_REQUEST req;

        if (xQueueReceive(storageQueue, &req, 0) == pdTRUE) {
            if (xSemaphoreTake(nvsMutex, portMAX_DELAY) == pdTRUE) {
                switch (req.action) {
                    case SET_HEADER_T:
                        settingsStorage.saveHeaderText(req.data.text);
#ifdef DEBUG_LEDSTACK
                        Serial.println("Storage: Saved header text");
#endif
                        break;
                    case SET_HEADER_COL:
                        settingsStorage.saveHeaderColor(req.data.color);
#ifdef DEBUG_LEDSTACK
                        Serial.println("Storage: Saved header color");
#endif
                        break;
                    case SET_TIME_COL:
                        settingsStorage.saveTimeColor(req.data.color);
#ifdef DEBUG_LEDSTACK
                        Serial.println("Storage: Saved time color");
#endif
                        break;
                    case SET_BG_COL:
                        settingsStorage.saveBgColor(req.data.color);
#ifdef DEBUG_LEDSTACK
                        Serial.println("Storage: Saved background color");
#endif
                        break;
                    case SET_LED_BRIGHT:
                        settingsStorage.saveBrightness(req.data.brightness);
#ifdef DEBUG_LEDSTACK
                        Serial.println("Storage: Saved brightness");
#endif
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