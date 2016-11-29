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
#ifndef FB_H__
#define FB_H__

#include "fonts.h"

#include <stdint.h>


#define FB_PTR_AND_PROTOTYPE(ret_type, func_name, ...) \
typedef ret_type (*func_name ## _ptr_t) (__VA_ARGS__); \
ret_type func_name(__VA_ARGS__)


#ifdef FB_CUSTOM

#define FB_WIDTH  FB_CUSTOM_WIDTH
#define FB_HEIGHT FB_CUSTOM_HEIGHT

#else

// Begin - Depracated
#if defined(MLCD_PCA64107_2INCH7) || defined(MLCD_PCA63504_2INCH7) || defined(MLCD_PCA63520_2INCH7)
#define DISP_SHARP_LS027B7DH01A
#endif

#ifdef MLCD_PCA63517_1INCH27
#define FB_WIDTH  (96)
#define FB_HEIGHT (96)
#endif
// End - Deprecated


#ifdef DISP_SHARP_LS012B7DH05 // 0.96-inch monochrome display
#define FB_WIDTH  (192)
#define FB_HEIGHT (192)
#endif

#ifdef DISP_SHARP_LS010B7DH04 // 0.96-inch monochrome display
#define FB_WIDTH  (128)
#define FB_HEIGHT (128)
#endif

#ifdef DISP_SHARP_LS012B7DD01 // 1.17-inch monochrome display
#define FB_WIDTH  (184)
#define FB_HEIGHT (38)

#endif
#ifdef DISP_SHARP_LS013B7DH05 // 1.26-inch monochrome display
#define FB_WIDTH  (144)
#define FB_HEIGHT (168)
#endif

#ifdef DISP_SHARP_LS013B7DH06 // 1.33-inch 8-color display
#define FB_WIDTH       (128)
#define FB_HEIGHT      (128)

#define FB_COLOR_DEPTH (3)
#endif

#ifdef DISP_SHARP_LS027B7DH01A // 2.7-inch monochrome display
#define FB_WIDTH  (400)
#define FB_HEIGHT (240)
#endif

#ifdef DISP_SHARP_LS032B7DD02 // 3.16-inch monochrome display
#define FB_WIDTH  (336)
#define FB_HEIGHT (536)
#endif

#ifdef DISP_SHARP_LS044Q7DH01 // 4.4-inch monochrome display
#define FB_WIDTH  (320)
#define FB_HEIGHT (240)
#endif


#if defined(DISP_JDISPLAY_LPM013M126A) || defined(DISP_JDISPLAY_LPM013M126C) // 1.282 inch 8-color display (the C-variant is with backlight)
#define FB_WIDTH       (176)
#define FB_HEIGHT      (176)

#define FB_COLOR_DEPTH (3)
#endif


#endif


#if !defined(FB_WIDTH) || !defined(FB_HEIGHT)
#error "A display or custom framebuffer shall be defined!"
#endif


#ifndef FB_COLOR_DEPTH
#define FB_COLOR_DEPTH  1
#endif


#if (FB_COLOR_DEPTH % 3) == 0
#ifndef FB_COLOR_RED_DEPTH 
#define FB_COLOR_RED_DEPTH   (FB_COLOR_DEPTH / 3)
#endif
#ifndef FB_COLOR_GREEN_DEPTH 
#define FB_COLOR_GREEN_DEPTH (FB_COLOR_DEPTH / 3)
#endif
#ifndef FB_COLOR_BLUE_DEPTH 
#define FB_COLOR_BLUE_DEPTH  (FB_COLOR_DEPTH / 3)
#endif
#endif


#define FB_INVALID_LINE (0xFFFF)


#if FB_COLOR_DEPTH == 1
typedef enum
{
    FB_COLOR_BLACK  = 0,
    FB_COLOR_WHITE  = 1,
} fb_color_t;
#endif


#if FB_COLOR_DEPTH == 3
typedef enum
{
    FB_COLOR_BLACK  = 0,
    FB_COLOR_RED    = 1,
    FB_COLOR_GREEN  = 2,
    FB_COLOR_YELLOW = 3,
    FB_COLOR_BLUE   = 4,
    FB_COLOR_MAGENTA= 5,
    FB_COLOR_CYAN   = 6,
    FB_COLOR_WHITE  = 7,
} fb_color_t;
#endif

uint16_t calc_string_width(char *str);

/**@brief Resets the framebuffer.
 *
 * @param color The default color of the pixels in the framebuffer.
 */
void fb_reset(fb_color_t color);


/**@brief Draws a circle in the framebuffer.
 *
 * @param cx     The x coordinate of the circle center.
 * @param cy     The y coordinate of the circle center.
 * @param color  The color of the circle.
 */
void fb_circle(uint16_t xc, uint16_t yc, uint16_t r, fb_color_t color);


/**@brief Draws a pixel in the framebuffer.
 *
 * @param cx     The x coordinate of the pixel.
 * @param cy     The y coordinate of the pixel.
 * @param color  The color of the pixel.
 */
void fb_pixel_set(uint16_t x, uint16_t y, fb_color_t color);


/**@brief Gets a pointer to the storage for the requested line.
 *
 * @param[in]  line_number      The requested line number.
 * @param[out] p_line_length    A pointer to storage for the length of the
 *                              requested line.
 * @param[out] p_line           A pointer to a pointer pointing to the
 *                              line in the frame buffer.
 *
 * @return The number of the line, or 0xFFFF if the requested line  
 *         is not in the buffer.
 */
FB_PTR_AND_PROTOTYPE(uint16_t, fb_line_storage_ptr_get, uint16_t line_number, uint8_t *p_line_length, uint8_t **p_line);


/**@brief Sets the storage for the requested line.
 *
 * @note The old content remains if there is not enought content to fill the
 *       entire line.
 *
 * @note The new content will be truncated if it is bigger than the storage in
 *       the framebuffer.
 *
 * @param[in]  line_number  The requested line number.
 * @param[out] line_length  The length of the new line content.
 * @param[out] p_line       A pointer to the new data for the
 *                          line in the frame buffer.
 *
 * @return The number of the line set, or 0xFFFF if the requested line  
 *         is not in the buffer.
 */
FB_PTR_AND_PROTOTYPE(uint16_t, fb_line_storage_set, uint16_t line_number, uint8_t line_length, uint8_t *p_line);


/**@brief Gets the next line that has been modified in the buffer.
 *
 * @param p_line_length A pointer to storage where the length of the next
 *                      modified line in the buffer will be stored.
 * @param p_line        A pointer to the next modified line in the buffer.
 *
 * @return The number of the modified line, or 0xFFFF if there is no modified 
 *         line in the buffer.
 */
FB_PTR_AND_PROTOTYPE(uint16_t, fb_next_dirty_line_get, uint8_t *p_line_length, uint8_t **p_line);


/**@brief Draws a line in the framebuffer.
 *
 * @param x1     The x coordinate of the first point of the line.
 * @param y1     The y coordinate of the first point of the line.
 * @param x2     The x coordinate of the second point of the line.
 * @param y2     The y coordinate of the second point of the line.
 * @param color  The color of the line.
 */
void fb_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color);


/**@brief Sets the font to be used by the printing functions.
 *
 * @param p_font_info   Pointer to the font information structure.
 */
void fb_font_set(font_info_t const *p_font_info);


/**@brief Puts a character in the framebuffer.
 *
 * @param x      The x coordinate of the upper left corner of the character.
 * @param y      The y coordinate of the upper left corner of the character.
 * @param ch     The character to put.
 * @param color  The color of the character.
 */
void fb_char_put(uint16_t x, uint16_t y, char ch, fb_color_t color);


/**@brief Puts a rectangle in the framebuffer.
 *
 * @param x1     The x coordinate of the first corner of the rectangle.
 * @param y1     The y coordinate of the first corner of the rectangle.
 * @param x2     The x coordinate of the corner diagonal to the first corner of the rectangle.
 * @param y2     The y coordinate of the corner diagonal to the first corner of the rectangle.
 * @param color  The color of the rectangle.
 */
void fb_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color);


/**@brief Puts a solid bar in the framebuffer.
 *
 * @param x1     The x coordinate of the first corner of the bar.
 * @param y1     The y coordinate of the first corner of the bar.
 * @param x2     The x coordinate of the corner diagonal to the first corner of the bar.
 * @param y2     The y coordinate of the corner diagonal to the first corner of the bar.
 * @param color  The color of the bar.
 */
void fb_bar(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color);


/**@brief Puts a text string in the framebuffer.
 *
 * @param x      The x coordinate of the upper left corner of the first character of the string.
 * @param y      The y coordinate of the upper left corner of the first character of the string.
 * @param str    The string to put.
 * @param color  The color of the string.
 */
void fb_string_put(uint16_t x, uint16_t y, char *str, fb_color_t color);


/**@brief Puts a bitmap in the framebuffer.
 *
 * @note The 8 bits version of the function to put a bitmap in the framebuffer use less
 *       pitmap storage (unless the bitmap use up all the bits to the next 32 bits boundary).
 *
 * @param x      The x coordinate of the upper left corner of the bitmap.
 * @param y      The y coordinate of the upper left corner of the bitmap.
 * @param ch     The bitmap to put.
 * @param width  The width of the bitmap.
 * @param height The height of the bitmap.
 * @param color  The color of the pixels defined by the bitmap.
 */
void fb_bitmap_put(uint16_t x, uint16_t y, uint32_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color);


/**@brief Puts a bitmap in the framebuffer.
 *
 * @note The 32 bits version of the function to put a bitmap in the framebuffer is faster.
 *
 * @param x      The x coordinate of the upper left corner of the bitmap.
 * @param y      The y coordinate of the upper left corner of the bitmap.
 * @param ch     The bitmap to put.
 * @param width  The width of the bitmap.
 * @param height The height of the bitmap.
 * @param color  The color of the pixels defined by the bitmap.
 */void fb_bitmap8_put(uint16_t x, uint16_t y, uint8_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color);

#endif // FB_H__
