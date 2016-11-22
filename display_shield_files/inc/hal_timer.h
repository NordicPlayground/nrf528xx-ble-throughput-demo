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
#ifndef HAL_TIMER_H__
#define HAL_TIMER_H__

#include <stdbool.h>
#include <stdint.h>

#if !(defined(HAL_TIMER_TIMER0) || defined(HAL_TIMER_TIMER1) || defined(HAL_TIMER_TIMER2))
#include "sys_cfg.h"
#endif

#ifdef HAL_TIMER_CC_COUNT
#if HAL_TIMER_CC_COUNT <= 0
#error "CC count too small."
#endif
#if HAL_TIMER_CC_COUNT > 0
#define HAL_TIMER_CC0
#endif
#if HAL_TIMER_CC_COUNT > 1
#define HAL_TIMER_CC1
#endif
#if HAL_TIMER_CC_COUNT > 2
#define HAL_TIMER_CC2
#endif
#if HAL_TIMER_CC_COUNT > 3
#define HAL_TIMER_CC3
#endif
#if HAL_TIMER_CC_COUNT > 4
#error "CC count too big."
#endif
#endif

/**@brief The signal types of the hal_timer module.
 */
typedef enum
{
    HAL_TIMER_SIGNAL_TYPE_COMPARE0, ///< The signal type when EVENT_COMPARE[0] has triggered the signal callback to run.
    HAL_TIMER_SIGNAL_TYPE_COMPARE1, ///< The signal type when EVENT_COMPARE[1] has triggered the signal callback to run.
    HAL_TIMER_SIGNAL_TYPE_COMPARE2, ///< The signal type when EVENT_COMPARE[2] has triggered the signal callback to run.
    HAL_TIMER_SIGNAL_TYPE_COMPARE3, ///< The signal type when EVENT_COMPARE[2] has triggered the signal callback to run.
} hal_timer_signal_type_t;


/**@brief The IDs of the capture compare instances.
 */
typedef enum
{
    HAL_TIMER_CC_ID_0 = 0,      ///< The ID to identify capture compare registers for the first instance of the timer.
    HAL_TIMER_CC_ID_1 = 1,      ///< The ID to identify capture compare registers for the second instance of the timer.
    HAL_TIMER_CC_ID_2 = 2,      ///< The ID to identify capture compare registers for the third instance of the timer.
    HAL_TIMER_CC_ID_3 = 3,      ///< The ID to identify capture compare registers for the fourth instance of the timer.
    HAL_TIMER_CC_ID_NONE,       ///< No ID.
} hal_timer_cc_id_t;


/**@brief The timer IDs.
 */
typedef enum
{
    HAL_TIMER_ID_TIMER0 = 0,
    HAL_TIMER_ID_TIMER1 = 1,
    HAL_TIMER_ID_TIMER2 = 2,
    HAL_TIMER_ID_NONE,
} hal_timer_id_t;


/**@brief The timer hardware register IDs.
 */
typedef enum
{
    HAL_TIMER_HW_REG_ID_TASKS_START,
    HAL_TIMER_HW_REG_ID_TASKS_STOP,
    HAL_TIMER_HW_REG_ID_TASKS_COUNT,
    HAL_TIMER_HW_REG_ID_TASKS_CLEAR,
    HAL_TIMER_HW_REG_ID_TASKS_CAPTURE0,
    HAL_TIMER_HW_REG_ID_EVENTS_COMPARE0,
    HAL_TIMER_HW_REG_ID_TASKS_CAPTURE1,
    HAL_TIMER_HW_REG_ID_EVENTS_COMPARE1,
    HAL_TIMER_HW_REG_ID_TASKS_CAPTURE2,
    HAL_TIMER_HW_REG_ID_EVENTS_COMPARE2,
    HAL_TIMER_HW_REG_ID_TASKS_CAPTURE3,
    HAL_TIMER_HW_REG_ID_EVENTS_COMPARE3,
    HAL_TIMER_HW_REG_ID_NONE,
} hal_timer_hw_reg_id_t;


/**@brief The timer access modes.
 */
typedef enum
{
    HAL_TIMER_ACCESS_MODE_EXCLUSIVE, ///< All features of the timer is available to use.
    HAL_TIMER_ACCESS_MODE_SHARED,    ///< Only features related to the acquired capture compare register(s) are available.
} hal_timer_access_mode_t;


/**@brief The capture compare instance modes.
 */
typedef enum
{
    HAL_TIMER_CC_MODE_CALLBACK, ///< The signal callback is called at capture compare match.
    HAL_TIMER_CC_MODE_POLLING,  ///< The executions continues after the capture compare match.
    HAL_TIMER_CC_MODE_DEFAULT,  ///< In the default mode, only the CC register is set.
} hal_timer_cc_mode_t;

/**@brief The timer status codes.
 */
enum
{
    HAL_TIMER_STATUS_CODE_SUCCESS,          ///< Successfull.
    HAL_TIMER_STATUS_CODE_DISALLOWED,       ///< Disallowed.
    HAL_TIMER_STATUS_CODE_INVALID_PARAM,    ///< Invalid parameter.
};


/**@brief The timer configuration.
 */
typedef struct
{
    hal_timer_access_mode_t access_mode;
    uint32_t                mode;
    uint32_t                bitmode;
    uint32_t                prescaler;
} hal_timer_cfg_t;


/**@brief The type of the signal callback conveying signals from the driver.
 */
typedef void (*hal_timer_sig_callback_t) (hal_timer_signal_type_t hal_timer_signal_type);


/**@brief Initializes the timer interface.
 */
void hal_timer_init(void);


/**@brief Opens the driver to access the specified HW peripheral.
 *
 * @note The driver will search for a timer when no id is specified.
 *
 * @param[in, out]  p_id  Pointer to the id of the HW peripheral to open the driver for.
 * @param[in]       p_cfg The driver configuration.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if the driver could not be opened.
 */
uint32_t hal_timer_open(hal_timer_id_t *p_id, hal_timer_cfg_t const * const p_cfg);


/**@brief Sets the callback function.
 *
 * @nore Setting the capture compare register(s) will be blocking calls if no callback is set.
 *
 * @param[in] id                        The id of the HW peripheral to set the callback for.
 * @param[in] cc_id                     The capture compare id of the HW peripheral to set the callback for.
 * @param[in] mode                      The capture compare mode.
 * @param[in] hal_timer_sig_callback    The signal callback function, when callback mode is configured.
 */
uint32_t hal_timer_cc_configure(hal_timer_id_t id, hal_timer_cc_id_t cc_id, hal_timer_cc_mode_t mode, hal_timer_sig_callback_t hal_timer_sig_callback);


/**@brief Acquires the specified capture compare instance.
 *
 * @note The driver will search for a capture compare instance when no id is specified.
 *
 * @param[in]       id          The id of the HW peripheral.
 * @param[in, out]  p_cc_id     Pointer to the capture compare id of the HW peripheral to acquire.
 * @param[out]      p_evt_addr  The address of the compare event.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_cc_acquire(hal_timer_id_t id, hal_timer_cc_id_t *p_cc_id, uint32_t ** p_evt_addr);


/**@brief Sets the specified shorts.
 *
 * @param[in]   id          The id of the HW peripheral.
 * @param[in]   cc_id       The capture compare id of the HW peripheral to set shorts for.
 * @param[in]   set_mask    The mask specifying the shorts to set.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_shorts_set(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t set_mask);


/**@brief Clears the specified shorts.
 *
 * @param[in]   id          The id of the HW peripheral.
 * @param[in]   cc_id       The capture compare id of the HW peripheral to clear shorts for.
 * @param[in]   clear_mask  The mask specifying the shorts to clear.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_shorts_clear(hal_timer_id_t id, hal_timer_cc_id_t cc_id, uint32_t clear_mask);


/**@brief Sets the capture compare.
 *
 * @param[in]   id          The id of the HW peripheral.
 * @param[in]   cc_id       The capture compare id of the HW peripheral to set shorts for.
 * @param[in]   value       The mask specifying the shorts to set.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_cc_reg_set(hal_timer_id_t p_id, hal_timer_cc_id_t cc_id, uint32_t value);


/**@brief Gets the capture compare.
 *
 * @param[in]   id          The id of the HW peripheral.
 * @param[in]   cc_id       The capture compare id of the HW peripheral to set shorts for.
 * @param[out]  p_value     The address where the value of the capture compare register is stored.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_cc_reg_get(hal_timer_id_t p_id, hal_timer_cc_id_t cc_id, uint32_t * p_value);


/**@brief Clears the capture compare.
 *
 * @param[in]   id          The id of the HW peripheral.
 * @param[in]   cc_id       The capture compare id of the HW peripheral to set shorts for.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not available at this time.
 */
uint32_t hal_timer_cc_reg_clear(hal_timer_id_t id, hal_timer_cc_id_t cc_id);


/**@brief Gets the hardware address of the specified register.
 *
 * @note The task will be triggered immediately if no triggering event address is specified.
 *
 * @param[in]   id      The id of the HW peripheral.
 * @param[in]   cc_id   The id of the capture compare instance the capture task belongs to.
 * @param[in]   reg_id  The address of the HW event that will be used to trigger the event.
 *
 * @return          the hardware address, if successful.
 * @retval ::NULL   if not available.
*/
uint32_t * hal_timer_reg_addr_get(hal_timer_id_t p_id, hal_timer_cc_id_t cc_id, hal_timer_hw_reg_id_t reg_id);

/**@brief Releases the specified capture compare instance.
 *
 * @param[in]   id      The id of the HW peripheral.
 * @param[in]   cc_id   The capture compare id of the HW peripheral to acquire.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if not previously acquired.
 */
uint32_t hal_timer_cc_release(hal_timer_id_t id, hal_timer_cc_id_t cc_id);


/**@brief Closes the specified driver.
 *
 * @param[in] id    The id of the HW peripheral to close the driver for.
 *
 * @retval ::HAL_TIMER_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_TIMER_STATUS_CODE_DISALLOWED if the driver could not be closed.
 */
uint32_t hal_timer_close(hal_timer_id_t id);


#endif // HAL_TIMER_H__
