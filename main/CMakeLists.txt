idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES drv_lcd drv_rtc drv_audio drv_touch drv_storage)
target_link_libraries(${COMPONENT_LIB} INTERFACE m)
