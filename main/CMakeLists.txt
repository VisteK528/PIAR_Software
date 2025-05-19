
FILE(GLOB_RECURSE SOURCES "src/*.c")

idf_component_register(SRCS "main.c" ${SOURCES}
        PRIV_REQUIRES spi_flash
        REQUIRES esp_driver_gpio esp_wifi esp_http_server nvs_flash led_strip
        INCLUDE_DIRS "include")
