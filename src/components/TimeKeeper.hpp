#pragma once

#include <esp32/ulp.h>
#include <esp_sleep.h>
#include "Types.hpp"

class TimeKeeper {
public:
    void init();
    void loadULPProgram();
    TimeData getCurrentTime();
    void setTime(uint8_t h, uint8_t m, uint8_t s);
    PowerStatus getPowerStatus();
    void enterDeepSleep();
    bool wasWokenByULP();
    
private:
    void initADC();
    void configureWakeup();
    void syncTimeFromULP();
    void syncTimeToULP();
};

extern RTC_DATA_ATTR uint32_t ulp_seconds;
extern RTC_DATA_ATTR uint32_t ulp_minutes;
extern RTC_DATA_ATTR uint32_t ulp_hours;
extern RTC_DATA_ATTR uint32_t ulp_power_status;
