#include "bsp/ble.h"

#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "string.h"

#define DEVICE_NAME            "BLE Door Lock"
#define GAP_EVENT_HANDLER_TAG  "gap_event_handler"
#define GATT_EVENT_HANDLER_TAG "gatt_event_handler"

/* Semaphore handle */
extern SemaphoreHandle_t sem1;
extern SemaphoreHandle_t sem2;

const uint8_t manufacturer_data[8] = "\x6B\x11\xD4\xFF\x69\x38\x64\xE4";
const uint8_t service_uuid[16] =
    "\xC1\xF5\xB3\xFA\x7B\xEF\x56\x7B\xEC\x8E\xA6\x52\x81\x29\xF0\x00";
const uint8_t value_uuid[16] =
    "\xC1\xF5\xB3\xFA\x7B\xEF\x56\x7B\xEC\x8E\xA6\x52\x81\x29\xF0\x01";

const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
const uint16_t char_dec_uuid        = ESP_GATT_UUID_CHAR_DECLARE;
const uint8_t  char_prop_write      = ESP_GATT_CHAR_PROP_BIT_WRITE;

/* Door lock status */
uint8_t door_lock_status = 0;

/* 广播数据 */
static esp_ble_adv_data_t door_lock_adv_config = {
    .set_scan_rsp        = false,
    .include_name        = false,
    .manufacturer_len    = sizeof(manufacturer_data),
    .p_manufacturer_data = (uint8_t *)manufacturer_data,
    .flag                = ESP_BLE_ADV_FLAG_GEN_`DISC,  // Generic discover
};

/* 广播参数 */;
static esp_ble_adv_params_t door_lock_adv_params = {
    .adv_int_min       = 0x080,
    .adv_int_max       = 0x190,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_RANDOM,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* GATT attribute table */
static const esp_gatts_attr_db_t door_lock_gatt_db[3] = {
    /* Door lock service decleration */
    [0] =
        {
            {ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *)&primary_service_uuid,
                ESP_GATT_PERM_READ,
                ESP_UUID_LEN_128,
                sizeof(service_uuid),
                (uint8_t *)service_uuid,
            },
        },
    /* Door lock status characteristic declaraion */
    [1] =
        {
            {ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *)&char_dec_uuid,
                ESP_GATT_PERM_READ,
                sizeof(uint8_t),
                sizeof(uint8_t),
                (uint8_t *)&char_prop_write,
            },
        },
    /* Door lock status value declaration */
    [2] =
        {
            {ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_128,
                (uint8_t *)value_uuid,
                ESP_GATT_PERM_WRITE_ENC_MITM,
                sizeof(uint8_t),
                sizeof(door_lock_status),
                (uint8_t *)&door_lock_status,
            },
        },
};
/* Service handle table */
uint16_t door_lock_handle_table[3];

/* Application 参数 */
static void gatts_profile_event_handler(esp_gatts_cb_event_t      event,
                                        esp_gatt_if_t             gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst door_lock_profile_tab[1] = {
    {
        .gatts_cb = gatts_profile_event_handler,
        /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

/**
 * @brief GATT 应用配置回调函数
 *
 * @param event 事件
 * @param gatts_if GATT 接口
 * @param param 回调函数参数
 */
static void gatts_profile_event_handler(esp_gatts_cb_event_t      event,
                                        esp_gatt_if_t             gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    ESP_LOGI(GAP_EVENT_HANDLER_TAG, "GATTS_PROFILE_EVT, event %d", event);
    switch (event) {
        /* 应用程序注册事件 */
        case ESP_GATTS_REG_EVT:
            esp_ble_gap_set_device_name(DEVICE_NAME);
            /* 生成一个随机可解析地址 */
            esp_ble_gap_config_local_privacy(true);
            esp_ble_gatts_create_attr_tab(door_lock_gatt_db, gatts_if, 3, 0);
            break;
        /* 客户端断开连接事件 */
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATT_EVENT_HANDLER_TAG,
                     "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x",
                     param->disconnect.reason);
            /* 当断开链接时重新开始广播 */
            esp_ble_gap_start_advertising(&door_lock_adv_params);
            break;
        /* 客户端连接事件 */
        case ESP_GATTS_CONNECT_EVT:
            esp_ble_set_encryption(param->connect.remote_bda,
                                   ESP_BLE_SEC_ENCRYPT_MITM);
            break;
        /* 创建 GATT 属性表事件 */
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(GATT_EVENT_HANDLER_TAG,
                         "create attribute table failed, error code=0x%x",
                         param->add_attr_tab.status);
            } else if (param->add_attr_tab.num_handle != 3) {
                ESP_LOGE(GATT_EVENT_HANDLER_TAG,
                         "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to 3",
                         param->add_attr_tab.num_handle);
            } else {
                ESP_LOGI(GATT_EVENT_HANDLER_TAG,
                         "create attribute table successfully, the number "
                         "handle = %d\n",
                         param->add_attr_tab.num_handle);
                memcpy(door_lock_handle_table, param->add_attr_tab.handles,
                       sizeof(door_lock_handle_table));
                esp_ble_gatts_start_service(door_lock_handle_table[0]);
            }
            break;
        /* 读取属性事件 */
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(GATT_EVENT_HANDLER_TAG, "ESP_GATTS_READ_EVT");
            break;
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(GATT_EVENT_HANDLER_TAG,
                     "client request to write, handle: %d",
                     param->write.handle);
            xSemaphoreGive(sem1);
            break;
        default:
            break;
    }
}

/**
 * @brief BLE GAP 事件回调函数
 *
 * @param event 回调事件
 * @param param 回调事件参数
 */
void gap_event_handler(esp_gap_ble_cb_event_t  event,
                       esp_ble_gap_cb_param_t *param) {
    ESP_LOGI(GAP_EVENT_HANDLER_TAG, "GAP_EVT, event %d", event);

    switch (event) {
        /* 广播数据设置完成事件 */
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&door_lock_adv_params);
            break;
        /* 本地隐私设置完成事件 */
        case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: {
            esp_err_t ret = esp_ble_gap_config_adv_data(&door_lock_adv_config);
            if (ret) {
                ESP_LOGE(GAP_EVENT_HANDLER_TAG,
                         "config adv data failed, error code = %x", ret);
            }
            break;
        }
        /* 请求密码比较事件 */
        case ESP_GAP_BLE_NC_REQ_EVT:
            ESP_LOGI(GAP_EVENT_HANDLER_TAG,
                     "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d",
                     param->ble_security.key_notif.passkey);
            /* 亮灯提示 */
            gpio_set_level(GPIO_NUM_2, 1);
            /* 清除信号量 */
            xSemaphoreTake(sem2, 0);
            /* 5s 内按下按钮， 同意配对 */
            if (xSemaphoreTake(sem2, 5000 / portTICK_PERIOD_MS) == pdTRUE) {
                esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr,
                                      true);
            } else {
                esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr,
                                      false);
            }
            gpio_set_level(GPIO_NUM_2, 0);
            break;
        // GAP 认证完成事件
        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
            esp_bd_addr_t bd_addr;
            memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr,
                   sizeof(esp_bd_addr_t));
            ESP_LOGI(GAP_EVENT_HANDLER_TAG, "remote BD_ADDR: %08x%04x",
                     (bd_addr[0] << 24) + (bd_addr[1] << 16) +
                         (bd_addr[2] << 8) + bd_addr[3],
                     (bd_addr[4] << 8) + bd_addr[5]);
            ESP_LOGI(GAP_EVENT_HANDLER_TAG, "address type = %d",
                     param->ble_security.auth_cmpl.addr_type);
            ESP_LOGI(
                GAP_EVENT_HANDLER_TAG, "pair status = %s",
                param->ble_security.auth_cmpl.success ? "success" : "fail");
            break;
        }
        default:
            break;
    }
}

/**
 * @brief BLE GATT 事件回调函数
 *
 * @param event 回调事件
 * @param gatts_if 回调接口
 * @param param 回调事件参数
 */
void gatts_event_handler(esp_gatts_cb_event_t      event,
                         esp_gatt_if_t             gatts_if,
                         esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            ESP_LOGI(GATT_EVENT_HANDLER_TAG, "reg app success, app_id %04x",
                     param->reg.app_id);
            door_lock_profile_tab[0].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATT_EVENT_HANDLER_TAG,
                     "reg app failed, app_id %04x, status %d",
                     param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < 1; idx++) {
            if (gatts_if ==
                    ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a
                                           certain gatt_if, need to call
                                           every profile cb function */
                gatts_if == door_lock_profile_tab[idx].gatts_if) {
                if (door_lock_profile_tab[idx].gatts_cb) {
                    door_lock_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}
