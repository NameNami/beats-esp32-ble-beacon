/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "gap.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "cJSON.h"

typedef struct {
    const char* ssid;
    const char* password;
} wifi_network_t;

// List networks in order of priority
static const wifi_network_t wifi_networks[] = {
    {"Dany - 5 GHZ", "tunggujap"}, // esp32 cant use 5GHZ btw, just to test the fallback mechanism
    {"Dany - 2.4 GHZ", "tunggujap"},
    {"najis", "sanusi73"}
};

static const char *server_root_cert = "-----BEGIN CERTIFICATE-----\n"
"MIICnzCCAiWgAwIBAgIQf/MZd5csIkp2FV0TttaF4zAKBggqhkjOPQQDAzBHMQsw\n"
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
"MBIGA1UEAxMLR1RTIFJvb3QgUjQwHhcNMjMxMjEzMDkwMDAwWhcNMjkwMjIwMTQw\n"
"MDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZp\n"
"Y2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARvzTr+\n"
"Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4mMTG6J7/EFmP\n"
"LCaY9eYmJbsPAvpWo4H+MIH7MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggr\n"
"BgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQU\n"
"kHeSNWfE/6jMqeZ72YB5e8yT+TgwHwYDVR0jBBgwFoAUgEzW63T/STaj1dj8tT7F\n"
"avCUHYwwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzAChhhodHRwOi8vaS5wa2ku\n"
"Z29vZy9yNC5jcnQwKwYDVR0fBCQwIjAgoB6gHIYaaHR0cDovL2MucGtpLmdvb2cv\n"
"ci9yNC5jcmwwEwYDVR0gBAwwCjAIBgZngQwBAgEwCgYIKoZIzj0EAwMDaAAwZQIx\n"
"AOcCq1HW90OVznX+0RGU1cxAQXomvtgM8zItPZCuFQ8jSBJSjz5keROv9aYsAm5V\n"
"sQIwJonMaAFi54mrfhfoFNZEfuNMSQ6/bIBiNLiyoX46FohQvKeIoJ99cx7sUkFN\n"
"7uJW\n"
"-----END CERTIFICATE-----";

// Track which network we are currently trying to connect to
static int current_network_index = 0;
#define NUM_NETWORKS (sizeof(wifi_networks) / sizeof(wifi_networks[0]))

// Event handler to automatically reconnect if Wi-Fi drops
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("Failed to connect to %s.\n", wifi_networks[current_network_index].ssid);
        
        // Move to the next network in the list (loop back to 0 if at the end)
        current_network_index = (current_network_index + 1) % NUM_NETWORKS;
        
        printf("Trying next network: %s...\n", wifi_networks[current_network_index].ssid);
        
        // Dynamically reconfigure Wi-Fi settings with new credentials
        wifi_config_t wifi_config = {0};
        strncpy((char*)wifi_config.sta.ssid, wifi_networks[current_network_index].ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, wifi_networks[current_network_index].password, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf("Successfully connected to %s! IP: " IPSTR "\n", 
               wifi_networks[current_network_index].ssid, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void) {
    // 1. Initialize the underlying TCP/IP network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Setup Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Register our event handler to listen for connection/disconnection
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 4. Set the SSID and Password
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifi_networks[0].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_networks[0].password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

#define BLUE_LED_GPIO 2
#define RED_LED_GPIO 4

void heartbeat_task(void *pvParameters) {
    // 1. Configure GPIOs for LEDs
    gpio_reset_pin(BLUE_LED_GPIO);
    gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(RED_LED_GPIO);
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);

    // Turn on RED LED at startup
    gpio_set_level(RED_LED_GPIO, 1);

    // Wait a few seconds on boot to ensure connection is established
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    uint8_t raw_mac[6];
    char mac_address_str[18];
    char post_data[64];

    // Read the factory MAC address from eFuse
    if (esp_efuse_mac_get_default(raw_mac) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address");
        vTaskDelete(NULL);
        return;
    }

    // Format the hex values into a lowercase string
    snprintf(mac_address_str, sizeof(mac_address_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             raw_mac[0], raw_mac[1], raw_mac[2], 
             raw_mac[3], raw_mac[4], raw_mac[5]);

    snprintf(post_data, sizeof(post_data), "{\"mac_address\":\"%s\"}", mac_address_str);

    while(1) {
        printf("Sending heartbeat to server...\n");

        esp_http_client_config_t config = {
            .host = "beats.namix.my",
            .path = "/api/beacon/heartbeat",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .method = HTTP_METHOD_POST,
            .cert_pem = server_root_cert,
        };
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "Accept", "application/json");
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            printf("HTTP POST Status = %d\n", status_code);
            
            // Read response body
            int content_length = esp_http_client_get_content_length(client);
            char *response_buffer = NULL;
            if (content_length > 0) {
                response_buffer = malloc(content_length + 1);
                if (response_buffer) {
                    int read_len = esp_http_client_read_response(client, response_buffer, content_length);
                    if (read_len > 0) {
                        response_buffer[read_len] = '\0';
                        printf("Response Content: %s\n", response_buffer);
                    }
                }
            }

            if (status_code == 200) {
                // Success: Blink Blue LED
                gpio_set_level(BLUE_LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(BLUE_LED_GPIO, 0);
                
                if (response_buffer) {
                    cJSON *json = cJSON_Parse(response_buffer);
                    if (json) {
                        cJSON *uuid = cJSON_GetObjectItem(json, "uuid");
                        if (cJSON_IsString(uuid) && (uuid->valuestring != NULL)) {
                            printf("Parsed UUID: %s\n", uuid->valuestring);
                            if (update_beacon_uuid(uuid->valuestring)) {
                                // UUID Changed: Double blink Blue LED to distinguish from regular heartbeat
                                for (int i = 0; i < 2; i++) {
                                    gpio_set_level(BLUE_LED_GPIO, 1);
                                    vTaskDelay(pdMS_TO_TICKS(100));
                                    gpio_set_level(BLUE_LED_GPIO, 0);
                                    vTaskDelay(pdMS_TO_TICKS(100));
                                }
                            }
                        }
                        cJSON_Delete(json);
                    }
                }
            } else {
                // Request failed (not 200): Turn Blue LED ON steady
                gpio_set_level(BLUE_LED_GPIO, 1);
            }
            if (response_buffer) free(response_buffer);
        } else {
            printf("HTTP POST request failed: %s\n", esp_err_to_name(err));
            // Connection failed: Turn Blue LED ON steady
            gpio_set_level(BLUE_LED_GPIO, 1);
        }

        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(60000)); 
    }
}

void app_main(void)
{
    /* Local variables */
    int rc = 0;
    esp_err_t ret = ESP_OK;

    /* 1. Initialize NVS (Required for both Wi-Fi and BLE) */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE("APP_MAIN", "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* 2. Initialize Wi-Fi */
    wifi_init_sta();

    /* 3. NimBLE host stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE("APP_MAIN", "failed to initialize nimble stack, error code: %d ", ret);
        return;
    }

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    /* 4. GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE("APP_MAIN", "failed to initialize GAP service, error code: %d", rc);
        return;
    }
#endif

    /* 5. NimBLE host configuration initialization */
    nimble_host_config_init();

    /* 6. Start NimBLE host task thread */
    xTaskCreatePinnedToCore(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL, 0);

    /* 7. Start the API Heartbeat Task */
    xTaskCreatePinnedToCore(heartbeat_task, "Heartbeat", 4096, NULL, 5, NULL, 1);
    return;
}
