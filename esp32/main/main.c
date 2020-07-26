/* C files */
#include <stdio.h>
#include <string.h>
/* FreeRTOS headfiles */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
/* ESP headfiles */
#include "esp_log.h"
#include "nvs_flash.h"
/* BT headfile */
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
/* BSP headfiles */
#include "bsp/servo.h"
/* BLE headfiles */
#include "bsp/ble.h"

/* Defines */

/* button gpio config */
gpio_config_t gpio0_config = {
    .pin_bit_mask = GPIO_SEL_0,
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_NEGEDGE,
};
/* LED gpio config */
gpio_config_t gpio2_config = {
    .pin_bit_mask = GPIO_SEL_2,
    .mode         = GPIO_MODE_OUTPUT,
    .pull_up_en   = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
};
/* BT controller config */
esp_bt_controller_config_t bt_conf = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
/* Task handle */
TaskHandle_t servo_task;
/* Semaphore handle */
SemaphoreHandle_t sem1; // servo binary sem
SemaphoreHandle_t sem2; // button (used to comfirm BLE bonding) sem

void servo_task_entry(void *parameter);
void gpio0_isr(void *arg);

/**
 * @brief 主函数
 *
 */
void app_main(void) {
    esp_err_t ret = 0;

    ESP_LOGI("main", "Init servo and BLE...");

    /* Init semaphore */
    sem1 = xSemaphoreCreateBinary();
    sem2 = xSemaphoreCreateBinary();
    /* Init servo control task */
    xTaskCreatePinnedToCore(servo_task_entry, "servo", 1024, NULL, 10,
                            servo_task, APP_CPU_NUM);
    /* GPIO config */
    gpio_config(&gpio0_config);
    gpio_config(&gpio2_config);
    gpio_set_level(GPIO_NUM_2, 0);
    /* install ISR service */
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    /* add isr handler */
    gpio_isr_handler_add(GPIO_NUM_0, gpio0_isr, (void *)GPIO_NUM_0);

    /* Init NVS（Non-Volatile Storage） */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* Init BLE */
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    ESP_ERROR_CHECK(ret);
    ret = esp_bt_controller_init(&bt_conf);
    if (ret) {
        ESP_LOGE("main", "%s initialize controller failed: %s", __func__,
                 esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE("main", "%s enable controller failed: %s", __func__,
                 esp_err_to_name(ret));
        return;
    }
    /* Init protocal stack */
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE("main", "%s initialize bluetooth failed: %s", __func__,
                 esp_err_to_name(ret));
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE("main", "%s enable bluetooth failed: %s", __func__,
                 esp_err_to_name(ret));
    }
    /* Set TX power */
    ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N9);
    if (ret) {
        ESP_LOGE("main", "%s set adv tx power failed: %s", __func__,
                 esp_err_to_name(ret));
    }
    /* Register callback */
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE("main", "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE("main", "gap register error, error code = %x", ret);
        return;
    }
    /* 注册应用 ID */
    ret = esp_ble_gatts_app_register(0x55);
    if (ret) {
        ESP_LOGE("main", "gatts app register error, error code = %x", ret);
        return;
    }

    /* 更新安全参数 */
    /* 请求进行安全连接、MITM、绑定认证 */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    /* 设备 IO 能力（决定了认证的方式）：具有 Yes/No 按钮 */
    esp_ble_io_cap_t iocap = ESP_IO_CAP_IO;
    /* 密钥大小 */
    uint8_t key_size = 16;
    /* 初始化密钥 */
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    /* 相应密钥 */
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    /* 认证选项 */
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    /* 带外（Out of Band）传输支持 */
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device acts as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the master;
    If your BLE device acts as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

/**
 * @brief 舵机控制任务入口
 *
 * @param parameter 参数指针
 */
void servo_task_entry(void *parameter) {
    /* 舵机配置 */
    servo_config_t servo1 = {
        .mcpwm_num   = MCPWM_UNIT_0,
        .mcpwm_timer = MCPWM_TIMER_0,
        .io_signal   = MCPWM0A,
        .pin         = 13,
    };

    /* 初始化舵机控制，MCPWM */
    servo_init(&servo1);
    servo_set_angle(&servo1, 10);

    while (1) {
        /* 等待信号量 */ 
        xSemaphoreTake(sem1, portMAX_DELAY);
        /* 设置舵机角度 */
        servo_set_angle(&servo1, 110);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        servo_set_angle(&servo1, 10);
    }
}

/**
 * @brief 按钮中断服务函数
 * 
 * @param arg 参数指针
 */
void gpio0_isr(void *arg) {
    /* 同意蓝牙绑定 */
    xSemaphoreGiveFromISR(sem2, NULL);
}
