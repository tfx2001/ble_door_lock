#include <bsp/servo.h>

/**
 * @brief 初始化舵机
 *
 * @param servo_conf 舵机设置对象指针
 * @return uint8_t 初始化结果
 */
uint8_t servo_init(servo_config_t *servo_conf) {
    mcpwm_config_t mcpwm_conf = {.cmpr_a = 0,
                                 .cmpr_b = 0,
                                 .counter_mode = MCPWM_UP_COUNTER,
                                 .duty_mode = MCPWM_DUTY_MODE_0,
                                 .frequency = 50};

    mcpwm_gpio_init(servo_conf->mcpwm_num, servo_conf->io_signal,
                    servo_conf->pin);
    mcpwm_init(servo_conf->mcpwm_num, servo_conf->mcpwm_timer, &mcpwm_conf);
    return 0;
}

/**
 * @brief 设置舵机的角度
 *
 * @param servo_conf 舵机设置对象指针
 * @param angle 要设置的角度
 */
void servo_set_angle(servo_config_t *servo_conf, uint8_t angle) {
    uint32_t duty_us;

    servo_conf->deg = angle;
    duty_us = (0.5 + (angle / 180.0) * 2.0) * 1000;
    mcpwm_set_duty_in_us(servo_conf->mcpwm_num, servo_conf->mcpwm_timer,
                         servo_conf->io_signal % 2 ? MCPWM_OPR_B : MCPWM_OPR_A,
                         duty_us);
}