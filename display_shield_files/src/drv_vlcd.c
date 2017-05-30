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
#include "drv_vlcd.h"
#include "drv_23lcv.h"
#include "drv_disp_engine.h"
#include "fb.h"
#include "nrf.h"

#include <stdlib.h>
#include <string.h>

static drv_vlcd_cfg_t *p_cfg;


static struct
{
    struct
    {
        uint16_t    x0;
        uint16_t    y0;
    } fb_pos;
    uint16_t                current_line_number;
    drv_vlcd_sig_callback_t current_sig_callback;
    drv_vlcd_output_mode_t  current_output_mode;
} m_drv_vlcd;


static const uint8_t M_VLCD_WR       = 0x01; //M_VLCD write line command

#define M_VLCD_PADDING   0x00 // Padding

#define M_VLCD_COMMAND_FIELD_LENGTH         1
#define M_VLCD_LINE_NUMBER_FIELD_LENGTH     1
#define M_VLCD_SHORT_PADDING_FIELD_LENGTH   1


static void drv_23lcv_sig_callback(drv_23lcv_signal_type_t drv_23lcv_signal_type);



static uint32_t line_payload_addr_get(uint16_t line_number)
{
    return ( M_VLCD_COMMAND_FIELD_LENGTH + (line_number * (M_VLCD_LINE_NUMBER_FIELD_LENGTH + (VLCD_WIDTH / 8) + M_VLCD_SHORT_PADDING_FIELD_LENGTH)) + M_VLCD_LINE_NUMBER_FIELD_LENGTH );
}


static uint32_t access_begin(drv_disp_engine_access_descr_t *p_access_descr, drv_disp_engine_proc_type_t new_proc)
{   
    static drv_23lcv_cfg_t cfg;
    
    (void)p_access_descr;
    (void)new_proc;

    cfg.spi.ss_pin     = p_cfg->spi.ss_pin;
    cfg.spi.p_config   = p_cfg->spi.p_config;
    cfg.spi.p_instance = p_cfg->spi.p_instance;

    if ( drv_23lcv_open(&cfg) != DRV_VLCD_STATUS_CODE_SUCCESS )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


static bool access_update(drv_disp_engine_access_descr_t *p_access_descr)
{    
    if ( p_access_descr->current_proc == DRV_DISP_ENGINE_PROC_CLEAR )
    {
        if ( m_drv_vlcd.current_line_number == VLCD_HEIGHT )
        {
            return ( false );
        }
        else if ( m_drv_vlcd.current_line_number == VLCD_HEIGHT - 1 )
        {
            p_access_descr->buffers.postamble_length = 2;
        }
        
        if ( m_drv_vlcd.current_line_number > 0 )
        {
            p_access_descr->buffers.p_preamble[0] = m_drv_vlcd.current_line_number + 1;
            p_access_descr->buffers.preamble_length = 1;
        }
    }
    else if ( p_access_descr->buffers.data_length == 0 )
    {
        return ( false );
    }
    
    
    return ( true );
}


static uint32_t line_write(drv_disp_engine_proc_access_type_t access_type, uint16_t line_number, uint8_t *p_buf, uint8_t buf_length)
{
    uint16_t dest_addr;
    int16_t size;
    
    switch ( access_type )
    {
        case DRV_DISP_ENGINE_PROC_ACCESS_TYPE_PREAMBLE:
            dest_addr = line_payload_addr_get(m_drv_vlcd.current_line_number) - M_VLCD_LINE_NUMBER_FIELD_LENGTH;
            if ( m_drv_vlcd.current_line_number == 0 )
            {
                dest_addr -= M_VLCD_COMMAND_FIELD_LENGTH;
            }
            size = -buf_length;
            break;
        case DRV_DISP_ENGINE_PROC_ACCESS_TYPE_DATA:
            if ( line_number == FB_INVALID_LINE )
            {
                dest_addr = DRV_23LCV_NO_ADDR;
                size = -buf_length;
            }
            else
            {
                dest_addr = line_payload_addr_get(m_drv_vlcd.fb_pos.y0 + line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);
                size = buf_length;
            }
            break;
        case DRV_DISP_ENGINE_PROC_ACCESS_TYPE_POSTAMBLE:
            dest_addr = DRV_23LCV_NO_ADDR;
            size = buf_length;
        
            m_drv_vlcd.current_line_number += 1;
            break;
    }

    if ( drv_23lcv_write(dest_addr, p_buf, size) != DRV_23LCV_STATUS_CODE_SUCCESS )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


static uint32_t line_read(drv_disp_engine_proc_access_type_t access_type, uint8_t *p_buf, uint16_t line_number, uint8_t length)
{
    while ( access_type != DRV_DISP_ENGINE_PROC_ACCESS_TYPE_DATA ); // Only payload should ever be read.
    
    if ( drv_23lcv_read(p_buf, line_payload_addr_get(m_drv_vlcd.fb_pos.y0 + line_number) + (m_drv_vlcd.fb_pos.x0 >> 3), length) != DRV_23LCV_STATUS_CODE_SUCCESS )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


static uint32_t access_end(void)
{
    if ( drv_23lcv_close() != DRV_23LCV_STATUS_CODE_SUCCESS )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


static drv_disp_engine_user_cfg_t user_cfg =
{
    .access_begin  = access_begin,
    .line_write    = line_write,
    .line_read     = line_read,
    .access_update = access_update,
    .access_end    = access_end,
};


static void drv_23lcv_sig_callback(drv_23lcv_signal_type_t drv_23lcv_signal_type)
{
    drv_disp_engine_proc_drive_info_t drive_info;
    
    drv_disp_engine_proc_drive(DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK, &drive_info);
    
    if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_COMPLETE )
    {
        switch ( drive_info.proc_type )
        {
            case DRV_DISP_ENGINE_PROC_CLEAR:
                m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_CLEARED);
                break;
            case DRV_DISP_ENGINE_PROC_WRITE:
            case DRV_DISP_ENGINE_PROC_UPDATE:
            case DRV_DISP_ENGINE_PROC_FROMFBCPY:
                m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_WRITE_COMPLETE);
                break;
            case DRV_DISP_ENGINE_PROC_READ:
            case DRV_DISP_ENGINE_PROC_TOFBCPY:
                m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_READ_COMPLETE);
                break;
            default:
                for ( ;; );
                //break;
        };
    }
    else if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_PAUSE )
    {
        m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_PAUSE);
    }
    else if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_ERROR )
    {
        m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_ERROR);
    }
}


void drv_vlcd_init(drv_vlcd_cfg_t const * const p_vlcd_cfg)
{
    p_cfg = (drv_vlcd_cfg_t *)p_vlcd_cfg;
    
    m_drv_vlcd.current_output_mode = DRV_VLCD_OUTPUT_MODE_DISABLED;
    
    m_drv_vlcd.fb_pos.x0 = 0;
    m_drv_vlcd.fb_pos.y0 = 0;
}


uint32_t drv_vlcd_storage_size_get(void)
{
    return ( M_VLCD_COMMAND_FIELD_LENGTH + (VLCD_HEIGHT * (M_VLCD_LINE_NUMBER_FIELD_LENGTH + (VLCD_WIDTH / 8) + M_VLCD_SHORT_PADDING_FIELD_LENGTH)) + M_VLCD_LINE_NUMBER_FIELD_LENGTH );
}


void drv_vlcd_callback_set(drv_vlcd_sig_callback_t drv_vlcd_sig_callback)
{
    m_drv_vlcd.current_sig_callback = drv_vlcd_sig_callback;
    
    drv_23lcv_callback_set((m_drv_vlcd.current_sig_callback != NULL) ? drv_23lcv_sig_callback : NULL);
}


static uint32_t vlcd_proc_vlcdfromfb_copy(void)
{
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_FROMFBCPY, NULL) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        m_drv_vlcd.current_line_number      = FB_INVALID_LINE;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
}


static uint32_t vlcd_proc_vlcdtofb_copy(void)
{
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_TOFBCPY, NULL) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        m_drv_vlcd.current_line_number      = FB_INVALID_LINE;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_fb_location_set(drv_vlcd_fb_location_set_action_t action, uint16_t fb_x0, uint16_t fb_y0)
{
    if ( ((fb_x0 & 0x07)    == 0)
    &&   (fb_x0 + FB_WIDTH  <= VLCD_WIDTH)
    &&   (fb_y0 + FB_HEIGHT <= VLCD_HEIGHT) )
    {
        m_drv_vlcd.fb_pos.x0 = fb_x0;
        m_drv_vlcd.fb_pos.y0 = fb_y0;
        
        switch ( action )
        {
            case DRV_VLCD_FB_LOCATION_SET_ACTION_COPY_TO_VLCD:
                return ( vlcd_proc_vlcdfromfb_copy() );
            case DRV_VLCD_FB_LOCATION_SET_ACTION_COPY_FROM_VLCD:
                return ( vlcd_proc_vlcdtofb_copy() );
            case DRV_VLCD_FB_LOCATION_SET_ACTION_NONE:
                return ( DRV_VLCD_STATUS_CODE_SUCCESS );
            default:
                break;
        }
    }
    
    return ( DRV_VLCD_STATUS_CODE_INVALID_PARAM );
}


uint32_t drv_vlcd_output_mode_set(drv_vlcd_output_mode_t output_mode)
{
    switch ( output_mode )
    {
        case DRV_VLCD_OUTPUT_MODE_DIRECT_SPI:
            if ( m_drv_vlcd.current_output_mode == DRV_VLCD_OUTPUT_MODE_DISABLED )
            {
                drv_23lcv_callback_set(NULL);
                if ( (access_begin(NULL, DRV_DISP_ENGINE_PROC_NONE) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS)
                &&   (drv_23lcv_read(NULL, 0, 0) == DRV_23LCV_STATUS_CODE_SUCCESS) )
                {
                    m_drv_vlcd.current_output_mode = DRV_VLCD_OUTPUT_MODE_DIRECT_SPI;
                    return ( DRV_VLCD_STATUS_CODE_SUCCESS );
                }
            }
            break;
        case DRV_VLCD_OUTPUT_MODE_DISABLED:
            if ( (m_drv_vlcd.current_output_mode == DRV_VLCD_OUTPUT_MODE_DIRECT_SPI)
            &&   (drv_23lcv_read(NULL, DRV_23LCV_NO_ADDR, 0) == DRV_23LCV_STATUS_CODE_SUCCESS) )
            {
                (void)access_end();
                
                drv_23lcv_callback_set((m_drv_vlcd.current_sig_callback != NULL) ? drv_23lcv_sig_callback : NULL);
                m_drv_vlcd.current_output_mode = DRV_VLCD_OUTPUT_MODE_DISABLED;
                return ( DRV_VLCD_STATUS_CODE_SUCCESS );
            }
            break;
        default:
            break;
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_update(void)
{
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_UPDATE, NULL) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        m_drv_vlcd.current_line_number      = FB_INVALID_LINE;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_write(uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    drv_disp_engine_access_descr_t * p_access_descr;

    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_WRITE, &p_access_descr) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        p_access_descr->buffers.p_data      = p_line;
        p_access_descr->buffers.data_length = line_length;
        m_drv_vlcd.current_line_number      = line_number;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_read(uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    drv_disp_engine_access_descr_t * p_access_descr;
    
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_READ, &p_access_descr) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        p_access_descr->buffers.p_data      = p_line;
        p_access_descr->buffers.data_length = line_length;
        m_drv_vlcd.current_line_number      = line_number;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


#define LINE_LEN  M_VLCD_COMMAND_FIELD_LENGTH + M_VLCD_LINE_NUMBER_FIELD_LENGTH + (VLCD_WIDTH / 8) + (2 * M_VLCD_PADDING)

uint32_t drv_vlcd_clear(drv_vlcd_color_t bg_color)
{
    static uint8_t m_one_line_vlcd[LINE_LEN];
    
    drv_disp_engine_access_descr_t * p_access_descr;
    
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_CLEAR, &p_access_descr) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {
        uint8_t idx;    

        m_drv_vlcd.current_line_number = 0;
        
        idx = 0;
        m_one_line_vlcd[idx] = M_VLCD_WR;
        idx += M_VLCD_COMMAND_FIELD_LENGTH;
        m_one_line_vlcd[idx] = 1;
        idx += M_VLCD_LINE_NUMBER_FIELD_LENGTH;
        memset(&(m_one_line_vlcd[idx]), (bg_color == DRV_VLCD_COLOR_BLACK) ? 0x00 : 0xFF, VLCD_WIDTH / 8);
        idx += VLCD_WIDTH / 8;
        m_one_line_vlcd[idx++] = M_VLCD_PADDING;
        m_one_line_vlcd[idx  ] = M_VLCD_PADDING;
    
        idx = 0;
        p_access_descr->buffers.p_preamble          = &(m_one_line_vlcd[idx]);
        p_access_descr->buffers.preamble_length     = M_VLCD_COMMAND_FIELD_LENGTH + M_VLCD_LINE_NUMBER_FIELD_LENGTH;
        idx += p_access_descr->buffers.preamble_length;
        p_access_descr->buffers.p_data              = &(m_one_line_vlcd[idx]);
        p_access_descr->buffers.data_length         = VLCD_WIDTH / 8;
        idx += p_access_descr->buffers.data_length;
        p_access_descr->buffers.p_postamble         = &(m_one_line_vlcd[idx]);
        p_access_descr->buffers.postamble_length    = 1;
        
        return ( drv_disp_engine_proc_drive((m_drv_vlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}
