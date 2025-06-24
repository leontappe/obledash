/*
 * Copyright 2021 LVGL LLC. All Rights Reserved.
 * Use of this source code is governed by a MIT-style license that can be
 * found in the LICENSE file or at https://opensource.org/licenses/MIT
 */

/* clang-format off */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH     16

/*Swap the 2 bytes of RGB565 color. Useful if the display controller requires it.
 *E.g. ILI9341 needs it.*/
#define LV_COLOR_16_SWAP   1  // Often needed for ILI9341

/*Enable features to draw on transparent background.
 *Useful if you need to draw effects on a background.
 *E.g. blur, color filter, opacity, etc.
 *Requires `LV_COLOR_DEPTH = 32` colors and the screen drivers shall support ARGB8888*/
#define LV_COLOR_SCREEN_TRANSP    0

/*Images pixels with this color will be transparent if `LV_COLOR_SCREEN_TRANSP` is not enabled.
 *`LV_COLOR_CHROMA_KEY` makes sense only if `LV_COLOR_DEPTH != 32`*/
#define LV_COLOR_CHROMA_KEY               lv_color_hex(0x00ff00)         /*Green*/

/*=========================
   MEMORY SETTINGS
 *=========================*/

/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()`*/
#define LV_MEM_CUSTOM      0
#if LV_MEM_CUSTOM == 0
/*Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB)*/
#  define LV_MEM_SIZE    (32U * 1024U)          /*[bytes]*/

/*Set an address for the memory pool instead of allocating it as a normal array. Can be in external SRAM too.*/
#  define LV_MEM_ADR          0     /*0: unused*/
/*Instead of an address give a memory allocator that will be called to get a memory pool for LVGL. E.g. my_alloc*/
#  if LV_MEM_ADR == 0
#    undef LV_MEM_POOL_INCLUDE
#    undef LV_MEM_POOL_ALLOC
#  endif

#endif     /*LV_MEM_CUSTOM*/

/*Number of the intermediate memory buffer used during rendering and other internal processing mechanisms.
 *You will see an error log message if there wasn't enough buffers. */
#define LV_MEM_BUF_MAX_NUM          16

/*Use the standard `memcpy` and `memset` instead of LVGL's own functions. (Might be faster)*/
#define LV_MEMCPY_MEMSET_STD    0

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh period.
 *Can be changed in the display driver (`lv_disp_drv_t`).*/
#define LV_DISP_DEF_REFR_PERIOD     30      /*[ms]*/

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD    30      /*[ms]*/

/*Use a custom tick source that tells the elapsed time in milliseconds.
 *It removes the need to call `lv_tick_inc()` in every ms.
 *`LV_TICK_CUSTOM_SYS_TIME_EXPR` should be an expression which gives the current system time in ms*/
#define LV_TICK_CUSTOM     1
#if LV_TICK_CUSTOM
#  define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"         /*Header for the system time function*/
#  define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())     /*Expression evaluating to current system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

/*Default Dot Per Inch*/
#define LV_DPI_DEF                  130     /*[dpi]*/

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Drawing
 *-----------*/

/*Enable complex draw engine.
 *Required to draw shadow, gradient, rounded corners, circles, arc, skew lines, image transformations or any masks*/
#define LV_DRAW_COMPLEX             1
#if LV_DRAW_COMPLEX != 0
/*Allow buffering some drawing tasks. This can speed up drawing complex scenes. */
#  define LV_DRAW_BUF_NU_LAYER         16   /*The number of layers here has a large impact on memory usage*/
#endif

/*Default image cache size. Image caching keeps the images opened.
 *If only the built-in image formats are used there is no real advantage of caching. (I.e. if no new image decoder is added)
 *With complex image decoders (e.g. PNG or JPG) caching can save the continuous open/decode of images.
 *However the opened images might consume additional RAM.
 *0: to disable caching*/
#define LV_IMG_CACHE_DEF_SIZE       0

/*Maximum buffer size to allocate for rotation. Only used if software rotation is enabled in the display driver.*/
# define LV_DISP_ROT_MAX_BUF        (10*1024)

/*-----------
 * GPU
 *-----------*/

/*Use STM32's DMA2D (aka Chrom ART) GPU*/
#define LV_USE_GPU_STM32_DMA2D  0
#if LV_USE_GPU_STM32_DMA2D
/*Must be defined to include path of CMSIS header of target processor
e.g. "stm32f769xx.h" or "stm32f429xx.h"*/
#  define LV_GPU_DMA2D_CMSIS_INCLUDE
#endif

/*Use NXP's PXP GPU i.MX RTxxx platforms*/
#define LV_USE_GPU_NXP_PXP      0
#if LV_USE_GPU_NXP_PXP
#  define LV_GPU_NXP_PXP_SIZE_LIMITATION_IGNORE    0   /*By default PXP is not used if image is too small (both dimensions are less than 32 px)*/
#  define LV_GPU_NXP_PXP_SHOW_WATERMARK            1
#endif

/*Use NXP's VG-Lite GPU i.MX RTxxx platforms*/
#define LV_USE_GPU_NXP_VG_LITE 0

/*Use SWM341's DMA2D GPU*/
#define LV_USE_GPU_SWM341_DMA2D  0
#if LV_USE_GPU_SWM341_DMA2D
#  define LV_GPU_SWM341_DMA2D_INCLUDE "SWM341.h"
#endif

/*-----------
 * Logging
 *-----------*/

/*Enable the log module*/
#define LV_USE_LOG      1
#if LV_USE_LOG

/*How important log should be added:
 *LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
 *LV_LOG_LEVEL_INFO        Log important events
 *LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
 *LV_LOG_LEVEL_ERROR       Only critical problems, when the system may fail
 *LV_LOG_LEVEL_USER        Only logs added by the user
 *LV_LOG_LEVEL_NONE        Do not log anything*/
#  define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN

/*1: Print the log with 'printf'; 0: User need to register a callback*/
#  define LV_LOG_PRINTF   1
/*The user log function can be registered with `lv_log_register_print_cb`*/
#endif  /*LV_USE_LOG*/

/*-------------
 * Asserts
 *-------------*/

/*Enable asserts if an operation is failed or an invalid data is found.
 *If LV_USE_LOG is enabled an error message will be printed on failure*/
#define LV_USE_ASSERT_NULL          1   /*Check if the parameter is NULL. (Very fast)*/
#define LV_USE_ASSERT_MALLOC        1   /*Checks is the memory is successfully allocated or LV_MEM_SIZE is too small. (Very fast)*/
#define LV_USE_ASSERT_STYLE         0   /*Check if the styles are properly initialized. (Very fast)*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*Check the integrity of `lv_mem` after critical operations. (Slow)*/
#define LV_USE_ASSERT_OBJ_PARENT    0   /*Check if the parent is still valid when handling an object. (Slow)*/

/*Add a custom handler when assert happens e.g. to restart the MCU*/
#define LV_ASSERT_HANDLER_INCLUDE       <stdint.h>
#define LV_ASSERT_HANDLER while(0);   /*Halt by default*/

/*-------------
 * Others
 *-------------*/

/*1: Show CPU usage and FPS count*/
#define LV_USE_PERF_MONITOR     0
#if LV_USE_PERF_MONITOR
#  define LV_USE_PERF_MONITOR_POS_TOP     0 /*Top; Left: 0; Right: 1*/
#  define LV_USE_PERF_MONITOR_POS_RIGHT   0
#endif

/*1: Show the used memory and the memory fragmentation
 * Requires LV_MEM_CUSTOM = 0*/
#define LV_USE_MEM_MONITOR      0
#if LV_USE_MEM_MONITOR
#  define LV_USE_MEM_MONITOR_POS_TOP      0 /*Top; Left: 0; Right: 1*/
#  define LV_USE_MEM_MONITOR_POS_RIGHT    0
#endif

/*1: Draw random colored rectangles over the redrawn areas*/
#define LV_USE_REFR_DEBUG       0

/*Change the built-in fonts configuration*/
#define LV_FONT_MONTSERRAT_12    1
#define LV_FONT_MONTSERRAT_14    1  /*Default font*/
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_DEFAULT          &lv_font_montserrat_14

/*Declare the fonts.
 *Greatly reduces the binary size if only a few fonts are used.
 *Other fonts need to be converted using Font converter tool: offline-font-converter.py
 *Every font status have to be defined if LV_FONT_CUSTOM_DECLARE is enabled*/
#define LV_FONT_CUSTOM_DECLARE /* Used to be: #define LV_FONT_CUSTOM_DECLARE 0 - causes build error with modern compilers if set to 0 directly here. Define as empty if no custom declaration.*/

/*Enable the Operating system abstraction layer*/
#define LV_USE_OS               LV_OS_NONE

/*---------------------
 *  TEXT SETTINGS
 *--------------------*/

/**
 * Select a character encoding for strings.
 * Your IDE or editor should have the same character encoding
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*Can break (wrap) texts on these chars*/
#define LV_TXT_BREAK_CHARS                  " ,.;:-_"

/*If characters are unknown, show this character.*/
#define LV_TXT_REPLACE_CHAR                 0xEF    /*REPLACEMENT CHARACTER (U+FFFD)*/

/*Enable bidi logical conversion and join letters in Arabic languages. Requires `LV_USE_ARABIC_PERSIAN_CHARS`*/
#define LV_USE_BIDI         0
#if LV_USE_BIDI
/*Set the default direction. Supported values:
 *`LV_BASE_DIR_LTR` Left-to-Right
 *`LV_BASE_DIR_RTL` Right-to-Left
 *`LV_BASE_DIR_AUTO` Detect base direction based on the first strong character
 *`LV_BASE_DIR_NEUTRAL` Like LTR but marks the text as neutral.
 */
#  define LV_BIDI_BASE_DIR_DEF  LV_BASE_DIR_AUTO
#endif

/*Enable Arabic/Persian processing
 *In this case characters like FETHATEN, DAMMATEN, KASRATEN, etc are used instead of FATHA, DAMMA, KASRA, etc to make it easier to
 *type words in Arabic script. This conversion is applied only if `LV_USE_BIDI` is enabled and the base direction is RTL*/
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 * WIDGETS
 *================*/

/*Documentation of the widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html*/

#define LV_USE_ARC        1
#define LV_USE_ANIMIMG    1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1   /*Requires LV_USE_LABEL*/
#define LV_USE_IMG        1   /*Requires LV_USE_LABEL*/
#define LV_USE_LABEL      1
#if LV_USE_LABEL
#  define LV_LABEL_TEXT_SELECTION     1 /*Enable selecting text of the label*/
#  define LV_LABEL_LONG_TXT_HINT      1 /*Store some extra info in labels to speed up drawing of long texts*/
#endif
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1   /*Requires LV_USE_LABEL*/
#define LV_USE_SLIDER     1   /*Requires LV_USE_BAR*/
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1   /*Requires LV_USE_LABEL*/
#if LV_USE_TEXTAREA != 0
#  define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500    /*ms*/
#endif
#define LV_USE_TABLE      1

/*==================
 * EXTRA THEMES
 *==================*/

/*-----------
 * Themes
 *-----------*/

/*A simple, elegant and light theme*/
#define LV_THEME_DEFAULT_DARK           0
#define LV_THEME_DEFAULT_LIGHT          1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_THEME_DEFAULT_FONT_SMALL         &lv_font_montserrat_12
#define LV_THEME_DEFAULT_FONT_NORMAL        &lv_font_montserrat_14
#define LV_THEME_DEFAULT_FONT_LARGE         &lv_font_montserrat_16

/*A theme designed for monochrome displays*/
#define LV_THEME_MONO_LIGHT             0   /*1: enable*/
#define LV_THEME_MONO_DARK              0   /*1: enable*/

/*Right to left alignment support*/
#define LV_USE_THEME_RTL         0

/*---------------
 * Screen size
 *--------------*/
#define LV_HOR_RES_MAX          (240) // For ESP32-2432S028R (which is 320x240 but usually held portrait)
#define LV_VER_RES_MAX          (320) // If landscape, swap these

/*--------------------
 * Font
 *--------------------*/

/*Montserrat fonts with ASCII range and some symbols using bpp = 4
 *https://fonts.google.com/specimen/Montserrat*/
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    1
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    0
#define LV_FONT_MONTSERRAT_20    0
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    0
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    0
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    0
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    0

/*Demonstrate special features*/
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /*bpp = 3*/
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /*Hebrew, Arabic, Persian letters and all their forms*/
#define LV_FONT_SIMSUN_16_CJK            0  /*1000 most common CJK radicals*/

/*Pixel perfect rendering can be enabled*/
#define LV_FONT_SUBPX_BGR                0  /*Enable sub-pixel rendering*/

/*-------------
 * Others
 *-------------*/
#define LV_SPRINTF_CUSTOM                0
#define LV_SPRINTF_INCLUDE               <stdio.h>

#define LV_USE_SHADOW                    1
#define LV_USE_OUTLINE                   1
#define LV_USE_PATTERN                   1
#define LV_USE_VALUE_STR                 1
#define LV_USE_BLEND_MODES               1
#define LV_USE_OPACITY                   1
#define LV_USE_FLEX                      1
#define LV_USE_GRID                      1
#define LV_USE_FS_STDIO                  0 /*Uses fopen, fread, etc*/
#define LV_USE_FS_POSIX                  0 /*Uses open, read, etc*/
#define LV_USE_FS_WIN32                  0 /*Uses CreateFile, ReadFile, etc*/
#define LV_USE_FS_FATFS                  0 /*Uses f_open, f_read, etc*/

/*------------------
 * LVGL examples
 *-----------------*/
#define LV_BUILD_EXAMPLES                0 // Disable examples by default
#define LV_USE_DEMO_WIDGETS              0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER   0
#define LV_USE_DEMO_BENCHMARK            0
#define LV_USE_DEMO_STRESS               0
#define LV_USE_DEMO_MUSIC                0

#endif /*LV_CONF_H*/

/* clang-format on */
