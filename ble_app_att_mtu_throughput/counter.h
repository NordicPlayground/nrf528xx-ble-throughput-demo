/* Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
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
/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#ifndef COUNTER_H__
#define COUNTER_H__

#include <stdint.h>

/**@brief   Function for initializing the RTC driver instance. */
void counter_init(void);


/**@brief   Function for starting the counter. */
void counter_start(void);


/**@brief   Function for stopping the counter. */
void counter_stop(void);


/*@brief    Function for retrieving the counter value. */
uint32_t counter_get(void);


/**@brief   Function for printing the counter value. */
void counter_print(void);

#endif // COUNTER_H__
/** @}
 *  @endcond
 */
