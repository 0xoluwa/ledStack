#pragma once

//pin configuration
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN -1
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

//panel chaining configuration
#define PANEL_RES_X 80
#define PANEL_RES_Y 40
#define NUM_COLS 2
#define NUM_ROWS 1

#define VIRTUAL_MATRIX_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN
#define PANEL_SCAN FOUR_SCAN_40PX_HIGH
#define SHIFT_DRIVER HUB75_I2S_CFG::ICN2038S

// WiFi AP configuration
#define DEFAULT_AP_SSID "ledStack-AP"
#define DEFAULT_AP_PASSWORD "12345678"

// Web authentication
#define WEB_USERNAME "ledStack"
#define WEB_PASSWORD "generic"