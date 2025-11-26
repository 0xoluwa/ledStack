#include "TimeKeeper.hpp"
#include "Config.hpp"

#include <esp32/ulp.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc_ulp.h>
#include <soc/rtc_io_reg.h>
#include <soc/soc.h>
#include <driver/rtc_cntl.h>
#include <driver/rtc_io.h>
#include <soc/rtc.h>


// Power sense configuration
#define POWER_SENSE_PIN GPIO_NUM_32  // RTC GPIO: HIGH = main power, LOW = battery

// RTC memory shared with ULP (persists through deep sleep)
RTC_DATA_ATTR uint32_t ulp_seconds = 0;
RTC_DATA_ATTR uint32_t ulp_minutes = 0;
RTC_DATA_ATTR uint32_t ulp_hours = 0;

void TimeKeeper::init() {
    // Configure power sense GPIO as RTC input
    rtc_gpio_init(POWER_SENSE_PIN);
    rtc_gpio_set_direction(POWER_SENSE_PIN, RTC_GPIO_MODE_INPUT_ONLY);

    rtc_gpio_pulldown_en(POWER_SENSE_PIN);
    gpio_pulldown_en(POWER_SENSE_PIN);

    // Only initialize time on first boot (not after waking from sleep)
    // RTC memory persists through deep sleep, so time is already set if we woke up
    esp_reset_reason_t reset_reason = esp_reset_reason();
    if (reset_reason == ESP_RST_POWERON) {
        ulp_hours = 12;
        ulp_minutes = 0;
        ulp_seconds = 0;
#ifdef DEBUG_LEDSTACK
        Serial.println("First boot - initializing time to 12:00:00");
#endif
    } 
#ifdef DEBUG_LEDSTACK
    else {
        Serial.printf("Resumed - Time preserved: %02d:%02d:%02d\n", ulp_hours, ulp_minutes, ulp_seconds);
    }
#endif

#ifdef DEBUG_LEDSTACK
    int gpio_level = rtc_gpio_get_level(POWER_SENSE_PIN);
    PowerStatus powerStatus = gpio_level ? MAIN_POWER : BATTERY_POWER;

    Serial.printf("GPIO 32 level: %d (0=LOW/battery, 1=HIGH/main)\n", gpio_level);
    Serial.printf("Power status: %s\n", powerStatus == MAIN_POWER ? "MAIN_POWER" : "BATTERY_POWER");
#endif

    configureWakeup();
    loadULPProgram();

#ifdef DEBUG_LEDSTACK
    Serial.printf("TimeKeeper initialized - Power: %s\n", powerStatus == MAIN_POWER ? "MAIN" : "BATTERY");
#endif
}

void TimeKeeper::configureWakeup() {
    esp_sleep_enable_ulp_wakeup();
}

void TimeKeeper::loadULPProgram() {
#ifdef DEBUG_LEDSTACK
    Serial.println("Setting up ULP program for timekeeping and power monitoring...");
#endif

    // Calibrate RTC slow clock for better timekeeping accuracy
    // This measures the actual RTC_SLOW_CLK frequency against the crystal
    uint32_t calibrated_period = 1000000;  // Default: 1 second
    uint32_t cal_value = rtc_clk_cal(RTC_CAL_RTC_MUX, 1000);

    if (cal_value){
#ifdef DEBUG_LEDSTACK
        Serial.printf("RTC calibration value: %d\n", cal_value);
        Serial.printf("Calibration represents: %d us per RTC clock cycle\n", cal_value >> 19);
#endif
        // cal_value is in Q13.19 fixed-point format (microseconds per RTC cycle * 2^19)
        // To get 1 second (1000000 us), we need: cycles = 1000000 * 2^19 / cal_value
        // Then convert cycles to microseconds for ulp_set_wakeup_period
        // But ulp_set_wakeup_period expects microseconds, so we use cal_value directly

        // For 1 second at actual RTC frequency:
        // We want: 1000000 microseconds of real time
        // The cal_value tells us how many microseconds each RTC cycle actually takes
        // So we keep the period at 1000000 us - the RTC will handle the rest
        calibrated_period = 1000000;  // Keep at 1 second - calibration is already factored in

#ifdef DEBUG_LEDSTACK
        Serial.printf("Using standard ULP timer period: %d us\n", calibrated_period);
        Serial.printf("RTC calibration will be handled by hardware\n");
#endif
    }

    // Calculate RTC_SLOW_MEM offsets for our variables
    size_t addr_offset_seconds = ((uint32_t)&ulp_seconds - SOC_RTC_DATA_LOW) / sizeof(uint32_t);
    size_t addr_offset_minutes = ((uint32_t)&ulp_minutes - SOC_RTC_DATA_LOW) / sizeof(uint32_t);
    size_t addr_offset_hours = ((uint32_t)&ulp_hours - SOC_RTC_DATA_LOW) / sizeof(uint32_t);

#ifdef DEBUG_LEDSTACK
    Serial.printf("ULP memory offsets: seconds=%d, minutes=%d, hours=%d\n", addr_offset_seconds, addr_offset_minutes, addr_offset_hours);
#endif

    // ULP program (runs every 1 second during deep sleep):
    // 1. Increment seconds counter
    // 2. Handle minute/hour overflow
    // 3. Read GPIO 32 to check power status
    // 4. Wake CPU if main power detected
    const ulp_insn_t ulp_program[] = {
        // Increment seconds
        I_MOVI(R1, addr_offset_seconds),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check if seconds >= 60
        M_BL(1, 60),          // Branch to label 1 if R0 < 60
        // Seconds overflow: reset to 0 and increment minutes
        I_MOVI(R0, 0),
        I_MOVI(R1, addr_offset_seconds),
        I_ST(R0, R1, 0),

        // Increment minutes
        I_MOVI(R1, addr_offset_minutes),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check if minutes >= 60
        M_BL(1, 60),          // Branch to label 1 if R0 < 60
        // Minutes overflow: reset to 0 and increment hours
        I_MOVI(R0, 0),
        I_MOVI(R1, addr_offset_minutes),
        I_ST(R0, R1, 0),

        // Increment hours
        I_MOVI(R1, addr_offset_hours),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check if hours >= 24
        M_BL(1, 24),          // Branch to label 1 if R0 < 24
        // Hours overflow: reset to 0
        I_MOVI(R0, 0),
        I_MOVI(R1, addr_offset_hours),
        I_ST(R0, R1, 0),

        // Label 1: Check power status
        M_LABEL(1),
        // Read RTC GPIO 9 (GPIO 32) state into R0
        I_RD_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 9, RTC_GPIO_IN_NEXT_S + 9),

        // If GPIO HIGH (main power), wake the CPU
        M_BL(2, 1),           // Branch to label 2 if R0 < 1 (battery)
        I_WAKE(),             // Main power detected - wake CPU!
        I_HALT(),

        M_LABEL(2),           // Battery power - stay asleep
        I_HALT()
    };

    size_t program_size = sizeof(ulp_program) / sizeof(ulp_insn_t);

#ifdef DEBUG_LEDSTACK
    Serial.printf("ULP program size: %d instructions\n", program_size);
#endif

    esp_err_t err = ulp_process_macros_and_load(0, ulp_program, &program_size);

    if (err != ESP_OK) {
#ifdef DEBUG_LEDSTACK
        Serial.printf("Failed to load ULP program: %s\n", esp_err_to_name(err));
#endif
        return;
    }
#ifdef DEBUG_LEDSTACK
    Serial.printf("ULP program loaded successfully, final size: %d\n", program_size);
#endif

    // Configure ULP wakeup timer with calibrated period for accurate timekeeping
    ulp_set_wakeup_period(0, calibrated_period);

#ifdef DEBUG_LEDSTACK
    Serial.printf("ULP timer configured with calibrated period (%d us)\n", calibrated_period);
#endif

    // Start the ULP program (sets entry point to address 0)
    err = ulp_run(0);
    if (err != ESP_OK) {
#ifdef DEBUG_LEDSTACK
        Serial.printf("ERROR: Failed to start ULP program: %s\n", esp_err_to_name(err));
#endif
        return;
    }
#ifdef DEBUG_LEDSTACK
    Serial.println("ULP program started successfully");
    Serial.println("ULP will wake CPU automatically when GPIO 32 goes HIGH (main power restored)");
#endif
}

TimeData TimeKeeper::getCurrentTime() {
    TimeData time;
    time.hour = ulp_hours;
    time.minute = ulp_minutes;
    time.second = ulp_seconds;
    return time;
}

void TimeKeeper::setTime(uint8_t h, uint8_t m, uint8_t s) {
    ulp_hours = h;
    ulp_minutes = m;
    ulp_seconds = s;
}

PowerStatus TimeKeeper::getPowerStatus() {
    // Read GPIO 32 directly to get current power status
    int gpio_level = rtc_gpio_get_level(POWER_SENSE_PIN);
    return gpio_level ? MAIN_POWER : BATTERY_POWER;
}

void TimeKeeper::enterDeepSleep() {
#ifdef DEBUG_LEDSTACK
    Serial.println("Entering deep sleep...");
    Serial.println("ULP will monitor GPIO 32 and wake when main power is restored");
    Serial.flush();
#endif

    delay(100);

    esp_deep_sleep_start();

#ifdef DEBUG_LEDSTACK
    Serial.println("ERROR: Failed to enter deep sleep!");
#endif
}

bool TimeKeeper::wasWokenByULP() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP;
}
