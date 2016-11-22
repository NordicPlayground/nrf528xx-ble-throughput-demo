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
#include "drv_23lcv.h"
#include "nrf_gpio.h"

#include <stdlib.h>

#define READ  0x03 // Read data from memory array beginning at selected address
#define WRITE 0x02 // Write data to memory array beginning at selected address
//#define EDIO  0x3B // Enter Dual I/O access
//#define RSTIO 0xFF // Reset Dual I/O access 
//#define RDMR  0x05 // Read Mode Register
//#define WRMR  0x01 // Write Mode Register


typedef enum
{
    M_ACCESS_TYPE_NONE = 0,   ///< No kind of access.
    M_ACCESS_TYPE_SINGLE,     ///< The buffer to write is the only buffer to write to the memory.
    M_ACCESS_TYPE_START,      ///< The buffer to write is the first buffer to write to the memory.
    M_ACCESS_TYPE_CONTINUE,   ///< The buffer to write is a consecutive buffer (and not the last one) to write to the memory.
    M_ACCESS_TYPE_LAST,       ///< The buffer to write is the last buffer to write to the memory.
} m_access_type_t;


static struct
{
    enum
    {
        CMD_STATE_IDLE = 0,
        CMD_STATE_INIT_WRITE,
        CMD_STATE_INIT_READ,
        CMD_STATE_WRITE_DATA,
        CMD_STATE_READ_DATA,
        CMD_STATE_WRITE_DONE,
        CMD_STATE_READ_DONE,
    } cmd_state;
    enum
    {
        TRX_STATUS_NONE = 0,
        TRX_STATUS_SINGLE_PENDING,
        TRX_STATUS_START_PENDING,
        TRX_STATUS_CONTINUE_PENDING,
        TRX_STATUS_LAST_PENDING,
    } trx_status;
    struct
    {
        uint8_t *   p_data;
        uint8_t     header[3];
        uint8_t     data_length;
    } buffers;
    drv_23lcv_sig_callback_t  current_sig_callback;
    drv_23lcv_cfg_t const * p_cfg;
} m_drv_23lcv;


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event);


static void begin_23lcv_access(void)
{
    nrf_gpio_pin_clear(m_drv_23lcv.p_cfg->spi.ss_pin);
}


static void end_23lcv_access(void)
{
    nrf_gpio_pin_set(m_drv_23lcv.p_cfg->spi.ss_pin);
}


static uint8_t bitorder_swap(uint8_t value)
{
   uint8_t b = value;
    
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}


static void cmd_handler(void)
{
    bool done = (m_drv_23lcv.current_sig_callback != NULL);

    do
    {
        switch ( m_drv_23lcv.cmd_state )
        {
            case CMD_STATE_INIT_WRITE:
            case CMD_STATE_INIT_READ:    
                if ( m_drv_23lcv.buffers.data_length != 0 )
                {
                    m_drv_23lcv.cmd_state = (m_drv_23lcv.cmd_state == CMD_STATE_INIT_WRITE) ? CMD_STATE_WRITE_DATA : CMD_STATE_READ_DATA;
                }
                else
                {
                    m_drv_23lcv.cmd_state = (m_drv_23lcv.cmd_state == CMD_STATE_INIT_WRITE) ? CMD_STATE_WRITE_DONE : CMD_STATE_READ_DONE;
                }
                (void)nrf_drv_spi_transfer(m_drv_23lcv.p_cfg->spi.p_instance, &(m_drv_23lcv.buffers.header[0]), sizeof(m_drv_23lcv.buffers.header), NULL, 0);
                break;
            case CMD_STATE_WRITE_DATA:
                m_drv_23lcv.cmd_state = CMD_STATE_WRITE_DONE;
                (void)nrf_drv_spi_transfer(m_drv_23lcv.p_cfg->spi.p_instance, &(m_drv_23lcv.buffers.p_data[0]), m_drv_23lcv.buffers.data_length, NULL, 0);
                break;
            case CMD_STATE_READ_DATA:
                m_drv_23lcv.cmd_state = CMD_STATE_READ_DONE;
                (void)nrf_drv_spi_transfer(m_drv_23lcv.p_cfg->spi.p_instance, NULL, 0, &(m_drv_23lcv.buffers.p_data[0]), m_drv_23lcv.buffers.data_length);
                break;
            case CMD_STATE_WRITE_DONE:
            case CMD_STATE_READ_DONE:
                m_drv_23lcv.cmd_state = CMD_STATE_IDLE;
                if ( (m_drv_23lcv.trx_status == TRX_STATUS_SINGLE_PENDING) 
                ||   (m_drv_23lcv.trx_status == TRX_STATUS_LAST_PENDING) )
                {
                    end_23lcv_access();
                    m_drv_23lcv.trx_status = TRX_STATUS_NONE;
                    if ( m_drv_23lcv.current_sig_callback != NULL )
                    {
                        m_drv_23lcv.current_sig_callback(
                            (m_drv_23lcv.cmd_state == CMD_STATE_WRITE_DONE) ? DRV_DRV_23LCV_SIGNAL_TYPE_WRITE_COMPLETE : DRV_DRV_23LCV_SIGNAL_TYPE_READ_COMPLETE );
                    }
                }
                else if ( (m_drv_23lcv.trx_status == TRX_STATUS_START_PENDING) 
                ||        (m_drv_23lcv.trx_status == TRX_STATUS_CONTINUE_PENDING) )
                {
                    if ( m_drv_23lcv.current_sig_callback != NULL )
                    {
                        m_drv_23lcv.current_sig_callback(
                            (m_drv_23lcv.cmd_state == CMD_STATE_WRITE_DONE) ? DRV_DRV_23LCV_SIGNAL_TYPE_WRITE_PAUSED : DRV_DRV_23LCV_SIGNAL_TYPE_READ_PAUSED );
                    }
                }
              //else
              //{
              //    ASSERT( false );
              //}
                done = true;
                break;
            case CMD_STATE_IDLE:
            default:
                // ASSERT( false );
                break;
        }
    } while ( !done );
}


static void nrf_drv_spi_handler(nrf_drv_spi_evt_t const * event)
{
    if ( event->type == NRF_DRV_SPI_EVENT_DONE )
    {
        cmd_handler();
    }
//    else
//    {
//        for ( ;; );
//    }
}


static void drv_23lcv_access(m_access_type_t write_req_type, uint8_t cmd, uint16_t start_addr, uint8_t buffer_length, uint8_t *p_buffer)
{
    if ( (write_req_type == M_ACCESS_TYPE_SINGLE)
    ||   (write_req_type == M_ACCESS_TYPE_START) )
    {
        begin_23lcv_access();
        
        m_drv_23lcv.cmd_state = (cmd == WRITE) ? CMD_STATE_INIT_WRITE : CMD_STATE_INIT_READ;
        m_drv_23lcv.trx_status = (write_req_type == M_ACCESS_TYPE_SINGLE) ?  TRX_STATUS_SINGLE_PENDING : TRX_STATUS_START_PENDING;
        
        if ( m_drv_23lcv.p_cfg->spi.p_config->bit_order == NRF_DRV_SPI_BIT_ORDER_MSB_FIRST )
        {
            m_drv_23lcv.buffers.header[0] = cmd;
            m_drv_23lcv.buffers.header[1] = (uint8_t)(start_addr >>  8);
            m_drv_23lcv.buffers.header[2] = (uint8_t)(start_addr);
        }
        else
        {
            m_drv_23lcv.buffers.header[0] = bitorder_swap(cmd);
            m_drv_23lcv.buffers.header[1] = bitorder_swap((uint8_t)(start_addr >>  8));
            m_drv_23lcv.buffers.header[2] = bitorder_swap((uint8_t)(start_addr));
        }
    }
    else 
    {
        if ( m_drv_23lcv.buffers.data_length != 0 )
        {
            m_drv_23lcv.cmd_state = (cmd == WRITE) ? CMD_STATE_WRITE_DATA : CMD_STATE_READ_DATA;
        }
        else
        {
            m_drv_23lcv.cmd_state = (cmd == WRITE) ? CMD_STATE_WRITE_DONE : CMD_STATE_READ_DONE;
        }
        m_drv_23lcv.trx_status = (write_req_type == M_ACCESS_TYPE_CONTINUE) ?  TRX_STATUS_CONTINUE_PENDING : TRX_STATUS_LAST_PENDING;
    }
    
    m_drv_23lcv.buffers.p_data      = p_buffer;
    m_drv_23lcv.buffers.data_length = buffer_length;
    
    cmd_handler();
}


static uint32_t m_access_prepare(m_access_type_t *p_access_type, uint8_t * p_buf, uint16_t addr, int16_t *p_size)
{
    if ( m_drv_23lcv.p_cfg == NULL )
    {
        return ( DRV_23LCV_STATUS_CODE_DISALLOWED );
    }
    
    if ( (*p_size != 0) && (p_buf == NULL) )
    {
        return ( DRV_23LCV_STATUS_CODE_INVALID_PARAM );
    }
    
    if ( m_drv_23lcv.trx_status == TRX_STATUS_NONE )
    {
        if ( *p_size <= 0 )
        {
            *p_size        = -*p_size;
            *p_access_type = M_ACCESS_TYPE_START;
        }
        else
        {
            *p_access_type = M_ACCESS_TYPE_SINGLE;
        }
    }
    else if ( (m_drv_23lcv.trx_status == TRX_STATUS_START_PENDING)
    ||        (m_drv_23lcv.trx_status == TRX_STATUS_CONTINUE_PENDING) )
    {
        if ( *p_size >= 0 )
        {
            *p_access_type = M_ACCESS_TYPE_LAST;
        }
        else
        {
            *p_size    = -*p_size;
            *p_access_type = M_ACCESS_TYPE_CONTINUE;
        }
    }
    else
    {
        return ( DRV_23LCV_STATUS_CODE_DISALLOWED );
    }
    
    return ( DRV_23LCV_STATUS_CODE_SUCCESS );
}


static void m_reset_status(void)
{
    m_drv_23lcv.cmd_state     = CMD_STATE_IDLE;
    m_drv_23lcv.trx_status    = TRX_STATUS_NONE;
}


void drv_23lcv_init(void)
{
    m_reset_status();
}


uint32_t drv_23lcv_open(drv_23lcv_cfg_t const * const p_drv_23lcv_cfg)
{
    if ( (m_drv_23lcv.p_cfg == NULL)
    &&   (p_drv_23lcv_cfg   != NULL)
    &&   (nrf_drv_spi_init(p_drv_23lcv_cfg->spi.p_instance,
          p_drv_23lcv_cfg->spi.p_config,
          (m_drv_23lcv.current_sig_callback != NULL) ? nrf_drv_spi_handler : NULL ) == NRF_SUCCESS) )
    {
        m_drv_23lcv.p_cfg = p_drv_23lcv_cfg;

        nrf_gpio_pin_set(m_drv_23lcv.p_cfg->spi.ss_pin);
        nrf_gpio_pin_dir_set(m_drv_23lcv.p_cfg->spi.ss_pin, NRF_GPIO_PIN_DIR_OUTPUT);
        
        m_reset_status();

        return ( DRV_23LCV_STATUS_CODE_SUCCESS );
    }

    return ( DRV_23LCV_STATUS_CODE_DISALLOWED );

}


void drv_23lcv_callback_set(drv_23lcv_sig_callback_t drv_23lcv_sig_callback)
{
    if ( m_drv_23lcv.p_cfg == NULL )
    {
        m_drv_23lcv.current_sig_callback = drv_23lcv_sig_callback;
    }
    else
    {
        nrf_drv_spi_uninit(m_drv_23lcv.p_cfg->spi.p_instance);
        m_drv_23lcv.current_sig_callback = drv_23lcv_sig_callback;
        (void)nrf_drv_spi_init(m_drv_23lcv.p_cfg->spi.p_instance,
                               m_drv_23lcv.p_cfg->spi.p_config,
                               (m_drv_23lcv.current_sig_callback != NULL) ? nrf_drv_spi_handler : NULL);
    }
}


uint32_t drv_23lcv_write(uint16_t dest_addr, uint8_t * p_src, int16_t size)
{
    int16_t         size_mod    = size;
    uint32_t        result;
    m_access_type_t access_type;
    
    if ( (result = m_access_prepare(&access_type, p_src, dest_addr, &size_mod)) != DRV_23LCV_STATUS_CODE_SUCCESS )
    {
        return ( result );
    }
    
    if  ( (((access_type == M_ACCESS_TYPE_SINGLE) ||
            (access_type == M_ACCESS_TYPE_START)) 
    &&     (dest_addr != DRV_23LCV_NO_ADDR))
    ||    (((access_type == M_ACCESS_TYPE_CONTINUE) ||
            (access_type == M_ACCESS_TYPE_LAST)) 
    &&     (dest_addr == DRV_23LCV_NO_ADDR)) )
    {
        drv_23lcv_access(access_type, WRITE, dest_addr, size_mod, p_src);
        
        return ( DRV_23LCV_STATUS_CODE_SUCCESS );
    }
    else
    {
        return ( DRV_23LCV_STATUS_CODE_INVALID_PARAM );
    }
}


uint32_t drv_23lcv_read(uint8_t * p_dest, uint16_t src_addr, int16_t size)
{
    int16_t         size_mod    = size;
    uint32_t        result;
    m_access_type_t access_type;
    
    if ( (result = m_access_prepare(&access_type, p_dest, src_addr, &size_mod)) != DRV_23LCV_STATUS_CODE_SUCCESS )
    {
        return ( result );
    }
    
    if  ( (((access_type == M_ACCESS_TYPE_SINGLE) ||
            (access_type == M_ACCESS_TYPE_START)) 
    &&     (src_addr != DRV_23LCV_NO_ADDR))
    ||    (((access_type == M_ACCESS_TYPE_CONTINUE) ||
            (access_type == M_ACCESS_TYPE_LAST)) 
    &&     (src_addr == DRV_23LCV_NO_ADDR)) )
    {
        drv_23lcv_access(access_type, READ, src_addr, size, p_dest);
        
        return ( DRV_23LCV_STATUS_CODE_SUCCESS );
    }
    else
    {
        return ( DRV_23LCV_STATUS_CODE_INVALID_PARAM );
    }
}


uint32_t drv_23lcv_close(void)
{
    if ( m_drv_23lcv.p_cfg != NULL )
    {
        nrf_drv_spi_uninit(m_drv_23lcv.p_cfg->spi.p_instance);
        m_drv_23lcv.p_cfg = NULL;

        return ( DRV_23LCV_STATUS_CODE_SUCCESS );
    }

    return ( DRV_23LCV_STATUS_CODE_DISALLOWED );
}

