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
#ifndef DRV_PCA63520_IO_H__
#define DRV_PCA63520_IO_H__
#include "drv_sx1509.h"
#include <stdint.h>

#define DRV_PCA63520_IO_DISP_PWR_CTRL_PIN        0
#define DRV_PCA63520_IO_DISP_PIN                 1
#define DRV_PCA63520_IO_LCD_SPI_DATA_CTRL_PIN    2
#define DRV_PCA63520_IO_HF_OSC_PWR_CTRL_PIN      3
#define DRV_PCA63520_IO_HF_OSC_ST_PIN            4
#define DRV_PCA63520_IO_SPI_CTRL_PIN             5
/* Pins 6 to 7 are unused. */
#define DRV_PCA63520_IO_EXTMODE_PIN              8
#define DRV_PCA63520_IO_EXTCOMIN_EXT_PIN         9
#define DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN     10
#define DRV_PCA63520_IO_LF_CNT_PIN              11
#define DRV_PCA63520_IO_LF_CNTR_CLR_PIN         12
#define DRV_PCA63520_IO_LF_SEL_EN_PIN           13
#define DRV_PCA63520_IO_LF_SEL0_PIN             14
#define DRV_PCA63520_IO_LF_SEL1_PIN             15


typedef struct
{
    uint32_t    hf_osc_ctrl : 6;
} drv_pca63520_io_psel_cfg_t;


/**@brief The 23LCVx configuration.
 */
typedef struct
{
    drv_pca63520_io_psel_cfg_t          psel;
    drv_sx1509_cfg_t           const *  p_drv_sx1509_cfg;
} drv_pca63520_io_cfg_t;

/**@brief The drv_23lcv status codes.
 */
enum
{
    DRV_PCA63520_IO_STATUS_CODE_SUCCESS,     ///< Successful.
    DRV_PCA63520_IO_STATUS_CODE_DISALLOWED,  ///< Disallowed.
};


/**@brief The PCA63520 I/O EXTCOM clock configuration.
 */
typedef enum
{
    DRV_PCA63520_IO_EXTCOM_CLK_NONE = 0,
    DRV_PCA63520_IO_EXTCOM_CLK_8HZ,
    DRV_PCA63520_IO_EXTCOM_CLK_16HZ,
    DRV_PCA63520_IO_EXTCOM_CLK_32HZ,
    DRV_PCA63520_IO_EXTCOM_CLK_64HZ,
} drv_pca63520_io_extcom_clk_t;


/**@brief The PCA63520 I/O EXTCOM clock configuration.
 */
typedef enum
{
    DRV_PCA63520_IO_EXTCOM_MODE_EXTENDER_LOW = 0,   ///< EXTCOM input is connected to logic '0' by the I/O extender.
    DRV_PCA63520_IO_EXTCOM_MODE_EXTENDER_HIGH,      ///< EXTCOM input is connected to logic '1' by the I/O extender.
    DRV_PCA63520_IO_EXTCOM_MODE_EXTCOM_CLOCK,       ///< EXTCOM input is connected to the EXTCOM clock.
} drv_pca63520_io_extcom_mode_t;


/**@brief The PCA63520 I/O EXTCOM clock configuration.
 */
typedef enum
{
    DRV_PCA63520_IO_EXTCOM_LEVEL_LOW = 0,   ///< EXTCOM input is logic '0'.
    DRV_PCA63520_IO_EXTCOM_LEVEL_HIGH,      ///< EXTCOM input is logic '1'.
} drv_pca63520_io_extcom_level_t;


/**@brief The PCA63520 I/O SPI clock configuration.
 */
typedef enum
{
    DRV_PCA63520_IO_SPI_CLK_MODE_DISABLED = 0,    ///< SPI clock genarator disabled mode.
    DRV_PCA63520_IO_SPI_CLK_MODE_ENABLED,         ///< SPI clock genarator enabled mode.
} drv_pca63520_io_spi_clk_mode_t;


/**@brief The PCA63520 I/O display SPI SI configuration.
 */
typedef enum
{
    DRV_PCA63520_IO_DISP_SPI_SI_MODE_NORMAL = 0,    ///< The SPI SI of the display is connected to MOSI.
    DRV_PCA63520_IO_DISP_SPI_SI_MODE_RAM,           ///< The SPI SI of the display is connected SPI SO of the RAM.
} drv_pca63520_io_disp_spi_si_mode_t;


/**@brief The PCA63520 display power mode.
 */
typedef enum
{
    DRV_PCA63520_IO_DISP_PWR_MODE_DISABLED = 0,    ///< The display power is disabled.
    DRV_PCA63520_IO_DISP_PWR_MODE_ENABLED,         ///< The display power is enabled.
} drv_pca63520_io_disp_pwr_mode_t;


/**@brief The PCA63520 display mode.
 */
typedef enum
{
    DRV_PCA63520_IO_DISP_MODE_OFF = 0,    ///< The display is off.
    DRV_PCA63520_IO_DISP_MODE_ON,         ///< The display is on.
} drv_pca63520_io_disp_mode_t;


/**@brief Inits the pca63520 io driver.
 *
 * @param p_drv_pca63520_io_cfg A pointer to the display configuration.
 */
uint32_t drv_pca63520_io_init(drv_pca63520_io_cfg_t const * const drv_pca63520_io_cfg);


/**@brief Configures the EXTCOM clock.
 *
 * @param extcom_clk   The extcom clock to use.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_extcom_clk_cfg(drv_pca63520_io_extcom_clk_t extcom_clk);


/**@brief Configures the EXTCOM input.
 *
 * @param extcom_mode  The extcom mode to use.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the input could not be configured.
 */
uint32_t drv_pca63520_io_extcom_mode_cfg(drv_pca63520_io_extcom_mode_t extcom_mode);


/**@brief Configures the EXTCOM clock.
 *
 * @param extcom_clk   The extcom clock to use.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_extcom_level_get(drv_pca63520_io_extcom_level_t *extcom_level);


/**@brief Configures the SPI clock genarator mode.
 *
 * @param spi_clk_mode  The extcom clock to use.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_spi_clk_mode_cfg(drv_pca63520_io_spi_clk_mode_t spi_clk_mode);


/**@brief Configures how the display SPI SI signal is connected to the SPI.
 *
 * @param disp_spi_si_mode  The display SPI SI mode.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_disp_spi_si_mode_cfg(drv_pca63520_io_disp_spi_si_mode_t disp_spi_si_mode);


/**@brief Configures the display power mode.
 *
 * @param disp_power_mode  The display power mode.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_disp_pwr_mode_cfg(drv_pca63520_io_disp_pwr_mode_t disp_power_mode);


/**@brief Configures the display mode.
 *
 * @param disp_power_mode  The display mode.
 *
 * @retval ::HAL_SPI_STATUS_CODE_SUCCESS    if successful.
 * @retval ::HAL_SPI_STATUS_CODE_DISALLOWED if the clock could not be configured.
 */
uint32_t drv_pca63520_io_disp_mode_cfg(drv_pca63520_io_disp_mode_t disp_on_mode);


#endif // DRV_PCA63520_IO_H__
