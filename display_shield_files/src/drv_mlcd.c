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
#include "fb.h"
#include "drv_disp_engine.h"
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
    uint8_t scratch_buffer[4];
    struct
    {
        bool                        active;
        drv_mlcd_write_req_type_t   type;
        uint8_t                     line_number;
    } current_wrt_req;
    struct
    {
        bool                                in_progress;
        drv_disp_engine_proc_access_type_t  type;
    } current_access;
    
    uint8_t * p_current_line_number;
    drv_disp_engine_proc_access_type_t access_type;
    drv_mlcd_write_req_type_t       current_wrt_req_type;
    uint16_t                        current_line_number;
    drv_mlcd_sig_callback_t         current_sig_callback;
    drv_mlcd_input_mode_t           current_input_mode;
} m_drv_mlcd;


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event);


static uint32_t access_begin(drv_disp_engine_access_descr_t *p_access_descr, drv_disp_engine_proc_type_t new_proc)
{   
    if ( (  (m_drv_mlcd.current_sig_callback == NULL)
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
        m_drv_mlcd.current_access.in_progress = false;
        m_drv_mlcd.current_wrt_req.active     = false;
        
        m_drv_mlcd.p_current_line_number = NULL;
        
        nrf_gpio_pin_set(p_cfg->spi.ss_pin);

        return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
    }   

    return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
}


static bool access_update(drv_disp_engine_access_descr_t *p_access_descr)
{    
    uint8_t  static const MLCD_NO       = 0x00; //MLCD NOP command (used to switch VCOM)
    uint8_t  static const MLCD_WR       = 0x01; //MLCD write line command
    uint8_t  static const MLCD_CM       = 0x04; //MLCD clear memory command

    uint8_t  static const MLCD_PADDING  = 0x00; // Padding
    uint8_t  static const MLCD_VBIT     = 0x02; // VCOM bit.
    
    switch ( p_access_descr->current_proc )
    {
        case DRV_DISP_ENGINE_PROC_CLEAR:
            if ( p_access_descr->buffers.data_length == 0 )
            {
                p_access_descr->buffers.p_data = &(m_drv_mlcd.scratch_buffer[0]);
                p_access_descr->buffers.p_data[0] = MLCD_CM;
                p_access_descr->buffers.p_data[1] = MLCD_PADDING;
                p_access_descr->buffers.data_length = 2;
            }
            else
            {
                return ( false );
            }
            break;
        case DRV_DISP_ENGINE_PROC_WRITE:
            if ( !m_drv_mlcd.current_wrt_req.active )
            {
                static uint8_t vbit_state = 0;
                
                vbit_state ^= MLCD_VBIT;

                p_access_descr->buffers.p_data = &(m_drv_mlcd.scratch_buffer[0]);
                p_access_descr->buffers.p_data[0] = MLCD_NO | vbit_state;
                p_access_descr->buffers.p_data[1] = MLCD_PADDING;
                p_access_descr->buffers.data_length = 2;
            }
            else
            {
                if ( p_access_descr->buffers.preamble_length == 0 )
                {
                    p_access_descr->buffers.p_preamble  = &(m_drv_mlcd.scratch_buffer[0]);

                    p_access_descr->buffers.p_preamble[0] = MLCD_WR;
                    p_access_descr->buffers.p_preamble[1] = m_drv_mlcd.current_wrt_req.line_number;
                    p_access_descr->buffers.preamble_length = 2;
                }
                else
                {
                    p_access_descr->buffers.p_preamble[0] = m_drv_mlcd.current_wrt_req.line_number;
                    p_access_descr->buffers.preamble_length = 1;
                }
                
                if ( p_access_descr->buffers.postamble_length == 0 )
                {
                    p_access_descr->buffers.p_postamble = &(m_drv_mlcd.scratch_buffer[2]);
                    
                    p_access_descr->buffers.p_postamble[0] = MLCD_PADDING; 
                    p_access_descr->buffers.p_postamble[1] = MLCD_PADDING; 
                    
                    if ( m_drv_mlcd.current_wrt_req.type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE )
                    {
                        p_access_descr->buffers.preamble_length = 2;
                    }
                    else
                    {
                        p_access_descr->buffers.preamble_length = 1;
                    } 
                }
                else
                {
                    if ( m_drv_mlcd.current_wrt_req.type == DRV_MLCD_WRITE_REQ_TYPE_LAST )
                    {
                        p_access_descr->buffers.preamble_length = 2;
                    }
                    
                    return ( false );
                }
            }
            break;
        case DRV_DISP_ENGINE_PROC_UPDATE:
                if ( p_access_descr->buffers.data_length != 0 )
                {
                    if ( p_access_descr->buffers.preamble_length == 0 )
                    {
                        p_access_descr->buffers.p_preamble  = &(m_drv_mlcd.scratch_buffer[0]);
                        p_access_descr->buffers.p_postamble = &(m_drv_mlcd.scratch_buffer[2]);

                        p_access_descr->buffers.p_preamble[0] = MLCD_WR;
                        m_drv_mlcd.p_current_line_number = &(p_access_descr->buffers.p_preamble[1]); 
                        p_access_descr->buffers.preamble_length = 2;
                        
                        p_access_descr->buffers.p_postamble[0] = MLCD_PADDING; 
                        p_access_descr->buffers.postamble_length = 1;
                    }
                    else
                    {
                        m_drv_mlcd.p_current_line_number = &(p_access_descr->buffers.p_preamble[0]); 
                        p_access_descr->buffers.preamble_length = 1;
                    }
                }
                else if ( p_access_descr->buffers.preamble_length != 0 )
                {
                    m_drv_mlcd.p_current_line_number = NULL;
                    p_access_descr->buffers.preamble_length = 0;
                }
                else
                {
                    return ( false );
                }
                break;
        default:
            break;
    }
    
    return ( true );
}


static uint32_t line_write(drv_disp_engine_proc_access_type_t access_type, uint16_t line_number, uint8_t *p_buf, uint8_t buf_length)
{
    m_drv_mlcd.current_access.in_progress = true;
    m_drv_mlcd.current_access.type = access_type;
    
    if ( (line_number                      != FB_INVALID_LINE)
    &&   (m_drv_mlcd.p_current_line_number != NULL) )
    {
        *m_drv_mlcd.p_current_line_number = (uint8_t)line_number;
    }
    
    if (  nrf_drv_spi_transfer(p_cfg->spi.p_instance, p_buf, buf_length, NULL, 0) != NRF_SUCCESS )
    {
        m_drv_mlcd.current_access.in_progress = false;
        
        return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


static uint32_t line_read(drv_disp_engine_proc_access_type_t access_type, uint8_t *p_buf, uint16_t line_number, uint8_t length)
{
    (void)access_type;
    (void)p_buf;
    (void)line_number;
    (void)length;
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
}


static uint32_t access_end(void)
{
    nrf_gpio_pin_clear(p_cfg->spi.ss_pin);
    
    if ( m_drv_mlcd.current_input_mode != DRV_MLCD_INPUT_MODE_DIRECT_SPI )
    {
        nrf_drv_spi_uninit(p_cfg->spi.p_instance);
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


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event)
{
    drv_disp_engine_proc_drive_info_t drive_info;
    
    (void)event;
    drv_disp_engine_proc_drive(DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK, NULL);
    if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_COMPLETE )
    {
        switch ( drive_info.proc_type )
        {
            case DRV_DISP_ENGINE_PROC_CLEAR:
                m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_CLEARED);
                break;
            case DRV_DISP_ENGINE_PROC_WRITE:
            case DRV_DISP_ENGINE_PROC_UPDATE:
                m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_WRITE_COMPLETE);
                break;
            default:
                for ( ;; );
                //break;
        };
    }
    else if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_PAUSE )
    {
        m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_WRITE_PAUSED);
    }
    else if ( drive_info.exit_status == DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_ERROR )
    {
        m_drv_mlcd.current_sig_callback(DRV_MLCD_SIGNAL_TYPE_ERROR);
    }
}


void drv_mlcd_init(drv_mlcd_cfg_t const * const p_drv_mlcd_cfg)
{
    p_cfg = (drv_mlcd_cfg_t *)p_drv_mlcd_cfg;
    
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
            if ( m_drv_mlcd.current_input_mode == DRV_MLCD_INPUT_MODE_DISABLED )
            {
                nrf_gpio_pin_set(p_cfg->spi.ss_pin);

                m_drv_mlcd.current_input_mode = input_mode;
                
                return ( DRV_MLCD_STATUS_CODE_SUCCESS );
            }
            break;
        case DRV_MLCD_INPUT_MODE_DISABLED:
            if ( m_drv_mlcd.current_input_mode == DRV_MLCD_INPUT_MODE_DIRECT_SPI )
            {
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


uint32_t drv_mlcd_update(void)
{
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_UPDATE, NULL) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {   
        return ( drv_disp_engine_proc_drive((m_drv_mlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_clear(void)
{
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_CLEAR, NULL) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {
        
        return ( drv_disp_engine_proc_drive((m_drv_mlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_nop(void)
{
    drv_disp_engine_access_descr_t * p_access_descr;
    
    if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_WRITE, &p_access_descr) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
    {
        return ( drv_disp_engine_proc_drive((m_drv_mlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );    
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}


uint32_t drv_mlcd_write(drv_mlcd_write_req_type_t write_req_type, uint8_t line_number, uint8_t line_length, uint8_t *p_line)
{
    static drv_disp_engine_access_descr_t * p_access_descr = NULL;
    
    if ( (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_SINGLE)
    ||   (write_req_type == DRV_MLCD_WRITE_REQ_TYPE_START) )
    {
        if ( drv_disp_engine_proc_initiate(&user_cfg, DRV_DISP_ENGINE_PROC_WRITE, &p_access_descr) == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
        {
            m_drv_mlcd.current_wrt_req.type        = write_req_type;
            m_drv_mlcd.current_wrt_req.line_number = line_number;
            m_drv_mlcd.current_wrt_req.active      = true;
            
            p_access_descr->buffers.p_data      = p_line;
            p_access_descr->buffers.data_length = line_length;
             
            return ( drv_disp_engine_proc_drive((m_drv_mlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );
        }
        else
        {
            p_access_descr = NULL;
            
            return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
        }
    }
    else if ( m_drv_mlcd.current_wrt_req.active )
    {
        m_drv_mlcd.current_wrt_req.type        = write_req_type;
        m_drv_mlcd.current_wrt_req.line_number = line_number;

        p_access_descr->buffers.p_data      = p_line;
        p_access_descr->buffers.data_length = line_length;
        
        return ( drv_disp_engine_proc_drive((m_drv_mlcd.current_sig_callback != NULL) ? DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK : DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, NULL) );
    }
    
    return ( DRV_MLCD_STATUS_CODE_DISALLOWED );
}
