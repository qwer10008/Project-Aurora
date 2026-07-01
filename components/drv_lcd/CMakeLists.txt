idf_component_register(SRCS "lcd.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp_mm)
target_link_libraries(${COMPONENT_LIB} INTERFACE m)
