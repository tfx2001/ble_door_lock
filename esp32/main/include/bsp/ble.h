#ifndef __BLE_H
#define __BLE_H

#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"

struct gatts_profile_inst {
    esp_gatts_cb_t       gatts_cb;
    uint16_t             gatts_if;
    uint16_t             app_id;
    uint16_t             conn_id;
    uint16_t             service_handle;
    esp_gatt_srvc_id_t   service_id;
    uint16_t             char_handle;
    esp_bt_uuid_t        char_uuid;
    esp_gatt_perm_t      perm;
    esp_gatt_char_prop_t property;
    uint16_t             descr_handle;
    esp_bt_uuid_t        descr_uuid;
};

void gap_event_handler(esp_gap_ble_cb_event_t  event,
                           esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t      event,
                             esp_gatt_if_t             gatts_if,
                             esp_ble_gatts_cb_param_t *param);

#endif