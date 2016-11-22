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
#ifndef DRV_23LCV_H__
#define DRV_23LCV_H__
#include "nrf_drv_spi.h"
#include <stdint.h>

/**
 * Multiple read/write operation feature
 * -------------------------------------
 *
 * The driver supports multiple write operations or read operations (mix between read and write is not allowed)
 * within the period when the chip is continously selected (i.e. CS pin is continously kept low).
 *
 * Starting a sequence of 0 or more consecutive write/read operations is done by specifying an address and
 * a negative (or 0) size (i.e. the number of bytes less to be written to or read from the device after the write/read operation
 * has finished.
 *
 * Consecutive write/read operations are inittiated by specifying a negative size (i.e. the total number of bytes less to be written
 * to or read from the device after the write/read operation has finished. No addres is allowed for this kind of access (@ref DRV_23LCV_NO_ADDR).
 *
 * Ending a sequence of 0 or more consecutive write/read operations is done by specifying size equal to or grater than zero. Specifying an address
 *  is not allowed when ending a sequence (@ref DRV_23LCV_NO_ADDR).
 */


/**@brief An address not available in the 23lcv device. */
#define DRV_23LCV_NO_ADDR (0xFFFF)

/**@brief The signal types conveyed by the signal callback.
 */
typedef enum
{
    DRV_DRV_23LCV_SIGNAL_TYPE_WRITE_COMPLETE,    ///< Sent when the write command has completed.
    DRV_DRV_23LCV_SIGNAL_TYPE_WRITE_PAUSED,      ///< Sent when one block has been written and there are more to be written.
    DRV_DRV_23LCV_SIGNAL_TYPE_READ_COMPLETE,     ///< Sent when the write command has completed.
    DRV_DRV_23LCV_SIGNAL_TYPE_READ_PAUSED,       ///< Sent when one block has been read and there are more to be read.
    
} drv_23lcv_signal_type_t;


/**@brief The drv_23lcv status codes.
 */
enum
{
    DRV_23LCV_STATUS_CODE_SUCCESS,          ///< Successfull.
    DRV_23LCV_STATUS_CODE_DISALLOWED,       ///< Disallowed.
    DRV_23LCV_STATUS_CODE_INVALID_PARAM,    ///< Invalid parameters.
};


/**@brief The type of the signal callback conveying signals from the driver.
 *
 * @param drv_23lcv_signal_type The signal conveyed by the signal callback.
 */
typedef void (*drv_23lcv_sig_callback_t) (drv_23lcv_signal_type_t drv_23lcv_signal_type);


/**@brief The 23LCVx configuration.
 */
typedef struct
{
    struct
    {
        uint32_t                     ss_pin;
        nrf_drv_spi_t        const * p_instance;
        nrf_drv_spi_config_t const * p_config;
    } spi;
} drv_23lcv_cfg_t;


/**@brief tbd.
 */
typedef struct
{
    uint16_t    offset; ///< The offset of the box.
    uint16_t    width;  ///< The width of the box.
    uint16_t    height; ///< The height of the box
} drv_23lcv_box_t;


/**@brief tbd.
 */
typedef struct
{
    drv_23lcv_box_t primary_box;
    drv_23lcv_box_t secondary_box;
} drv_23lcv_block_write_cfg_t;

/**@brief Inits the memory display driver.
 *
 * @param p_drv_23lcv_cfg  A pointer to the display configuration.
 */
void drv_23lcv_init(void);


/**@brief Sets the callback function.
 *
 * @note Writing to the lcd will be blocking calls if no callback is set.
 *
 * @param{in] hal_spi_sig_callback  The signal callback function, or NULL if not used.
 */
void drv_23lcv_callback_set(drv_23lcv_sig_callback_t drv_23lcv_sig_callback);


/**@brief Opens the 23lcv driver according to the specified configuration.
 *
 * @param[in]   p_drv_23lcv_cfg Pointer to the driver configuration for the session to be opened.
 *
 * @return DRV_23LCV_STATUS_CODE_SUCCESS      If the call was successful.
 * @return DRV_23LCV_STATUS_CODE_DISALLOWED   If the call was not allowed at this time. */
uint32_t drv_23lcv_open(drv_23lcv_cfg_t const * const p_drv_23lcv_cfg);


/**@brief Writes the the memory.
 *
 * @note
 *
 * @param[in]  dest_addr    The destination address of the data to write, or according to the specific rules
 *                          for multiple write operations while the chip is continiuosly selected (i.e. CS pin remains low).
 * @param[in]  p_src        Pointer to the data source.
 * @param[in]  size         Size of the data to be stored, or according to the specific rules
 *                          for multiple write operations while the chip is continiuosly selected (i.e. CS pin remains low).
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_23lcv_write(uint16_t dest_addr, uint8_t * p_src, int16_t size);


/**@brief Reads the the memory.
 *
 * @param[in]  p_dest       Pointer to the data to be stored.
 * @param[in]  src_addr     The destination address of the data to write, or according to the specific rules
 *                          for multiple read operations while the chip is continiuosly selected (i.e. CS pin remains low).
 * @param[in]  size         Size of the destination buffer, or according to the specific rules
 *                          for multiple read operations while the chip is continiuosly selected (i.e. CS pin remains low).
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_23lcv_read(uint8_t * p_dest, uint16_t src_addr, int16_t size);


/**@brief Closes the 23lcv driver.
 *
 * @return DRV_23LCV_STATUS_CODE_SUCCESS      If the call was successful.
 * @return DRV_23LCV_STATUS_CODE_DISALLOWED   If the call was not allowed at this time. */
uint32_t drv_23lcv_close(void);


#endif // DRV_23LCV_H__
