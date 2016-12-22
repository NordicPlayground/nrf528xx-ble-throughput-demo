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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "amt.h"
#include "counter.h"

#include "sdk_config.h"
#include "boards.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "nrf.h"
#include "ble.h"
#include "ble_hci.h"
#include "nordic_common.h"
#include "nrf_gpio.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "softdevice_handler.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "app_error.h"
#include "ble_conn_params.h"

#define NRF_LOG_MODULE_NAME "DEMO"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#include "display.h"

#define TIMER_PRESCALER         0                                   /**< Value of the RTC1 PRESCALER register. */
#define TIMER_OP_QUEUE_SIZE     4                                   /**< Size of timer operation queues. */

#define ATT_MTU_DEFAULT         247                                 /**< Default ATT MTU size, in bytes. */
#define CONN_INTERVAL_DEFAULT   MSEC_TO_UNITS(400, UNIT_1_25_MS)     /**< Default connection interval used at connection establishment by central side. */

#define CONN_INTERVAL_MIN       MSEC_TO_UNITS(7.5, UNIT_1_25_MS)    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX       MSEC_TO_UNITS(500, UNIT_1_25_MS)    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT        MSEC_TO_UNITS(4000,  UNIT_10_MS)    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY           0                                   /**< Slave latency. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(500, TIMER_PRESCALER)      /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (0.5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(500, TIMER_PRESCALER)      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (0.5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    2                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define LED_SCANNING_ADVERTISING  BSP_BOARD_LED_0
#define LED_READY                 BSP_BOARD_LED_1
#define LED_PROGRESS              BSP_BOARD_LED_2
//#define LED_FINISHED              BSP_BOARD_LED_3

#define BUTTON_UP				  BUTTON_1
#define BUTTON_DOWN				  BUTTON_3
#define BUTTON_SEL				  BUTTON_2
#define BUTTON_BACK				  BUTTON_4


#define BUTTON_DETECTION_DELAY    APP_TIMER_TICKS(50, TIMER_PRESCALER)       /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define LL_HEADER_LEN             4

#define SENSITIVITY_125KBPS	      (-103)
#define SENSITIVITY_1MBPS	      (-96)
#define SENSITIVITY_2MBPS	      (-93)

#define RSSI_MOVING_AVERAGE_ALPHA	0.5		//higher value will lower the frequency of the filter

#define DISPLAY_TIMER_UPDATE_INTERVAL	APP_TIMER_TICKS(200, TIMER_PRESCALER)
APP_TIMER_DEF(m_display_timer_id);

typedef enum
{
    NOT_SELECTED = 0x00,
    BOARD_TESTER,
    BOARD_DUMMY,
} board_role_t;

static void button_event_handler(uint8_t pin_no, uint8_t button_action);
	
static app_button_cfg_t buttons[] =
{
	{BUTTON_1, false, BUTTON_PULL, button_event_handler},
	{BUTTON_2,  false, BUTTON_PULL, button_event_handler},
	{BUTTON_3,  false, BUTTON_PULL, button_event_handler},
	{BUTTON_4,  false, BUTTON_PULL, button_event_handler}
};

static uint8_t m_button = 0xff;

static volatile bool 			m_display_show = false;
static volatile bool 			m_display_show_transfer_data = false;
static volatile bool			m_transfer_done = false;
static transfer_data_t			m_transfer_data = {.kb_transfer_size = (AMT_BYTE_TRANSFER_CNT_DEFAULT/1024), .bytes_transfered = 0};
static rssi_data_t				m_rssi_data;

/**@brief Variable length data encapsulation in terms of length and pointer to data. */
typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;

static nrf_ble_amtc_t m_amtc;
static nrf_ble_amts_t m_amts;

static bool volatile m_counter_started = false;
static bool volatile m_run_test;
static bool volatile m_print_menu;
static bool volatile m_notif_enabled;
static bool volatile m_mtu_exchanged;
static bool volatile m_phy_updated;
static bool volatile m_conn_interval_configured;
static bool volatile m_test_started = false;
static bool volatile m_test_continuous = false;

static board_role_t volatile m_board_role  = NOT_SELECTED;
static uint16_t              m_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Handle of the current BLE connection .*/
static uint8_t               m_gap_role    = BLE_GAP_ROLE_INVALID;    /**< BLE role for this connection, see @ref BLE_GAP_ROLES */

static nrf_ble_gatt_t     m_gatt;                /**< GATT module instance. */
static ble_db_discovery_t m_ble_db_discovery;    /**< Structure used to identify the DB Discovery module. */

/* Name to use for advertising and connection. */
static const char m_target_periph_name[] = DEVICE_NAME;

/* Test parameters. */
static test_params_t m_test_params =
{
    .att_mtu                  = ATT_MTU_DEFAULT,
    .conn_interval            = CONN_INTERVAL_DEFAULT,
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
	.rxtx_phy                 = BLE_GAP_PHY_2MBPS,
	.tx_power				  = 8,
};

/* Scan parameters requested for scanning and connection. */
static ble_gap_scan_params_t const m_scan_param =
{
    .active         = 0x00,
    .interval       = SCAN_INTERVAL,
    .window         = SCAN_WINDOW,
    .use_whitelist  = 0x00,
    .adv_dir_report = 0x00,
    .timeout        = 0x0000, // No timeout.
};

/* Connection parameters requested for connection. */
static ble_gap_conn_params_t m_conn_param =
{
    .min_conn_interval = (uint16_t)CONN_INTERVAL_MIN,   // Minimum connection interval.
    .max_conn_interval = (uint16_t)CONN_INTERVAL_MAX,   // Maximum connection interval.
    .slave_latency     = (uint16_t)SLAVE_LATENCY,       // Slave latency.
    .conn_sup_timeout  = (uint16_t)CONN_SUP_TIMEOUT     // Supervisory timeout.
};

static const test_params_t ble_5_HS_version_params =
{
    .att_mtu                  = 247,
    .conn_interval            = MSEC_TO_UNITS(400, UNIT_1_25_MS),
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
	.rxtx_phy                 = BLE_GAP_PHY_2MBPS,
	.tx_power				  = 8,	
};

static const test_params_t ble_5_LR_version_params =
{
    .att_mtu                  = 23,
    .conn_interval            = MSEC_TO_UNITS(7.5, UNIT_1_25_MS),
    .data_len_ext_enabled     = false,
    .conn_evt_len_ext_enabled = false,
	.rxtx_phy                 = BLE_GAP_PHY_CODED,
	.tx_power				  = 8,
};

static const test_params_t ble_4_2_version_params =
{
    .att_mtu                  = 247,
    .conn_interval            = MSEC_TO_UNITS(400, UNIT_1_25_MS),
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
	.rxtx_phy                 = BLE_GAP_PHY_1MBPS,
	.tx_power				  = 4,
};

static const test_params_t ble_4_1_version_params =
{
    .att_mtu                  = 23,
    .conn_interval            = MSEC_TO_UNITS(7.5, UNIT_1_25_MS),
    .data_len_ext_enabled     = false,
    .conn_evt_len_ext_enabled = false,
	.rxtx_phy                 = BLE_GAP_PHY_1MBPS,
	.tx_power				  = 4,
};

void advertising_start(void);
void scan_start(void);
void test_params_print(void);
void display_test_params_print(void);
uint8_t button_read(void);
void buttons_enable(void);
void buttons_disable(void);
static void wait_for_event(void);

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
    bsp_board_leds_init();
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timer_init(void)
{
    // Initialize timer module.
    APP_TIMER_INIT(TIMER_PRESCALER, TIMER_OP_QUEUE_SIZE, false);
}

/**@brief Function for initializing GAP parameters.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (uint8_t const*)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_ppcp_set(&m_conn_param);
    APP_ERROR_CHECK(err_code);

}

void test_run(bool wait_for_button)
{
	if(wait_for_button)
	{
		NRF_LOG_RAW_INFO("\r\n");
		NRF_LOG_INFO("Test is ready. Press any button to run.\r\n");
		NRF_LOG_FLUSH();
		
		display_print_line_inc("Test is ready. Press any button to run.");
		display_show();

		(void) button_read();
	}
	
	m_counter_started = false;
    nrf_ble_amts_notif_spam(&m_amts);
	
	m_test_started = true;
	app_timer_start(m_display_timer_id, DISPLAY_TIMER_UPDATE_INTERVAL, NULL);
}

void terminate_test(void)
{
    m_run_test                 = false;
    m_notif_enabled            = false;
    m_mtu_exchanged            = false;
	m_phy_updated              = false;
    m_conn_interval_configured = false;
	m_test_started				= false;
	
	app_timer_stop(m_display_timer_id);
	
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_INFO("Disconnecting.\r\n");
		
        ret_code_t ret = sd_ble_gap_disconnect(m_conn_handle,
            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (ret != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("Disconnected returned 0x%0x", ret);
        }
    }
    else
    {
        if (m_board_role == BOARD_TESTER)
        {
            m_print_menu = true;
        }
        if (m_board_role == BOARD_DUMMY)
        {
			advertising_start();
        }
    }
}


/**@brief AMT Service Handler.
 */
static void amts_evt_handler(nrf_ble_amts_evt_t evt)
{
    ret_code_t  err_code;
    switch (evt.evt_type)
    {
        case SERVICE_EVT_NOTIF_ENABLED:
        {
            bsp_board_led_on(LED_READY);
            NRF_LOG_INFO("Notifications enabled.\r\n");
			display_print_line_inc("Notifications enabled.");
			m_display_show = true;
			
            m_notif_enabled = true;
            if (m_board_role == BOARD_TESTER)
            {
                if (m_gap_role == BLE_GAP_ROLE_PERIPH)
                {
                    m_conn_interval_configured = false;
                    m_conn_param.min_conn_interval = m_test_params.conn_interval;
                    m_conn_param.max_conn_interval = m_test_params.conn_interval+1;
                    err_code = ble_conn_params_change_conn_params(&m_conn_param);
                    if (err_code != NRF_SUCCESS)
                    {
                        NRF_LOG_ERROR("sd_ble_gap_conn_param_update returned 0x%x.\r\n", err_code);
                    }
                }
                if (m_gap_role == BLE_GAP_ROLE_CENTRAL)
                {
                    m_conn_interval_configured     = true;
                    m_conn_param.min_conn_interval = m_test_params.conn_interval;
                    m_conn_param.max_conn_interval = m_test_params.conn_interval;
                    err_code = sd_ble_gap_conn_param_update(m_conn_handle,
                                                                       &m_conn_param);
                    if (err_code != NRF_SUCCESS)
                    {
                        NRF_LOG_ERROR("sd_ble_gap_conn_param_update returned 0x%x.\r\n", err_code);
                    }
                }
            }
        } break;

        case SERVICE_EVT_NOTIF_DISABLED:
            bsp_board_led_off(LED_READY);
            NRF_LOG_INFO("Notifications disabled.\r\n");
            break;

        case SERVICE_EVT_TRANSFER_1KB:
        {
            bsp_board_led_invert(LED_PROGRESS);
			
        } break;

        case SERVICE_EVT_TRANSFER_FINISHED:
        {
			counter_stop();
			
            bsp_board_led_off(LED_PROGRESS);
            //bsp_board_led_on(LED_FINISHED);
			
			uint32_t counter_ticks = counter_get();
			m_transfer_data.counter_ticks = counter_ticks;
			m_transfer_data.bytes_transfered = evt.bytes_transfered_cnt;
			
			float sent_octet_cnt = evt.bytes_transfered_cnt * 8;
			float throughput = (float)(sent_octet_cnt * 32768) / (float)counter_ticks;
			throughput = throughput / (float)1000;
			
            NRF_LOG_INFO("Done.\r\n\r\n");
            NRF_LOG_INFO("=============================\r\n");
            NRF_LOG_INFO("Time: " NRF_LOG_FLOAT_MARKER " seconds elapsed.\r\n",
                         NRF_LOG_FLOAT((float)counter_ticks / 32768));
            NRF_LOG_INFO("Throughput: " NRF_LOG_FLOAT_MARKER " Kbits/s.\r\n",
                         NRF_LOG_FLOAT(throughput));
            NRF_LOG_INFO("=============================\r\n");
            NRF_LOG_INFO("Sent %u bytes of ATT payload.\r\n", evt.bytes_transfered_cnt);
            NRF_LOG_INFO("Retrieving amount of bytes received from peer...\r\n");
			
			m_transfer_data.last_throughput = throughput;
			
			if(m_test_continuous)
			{
				m_run_test = false;
			}
			else
			{
				m_transfer_done = true;
				m_display_show_transfer_data = false;
				terminate_test();
			}
			
			/*
            err_code = nrf_ble_amtc_rcb_read(&m_amtc);
            if (err_code != NRF_SUCCESS)
            {
                NRF_LOG_ERROR("nrf_ble_amt_c_rcb_read returned 0x%x.\r\n", err_code);
                terminate_test();
            }
			*/

        } break;
    }
}


/**@brief AMT Client Handler.
 */
void amtc_evt_handler(nrf_ble_amtc_t * p_amt_c, nrf_ble_amtc_evt_t * p_evt)
{
    uint32_t err_code;

    switch (p_evt->evt_type)
    {
        case NRF_BLE_AMT_C_EVT_DISCOVERY_COMPLETE:
        {
            NRF_LOG_INFO("AMT service discovered on the peer.\r\n");
			
            err_code = nrf_ble_amtc_handles_assign(p_amt_c ,
                                                    p_evt->conn_handle,
                                                    &p_evt->params.peer_db);
            APP_ERROR_CHECK(err_code);

            // Enable notifications.
            err_code = nrf_ble_amtc_notif_enable(p_amt_c);
            APP_ERROR_CHECK(err_code);
        } break;

        case NRF_BLE_AMT_C_EVT_NOTIFICATION:
        {
            static uint32_t bytes_cnt  = 0;
            static uint32_t kbytes_cnt = 0;

            if (p_evt->params.hvx.bytes_sent == 0)
            {
                bytes_cnt  = 0;
                kbytes_cnt = 0;
            }

            bytes_cnt += p_evt->params.hvx.notif_len;

            if (bytes_cnt > 1024)
            {
                bsp_board_led_invert(LED_PROGRESS);

                bytes_cnt -= 1024;
                kbytes_cnt++;
								
				if((kbytes_cnt % 10) == 0)
				{
					NRF_LOG_INFO("Received %u kbytes\r\n", kbytes_cnt);
				}

                nrf_ble_amts_rbc_set(&m_amts, p_evt->params.hvx.bytes_rcvd);
            }

            NRF_LOG_DEBUG("AMT Notification bytes cnt %u\r\n", p_evt->params.hvx.bytes_sent);

            if (p_evt->params.hvx.bytes_rcvd >= amt_byte_transfer_count)
            {
                bsp_board_led_off(LED_PROGRESS);

                bytes_cnt  = 0;
                kbytes_cnt = 0;

                NRF_LOG_INFO("AMT Transfer complete, received %u bytes.\r\n",
                             p_evt->params.hvx.bytes_rcvd);

                nrf_ble_amts_rbc_set(&m_amts, p_evt->params.hvx.bytes_rcvd);
            }

        } break;

        case NRF_BLE_AMT_C_EVT_RBC_READ_RSP:
            NRF_LOG_INFO("AMT peer received %u bytes (%u KBytes).\r\n",
                         (p_evt->params.rcv_bytes_cnt), (p_evt->params.rcv_bytes_cnt / 1024));

            terminate_test();
            break;

        default:
            break;
    }
}


uint32_t phy_str(uint8_t phy)
{
    char const * phy_str[] =
    {
        "1 Mbps",
        "2 Mbps",
        "125 Kbps",
        "Unknown"
    };

    switch (phy)
    {
        case BLE_GAP_PHY_1MBPS:
            return (uint32_t)(phy_str[0]);

        case BLE_GAP_PHY_2MBPS:
            return (uint32_t)(phy_str[1]);

        case BLE_GAP_PHY_CODED:
            return (uint32_t)(phy_str[2]);

        default:
            return (uint32_t)(phy_str[3]);
    }
}

/**
 * @brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  Type of data to be looked for in advertisement data.
 * @param[in]  Advertisement report length and pointer to report.
 * @param[out] If data type requested is found in the data report, type data length and
 *             pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata)
{
    uint32_t  index = 0;
    uint8_t * p_data;

    p_data = p_advdata->p_data;

    while (index < p_advdata->data_len)
    {
        uint8_t field_length = p_data[index];
        uint8_t field_type   = p_data[index + 1];

        if (field_type == type)
        {
            p_typedata->p_data   = &p_data[index + 2];
            p_typedata->data_len = field_length - 1;
            return NRF_SUCCESS;
        }
        index += field_length + 1;
    }
    return NRF_ERROR_NOT_FOUND;
}


/**@brief Function for searching a given name in the advertisement packets.
 *
 * @details Use this function to parse received advertising data and to find a given
 * name in them either as 'complete_local_name' or as 'short_local_name'.
 *
 * @param[in]   p_adv_report   advertising data to parse.
 * @param[in]   name_to_find   name to search.
 * @return   true if the given name was found, false otherwise.
 */
static bool find_adv_name(const ble_gap_evt_adv_report_t * p_adv_report, char const * name_to_find)
{
    uint32_t err_code;
    data_t   adv_data;
    data_t   dev_name;
    bool     found = false;

    // Initialize advertisement report for parsing.
    adv_data.p_data     = (uint8_t *)p_adv_report->data;
    adv_data.data_len   = p_adv_report->dlen;

    // Search for matching advertising names.
    err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
                                &adv_data,
                                &dev_name);

    if (   (err_code == NRF_SUCCESS)
        && (strlen(name_to_find) == dev_name.data_len)
        && (memcmp(name_to_find, dev_name.p_data, dev_name.data_len) == 0))
    {
        found = true;
    }
    else
    {
        // Look for the short local name if the complete name was not found.
        err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
                                    &adv_data,
                                    &dev_name);

        if (   (err_code == NRF_SUCCESS)
            && (strlen(name_to_find) == dev_name.data_len)
            && (memcmp(m_target_periph_name, dev_name.p_data, dev_name.data_len) == 0))
        {
            found = true;
        }
    }

    return found;
}


/**@brief Function for handling BLE_GAP_EVT_CONNECTED events.
 * Save the connection handle and GAP role, then discover the peer DB.
 */
void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt)
{
    m_conn_handle = p_gap_evt->conn_handle;
    m_gap_role    = p_gap_evt->params.connected.role;

    if (m_gap_role == BLE_GAP_ROLE_PERIPH)
    {
        NRF_LOG_INFO("Connected as a peripheral.\r\n");
		display_print_line_inc("Connected as a peripheral.");
		m_display_show = true;
    }
    else if (m_gap_role == BLE_GAP_ROLE_CENTRAL)
    {
        NRF_LOG_INFO("Connected as a central.\r\n");
		display_print_line_inc("Connected as a central.");
		m_display_show = true;
    }

    // Stop scanning and advertising.
    (void) sd_ble_gap_scan_stop();
    (void) sd_ble_gap_adv_stop();

    NRF_LOG_INFO("Discovering GATT database...\r\n");

    // Zero the database before starting discovery.
    memset(&m_ble_db_discovery, 0x00, sizeof(m_ble_db_discovery));

    ret_code_t err_code;
    err_code  = ble_db_discovery_start(&m_ble_db_discovery, p_gap_evt->conn_handle);
    APP_ERROR_CHECK(err_code);

	
	if(m_board_role == BOARD_TESTER)
	{
		// Request PHY.
		ble_gap_phys_t phys =
		{
			.tx_phys = m_test_params.rxtx_phy,
			.rx_phys = m_test_params.rxtx_phy,
		};

		err_code = sd_ble_gap_phy_request(p_gap_evt->conn_handle, &phys);
		APP_ERROR_CHECK(err_code);
		
		err_code = sd_ble_gap_rssi_start(m_conn_handle, BLE_GAP_RSSI_THRESHOLD_INVALID, 0);
		APP_ERROR_CHECK(err_code);
	}
	
	
    bsp_board_led_off(LED_SCANNING_ADVERTISING);
	bsp_board_led_off(LED_READY);
	bsp_board_led_off(LED_PROGRESS);
}


/**@brief Function for handling BLE_GAP_EVT_DISCONNECTED events.
 * Unset the connection handle and terminate the test.
 */
void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt)
{
    m_conn_handle = BLE_CONN_HANDLE_INVALID;

    NRF_LOG_INFO("Disconnected (reason 0x%x).\r\n", p_gap_evt->params.disconnected.reason);

    if (m_run_test)
    {
        NRF_LOG_WARNING("GAP disconnection event received while test was running.\r\n")
    }

    bsp_board_led_off(LED_SCANNING_ADVERTISING);
	bsp_board_led_off(LED_READY);
	bsp_board_led_off(LED_PROGRESS);

    terminate_test();
}


/**@brief Function for handling BLE_GAP_ADV_REPORT events.
 * Search for a peer with matching device name.
 * If found, stop advertising and send a connection request to the peer.
 */
void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt)
{
    if (!find_adv_name(&p_gap_evt->params.adv_report, m_target_periph_name))
    {
        return;
    }

    NRF_LOG_INFO("Device \"%s\" with matching name found, sending a connection request.\r\n",
                 (uint32_t) m_target_periph_name);
	
	display_print_line_inc("Device with matching name found");
	m_display_show = true;

    // Stop advertising.
    (void) sd_ble_gap_adv_stop();

    // Initiate connection.
    m_conn_param.min_conn_interval = CONN_INTERVAL_DEFAULT;
    m_conn_param.max_conn_interval = CONN_INTERVAL_DEFAULT;
 	
    ret_code_t err_code;
    err_code = sd_ble_gap_connect(&p_gap_evt->params.adv_report.peer_addr,
                                    &m_scan_param,
                                    &m_conn_param);

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("sd_ble_gap_connect() failed: 0x%x.\r\n", err_code);
    }
}


/**@brief Function for handling BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t              err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
            on_ble_gap_evt_adv_report(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONNECTED:
            on_ble_gap_evt_connected(p_gap_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_ble_gap_evt_disconnected(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            ble_gap_conn_params_t params;
            // Accept parameters requested by the peer.
            params = p_gap_evt->params.conn_param_update_request.conn_params;
            params.max_conn_interval = params.min_conn_interval;
            err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle, &params);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            err_code = sd_ble_gatts_sys_attr_set(p_gap_evt->conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
        case BLE_GATTS_EVT_TIMEOUT:
            NRF_LOG_DEBUG("GATT timeout, disconnecting.\r\n");
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break;

		case BLE_GAP_EVT_PHY_UPDATE:
        {
			ble_gap_evt_phy_update_t phy_update;
			phy_update = p_gap_evt->params.phy_update;
            
			switch(phy_update.tx_phy)
			{
				case BLE_GAP_PHY_2MBPS:
					NRF_LOG_INFO("PHY updated to 2Mbps\r\n");
					display_print_line_inc("PHY updated to 2Mbps");
					break;
				case BLE_GAP_PHY_1MBPS:
					NRF_LOG_INFO("PHY updated to 1Mbps\r\n");
					display_print_line_inc("PHY updated to 1Mbps");
					break;
				case BLE_GAP_PHY_CODED:
					NRF_LOG_INFO("PHY updated to 125Kbps\r\n");
					display_print_line_inc("PHY updated to 125Kbps");
					break;
				default:
					NRF_LOG_INFO("PHY updated to unknown\r\n");
					display_print_line_inc("PHY updated to unknown");
					break;
			}
			
			m_display_show = true;
            m_phy_updated = true;
        } break;
		
		case BLE_EVT_TX_COMPLETE:
			if(!m_counter_started)
			{
				NRF_LOG_INFO("Counter started\r\n");
				counter_start();
				m_counter_started = true;
			}
			break;
			
        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, p_ble_evt);
    nrf_ble_gatt_on_ble_evt(&m_gatt, p_ble_evt);
    nrf_ble_amts_on_ble_evt(&m_amts, p_ble_evt);
    nrf_ble_amtc_on_ble_evt(&m_amtc, p_ble_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler library.
    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    // Retrieve default BLE stack configuration parameters.
    ble_enable_params_t ble_enable_params;
    (void) softdevice_enable_get_default_config(NRF_BLE_CENTRAL_LINK_COUNT,
                                                NRF_BLE_PERIPHERAL_LINK_COUNT,
                                                &ble_enable_params);

    // Manually override the default ATT MTU size.
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_GATT_MAX_MTU_SIZE;

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register a BLE event handler with the SoftDevice handler library.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for setting up advertising data.
 */
static void advertising_data_set(void)
{
    uint32_t err_code;
    ble_advdata_t const adv_data =
    {
        .name_type          = BLE_ADVDATA_FULL_NAME,
        .flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        .include_appearance = false,
    };

    err_code = ble_advdata_set(&adv_data, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
void advertising_start(void)
{
    ble_gap_adv_params_t const adv_params =
    {
        .type        = BLE_GAP_ADV_TYPE_ADV_IND,
        .p_peer_addr = NULL,
        .fp          = BLE_GAP_ADV_FP_ANY,
        .interval    = ADV_INTERVAL,
        .timeout     = 0,
    };

    NRF_LOG_INFO("Starting advertising.\r\n");
	
	display_print_line_inc("Starting advertising.");
	m_display_show = true;

    uint32_t err_code;
    err_code = sd_ble_gap_adv_start(&adv_params);
    APP_ERROR_CHECK(err_code);

    bsp_board_led_on(LED_SCANNING_ADVERTISING);
}


/**@brief Function to start scanning.
 */
void scan_start(void)
{
    NRF_LOG_INFO("Starting scan.\r\n");
	
	display_print_line_inc("Starting scan.");
	m_display_show = true;

    ret_code_t err_code;
    err_code = sd_ble_gap_scan_start(&m_scan_param);
    APP_ERROR_CHECK(err_code);

    bsp_board_led_on(LED_SCANNING_ADVERTISING);
}

/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
	if(button_action == APP_BUTTON_PUSH)
	{
		if(m_test_started)
		{
			counter_stop();
			terminate_test();
		}
		m_button = pin_no;
	}
}

/**@brief Function for initializing the button library.
 */
static void buttons_init(app_button_cfg_t *btns)
{
    uint32_t err_code;
	
    err_code = app_button_init(btns, ARRAY_SIZE(buttons), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for enabling button input.
 */
static void buttons_enable(void)
{
    ret_code_t err_code;
    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}

uint8_t button_read(void)
{
	m_button = 0xff;
    //buttons_enable();

    while (m_button == 0xff)
    {
        if (!NRF_LOG_PROCESS())
        {
            wait_for_event();
        }
    }

    //buttons_disable();
	return m_button;
}

static void wait_for_event(void)
{
    (void) sd_app_evt_wait();
}

/**@brief Function for handling a Connection Parameters event.
 *
 * @param[in] p_evt  event.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED)
    {
        m_conn_interval_configured = true;
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t err_code;

    ble_conn_params_init_t const connection_params_init =
    {
        .p_conn_params                  = NULL,
        .first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY,
        .next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY,
        .max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT,
        .disconnect_on_fail             = true,
        .evt_handler                    = on_conn_params_evt,
        .error_handler                  = conn_params_error_handler,
    };

    err_code = ble_conn_params_init(&connection_params_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t * p_evt)
{
    NRF_LOG_INFO("ATT MTU exchange completed.\r\n");
	display_print_line_inc("ATT MTU exchange completed.");
	m_display_show = true;
    m_mtu_exchanged = true;
    nrf_ble_amts_on_gatt_evt(&m_amts, p_evt);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Database Discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function should forward the events
 *          to their respective service instances.
 *
 * @param[in] p_event  Pointer to the database discovery event.
 */
static void db_disc_handler(ble_db_discovery_evt_t * p_evt)
{
    nrf_ble_amtc_on_db_disc_evt(&m_amtc, p_evt);
}


void client_init(void)
{
    ret_code_t err_code = ble_db_discovery_init(db_disc_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_amtc_init(&m_amtc, amtc_evt_handler);
    APP_ERROR_CHECK(err_code);
}


void server_init(void)
{
    nrf_ble_amts_init(&m_amts, amts_evt_handler);
}


void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
}

void preferred_phy_set(uint8_t phy)
{
    ble_opt_t opts =
    {
        .gap_opt.preferred_phys.tx_phys = phy,
        .gap_opt.preferred_phys.rx_phys = phy,
    };

    ret_code_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PREFERRED_PHYS_SET, &opts);
    NRF_LOG_DEBUG("Setting preferred phy (rxtx) to %u: 0x%x\r\n", phy, err_code);
    APP_ERROR_CHECK(err_code);
}

void gatt_mtu_set(uint16_t att_mtu)
{
    ret_code_t err_code;
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);
}


void conn_evt_len_ext_set(bool status)
{
    ret_code_t err_code;
    ble_opt_t  opt;

    memset(&opt, 0x00, sizeof(opt));
    opt.common_opt.conn_evt_ext.enable = status ? 1 : 0;

    err_code = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
    APP_ERROR_CHECK(err_code);
	
	if(m_board_role == BOARD_TESTER)
	{		
		memset(&opt, 0x00, sizeof(opt));
		
		opt.common_opt.conn_bw.role = BLE_GAP_ROLE_CENTRAL;
		
		if(status == true)
		{
			opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
			opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
		}
		else
		{
			opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_LOW;
			opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_LOW;
		}

		err_code = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &opt);
		APP_ERROR_CHECK(err_code);
	}
	NRF_LOG_INFO("Setting connection event extension to %d\r\n", opt.common_opt.conn_evt_ext.enable);
}


void data_len_ext_set(bool status)
{
    ret_code_t err_code;
    ble_opt_t  opt;

	uint16_t pdu_size = status ?
        (m_test_params.att_mtu + LL_HEADER_LEN) : BLE_GATT_MTU_SIZE_DEFAULT + LL_HEADER_LEN;
	
	if(pdu_size > 251)
	{
		pdu_size = 251;
	}
	
    memset(&opt, 0x00, sizeof(opt));
    opt.gap_opt.ext_len.rxtx_max_pdu_payload_size = pdu_size;

    err_code = sd_ble_opt_set(BLE_GAP_OPT_EXT_LEN, &opt);
    NRF_LOG_DEBUG("Setting DLE to %u\r\n", pdu_size);
    APP_ERROR_CHECK(err_code);
}

void update_link_budget()
{
	int8_t sensitivity;
	
	switch(m_test_params.rxtx_phy)
	{
		case BLE_GAP_PHY_2MBPS:
			sensitivity = SENSITIVITY_2MBPS;
			break;
		case BLE_GAP_PHY_1MBPS:
			sensitivity = SENSITIVITY_1MBPS;
			break;
		case BLE_GAP_PHY_CODED:
			sensitivity = SENSITIVITY_125KBPS;
			break;
		default:
			APP_ERROR_CHECK(NRF_ERROR_INVALID_STATE);
			break;
	}
	
	m_test_params.link_budget = m_test_params.tx_power - sensitivity;
}

void tx_power_set(int8_t tx_power)
{
	uint32_t err_code;
	err_code = sd_ble_gap_tx_power_set(tx_power);
	APP_ERROR_CHECK(err_code);
	
	update_link_budget();
}

void set_all_parameters()
{
	gatt_mtu_set(m_test_params.att_mtu);
    data_len_ext_set(m_test_params.data_len_ext_enabled);
    conn_evt_len_ext_set(m_test_params.conn_evt_len_ext_enabled);
    preferred_phy_set(m_test_params.rxtx_phy);
	tx_power_set(m_test_params.tx_power);
}


void test_begin(void)
{
    NRF_LOG_INFO("Preparing the test.\r\n");
    NRF_LOG_FLUSH();
	
	display_clear();

    if (m_board_role == BOARD_TESTER)
    {
        scan_start();
    }
    if (m_board_role == BOARD_DUMMY)
    {
        advertising_start();
    }
}

typedef void (*handler_t)(uint32_t option_index);

typedef struct
{
	char *title;
	handler_t callback;
	void *next;
} menu_option_t;

typedef struct
{
	uint8_t nr_of_options;
	void *prev;
	menu_option_t *options;
	bool show_values;
	uint8_t index;
} menu_page_t;

void test_func(uint32_t option_index)
{
	
}

menu_page_t menu_main_page;

//BLE VERSION OPTIONS

menu_option_t menu_version_options[4] = 
{
	{"BLE 5 HS", test_func, &menu_main_page},
	{"BLE 5 LR", test_func, &menu_main_page},
	{"BLE 4.2", test_func, &menu_main_page},
	{"BLE 4.1", test_func, &menu_main_page},
};

menu_page_t menu_version_page = {4, &menu_main_page, menu_version_options, false};

//PHY OPTIONS

menu_option_t menu_phy_options[3] = 
{
	{"2 Mbps (High Speed)", test_func, &menu_main_page},
	{"1 Mbps", test_func, &menu_main_page},
	{"125 Kbps (long range)", test_func, &menu_main_page},
};

menu_page_t menu_phy_page = {3, &menu_main_page, menu_phy_options, false};

//CONNECTION INTERVAL OPTIONS

float conn_int_options[3] = {7.5, 50, 400};

menu_option_t menu_conn_int_options[3] = 
{
	{"7.5 ms", test_func, &menu_main_page},
	{"50 ms", test_func, &menu_main_page},
	{"400 ms", test_func, &menu_main_page},
};

menu_page_t menu_conn_int_page = {3, &menu_main_page, menu_conn_int_options, false};

//ATT MTU SIZE OPTIONS

uint32_t att_mtu_options[3] = {23, 158, 247};

menu_option_t menu_att_mtu_options[3] = 
{
	{"23 bytes", test_func, &menu_main_page},
	{"158 bytes", test_func, &menu_main_page},
	{"247 bytes", test_func, &menu_main_page},
};

menu_page_t menu_att_mtu_page = {3, &menu_main_page, menu_att_mtu_options, false};

//DATA LENGTH EXTENSION OPTIONS

bool dle_option;

menu_option_t menu_dle_options[2] = 
{
	{"ON", test_func, &menu_main_page},
	{"OFF", test_func, &menu_main_page},
};

menu_page_t menu_dle_page = {2, &menu_main_page, menu_dle_options, false};

//CONNECTION EVENT LENGTH EXTENSION OPTIONS

bool conn_evt_ext_option;

menu_option_t menu_conn_evt_ext_options[2] = 
{
	{"ON", test_func, &menu_main_page},
	{"OFF", test_func, &menu_main_page},
};

menu_page_t menu_conn_evt_ext_page = {2, &menu_main_page, menu_conn_evt_ext_options, false};

//TX POWER OPTIONS

uint32_t tx_power_options[3] = {0, 4, 8};

menu_option_t menu_tx_power_options[3] = 
{
	{"0 dBm", test_func, &menu_main_page},
	{"4 dBm", test_func, &menu_main_page},
	{"8 dBm", test_func, &menu_main_page},
};

menu_page_t menu_tx_power_page = {3, &menu_main_page, menu_tx_power_options, false};

//TRANSFER DATA SIZE OPTIONS

uint32_t transfer_data_size_options[3] = {100, 500, 1024};

menu_option_t menu_transfer_data_size_options[3] = 
{
	{"100 KB", test_func, &menu_main_page},
	{"500 KB", test_func, &menu_main_page},
	{"1 MB", test_func, &menu_main_page},
};

menu_page_t menu_transfer_data_size_page = {3, &menu_main_page, menu_transfer_data_size_options, false};

//LINK BUDGET OPTIONS

//TODO
uint32_t link_budget_options = 101;

menu_option_t menu_link_budget_options = {"101 dBm", test_func, NULL};

menu_page_t menu_link_budget_page = {1, &menu_main_page, &menu_link_budget_options, false};

//MAIN MENU OPTIONS

menu_option_t main_options[11] = 
{
	{"Run single transfer", test_func, NULL},
	{"Run cont. transfer", test_func, NULL},
	{"BLE version", test_func, &menu_version_page},
	{"Preferred PHY", test_func, &menu_phy_page},
	{"Conn. interval", test_func, &menu_conn_int_page},
	{"ATT MTU size", test_func, &menu_att_mtu_page},
	{"Data length ext", test_func, &menu_dle_page},
	{"Conn evt ext", test_func, &menu_conn_evt_ext_page},
	{"Tx power", test_func, &menu_tx_power_page},
	{"Transfer data size", test_func, &menu_transfer_data_size_page},
	{"Link budget", test_func, &menu_link_budget_page},
};

menu_page_t menu_main_page = {11, NULL, main_options, true};

menu_page_t *m_menu_current_page;

void menu_print()
{
	static const uint16_t number_pos = 220;
	static const uint16_t text_pos = 20;
	static uint8_t max_lines = MAX_LINES - 2;

	uint8_t max_index = m_menu_current_page->nr_of_options;
	uint8_t opt_index = m_menu_current_page->index;
	int8_t line_index;
	uint8_t pointer_pos;
	
	while(1)
	{
		//start scrolling if opt_index is larger than the max lines
		if(opt_index > (max_lines - 1))
		{
			line_index =  max_lines - 1 - opt_index;
			pointer_pos = max_lines - 1;
		}
		else
		{
			line_index = 0;
			pointer_pos = opt_index;
		}
		
		display_clear();

		display_print_line("[Btn1: UP, Btn2: DOWN, Btn3: Sel, Btn4: Back]", 0, max_lines+1);
		display_print_line("->", 0, pointer_pos);
		
		for(int8_t i = 0; i < max_index; i++)
		{
			if((i+line_index) <= max_lines)
			{
				display_print_line(m_menu_current_page->options[i].title, text_pos, i + line_index);
				if(m_menu_current_page->show_values)
				{
					menu_page_t *next_page = m_menu_current_page->options[i].next;
					if(next_page != NULL)
					{
						//TODO change the index here according to what is the parameter set
						display_print_line(next_page->options[0].title, number_pos, i + line_index);
					}
				}
			}
		}
		
		display_show();
		
		switch (button_read())
		{
			case BUTTON_DOWN:
				if(opt_index < (max_index-1))
				{
					opt_index++;
					if(pointer_pos < max_lines)
					{
						pointer_pos++;
					}
				}
				break;

			case BUTTON_UP:
				if(opt_index != 0)
				{
					opt_index--;
				}
				
				if(pointer_pos != 0)
				{
					pointer_pos--;
				}
				break;
			
			case BUTTON_SEL:
				m_menu_current_page->index = opt_index;
			
				if(m_menu_current_page->options[opt_index].callback != NULL)
				{
					m_menu_current_page->options[opt_index].callback(pointer_pos);
				}
				if(m_menu_current_page->options[opt_index].next != NULL)
				{
					m_menu_current_page = m_menu_current_page->options[opt_index].next;
				}
				return;
				break;
			
			case BUTTON_BACK:
				if(m_menu_current_page->prev != NULL)
				{
					m_menu_current_page = m_menu_current_page->prev;
					return;
				}
				break;
		}
	}
}


static bool is_test_ready()
{
    if (   (m_board_role == BOARD_TESTER)
        && m_conn_interval_configured
        && m_notif_enabled
        && m_mtu_exchanged
        && m_phy_updated
        && !m_run_test)
    {
        return true;
    }
    return false;
}

static void display_timer_handler(void *p_context)
{
	m_transfer_data.counter_ticks = counter_get();
	m_transfer_data.bytes_transfered = m_amts.bytes_sent;
	m_display_show_transfer_data = true;
	
	int8_t rssi;
	uint32_t err_code;
	
	err_code = sd_ble_gap_rssi_get(m_conn_handle, &rssi);
	if(err_code == NRF_SUCCESS)
	{
		if(m_rssi_data.nr_of_samples == 0)
		{
			m_rssi_data.moving_average = rssi;
		}
		else
		{
			m_rssi_data.moving_average = 
					(float)m_rssi_data.moving_average * RSSI_MOVING_AVERAGE_ALPHA
					+ (float)rssi * (1.0 - RSSI_MOVING_AVERAGE_ALPHA);
		}
		
		m_rssi_data.sum += rssi;
		m_rssi_data.nr_of_samples++;
		m_rssi_data.current_rssi = rssi;
		//check in case the RSSI value magically drops below the spec
		if( (-m_rssi_data.moving_average) > (m_test_params.link_budget - m_test_params.tx_power))
		{
			m_rssi_data.link_budget = 0;
		}
		else
		{
			m_rssi_data.link_budget = m_test_params.link_budget - m_test_params.tx_power + m_rssi_data.moving_average;
		}
		
		m_rssi_data.range_multiplier = pow(10.0, (double)m_rssi_data.link_budget/20.0);
	}
}

int main(void)
{
	m_menu_current_page = &menu_main_page;
	
    log_init();
	
	display_init();
	
    leds_init();
    timer_init();
    counter_init();
	
	uint32_t err_code;
	err_code = app_timer_create(&m_display_timer_id, APP_TIMER_MODE_REPEATED, display_timer_handler);
	APP_ERROR_CHECK(err_code);
	
	buttons_init(buttons);
	buttons_enable();
	
    ble_stack_init();
	
	sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	
    gap_params_init();
    conn_params_init();
    gatt_init();
    advertising_data_set();

    server_init();
    client_init();

    gatt_mtu_set(m_test_params.att_mtu);
    data_len_ext_set(m_test_params.data_len_ext_enabled);
    conn_evt_len_ext_set(m_test_params.conn_evt_len_ext_enabled);
	
	tx_power_set(m_test_params.tx_power);
	
    NRF_LOG_INFO("ATT MTU example started.\r\n");
    NRF_LOG_INFO("Press button 1 on the board connected to the PC.\r\n");
    NRF_LOG_INFO("Press button 2 on other board.\r\n");
    NRF_LOG_FLUSH();
	
	display_clear();
    display_print_line_inc("- Press button 1 on this board");
    display_print_line_inc("- Press button 2 on other board.");
	display_show();

    uint8_t button = button_read();

	if(button == BUTTON_1)
	{
		m_board_role = BOARD_TESTER;
		preferred_phy_set(m_test_params.rxtx_phy);
		m_print_menu = true;
	}
	else
	{
		m_board_role = BOARD_DUMMY;
		preferred_phy_set(BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED);
		advertising_start();
	}

    for (;;)
    {
		if(m_transfer_done)
		{
			display_test_done_screen(&m_transfer_data, &m_rssi_data);
			
			button_read();
			m_transfer_done = false;
		}
		
        if (m_print_menu && !m_transfer_done)
        {
            menu_print();
        }

        if (is_test_ready())
        {
            m_run_test = true;
            test_run(!m_test_continuous);
        }

		if(m_display_show_transfer_data)
		{
			display_draw_test_run_screen(&m_transfer_data, &m_rssi_data);
			m_display_show_transfer_data = false;
		}
		
		if(m_display_show)
		{
			display_show();
			m_display_show = false;
		}
		
        if (!NRF_LOG_PROCESS())
        {
            wait_for_event();
        }
    }
}


/**
 * @}
 */
