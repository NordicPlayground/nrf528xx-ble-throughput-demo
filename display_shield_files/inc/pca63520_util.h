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

#include <stdbool.h>
#include <stdint.h>


enum
{
    PCA63520_UTIL_STATUS_CODE_SUCCESS = 0,
    PCA63520_UTIL_STATUS_CODE_DISALLOWED,
};

/**@brief Syncronices the memory LCD with the virtual LCD.
 */
uint32_t pca63520_util_vlcd_mlcd_sync(void);


/**@brief Tells whether the memory LCD is getting synchronized with the virtual LCD.
 *
 * @retval  true    If syncronizing.
 * @retval  false   If not syncronising.
 */
bool pca63520_util_vlcd_mlcd_sync_active(void);


#endif // PCA63520_UTIL_H__
