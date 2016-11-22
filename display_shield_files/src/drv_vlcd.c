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
#include "fb.h"
#include "nrf.h"

#include <stdlib.h>
#include <string.h>


typedef enum
{
    M_PROC_NONE = 0,
    M_PROC_CLEAR,
    M_PROC_WRITE,
    M_PROC_READ,
    M_PROC_UPDATE,
    M_PROC_VLCDTOFB_COPY,
    M_PROC_VLCDFROMFB_COPY,
    M_PROC_DIRECT_SPI,
} m_proc_type_t;


static drv_vlcd_cfg_t *p_cfg;


static struct
{
    enum
    {
        M_CMD_STATE_IDLE = 0,         ///< No command pending or in progress.
        M_CMD_STATE_WRT_PREAMBLE,     ///< A write preamble is pending or in progress.
        M_CMD_STATE_WRT_DATA,         ///< A data buffer write operation is pending or in progress.
        M_CMD_STATE_WRT_POSTAMBLE,    ///< A write postamble is pending or in progress.
        M_CMD_STATE_WRT_LAST,         ///< Last portion of the write command is pending or in progress.
        M_CMD_STATE_WRT_COMPLETE,     ///< The write command has complete.
        M_CMD_STATE_WRT_ERROR,        ///< An error occured while writing.
        M_CMD_STATE_READ_PREAMBLE,    ///< A write preamble is pending or in progress.
        M_CMD_STATE_READ_DATA,        ///< A data buffer read operation is pending or in progress.
        M_CMD_STATE_READ_POSTAMBLE,   ///< A read postamble is pending or in progress.
        M_CMD_STATE_READ_LAST,        ///< Last portion of the read command is pending or in progress.
        M_CMD_STATE_READ_COMPLETE,    ///< The read command has complete.
        M_CMD_STATE_READ_ERROR,       ///< An error occured while reading.
    } cmd_state;
    m_proc_type_t current_proc;
    struct
    {
        uint8_t *   p_data;
        uint8_t     start[2];
        uint8_t     end[2];
        uint8_t     data_length;
        uint8_t     start_length;
        uint8_t     end_length;
    } buffers;
    struct
    {
        uint16_t    x0;
        uint16_t    y0;
    } fb_pos;
    uint16_t                current_line_number;
    drv_vlcd_sig_callback_t current_sig_callback;
    drv_vlcd_output_mode_t  current_output_mode;
} m_drv_vlcd;


//uint8_t  static const M_VLCD_NO       = 0x00; //M_VLCD NOP command (used to switch VCOM)
uint8_t  static const M_VLCD_WR       = 0x01; //M_VLCD write line command
//uint8_t  static const M_VLCD_CM       = 0x04; //M_VLCD clear memory command

uint8_t  static const M_VLCD_PADDING  = 0x00; // Padding

uint8_t  static const M_VLCD_COMMAND_FIELD_LENGTH         = 1;
uint8_t  static const M_VLCD_LINE_NUMBER_FIELD_LENGTH     = 1;
uint8_t  static const M_VLCD_SHORT_PADDING_FIELD_LENGTH   = 1;


static void drv_23lcv_sig_callback(drv_23lcv_signal_type_t drv_23lcv_signal_type);


static uint32_t cmd_handler(void);


static uint32_t line_addr_get(uint16_t line_number)
{
    return ( M_VLCD_COMMAND_FIELD_LENGTH + (line_number * (M_VLCD_LINE_NUMBER_FIELD_LENGTH + (VLCD_WIDTH / 8) + M_VLCD_SHORT_PADDING_FIELD_LENGTH)) + M_VLCD_LINE_NUMBER_FIELD_LENGTH );
}


static bool begin_vlcd_access(m_proc_type_t new_proc)
{    
    if ( (m_drv_vlcd.current_proc == M_PROC_NONE)
    &&   (new_proc                != M_PROC_NONE) )
    {
        static drv_23lcv_cfg_t cfg;
     
        m_drv_vlcd.current_proc = new_proc;
        m_drv_vlcd.cmd_state    = M_CMD_STATE_IDLE;
        
        cfg.spi.ss_pin     = p_cfg->spi.ss_pin;
        cfg.spi.p_config   = p_cfg->spi.p_config;
        cfg.spi.p_instance = p_cfg->spi.p_instance;

        return ( drv_23lcv_open(&cfg) == DRV_VLCD_STATUS_CODE_SUCCESS );
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


static bool end_vlcd_access(void)
{
    if ( m_drv_vlcd.current_proc != M_PROC_NONE )
    {
        m_drv_vlcd.current_proc = M_PROC_NONE;
        
        return ( drv_23lcv_close() == DRV_23LCV_STATUS_CODE_SUCCESS );
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


static void proc_clear_cmd_handle(void)
{
    uint16_t start_addr;
    
    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
            start_addr = line_addr_get(m_drv_vlcd.current_line_number) - m_drv_vlcd.buffers.start_length;
            
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_DATA;
            if ( drv_23lcv_write(start_addr, &(m_drv_vlcd.buffers.start[0]), -m_drv_vlcd.buffers.start_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;
        case M_CMD_STATE_WRT_DATA:
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_POSTAMBLE;
            if ( drv_23lcv_write(DRV_23LCV_NO_ADDR, m_drv_vlcd.buffers.p_data, -m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( drv_23lcv_write(DRV_23LCV_NO_ADDR, &(m_drv_vlcd.buffers.end[0]),  m_drv_vlcd.buffers.end_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;    
        case M_CMD_STATE_WRT_LAST:
            ++m_drv_vlcd.current_line_number;
            
            if ( m_drv_vlcd.current_line_number == VLCD_HEIGHT )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            else 
            {
                if ( m_drv_vlcd.current_line_number == (VLCD_HEIGHT - 1) )
                {
                    m_drv_vlcd.buffers.end_length = 2;
                }
                
                m_drv_vlcd.buffers.start[0]     = m_drv_vlcd.current_line_number + 1;
                m_drv_vlcd.buffers.start_length = 1;
                
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_PREAMBLE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_write_cmd_handle(void)
{
    uint16_t start_addr;
    
    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_DATA:
            start_addr = line_addr_get(m_drv_vlcd.current_line_number);
            
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( drv_23lcv_write(start_addr, m_drv_vlcd.buffers.p_data, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        case M_CMD_STATE_WRT_LAST:
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_read_cmd_handle(void)
{
    int16_t start_addr;
    
    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_READ_DATA:
            start_addr = line_addr_get(m_drv_vlcd.current_line_number);

            m_drv_vlcd.cmd_state = M_CMD_STATE_READ_LAST;
            if ( drv_23lcv_read(m_drv_vlcd.buffers.p_data, start_addr, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_READ_ERROR;
            }
            break;
        case M_CMD_STATE_READ_LAST:
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_update_cmd_handle(void)
{
    int16_t     start_addr;

    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_DATA:
           start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);

            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( drv_23lcv_write(start_addr, m_drv_vlcd.buffers.p_data, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        case M_CMD_STATE_WRT_LAST:
            m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_next_dirty_line_get(&m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
            if ( m_drv_vlcd.current_line_number != 0xFFFF )
            {
                start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);
                if ( drv_23lcv_write(start_addr, m_drv_vlcd.buffers.p_data, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
                {
                    m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
                }
            }
            else
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_vlcdtofb_cmd_handle(void)
{
    int16_t start_addr;
    
    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_READ_DATA:
            start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);

            m_drv_vlcd.cmd_state = M_CMD_STATE_READ_LAST;
            if ( drv_23lcv_read(m_drv_vlcd.buffers.p_data, start_addr, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_READ_ERROR;
            }
            break;
        case M_CMD_STATE_READ_LAST:
            p_cfg->vlcd_fb_line_storage_set(m_drv_vlcd.current_line_number, m_drv_vlcd.buffers.data_length, m_drv_vlcd.buffers.p_data);
            m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_line_storage_ptr_get(m_drv_vlcd.current_line_number + 1, &m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
            if ( m_drv_vlcd.current_line_number != 0xFFFF )
            {
                start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);
                if ( drv_23lcv_read(m_drv_vlcd.buffers.p_data, start_addr, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
                {
                    m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
                }
            }
            else
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_vlcdfromfb_cmd_handle(void)
{
    uint16_t start_addr;
    
    switch ( m_drv_vlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_DATA:
           start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);

            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( drv_23lcv_write(start_addr, m_drv_vlcd.buffers.p_data, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        case M_CMD_STATE_WRT_LAST:
            m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_line_storage_ptr_get(m_drv_vlcd.current_line_number + 1, &m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
            if ( m_drv_vlcd.current_line_number != 0xFFFF )
            {
                start_addr = line_addr_get(m_drv_vlcd.fb_pos.y0 + m_drv_vlcd.current_line_number) + (m_drv_vlcd.fb_pos.x0 >> 3);
                if ( drv_23lcv_write(start_addr, m_drv_vlcd.buffers.p_data, m_drv_vlcd.buffers.data_length) != DRV_23LCV_STATUS_CODE_SUCCESS )
                {
                    m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
                }
            }
            else
            {
                m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static uint32_t cmd_handler(void)
{
    uint32_t    result  = DRV_VLCD_STATUS_CODE_SUCCESS;
    bool        done    = (m_drv_vlcd.current_sig_callback != NULL);

    do
    {
        switch ( m_drv_vlcd.current_proc )
        {
            case M_PROC_CLEAR:
                proc_clear_cmd_handle();
                break;
            case M_PROC_WRITE:
                proc_write_cmd_handle();
                break;
            case M_PROC_READ:
                proc_read_cmd_handle();
                break;
            case M_PROC_UPDATE:
                proc_update_cmd_handle();
                break;
            case M_PROC_VLCDTOFB_COPY:
                proc_vlcdtofb_cmd_handle();
                break;
            case M_PROC_VLCDFROMFB_COPY:
                proc_vlcdfromfb_cmd_handle();
                break;
            case M_PROC_NONE:
            default:
                for ( ;; );
                // break;
        }
        
        if ( (m_drv_vlcd.cmd_state == M_CMD_STATE_WRT_COMPLETE)
        ||   (m_drv_vlcd.cmd_state == M_CMD_STATE_READ_COMPLETE) )
        {
            m_proc_type_t latest_proc = m_drv_vlcd.current_proc;
            (void)end_vlcd_access();
            done = true;

            if ( m_drv_vlcd.current_sig_callback != NULL )
            {
                switch ( latest_proc )
                {
                    case M_PROC_CLEAR:
                        m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_CLEARED);
                        break;
                    case M_PROC_UPDATE:
                    case M_PROC_WRITE:
                    case M_PROC_VLCDFROMFB_COPY:
                        m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_WRITE_COMPLETE);
                        break;
                    case M_PROC_VLCDTOFB_COPY:
                    case M_PROC_READ:
                        m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_READ_COMPLETE);
                        break;
                    case M_PROC_NONE:
                    default:
                        for ( ;; );
                        // break;
                }
            }
        }
        else if ( (m_drv_vlcd.cmd_state == M_CMD_STATE_WRT_ERROR)
        ||        (m_drv_vlcd.cmd_state == M_CMD_STATE_READ_ERROR) )
        {
            (void)end_vlcd_access();
            result = DRV_VLCD_STATUS_CODE_DISALLOWED;
            done = true;
            
            if ( m_drv_vlcd.current_sig_callback != NULL )
            {
                m_drv_vlcd.current_sig_callback(DRV_VLCD_SIGNAL_TYPE_ERROR);
            }
        }
    } while ( !done );
    
    return ( result );
}


static void drv_23lcv_sig_callback(drv_23lcv_signal_type_t drv_23lcv_signal_type)
{
    (void)cmd_handler();
}


void drv_vlcd_init(drv_vlcd_cfg_t const * const p_vlcd_cfg)
{
    p_cfg = (drv_vlcd_cfg_t *)p_vlcd_cfg;
    
    m_drv_vlcd.current_proc = M_PROC_NONE;
    
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
    if ( begin_vlcd_access(M_PROC_VLCDFROMFB_COPY) )
    {
        m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_line_storage_ptr_get(0, &m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
        if ( m_drv_vlcd.current_line_number != 0xFFFF )
        {
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_DATA;
            
            return ( cmd_handler() );
        }
        else
        {
            (void)end_vlcd_access();
        }
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


static uint32_t vlcd_proc_vlcdtofb_copy(void)
{
    if ( begin_vlcd_access(M_PROC_VLCDTOFB_COPY) )
    {
        m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_line_storage_ptr_get(0, &m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
        if ( m_drv_vlcd.current_line_number != 0xFFFF )
        {
            m_drv_vlcd.cmd_state = M_CMD_STATE_READ_DATA;
            
            return ( cmd_handler() );
        }
        else
        {
            (void)end_vlcd_access();
        }
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
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
            
                if ( (begin_vlcd_access(M_PROC_DIRECT_SPI))
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
                (void)end_vlcd_access();
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
    if ( begin_vlcd_access(M_PROC_UPDATE) )
    {
        m_drv_vlcd.current_line_number = p_cfg->vlcd_fb_next_dirty_line_get(&m_drv_vlcd.buffers.data_length, &m_drv_vlcd.buffers.p_data);
        if ( m_drv_vlcd.current_line_number != 0xFFFF )
        {
            m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_DATA;
            
            return ( cmd_handler() );
        }
        else
        {
            (void)end_vlcd_access();
        }
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_write(uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    if (begin_vlcd_access(M_PROC_WRITE))
    {
        m_drv_vlcd.current_line_number = line_number;
        m_drv_vlcd.buffers.p_data      = &(p_line[0]);
        m_drv_vlcd.buffers.data_length = line_length;
        
        m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_DATA;
        
        return (  cmd_handler() );
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_read(uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    if (begin_vlcd_access(M_PROC_READ))
    {
        m_drv_vlcd.current_line_number = line_number;
        m_drv_vlcd.buffers.p_data      = &(p_line[0]);
        m_drv_vlcd.buffers.data_length = line_length;
        
        m_drv_vlcd.cmd_state = M_CMD_STATE_READ_DATA;
        
        return (  cmd_handler() );
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_vlcd_clear(drv_vlcd_color_t bg_color)
{
    static uint8_t buffer[VLCD_WIDTH / 8];
    
    if ( begin_vlcd_access(M_PROC_CLEAR) )
    {
        m_drv_vlcd.current_line_number = 0;
                
        m_drv_vlcd.buffers.start[0]     = M_VLCD_WR;
        m_drv_vlcd.buffers.start[1]     = m_drv_vlcd.current_line_number + 1;
        m_drv_vlcd.buffers.start_length = 2;
        
        memset(&(buffer[0]), (bg_color == DRV_VLCD_COLOR_BLACK) ? 0x00 : 0xFF, VLCD_WIDTH / 8);
        m_drv_vlcd.buffers.p_data      = &(buffer[0]);
        m_drv_vlcd.buffers.data_length = VLCD_WIDTH / 8;
        
        m_drv_vlcd.buffers.end[0]     = M_VLCD_PADDING;
        m_drv_vlcd.buffers.end[1]     = M_VLCD_PADDING;
        m_drv_vlcd.buffers.end_length = 1;

        m_drv_vlcd.cmd_state = M_CMD_STATE_WRT_PREAMBLE;
        
        return ( cmd_handler() );    
    }
    
    return ( DRV_VLCD_STATUS_CODE_DISALLOWED );
}
