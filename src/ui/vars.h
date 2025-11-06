#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_C_HEADER = 0,
    FLOW_GLOBAL_VARIABLE_C_TIME = 1
};

// Native global variables

extern const char *get_var_c_header();
extern void set_var_c_header(const char *value);
extern const char *get_var_c_time();
extern void set_var_c_time(const char *value);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/