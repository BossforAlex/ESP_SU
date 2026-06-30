idf_component_register(SRCS "main.c" "usb_host.c" "fastboot.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash driver)
target_include_directories(${COMPONENT_LIB} PRIVATE ".")