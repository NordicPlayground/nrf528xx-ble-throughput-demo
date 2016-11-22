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
#ifndef DRV_MLCD_H__
#define DRV_MLCD_H__
#include "nrf_spi.h"
#include "nrf_drv_spi.h"
#include <stdint.h>


/**@brief The signal types conveyed by the signal callback.
 */
typedef enum
{
    DRV_MLCD_SIGNAL_TYPE_CLEARED,           ///< Sent when the display has been cleared.
    DRV_MLCD_SIGNAL_TYPE_WRITE_PAUSED,      ///< Sent when the write command has paused.
    DRV_MLCD_SIGNAL_TYPE_WRITE_COMPLETE,    ///< Sent when the write command has completed.
    DRV_MLCD_SIGNAL_TYPE_ERROR,             ///< The last request did not complete sucessfully.
} drv_mlcd_signal_type_t;


/**@brief The mlcd status codes.
 */
enum
{
    DRV_MLCD_STATUS_CODE_SUCCESS,     ///< Successfull.
    DRV_MLCD_STATUS_CODE_DISALLOWED,  ///< Disallowed.
};


typedef enum
{
    DRV_MLCD_INPUT_MODE_DISABLED = 0,  ///< The memory display output is disabled.
    DRV_MLCD_INPUT_MODE_DIRECT_SPI,    ///< The memory display output is available through the SPI.
} drv_mlcd_input_mode_t;


/**@brief The type of the signal callback conveying signals from the driver.
 *
 * @param drv_mlcd_signal_type The signal conveyed by the signal callback.
 */
typedef void (*drv_mlcd_sig_callback_t) (drv_mlcd_signal_type_t drv_mlcd_signal_type);


/**@brief Gets the next line that has been modified in the buffer.
 *
 * @param p_line_length A pointer to storage where the length of the next
 *                      modified line in the buffer will be stored.
 * @param p_line        A pointer to the next modified line in the buffer.
 *
 * @return The number of the modified line, or 0xFFFF if there is no modified 
 *         line in the buffer.
 */
typedef uint16_t (*drv_mlcd_fb_next_dirty_line_get_t) (uint8_t *p_line_length, uint8_t **p_line);


typedef enum
{
    DRV_MLCD_WRITE_REQ_TYPE_SINGLE = 0, ///< The line to write is the only line to write to the display.
    DRV_MLCD_WRITE_REQ_TYPE_START,      ///< The line to write is the first line to write to the display.
    DRV_MLCD_WRITE_REQ_TYPE_CONTINUE,   ///< The line to write is a consecutive line (and not the last one) to write to the display.
    DRV_MLCD_WRITE_REQ_TYPE_LAST,       ///< The line to write is the last line to write to the display.
} drv_mlcd_write_req_type_t;

/**@brief The spi configuration.
 */
typedef struct
{
    struct
    {
        uint32_t                     ss_pin;
        nrf_drv_spi_t        const * p_instance;
        nrf_drv_spi_config_t const * p_config;
    } spi;
} drv_mlcd_cfg_t;


/**@brief Inits the memory display driver.
 *
 * @param p_drv_mlcd_cfg  A pointer to the display configuration.
 */
void drv_mlcd_init(drv_mlcd_cfg_t  const * const p_drv_mlcd_cfg);


/**@brief Sets the callback function.
 *
 * @note Writing to the lcd will be blocking calls if no callback is set.
 *
 * @param{in] hal_spi_sig_callback  The signal callback function, or NULL if not used.
 */
void drv_mlcd_callback_set(drv_mlcd_sig_callback_t drv_mlcd_sig_callback);
    
uint32_t drv_mlcd_input_mode_set(drv_mlcd_input_mode_t input_mode);

/**@brief Clears the display.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_mlcd_clear(void);


/**@brief Performes the nop command of the display to update the VCOM bit.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_mlcd_nop(void);


/**@brief Writes the the display.
 *
 * @param write_req_type  The type of write operation.
 * @param line_number     The number of the line to write.
 * @param line_length     The length of the line content to write.
 * @param p_line          A pointer to the content of the line to write.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_mlcd_write(drv_mlcd_write_req_type_t write_req_type, uint8_t line_number, uint8_t line_length, uint8_t *p_line);


/**@brief Updates all "dirty" lines in the frame buffer.
 *
 * @note Updating the lcd will be blocking calls if no callback is set.
 *
 * @param{in] drv_mlcd_fb_next_dirty_line_get  A pointer to a function that gets the next "dirty"
 *                                         line in the frame buffer.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened. */
uint32_t drv_mlcd_update(drv_mlcd_fb_next_dirty_line_get_t drv_mlcd_fb_next_dirty_line_get);


#endif // DRV_MLCD_H__
