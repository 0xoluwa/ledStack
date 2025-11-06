#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include "../Config.hpp"
#include "../Types.hpp"

class WebServerManager {
public:
    void init();
    void begin();
    void handleClient();

    // Set callback for display control
    void setDisplayControlCallback(void (*callback)(LED_PANEL_REQUEST));

private:
    WebServer* server;
    void (*displayControlCallback)(LED_PANEL_REQUEST);

    // WiFi AP configuration
    void initWiFiAP();

    // HTTP request handlers
    void handleRoot();
    void handleUserControl();
    void handleAdmin();
    void handleNotFound();

    // API endpoints
    void apiSetHeaderText();
    void apiSetHeaderColor();
    void apiSetTimeColor();
    void apiSetBgColor();
    void apiSetBrightness();
    void apiSetDisplayPower();
    void apiSyncTime();
    void apiUpdateWiFiCredentials();

    // Authentication
    bool authenticate();

    // WiFi credentials storage
    struct WiFiCredentials {
        char ssid[32];
        char password[64];
    };

    bool loadWiFiCredentials(WiFiCredentials& creds);
    bool saveWiFiCredentials(const WiFiCredentials& creds);

    // HTML page generators
    String generateLoginPage();
    String generateUserControlPage();
    String generateAdminPage();
};