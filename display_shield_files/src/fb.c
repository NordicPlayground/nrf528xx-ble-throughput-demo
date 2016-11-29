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

#define M_COLOR_FIELD_MASK              ((1UL << FB_COLOR_DEPTH) - 1)
#define M_ROW_SIZE_BYTES                (((8 - FB_COLOR_DEPTH) + (FB_WIDTH * FB_COLOR_DEPTH)) / 8)
#define M_ROW_START_OFFS(row)           (M_ROW_SIZE_BYTES * 8 * row)
#define M_ROW_UINT8_PTR(line_number)    (&(((uint8_t *)&(m_fb.pixels[0]))[M_ROW_SIZE_BYTES * line_number]))

#define BITS_COUNT_TO_UINT32_COUNT(bits_count) ((31 + bits_count) / 32)

#define LAST_STORAGE_WORD_MASK (((uint64_t)1 << (((FB_WIDTH - 1) % 32) + 1)) - 1)


typedef enum
{
    FB_LINE_STATUS_CLEAN = 0,
    FB_LINE_STATUS_DIRTY = 1,
} fb_line_status_t;


#ifndef FONTS_DEFAULT_FONT
FONTS_CONST_GENERIC_8PT_DECLARE(FONTS_DEFAULT_FONT_DECLARATION);
#endif


static struct
{
    uint32_t dirty_flags[BITS_COUNT_TO_UINT32_COUNT(FB_HEIGHT)];
    uint32_t pixels[(3 + M_ROW_SIZE_BYTES * FB_HEIGHT) / 4];
    
    font_info_t const * p_default_font;
} m_fb = {.p_default_font = &FONTS_DEFAULT_FONT_DECLARATION};


static int16_t abs16(int16_t x)
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


static void inline vline_set(uint16_t x, uint16_t y, uint16_t count, fb_color_t color)
{
    uint16_t i;
    uint16_t y2 = y + count;
    
    for ( i = y; i < y2; i++ )
    {
        fb_pixel_set(x, i, color);
    }
}


static void inline bit_pattern_set(uint16_t x, uint16_t y, fb_color_t color, uint16_t width, uint32_t pattern)
{
    uint32_t    pixel_pos       = M_ROW_START_OFFS(y) +  x          * FB_COLOR_DEPTH;
    uint32_t    last_pixel_pos  = M_ROW_START_OFFS(y) + (x + width) * FB_COLOR_DEPTH;
    uint16_t    pixels_idx      = pixel_pos >> 5;
    uint8_t     pixel_offs      = pixel_pos & 0x1F;
    uint32_t    u32             = m_fb.pixels[pixels_idx] & ((1UL << pixel_offs) - 1);
    
    uint8_t     pattern_offs    = 0;
    
    while ( pixel_pos < last_pixel_pos )
    {
        if ( (pattern & (1UL << pattern_offs)) != 0 )
        {
            u32 |= (uint32_t)color << pixel_offs;
        }
        else
        {
            u32 |= m_fb.pixels[pixels_idx] & (M_COLOR_FIELD_MASK << pixel_offs);
        }
        
        if ( (pixel_offs + FB_COLOR_DEPTH) < (sizeof(uint32_t) * 8) )
        {
            pixel_offs += FB_COLOR_DEPTH;
        }
        else
        {            
            m_fb.pixels[pixels_idx++] = u32;
            
            pixel_offs -= (sizeof(uint32_t) * 8) - FB_COLOR_DEPTH;
            if ( (pattern & (1UL << pattern_offs)) != 0 )
            {
                u32 = (uint32_t)color >> (FB_COLOR_DEPTH - pixel_offs);
            }
            else
            {
                u32 = m_fb.pixels[pixels_idx] & (M_COLOR_FIELD_MASK >> (FB_COLOR_DEPTH - pixel_offs));
            }
        }
        
        pixel_pos += FB_COLOR_DEPTH;        
        pattern_offs = (pattern_offs + 1) & 0x1F;
    }
    
    if ( pixel_offs > 0 )
    {
        m_fb.pixels[pixels_idx] &= ~((1UL << pixel_offs) - 1);
        m_fb.pixels[pixels_idx] |= u32;
    }
    
    dirty_flag_set(y);
}


static void inline hline_set(uint16_t x, uint16_t y, uint16_t count, fb_color_t color)
{
    bit_pattern_set(x, y, color, count, 0xFFFFFFFF);
}

uint16_t fb_line_storage_ptr_get(uint16_t line_number, uint8_t *p_line_length, uint8_t **p_line)
{
    if ( line_number < FB_HEIGHT )
    {
        *p_line_length = M_ROW_SIZE_BYTES;
        *p_line = M_ROW_UINT8_PTR(line_number);
        return ( line_number );
    }

    return ( FB_INVALID_LINE );
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
            
            width  = ( m_fb.p_default_font->width  != -1 ) ? m_fb.p_default_font->width  : p_descr_entry_base[j];
			j += 2;
            height = ( m_fb.p_default_font->height != -1 ) ? m_fb.p_default_font->height : p_descr_entry_base[j++];
            offset = ( m_fb.p_default_font->offset != -1 ) ? m_fb.p_default_font->offset * char_idx : *((__packed uint16_t *)&(p_descr_entry_base[j]));
        }
        bytes_per_line = (width >> 3) + ((width & 0x07) ? 1 : 0);
        
        for (uint8_t i = 0; i < height; i++)
        {
            uint8_t j;

            for ( j = 0; j < (width >> 5); j++ )
            {
                bit_pattern_set(x + j * 32, y + i, color, 32, *((uint32_t *)&(m_fb.p_default_font->p_bitmap[offset + (i * bytes_per_line) + j])));
            }

            if ( (width - (j * 32)) > 0  )
            {
                bit_pattern_set(x + j * 32, y + i, color, width - (j * 32), *((uint32_t *)&(m_fb.p_default_font->p_bitmap[offset + (i * bytes_per_line) + j])));
            }
        }
        
        return ( width );
    }
    
    return ( 0 );
}


void fb_reset(fb_color_t color)
{
    uint16_t i;
    
    hline_set(0, 0, FB_WIDTH, color);
    
    for ( i = 1; i < FB_HEIGHT; i++ )
    {
        memcpy(M_ROW_UINT8_PTR(i), M_ROW_UINT8_PTR(0), M_ROW_SIZE_BYTES);
    }
    
    memset((uint8_t *)&(m_fb.dirty_flags[0]), ~((uint8_t)FB_LINE_STATUS_CLEAN), sizeof(m_fb.dirty_flags));
}


void fb_pixel_set(uint16_t x, uint16_t y, fb_color_t color)
{
    uint32_t pixel_pos  = M_ROW_START_OFFS(y) + x * FB_COLOR_DEPTH;
    uint16_t pixels_idx = pixel_pos >> 5;
    uint8_t pixel_offs  = pixel_pos & 0x1F;
    
    if ( (pixel_offs + FB_COLOR_DEPTH) < (sizeof(uint32_t) * 8) )
    {
        m_fb.pixels[pixels_idx] = (m_fb.pixels[pixels_idx] & ~(M_COLOR_FIELD_MASK << pixel_offs)) | ((uint32_t)color << pixel_offs);
    }
    else
    {
        uint8_t     pixel_offs  = pixel_pos & 0x07;
        uint8_t   * p_pixels    = &(((uint8_t *)&(m_fb.pixels[0]))[pixel_pos >> 3]);
        
        *((__packed uint32_t *)p_pixels) = (*((__packed uint32_t *)p_pixels) & ~(M_COLOR_FIELD_MASK << pixel_offs)) | ((uint32_t)color << pixel_offs);
    }
    
    dirty_flag_set(y);
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

uint16_t calc_string_width(char *str)
{
	uint16_t line_width = 0;
	uint8_t char_counter = 0;
	while(str[char_counter] != '\0')
	{
		uint8_t char_idx = str[char_counter] - m_fb.p_default_font->start_char;
		uint8_t const * const p_descr_entry_base = &(m_fb.p_default_font->p_descriptor[m_fb.p_default_font->descr_entry_size * char_idx]);
		line_width += *((__packed uint16_t *)&(p_descr_entry_base[0]));
		char_counter++;
	}
	return line_width;
}


void fb_bitmap_put(uint16_t x, uint16_t y, uint32_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color)
{
    const uint8_t entries_per_row = (((width - 1) >> 5) + 1);
    uint16_t i, n;

    for ( i = 0; i < height; i++ )
    {   
        for ( n = 0; n < entries_per_row; n++ )
        {
            bit_pattern_set(x + 32 * n, y + i, color, 32, bitmap[i * entries_per_row + n]);
        }
    }
}


void fb_bitmap8_put(uint16_t x, uint16_t y, uint8_t const * const bitmap, uint16_t width, uint16_t height, fb_color_t color)
{
    const uint8_t   entries_per_row = (((width - 1) >> 3) + 1);
    uint16_t    n, i;
    
    for ( n = 0; n < height; n++ )
    {
        for ( i = 0; i < (width >> 5); i++ )
        {
            bit_pattern_set(x + i * 32, y + n, color, 32, ((uint32_t *)&(bitmap[n * entries_per_row]))[i]);
        }

        if ( (width - (i * 32)) > 0  )
        {
            bit_pattern_set(x + i * 32, y + n, color, width - (i * 32), ((uint32_t *)&(bitmap[n * entries_per_row]))[i]);
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


uint16_t fb_line_storage_set(uint16_t line_number, uint8_t line_length, uint8_t *p_line)
{
    if ( line_number < FB_HEIGHT )
    {
        if ( M_ROW_UINT8_PTR(line_number) != &(p_line[0]) )
        {
            memcpy(M_ROW_UINT8_PTR(line_number), &(p_line[0]),
                (line_length < M_ROW_SIZE_BYTES ? line_length : M_ROW_SIZE_BYTES));
        }
        dirty_flag_set(line_number);
        
        return ( line_number );
    }

    return ( FB_INVALID_LINE );
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
                
                *p_line_length = M_ROW_SIZE_BYTES;
                *p_line = M_ROW_UINT8_PTR(y);
                return ( y );
            }
            
            ++y;
            tmp_u32 >>= 1;
        }
    }
    
    return ( FB_INVALID_LINE );
}
