idf_component_register(SRCS "main.c" "hid_handler.c" "script_runner.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash driver esp_tinyusb)