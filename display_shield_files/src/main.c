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
#include "nrf_drv_twi.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "app_util_platform.h"
#include "drv_mlcd.h"
#include "drv_vlcd.h"
#include "drv_disp_engine.h"
#include "drv_pca63520_io.h"
#include "pca63520_util.h"

#include "fb.h"
#include "fb_util.h"

#include <stdint.h>
#include <stdbool.h>
#include "stdint.h"
#include "nrf.h"

//#define M_BUSY_PIN  8 // Uncomment this line to toggle the specified pin while in the loop.

#define USE_CALLBACK    // Comment this line to run the display updates in blocking mode.

static const uint8_t smilies[4][32/4*32] =
{
    {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0xF0, 0x0F, 0x00,
        0x00, 0x06, 0x60, 0x00,
        0x80, 0x01, 0x80, 0x00,
        0x40, 0x00, 0x00, 0x03,
        0x20, 0x00, 0x00, 0x04,
        0x10, 0x00, 0x00, 0x08,
        0x10, 0x00, 0x00, 0x08,
        0x08, 0x00, 0x00, 0x10,
        0x04, 0x00, 0x00, 0x30,
        0x04, 0x1C, 0x38, 0x20,
        0x04, 0x1E, 0x78, 0x20,
        0x02, 0x1E, 0x78, 0x40,
        0x02, 0x0E, 0x38, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x80, 0x00,
        0x04, 0x03, 0xC0, 0x20,
        0x04, 0x06, 0x60, 0x20,
        0x0C, 0x18, 0x18, 0x10,
        0x08, 0xE0, 0x07, 0x10,
        0x10, 0x00, 0x00, 0x08,
        0x30, 0x00, 0x00, 0x04,
        0x60, 0x00, 0x00, 0x02,
        0x80, 0x00, 0x00, 0x01,
        0x00, 0x03, 0xC0, 0x00,
        0x00, 0x1C, 0x38, 0x00,
        0x00, 0xE0, 0x07, 0x00,
    },
    {
        0x00, 0x00, 0x01, 0x00,
        0x00, 0x38, 0x1C, 0x00,
        0x00, 0x03, 0x60, 0x00,
        0x80, 0x00, 0x80, 0x01,
        0x60, 0x00, 0x00, 0x03,
        0x30, 0x00, 0x00, 0x06,
        0x18, 0x00, 0x00, 0x04,
        0x08, 0x00, 0x00, 0x08,
        0x04, 0x00, 0x00, 0x10,
        0x04, 0x00, 0x00, 0x10,
        0x02, 0x00, 0x00, 0x20,
        0x02, 0x00, 0x00, 0x20,
        0x02, 0x0F, 0x4C, 0x20,
        0x81, 0x10, 0xC4, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x00, 0x20,
        0x02, 0x00, 0x00, 0x20,
        0x02, 0x01, 0x40, 0x20,
        0x02, 0x02, 0x20, 0x10,
        0x04, 0x0C, 0x18, 0x10,
        0x08, 0xF0, 0x0F, 0x18,
        0x08, 0x00, 0x00, 0x08,
        0x10, 0x00, 0x00, 0x04,
        0x20, 0x00, 0x00, 0x02,
        0x40, 0x00, 0x00, 0x01,
        0x80, 0x01, 0xC0, 0x00,
        0x00, 0x0E, 0x38, 0x00,
        0x00, 0xF0, 0x07, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0xF8, 0x1F, 0x00,
        0x00, 0x0E, 0x70, 0x00,
        0x00, 0x03, 0x80, 0x01,
        0xC0, 0x00, 0x00, 0x03,
        0x60, 0x00, 0x00, 0x04,
        0x10, 0x00, 0x00, 0x08,
        0x18, 0x00, 0x00, 0x08,
        0x08, 0x00, 0x00, 0x10,
        0x04, 0x00, 0x00, 0x30,
        0x04, 0x08, 0x00, 0x20,
        0x02, 0x3F, 0xF8, 0x40,
        0x82, 0x21, 0x8C, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x40,
        0x02, 0x00, 0x00, 0x41,
        0x02, 0x03, 0xC0, 0x40,
        0x06, 0x06, 0x70, 0x20,
        0x04, 0x38, 0x1E, 0x20,
        0x04, 0xF0, 0x0F, 0x30,
        0x08, 0xF0, 0x0F, 0x10,
        0x18, 0xE0, 0x07, 0x10,
        0x10, 0xE0, 0x07, 0x08,
        0x20, 0x80, 0x01, 0x04,
        0xC0, 0x00, 0x00, 0x03,
        0x80, 0x01, 0x80, 0x01,
        0x00, 0x06, 0x60, 0x00,
        0x00, 0xF8, 0x1F, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    {
        0x00, 0xF0, 0x07, 0x00,
        0x00, 0x0E, 0x38, 0x00,
        0x80, 0x01, 0xE0, 0x00,
        0xC0, 0x00, 0x80, 0x01,
        0x20, 0x00, 0x00, 0x02,
        0x10, 0x00, 0x00, 0x04,
        0x08, 0x00, 0x00, 0x0C,
        0x08, 0x00, 0x00, 0x08,
        0x04, 0x00, 0x00, 0x18,
        0x00, 0x00, 0x00, 0x10,
        0x82, 0x10, 0x42, 0x10,
        0x02, 0x19, 0x44, 0x20,
        0x02, 0x06, 0x38, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00, 0x20,
        0x02, 0x01, 0x40, 0x20,
        0x02, 0x06, 0x30, 0x20,
        0x06, 0x0C, 0x1C, 0x10,
        0x04, 0xF0, 0x07, 0x10,
        0x0C, 0x00, 0x00, 0x18,
        0x08, 0x00, 0x00, 0x0C,
        0x10, 0x00, 0x00, 0x04,
        0x20, 0x00, 0x00, 0x02,
        0x40, 0x00, 0x00, 0x01,
        0x80, 0x00, 0xC0, 0x00,
        0x00, 0x07, 0x78, 0x00,
        0x00, 0xFC, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
};

#ifndef MLCD_PCA63520_2INCH7
#error "Runs only on the PCA63520 board."
#endif

#if defined(NRF51)
#define ARDUINO_SCL_PIN      7
#define ARDUINO_SDA_PIN     30

#define ARDUINO_MOSI_PIN    25
#define ARDUINO_MISO_PIN    28
#define ARDUINO_SCK_PIN     29
#define ARDUINO_D2          14
#define ARDUINO_D3          15
#define ARDUINO_D9          23
#define ARDUINO_D10         24
#elif defined(NRF52)
#define ARDUINO_SCL_PIN     27
#define ARDUINO_SDA_PIN     26

#define ARDUINO_MOSI_PIN    23
#define ARDUINO_MISO_PIN    24
#define ARDUINO_SCK_PIN     25
#define ARDUINO_D2          13
#define ARDUINO_D3          14
#define ARDUINO_D9          20
#define ARDUINO_D10         22
#else
#error "Runs only on nRF51 or nRF52."
#endif


static const nrf_drv_twi_t m_twi_instance1 = NRF_DRV_TWI_INSTANCE(1);
static const nrf_drv_twi_config_t m_twi_cfg =
{
    .scl  = ARDUINO_SCL_PIN,
    .sda  = ARDUINO_SDA_PIN,
    .frequency          = NRF_TWI_FREQ_400K,
    .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
};


static const nrf_drv_spi_t m_spi_instance0 = NRF_DRV_SPI_INSTANCE(0);
static const nrf_drv_spi_config_t m_spi_cfg =
{
    .sck_pin      = ARDUINO_SCK_PIN,
    .mosi_pin     = ARDUINO_MOSI_PIN,
    .miso_pin     = ARDUINO_MISO_PIN,
    .ss_pin       = NRF_DRV_SPI_PIN_NOT_USED,
    .irq_priority = APP_IRQ_PRIORITY_HIGH,
    .orc          = 0xFF,
    .frequency    = NRF_DRV_SPI_FREQ_1M,
    .mode         = NRF_DRV_SPI_MODE_0,
    .bit_order    = NRF_DRV_SPI_BIT_ORDER_LSB_FIRST,
};


static const drv_mlcd_cfg_t drv_mlcd_cfg =
{
    .spi.p_config   = &m_spi_cfg,
    .spi.p_instance = &m_spi_instance0,
    .spi.ss_pin     = ARDUINO_D10,
};


static const drv_vlcd_cfg_t drv_vlcd_cfg =
{
    .spi.p_config   = &m_spi_cfg,
    .spi.p_instance = &m_spi_instance0,
    .spi.ss_pin     = ARDUINO_D9,
    .fb_dim.width = FB_WIDTH,
    .fb_dim.height = FB_HEIGHT,
};


static const drv_disp_engine_cfg_t drv_disp_engine_cfg =
{
    .fb_next_dirty_line_get = fb_next_dirty_line_get,
    .fb_line_storage_ptr_get = fb_line_storage_ptr_get,
    .fb_line_storage_set = fb_line_storage_set,
};


static const drv_sx1509_cfg_t drv_sx1509_cfg =
{
    .twi_addr = 0x3E,
    .p_twi_instance = &m_twi_instance1,
    .p_twi_cfg      = &m_twi_cfg,
};


static const drv_pca63520_io_cfg_t drv_pca63520_io_cfg =
{
    .psel.hf_osc_ctrl  = ARDUINO_D2,
    .p_drv_sx1509_cfg  = &drv_sx1509_cfg,
};


static volatile bool drv_vlcd_sig_callback_invoked = false;
static volatile bool m_vlcd_update_in_progrees = false;

PCA63520_UTIL_CONST_DEFAULT_CONFIG_DECLARE(ARDUINO_D2, ARDUINO_D3);

void app_util_critical_region_enter (uint8_t *p_nested){}
void app_util_critical_region_exit (uint8_t nested){}

static bool m_next_icon_put(void)
{
    static uint8_t x = 0;
    static uint8_t y = 0;
    static uint8_t i = 0;
    
    m_vlcd_update_in_progrees = true;
    
    drv_vlcd_fb_location_set(DRV_VLCD_FB_LOCATION_SET_ACTION_NONE, x * FB_WIDTH, y * FB_HEIGHT);
    fb_reset(FB_COLOR_WHITE);
    fb_bitmap8_put(4, 4, &(smilies[(y + x + i) % 4][0]), 32, 32, FB_COLOR_BLACK);
    
    drv_vlcd_update();
    
    x = ( (x + 1) < (400 / FB_WIDTH ) ) ? x + 1 : 0;
    if ( x == 0 )
    {
        y = ( (y + 1) < (240 / FB_HEIGHT) ) ? y + 1 : 0;
    
        if ( y == 0 )
        {
            i = ( i + 1 < 4 ) ? i + 1 : 0;
            return ( true );
        }
    }
    
    return ( false );
}


#ifdef USE_CALLBACK
void drv_vlcd_sig_callback(drv_vlcd_signal_type_t drv_vlcd_signal_type)
{
    static bool last_update_in_progress = false;
    
    if ( last_update_in_progress )
    {
        last_update_in_progress   = false;
        m_vlcd_update_in_progrees = false;
        pca63520_util_vlcd_mlcd_sync();
    }
    else
    {
        last_update_in_progress = m_next_icon_put();
    }
}    
#endif


void m_mlcd_shield_demo(void)
{
#ifdef M_BUSY_PIN
        nrf_gpio_cfg_output(M_BUSY_PIN);
#endif
    drv_pca63520_io_disp_pwr_mode_cfg(DRV_PCA63520_IO_DISP_PWR_MODE_ENABLED);
    drv_pca63520_io_disp_mode_cfg(DRV_PCA63520_IO_DISP_MODE_ON);
    drv_mlcd_clear();
    drv_vlcd_clear(DRV_VLCD_COLOR_WHITE);
#ifdef USE_CALLBACK
    drv_vlcd_callback_set(drv_vlcd_sig_callback);
#endif
    while ( true )
    {
#ifdef M_BUSY_PIN
        nrf_gpio_pin_set(M_BUSY_PIN);
#endif
#ifdef USE_CALLBACK
        if ( !m_vlcd_update_in_progrees
        &&   !pca63520_util_vlcd_mlcd_sync_active() )
        {
            m_next_icon_put();
        }
#else
    if ( m_next_icon_put() )
    {
        pca63520_util_vlcd_mlcd_sync();
        while (pca63520_util_vlcd_mlcd_sync_active());
    }
#endif
#ifdef M_BUSY_PIN
        nrf_gpio_pin_clear(M_BUSY_PIN);
#endif    
    }
}


int main(void)
{
    nrf_drv_ppi_init();
    nrf_drv_gpiote_init();

    drv_23lcv_init();
    drv_disp_engine_init(&drv_disp_engine_cfg);
    drv_pca63520_io_init(&drv_pca63520_io_cfg);
    pca63520_util_vlcd_mlcd_sync_setup(&m_pca63520_util_cfg);
    drv_mlcd_init(&drv_mlcd_cfg);
    drv_vlcd_init(&drv_vlcd_cfg);

    NVIC_EnableIRQ(TIMER2_IRQn);
    
    m_mlcd_shield_demo(); 

    for (;;);
}



/* ------------------------------------------------------------------------------------------------------------ */

