/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#ifndef FB_UTIL_H__
#define FB_UTIL_H__

#include "fb.h"

#include <stdint.h>


#if defined(MLCD_PCA64107_2INCH7) || defined(MLCD_PCA63504_2INCH7) || defined(MLCD_PCA63520_2INCH7)

#define FB_UTIL_LCD_WIDTH       (400)
#define FB_UTIL_LCD_HEIGHT      (240)
#define FB_UTIL_LCD_LINE_LENGTH (FB_UTIL_LCD_WIDTH / 8)

#endif


#ifdef MLCD_PCA63517_1INCH27

#define FB_UTIL_LCD_WIDTH       (96)
#define FB_UTIL_LCD_HEIGHT      (92)
#define FB_UTIL_LCD_LINE_LENGTH (FB_UTIL_LCD_WIDTH / 8)

#endif


/**@brief Resets the virtual lcd handler feature.
 *
 * @note The x offset should alweys be kept a multiple of 8, or else updating the display
 *       will be significantly slower.
 *
 * @param   x_offs          The offset in pixels from the first column of the lcd screen
 *                          to the first column of the virtual screen.
 * @param   y_offs          The offset in pixels from the first row of the lcd screen
 *                          to the first row of the virtual screen.
 * @param   padding_color   The default color of the pixels surrounding the framebuffer content.
 */
void fb_util_virtual_lcd_reset(uint16_t x_offs, uint16_t y_offs, fb_color_t padding_color);


/**@brief Handles the virtual lcd and framebuffer updates.
 *
 * @param p_line_length A pointer to storage where the length of the next
 *                      modified line in the buffer will be stored.
 * @param p_line        A pointer to the next modified line in the buffer.
 *
 * @return The number of the modified line, or 0xFFFF if there is no modified 
 *         line in the buffer.
 */
uint16_t fb_util_virtual_lcd_next_dirty_line_get(uint8_t *p_line_length, uint8_t **p_line);


#endif // FB_UTIL_H__
