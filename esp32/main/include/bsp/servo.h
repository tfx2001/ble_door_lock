#ifndef __SERVO_H
#define __SERVO_H

#include "driver/mcpwm.h"

typedef struct {
    mcpwm_unit_t mcpwm_num;
    mcpwm_io_signals_t io_signal;
    mcpwm_timer_t mcpwm_timer;
    uint8_t pin;
    uint8_t deg;
} servo_config_t;

uint8_t servo_init(servo_config_t *servo_conf);
void servo_set_angle(servo_config_t *servo_conf, uint8_t angle);

#endif