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
#include "pca63520_util.h"

#include "drv_vlcd.h"
#include "drv_mlcd.h"
#include "drv_pca63520_io.h"
#include "nrf_drv_ppi.h"

#include "nrf.h"

#define M_CONFIGURE_MODE_SETUP      ( 1)
#define M_CONFIGURE_MODE_TEARDOWN   (-1)

static struct
{
    nrf_ppi_channel_t   ch_clk_edge_count; ///< The PPI channel triggering the falling edge counter.
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
    nrf_ppi_channel_t   ch_edge_count;     ///< The PPI channel triggering the DCX signal counter.
    nrf_ppi_channel_t   ch_pin_set;        ///< The PPI channel setting the DCX bit.
    nrf_ppi_channel_t   ch_pin_clr;        ///< The PPI channel clearing the DCX bit.
#endif
    nrf_ppi_channel_t   ch_clk_disable;    ///< The PPI channel disabling the clock.
    uint32_t            all_ppi_channels;  ///< All PPI channels represented as a mask.
    
    struct
    {
        volatile uint32_t   current_total_cycles;
        volatile bool       done;
    } sync_status;
    
    pca63520_util_cfg_t const * p_cfg;
} m_pca63520_util =
{
    .sync_status.done = true,
};

static void spi_clock_counter_setup(uint32_t total_cycles)
{
    m_pca63520_util.sync_status.current_total_cycles = total_cycles;
    
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
    nrf_drv_timer_enable(m_pca63520_util.p_cfg->dcx.counter.p_instance);
    nrf_drv_timer_clear(m_pca63520_util.p_cfg->dcx.counter.p_instance);
    if ( m_pca63520_util.p_cfg->dcx.initial_latency_cycles > 0 )
    {
        for ( uint16_t i = 0; i < (0x10000 - m_pca63520_util.p_cfg->dcx.initial_latency_cycles); i++ )
        {
            nrf_drv_timer_increment(m_pca63520_util.p_cfg->dcx.counter.p_instance);
        }
    }
    nrf_drv_timer_extended_compare(m_pca63520_util.p_cfg->dcx.counter.p_instance, NRF_TIMER_CC_CHANNEL0, 9, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);
    nrf_drv_timer_compare(m_pca63520_util.p_cfg->dcx.counter.p_instance, NRF_TIMER_CC_CHANNEL1, 8, false);
    nrf_drv_gpiote_out_task_enable(m_pca63520_util.p_cfg->dcx.pin.id);
#endif
    
    nrf_drv_timer_enable(m_pca63520_util.p_cfg->sense.counter.p_instance);
    nrf_drv_timer_clear(m_pca63520_util.p_cfg->sense.counter.p_instance);    
    nrf_drv_timer_compare(m_pca63520_util.p_cfg->sense.counter.p_instance,
                          NRF_TIMER_CC_CHANNEL0,
                          m_pca63520_util.sync_status.current_total_cycles & 0xFFFF,
                          true);
    m_pca63520_util.sync_status.current_total_cycles -= (m_pca63520_util.sync_status.current_total_cycles & 0xFFFF);

    nrf_drv_gpiote_in_event_enable(m_pca63520_util.p_cfg->sense.pin.id, false);
    nrf_drv_gpiote_out_task_enable(m_pca63520_util.p_cfg->disable.pin.id);
    
    if ( m_pca63520_util.sync_status.current_total_cycles == 0 )
    {
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_clk_edge_count);
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_edge_count);
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_pin_set);
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_pin_clr);
#endif
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_clk_disable);
    }
    else
    {
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_clk_edge_count);
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED        
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_edge_count);
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_pin_set);
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_pin_clr);
#endif
    }
    
    nrf_drv_gpiote_out_task_trigger(m_pca63520_util.p_cfg->disable.pin.id);
}

void nrf_timer_empty_event_handler(nrf_timer_event_t event_type, void * p_context)
{
    for ( ;; );
}


void nrf_timer_event_handler(nrf_timer_event_t event_type, void * p_context)
{
    if ( m_pca63520_util.sync_status.current_total_cycles == 0x10000 )
    {
        nrf_drv_ppi_channel_enable(m_pca63520_util.ch_clk_disable);
    }
    else if ( m_pca63520_util.sync_status.current_total_cycles == 0 )
    {
        nrf_ppi_channels_disable(m_pca63520_util.all_ppi_channels);
        
        nrf_drv_timer_disable(m_pca63520_util.p_cfg->sense.counter.p_instance);
        
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
        nrf_drv_timer_disable(m_pca63520_util.p_cfg->dcx.counter.p_instance);
#endif
        
        drv_pca63520_io_spi_clk_mode_cfg(DRV_PCA63520_IO_SPI_CLK_MODE_DISABLED);
        drv_mlcd_input_mode_set(DRV_MLCD_INPUT_MODE_DISABLED);
        drv_pca63520_io_disp_spi_si_mode_cfg(DRV_PCA63520_IO_DISP_SPI_SI_MODE_NORMAL);
        drv_vlcd_output_mode_set(DRV_VLCD_OUTPUT_MODE_DISABLED);
        m_pca63520_util.sync_status.done = true;
    }
    
    m_pca63520_util.sync_status.current_total_cycles -= 0x10000;
}


static bool m_setup_teardown(int8_t mode)
{
    static const int8_t LAST_INDEX = 14;
    uint32_t    ret_val;
    int8_t      i;
    int8_t      delta_i = mode;
    
    if ( mode == M_CONFIGURE_MODE_SETUP )
    {
        m_pca63520_util.all_ppi_channels = 0;
        i = 0;
    }
    else if ( mode == M_CONFIGURE_MODE_TEARDOWN )
    {
        i = LAST_INDEX;
    }
    else
    {
        return ( false );
    }
    
    while ( i >= 0 )
    {
        switch ( i )
        {
            case 0:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_alloc(&m_pca63520_util.ch_clk_edge_count);
                    m_pca63520_util.all_ppi_channels |= (1UL << m_pca63520_util.ch_clk_edge_count);
                }
                else
                {
                    nrf_drv_ppi_channel_free(m_pca63520_util.ch_clk_edge_count);
                }
                break;
            case 1:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_alloc(&m_pca63520_util.ch_edge_count);
                    m_pca63520_util.all_ppi_channels |= (1UL << m_pca63520_util.ch_edge_count);
                }
                else
                {
                    nrf_drv_ppi_channel_free(m_pca63520_util.ch_edge_count);
                }
#endif
                break;
            case 2:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_alloc(&m_pca63520_util.ch_pin_set);
                    m_pca63520_util.all_ppi_channels |= (1UL << m_pca63520_util.ch_pin_set);
                }
                else
                {
                    nrf_drv_ppi_channel_free(m_pca63520_util.ch_pin_set);
                }
#endif
                break;
            case 3:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_alloc(&m_pca63520_util.ch_pin_clr);
                    m_pca63520_util.all_ppi_channels |= (1UL << m_pca63520_util.ch_pin_clr);
                }
                else
                {
                    nrf_drv_ppi_channel_free(m_pca63520_util.ch_pin_clr);
                }
#endif
                break;
            case 4:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_alloc(&m_pca63520_util.ch_clk_disable);
                    m_pca63520_util.all_ppi_channels |= (1UL <<m_pca63520_util.ch_clk_disable);
                }
                else
                {
                    nrf_drv_ppi_channel_free(m_pca63520_util.ch_clk_disable);
                }
                break;
            case 5:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_timer_init(m_pca63520_util.p_cfg->dcx.counter.p_instance,
                                                 m_pca63520_util.p_cfg->dcx.counter.p_config,
                                                 nrf_timer_empty_event_handler);
                }
                else
                {
                    nrf_drv_timer_uninit(m_pca63520_util.p_cfg->dcx.counter.p_instance);
                }
#endif
                break;
            case 6:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_timer_init(m_pca63520_util.p_cfg->sense.counter.p_instance,
                                                 m_pca63520_util.p_cfg->sense.counter.p_config,
                                                 nrf_timer_event_handler);
                }
                else
                {
                    nrf_drv_timer_uninit(m_pca63520_util.p_cfg->sense.counter.p_instance);
                }
                break;
            case 7:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_gpiote_in_init(m_pca63520_util.p_cfg->sense.pin.id, m_pca63520_util.p_cfg->sense.pin.p_config, NULL);
                }
                else
                {
                    nrf_drv_gpiote_in_uninit(m_pca63520_util.p_cfg->sense.pin.id);
                }
                break;
            case 8:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_gpiote_out_init(m_pca63520_util.p_cfg->dcx.pin.id, m_pca63520_util.p_cfg->dcx.pin.p_config);
                }
                else
                {
                    nrf_drv_gpiote_out_uninit(m_pca63520_util.p_cfg->dcx.pin.id);
                }
#endif
                break;
            case 9:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_gpiote_out_init(m_pca63520_util.p_cfg->disable.pin.id, m_pca63520_util.p_cfg->disable.pin.p_config);
                }
                else
                {
                    nrf_drv_gpiote_out_uninit(m_pca63520_util.p_cfg->disable.pin.id);
                }
                break;
            case 10:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_assign(m_pca63520_util.ch_clk_edge_count,
                                                         nrf_drv_gpiote_in_event_addr_get(m_pca63520_util.p_cfg->sense.pin.id),
                                                         nrf_drv_timer_task_address_get(m_pca63520_util.p_cfg->sense.counter.p_instance,
                                                         NRF_TIMER_TASK_COUNT));
                }
//                else
//                {
//                    // There is no un-assign function, so do nothing.
//                }
                break;
            case 11:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_assign(m_pca63520_util.ch_edge_count,
                                                         nrf_drv_gpiote_in_event_addr_get(m_pca63520_util.p_cfg->sense.pin.id),
                                                         nrf_drv_timer_task_address_get(m_pca63520_util.p_cfg->dcx.counter.p_instance, NRF_TIMER_TASK_COUNT));
                }
//                else
//                {
//                    // There is no un-assign function, so do nothing.
//                }
#endif
                break;
            case 12:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                    if ( delta_i == M_CONFIGURE_MODE_SETUP )
                    {
                        ret_val = nrf_drv_ppi_channel_assign(m_pca63520_util.ch_pin_set,
                                                             nrf_drv_timer_compare_event_address_get(m_pca63520_util.p_cfg->dcx.counter.p_instance, 1),
                                                             nrf_drv_gpiote_out_task_addr_get(m_pca63520_util.p_cfg->dcx.pin.id));
                    }
    //                else
    //                {
    //                    // There is no un-assign function, so do nothing.
    //                }
#endif
                break;
            case 13:
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
                    if ( delta_i == M_CONFIGURE_MODE_SETUP )
                    {
                        ret_val = nrf_drv_ppi_channel_assign(m_pca63520_util.ch_pin_clr,
                                                             nrf_drv_timer_compare_event_address_get(m_pca63520_util.p_cfg->dcx.counter.p_instance, 0),
                                                             nrf_drv_gpiote_out_task_addr_get(m_pca63520_util.p_cfg->dcx.pin.id));
                    }
    //                else
    //                {
    //                    // There is no un-assign function, so do nothing.
    //                }
#endif
                break;
            case LAST_INDEX:
                if ( delta_i == M_CONFIGURE_MODE_SETUP )
                {
                    ret_val = nrf_drv_ppi_channel_assign(m_pca63520_util.ch_clk_disable,
                                                         nrf_drv_timer_compare_event_address_get(m_pca63520_util.p_cfg->sense.counter.p_instance, 0),
                                                         nrf_drv_gpiote_out_task_addr_get(m_pca63520_util.p_cfg->disable.pin.id));
                }
//                else
//                {
//                    // There is no un-assign function, so do nothing.
//                }
                break;
            default:
                return ( true );

        }
        
        if ( ret_val != NRF_SUCCESS )
        {
            m_pca63520_util.all_ppi_channels = 0;
            delta_i = -1;
        }
        
        i += delta_i;
    }
    
    return ( mode == M_CONFIGURE_MODE_TEARDOWN );
}


uint32_t pca63520_util_vlcd_mlcd_sync_setup(pca63520_util_cfg_t const * p_pca63520_util_cfg)
{
    m_pca63520_util.p_cfg = p_pca63520_util_cfg;
    
    if ( m_setup_teardown(M_CONFIGURE_MODE_SETUP) )
    {
        return ( PCA63520_UTIL_STATUS_CODE_SUCCESS );
    }
    
    return ( PCA63520_UTIL_STATUS_CODE_DISALLOWED );
}


uint32_t pca63520_util_vlcd_mlcd_sync(void)
{
    if ( m_pca63520_util.sync_status.done )
    {
        m_pca63520_util.sync_status.done = false;
        
        drv_pca63520_io_spi_clk_mode_cfg(DRV_PCA63520_IO_SPI_CLK_MODE_ENABLED);
        drv_vlcd_output_mode_set(DRV_VLCD_OUTPUT_MODE_DIRECT_SPI);
        drv_pca63520_io_disp_spi_si_mode_cfg(DRV_PCA63520_IO_DISP_SPI_SI_MODE_RAM);
        drv_mlcd_input_mode_set(DRV_MLCD_INPUT_MODE_DIRECT_SPI);
        spi_clock_counter_setup(drv_vlcd_storage_size_get() * 8);
        
        return ( PCA63520_UTIL_STATUS_CODE_SUCCESS );
    }
    
    return ( PCA63520_UTIL_STATUS_CODE_DISALLOWED );
}


bool pca63520_util_vlcd_mlcd_sync_active(void)
{
    return ( !m_pca63520_util.sync_status.done );
}


uint32_t pca63520_util_vlcd_mlcd_sync_teardown(void)
{
    if ( (m_pca63520_util.p_cfg != NULL)
    &&   (m_pca63520_util.sync_status.done) )
    {
        if ( m_setup_teardown(M_CONFIGURE_MODE_TEARDOWN) )
        {
            m_pca63520_util.p_cfg = NULL;
            
            return ( PCA63520_UTIL_STATUS_CODE_SUCCESS );
        }
    }
    
    return ( PCA63520_UTIL_STATUS_CODE_DISALLOWED );
}

