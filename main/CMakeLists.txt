idf_component_register(
  SRC_DIRS "."
  INCLUDE_DIRS "."
  REQUIRES openthread esp_tinyusb nvs_flash ot_examples_common spiffs esp_ot_cli_extension esp_ot_br_server esp_eth
)
