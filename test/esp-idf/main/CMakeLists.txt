set(root "${CMAKE_CURRENT_LIST_DIR}/../../..")
file(GLOB core_srcs "${root}/core/*.c")
file(GLOB port_srcs "${root}/port/esp32/*.c")

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(wdt_hal esp_hal_wdt)
else()
    set(wdt_hal hal)
endif()

idf_component_register(
    SRCS "main.c" ${core_srcs} ${port_srcs}
    INCLUDE_DIRS "${root}/include"
    PRIV_INCLUDE_DIRS "${root}/core" "${root}/port/esp32"
    PRIV_REQUIRES nvs_flash app_update esp_partition esp_timer esp_hw_support
                  esp_http_client mbedtls ${wdt_hal}
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
