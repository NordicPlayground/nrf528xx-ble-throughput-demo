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
#include "drv_disp_engine.h"

#include <stdlib.h>


struct
{
    enum
    {
        M_CMD_STATE_IDLE = 0,         ///< No command pending or in progress.
        M_CMD_STATE_WRT_PREAMBLE,     ///< A write preamble is pending or in progress.
        M_CMD_STATE_WRT_DATA,         ///< A data buffer write operation is pending or in progress.
        M_CMD_STATE_WRT_POSTAMBLE,    ///< A write postamble is pending or in progress.
        M_CMD_STATE_WRT_LAST,         ///< Last portion of the write command is pending or in progress.
        M_CMD_STATE_WRT_COMPLETE,     ///< The write command has complete.
        M_CMD_STATE_WRT_PAUSE,        ///< The write command has a pause.
        M_CMD_STATE_WRT_ERROR,        ///< An error occured while writing.
        M_CMD_STATE_READ_PREAMBLE,    ///< A write preamble is pending or in progress.
        M_CMD_STATE_READ_DATA,        ///< A data buffer read operation is pending or in progress.
        M_CMD_STATE_READ_POSTAMBLE,   ///< A read postamble is pending or in progress.
        M_CMD_STATE_READ_LAST,        ///< Last portion of the read command is pending or in progress.
        M_CMD_STATE_READ_COMPLETE,    ///< The read command has complete.
        M_CMD_STATE_READ_PAUSE,       ///< The read command has a pause.
        M_CMD_STATE_READ_ERROR,       ///< An error occured while reading.
    } cmd_state;
    
    drv_disp_engine_user_cfg_t        * p_user_cfg;
    drv_disp_engine_cfg_t       const * p_cfg;
    drv_disp_engine_access_descr_t      access_descr;
    uint16_t                            current_line_number;
} m_drv_disp_engine;


static uint32_t end_disp_engine_access(void)
{
    uint32_t result = DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED;
    
    if ( m_drv_disp_engine.access_descr.current_proc != DRV_DISP_ENGINE_PROC_NONE )
    {
        result = m_drv_disp_engine.p_user_cfg->access_end();
        
        m_drv_disp_engine.access_descr.current_proc = DRV_DISP_ENGINE_PROC_NONE;
    }
    
    return ( result );
}


static bool begin_disp_engine_access(drv_disp_engine_proc_type_t new_proc)
{    
    uint32_t result = DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED;
    
    if ( (m_drv_disp_engine.access_descr.current_proc == DRV_DISP_ENGINE_PROC_NONE)
    &&   (new_proc                                    != DRV_DISP_ENGINE_PROC_NONE) )
    {
        m_drv_disp_engine.cmd_state = M_CMD_STATE_IDLE;
        
        m_drv_disp_engine.access_descr.buffers.preamble_length  = 0;
        m_drv_disp_engine.access_descr.buffers.data_length      = 0;
        m_drv_disp_engine.access_descr.buffers.postamble_length = 0;

        result = m_drv_disp_engine.p_user_cfg->access_begin(&(m_drv_disp_engine.access_descr), new_proc);
        
        if ( result == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
        {
            m_drv_disp_engine.access_descr.current_proc = new_proc;
        }
    }
    
    return ( result == DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


uint32_t drv_disp_engine_init(drv_disp_engine_cfg_t const * const p_drv_disp_engine_cfg)
{
    m_drv_disp_engine.p_cfg      = p_drv_disp_engine_cfg;
    m_drv_disp_engine.p_user_cfg = NULL;
    
    m_drv_disp_engine.access_descr.current_proc = DRV_DISP_ENGINE_PROC_NONE;
    
    return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
}


uint32_t drv_disp_engine_proc_initiate(drv_disp_engine_user_cfg_t *p_user_cfg, drv_disp_engine_proc_type_t proc, drv_disp_engine_access_descr_t ** p_access_descr)
{
    if ( (p_user_cfg                == NULL)
    ||   (p_user_cfg->access_begin  == NULL)
    ||   (p_user_cfg->access_update == NULL)
    ||   (p_user_cfg->line_write    == NULL)
    ||   (p_user_cfg->line_read     == NULL)
    ||   (p_user_cfg->access_end    == NULL) )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_INVALID_PARAM );
    }
    
    if ( (proc == DRV_DISP_ENGINE_PROC_UPDATE)
    &&   (
            (m_drv_disp_engine.p_cfg                          == NULL)
      ||    (m_drv_disp_engine.p_cfg->fb_next_dirty_line_get  == NULL)
         ) )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_INVALID_PARAM );
    }

    if ( (  (proc == DRV_DISP_ENGINE_PROC_TOFBCPY)
      ||    (proc == DRV_DISP_ENGINE_PROC_FROMFBCPY)
         )
    &&   (
            (m_drv_disp_engine.p_cfg                          == NULL)
      ||    (m_drv_disp_engine.p_cfg->fb_line_storage_ptr_get == NULL)
      ||    (m_drv_disp_engine.p_cfg->fb_line_storage_set     == NULL)
         ) )
    {
        return ( DRV_DISP_ENGINE_STATUS_CODE_INVALID_PARAM );
    }
    
    m_drv_disp_engine.p_user_cfg = p_user_cfg;
    
    if ( begin_disp_engine_access(proc) )
    {
        m_drv_disp_engine.p_user_cfg = p_user_cfg;
        m_drv_disp_engine.access_descr.current_proc = proc;
        
        if ( p_access_descr != NULL )
        {
            *p_access_descr = &m_drv_disp_engine.access_descr;
        }
        
        return ( DRV_DISP_ENGINE_STATUS_CODE_SUCCESS );
    }

    return ( DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED );
}


static void proc_generic_write_cmd_active_handle(void)
{
    switch ( m_drv_disp_engine.cmd_state )
    {
        case M_CMD_STATE_WRT_PREAMBLE:
            m_drv_disp_engine.cmd_state = ( m_drv_disp_engine.access_descr.buffers.data_length != 0 ) ? M_CMD_STATE_WRT_DATA : M_CMD_STATE_WRT_LAST;
            
            if ( m_drv_disp_engine.p_user_cfg->line_write(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_PREAMBLE,
                                                          m_drv_disp_engine.current_line_number,
                                                          m_drv_disp_engine.access_descr.buffers.p_preamble,
                                                          m_drv_disp_engine.access_descr.buffers.preamble_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;
        case M_CMD_STATE_WRT_DATA:
            m_drv_disp_engine.cmd_state = ( m_drv_disp_engine.access_descr.buffers.postamble_length != 0 ) ? M_CMD_STATE_WRT_POSTAMBLE : M_CMD_STATE_WRT_LAST;

            if ( m_drv_disp_engine.p_user_cfg->line_write(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_DATA,
                                                          m_drv_disp_engine.current_line_number,
                                                          m_drv_disp_engine.access_descr.buffers.p_data,
                                                          m_drv_disp_engine.access_descr.buffers.data_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;
        case M_CMD_STATE_WRT_POSTAMBLE:
            m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_LAST;
            if ( m_drv_disp_engine.p_user_cfg->line_write(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_POSTAMBLE,
                                                          m_drv_disp_engine.current_line_number,
                                                          m_drv_disp_engine.access_descr.buffers.p_postamble,
                                                          m_drv_disp_engine.access_descr.buffers.postamble_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_ERROR;
            }
            
            break;    
            default:
                for ( ;; );
                // break;
    }
}


static void proc_generic_write_cmd_idle_last_pause_handle(void)
{
    switch ( m_drv_disp_engine.cmd_state )
    {
        case M_CMD_STATE_IDLE:
        case M_CMD_STATE_WRT_LAST:
        case M_CMD_STATE_WRT_PAUSE:
            if ( m_drv_disp_engine.p_user_cfg->access_update(&m_drv_disp_engine.access_descr) )
            {
                if ( m_drv_disp_engine.access_descr.buffers.preamble_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_PREAMBLE;
                    proc_generic_write_cmd_active_handle();
                }
                else if ( m_drv_disp_engine.access_descr.buffers.data_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_DATA;
                    proc_generic_write_cmd_active_handle();
                }
                else if ( m_drv_disp_engine.access_descr.buffers.postamble_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_POSTAMBLE;
                    proc_generic_write_cmd_active_handle();
                }
                else
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_PAUSE;
                }
            }
            else
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_WRT_COMPLETE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}

static void proc_generic_write_cmd_handle(void)
{
    if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_IDLE)
    ||   (m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_LAST) )
    {
        proc_generic_write_cmd_idle_last_pause_handle();
    }
    else
    {
        proc_generic_write_cmd_active_handle();
    }
}


static void proc_generic_read_cmd_active_handle(void)
{
    switch ( m_drv_disp_engine.cmd_state )
    {
        case M_CMD_STATE_READ_PREAMBLE:
            m_drv_disp_engine.cmd_state = ( m_drv_disp_engine.access_descr.buffers.data_length != 0 ) ? M_CMD_STATE_READ_DATA : M_CMD_STATE_READ_LAST;
            
            if ( m_drv_disp_engine.p_user_cfg->line_read(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_PREAMBLE,
                                                         m_drv_disp_engine.access_descr.buffers.p_preamble, 
                                                         m_drv_disp_engine.current_line_number,
                                                         m_drv_disp_engine.access_descr.buffers.preamble_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_ERROR;
            }
            
            break;
        case M_CMD_STATE_READ_DATA:
            m_drv_disp_engine.cmd_state = ( m_drv_disp_engine.access_descr.buffers.postamble_length != 0 ) ? M_CMD_STATE_READ_POSTAMBLE : M_CMD_STATE_READ_LAST;

            if ( m_drv_disp_engine.p_user_cfg->line_read(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_DATA,
                                                         m_drv_disp_engine.access_descr.buffers.p_data, m_drv_disp_engine.current_line_number,
                                                         m_drv_disp_engine.access_descr.buffers.data_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_ERROR;
            }
            
            break;
        case M_CMD_STATE_READ_POSTAMBLE:
            m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_LAST;
            if ( m_drv_disp_engine.p_user_cfg->line_read(DRV_DISP_ENGINE_PROC_ACCESS_TYPE_POSTAMBLE,
                                                         m_drv_disp_engine.access_descr.buffers.p_postamble,
                                                         m_drv_disp_engine.current_line_number,
                                                         m_drv_disp_engine.access_descr.buffers.postamble_length) != DRV_DISP_ENGINE_STATUS_CODE_SUCCESS )
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_ERROR;
            }
            
            break;    
        default:
            for ( ;; );
            // break;
    }
}


static void proc_generic_read_cmd_idle_last_pause_handle(void)
{
    switch ( m_drv_disp_engine.cmd_state )
    {
        case M_CMD_STATE_IDLE:
        case M_CMD_STATE_READ_LAST:
        case M_CMD_STATE_READ_PAUSE:
            if ( m_drv_disp_engine.p_user_cfg->access_update(&m_drv_disp_engine.access_descr) )
            {
                if ( m_drv_disp_engine.access_descr.buffers.preamble_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_PREAMBLE;
                    proc_generic_read_cmd_active_handle();
                }
                else if ( m_drv_disp_engine.access_descr.buffers.data_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_DATA;
                    proc_generic_read_cmd_active_handle();
                }
                else if ( m_drv_disp_engine.access_descr.buffers.postamble_length != 0 )
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_POSTAMBLE;
                    proc_generic_read_cmd_active_handle();
                }
                else
                {
                    m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_PAUSE;
                }
            }
            else
            {
                m_drv_disp_engine.cmd_state = M_CMD_STATE_READ_COMPLETE;
            }
            break;
        default:
            for ( ;; );
            // break;
    }
}


static void proc_generic_read_cmd_handle(void)
{
    if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_IDLE)
    ||   (m_drv_disp_engine.cmd_state == M_CMD_STATE_READ_LAST) )
    {
        proc_generic_read_cmd_idle_last_pause_handle();
    }
    else
    {
        proc_generic_read_cmd_active_handle();
    }
}


static void proc_clear_cmd_handle(void)
{
    m_drv_disp_engine.current_line_number = FB_INVALID_LINE;
    
    proc_generic_write_cmd_handle();
}


static void proc_update_cmd_handle(void)
{
    if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_IDLE)
    ||   (m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_LAST) )
    {
        m_drv_disp_engine.access_descr.buffers.data_length = 0;
        m_drv_disp_engine.current_line_number = m_drv_disp_engine.p_cfg->fb_next_dirty_line_get(&m_drv_disp_engine.access_descr.buffers.data_length, &m_drv_disp_engine.access_descr.buffers.p_data);
        
        proc_generic_write_cmd_idle_last_pause_handle();
    }
    else
    {
        proc_generic_write_cmd_handle();
    }
}

static void proc_vlcdtofb_cmd_handle(void)
{
    if ( m_drv_disp_engine.cmd_state == M_CMD_STATE_IDLE )
    {
        m_drv_disp_engine.current_line_number = 0;
        
        m_drv_disp_engine.access_descr.buffers.data_length = 0;
        m_drv_disp_engine.current_line_number =
            m_drv_disp_engine.p_cfg->fb_line_storage_ptr_get(m_drv_disp_engine.current_line_number, 
                                                             &m_drv_disp_engine.access_descr.buffers.data_length,
                                                             &m_drv_disp_engine.access_descr.buffers.p_data);
        
        proc_generic_read_cmd_idle_last_pause_handle();
    }
    else if ( m_drv_disp_engine.cmd_state == M_CMD_STATE_READ_LAST )
    {
        m_drv_disp_engine.p_cfg->fb_line_storage_set(m_drv_disp_engine.current_line_number, 
                                                    m_drv_disp_engine.access_descr.buffers.data_length,
                                                    m_drv_disp_engine.access_descr.buffers.p_data);
        
        m_drv_disp_engine.access_descr.buffers.data_length = 0;
        m_drv_disp_engine.current_line_number =
            m_drv_disp_engine.p_cfg->fb_line_storage_ptr_get(m_drv_disp_engine.current_line_number + 1,
                                                             &m_drv_disp_engine.access_descr.buffers.data_length,
                                                             &m_drv_disp_engine.access_descr.buffers.p_data);
        
        proc_generic_read_cmd_idle_last_pause_handle();
    }
    else
    {
        proc_generic_read_cmd_handle();
    }
}


static void proc_vlcdfromfb_cmd_handle(void)
{
    if ( m_drv_disp_engine.cmd_state == M_CMD_STATE_IDLE )
    {
        m_drv_disp_engine.current_line_number = 0;
        
        m_drv_disp_engine.access_descr.buffers.data_length = 0;
        m_drv_disp_engine.current_line_number =
            m_drv_disp_engine.p_cfg->fb_line_storage_ptr_get(m_drv_disp_engine.current_line_number, 
                                                             &m_drv_disp_engine.access_descr.buffers.data_length,
                                                             &m_drv_disp_engine.access_descr.buffers.p_data);
        
        proc_generic_write_cmd_idle_last_pause_handle();
    }
    else if ( m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_LAST )
    {
        m_drv_disp_engine.access_descr.buffers.data_length = 0;
        m_drv_disp_engine.current_line_number =
            m_drv_disp_engine.p_cfg->fb_line_storage_ptr_get(m_drv_disp_engine.current_line_number + 1,
                                                             &m_drv_disp_engine.access_descr.buffers.data_length,
                                                             &m_drv_disp_engine.access_descr.buffers.p_data);
        
        proc_generic_write_cmd_idle_last_pause_handle();
    }
    else
    {
        proc_generic_write_cmd_handle();
    }
}


uint32_t drv_disp_engine_proc_drive(drv_disp_engine_proc_drive_type_t drive_type, drv_disp_engine_proc_drive_info_t *drive_info)
{
    uint32_t    result  = DRV_DISP_ENGINE_STATUS_CODE_SUCCESS;
    bool        done    = (drive_type == DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK);

    do
    {
        switch ( m_drv_disp_engine.access_descr.current_proc )
        {
            case DRV_DISP_ENGINE_PROC_CLEAR:
                proc_clear_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_WRITE:
                proc_generic_write_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_READ:
                proc_generic_read_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_UPDATE:
                proc_update_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_TOFBCPY:
                proc_vlcdtofb_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_FROMFBCPY:
                proc_vlcdfromfb_cmd_handle();
                break;
            case DRV_DISP_ENGINE_PROC_NONE:
            default:
                for ( ;; );
                // break;
        }
        
        if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_COMPLETE)
        ||   (m_drv_disp_engine.cmd_state == M_CMD_STATE_READ_COMPLETE) )
        {
            if ( drive_info != NULL )
            {
                drive_info->proc_type   = m_drv_disp_engine.access_descr.current_proc;
                drive_info->exit_status = DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_COMPLETE;
            }
            result = end_disp_engine_access();
            done = true;
        }
        else if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_ERROR)
        ||        (m_drv_disp_engine.cmd_state == M_CMD_STATE_READ_ERROR) )
        {
            if ( drive_info != NULL )
            {
                drive_info->proc_type   = m_drv_disp_engine.access_descr.current_proc;
                drive_info->exit_status = DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_ERROR;
            }
            (void)end_disp_engine_access();
            result = DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED;
            done = true;
        }
        else if ( (m_drv_disp_engine.cmd_state == M_CMD_STATE_WRT_PAUSE)
        ||        (m_drv_disp_engine.cmd_state == M_CMD_STATE_READ_PAUSE) )
        {
            if ( drive_info != NULL )
            {
                drive_info->proc_type   = m_drv_disp_engine.access_descr.current_proc;
                drive_info->exit_status = DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_PAUSE;
            }

            done = true;
        }
    } while ( !done );
    
    return ( result );
}
