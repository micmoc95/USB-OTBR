#include "esp_ot_br.h"

bool wifi_mode(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_NUM_0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    for (int i = 0 ; i < 100 ; i++) {
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

static esp_err_t netif_transmit_callback(void *h, void *buffer, size_t len) {
	tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100));
	return ESP_OK;
}

static esp_err_t netif_recv_callback(void *buffer, uint16_t len, void *ctx) {
	void *buf_copy = malloc(len);
	if (!buf_copy) {
		return ESP_ERR_NO_MEM;
	}
	memcpy(buf_copy, buffer, len);
	return esp_netif_receive(*(esp_netif_t **)ctx, buf_copy, len, NULL);
}

static esp_err_t init_spiffs(void) {
	esp_vfs_spiffs_conf_t web_server_conf = {
	    .base_path = "/spiffs", .partition_label = "web_storage", .max_files = 10, .format_if_mount_failed = false
	};
	ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&web_server_conf), TAG, "Failed to mount web storage");
	return ESP_OK;
}

static void got_ipv4_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
	ESP_LOGI(TAG, "Got IPV4 event: Interface \"%s\" address:" IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));

	SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;
	if (sem) {
		xSemaphoreGive((SemaphoreHandle_t)arg);
	}
}

static void got_ipv6_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
	ESP_LOGI(TAG, "Got IPV6 event: Interface \"%s\" address:" IPV6STR , esp_netif_get_desc(event->esp_netif), IPV62STR(event->ip6_info.ip));

	SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;
	if (sem) {
		xSemaphoreGive((SemaphoreHandle_t)arg);
	}
}

static void netif_free_callback(void *h, void *buffer) {
	if (buffer != NULL) free(buffer);
}

static esp_netif_t *get_usb_netif() {
	SemaphoreHandle_t sem_got_ipv4 = xSemaphoreCreateBinary();
	SemaphoreHandle_t sem_got_ipv6 = xSemaphoreCreateBinary();

	const tinyusb_config_t tusb_config = TINYUSB_DEFAULT_CONFIG();
	ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_config));
	ESP_LOGI(TAG, "driver installed");

	static esp_netif_t *s_netif = NULL;
	tinyusb_net_config_t net_config = {
	    .on_recv_callback = netif_recv_callback,
	    .free_tx_buffer = NULL, //netif_free_callback,
		.user_context = &s_netif,
	};
	uint8_t mac[6] =  {0x02, 0x12, 0x34, 0x56, 0x78, 0x90};
	memcpy(net_config.mac_addr, mac, sizeof(mac));
	ESP_ERROR_CHECK(tinyusb_net_init(&net_config));
	ESP_LOGI(TAG, "net init done");

	esp_netif_inherent_config_t base_config = {
	    .flags = ESP_NETIF_FLAG_AUTOUP | ESP_NETIF_FLAG_GARP | ESP_NETIF_DEFAULT_ARP_FLAGS | ESP_NETIF_FLAG_EVENT_IP_MODIFIED | ESP_NETIF_DEFAULT_IPV6_AUTOCONFIG_FLAGS | ESP_NETIF_FLAG_IS_BRIDGE | ESP_NETIF_FLAG_MLDV6_REPORT,
	    .get_ip_event = IP_EVENT_ETH_GOT_IP,
	    .if_key = "USB-NETIF",
	    .if_desc = "usb-netif",
	    .route_prio = 10,
	};
	esp_netif_driver_ifconfig_t driver_config = {
	    .handle = (void *)1,
	    .transmit = netif_transmit_callback,
	    .driver_free_rx_buffer = netif_free_callback
	};
	struct esp_netif_netstack_config lwip_netif_config = {
	    .lwip = {
	        .init_fn = ethernetif_init,
	        .input_fn = ethernetif_input
	    }
	};
	esp_netif_config_t cfg = {
	    .base = &base_config,
	    .driver = &driver_config,
	    .stack = &lwip_netif_config,
	};
	s_netif = esp_netif_new(&cfg);
	ESP_LOGI(TAG, "netif created");

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ipv4_handler, (void *)sem_got_ipv4));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &got_ipv6_handler, (void *)sem_got_ipv6));

	ESP_ERROR_CHECK(esp_netif_set_mac(s_netif, mac));
	esp_netif_action_start(s_netif, 0, 0, 0);
	esp_netif_action_connected(s_netif, NULL, 0, NULL);
	esp_netif_create_ip6_linklocal(s_netif);
	ESP_LOGI(TAG, "netif connected");
	
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_netif));
	esp_netif_ip_info_t ip_info = {0};
	ip_info.ip.addr = ipaddr_addr(CONFIG_USB_NETIF_IP);
	ip_info.gw.addr = ipaddr_addr(CONFIG_USB_NETIF_GATEWAY);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_USB_NETIF_NETMASK);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif, &ip_info));

	xSemaphoreTake(sem_got_ipv4, portMAX_DELAY);
	ESP_LOGI(TAG, "IPV4 lock released");

	xSemaphoreTake(sem_got_ipv6, portMAX_DELAY);
	ESP_LOGI(TAG, "IPV6 lock released");

	ESP_ERROR_CHECK(mdns_init());
	ESP_ERROR_CHECK(mdns_hostname_set("esp-ot-br"));
	ESP_ERROR_CHECK(mdns_register_netif(s_netif));
	//ESP_ERROR_CHECK(mdns_netif_action(s_netif, MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ENABLE_IP6 | MDNS_EVENT_ANNOUNCE_IP6));
	ESP_ERROR_CHECK(mdns_netif_action(s_netif, MDNS_EVENT_ENABLE_IP6 | MDNS_EVENT_ANNOUNCE_IP6));

	return s_netif;
}

static void ot_br_init(void *ctx) {
	esp_openthread_lock_acquire(portMAX_DELAY);
	ESP_ERROR_CHECK(esp_openthread_border_router_init());

	otOperationalDatasetTlvs dataset;
	otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
	ESP_ERROR_CHECK(esp_openthread_auto_start(&dataset));

	esp_openthread_lock_release();
	vTaskDelete(NULL);
}

void app_main(void) {
	esp_vfs_eventfd_config_t eventfd_config = {.max_fds = 3,};
	ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
	ESP_ERROR_CHECK(nvs_flash_init());   
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_LOGI(TAG, "init done");

	ESP_ERROR_CHECK(init_spiffs());
	esp_br_web_start("/spiffs"); 
	ESP_LOGI(TAG, "web started");

	ot_console_start();
	ESP_LOGI(TAG, "console started");

	esp_openthread_config_t openthread_config = {
	    .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
	    .platform_config = {
	            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
	            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
	            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
	        },
	};

	esp_netif_t *s_netif = NULL;

	if (wifi_mode()) {
		ESP_ERROR_CHECK(esp_ot_wifi_connect("MICIOT", "1590159346002"));
		s_netif = get_example_netif();
		ESP_ERROR_CHECK(mdns_init());
		ESP_ERROR_CHECK(mdns_hostname_set("esp-ot-br"));
		ESP_LOGI(TAG, "Connected via Wi-Fi");
	} 
	else {
		s_netif = get_usb_netif();
		ESP_LOGI(TAG, "Connected via USB-Fi");
	}
	assert(s_netif != NULL);

	esp_openthread_set_backbone_netif(s_netif);
	ESP_LOGI(TAG, "backbone configured");

	ESP_ERROR_CHECK(esp_openthread_start(&openthread_config));
	ESP_LOGI(TAG, "openthread started");

	esp_cli_custom_command_init();
	ot_register_external_commands();
	ESP_LOGI(TAG, "client console started");

	xTaskCreate(ot_br_init, "ot_br_init", 6144, NULL, 4, NULL);
}
