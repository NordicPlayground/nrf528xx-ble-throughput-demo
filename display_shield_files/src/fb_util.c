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
#include "fb_util.h"

#include <string.h>


typedef void * (*fb_util_memcpy_t) (void *dest, const void *src, size_t n);


static struct
{
    struct
    {
        uint16_t            x_offs;
        uint16_t            y_offs;
        uint8_t             x_bit_offs;
        fb_color_t          padding_color;
        uint8_t             line_init_idx;
        uint8_t             tmp_buf_idx;
        uint8_t             tmp_buf[3][FB_UTIL_LCD_LINE_LENGTH];
        fb_util_memcpy_t    p_memcpy;
    } virtual_lcd;
} m_fb_util;


static void *m_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t i;
    uint16_t tmp = (1 << (m_fb_util.virtual_lcd.x_bit_offs + 8)) - 1;

    for ( i = 0; i < n; i++ )
    {
        tmp = (((uint16_t)(((uint8_t *)src)[i])) << m_fb_util.virtual_lcd.x_bit_offs) | (tmp >> 8);
        ((uint8_t *)dest)[i] = tmp & 0xFF;
    }
    ((uint8_t *)dest)[i] = (tmp >> 8) | (((1 << (8 - m_fb_util.virtual_lcd.x_bit_offs)) - 1) << m_fb_util.virtual_lcd.x_bit_offs);
    
    return ( dest );
}


void fb_util_virtual_lcd_reset(uint16_t x_offs, uint16_t y_offs, fb_color_t padding_color)
{
    m_fb_util.virtual_lcd.x_bit_offs = x_offs - (x_offs & ~0x03);

    if ( m_fb_util.virtual_lcd.x_bit_offs == 0 )
    {
        m_fb_util.virtual_lcd.p_memcpy = memcpy;
    }
    else
    {
        m_fb_util.virtual_lcd.p_memcpy = m_memcpy;
    }
    
    m_fb_util.virtual_lcd.x_offs = x_offs;
    m_fb_util.virtual_lcd.y_offs = y_offs;
    
    m_fb_util.virtual_lcd.padding_color = padding_color;
    
    m_fb_util.virtual_lcd.line_init_idx = 0;
    
    memset((uint8_t *)&(m_fb_util.virtual_lcd.tmp_buf[0][0]), (padding_color == FB_COLOR_WHITE) ? 0xFF : 0x00, sizeof(m_fb_util.virtual_lcd.tmp_buf));
    m_fb_util.virtual_lcd.tmp_buf_idx = 0;
}


uint16_t fb_util_virtual_lcd_next_dirty_line_get(uint8_t *p_line_length, uint8_t **p_line)
{    
    uint16_t line_number;
    
    if ( m_fb_util.virtual_lcd.line_init_idx < FB_UTIL_LCD_HEIGHT )
    {
        line_number = m_fb_util.virtual_lcd.line_init_idx;
        *p_line = &(m_fb_util.virtual_lcd.tmp_buf[m_fb_util.virtual_lcd.tmp_buf_idx][0]);
        *p_line_length = FB_UTIL_LCD_LINE_LENGTH;

        ++m_fb_util.virtual_lcd.tmp_buf_idx;
        if ( m_fb_util.virtual_lcd.tmp_buf_idx > 2 )
        {
            m_fb_util.virtual_lcd.tmp_buf_idx = 0;
        }
            
        ++m_fb_util.virtual_lcd.line_init_idx;
        if ( m_fb_util.virtual_lcd.line_init_idx == m_fb_util.virtual_lcd.y_offs )
        {
            m_fb_util.virtual_lcd.line_init_idx += FB_HEIGHT;
        }
    }
    else
    {
        line_number = fb_next_dirty_line_get(p_line_length, p_line);
        
        if ( line_number != 0xFFFF )
        {
            m_fb_util.virtual_lcd.p_memcpy(&(m_fb_util.virtual_lcd.tmp_buf[m_fb_util.virtual_lcd.tmp_buf_idx][m_fb_util.virtual_lcd.x_offs >> 3]), *p_line, *p_line_length);
            
            *p_line = &(m_fb_util.virtual_lcd.tmp_buf[m_fb_util.virtual_lcd.tmp_buf_idx][0]);
            *p_line_length = FB_UTIL_LCD_LINE_LENGTH;
            
            ++m_fb_util.virtual_lcd.tmp_buf_idx;
            if ( m_fb_util.virtual_lcd.tmp_buf_idx > 2 )
            {
                m_fb_util.virtual_lcd.tmp_buf_idx = 0;
            }
            
            line_number += m_fb_util.virtual_lcd.y_offs;
        }
    }
    
    return ( line_number );
}
