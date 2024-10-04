#include "includes.h"

#include "NetworkConnection.h"

#include "esp_netif_sntp.h"

LOG_TAG(NetworkConnection);

NetworkConnection *NetworkConnection::_instance = nullptr;

NetworkConnection::NetworkConnection(Queue *synchronizationQueue)
    : _synchronization_queue(synchronizationQueue), _attempt(0), _have_sntp_synced(false) {
    _instance = this;
}

void NetworkConnection::begin() {
    _wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instanceAnyId;
    esp_event_handler_instance_t instanceGotIp;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((NetworkConnection *)eventHandlerArg)->event_handler(eventBase, eventId, eventData);
        },
        this, &instanceAnyId));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((NetworkConnection *)eventHandlerArg)->event_handler(eventBase, eventId, eventData);
        },
        this, &instanceGotIp));

    wifi_config_t wifiConfig = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,

                // Authmode threshold resets to WPA2 as default if password
                // matches WPA2 standards (pasword len => 8). If you want to
                // connect the device to deprecated WEP/WPA networks, Please set
                // the threshold value to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and
                // set the password with length and format matching to
                // WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.

                .threshold =
                    {
                        .authmode = WIFI_AUTH_WPA2_PSK,
                    },
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
                .sae_h2e_identifier = "",
            },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Finished setting up WiFi");
}

void NetworkConnection::event_handler(esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to AP, attempt %d", _attempt + 1);
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        auto event = (wifi_event_sta_disconnected_t *)eventData;

        // Reasons are defined here:
        // https://github.com/espressif/esp-idf/blob/4b5b064caf951a48b48a39293d625b5de2132719/components/esp_wifi/include/esp_wifi_types_generic.h#L79.

        ESP_LOGW(TAG, "Disconnected from AP, reason %d", event->reason);

        if (_attempt++ < CONFIG_DEVICE_NETWORK_CONNECT_ATTEMPTS) {
            ESP_LOGI(TAG, "Retrying...");
            esp_wifi_connect();
        } else {
            _state_changed.queue(_synchronization_queue, {.connected = false, .errorReason = event->reason});
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        auto event = (ip_event_got_ip_t *)eventData;

        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        setup_sntp();
    }
}

void NetworkConnection::setup_sntp() {
    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    config.sync_cb = [](struct timeval *tv) {
        tm time_info;
        localtime_r(&tv->tv_sec, &time_info);

        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);
        ESP_LOGI(TAG, "Time synchronized to %s", time_str);

        if (!_instance->_have_sntp_synced) {
            _instance->_have_sntp_synced = true;

            _instance->_state_changed.queue(_instance->_synchronization_queue, {.connected = true, .errorReason = 0});
        }
    };

    esp_netif_sntp_init(&config);
}