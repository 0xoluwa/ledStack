#include "WebServer.hpp"
#include <nvs.h>
#include <nvs_flash.h>

void WebServerManager::init() {
    server = nullptr;
    displayControlCallback = nullptr;
}

void WebServerManager::begin() {
    initWiFiAP();

    server = new WebServer(80);

    // Route handlers
    server->on("/", [this]() { handleRoot(); });
    server->on("/control", [this]() { handleUserControl(); });
    server->on("/admin", [this]() { handleAdmin(); });

    // API endpoints - using uri() and method() to match routes with query params
    server->on("/api/header/text", [this]() {
        if (server->method() == HTTP_POST) apiSetHeaderText();
    });
    server->on("/api/header/color", [this]() {
        if (server->method() == HTTP_POST) apiSetHeaderColor();
    });
    server->on("/api/time/color", [this]() {
        if (server->method() == HTTP_POST) apiSetTimeColor();
    });
    server->on("/api/bg/color", [this]() {
        if (server->method() == HTTP_POST) apiSetBgColor();
    });
    server->on("/api/brightness", [this]() {
        if (server->method() == HTTP_POST) apiSetBrightness();
    });
    server->on("/api/power", [this]() {
        if (server->method() == HTTP_POST) apiSetDisplayPower();
    });
    server->on("/api/time/sync", [this]() {
        if (server->method() == HTTP_POST) apiSyncTime();
    });
    server->on("/api/wifi", [this]() {
        if (server->method() == HTTP_POST) apiUpdateWiFiCredentials();
    });

    server->onNotFound([this]() { handleNotFound(); });

    server->begin();
    Serial.println("HTTP server started");
}

void WebServerManager::handleClient() {
    if (server) {
        server->handleClient();
    }
}

void WebServerManager::setDisplayControlCallback(void (*callback)(LED_PANEL_REQUEST)) {
    displayControlCallback = callback;
}

void WebServerManager::initWiFiAP() {
    WiFiCredentials creds;
    if (!loadWiFiCredentials(creds)) {
        // Use defaults
        strncpy(creds.ssid, DEFAULT_AP_SSID, sizeof(creds.ssid) - 1);
        strncpy(creds.password, DEFAULT_AP_PASSWORD, sizeof(creds.password) - 1);
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(creds.ssid, creds.password);

    Serial.println("WiFi AP started");
    Serial.printf("SSID: %s\n", creds.ssid);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void WebServerManager::handleRoot() {
    if (!authenticate()) {
        return;
    }
    server->sendHeader("Location", "/control");
    server->send(302);
}

void WebServerManager::handleUserControl() {
    if (!authenticate()) {
        return;
    }
    server->send(200, "text/html", generateUserControlPage());
}

void WebServerManager::handleAdmin() {
    if (!authenticate()) {
        return;
    }
    server->send(200, "text/html", generateAdminPage());
}

void WebServerManager::handleNotFound() {
    String uri = server->uri();
    Serial.printf("WebServer: 404 Not Found - URI: %s, Method: %s\n",
                  uri.c_str(),
                  server->method() == HTTP_GET ? "GET" : "POST");
    Serial.printf("WebServer: Args count: %d\n", server->args());
    for (int i = 0; i < server->args(); i++) {
        Serial.printf("  Arg %d: %s = %s\n", i, server->argName(i).c_str(), server->arg(i).c_str());
    }
    server->send(404, "text/plain", "404: Not Found");
}

void WebServerManager::apiSetHeaderText() {
    Serial.println("WebServer: apiSetHeaderText called");

    if (!authenticate()) {
        Serial.println("WebServer: Authentication failed");
        return;
    }

    if (server->hasArg("text")) {
        String text = server->arg("text");
        Serial.printf("WebServer: Received text='%s'\n", text.c_str());

        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_HEADER_T;
            strncpy(req.data.text, text.c_str(), sizeof(req.data.text) - 1);
            req.data.text[sizeof(req.data.text) - 1] = '\0';
            displayControlCallback(req);
            Serial.println("WebServer: Request sent to display");
        } else {
            Serial.println("WebServer: ERROR - displayControlCallback is NULL");
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        Serial.println("WebServer: ERROR - missing 'text' parameter");
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing text\"}");
    }
}

void WebServerManager::apiSetHeaderColor() {
    Serial.println("WebServer: apiSetHeaderColor called");

    if (!authenticate()) {
        Serial.println("WebServer: Authentication failed");
        return;
    }

    if (server->hasArg("color")) {
        String colorStr = server->arg("color");
        Serial.printf("WebServer: Received color='%s'\n", colorStr.c_str());

        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_HEADER_COL;
            req.data.color = strtoul(colorStr.c_str(), nullptr, 16);
            Serial.printf("WebServer: Parsed color=0x%06X\n", req.data.color);
            displayControlCallback(req);
            Serial.println("WebServer: Request sent to display");
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        Serial.println("WebServer: ERROR - missing 'color' parameter");
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing color\"}");
    }
}

void WebServerManager::apiSetTimeColor() {
    if (!authenticate()) {
        return;
    }

    if (server->hasArg("color")) {
        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_TIME_COL;
            req.data.color = strtoul(server->arg("color").c_str(), nullptr, 16);
            displayControlCallback(req);
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing color\"}");
    }
}

void WebServerManager::apiSetBgColor() {
    if (!authenticate()) {
        return;
    }

    if (server->hasArg("color")) {
        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_BG_COL;
            req.data.color = strtoul(server->arg("color").c_str(), nullptr, 16);
            displayControlCallback(req);
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing color\"}");
    }
}

void WebServerManager::apiSetBrightness() {
    Serial.println("WebServer: apiSetBrightness called");

    if (!authenticate()) {
        Serial.println("WebServer: Authentication failed");
        return;
    }

    if (server->hasArg("brightness")) {
        int brightness = server->arg("brightness").toInt();
        Serial.printf("WebServer: Received brightness=%d\n", brightness);

        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_LED_BRIGHT;
            req.data.brightness = brightness;
            displayControlCallback(req);
            Serial.println("WebServer: Request sent to display");
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        Serial.println("WebServer: ERROR - missing 'brightness' parameter");
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing brightness\"}");
    }
}

void WebServerManager::apiSetDisplayPower() {
    if (!authenticate()) {
        return;
    }

    if (server->hasArg("power")) {
        bool power = server->arg("power") == "on";

        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_LED_BRIGHT;
            req.data.brightness = power ? 255 : 0;
            displayControlCallback(req);
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing power\"}");
    }
}

void WebServerManager::apiSyncTime() {
    if (!authenticate()) {
        return;
    }

    if (server->hasArg("hour") && server->hasArg("minute") && server->hasArg("second")) {
        if (displayControlCallback) {
            LED_PANEL_REQUEST req;
            req.action = SET_TIME_DATA;
            req.data.timeData.hour = server->arg("hour").toInt();
            req.data.timeData.minute = server->arg("minute").toInt();
            req.data.timeData.second = server->arg("second").toInt();
            displayControlCallback(req);
        }

        server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing time data\"}");
    }
}

void WebServerManager::apiUpdateWiFiCredentials() {
    if (!authenticate()) {
        return;
    }

    if (server->hasArg("ssid") && server->hasArg("password")) {
        WiFiCredentials creds;
        strncpy(creds.ssid, server->arg("ssid").c_str(), sizeof(creds.ssid) - 1);
        strncpy(creds.password, server->arg("password").c_str(), sizeof(creds.password) - 1);
        creds.ssid[sizeof(creds.ssid) - 1] = '\0';
        creds.password[sizeof(creds.password) - 1] = '\0';

        if (saveWiFiCredentials(creds)) {
            server->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"WiFi credentials saved. Restart to apply.\"}");
        } else {
            server->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save credentials\"}");
        }
    } else {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing ssid or password\"}");
    }
}

bool WebServerManager::authenticate() {
    if (!server->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
        server->requestAuthentication();
        return false;
    }
    return true;
}

bool WebServerManager::loadWiFiCredentials(WiFiCredentials& creds) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t ssid_len = sizeof(creds.ssid);
    size_t pass_len = sizeof(creds.password);

    err = nvs_get_str(handle, "ssid", creds.ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_get_str(handle, "password", creds.password, &pass_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

bool WebServerManager::saveWiFiCredentials(const WiFiCredentials& creds) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_str(handle, "ssid", creds.ssid);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_set_str(handle, "password", creds.password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return err == ESP_OK;
}

String WebServerManager::generateUserControlPage() {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ledStack Control</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #1a1a1a; color: #fff; }
        h1 { color: #4CAF50; }
        .control-group { margin: 20px 0; padding: 15px; background: #2a2a2a; border-radius: 5px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="number"], input[type="color"] { width: 100%; padding: 8px; margin-bottom: 10px; border: 1px solid #444; background: #333; color: #fff; border-radius: 3px; }
        button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background: #45a049; }
        .nav { margin-bottom: 20px; }
        .nav a { color: #4CAF50; text-decoration: none; margin-right: 15px; }
        .status { padding: 10px; margin-top: 10px; border-radius: 3px; display: none; }
        .status.success { background: #4CAF50; }
        .status.error { background: #f44336; }
    </style>
</head>
<body>
    <div class="nav">
        <a href="/control">Control</a>
        <a href="/admin">Admin</a>
    </div>
    <h1>ledStack Display Control</h1>

    <div class='control-group'>
        <h3>Display Power</h3>
        <button onclick='setPower("on")'>Turn ON</button>
        <button onclick='setPower("off")'>Turn OFF</button>
    </div>

    <div class='control-group'>
        <h3>Header Text</h3>
        <input type='text' id='headerText' placeholder='Enter header text'>
        <button onclick='setHeaderText()'>Update Header</button>
    </div>

    <div class='control-group'>
        <h3>Colors</h3>
        <label>Header Color:</label>
        <input type='color' id='headerColor' value='#0000ff'>
        <button onclick='setHeaderColor()'>Update</button>

        <label>Time Color:</label>
        <input type='color' id='timeColor' value='#ffffff'>
        <button onclick='setTimeColor()'>Update</button>

        <label>Background Color:</label>
        <input type='color' id='bgColor' value='#000000'>
        <button onclick='setBgColor()'>Update</button>
    </div>

    <div class='control-group'>
        <h3>Brightness</h3>
        <input type='number' id='brightness' min='0' max='255' value='255'>
        <button onclick='setBrightness()'>Update</button>
    </div>

    <div id="status" class="status"></div>

    <script>
        // Sync time on page load
        window.addEventListener('load', function() {
            const now = new Date();
            const hour = now.getHours();
            const minute = now.getMinutes();
            const second = now.getSeconds();

            fetch('/api/time/sync?hour=' + hour + '&minute=' + minute + '&second=' + second, { method: 'POST' })
                .then(r => r.json())
                .then(d => console.log('Time synced with device'))
                .catch(e => console.error('Time sync failed:', e));
        });

        function showStatus(message, isError) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + (isError ? 'error' : 'success');
            status.style.display = 'block';
            setTimeout(() => status.style.display = 'none', 3000);
        }

        function setPower(state) {
            fetch('/api/power?power=' + state, { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Display ' + state, false))
                .catch(e => showStatus('Error: ' + e, true));
        }

        function setHeaderText() {
            const text = document.getElementById('headerText').value;
            fetch('/api/header/text?text=' + encodeURIComponent(text), { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Header updated', false))
                .catch(e => showStatus('Error: ' + e, true));
        }

        function setHeaderColor() {
            const color = document.getElementById('headerColor').value.substring(1);
            fetch('/api/header/color?color=' + color, { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Header color updated', false))
                .catch(e => showStatus('Error: ' + e, true));
        }

        function setTimeColor() {
            const color = document.getElementById('timeColor').value.substring(1);
            fetch('/api/time/color?color=' + color, { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Time color updated', false))
                .catch(e => showStatus('Error: ' + e, true));
        }

        function setBgColor() {
            const color = document.getElementById('bgColor').value.substring(1);
            fetch('/api/bg/color?color=' + color, { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Background color updated', false))
                .catch(e => showStatus('Error: ' + e, true));
        }

        function setBrightness() {
            const brightness = document.getElementById('brightness').value;
            fetch('/api/brightness?brightness=' + brightness, { method: 'POST' })
                .then(r => r.json())
                .then(d => showStatus('Brightness updated', false))
                .catch(e => showStatus('Error: ' + e, true));
        }
    </script>
</body>
</html>
)";
}

String WebServerManager::generateAdminPage() {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ledStack Admin</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #1a1a1a; color: #fff; }
        h1 { color: #ff9800; }
        .control-group { margin: 20px 0; padding: 15px; background: #2a2a2a; border-radius: 5px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="password"] { width: 100%; padding: 8px; margin-bottom: 10px; border: 1px solid #444; background: #333; color: #fff; border-radius: 3px; }
        button { background: #ff9800; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #e68900; }
        .nav { margin-bottom: 20px; }
        .nav a { color: #ff9800; text-decoration: none; margin-right: 15px; }
        .warning { background: #f44336; padding: 10px; border-radius: 3px; margin-bottom: 15px; }
        .status { padding: 10px; margin-top: 10px; border-radius: 3px; display: none; }
        .status.success { background: #4CAF50; }
        .status.error { background: #f44336; }
    </style>
</head>
<body>
    <div class="nav">
        <a href="/control">Control</a>
        <a href="/admin">Admin</a>
    </div>
    <h1>ledStack Admin Panel</h1>

    <div class="control-group">
        <div class="warning">
            <strong>Warning:</strong> Changes will take effect after ESP32 restart.
        </div>
        <h3>WiFi Access Point Settings</h3>
        <label>SSID:</label>
        <input type="text" id="ssid" placeholder="WiFi SSID" maxlength="31">

        <label>Password:</label>
        <input type="password" id="password" placeholder="WiFi Password (min 8 chars)\" minlength="8" maxlength="63">

        <button onclick="updateWiFi()\">Save WiFi Settings</button>
    </div>

    <div id="status" class="status"></div>

    <script>
        function showStatus(message, isError) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + (isError ? 'error' : 'success');
            status.style.display = 'block';
            setTimeout(() => status.style.display = 'none', 5000);
        }

        function updateWiFi() {
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;

            if (!ssid || !password) {
                showStatus('Please fill in both SSID and password', true);
                return;
            }

            if (password.length < 8) {
                showStatus('Password must be at least 8 characters', true);
                return;
            }

            fetch('/api/wifi?ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password), { method: 'POST' })
                .then(r => r.json())
                .then(d => {
                    if (d.status === 'ok') {
                        showStatus(d.message, false);
                    } else {
                        showStatus(d.message, true);
                    }
                })
                .catch(e => showStatus('Error: ' + e, true));
        }
    </script>
</body>
</html>
)";
}
