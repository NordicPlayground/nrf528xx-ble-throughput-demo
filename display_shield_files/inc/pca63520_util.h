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
#ifndef PCA63520_UTIL_H__
#define PCA63520_UTIL_H__

#include "nrf_drv_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_ppi.h"

#include <stdbool.h>
#include <stdint.h>


/**@brief The default multi-timer configuration. */
#define PCA63520_UTIL_DEFAULT_TIMER_CONFIG \
{\
    .frequency  = NRF_TIMER_FREQ_16MHz,\
    .mode       = NRF_TIMER_MODE_COUNTER,\
    .bit_width  = NRF_TIMER_BIT_WIDTH_16,\
    .interrupt_priority = APP_IRQ_PRIORITY_HIGH\
}


/**@brief The default GPIOFE input configuration. */
#define PCA63520_UTIL_DEFAULT_GPIOTE_IN_CONFIG \
{\
    .sense          = NRF_GPIOTE_POLARITY_HITOLO,\
    .pull           = NRF_GPIO_PIN_NOPULL,\
    .is_watcher     = false,\
    .hi_accuracy    = true,\
}


/**@brief The default GPIOFE output configuration. */
#define PCA63520_UTIL_DEFAULT_GPIOTE_OUT_CONFIG \
{\
    .action = NRF_GPIOTE_POLARITY_TOGGLE,\
    .init_state = NRF_GPIOTE_INITIAL_VALUE_LOW,\
    .task_pin = true,\
}


/**@brief Declares the default configuration according to what state PCA63520_UTIL_DCX_PIN_ENABLED flag is in. */
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
#define PCA63520_UTIL_CONST_DEFAULT_CONFIG_DECLARE(hf_osc_ctrl_pin, hf_osc_out_sense_pin, dcx_pin, latency_cycles)       \
static const nrf_drv_timer_t m_pca63520_util_timer_instanse1 = NRF_DRV_TIMER_INSTANCE(1);                                \
static const nrf_drv_timer_t m_pca63520_util_timer_instanse2 = NRF_DRV_TIMER_INSTANCE(2);                                \
static const nrf_drv_timer_config_t m_pca63520_util_counter_config            = PCA63520_UTIL_DEFAULT_TIMER_CONFIG;      \
static const nrf_drv_gpiote_in_config_t m_pca63520_util_gpiote_input_config   = PCA63520_UTIL_DEFAULT_GPIOTE_IN_CONFIG;  \
static const nrf_drv_gpiote_out_config_t m_pca63520_util_gpiote_output_config = PCA63520_UTIL_DEFAULT_GPIOTE_OUT_CONFIG; \
static const pca63520_util_cfg_t m_pca63520_util_cfg =                                                                   \
{                                                                                                                        \
    .sense.pin.id             = hf_osc_out_sense_pin,                                                                    \
    .sense.pin.p_config       = &(m_pca63520_util_gpiote_input_config),                                                  \
    .sense.counter.p_instance = &(m_pca63520_util_timer_instanse1),                                                      \
    .sense.counter.p_config   = &(m_pca63520_util_counter_config),                                                       \
                                                                                                                         \
    .dcx.initial_latency_cycles = latency_cycles,                                                                        \
    .dcx.pin.id                 = dcx_pin,                                                                               \
    .dcx.pin.p_config           = &(m_pca63520_util_gpiote_output_config),                                               \
                                                                                                                         \
    .dcx.counter.p_instance = &(m_pca63520_util_timer_instanse2),                                                        \
    .dcx.counter.p_config   = &(m_pca63520_util_counter_config),                                                         \
                                                                                                                         \
    .disable.pin.id         = hf_osc_ctrl_pin,                                                                           \
    .disable.pin.p_config   = &(m_pca63520_util_gpiote_output_config),                                                   \
}
#else
#define PCA63520_UTIL_CONST_DEFAULT_CONFIG_DECLARE(hf_osc_ctrl_pin, hf_osc_out_sense_pin)                         \
static const nrf_drv_timer_t m_pca63520_util_timer_instanse1 = NRF_DRV_TIMER_INSTANCE(1);                                \
static const nrf_drv_timer_config_t m_pca63520_util_counter_config            = PCA63520_UTIL_DEFAULT_TIMER_CONFIG;      \
static const nrf_drv_gpiote_in_config_t m_pca63520_util_gpiote_input_config   = PCA63520_UTIL_DEFAULT_GPIOTE_IN_CONFIG;  \
static const nrf_drv_gpiote_out_config_t m_pca63520_util_gpiote_output_config = PCA63520_UTIL_DEFAULT_GPIOTE_OUT_CONFIG; \
static const pca63520_util_cfg_t m_pca63520_util_cfg =                                                                   \
{                                                                                                                        \
    .sense.pin.id             = hf_osc_out_sense_pin,                                                                    \
    .sense.pin.p_config       = &(m_pca63520_util_gpiote_input_config),                                                  \
    .sense.counter.p_instance = &(m_pca63520_util_timer_instanse1),                                                      \
    .sense.counter.p_config   = &(m_pca63520_util_counter_config),                                                       \
                                                                                                                         \
    .disable.pin.id         = hf_osc_ctrl_pin,                                                                           \
    .disable.pin.p_config   = &(m_pca63520_util_gpiote_output_config),                                                   \
}
#endif


/**@brief The pca63520_util module status codes. */
enum
{
    PCA63520_UTIL_STATUS_CODE_SUCCESS = 0,
    PCA63520_UTIL_STATUS_CODE_DISALLOWED,
};



/**@brief The pca63520_util module configuration. */
typedef struct
{
    struct
    {
        struct
        {
            nrf_drv_gpiote_pin_t                id;         ///< The sense pin.      
            nrf_drv_gpiote_in_config_t  const * p_config;   ///< The GPIOTE configuration for the sense pin.
        } pin;
        
        struct
        {
            nrf_drv_timer_t             const * p_instance; ///< The timer instance to use to count the falling edges.
            nrf_drv_timer_config_t      const * p_config;   ///< The timer counter configuration to count falling edges.
        } counter;
    } sense;
#ifdef PCA63520_UTIL_DCX_PIN_ENABLED
    struct
    {
        uint16_t initial_latency_cycles;                    ///< The number of clock cycles to the first group of 9 bits containing the DCX bit.

        struct
        {
            nrf_drv_gpiote_pin_t                id;         ///< The DCX pin.
            nrf_drv_gpiote_out_config_t const * p_config;   ///< The GPIOTE configuration for the DCX pin.
        } pin;
        
        struct
        {
            nrf_drv_timer_t             const * p_instance; ///< The timer instance to use to trigger the DCX signal.
            nrf_drv_timer_config_t      const * p_config;   ///< The timer counter configuration to trigger the DCX signal.
        } counter;
    } dcx;
#endif
    struct
    {
        struct
        {
            nrf_drv_gpiote_pin_t                id;         ///< The disable pin.
            nrf_drv_gpiote_out_config_t const * p_config;   ///< The GPIOTE configuration for the disable pin.
        } pin;
    } disable;
} pca63520_util_cfg_t;



/**@brief Sets up the sync feature. 
 *
 * @param p_pca63520_util_cfg   Points to the pca63520_util configuration.
 *
 * @retval PCA63520_UTIL_STATUS_CODE_SUCCESS, If successful.
 * @retval PCA63520_UTIL_STATUS_CODE_DISALLOWED, If currently not allowed (i.e. resourses are not available). */
uint32_t pca63520_util_vlcd_mlcd_sync_setup(pca63520_util_cfg_t const * p_pca63520_util_cfg);


/**@brief Syncronizes the memory LCD with the virtual LCD.
 *
 * @param p_pca63520_util_cfg   Points to the pca63520_util configuration.
 *
 * @retval PCA63520_UTIL_STATUS_CODE_SUCCESS, If successful.
 * @retval PCA63520_UTIL_STATUS_CODE_DISALLOWED, If already synchronizing. */
uint32_t pca63520_util_vlcd_mlcd_sync(void);


/**@brief Tells whether the memory LCD is currently getting synchronized with the virtual LCD.
 *
 * @retval  true    If syncronizing.
 * @retval  false   If not syncronising. */
bool pca63520_util_vlcd_mlcd_sync_active(void);


/**@brief Tears down the sync feature (i.e. returning the resourses). 
 *
 * @retval PCA63520_UTIL_STATUS_CODE_SUCCESS, If successful.
 * @retval PCA63520_UTIL_STATUS_CODE_DISALLOWED, If not allowed at this time. */
uint32_t pca63520_util_vlcd_mlcd_sync_teardown(void);


#endif // PCA63520_UTIL_H__
