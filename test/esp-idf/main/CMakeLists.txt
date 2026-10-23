set(root "${CMAKE_CURRENT_LIST_DIR}/../../..")
file(GLOB core_srcs "${root}/core/*.c")
file(GLOB port_srcs "${root}/port/esp32/*.c")

idf_component_register(
    SRCS "main.c" ${core_srcs} ${port_srcs}
    INCLUDE_DIRS "${root}/core"
    PRIV_REQUIRES nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
