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
#include "drv_pca63520_io.h"
#include "drv_sx1509.h"
#include "nrf_gpio.h"
#include <stdlib.h>

drv_pca63520_io_cfg_t const * p_cfg;

uint32_t drv_pca63520_io_init(drv_pca63520_io_cfg_t const * p_drv_pca63520_io_cfg)
{
    uint16_t tmp_u16;
    
    if ( (p_cfg == NULL) 
    &&   (p_drv_pca63520_io_cfg != NULL)
    &&   (p_drv_pca63520_io_cfg->p_drv_sx1509_cfg->p_twi_cfg      != NULL)
    &&   (p_drv_pca63520_io_cfg->p_drv_sx1509_cfg->p_twi_instance != NULL)
    &&   (drv_sx1509_open(p_drv_pca63520_io_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS) )
    {
        p_cfg = p_drv_pca63520_io_cfg;

        if ( p_cfg->psel.hf_osc_ctrl != 0xFFFFFFFF )
        {
                nrf_gpio_pin_clear(p_cfg->psel.hf_osc_ctrl);
                nrf_gpio_pin_dir_set(p_cfg->psel.hf_osc_ctrl, NRF_GPIO_PIN_DIR_OUTPUT);
        }


        
        tmp_u16 = (1 << DRV_PCA63520_IO_DISP_PWR_CTRL_PIN)    |
                  (1 << DRV_PCA63520_IO_DISP_PIN)             |
                  (1 << DRV_PCA63520_IO_LCD_SPI_DATA_CTRL_PIN)|
                  (1 << DRV_PCA63520_IO_HF_OSC_PWR_CTRL_PIN)  |
                  (1 << DRV_PCA63520_IO_HF_OSC_ST_PIN)        |
                  (1 << DRV_PCA63520_IO_SPI_CTRL_PIN)         |
                  (1 << DRV_PCA63520_IO_EXTMODE_PIN)          |
                  (1 << DRV_PCA63520_IO_EXTCOMIN_EXT_PIN)     |
                  (1 << DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN)  |
                  //(1 << DRV_PCA63520_IO_LF_CNT_PIN)           | // Input.
                  (1 << DRV_PCA63520_IO_LF_CNTR_CLR_PIN)      |
                  (1 << DRV_PCA63520_IO_LF_SEL_EN_PIN)        |
                  (1 << DRV_PCA63520_IO_LF_SEL0_PIN)          |
                  (1 << DRV_PCA63520_IO_LF_SEL1_PIN);


        drv_sx1509_pulldown_modify((1 << DRV_PCA63520_IO_LF_CNT_PIN), tmp_u16);
        drv_sx1509_dir_modify((1 << DRV_PCA63520_IO_LF_CNT_PIN), tmp_u16);
        drv_sx1509_data_modify(0, tmp_u16);
        
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_extcom_clk_cfg(drv_pca63520_io_extcom_clk_t extcom_clk)
{
    uint16_t value;
    uint16_t extcom_clk_cfg_pins = (1 << DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN) |
                                   (1 << DRV_PCA63520_IO_LF_SEL_EN_PIN)       |
                                   (1 << DRV_PCA63520_IO_LF_SEL0_PIN)         |
                                   (1 << DRV_PCA63520_IO_LF_SEL1_PIN);
    
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( extcom_clk )
        {
            case DRV_PCA63520_IO_EXTCOM_CLK_NONE:
                value = 0;
                break;
            case DRV_PCA63520_IO_EXTCOM_CLK_8HZ:
                value = (1 << DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN) |
                        (1 << DRV_PCA63520_IO_LF_SEL_EN_PIN);
                break;
            case DRV_PCA63520_IO_EXTCOM_CLK_16HZ:
                value = (1 << DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN) |
                        (1 << DRV_PCA63520_IO_LF_SEL_EN_PIN)       |
                        (1 << DRV_PCA63520_IO_LF_SEL0_PIN);
                break;
            case DRV_PCA63520_IO_EXTCOM_CLK_32HZ:
                value = (1 << DRV_PCA63520_IO_LF_OSC_PWR_CTRL_PIN) |
                        (1 << DRV_PCA63520_IO_LF_SEL_EN_PIN)       |
                        (1 << DRV_PCA63520_IO_LF_SEL1_PIN);
                break;
            case DRV_PCA63520_IO_EXTCOM_CLK_64HZ:
                value = extcom_clk_cfg_pins;
                break;
        }
        
        if ( extcom_clk != DRV_PCA63520_IO_EXTCOM_CLK_NONE )
        {
            drv_sx1509_dir_modify(0, 1 << DRV_PCA63520_IO_LF_CNT_PIN);
        }
        drv_sx1509_data_modify(value, extcom_clk_cfg_pins & ~value);
        if ( extcom_clk == DRV_PCA63520_IO_EXTCOM_CLK_NONE )
        {
            drv_sx1509_dir_modify(1 << DRV_PCA63520_IO_LF_CNT_PIN, 0);
        }
    
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_extcom_mode_cfg(drv_pca63520_io_extcom_mode_t extcom_mode)
{
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( extcom_mode )
        {
            case DRV_PCA63520_IO_EXTCOM_MODE_EXTENDER_LOW:
                drv_sx1509_data_modify(0, (1 << DRV_PCA63520_IO_EXTMODE_PIN)     |
                                          (1 << DRV_PCA63520_IO_EXTCOMIN_EXT_PIN));
                break;
            case DRV_PCA63520_IO_EXTCOM_MODE_EXTENDER_HIGH:
                drv_sx1509_data_modify(1 << DRV_PCA63520_IO_EXTCOMIN_EXT_PIN, 
                                       1 << DRV_PCA63520_IO_EXTMODE_PIN);
                break;
            case DRV_PCA63520_IO_EXTCOM_MODE_EXTCOM_CLOCK:
                drv_sx1509_data_modify(1 << DRV_PCA63520_IO_EXTMODE_PIN, 
                                       1 << DRV_PCA63520_IO_EXTCOMIN_EXT_PIN);
                break;
            default:
                break;
        }
    
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_extcom_level_get(drv_pca63520_io_extcom_level_t *extcom_level)
{
    uint16_t value;
    
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        if ( drv_sx1509_data_get(&value) == DRV_SX1509_STATUS_CODE_SUCCESS )
        {
            if ((value & (1 << DRV_PCA63520_IO_LF_CNT_PIN)) != 0 )
            {
                *extcom_level = DRV_PCA63520_IO_EXTCOM_LEVEL_HIGH;
            }
            else
            {
                *extcom_level = DRV_PCA63520_IO_EXTCOM_LEVEL_LOW;
            }
        }
        
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_spi_clk_mode_cfg(drv_pca63520_io_spi_clk_mode_t spi_clk_mode)
{
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( spi_clk_mode )
        {
            case DRV_PCA63520_IO_SPI_CLK_MODE_DISABLED:
                drv_sx1509_data_modify(0, (1 << DRV_PCA63520_IO_HF_OSC_ST_PIN      ) |
                                          (1 << DRV_PCA63520_IO_HF_OSC_PWR_CTRL_PIN));
                break;
            case DRV_PCA63520_IO_SPI_CLK_MODE_ENABLED:
                drv_sx1509_data_modify((1 << DRV_PCA63520_IO_HF_OSC_ST_PIN      ) |
                                       (1 << DRV_PCA63520_IO_HF_OSC_PWR_CTRL_PIN), 0);
                break;
            default:
                break;
        }
    
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_disp_spi_si_mode_cfg(drv_pca63520_io_disp_spi_si_mode_t disp_spi_si_mode)
{
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( disp_spi_si_mode )
        {
            case DRV_PCA63520_IO_DISP_SPI_SI_MODE_NORMAL:
                drv_sx1509_data_modify(0, (1 << DRV_PCA63520_IO_LCD_SPI_DATA_CTRL_PIN) |
                                          (1 << DRV_PCA63520_IO_SPI_CTRL_PIN));
                break;
            case DRV_PCA63520_IO_DISP_SPI_SI_MODE_RAM:
                drv_sx1509_data_modify((1 << DRV_PCA63520_IO_LCD_SPI_DATA_CTRL_PIN) |
                                       (1 << DRV_PCA63520_IO_SPI_CTRL_PIN), 0);
                break;
            default:
                break;
        }
    
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_disp_pwr_mode_cfg(drv_pca63520_io_disp_pwr_mode_t disp_pwr_mode)
{
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( disp_pwr_mode )
        {
            case DRV_PCA63520_IO_DISP_PWR_MODE_DISABLED:
                drv_sx1509_data_modify(0, 1 << DRV_PCA63520_IO_DISP_PWR_CTRL_PIN);
                break;
            case DRV_PCA63520_IO_DISP_PWR_MODE_ENABLED:
                drv_sx1509_data_modify(1 << DRV_PCA63520_IO_DISP_PWR_CTRL_PIN, 0);
                break;
            default:
                break;
        }

        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}


uint32_t drv_pca63520_io_disp_mode_cfg(drv_pca63520_io_disp_mode_t disp_mode)
{
    if ( drv_sx1509_open(p_cfg->p_drv_sx1509_cfg) == DRV_SX1509_STATUS_CODE_SUCCESS )
    {
        switch ( disp_mode )
        {
            case DRV_PCA63520_IO_DISP_MODE_OFF:
                drv_sx1509_data_modify(0, 1 << DRV_PCA63520_IO_DISP_PIN);
                break;
            case DRV_PCA63520_IO_DISP_MODE_ON:
                drv_sx1509_data_modify(1 << DRV_PCA63520_IO_DISP_PIN, 0);
                break;
            default:
                break;
        }
        return ( drv_sx1509_close() );
    }
    
    return ( DRV_PCA63520_IO_STATUS_CODE_DISALLOWED );
}

