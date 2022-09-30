idf_component_register(SRCS "main.c"
                            "ble.c"
                            "wifi.c"
                            "powerblade.c"
               INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")