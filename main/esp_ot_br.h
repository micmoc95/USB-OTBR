// esp-idf/components/console
#include "esp_console.h"

// esp-idf/components/esp_common
#include "esp_check.h"
#include "esp_err.h"

// esp-idf/components/esp_event
#include "esp_event.h"

// esp-idf/components/esp_hw_support
#include "esp_mac.h"

// esp-idf/components/esp_netif
#include "esp_netif.h"

// esp-idf/components/log
#include "esp_log.h"

// esp-idf/components/lwip
#include "lwip/netif.h"
#include "lwip/esp_netif_net_stack.h"

// esp-idf/components/nvs_flash
#include "nvs_flash.h"

// esp-idf/components/openthread
#include "esp_openthread_border_router.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "mdns.h"
#include "openthread/dataset_ftd.h"

// esp-idf/components/spiffs
#include "esp_spiffs.h"

// esp-idf/components/vfs
#include "esp_vfs_eventfd.h"

// usb-thread-br/components/esp_ot_br_server
#include "esp_br_web.h"

// usb-thread-br/components/esp_ot_cli_extension
#include "esp_ot_cli_extension.h"
#include "esp_ot_ota_commands.h"

// usb-thread-br/components/ot_examples_common
#include "ot_examples_common.h"

// usb-thread-br/managed_components/espressif__esp_tinyusb
#include "tinyusb_default_config.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

// usb-thread-br/managed_components/espressif__mdns
#include "mdns.h"

#define TAG "esp_ot_br"
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()              \
    {                                                      \
        .radio_mode = RADIO_MODE_UART_RCP,                 \
        .radio_uart_config = {                             \
            .port = 1,                                     \
            .uart_config =                                 \
                {                                          \
                    .baud_rate = 460800,                   \
                    .data_bits = UART_DATA_8_BITS,         \
                    .parity = UART_PARITY_DISABLE,         \
                    .stop_bits = UART_STOP_BITS_1,         \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
                    .rx_flow_ctrl_thresh = 0,              \
                    .source_clk = UART_SCLK_DEFAULT,       \
                },                                         \
            .rx_pin = 17,                \
            .tx_pin = 18,                \
        },                                                 \
    }
#define ESP_OPENTHREAD_RCP_UPDATE_CONFIG()                                                                   \
    {                                                                                                        \
        .rcp_type = RCP_TYPE_UART, .uart_rx_pin = 17, .uart_tx_pin = 18, \
        .uart_port = 1, .uart_baudrate = 115200, .reset_pin = 7,                       \
        .boot_pin = 8, .update_baudrate = 460800,                                       \
        .firmware_dir = "/" CONFIG_RCP_PARTITION_NAME "/ot_rcp", .target_chip = ESP32H2_CHIP         \
    }
#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()               \
    {                                                      \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }
#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
