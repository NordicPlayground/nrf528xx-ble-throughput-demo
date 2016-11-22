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
#include "hal_timer.h"
#include "nrf_gpio.h"

#include "nrf.h"

#ifdef USE_SOFTDEVICE_API
#include "nrf_sdm.h"
#endif

/*
 * Begin - Temporary copy of the SDK gpiote header file. 
 */

 /**
 * @enum nrf_gpiote_outinit_t
 * @brief Initial output value for GPIOTE channel enumerator.
 */
typedef enum
{
  NRF_GPIOTE_INITIAL_VALUE_LOW  = GPIOTE_CONFIG_OUTINIT_Low,       ///<  Low to high
  NRF_GPIOTE_INITIAL_VALUE_HIGH = GPIOTE_CONFIG_OUTINIT_High       ///<  High to low
} nrf_gpiote_outinit_t;

 /**
 * @enum nrf_gpiote_polarity_t
 * @brief Polarity for GPIOTE channel enumerator.
 */
typedef enum
{
  NRF_GPIOTE_POLARITY_LOTOHI = GPIOTE_CONFIG_POLARITY_LoToHi,       ///<  Low to high
  NRF_GPIOTE_POLARITY_HITOLO = GPIOTE_CONFIG_POLARITY_HiToLo,       ///<  High to low
  NRF_GPIOTE_POLARITY_TOGGLE = GPIOTE_CONFIG_POLARITY_Toggle        ///<  Toggle
} nrf_gpiote_polarity_t;


/**
 * @brief Function for configuring GPIOTE channel as output, setting the properly desired output level.
 *
 *
 * @param channel_number specifies the GPIOTE channel [0:3] to configure as an output channel.
 * @param pin_number specifies the pin number [0:30] to use in the GPIOTE channel.
 * @param polarity specifies the desired polarity in the output GPIOTE channel.
 * @param initial_value specifies the initial value of the GPIOTE channel input after the channel configuration.
 */
static __INLINE void nrf_gpiote_task_config(uint32_t channel_number, uint32_t pin_number, nrf_gpiote_polarity_t polarity, nrf_gpiote_outinit_t initial_value)
{
    /* Check if the output desired is high or low */
    if (initial_value == NRF_GPIOTE_INITIAL_VALUE_LOW)
    {
        /* Workaround for the OUTINIT PAN. When nrf_gpiote_task_config() is called a glitch happens
        on the GPIO if the GPIO in question is already assigned to GPIOTE and the pin is in the 
        correct state in GPIOTE but not in the OUT register. */

		NRF_GPIO->OUTCLR = (1 << pin_number);

        
        /* Configure channel to Pin31, not connected to the pin, and configure as a tasks that will set it to proper level */
        NRF_GPIOTE->CONFIG[channel_number] = (GPIOTE_CONFIG_MODE_Task       << GPIOTE_CONFIG_MODE_Pos)     |
                                             (31UL                          << GPIOTE_CONFIG_PSEL_Pos)     |
                                             (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);                                    
    } 
    else 
    {
        /* Workaround for the OUTINIT PAN. When nrf_gpiote_task_config() is called a glitch happens
        on the GPIO if the GPIO in question is already assigned to GPIOTE and the pin is in the 
        correct state in GPIOTE but not in the OUT register. */

		NRF_GPIO->OUTSET = (1 << pin_number);

        /* Configure channel to Pin31, not connected to the pin, and configure as a tasks that will set it to proper level */
        NRF_GPIOTE->CONFIG[channel_number] = (GPIOTE_CONFIG_MODE_Task       << GPIOTE_CONFIG_MODE_Pos)     |
                                             (31UL                          << GPIOTE_CONFIG_PSEL_Pos)     |
                                             (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);
    }

    /* Three NOPs are required to make sure configuration is written before setting tasks or getting events */
    __NOP();
    __NOP();
    __NOP(); 

    /* Launch the task to take the GPIOTE channel output to the desired level */
    NRF_GPIOTE->TASKS_OUT[channel_number] = 1;
    

    /* Finally configure the channel as the caller expects. If OUTINIT works, the channel is configured properly. 
       If it does not, the channel output inheritance sets the proper level. */
    NRF_GPIOTE->CONFIG[channel_number] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)     |
                                         ((uint32_t)pin_number    << GPIOTE_CONFIG_PSEL_Pos)     |
                                         ((uint32_t)polarity      << GPIOTE_CONFIG_POLARITY_Pos) |
                                         ((uint32_t)initial_value << GPIOTE_CONFIG_OUTINIT_Pos);

    /* Three NOPs are required to make sure configuration is written before setting tasks or getting events */
    __NOP();
    __NOP();
    __NOP(); 
}

/**
 * @brief Function for configuring GPIOTE channel as input, automatically clearing an event that appears in some cases under configuration.
 *
 * Note that when configuring the channel as input an event might be triggered. Care of disabling interrupts
 * for that channel is left to the user.
 *
 * @param channel_number specifies the GPIOTE channel [0:3] to configure as an input channel.
 * @param pin_number specifies the pin number [0:30] to use in the GPIOTE channel.
 * @param polarity specifies the desired polarity in the output GPIOTE channel.
 */
static __INLINE void nrf_gpiote_event_config(uint32_t channel_number, uint32_t pin_number, nrf_gpiote_polarity_t polarity)
{   
    /* Configure the channel as the caller expects */
    NRF_GPIOTE->CONFIG[channel_number] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)     |
                                         ((uint32_t)pin_number     << GPIOTE_CONFIG_PSEL_Pos)     |
                                         ((uint32_t)polarity       << GPIOTE_CONFIG_POLARITY_Pos);

    /* Three NOPs are required to make sure configuration is written before setting tasks or getting events */
    __NOP();
    __NOP();
    __NOP();
    
    /* Clear the event that appears in some cases */
    NRF_GPIOTE->EVENTS_IN[channel_number] = 0; 
}

/*
 * End - Temporary copy of the SDK gpiote header file. 
 */



#define _CHANNEL_NUMBER_TO_CHANNEL_BIT(channel_number) (PPI_CHEN_CH ## channel_number ## _Enabled << PPI_CHEN_CH ## channel_number ## _Pos)
#define CHANNEL_NUMBER_TO_CHANNEL_BIT(channel_number) _CHANNEL_NUMBER_TO_CHANNEL_BIT(channel_number)

#ifndef GPIOTE_SENSE_PIN
#ifdef NRF51
#define GPIOTE_SENSE_PIN                15
#endif
#ifdef NRF52832
#define GPIOTE_SENSE_PIN                14
#endif
#ifdef NRF52840_XXAA
#define GPIOTE_SENSE_PIN                (32+4)
#endif
#endif
#ifndef GPIOTE_SENSE_INSTANCE
#define GPIOTE_SENSE_INSTANCE           0
#endif
#ifndef PPI_SENSE_CHANNEL_NUMBER
#define PPI_SENSE_CHANNEL_NUMBER        5
#endif
#define PPI_SENSE_CHANNEL_BIT           CHANNEL_NUMBER_TO_CHANNEL_BIT(PPI_SENSE_CHANNEL_NUMBER)


#ifndef GPIOTE_CLOCK_ENABLE_PIN
#ifdef NRF51
#define GPIOTE_CLOCK_ENABLE_PIN         14
#endif
#ifdef NRF52832
#define GPIOTE_CLOCK_ENABLE_PIN         13
#endif
#ifdef NRF52840_XXAA
#define GPIOTE_CLOCK_ENABLE_PIN        (32+3)
#endif
#endif
#ifndef GPIOTE_CLOCK_ENABLE_INSTANCE
#define GPIOTE_CLOCK_ENABLE_INSTANCE    1
#endif
#ifndef PPI_CLOCK_ENABLE_CHANNEL_NUMBER
#define PPI_CLOCK_ENABLE_CHANNEL_NUMBER 6
#endif
#define PPI_CLOCK_ENABLE_CHANNEL_BIT    CHANNEL_NUMBER_TO_CHANNEL_BIT(PPI_CLOCK_ENABLE_CHANNEL_NUMBER)


#define DBG_SD_CALL_FAILURE     // {NRF_GPIO->DIRSET = (1 << 5); NRF_GPIO->OUTSET = (1 << 5); NRF_GPIO->DIRCLR = (1 << 5); NRF_GPIO->OUTCLR = (1 << 5);}
#define DBG_SYNC_START          // {NRF_GPIO->DIRSET = (1 << 4); NRF_GPIO->OUTSET = (1 << 4); NRF_GPIO->DIRCLR = (1 << 4); NRF_GPIO->OUTCLR = (1 << 4);}
#define DBG_SYNC_END            // {NRF_GPIO->DIRSET = (1 << 3); NRF_GPIO->OUTSET = (1 << 3); NRF_GPIO->DIRCLR = (1 << 3); NRF_GPIO->OUTCLR = (1 << 3);}
    
static const hal_timer_cfg_t timer_cfg =
{
    .access_mode = HAL_TIMER_ACCESS_MODE_EXCLUSIVE,
    .mode        = (TIMER_MODE_MODE_Counter     << TIMER_MODE_MODE_Pos          ),
    .bitmode     = (TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos    ),
};


static struct
{
    hal_timer_id_t      timer_id;
    hal_timer_cc_id_t   timer_cc_id;
    uint32_t            *timer_compare_addr;
    volatile uint32_t   timeout_value;
    volatile bool       done;
} spi_counter =
{
    .done = true,
};


static void hal_timer_sig_callback(hal_timer_signal_type_t hal_timer_signal_type)
{
    *spi_counter.timer_compare_addr = 0;
    
    if ( spi_counter.timeout_value == 0x10000 )
    {
#ifdef USE_SOFTDEVICE_API
        if ( sd_ppi_channel_enable_set(PPI_CLOCK_ENABLE_CHANNEL_BIT) != NRF_SUCCESS )
        {
            DBG_SD_CALL_FAILURE;
            for ( ;; );
        }
#else
        NRF_PPI->CHENSET = PPI_CLOCK_ENABLE_CHANNEL_BIT;
#endif
    }
    else if ( spi_counter.timeout_value == 0 )
    {
#ifdef USE_SOFTDEVICE_API
        if ( sd_ppi_channel_enable_clr(PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT) != NRF_SUCCESS )
        {
            DBG_SD_CALL_FAILURE;
            for ( ;; );
        }
#else
        NRF_PPI->CHENCLR = PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT; 
#endif
        *hal_timer_reg_addr_get(spi_counter.timer_id, spi_counter.timer_cc_id, HAL_TIMER_HW_REG_ID_TASKS_STOP) = 1;
        hal_timer_cc_release(spi_counter.timer_id, spi_counter.timer_cc_id);
        hal_timer_close(spi_counter.timer_id);
        
        drv_pca63520_io_spi_clk_mode_cfg(DRV_PCA63520_IO_SPI_CLK_MODE_DISABLED);
        drv_mlcd_input_mode_set(DRV_MLCD_INPUT_MODE_DISABLED);
        drv_pca63520_io_disp_spi_si_mode_cfg(DRV_PCA63520_IO_DISP_SPI_SI_MODE_NORMAL);
        drv_vlcd_output_mode_set(DRV_VLCD_OUTPUT_MODE_DISABLED);
        spi_counter.done = true;
        
        DBG_SYNC_END;
    }
    
    spi_counter.timeout_value -= 0x10000;
}


static void spi_clock_counter_setup(uint32_t timeout)
{    
   spi_counter.timer_id      = HAL_TIMER_ID_NONE;
   spi_counter.timer_cc_id   = HAL_TIMER_CC_ID_NONE;
   spi_counter.timeout_value = timeout;
    
    if ( hal_timer_open(&spi_counter.timer_id, &timer_cfg) == HAL_TIMER_STATUS_CODE_SUCCESS )
    {
        if ( hal_timer_cc_acquire(spi_counter.timer_id, &spi_counter.timer_cc_id, &spi_counter.timer_compare_addr) == HAL_TIMER_STATUS_CODE_SUCCESS )
        {   
            if ( hal_timer_cc_configure(spi_counter.timer_id, spi_counter.timer_cc_id, HAL_TIMER_CC_MODE_CALLBACK, hal_timer_sig_callback)  == HAL_TIMER_STATUS_CODE_SUCCESS )
            {
#ifdef USE_SOFTDEVICE_API
                if ( sd_ppi_channel_enable_clr(PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT) != NRF_SUCCESS )
                {
                    DBG_SD_CALL_FAILURE;
                    for ( ;; );
                }
#else
                NRF_PPI->CHENCLR = PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT; 
#endif
                
                nrf_gpiote_event_config(GPIOTE_SENSE_INSTANCE, GPIOTE_SENSE_PIN, NRF_GPIOTE_POLARITY_HITOLO);
#ifdef USE_SOFTDEVICE_API
                if ( sd_ppi_channel_assign(PPI_SENSE_CHANNEL_NUMBER,
                                           &(NRF_GPIOTE->EVENTS_IN[GPIOTE_SENSE_INSTANCE]),
                                           hal_timer_reg_addr_get(spi_counter.timer_id, spi_counter.timer_cc_id, HAL_TIMER_HW_REG_ID_TASKS_COUNT)) != NRF_SUCCESS )
                {
                    DBG_SD_CALL_FAILURE;
                    for ( ;; );
                }
#else
                NRF_PPI->CH[PPI_SENSE_CHANNEL_NUMBER].TEP = (uint32_t)hal_timer_reg_addr_get(spi_counter.timer_id, spi_counter.timer_cc_id, HAL_TIMER_HW_REG_ID_TASKS_COUNT); 
                NRF_PPI->CH[PPI_SENSE_CHANNEL_NUMBER].EEP = (uint32_t)&(NRF_GPIOTE->EVENTS_IN[GPIOTE_SENSE_INSTANCE]);     
#endif
                nrf_gpiote_task_config(GPIOTE_CLOCK_ENABLE_INSTANCE, GPIOTE_CLOCK_ENABLE_PIN, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);
#ifdef USE_SOFTDEVICE_API
                if ( sd_ppi_channel_assign(PPI_CLOCK_ENABLE_CHANNEL_NUMBER,
                                           spi_counter.timer_compare_addr,
                                           &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CLOCK_ENABLE_INSTANCE])) != NRF_SUCCESS )
                {
                    DBG_SD_CALL_FAILURE;
                    for ( ;; );
                }
#else
                NRF_PPI->CH[PPI_CLOCK_ENABLE_CHANNEL_NUMBER].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_OUT[GPIOTE_CLOCK_ENABLE_INSTANCE]); 
                NRF_PPI->CH[PPI_CLOCK_ENABLE_CHANNEL_NUMBER].EEP = (uint32_t)spi_counter.timer_compare_addr; 
#endif

                hal_timer_cc_reg_set(spi_counter.timer_id, spi_counter.timer_cc_id, spi_counter.timeout_value & 0xFFFF);
                spi_counter.timeout_value -= (spi_counter.timeout_value & 0xFFFF);

                if ( spi_counter.timeout_value == 0 )
                {
#ifdef USE_SOFTDEVICE_API
                    if ( sd_ppi_channel_enable_set(PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT) != NRF_SUCCESS )
                    {
                        DBG_SD_CALL_FAILURE;
                        for ( ;; );
                    }
#else
                    NRF_PPI->CHENSET = PPI_SENSE_CHANNEL_BIT | PPI_CLOCK_ENABLE_CHANNEL_BIT; 
#endif
                }
                else
                {
#ifdef USE_SOFTDEVICE_API
                    if ( sd_ppi_channel_enable_set(PPI_SENSE_CHANNEL_BIT) != NRF_SUCCESS )
                    {
                        DBG_SD_CALL_FAILURE;
                        for ( ;; );
                    }
#else
                    NRF_PPI->CHENSET = PPI_SENSE_CHANNEL_BIT;
#endif                    
                }

                DBG_SYNC_START;
                *hal_timer_reg_addr_get(spi_counter.timer_id, spi_counter.timer_cc_id, HAL_TIMER_HW_REG_ID_TASKS_START) = 1;
                NRF_GPIOTE->TASKS_OUT[GPIOTE_CLOCK_ENABLE_INSTANCE] = 1;
            }
        }
        else
        {
            hal_timer_close(spi_counter.timer_id);
        }
    }
}


uint32_t pca63520_util_vlcd_mlcd_sync(void)
{
    if ( spi_counter.done )
    {
        spi_counter.done = false;
        
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
    return ( !spi_counter.done );
}
