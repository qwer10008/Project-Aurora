idf_component_register(SRCS "ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl drv_lcd drv_touch esp_timer)
