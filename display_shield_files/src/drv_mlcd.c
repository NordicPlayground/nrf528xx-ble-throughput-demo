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
#include "drv_mlcd.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"

#include <stdlib.h>

typedef enum
{
    M_PROC_NONE = 0,
    M_PROC_NOP,
    M_PROC_CLEAR,
    M_PROC_WRITE,
    M_PROC_UPDATE,
    M_PROC_DIRECT_SPI,
} m_proc_type_t;


static drv_mlcd_cfg_t *p_cfg;


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
        M_CMD_STATE_WRT_PAUSED,       ///< The write command has paused.
        M_CMD_STATE_WRT_ERROR,        ///< An error occured while writing.
    } cmd_state;
    m_proc_type_t               current_proc;
    drv_mlcd_write_req_type_t   current_wrt_req_type;
    uint8_t vbit_state;
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
        drv_mlcd_fb_next_dirty_line_get_t   fb_next_dirty_line_get;
        uint8_t *                           p_next_line;
        uint16_t                            next_line_number;
        uint8_t                             next_line_length;
    } update_status;
    drv_mlcd_sig_callback_t current_sig_callback;
    drv_mlcd_input_mode_t   current_input_mode;
} m_drv_mlcd;


uint8_t  static const M_MLCD_NO       = 0x00; //MLCD NOP command (used to switch VCOM)
uint8_t  static const M_MLCD_WR       = 0x01; //MLCD write line command
uint8_t  static const M_MLCD_CM       = 0x04; //MLCD clear memory command

uint8_t  static const M_MLCD_PADDING  = 0x00; // Padding
uint8_t  static const M_MLCD_VBIT     = 0x02; // VCOM bit.


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event);


static bool begin_mlcd_access(m_proc_type_t new_proc)
{
    if ( (m_drv_mlcd.current_proc == M_PROC_NONE)
    &&   (new_proc                != M_PROC_NONE) )
    {        
        if ( (p_cfg                       != NULL)
        &&   (p_cfg->spi.p_config         != NULL)
        &&   (p_cfg->spi.p_instance       != NULL)
        &&   (p_cfg->spi.p_config->ss_pin == NRF_DRV_SPI_PIN_NOT_USED) )
        {
            if ( (new_proc == M_PROC_DIRECT_SPI)
            ||
                 (  (m_drv_mlcd.current_sig_callback == NULL)
                    && 
                    (nrf_drv_spi_init(p_cfg->spi.p_instance, p_cfg->spi.p_config, NULL) == NRF_SUCCESS)
                 )
            ||
                 ( (m_drv_mlcd.current_sig_callback != NULL)
                    && 
                    (nrf_drv_spi_init(p_cfg->spi.p_instance, p_cfg->spi.p_config, nrf_drv_spi_handler)  == NRF_SUCCESS)
                 )
            )
            {
                m_drv_mlcd.current_proc = new_proc;
                m_drv_mlcd.cmd_state    = M_CMD_STATE_IDLE;
                
                nrf_gpio_pin_set(p_cfg->spi.ss_pin);
     
                return ( true );
            }
        }
    }

    return ( false );
}


static void end_mlcd_access(void)
{
    if ( m_drv_mlcd.current_proc != M_PROC_NONE )
    {
        nrf_gpio_pin_clear(p_cfg->spi.ss_pin);
        if ( m_drv_mlcd.current_proc != M_PROC_DIRECT_SPI )
        {
            nrf_drv_spi_uninit(p_cfg->spi.p_instance);
        }
        
        m_drv_mlcd.current_proc = M_PROC_NONE;
    }
}


void m_mlcd_write_setup(drv_mlcd_write_req_type_t write_req_type, uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
 
    if ( (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE)
    ||   (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_START) )
    {            
            m_drv_mlcd.buffers.start[0]     = M_MLCD_WR;
            m_drv_mlcd.buffers.start[1]     = line_number;
            m_drv_mlcd.buffers.start_length = 2;
    }
    else
    {
        m_drv_mlcd.buffers.start[0]     = line_number;
        m_drv_mlcd.buffers.start_length = 1;
    }

    m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_PREAMBLE;
    m_drv_mlcd.current_wrt_req_type = write_req_type;

    m_drv_mlcd.buffers.p_data      = p_line;
    m_drv_mlcd.buffers.data_length = line_length;
    
    m_drv_mlcd.buffers.end[0] = M_MLCD_PADDING;
    m_drv_mlcd.buffers.end[1] = M_MLCD_PADDING;
    if ( (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE)
    ||   (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_LAST) )
    {
        m_drv_mlcd.buffers.end_length = 2;
    }
    else
    {
        m_drv_mlcd.buffers.end_length = 1;
    }
}


static void m_proc_update_cmd_next(void)
{
    uint8_t     line_number, line_length;
    uint8_t *   p_line;

    line_number = m_drv_mlcd.update_status.next_line_number;
    line_length = m_drv_mlcd.update_status.next_line_length;
    p_line = m_drv_mlcd.update_status.p_next_line;
    
    m_drv_mlcd.update_status.next_line_number =
        m_drv_mlcd.update_status.fb_next_dirty_line_get(&m_drv_mlcd.update_status.next_line_length, &m_drv_mlcd.update_status.p_next_line);
    if ( m_drv_mlcd.update_status.next_line_number != 0xFFFF )
    {
        if ( m_drv_mlcd.cmd_state == M_CMD_STATE_IDLE )
        {
            m_mlcd_write_setup(DRV_MLCD_WRITE_REQ_TYPE_START, line_number, line_length, p_line);
        }
        else
        {
            m_mlcd_write_setup(DRV_MLCD_WRITE_REQ_TYPE_CONTINUE, line_number, line_length, p_line);
        }
    }
    else
    {
        if ( m_drv_mlcd.cmd_state == M_CMD_STATE_IDLE )
        {
            m_mlcd_write_setup(DRV_MLCD_WRITE_REQ_TYPE_SINGLE, line_number, line_length, p_line);
        }
        else
        {
            m_mlcd_write_setup(DRV_MLCD_WRITE_REQ_TYPE_LAST, line_number, line_length, p_line);
        }
    }
}


static void m_generic_wrt_seq_handle(void)
{
    switch ( m_drv_mlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
            m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_DATA;
            if ( nrf_drv_spi_transfer(p_cfg->spi.p_instance, &(m_drv_mlcd.buffers.start[0]), m_drv_mlcd.buffers.start_length, NULL, 0) != NRF_SUCCESS )
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        case M_CMD_STATE_WRT_DATA:
            m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_POSTAMBLE;
            if ( nrf_drv_spi_transfer(p_cfg->spi.p_instance, m_drv_mlcd.buffers.p_data, m_drv_mlcd.buffers.data_length, NULL, 0) != NRF_SUCCESS )
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( nrf_drv_spi_transfer(p_cfg->spi.p_instance, &(m_drv_mlcd.buffers.end[0]), m_drv_mlcd.buffers.end_length, NULL, 0) != NRF_SUCCESS )
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            break;
        default:
            /* Any ther command state shall be handled specifically. */
            break;
    }
}


static void m_proc_clear_cmd_handle(void)
{
    switch ( m_drv_mlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
        case M_CMD_STATE_WRT_DATA:
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_generic_wrt_seq_handle();
            break;
        case M_CMD_STATE_WRT_LAST:
            m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            break;
        case M_CMD_STATE_IDLE:
        default:
            // ASSERT( false );
            break;
    }
}
                

static void m_proc_update_cmd_handle(void)
{
    switch ( m_drv_mlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
        case M_CMD_STATE_WRT_DATA:
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_generic_wrt_seq_handle();
            break;
        case M_CMD_STATE_WRT_LAST:
            if ( (m_drv_mlcd.current_wrt_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE) 
            ||   (m_drv_mlcd.current_wrt_req_type == DRV_MLCD_WRITE_REQ_TYPE_LAST) )
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            else
            {
                m_proc_update_cmd_next();
                m_generic_wrt_seq_handle();
            }
          //else
          //{
          //    ASSERT( false );
          //}
            break;
        case M_CMD_STATE_IDLE:
        default:
            // ASSERT( false );
            break;
    }
}


static void m_proc_write_cmd_handle(void)
{
    switch ( m_drv_mlcd.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
        case M_CMD_STATE_WRT_DATA:
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_generic_wrt_seq_handle();
            break;
        case M_CMD_STATE_WRT_LAST:
            if ( (m_drv_mlcd.current_wrt_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE) 
            ||   (m_drv_mlcd.current_wrt_req_type == DRV_MLCD_WRITE_REQ_TYPE_LAST) )
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            else
            {
                m_drv_mlcd.cmd_state = M_CMD_STATE_WRT_PAUSED;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static uint32_t cmd_handler(void)
{
    uint32_t    result  = DRV_MLCD_STATUS_CODE_SUCCESS;
    bool        done    = (m_drv_mlcd.current_sig_callback != NULL);

    do
    {
        switch ( m_drv_mlcd.current_proc )
        {
            case M_PROC_CLEAR:
                m_proc_clear_cmd_handle();
                break;
            case M_PROC_WRITE:
                m_proc_write_cmd_handle();
                break;
            case M_PROC_UPDATE:
                m_proc_update_cmd_handle();
                break;
            case M_PROC_NONE:
            default:
                for ( ;; );
                // break;
        }
        
        if ( m_drv_mlcd.cmd_state == M_CMD_STATE_WRT_COMPLETE )
        {
            m_proc_type_t latest_proc = m_drv_mlcd.current_proc;
            (void)end_mlcd_access();
            done = true;

            if ( m_drv_mlcd.current_sig_callback != NULL )
            {
                switch ( latest_proc )
                {
                    case M_PROC_CLEAR:
                        m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_CLEARED);
                        break;
                    case M_PROC_UPDATE:
                    case M_PROC_WRITE:
                        m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_WRITE_COMPLETE);
                        break;
                    case M_PROC_NONE:
                    default:
                        for ( ;; );
                        // break;
                }
            }
        }
        else if ( m_drv_mlcd.cmd_state == M_CMD_STATE_WRT_ERROR )
        {
            (void)end_mlcd_access();
            result = DRV_MLCD_STATUS_CODE_DISALLOWED;
            done = true;
            
            if ( m_drv_mlcd.current_sig_callback != NULL )
            {
                m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_ERROR);
            }
        }
        else if ( m_drv_mlcd.cmd_state == M_CMD_STATE_WRT_PAUSED )
        {
            done = true;
            
            if ( m_drv_mlcd.current_sig_callback != NULL )
            {
                m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_WRITE_PAUSED);
            }
        }
    } while ( !done );
    
    return ( result );
}


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event)
{
    (void)event;
    cmd_handler();
}


void drv_mlcd_init(drv_mlcd_cfg_t const * const p_drv_mlcd_cfg)
{
    p_cfg = (drv_mlcd_cfg_t *)p_drv_mlcd_cfg;
    
    m_drv_mlcd.vbit_state   = 0;
    m_drv_mlcd.current_proc = M_PROC_NONE;
    
    m_drv_mlcd.current_input_mode = DRV_MLCD_INPUT_MODE_DISABLED;
    
    nrf_gpio_pin_clear(p_cfg->spi.ss_pin);
    nrf_gpio_pin_dir_set(p_cfg->spi.ss_pin, NRF_GPIO_PIN_DIR_OUTPUT);
}


void drv_mlcd_callback_set(drv_mlcd_sig_callback_t drv_mlcd_sig_callback)
{
    m_drv_mlcd.current_sig_callback = drv_mlcd_sig_callback;
}


uint32_t drv_mlcd_input_mode_set(drv_mlcd_input_mode_t input_mode)
{
    switch ( input_mode )
    {
        case DRV_MLCD_INPUT_MODE_DIRECT_SPI:
            if ( (m_drv_mlcd.current_input_mode == DRV_MLCD_INPUT_MODE_DISABLED)
            &&   (begin_mlcd_access(M_PROC_DIRECT_SPI))  )
            {
                nrf_gpio_pin_set(p_cfg->spi.ss_pin);
                m_drv_mlcd.current_input_mode = input_mode;
                
                return ( DRV_MLCD_STATUS_CODE_SUCCESS );
            }
            break;
        case DRV_MLCD_INPUT_MODE_DISABLED:
            if ( m_drv_mlcd.current_input_mode == DRV_MLCD_INPUT_MODE_DIRECT_SPI )
            {
                end_mlcd_access();
                nrf_gpio_pin_clear(p_cfg->spi.ss_pin);
                m_drv_mlcd.current_input_mode = input_mode;
                
                return ( DRV_MLCD_STATUS_CODE_SUCCESS );
            }
            break;
        default:
            break;
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_update(drv_mlcd_fb_next_dirty_line_get_t drv_mlcd_fb_next_dirty_line_get)
{
    if ( begin_mlcd_access(M_PROC_UPDATE) )
    {
        m_drv_mlcd.update_status.fb_next_dirty_line_get = drv_mlcd_fb_next_dirty_line_get;
        m_drv_mlcd.update_status.next_line_number = m_drv_mlcd.update_status.fb_next_dirty_line_get(&m_drv_mlcd.update_status.next_line_length, &m_drv_mlcd.update_status.p_next_line);
        if ( m_drv_mlcd.update_status.next_line_number != 0xFFFF )
        {
            m_proc_update_cmd_next();
            
            return ( cmd_handler() );
        }
        else
        {
            (void)end_mlcd_access();
        }
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_clear(void)
{
    if ( begin_mlcd_access(M_PROC_CLEAR) )
    {
        m_drv_mlcd.buffers.end[0]     = M_MLCD_CM;
        m_drv_mlcd.buffers.end[1]     = M_MLCD_PADDING;
        m_drv_mlcd.buffers.end_length = 2;
        
        m_drv_mlcd.current_wrt_req_type = DRV_MLCD_WRITE_REQ_TYPE_SINGLE;
        m_drv_mlcd.cmd_state            = M_CMD_STATE_WRT_POSTAMBLE;
        
        return ( cmd_handler() ); 
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_nop(void)
{
    if ( begin_mlcd_access(M_PROC_NOP) )
    {
        m_drv_mlcd.vbit_state ^= M_MLCD_VBIT;
        
        m_drv_mlcd.buffers.end[0]     = M_MLCD_NO | m_drv_mlcd.vbit_state;
        m_drv_mlcd.buffers.end[1]     = M_MLCD_PADDING;
        m_drv_mlcd.buffers.end_length = 2;
        
        m_drv_mlcd.current_wrt_req_type = DRV_MLCD_WRITE_REQ_TYPE_SINGLE;
        m_drv_mlcd.cmd_state            = M_CMD_STATE_WRT_POSTAMBLE;

        return ( cmd_handler() ); 
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_write(drv_mlcd_write_req_type_t write_req_type, uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    if ( (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE)
    ||   (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_START) )
    {
        if ( !begin_mlcd_access(M_PROC_WRITE) )
        {
            return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
        }
    }

    m_mlcd_write_setup(write_req_type, line_number, line_length, p_line);
    
    return (  cmd_handler() );
}
