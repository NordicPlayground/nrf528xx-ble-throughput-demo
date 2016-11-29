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
#ifndef VLCD_H__
#define VLCD_H__
#include "drv_23lcv.h"
#include <stdint.h>


#if defined(MLCD_PCA64107_2INCH7) || defined(MLCD_PCA63520_2INCH7)
#define VLCD_WIDTH  (400)
#define VLCD_HEIGHT (240)
#endif

#ifdef MLCD_PCA63517_1INCH27
#define VLCD_WIDTH  (96)
#define VLCD_HEIGHT (96)
#endif


#if !defined(VLCD_WIDTH) || !defined(VLCD_HEIGHT)
#error "A display shall be defined!"
#endif


/**@brief The signal types conveyed by the signal callback.
 */
typedef enum
{
    DRV_VLCD_SIGNAL_TYPE_CLEARED,           ///< Sent when the virtual display has been cleared.
    DRV_VLCD_SIGNAL_TYPE_WRITE_COMPLETE,    ///< Sent when the write command has completed.
    DRV_VLCD_SIGNAL_TYPE_READ_COMPLETE,     ///< Sent when the read command has completed.
    DRV_VLCD_SIGNAL_TYPE_PAUSE,             ///< Sent when the current command has paused.
    DRV_VLCD_SIGNAL_TYPE_ERROR,             ///< Sent when the last operation failed.
} drv_vlcd_signal_type_t;


/**@brief The vlcd status codes.
 */
enum
{
    DRV_VLCD_STATUS_CODE_SUCCESS,       ///< Successfull.
    DRV_VLCD_STATUS_CODE_DISALLOWED,    ///< Disallowed.
    DRV_VLCD_STATUS_CODE_INVALID_PARAM, ///< Invalid parameters.
};


/**@brief The type of the signal callback conveying signals from the driver.
 *
 * @param drv_vlcd_signal_type The signal conveyed by the signal callback.
 */
typedef void (*drv_vlcd_sig_callback_t) (drv_vlcd_signal_type_t drv_vlcd_signal_type);


typedef enum
{
    DRV_VLCD_WRITE_REQ_TYPE_SINGLE = 0, ///< The line to write is the only line to write to the virtual display.
    DRV_VLCD_WRITE_REQ_TYPE_START,      ///< The line to write is the first line to write to the virtual display.
    DRV_VLCD_WRITE_REQ_TYPE_CONTINUE,   ///< The line to write is a consecutive line (and not the last one) to write to the virtual display.
    DRV_VLCD_WRITE_REQ_TYPE_LAST,       ///< The line to write is the last line to write to the virtual display.
} vlcd_write_req_type_t;


typedef enum
{
    DRV_VLCD_OUTPUT_MODE_DISABLED = 0,  ///< The virtual display output is disabled.
    DRV_VLCD_OUTPUT_MODE_DIRECT_SPI,    ///< The virtual display output is available through the SPI.
} drv_vlcd_output_mode_t;


typedef enum
{
    DRV_VLCD_FB_LOCATION_SET_ACTION_NONE = 0,       ///< Just move the framebuffer location.
    DRV_VLCD_FB_LOCATION_SET_ACTION_COPY_TO_VLCD,   ///< Copy the content of the framebuffer to the vlcd at the new location.
    DRV_VLCD_FB_LOCATION_SET_ACTION_COPY_FROM_VLCD, ///< Copy the content of the vlcd at the new location to the framebuffer.
} drv_vlcd_fb_location_set_action_t;


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
    struct
    {
        uint16_t    width;
        uint16_t    height;
    } fb_dim;
} drv_vlcd_cfg_t;


/**@brief The framebuffer interface  configuration.
 */
typedef struct
{
    struct
    {
        uint16_t    x0;
        uint16_t    y0;
    } fb_pos;
} drv_vlcd_fb_if_cfg_t;

typedef enum
{
    DRV_VLCD_COLOR_BLACK = 0,
    DRV_VLCD_COLOR_WHITE = 1,
} drv_vlcd_color_t;


/**@brief Inits the virtual display driver.
 *
 * @param p_vlcd_cfg  A pointer to the virtual display configuration.
 */
void drv_vlcd_init(drv_vlcd_cfg_t  const * const p_vlcd_cfg);


/**@brief Sets the callback function.
 *
 * @note Writing to the lcd will be blocking calls if no callback is set.
 *
 * @param{in] hal_spi_sig_callback  The signal callback function, or NULL if not used.
 */
void drv_vlcd_callback_set(drv_vlcd_sig_callback_t drv_vlcd_sig_callback);
    

/**@brief Gets the storage size of the virtual LCD.
 *
 */
uint32_t drv_vlcd_storage_size_get(void);


/**@brief Sets the output mode of the virtual LCD.
 *
 * @param output_mode  The mode (i.e. output content through the normal SPI or using the Direct SPI Access mode).
 */
uint32_t drv_vlcd_output_mode_set(drv_vlcd_output_mode_t output_mode);


/**@brief Configures the framebuffer interface.
 *
 * @param p_drv_vlcd_fb_if_cfg  Pointer to the framebuffer interface configuration.
 */
void drv_vlcd_fb_if_cfg(drv_vlcd_fb_if_cfg_t const * const p_drv_vlcd_fb_if_cfg);


/**@brief Sets the location of the frame buffer.
 *
 * @param fb_x0    The x coordinate of the framebuffer relative to virtual display.
 * @param fb_y0    The y coordinate of the framebuffer relative to virtual display.
 * @param action   The action to perform after moving the framebuffer to the new location.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS        if successful.
 * @retval ::HAL_SPI_STATUS_CODE_INVALID_PARAM  if the framebuffer doeas not fit in the specified location.
 */
uint32_t drv_vlcd_fb_location_set(drv_vlcd_fb_location_set_action_t action, uint16_t fb_x0, uint16_t fb_y0);


/**@brief Clears the virtual display.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_vlcd_clear(drv_vlcd_color_t bg_color);


/**@brief Writes the the virtual display.
 *
 * @param line_number     The number of the line to write.
 * @param line_length     The length of the line content to write.
 * @param p_line          A pointer to the content of the line to write.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_vlcd_write(uint8_t line_number, uint8_t line_length, uint8_t *p_line);


/**@brief Reads the the virtual display.
 *
 * @param line_number     The number of the line to write.
 * @param line_length     The length of the line content to write.
 * @param p_line          A pointer to the content of the line to write.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened.
 */
uint32_t drv_vlcd_read(uint8_t line_number, uint8_t line_length, uint8_t *p_line);


/**@brief Updates all "dirty" lines in the frame buffer.
 *
 * @note Updating the lcd will be blocking calls if no callback is set.
 *
 * @param{in] vlcd_fb_next_dirty_line_get  A pointer to a function that gets the next "dirty"
 *                                         line in the frame buffer.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the SPI driver could not be opened. */
 uint32_t drv_vlcd_update(void);


#endif // VLCD_H__
