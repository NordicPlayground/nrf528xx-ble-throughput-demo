
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
#include "drv_pca63520_io.h"
#include "pca63520_util.h"
#include "nrf_gpio.h"

#include "display.h"
#include "counter.h"

#include "fb.h"
#include "fb_util.h"

#define TEXT_HEIGHT 26
#define TEXT_INLINE	15
#define TRANSFER_BAR_LENGTH 200

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
#define ARDUINO_D9          23
#define ARDUINO_D10         24

#elif defined(NRF52840_XXAA)

#define ARDUINO_SCL_PIN     27
#define ARDUINO_SDA_PIN     26

#define ARDUINO_MOSI_PIN    (32+13)
#define ARDUINO_MISO_PIN    (32+14)
#define ARDUINO_SCK_PIN     (32+15)
#define ARDUINO_D2          (32+3)
#define ARDUINO_D9          (32+11)
#define ARDUINO_D10         (32+12)

#elif defined(NRF52)

#define ARDUINO_SCL_PIN     27
#define ARDUINO_SDA_PIN     26

#define ARDUINO_MOSI_PIN    23
#define ARDUINO_MISO_PIN    24
#define ARDUINO_SCK_PIN     25
#define ARDUINO_D2          13
#define ARDUINO_D9          20
#define ARDUINO_D10         22

#else

#error "Runs only on nRF51 or nRF52."

#endif

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
    .vlcd_fb_next_dirty_line_get = fb_next_dirty_line_get,
    .vlcd_fb_line_storage_ptr_get = fb_line_storage_ptr_get,
    .vlcd_fb_line_storage_set = fb_line_storage_set,
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

void m_mlcd_shield_demo2(void)
{
    fb_reset(FB_COLOR_WHITE);
    fb_string_put(4, 10, "Hello", FB_COLOR_BLACK);
    
    fb_string_put(4, 10 + 26*2, "Progress", FB_COLOR_BLACK);
    fb_rectangle(4, 10+26-1, 4+100, 10+26*2 - 1, FB_COLOR_BLACK);
    
    for(int i = 0; i < 100; i++)
    {
        fb_bar(4, 10+26 - 1, 4+i, 10+26*2 - 1, FB_COLOR_BLACK);
        drv_vlcd_update();
    
        pca63520_util_vlcd_mlcd_sync();
        while (pca63520_util_vlcd_mlcd_sync_active());
    }
}

void display_init()
{
	drv_23lcv_init();

    drv_pca63520_io_init(&drv_pca63520_io_cfg);
    drv_mlcd_init(&drv_mlcd_cfg);
    drv_vlcd_init(&drv_vlcd_cfg);

	NVIC_SetPriority(TIMER2_IRQn, 7);
    NVIC_EnableIRQ(TIMER2_IRQn);
	
	drv_pca63520_io_disp_pwr_mode_cfg(DRV_PCA63520_IO_DISP_PWR_MODE_ENABLED);
    drv_pca63520_io_disp_mode_cfg(DRV_PCA63520_IO_DISP_MODE_ON);
    drv_mlcd_clear();
    drv_vlcd_clear(DRV_VLCD_COLOR_WHITE);
}

void display_test()
{
	display_init();
	
	m_mlcd_shield_demo2();
}

void display_show()
{
	drv_vlcd_update();
    
	pca63520_util_vlcd_mlcd_sync();
	while (pca63520_util_vlcd_mlcd_sync_active());
}

static uint8_t line_counter = 0;

void display_clear()
{
	line_counter = 0;
	fb_reset(FB_COLOR_WHITE);
}

void display_print_line_inc(char * line, uint32_t x_pos)
{
	if(line_counter < 9)
	{
		fb_string_put(x_pos, line_counter * TEXT_HEIGHT, line, FB_COLOR_BLACK);
		line_counter ++;
	}
}

void display_print_line(char * line, uint32_t x_pos, uint8_t line_nr)
{
	fb_string_put(x_pos, line_nr * TEXT_HEIGHT, line, FB_COLOR_BLACK);
}

uint8_t display_line_nr_get()
{
	return line_counter;
}

void display_draw_nordic_logo()
{
	fb_bitmap_put(400-(80+10), 0, &(nordic_logo_image[0]), 80, 41, FB_COLOR_BLACK);
}

void display_draw_button_description()
{
	uint32_t x_pos = 310;
	
	display_print_line("Button", x_pos, 2);
	display_print_line("Desc:", x_pos, 3);
	#if defined(NRF52840_XXAA)
	display_print_line("b1: Enter", x_pos, 4);
	display_print_line("b2: Sel", x_pos, 5);
	display_print_line("b3: Back", x_pos, 6);
	#elif defined(NRF52)
	display_print_line("7: Enter", x_pos, 4);
	display_print_line("5: Sel", x_pos, 5);
	display_print_line("11: Back", x_pos, 6);
	#endif
	
	
	fb_line(305, 0, 305, TEXT_HEIGHT*7, FB_COLOR_BLACK);
	fb_line(305, TEXT_HEIGHT*7, 400, TEXT_HEIGHT*7, FB_COLOR_BLACK);
}

void display_draw_configuration(test_params_t *test_params)
{
	char str[50];
	//display_print_line_inc("Current test configuration:", 0);
	
	sprintf(str, "ATT MTU size: %d bytes", test_params->att_mtu);
	display_print_line_inc(str, 0);
	
    sprintf(str, "Conn. interval: %.2f ms", (float)test_params->conn_interval * 1.25);
	display_print_line_inc(str, 0);
    
	sprintf(str, "Data length extension (DLE): %s", test_params->data_len_ext_enabled ?
                 (uint32_t)"ON" :
                 (uint32_t)"OFF");
	display_print_line_inc(str, 0);
	
    sprintf(str, "Conn. event length ext.: %s", test_params->conn_evt_len_ext_enabled ?
                 (uint32_t)"ON" :
                 (uint32_t)"OFF");
	display_print_line_inc(str, 0);
}

void display_draw_sender_screen(uint8_t pointer_position, test_params_t *test_params)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	display_draw_configuration(test_params);
	
	line_counter++;
	
	display_print_line_inc("Select and option:", 0);
	display_print_line_inc("Run test", TEXT_INLINE);
	display_print_line_inc("Adjust test parameters", TEXT_INLINE);
	
	//pointer_position = display_line_nr_get() - 1;
	display_print_line(">", 0, line_counter - 2 + pointer_position);
	
	display_show();
}

void display_draw_init_screen(uint8_t pointer_position)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	
	display_print_line_inc("Select role", TEXT_INLINE);
	display_print_line_inc("Sender", TEXT_INLINE);
	display_print_line_inc("Receiver", TEXT_INLINE);
	display_print_line(">", 0, line_counter - 2 + pointer_position);
	display_show();
}

void display_draw_config_screen(uint8_t pointer_position, test_params_t *test_params)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	display_draw_configuration(test_params);
	
	line_counter++;
	
	display_print_line_inc("Adjust test parameters:", 0);
	display_print_line(">", 0, line_counter + pointer_position);
	
	display_print_line_inc("Select ATT MTU size.", TEXT_INLINE);
	display_print_line_inc("Select connection interval.", TEXT_INLINE);
	display_print_line_inc("Turn on/off Data length extension (DLE).", TEXT_INLINE);
	
	display_show();
}

void display_draw_config_mtu_screen(uint8_t pointer_position, test_params_t *test_params)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	display_draw_configuration(test_params);
	
	line_counter++;
	
	display_print_line_inc("Select an ATT MTU size:", 0);
	display_print_line(">", 0, line_counter + pointer_position);
	
	display_print_line_inc("23 bytes.", TEXT_INLINE);
	display_print_line_inc("158 bytes.", TEXT_INLINE);
	display_print_line_inc("247 bytes.", TEXT_INLINE);
	
	display_show();
}

void display_draw_config_conn_int_screen(uint8_t pointer_position, test_params_t *test_params)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	display_draw_configuration(test_params);
	
	line_counter++;
	
	display_print_line_inc("Select a connection interval:", 0);
	display_print_line(">", 0, line_counter + pointer_position);
	
	display_print_line_inc("7.5 ms.", TEXT_INLINE);
	display_print_line_inc("50 ms.", TEXT_INLINE);
	display_print_line_inc("400 ms.", TEXT_INLINE);
	
	display_show();
}

void display_draw_config_dle_screen(uint8_t pointer_position, test_params_t *test_params)
{
	display_clear();
	
	display_draw_nordic_logo();
	display_draw_button_description();
	display_draw_configuration(test_params);
	
	line_counter++;
	
	display_print_line_inc("Turn on Data Length Extension?", 0);
	display_print_line(">", 0, line_counter + pointer_position);
	
	display_print_line_inc("Yes.", TEXT_INLINE);
	display_print_line_inc("No.", TEXT_INLINE);
	
	display_show();
}

void display_draw_receiver_screen()
{
	display_clear();
	
	display_show();
}

void display_draw_test_run_screen(transfer_data_t *transfer_data)
{
	static float last_timems = 0;
	static uint16_t last_kB_transferred = 0;
	
	float throughput = 0;
	float timems = (float)(counter_get());
	if(timems != 0)
	{
		uint32_t sent_kbits = (transfer_data->kB_transfered - last_kB_transferred) * 8;
		throughput = (float)(sent_kbits * 100) / (timems - last_timems);
		
		last_kB_transferred = transfer_data->kB_transfered;
		last_timems = timems;
	}
	
	display_clear();
	
	display_print_line_inc("Transferring data:", 0);
	
	//print filled bar
	fb_rectangle(0, line_counter*TEXT_HEIGHT, TRANSFER_BAR_LENGTH, line_counter*2*TEXT_HEIGHT-1, FB_COLOR_BLACK);
	
	fb_bar(0, line_counter*TEXT_HEIGHT, (uint32_t)transfer_data->kB_transfered*TRANSFER_BAR_LENGTH / transfer_data->kb_transfer_size, line_counter*2*TEXT_HEIGHT-1, FB_COLOR_BLACK);
	line_counter++;
	
	char str[50];
	sprintf(str, "%dKB/%dKB transferred", transfer_data->kB_transfered, transfer_data->kb_transfer_size);
	display_print_line_inc(str, 0);
	
	//display_print_line("kbit/s", 150, line_counter);
	sprintf(str, "Speed: %.1f Kbits/s", throughput);
	display_print_line_inc(str, 0);
	
	display_show();
}

void display_draw_test_start_screen()
{
	display_clear();
	display_show();
}
