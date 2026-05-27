/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gap.h"
#include "common.h"
#include "esp_bt.h"

/* Private function declarations */
inline static void format_addr(char *addr_str, uint8_t addr[]);
static void start_advertising(void);

/* Private variables */
static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};
static uint8_t beacon_uuid[16] = {0}; // Store current UUID
static bool uuid_initialized = false;

/* Private functions */
inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void start_advertising(void) {
    /* Local variables */
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* Set advertising flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* Set iBeacon-style Manufacturer Specific Data if UUID is available */
    uint8_t mfg_data[25];
    if (uuid_initialized) {
        // Apple's Company ID (0x004c)
        mfg_data[0] = 0x4c;
        mfg_data[1] = 0x00;
        // iBeacon type (0x02) and data length (0x15)
        mfg_data[2] = 0x02;
        mfg_data[3] = 0x15;
        // 16-byte UUID
        memcpy(&mfg_data[4], beacon_uuid, 16);
        // Major (0), Minor (0)
        mfg_data[20] = 0x00; mfg_data[21] = 0x00; // Major
        mfg_data[22] = 0x00; mfg_data[23] = 0x00; // Minor
        
        /* TX Power Calibration Value at 1 meter. 
         * Since we are setting radio power to +9dBm, we use -51dBm as calibration.
         * -51 in 2's complement is 0xCD.
         */
        mfg_data[24] = 0xCD; 
        
        adv_fields.mfg_data = mfg_data;
        adv_fields.mfg_data_len = 25;
    }

    /* Set actual radio TX power to maximum (+9dBm) */
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

    /* Set advertisement fields */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    /* Set non-connectable and general discoverable mode to be a beacon */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Start advertising */
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started with UUID!");
}

/* Public functions */
bool update_beacon_uuid(const char *uuid_str) {
    uint8_t new_uuid[16];
    // Simple UUID string to hex conversion
    int i = 0, j = 0;
    while (uuid_str[i] != '\0' && j < 16) {
        if (uuid_str[i] == '-') {
            i++;
            continue;
        }
        unsigned int byte;
        sscanf(&uuid_str[i], "%02x", &byte);
        new_uuid[j++] = (uint8_t)byte;
        i += 2;
    }

    if (j == 16) {
        if (!uuid_initialized || memcmp(beacon_uuid, new_uuid, 16) != 0) {
            if (uuid_initialized) {
                ESP_LOGI(TAG, "UUID Changed! Old: %02x%02x... New: %s", beacon_uuid[0], beacon_uuid[1], uuid_str);
            } else {
                ESP_LOGI(TAG, "Initial UUID Set: %s", uuid_str);
            }
            memcpy(beacon_uuid, new_uuid, 16);
            uuid_initialized = true;
            
            // Restart advertising to apply new UUID
            ble_gap_adv_stop();
            start_advertising();
            return true; // Indicate change occurred
        }
        return false; // No change
    } else {
        ESP_LOGE(TAG, "Invalid UUID string length: %s", uuid_str);
        return false;
    }
}

/* Public functions */
void adv_init(void) {
    /* Local variables */
    int rc = 0;
    char addr_str[18] = {0};

    /* Make sure we have proper BT identity address set */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    /* Figure out BT address to use while advertising */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    /* Copy device address to addr_val */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    /* Start advertising. */
    start_advertising();
}

int gap_init(void) {
    /* Local variables */
    int rc = 0;

    /* Initialize GAP service */
    ble_svc_gap_init();

    /* Set GAP device name */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }

    /* Set GAP device appearance */
    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return rc;
    }
    return rc;
}
