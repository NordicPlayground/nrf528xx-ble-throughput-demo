
#include <string.h>

#include "display.h"
#include "menu.h"
#include "ble_gap.h"
#include "nrf_log.h"

typedef enum
{
	BOOL,
	INT8_T,
	UINT8_T,
	UINT16_T,
	FLOAT,
	STRING,
	PHY_T,
} type_t;

typedef void (*handler_t)(uint32_t option_index);

//TODO: make pointers point to const values
typedef struct
{
	uint8_t nr_of_options;
	void *prev;
	void *option_values;
	void *option_current_value;
	type_t option_type;
	char *option_unit;
	bool show_values;
	uint8_t index;
	handler_t callback;
	void *next_pages;
} menu_page_t;

static test_params_t m_test_params =
{
    .att_mtu                  	= 247,
    .conn_interval            	= 400.0f,
    .data_len_ext_enabled     	= true,
    .conn_evt_len_ext_enabled	= true,
	.rxtx_phy                 	= BLE_GAP_PHY_2MBPS,
#if defined(S132)
	.tx_power				  	= 4,
#elif defined(S140)
	.tx_power				  	= 8,
#endif
	.ble_version			  	= "BLE 5 High Speed",
	.transfer_data_size			= 1024,
	.link_budget				= 100,
};

static const test_params_t ble_5_HS_version_params =
{
    .att_mtu                  	= 247,
    .conn_interval            	= 400.0f,
    .data_len_ext_enabled     	= true,
    .conn_evt_len_ext_enabled 	= true,
	.rxtx_phy                 	= BLE_GAP_PHY_2MBPS,
#if defined(S132)
	.tx_power				  	= 4,
#elif defined(S140)
	.tx_power				  	= 8,
#endif
	.ble_version			  	= "BLE 5 High Speed",
	.transfer_data_size			= 1024,
	.link_budget				= 100,
};

#if defined(S140)
static const test_params_t ble_5_LR_version_params =
{
    .att_mtu                  	= 23,
    .conn_interval            	= 7.5f,
    .data_len_ext_enabled     	= false,
    .conn_evt_len_ext_enabled 	= false,
	.rxtx_phy                 	= BLE_GAP_PHY_CODED,
	.tx_power				  	= 8,
	.ble_version			  	= "BLE 5 Long Range",
	.transfer_data_size			= 100,
	.link_budget				= 111,
};
#endif

static const test_params_t ble_4_2_version_params =
{
    .att_mtu                  	= 247,
    .conn_interval            	= 400.0f,
    .data_len_ext_enabled     	= true,
    .conn_evt_len_ext_enabled 	= true,
	.rxtx_phy                 	= BLE_GAP_PHY_1MBPS,
	.tx_power				  	= 4,
	.ble_version			  	= "BLE 4.2",
	.transfer_data_size			= 512,
	.link_budget				= 100,
};

static const test_params_t ble_4_1_version_params =
{
    .att_mtu                  	= 23,
    .conn_interval            	= 7.5f,
    .data_len_ext_enabled     	= false,
    .conn_evt_len_ext_enabled 	= false,
	.rxtx_phy                 	= BLE_GAP_PHY_1MBPS,
	.tx_power				  	= 4,
	.ble_version			  	= "BLE 4.1",
	.transfer_data_size			= 100,
	.link_budget				= 100,
};

menu_page_t menu_main_page;

void get_test_params(test_params_t *params)
{
	memcpy(params, &m_test_params, sizeof(test_params_t));
}

//BLE VERSION

#define BLE_VERSION_OPTIONS_SIZE 4

#if defined(S132)
char *ble_version_options[BLE_VERSION_OPTIONS_SIZE] = {"BLE 5 High Speed", "BLE 4.2", "BLE 4.1"};
#elif defined(S140)
char *ble_version_options[BLE_VERSION_OPTIONS_SIZE] = {"BLE 5 High Speed", "BLE 5 Long Range", "BLE 4.2", "BLE 4.1"};
#endif

void menu_ble_version_func(uint32_t option_index)
{
	switch(option_index)
	{
		case 0:
			memcpy(&m_test_params, &ble_5_HS_version_params, sizeof(test_params_t));
			break;
#if defined(S132)
		case 1:
			memcpy(&m_test_params, &ble_4_2_version_params, sizeof(test_params_t));
			break;
		case 2:
			memcpy(&m_test_params, &ble_4_1_version_params, sizeof(test_params_t));
			break;
#elif defined(S140)
		case 1:
			memcpy(&m_test_params, &ble_5_LR_version_params, sizeof(test_params_t));
			break;
		case 2:
			memcpy(&m_test_params, &ble_4_2_version_params, sizeof(test_params_t));
			break;
		case 3:
			memcpy(&m_test_params, &ble_4_1_version_params, sizeof(test_params_t));
			break;
#endif
	}
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_ble_version_page = 
{
	.nr_of_options			= BLE_VERSION_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= ble_version_options,
	.option_current_value	= &m_test_params.ble_version,
	.option_type			= STRING,
	.option_unit			= "",
	.show_values			= false,
	.index					= 0,
	.callback				= menu_ble_version_func,
	.next_pages				= NULL,
};

//PREFERRED PHY

#if defined(S132)
#define PHY_OPTIONS_SIZE 2
uint8_t phy_options[PHY_OPTIONS_SIZE] = {BLE_GAP_PHY_2MBPS, BLE_GAP_PHY_1MBPS};

#elif defined(S140)
#define PHY_OPTIONS_SIZE 3
uint8_t phy_options[PHY_OPTIONS_SIZE] = {BLE_GAP_PHY_2MBPS, BLE_GAP_PHY_1MBPS, BLE_GAP_PHY_CODED};

#endif

void menu_phy_func(uint32_t option_index)
{
	m_test_params.rxtx_phy = phy_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_phy_page = 
{
	.nr_of_options			= PHY_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= phy_options,
	.option_current_value	= &m_test_params.rxtx_phy,
	.option_type			= PHY_T,
	.option_unit			= "",
	.show_values			= false,
	.index					= 0,
	.callback				= menu_phy_func,
	.next_pages				= NULL,
};

//CONNECTION INTERVAL

#define CONN_INT_OPTIONS_SIZE 3

float conn_int_options[CONN_INT_OPTIONS_SIZE] = {7.5f, 50.0f, 400.0f};

void menu_conn_int_func(uint32_t option_index)
{
	m_test_params.conn_interval = conn_int_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_conn_int_page = 
{
	.nr_of_options			= CONN_INT_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= conn_int_options,
	.option_current_value	= &m_test_params.conn_interval,
	.option_type			= FLOAT,
	.option_unit			= "ms",
	.show_values			= false,
	.index					= 2,
	.callback				= menu_conn_int_func,
	.next_pages				= NULL,
};

//ATT MTU SIZE_MAX

#define ATT_MTU_OPTIONS_SIZE 3

uint16_t att_mtu_options[ATT_MTU_OPTIONS_SIZE] = {23, 158, 247};

void menu_att_mtu_func(uint32_t option_index)
{
	m_test_params.att_mtu = att_mtu_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_att_mtu_page = 
{
	.nr_of_options			= ATT_MTU_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= att_mtu_options,
	.option_current_value	= &m_test_params.att_mtu,
	.option_type			= UINT16_T,
	.option_unit			= "bytes",
	.show_values			= false,
	.index					= 2,
	.callback				= menu_att_mtu_func,
	.next_pages				= NULL,
};


//DATA LENGTH EXTENSION

#define DATA_LENGTH_EXT_OPTIONS_SIZE 2

bool data_length_ext_options[DATA_LENGTH_EXT_OPTIONS_SIZE] = {true, false};

void menu_data_length_ext_func(uint32_t option_index)
{
	m_test_params.data_len_ext_enabled = data_length_ext_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_data_length_ext_page = 
{
	.nr_of_options			= DATA_LENGTH_EXT_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= data_length_ext_options,
	.option_current_value	= &m_test_params.data_len_ext_enabled,
	.option_type			= BOOL,
	.option_unit			= "",
	.show_values			= false,
	.index					= 0,
	.callback				= menu_data_length_ext_func,
	.next_pages				= NULL,
};

//CONNECTION EVENT LENGHT EXTENSION

#define CONN_EVT_LENGTH_EXT_OPTIONS_SIZE 2

bool conn_evt_length_ext_options[CONN_EVT_LENGTH_EXT_OPTIONS_SIZE] = {true, false};

void menu_conn_evt_length_ext_func(uint32_t option_index)
{
	m_test_params.conn_evt_len_ext_enabled = conn_evt_length_ext_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_conn_evt_length_ext_page = 
{
	.nr_of_options			= CONN_EVT_LENGTH_EXT_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= conn_evt_length_ext_options,
	.option_current_value	= &m_test_params.conn_evt_len_ext_enabled,
	.option_type			= BOOL,
	.option_unit			= "",
	.show_values			= false,
	.index					= 0,
	.callback				= menu_conn_evt_length_ext_func,
	.next_pages				= NULL,
};

//TX POWER

#if defined(S132)
#define TX_POWER_OPTIONS_SIZE 2
int8_t tx_power_options[TX_POWER_OPTIONS_SIZE] = {0, 4};

#elif defined(S140)
#define TX_POWER_OPTIONS_SIZE 3
int8_t tx_power_options[TX_POWER_OPTIONS_SIZE] = {0, 4, 8};

#endif

void menu_tx_power_func(uint32_t option_index)
{
	m_test_params.tx_power = tx_power_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_tx_power_page = 
{
	.nr_of_options			= TX_POWER_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= tx_power_options,
	.option_current_value	= &m_test_params.tx_power,
	.option_type			= INT8_T,
	.option_unit			= "dBm",
	.show_values			= false,
#if defined(S132)
	.index					= 1,
#elif defined(S140)
	.index					= 2,
#endif
	.callback				= menu_tx_power_func,
	.next_pages				= NULL,
};

//TRANSFER DATA SIZE

#define TRANSFER_DATA_SIZE_OPTIONS_SIZE 3

uint16_t transfer_data_size_options[TRANSFER_DATA_SIZE_OPTIONS_SIZE] = {1024, 512, 100};

void menu_transfer_data_size_func(uint32_t option_index)
{
	m_test_params.transfer_data_size = transfer_data_size_options[option_index];
	
	set_all_parameters(&m_test_params);
}

menu_page_t menu_transfer_data_size_page = 
{
	.nr_of_options			= TRANSFER_DATA_SIZE_OPTIONS_SIZE,
	.prev 					= &menu_main_page,
	.option_values			= transfer_data_size_options,
	.option_current_value	= &m_test_params.transfer_data_size,
	.option_type			= UINT16_T,
	.option_unit			= "Kbytes",
	.show_values			= false,
	.index					= 0,
	.callback				= menu_transfer_data_size_func,
	.next_pages				= NULL,
};

//LINK BUDGET

menu_page_t menu_link_budget_page = 
{
	.option_current_value	= &m_test_params.link_budget,
	.option_type			= UINT8_T,
	.option_unit			= "dBm",
};

//MAIN PAGE

#define MAIN_OPTIONS_SIZE 11

char *main_options[MAIN_OPTIONS_SIZE] = 
{
	"Run single transfer",
	"Run cont. transfer",
	"BLE version",
	"Preferred PHY",
	"Conn. interval",
	"ATT MTU size",
	"Data length ext",
	"Conn evt ext",
	"Tx power",
	"Transfer data size",
	"Link budget",
};

menu_page_t *main_next_pages[MAIN_OPTIONS_SIZE] =
{
	NULL,
	NULL,
	&menu_ble_version_page,
	&menu_phy_page,
	&menu_conn_int_page,
	&menu_att_mtu_page,
	&menu_data_length_ext_page,
	&menu_conn_evt_length_ext_page,
	&menu_tx_power_page,
	&menu_transfer_data_size_page,
	&menu_link_budget_page,
};

void menu_main_func(uint32_t option_index)
{
	if(option_index == 0)
	{
		test_begin(false);
	}
	else if(option_index == 1)
	{
		test_begin(true);
	}
}

menu_page_t menu_main_page = 
{
	.nr_of_options			= MAIN_OPTIONS_SIZE,
	.prev 					= NULL,
	.option_values			= main_options,
	.option_current_value	= NULL,
	.option_type			= STRING,
	.option_unit			= "",
	.show_values			= true,
	.index					= 0,
	.callback				= menu_main_func,
	.next_pages				= main_next_pages,
};

void print_var(void *array, uint8_t index, type_t type, char *unit, uint32_t x_pos, uint8_t line_nr, bool terminal)
{
	static char str[30];
	
	bool *var_array_b;
	int8_t *var_array_i8;
	uint8_t *var_array_u8;
	uint16_t *var_array_u16;
	float *var_array_f;
	char **var_array_s;
	
	switch(type)
	{
		case BOOL:
			var_array_b = array;
			if(var_array_b[index])
			{
				sprintf(str, "ON");
			}
			else
			{
				sprintf(str, "OFF");
			}
			break;
		case INT8_T:
			var_array_i8 = array;
			sprintf(str, "%d %s", var_array_i8[index], unit);
			break;
		case UINT8_T:
			var_array_u8 = array;
			sprintf(str, "%d %s", var_array_u8[index], unit);
			break;
		case UINT16_T:
			var_array_u16 = array;
			sprintf(str, "%d %s", var_array_u16[index], unit);
			break;
		case FLOAT:
			var_array_f = array;
			sprintf(str, "%.1f %s", var_array_f[index], unit);
			break;
		case STRING:
			var_array_s = array;
			sprintf(str, "%s %s", var_array_s[index], unit);
			break;
		case PHY_T:
			var_array_u8 = array;
			sprintf(str, "%s", (char*)phy_str(var_array_u8[index]));
			break;
	}
	
	if(terminal)
	{
		NRF_LOG_RAW_INFO("\033[%d;%dH", line_nr+1, x_pos/4);	//set position in terminal
		NRF_LOG_RAW_INFO("%s", nrf_log_push(str));
	}
	else
	{
		display_print_line(str, x_pos, line_nr);
	}
	
}

void menu_print()
{
	static menu_page_t *m_menu_current_page = &menu_main_page;
	
	static const uint16_t number_pos = 220;
	static const uint16_t text_pos = 20;
	static uint8_t max_lines = MAX_LINES - 2;

	uint8_t max_index = m_menu_current_page->nr_of_options;
	uint8_t opt_index = m_menu_current_page->index;
	int8_t line_index;	//first line displayed on the screen
	uint8_t cursor_index;
	
	while(1)
	{
		//start scrolling if opt_index is larger than the max lines that can be displayed on the screen - 1
		//(start scrolling when cursor is a t the line before the bottom line)
		if(opt_index > (max_lines - 1))
		{
			line_index =  max_lines - 1 - opt_index;
			cursor_index = max_lines - 1;	//place cursor at the line before the bottom line
		}
		else
		{
			line_index = 0;
			cursor_index = opt_index;
		}
		
		display_clear();
		//clear terminal screen (works in putty, tera term and RTT viewer, does not work in termite)
		NRF_LOG_RAW_INFO("\033[2J\033[;H");

		//display how the buttons works at the bottom of the page
		display_print_line("[Btn1: Up, Btn2: Sel, Btn3: Down, Btn4: Back]", 0, max_lines+1);
		display_print_line("->", 0, cursor_index);
		
		NRF_LOG_RAW_INFO("\033[%d;0H", max_index+1);
		NRF_LOG_RAW_INFO("[Btn1: UP, Btn2: SEL, Btn3: DOWN, Btn4: BACK]");
		NRF_LOG_RAW_INFO("\033[%d;%dH", opt_index + 1, 0);
		NRF_LOG_RAW_INFO("->");
		
		for(int8_t i = 0; i < max_index; i++)
		{
			if((i+line_index) <= max_lines)
			{
				//print to display
				print_var(m_menu_current_page->option_values, i, m_menu_current_page->option_type, 
						m_menu_current_page->option_unit, text_pos, i + line_index, false);
			
				if(m_menu_current_page->show_values)
				{
					if(m_menu_current_page->next_pages != NULL)
					{
						menu_page_t **next_pages = m_menu_current_page->next_pages;
						menu_page_t *next_page = next_pages[i];
						if(next_page != NULL)
						{
							print_var(next_page->option_current_value, 0, next_page->option_type, 
										next_page->option_unit, number_pos, i + line_index, false);
						}
					}
				}
			}
			
			//print to terminal
			print_var(m_menu_current_page->option_values, i, m_menu_current_page->option_type, 
						m_menu_current_page->option_unit, text_pos, i, true);
			
			if(m_menu_current_page->show_values)
			{
				if(m_menu_current_page->next_pages != NULL)
				{
					menu_page_t **next_pages = m_menu_current_page->next_pages;
					menu_page_t *next_page = next_pages[i];
					if(next_page != NULL)
					{
						print_var(next_page->option_current_value, 0, next_page->option_type, 
									next_page->option_unit, number_pos, i, true);
					}
				}
			}
			
		}
		
		//Update the display
		display_show();
		
		//Wait for button press from the user
		switch (button_read())
		{
			case BUTTON_DOWN:
				if(opt_index < (max_index-1))
				{
					opt_index++;
					if(cursor_index < max_lines)
					{
						cursor_index++;
					}
				}
				break;

			case BUTTON_UP:
				if(opt_index != 0)
				{
					opt_index--;
				}
				
				if(cursor_index != 0)
				{
					cursor_index--;
				}
				break;
			
			case BUTTON_SEL:
				
				//save the current index in the page
				//so that we get to the same place if we visit this page later
				m_menu_current_page->index = opt_index;
			
				//execute the callback function if this is not NULL
				if(m_menu_current_page->callback != NULL)
				{
					m_menu_current_page->callback(opt_index);
				}
				
				//change current page to next page
				//If the next page is NULL it means that it should go back to the previous page
				if(m_menu_current_page->next_pages != NULL)
				{
					menu_page_t **next_pages = m_menu_current_page->next_pages;
					menu_page_t *next_page = next_pages[opt_index];
					if(next_page != NULL)
					{
						//Do not change page if next page does not have options
						if(next_page->option_values != NULL)
						{
							m_menu_current_page = next_page;
						}
					}
				}
				else if(m_menu_current_page->prev != NULL)
				{
					m_menu_current_page = m_menu_current_page->prev;
				}
				
				return;
			
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
