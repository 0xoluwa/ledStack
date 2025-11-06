#include "TimeKeeper.hpp"
#include <driver/adc.h>
#include <driver/rtc_io.h>
#include <esp32/ulp.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc_ulp.h>

// ADC configuration
#define POWER_SENSE_PIN GPIO_NUM_2
#define POWER_SENSE_ADC_CHANNEL ADC1_CHANNEL_2
#define POWER_THRESHOLD_ADC 2300  // ~1.8V threshold (5V vs 4V after divider)

// RTC memory shared with ULP
RTC_DATA_ATTR uint32_t ulp_seconds = 0;
RTC_DATA_ATTR uint32_t ulp_minutes = 0;
RTC_DATA_ATTR uint32_t ulp_hours = 0;
RTC_DATA_ATTR uint32_t ulp_power_status = MAIN_POWER;
RTC_DATA_ATTR uint32_t ulp_previous_power_status = MAIN_POWER;

void TimeKeeper::init() {
    // Temporarily disable ADC and ULP for testing
    // initADC();
    // configureWakeup();
    // loadULPProgram();

    // Set initial time for testing (can be changed via setTime())
    ulp_hours = 12;
    ulp_minutes = 0;
    ulp_seconds = 0;
    ulp_power_status = MAIN_POWER;

    Serial.println("TimeKeeper initialized (ULP disabled for testing)");
}

void TimeKeeper::initADC() {
    // Configure ADC1 for power monitoring
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POWER_SENSE_ADC_CHANNEL, ADC_ATTEN_DB_11);  // 0-3.3V range

    // Configure GPIO as RTC GPIO for ULP access
    rtc_gpio_init(POWER_SENSE_PIN);
    rtc_gpio_set_direction(POWER_SENSE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
}

void TimeKeeper::configureWakeup() {
    // Enable ULP wakeup
    assert(esp_sleep_enable_ulp_wakeup());
}

void TimeKeeper::loadULPProgram() {
    // ULP program using C macros
    const ulp_insn_t ulp_program[] = {
        // Read ADC for power monitoring
        I_ADC(R0, 0, POWER_SENSE_ADC_CHANNEL),   // R0 = ADC reading
        M_BL(1, POWER_THRESHOLD_ADC),            // If R0 < threshold, jump to battery handler

        // Main power detected (R0 >= threshold)
        I_MOVI(R1, ulp_previous_power_status & 0xFFFF),
        I_LD(R2, R1, 0),                         // R2 = previous power status
        I_MOVI(R0, MAIN_POWER),
        I_MOVI(R1, ulp_power_status & 0xFFFF),
        I_ST(R0, R1, 0),                         // Store current status = MAIN_POWER

        // Check if transitioned from battery to main (R2 == BATTERY_POWER)
        I_MOVI(R3, BATTERY_POWER),
        I_SUBR(R3, R2, R3),                      // R3 = R2 - BATTERY_POWER
        M_BL(2, 1),                              // If R2 >= BATTERY_POWER, skip wake
        I_WAKE(),                                // Wake main CPU on battery->main transition
        M_LABEL(2),
        M_BX(3),                                 // Jump to time increment

        // Battery power detected (R0 < threshold)
        M_LABEL(1),
        I_MOVI(R0, BATTERY_POWER),
        I_MOVI(R1, ulp_power_status & 0xFFFF),
        I_ST(R0, R1, 0),

        // Increment time
        M_LABEL(3),
        I_MOVI(R1, ulp_seconds & 0xFFFF),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check seconds >= 60
        M_BGE(4, 60),                            // If R0 >= 60, handle overflow

        I_HALT(),                                // Done, sleep until next cycle

        // Seconds overflow: reset seconds, increment minutes
        M_LABEL(4),
        I_MOVI(R0, 0),
        I_MOVI(R1, ulp_seconds & 0xFFFF),
        I_ST(R0, R1, 0),
        I_MOVI(R1, ulp_minutes & 0xFFFF),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check minutes >= 60
        M_BGE(5, 60),                            // If R0 >= 60, handle overflow
        I_HALT(),

        // Minutes overflow: reset minutes, increment hours
        M_LABEL(5),
        I_MOVI(R0, 0),
        I_MOVI(R1, ulp_minutes & 0xFFFF),
        I_ST(R0, R1, 0),
        I_MOVI(R1, ulp_hours & 0xFFFF),
        I_LD(R0, R1, 0),
        I_ADDI(R0, R0, 1),
        I_ST(R0, R1, 0),

        // Check hours >= 24
        M_BGE(6, 24),                            // If R0 >= 24, reset hours
        I_HALT(),

        // Hours overflow: reset to 0
        M_LABEL(6),
        I_MOVI(R0, 0),
        I_MOVI(R1, ulp_hours & 0xFFFF),
        I_ST(R0, R1, 0),
        I_HALT()
    };

    size_t program_size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(0, ulp_program, &program_size);

    if (err != ESP_OK) {
        Serial.println("Failed to load ULP program");
        return;
    }

    // Set ULP wakeup period to 1 second
    ulp_set_wakeup_period(0, 1000000);  // 1 second in microseconds

    // Start ULP
    err = ulp_run(0);
    if (err != ESP_OK) {
        Serial.println("Failed to start ULP");
    }
}

TimeData TimeKeeper::getCurrentTime() {
    TimeData time;

    // Simulate time increment for testing (since ULP is disabled)
    static unsigned long lastMillis = 0;
    unsigned long currentMillis = millis();

    if (currentMillis - lastMillis >= 1000) {
        lastMillis = currentMillis;

        ulp_seconds++;
        if (ulp_seconds >= 60) {
            ulp_seconds = 0;
            ulp_minutes++;
            if (ulp_minutes >= 60) {
                ulp_minutes = 0;
                ulp_hours++;
                if (ulp_hours >= 24) {
                    ulp_hours = 0;
                }
            }
        }
    }

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
    return static_cast<PowerStatus>(ulp_power_status);
}

void TimeKeeper::enterDeepSleep() {
    Serial.println("Entering deep sleep...");
    esp_deep_sleep_start();
}

bool TimeKeeper::wasWokenByULP() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP;
}

void TimeKeeper::syncTimeFromULP() {
    // Time is already in shared RTC memory, no action needed
}

void TimeKeeper::syncTimeToULP() {
    // Time is already in shared RTC memory, no action needed
}
