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
#include "fb.h"
#include <string.h>


#define BITS_COUNT_TO_UINT32_COUNT(bits_count) ((31 + bits_count) / 32)

#define LAST_STORAGE_WORD_MASK (((uint64_t)1 << (((FB_WIDTH - 1) % 32) + 1)) - 1)


typedef enum
{
    FB_LINE_STATUS_CLEAN = 0,
    FB_LINE_STATUS_DIRTY = 1,
} fb_line_status_t;


#ifndef FONTS_DEFAULT_FONT
FONTS_CONST_ARIAL_20PT_DECLARE(FONTS_DEFAULT_FONT_DECLARATION);
#endif


static struct
{
    uint32_t dirty_flags[BITS_COUNT_TO_UINT32_COUNT(FB_HEIGHT)];
    uint32_t lines[FB_HEIGHT][BITS_COUNT_TO_UINT32_COUNT(FB_WIDTH)];
    
    font_info_t const * p_default_font;
} m_fb = {.p_default_font = &FONTS_DEFAULT_FONT_DECLARATION};


int16_t abs16(int16_t x)
{
    int16_t y = (x >> 31);
    return (x ^ y) - y;
}


static void inline dirty_flag_set(uint16_t row)
{
    m_fb.dirty_flags[row >> 5] |= ((uint32_t)FB_LINE_STATUS_DIRTY << (row & 0x1F));
}


static void inline dirty_flag_clear(uint16_t row)
{
    m_fb.dirty_flags[row >> 5] &= ~((uint32_t)FB_LINE_STATUS_DIRTY << (row & 0x1F));
}


void fb_pixel_set(uint16_t x, uint16_t y, fb_color_t color)
{
    uint8_t x_bit = x & 0x1F;
    uint8_t x_idx = x >> 5;
    
    m_fb.lines[y][x_idx] = (m_fb.lines[y][x_idx] & ~((uint32_t)1 << x_bit)) | ((uint32_t)color << x_bit);
    
    dirty_flag_set(y);
}


static void inline vline_set(uint16_t x, uint16_t y, uint16_t count, fb_color_t color)
{
    uint16_t i;
    uint8_t x_idx = x >> 5;
    uint8_t x_bit = x & 0x1F;
    uint32_t x_bit_mask     =  ((uint32_t)color << x_bit);
    uint32_t x_bit_clr_mask = ~((uint32_t)    1 << x_bit);
    
    for ( i = 0; i < count; i++ )
    {
        m_fb.lines[y + i][x_idx] = (m_fb.lines[y + i][x_idx] & x_bit_clr_mask) | x_bit_mask;
    
        dirty_flag_set(y + i);
    }
}


static void inline hline_set(uint16_t x, uint16_t y, uint16_t count, fb_color_t color)
{
    uint8_t i;
    uint8_t x1_idx = x >> 5;
    uint8_t x2_idx = (x + count) >> 5;
    uint32_t lmsk  = ((uint32_t)1 << (x & 0x1F)) - 1;
    uint32_t hmsk  = ~(((uint32_t)1 << ((x + count) & 0x1F)) - 1);
    
    if ( color == FB_COLOR_BLACK )
    {            
        if ( x1_idx == x2_idx )
        {
            m_fb.lines[y][x1_idx] = m_fb.lines[y][x1_idx] & (lmsk | hmsk);
        }
        else
        {
            m_fb.lines[y][x1_idx] = m_fb.lines[y][x1_idx] & lmsk;
            m_fb.lines[y][x2_idx] = m_fb.lines[y][x2_idx] & hmsk;
        }
        
        for ( i = x1_idx + 1; i < x2_idx; i++ )
        {
            m_fb.lines[y][i] = 0;
        }
    }
    else
    {
        // ASSERT( color == FB_COLOR_WHITE );
        if ( x1_idx == x2_idx )
        {
            m_fb.lines[y][x1_idx] = m_fb.lines[y][x1_idx] | ~(lmsk | hmsk);
        }
        else
        {
            m_fb.lines[y][x1_idx] = m_fb.lines[y][x1_idx] | ~lmsk;
            m_fb.lines[y][x2_idx] = m_fb.lines[y][x2_idx] | ~hmsk;
        }
        
        for ( i = x1_idx + 1; i < x2_idx; i++ )
        {
            m_fb.lines[y][i] = 0xFFFFFFFF;
        }
    } 

    dirty_flag_set(y);
}


uint16_t fb_line_storage_ptr_get(uint16_t line_number, uint8_t *p_line_length, uint8_t **p_line)
{
    if ( line_number < FB_HEIGHT )
    {
        *p_line_length = (7 + FB_WIDTH) >> 3;
        *p_line = (uint8_t *)&(m_fb.lines[line_number]);
        return ( line_number );
    }

    return ( 0xFFFF );
}


uint16_t fb_line_storage_set(uint16_t line_number, uint8_t line_length, uint8_t *p_line)
{
    if ( line_number < FB_HEIGHT )
    {
        if ( ((uint8_t *)&(m_fb.lines[line_number])) != &(p_line[0]) )
        {
            memcpy((uint8_t *)&(m_fb.lines[line_number]), &(p_line[0]),
                (line_length < sizeof(m_fb.lines[line_number]) ? line_length : sizeof(m_fb.lines[line_number])));
            m_fb.lines[line_number][BITS_COUNT_TO_UINT32_COUNT(FB_WIDTH) - 1] &= LAST_STORAGE_WORD_MASK;
        }
        dirty_flag_set(line_number);
        
        return ( line_number );
    }

    return ( 0xFFFF );
}


uint16_t fb_next_dirty_line_get(uint8_t *p_line_length, uint8_t **p_line)
{
    uint16_t i;
    
    for ( i = 0; i < BITS_COUNT_TO_UINT32_COUNT(FB_HEIGHT); i++ )
    {
        uint32_t tmp_u32 = m_fb.dirty_flags[i];
        uint8_t  y       = i << 5;

        while ( (y < FB_HEIGHT) && (tmp_u32 != 0) )
        {
            if ( tmp_u32 & 0x00000001 )
            {
                dirty_flag_clear(y);
                
                *p_line_length = (7 + FB_WIDTH) >> 3;
                *p_line = (uint8_t *)&(m_fb.lines[y]);
                return ( y );
            }
            
            ++y;
            tmp_u32 >>= 1;
        }
    }
    
    return ( 0xFFFF );
}


void fb_reset(fb_color_t color)
{
    uint16_t i;
    
    for ( i = 0; i < FB_HEIGHT; i++ )
    {
        if ( color == FB_COLOR_BLACK )
        {            
            memset((uint8_t *)&(m_fb.lines[i]), 0, sizeof(m_fb.lines[i]));
        }
        else
        {
            // ASSERT( color == FB_COLOR_WHITE );
            memset((uint8_t *)&(m_fb.lines[i]), 0xFF, sizeof(m_fb.lines[i]));
        }
    }
    
    memset((uint8_t *)&(m_fb.dirty_flags[0]), ~((uint8_t)FB_LINE_STATUS_CLEAN), sizeof(m_fb.dirty_flags));
}


void static inline bit_pattern_set(uint16_t x, uint16_t y, fb_color_t color, uint16_t width, uint32_t pattern)
{
    uint8_t first_idx = x >> 5;
    uint8_t last_idx = (x + width) >> 5;
    
    if ( color == FB_COLOR_BLACK )
    {
        m_fb.lines[y][first_idx] = (m_fb.lines[y][first_idx] & ~(pattern << (x & 0x1F)));
    }
    else
    {
        // ASSERT( color == FB_COLOR_WHITE );
        m_fb.lines[y][first_idx] = (m_fb.lines[y][first_idx] | (pattern << (x & 0x1F)));
    }

    if ( first_idx != last_idx )
    {
        if ( color == FB_COLOR_BLACK )
        {
            m_fb.lines[y][last_idx] = (m_fb.lines[y][last_idx] & ~(pattern >> (width - ((x + width) & 0x1F))));
        }
        else
        {
            // ASSERT( color == FB_COLOR_WHITE );
            m_fb.lines[y][last_idx] = (m_fb.lines[y][last_idx] | (pattern >> (width - ((x + width) & 0x1F))));
        }
    }
    
    dirty_flag_set(y);
}


static uint8_t m_put_char(uint16_t x, uint16_t y, char ch, fb_color_t color)
{    
    if ( (m_fb.p_default_font->start_char <= ch) && (ch <= m_fb.p_default_font->end_char) )
    { 
        uint8_t width;
        uint8_t height;
        uint16_t offset;
        uint8_t bytes_per_line;

        {
            uint8_t j = 0;
            uint8_t char_idx                         = ch - m_fb.p_default_font->start_char;
            uint8_t const * const p_descr_entry_base = &(m_fb.p_default_font->p_descriptor[m_fb.p_default_font->descr_entry_size * char_idx]);
            
            width  = ( m_fb.p_default_font->width  != -1 ) ? m_fb.p_default_font->width  : p_descr_entry_base[j++];
            height = ( m_fb.p_default_font->height != -1 ) ? m_fb.p_default_font->height : p_descr_entry_base[j++];
            offset = ( m_fb.p_default_font->offset != -1 ) ? m_fb.p_default_font->offset * char_idx : *((__packed uint16_t *)&(p_descr_entry_base[j]));
        }
        bytes_per_line = (width >> 3) + ((width & 0x07) ? 1 : 0);
        
        for (uint8_t i = 0; i < height; i++)
        {
            for (uint8_t j = 0; j < bytes_per_line; j++)
            {
                bit_pattern_set(x + j * 8, y + i, color, 8, m_fb.p_default_font->p_bitmap[offset + (i * bytes_per_line) + j]);
            }
        }
        
        return ( width );
    }
    
    return ( 0 );
}


void fb_font_set(font_info_t const *p_font_info)
{
    m_fb.p_default_font = ( p_font_info !=  NULL) ? p_font_info : &FONTS_DEFAULT_FONT_DECLARATION;
}


void fb_char_put(uint16_t x, uint16_t y, char ch, fb_color_t color)
{
    (void)m_put_char(x, y, ch, color);
}


void fb_string_put(uint16_t x, uint16_t y, char *str, fb_color_t color)
{
    uint8_t length = strlen(str);
    
    uint16_t current_x = x;
    
    for ( uint8_t n = 0; n < length; n++ )
    {
        uint8_t width = m_put_char(current_x, y, str[n], color);
        
        current_x += width + ((m_fb.p_default_font->width == -1) ? 1 : 0); // Add one pixel for fonts with dynamic width.
    }
}


void fb_bitmap_put(uint16_t x, uint16_t y, uint32_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color)
{
    const uint8_t entries_per_row = (((width - 1) >> 5) + 1);
    uint16_t i, n;

    for ( i = 0; i < height; i++ )
    {   
        for ( n = 0; n < entries_per_row; n++ )
        {
            if ( (32 * (n + 1)) < width )
            {
                bit_pattern_set(x + 32 * n, y + i, color, 32, bitmap[i * entries_per_row + n]);
            }
            else
            {
                bit_pattern_set(x + 32 * n, y + i, color, 32, bitmap[i * entries_per_row + n] & (0xFFFFFFFF >> ((32 * (n + 1)) - width)));
            }
        }
    }
}


void fb_bitmap8_put(uint16_t x, uint16_t y, uint8_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color)
{
    const uint8_t   entries_per_row = (((width - 1) >> 3) + 1);
    uint8_t         bit_count       = 0;
    uint32_t        bits            = 0;
    uint16_t         i, n;

    for ( i = 0; i < height; i++ )
    {   
        for ( n = 0; n < entries_per_row; n++ )
        {
            bits = ((uint32_t)bitmap[i * entries_per_row + n] << bit_count) | bits;
            if ( (8 * (n + 1)) <= width )
            {
                bit_count += 8;
            }
            else
            {
                bit_count += ((8 * (n + 1)) - width);
            }
            if ( bit_count > 24 )
            {
                bit_pattern_set(x + 8 * (n - 3), y + i, color, bit_count, bits & (0xFFFFFFFF >> (32 - bit_count)));
                bits = 0;
                bit_count = 0;
            }
        }
        if ( bit_count != 0 )
        {
            bit_pattern_set(x + 8 * (n - (bit_count >> 3)), y + i, color, bit_count, bits  & (0xFFFFFFFF >> (32 - bit_count)));
            bits = 0;
            bit_count = 0;
        }
    }
}


void fb_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color)
{
    if ( x1 < x2 )
    {
        hline_set(x1, y1, x2 - x1 + 1, color);
        hline_set(x1, y2, x2 - x1 + 1, color);
    }
    else
    {
        hline_set(x2, y1, x1 - x2 + 1, color);
        hline_set(x2, y2, x1 - x2 + 1, color);
    }
    if ( y1 < y2 )
    {
        vline_set(x1, y1 + 1, y2 - y1 - 1, color);
        vline_set(x2, y1 + 1, y2 - y1 - 1, color);
    }
    else
    {
        vline_set(x1, y2 + 1, y1 - y2 - 1, color);
        vline_set(x2, y2 + 1, y1 - y2 - 1, color);
    }
}


void fb_bar(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color)
{
    uint16_t i;
    
    if ( x1 < x2 )
    {
        if ( y1 < y2 )
        {
            for ( i = y1; i <= y2; i++ )
            {
                hline_set(x1, i, x2 - x1 + 1, color);
            }
        }
        else
        {
            for ( i = y2; i <= y1; i++ )
            {
                hline_set(x1, i, x2 - x1 + 1, color);
            }
            
        }
    }
    else
    {
        if ( y1 < y2 )
        {
            for ( i = y1; i <= y2; i++ )
            {
                hline_set(x2, i, x1 - x2 + 1, color);
            }
        }
        else
        {
            for ( i = y2; i <= y1; i++ )
            {
                hline_set(x2, i, x1 - x2 + 1, color);
            }
            
        }
    }
}


void fb_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, fb_color_t color)
{
    int16_t t, distance;
    int16_t xerr=0, yerr=0, delta_x, delta_y;
    int16_t incx, incy;

    /* compute the distances in both directions */
    delta_x=x2-x1;
    delta_y=y2-y1;

    if ( delta_x == 0 ) // Vertical line.
    {
        if ( delta_y < 0 ) // y1 is below (has a higher value) y2.
        {
            vline_set(x1, y2, y1 - y2 + 1, color);
        }
        else
        {
            vline_set(x1, y1, y2 - y1 + 1, color);
        }
    }
    else if ( delta_y == 0 ) // Horizontal line.
    {
        if ( delta_x < 0 ) // x1 is to the right (has a higher value) of y2.
        {
            hline_set(x2, y1, x1 - x2 + 1, color);
        }
        else
        {
            hline_set(x1, y1, x2 - x1 + 1, color);
        }    
    }
    else
    {
        /* Compute the direction of the increment,
           an increment of 0 means either a horizontal or vertical
           line.
        */
        if(delta_x>0) incx=1;
        /*else if(delta_x==0) incx=0;*/
        else incx=-1;

        if(delta_y>0) incy=1;
        /*else if(delta_y==0) incy=0;*/
        else incy=-1;
        

        /* determine which distance is greater */
        delta_x=abs16(delta_x);
        delta_y=abs16(delta_y);
        if(delta_x>delta_y) distance=delta_x;
        else distance=delta_y;

        /* draw the line */
        for(t=0; t<=distance+1; t++) {
            fb_pixel_set(x1, y1, color);
            
            xerr+=delta_x;
            yerr+=delta_y;
            if(xerr>distance) {
                xerr-=distance;
                x1+=incx;
            }
            if(yerr>distance) {
                yerr-=distance;
                y1+=incy;
            }
        }
    }
}


void fb_circle(uint16_t xc, uint16_t yc, uint16_t r, fb_color_t color)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t p = 3 - 2 * r;
    if (!r) return;
    while (y >= x) // only formulate 1/8 of circle
    {
        fb_pixel_set(xc-x, yc-y, color);//upper left left
        fb_pixel_set(xc-y, yc-x, color);//upper upper left
        fb_pixel_set(xc+y, yc-x, color);//upper upper right
        fb_pixel_set(xc+x, yc-y, color);//upper right right
        fb_pixel_set(xc-x, yc+y, color);//lower left left
        fb_pixel_set(xc-y, yc+x, color);//lower lower left
        fb_pixel_set(xc+y, yc+x, color);//lower lower right
        fb_pixel_set(xc+x, yc+y, color);//lower right right
        if (p < 0) p += 4*x++ + 6;
              else p += 4*(x++ - y--) + 10;
     }
}    
