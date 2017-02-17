
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "nrf_drv_twi.h"
#include "nrf_drv_spi.h"
#include "app_util_platform.h"
#include "drv_mlcd.h"
#include "drv_vlcd.h"
#include "drv_disp_engine.h"
#include "drv_pca63520_io.h"
#include "pca63520_util.h"
#include "nrf_gpio.h"
#include "nrf_log.h"

#include "display.h"
#include "counter.h"

#include "fb.h"
#include "fb_util.h"

#define TEXT_LEFT_MARGIN 				2
#define TEXT_HEIGHT 					19
#define TRANSFER_BAR_LENGTH 			300
#define TRANSFER_BAR_HEIGHT_IN_LINES 	2
#define TEXT_START_YPOS 				44
#define TITLE 							"BLE THROUGHPUT DEMO"
#define NR_OF_LINES						10

#define TERMINAL_TRANSFER_BAR_LENGTH	40

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

#elif defined(NRF52840_XXAA)

#define ARDUINO_SCL_PIN     27
#define ARDUINO_SDA_PIN     26

#define ARDUINO_MOSI_PIN    (32+13)
#define ARDUINO_MISO_PIN    (32+14)
#define ARDUINO_SCK_PIN     (32+15)
#define ARDUINO_D2          (32+3)
#define ARDUINO_D3          (32+4)
#define ARDUINO_D9          (32+11)
#define ARDUINO_D10         (32+12)

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

static bool m_display_connected = false;

static const uint32_t nordic_logo_image[((80 - 1) / 32 + 1) * 41] =
{
0x1C000700, 0x00000000, 0x00000000, 0xF6000F80, 0x00000000, 0x00000000, 0xF7007FF0, 0x00000001,
0x00000000, 0xF0C0FFF8, 0x00000007, 0x00000000, 0xF077FFFC, 0x0000001F, 0x00000000, 0xF01FFFFE,
0x0000003F, 0x00000000, 0xF05FFFFF, 0x0000003F, 0x00000000, 0xF0FFFFF9, 0x0000003F, 0x00000000,
0xF3FFFFF1, 0x0000003F, 0x00000000, 0xF7FFFFC1, 0x0000003F, 0x00000000, 0xFFFFFF01, 0x0000003F,
0x00000000, 0xFFFFFE01, 0x0000003F, 0x00000000, 0xFFFFF001, 0x0000003F, 0x00000000, 0xFFFFE001,
0x0000003F, 0x00000000, 0xFFFFC001, 0x0000003F, 0x00000000, 0xFFFE0001, 0x0000003F, 0x00000000,
0xFFFC0001, 0x0000003F, 0x00000000, 0xFFE00001, 0x0000003F, 0x00000000, 0xFFC00001, 0x0000003F,
0x00000000, 0xFE800001, 0x0000003F, 0x00000000, 0xFC000001, 0x0000003F, 0x00000000, 0xF8000C01,
0x0000003F, 0x00000000, 0xE0003C01, 0x0000003F, 0x00000000, 0x8000FC01, 0x0000003F, 0x00000000,
0x0001FC01, 0x0000003F, 0x00000000, 0x000BFC01, 0x38E26038, 0x0000711E, 0x001FFC06, 0x49126030,
0x00009932, 0x0023FC0C, 0x4912A018, 0x00000922, 0x01C1FC38, 0x3912A007, 0x00000922, 0x82007CE0,
0x49132003, 0x00000922, 0xDC001D80, 0x49132000, 0x00009932, 0x30000E00, 0x48E32000, 0x0000711E,
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xC7161BCE, 0x9C447931,
0x000071CF, 0x29961849, 0x2644C932, 0x00009222, 0x20952841, 0x02448952, 0x00009222, 0x20952BC6,
0x02448952, 0x00007222, 0x2094C848, 0x02448992, 0x00009222, 0x2994C849, 0x2644C992, 0x00009222,
0xC714CBC7, 0x1C387991, 0x000091C2,
};

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
    .frequency    = NRF_DRV_SPI_FREQ_2M,
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

PCA63520_UTIL_CONST_DEFAULT_CONFIG_DECLARE(ARDUINO_D2, ARDUINO_D3);

static volatile bool m_vlcd_update_in_progrees = false;

void drv_vlcd_sig_callback(drv_vlcd_signal_type_t drv_vlcd_signal_type)
{
	m_vlcd_update_in_progrees = false;
} 

bool display_init()
{
	uint32_t ret_code;
	
	nrf_drv_ppi_init();
    nrf_drv_gpiote_init();
	
	drv_23lcv_init();
	drv_disp_engine_init(&drv_disp_engine_cfg);
	
    ret_code = drv_pca63520_io_init(&drv_pca63520_io_cfg);
	
	if(ret_code != DRV_PCA63520_IO_STATUS_CODE_SUCCESS)
	{
		m_display_connected = false;
		return false;
	}
	
	pca63520_util_vlcd_mlcd_sync_setup(&m_pca63520_util_cfg);
    drv_mlcd_init(&drv_mlcd_cfg);
    drv_vlcd_init(&drv_vlcd_cfg);

	NVIC_SetPriority(TIMER2_IRQn, APP_IRQ_PRIORITY_HIGH);
    NVIC_EnableIRQ(TIMER2_IRQn);
	
	drv_pca63520_io_disp_pwr_mode_cfg(DRV_PCA63520_IO_DISP_PWR_MODE_ENABLED);
	
    drv_pca63520_io_disp_mode_cfg(DRV_PCA63520_IO_DISP_MODE_ON);
	
    drv_mlcd_clear();
    drv_vlcd_clear(DRV_VLCD_COLOR_WHITE);
	//drv_vlcd_callback_set(drv_vlcd_sig_callback);
	
	fb_font_set(&font_calibri_12pt_info);
	
	m_display_connected = true;
	return true;
}

void display_draw_nordic_logo()
{
	if(!m_display_connected)
	{
		return;
	}
	fb_bitmap_put(400-(80+10), 0, &(nordic_logo_image[0]), 80, 41, FB_COLOR_BLACK);
}

void display_draw_title()
{
	fb_font_set(&font_calibri_18pt_info);
	fb_string_put(TEXT_LEFT_MARGIN + 5, 10, TITLE, FB_COLOR_BLACK);
	fb_font_set(&font_calibri_12pt_info);
}

void display_show()
{
	if(!m_display_connected)
	{
		return;
	}
	/*
	if ( !m_vlcd_update_in_progrees
        &&   !pca63520_util_vlcd_mlcd_sync_active() )
	{
		m_vlcd_update_in_progrees = true;
		drv_vlcd_update();
	}
	*/
	while (pca63520_util_vlcd_mlcd_sync_active());
	
	drv_vlcd_update();
	
	pca63520_util_vlcd_mlcd_sync();
}

static uint8_t line_counter = 0;

uint8_t display_get_line_nr()
{
	return line_counter;
}

void display_clear()
{
	if(!m_display_connected)
	{
		return;
	}
	line_counter = 0;
	fb_reset(FB_COLOR_WHITE);
	display_draw_nordic_logo();
	display_draw_title();
	fb_line(0, TEXT_START_YPOS - 2, FB_UTIL_LCD_WIDTH, TEXT_START_YPOS - 2, FB_COLOR_BLACK);
}

void display_print_line_inc(char * line)
{
	if(!m_display_connected)
	{
		return;
	}
	if(line_counter < NR_OF_LINES)
	{
		line_counter++;
		fb_string_put(TEXT_LEFT_MARGIN, (line_counter - 1) * TEXT_HEIGHT + TEXT_START_YPOS, line, FB_COLOR_BLACK);
	}
}

void display_print_line_center_inc(char * line)
{
	if(!m_display_connected)
	{
		return;
	}
	if(line_counter < NR_OF_LINES)
	{
		uint16_t x_pos = (FB_UTIL_LCD_WIDTH - calc_string_width(line)) / 2;
		line_counter++;
		fb_string_put(x_pos, (line_counter - 1) * TEXT_HEIGHT + TEXT_START_YPOS, line, FB_COLOR_BLACK);
	}
}

void display_print_line(char * line, uint32_t x_pos, uint8_t line_nr)
{
	if(!m_display_connected)
	{
		return;
	}
	
	if(line_nr < NR_OF_LINES)
	{
		fb_string_put(x_pos + TEXT_LEFT_MARGIN, line_nr * TEXT_HEIGHT + TEXT_START_YPOS, line, FB_COLOR_BLACK);
	}
}

//TODO: move the log stuff to another function
void display_draw_test_run_screen(transfer_data_t *transfer_data, rssi_data_t *rssi_data)
{
	static uint32_t last_counter_ticks = 0;
	static uint32_t last_bytes_transferred = 0;
	
	static float throughput = 0;
	
	//if time is too small the accuracy of the throughput calculation is too bad
	if(transfer_data->counter_ticks != 0 && (transfer_data->counter_ticks - last_counter_ticks) > 10000)
	{
		float sent_bits = (transfer_data->bytes_transfered - last_bytes_transferred) * 8;
		throughput = (float)(sent_bits * 32768 / 1000) / (transfer_data->counter_ticks - last_counter_ticks);
		
		last_bytes_transferred = transfer_data->bytes_transfered;
		last_counter_ticks = transfer_data->counter_ticks;
	}
	
	display_clear();

	NRF_LOG_RAW_INFO("\033[30;0H");	//move cursor to correct position
	NRF_LOG_RAW_INFO("\033[0J");	//clear screen from cursor and to end of screen
	
	display_print_line_center_inc("Transferring data:");
	NRF_LOG_RAW_INFO("Transferring data:\r\n");
	
	if(m_display_connected)
	{
		//print filled bar
		fb_rectangle((FB_UTIL_LCD_WIDTH - TRANSFER_BAR_LENGTH)/2, 
					line_counter*TEXT_HEIGHT + TEXT_HEIGHT/2 + TEXT_START_YPOS, 
					(FB_UTIL_LCD_WIDTH + TRANSFER_BAR_LENGTH)/2, 
					(line_counter + TRANSFER_BAR_HEIGHT_IN_LINES)*TEXT_HEIGHT + TEXT_START_YPOS + TEXT_HEIGHT/2, 
					FB_COLOR_BLACK);
		
		fb_bar((FB_UTIL_LCD_WIDTH - TRANSFER_BAR_LENGTH)/2, 
				line_counter*TEXT_HEIGHT + TEXT_HEIGHT/2 + TEXT_START_YPOS,
				(FB_UTIL_LCD_WIDTH - TRANSFER_BAR_LENGTH)/2 + (uint32_t)(transfer_data->bytes_transfered/1024)*TRANSFER_BAR_LENGTH / transfer_data->kb_transfer_size, 
				(line_counter + TRANSFER_BAR_HEIGHT_IN_LINES)*TEXT_HEIGHT + TEXT_START_YPOS + TEXT_HEIGHT/2, 
				FB_COLOR_BLACK);
		
		line_counter += TRANSFER_BAR_HEIGHT_IN_LINES + 1;
	}
	
	NRF_LOG_RAW_INFO("[");
	for(uint32_t i = 0; i < TERMINAL_TRANSFER_BAR_LENGTH; i++)
	{
		if(i < (uint32_t)(transfer_data->bytes_transfered/1024*TERMINAL_TRANSFER_BAR_LENGTH)/transfer_data->kb_transfer_size)
		{
			NRF_LOG_RAW_INFO("#");
		}
		else
		{
			NRF_LOG_RAW_INFO(".");
		}
	}
	NRF_LOG_RAW_INFO("]\r\n");
	
	char str[50];
	sprintf(str, "%dKB/%dKB transferred", transfer_data->bytes_transfered/1024, transfer_data->kb_transfer_size);
	display_print_line_center_inc(str);
	NRF_LOG_RAW_INFO("%s\r\n", nrf_log_push(str));

	sprintf(str, "Speed: %.1f Kbits/s", throughput);
	display_print_line_center_inc(str);
	NRF_LOG_RAW_INFO("%s\r\n", nrf_log_push(str));
	
	sprintf(str, "Link budget: %d", rssi_data->link_budget);
	display_print_line_center_inc(str);
	NRF_LOG_RAW_INFO("%s\r\n", nrf_log_push(str));
	
	if(rssi_data->range_multiplier <= rssi_data->range_multiplier_max)
    {
        sprintf(str, "Range multiplier: %d", rssi_data->range_multiplier);
    }
    else
    {
        sprintf(str, "Range multiplier: %d+", rssi_data->range_multiplier_max);
    }

    display_print_line_center_inc(str);
	NRF_LOG_RAW_INFO("%s\r\n", nrf_log_push(str));
	
	if(transfer_data->last_throughput > 0)
	{
		sprintf(str, "Throughput last transfer: %.2f", transfer_data->last_throughput);
		display_print_line_center_inc(str);
		NRF_LOG_RAW_INFO("%s\r\n", nrf_log_push(str));
	}
		
	display_print_line_center_inc("Press any key to terminate the test");
	NRF_LOG_RAW_INFO("Press any key to terminate the test\r\n");
	
	display_show();
}

void display_test_done_screen(transfer_data_t *transfer_data, rssi_data_t *rssi_data)
{
	display_clear();
	
	char str[50];
	uint16_t number_x_pos = 130;

	sprintf(str, "%.2f seconds.", (float)transfer_data->counter_ticks / 32768);
	display_print_line(str, number_x_pos, display_get_line_nr());
	display_print_line_inc("Time:");
	
	sprintf(str, "%u KB (%u bytes)).", transfer_data->bytes_transfered/1024, transfer_data->bytes_transfered);
	display_print_line(str, number_x_pos, display_get_line_nr());
	display_print_line_inc("Transfered:");
	
	float sent_octet_cnt = transfer_data->bytes_transfered * 8;
	float throughput = (float)(sent_octet_cnt * 32768) / (float)transfer_data->counter_ticks;
	throughput = throughput / (float)1000;
	
	sprintf(str, "%.2f Kbits/s.", throughput);
	display_print_line(str, number_x_pos, display_get_line_nr());
	display_print_line_inc("Throughput:");
	
	if(rssi_data->sum != 0)
	{
		display_print_line_inc("");
		
		int8_t avg_rssi = (rssi_data->sum + (int32_t)rssi_data->nr_of_samples/2) / (int32_t)rssi_data->nr_of_samples;
		sprintf(str, "%d dBm.", avg_rssi);
		display_print_line(str, number_x_pos, display_get_line_nr());
		display_print_line_inc("Average RSSI:");
	}
	
	display_print_line_inc("");
	display_print_line_inc("Press any button to exit.");
	
	display_show();
}
