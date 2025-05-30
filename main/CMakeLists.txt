
FILE(GLOB_RECURSE SOURCES "src/*.c")

idf_component_register(SRCS "main.c" ${SOURCES}
        PRIV_REQUIRES spi_flash
        REQUIRES
            bt
            nvs_flash
            esp_wifi
            esp_netif
            esp_event
            wifi_provisioning
            esp_driver_gpio
            esp_http_client
            led_strip
            app_update
            esp_system
            pn532
            esp-tls
        INCLUDE_DIRS "include")
