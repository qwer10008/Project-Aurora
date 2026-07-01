idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES drv_rtc drv_lcd drv_touch drv_audio drv_storage drv_ui driver freertos esp_lcd esp_timer)
