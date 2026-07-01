/**
 * @file lv_conf.h
 * Configuration for LVGL 9.5.0 on ESP32-S3 (Project Aurora)
 * RGB565 16-bit, stdlib malloc (PSRAM), minimal features
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Enable LVGL configuration */
#if 1

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16                    /* RGB565 matches LCD frame buffer */
#define LV_COLOR_16_SWAP 0

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
/* Use standard C library malloc so allocations go through ESP-IDF heap (PSRAM) */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

/*====================
   HAL SETTINGS
 *====================*/
/* Use the standard OSAL (FreeRTOS via ESP-IDF) */
#define LV_USE_OS               LV_OS_FREERTOS

/* Tick period in ms - our hardware timer fires at 5ms, LVGL sees 1ms tick */
#define LV_DEF_REFR_PERIOD      33           /* LVGL refresh period ~30fps */

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_48   1            /* Large digits for clock display */
#define LV_FONT_DEFAULT         &lv_font_montserrat_48

/*====================
   WIDGETS
 *====================*/
/* Only enable widgets we need to save flash */
#define LV_USE_LABEL            1
#define LV_USE_BTN              0
#define LV_USE_SLIDER           0
#define LV_USE_DROPDOWN         0
#define LV_USE_ROLLER           0
#define LV_USE_TABLE            0
#define LV_USE_TEXTAREA         0
#define LV_USE_CALENDAR         0
#define LV_USE_CHART            0
#define LV_USE_CANVAS           0
#define LV_USE_LINE             0
#define LV_USE_IMAGE            0
#define LV_USE_ARC              0
#define LV_USE_SPINNER          0
#define LV_USE_ANIMIMG          0
#define LV_USE_KEYBOARD         0
#define LV_USE_LIST             0
#define LV_USE_MENU             0
#define LV_USE_MSGBOX           0
#define LV_USE_WIN              0
#define LV_USE_TABVIEW          0
#define LV_USE_TILEVIEW         0
#define LV_USE_LED              0
#define LV_USE_IMGBTN           0
#define LV_USE_SWITCH           0
#define LV_USE_CHECKBOX         0
#define LV_USE_BAR              0
#define LV_USE_SPAN             0
#define LV_USE_SPINBOX          0
#define LV_USE_SCALE            0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_SIMPLE     0
#define LV_USE_THEME_MONO       0

/*====================
   OTHERS
 *====================*/
#define LV_USE_OBJ_ID           0
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_INFO

/* Size of the memory pool used by LVGL's built-in allocator.
   But since we set LV_USE_STDLIB_MALLOC = CLIB, this is not used.
   LVGL will use malloc/free from the C library instead. */

/*---------------------
   DRAW CONFIG
 *--------------------*/
/* Use the built-in SW renderer (no GPU needed) */
#define LV_USE_DRAW_SW          1
#define LV_DRAW_SW_DRAW_UNIT_CNT    1

#endif /* LV_CONF_H */
#endif
