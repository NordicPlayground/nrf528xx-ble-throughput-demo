
#include <string.h>

#include "display.h"
#include "menu.h"
#include "ble_gap.h"

typedef enum
{
	BOOL,
	INT8_T,
	UINT8_T,
	UINT16_T,
	FLOAT,
	STRING,
} type_t;

typedef void (*handler_t)(uint32_t option_index);

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
    .att_mtu                  = 247,
    .conn_interval            = 400.0f,
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
	.rxtx_phy                 = BLE_GAP_PHY_2MBPS,
	.tx_power				  = 8,
};

menu_page_t menu_main_page;

//TX POWER OPTIONS

#define TX_POWER_OPTIONS_SIZE 3

int8_t tx_power_options[TX_POWER_OPTIONS_SIZE] = {0, 4, 8};

void menu_tx_power_func(uint32_t option_index)
{
	m_test_params.tx_power = tx_power_options[option_index];
	
	//update params
	//TODO
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
	.index					= 2,
	.callback				= menu_tx_power_func,
	.next_pages				= NULL,
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
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
	&menu_tx_power_page,
};

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
	.callback				= NULL,
	.next_pages				= main_next_pages,
};

void print_var(void *array, uint8_t index, type_t type, char *unit, uint32_t x_pos, uint8_t line_nr)
{
	char str[30];
	
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
	}
	
	display_print_line(str, x_pos, line_nr);
}

void menu_print()
{
	static menu_page_t *m_menu_current_page = &menu_main_page;
	
	static const uint16_t number_pos = 220;
	static const uint16_t text_pos = 20;
	static uint8_t max_lines = MAX_LINES - 2;

	uint8_t max_index = m_menu_current_page->nr_of_options;
	uint8_t opt_index = m_menu_current_page->index;
	int8_t line_index;
	uint8_t cursor_index;
	
	while(1)
	{
		//start scrolling if opt_index is larger than the max lines
		if(opt_index > (max_lines - 1))
		{
			line_index =  max_lines - 1 - opt_index;
			cursor_index = max_lines - 1;
		}
		else
		{
			line_index = 0;
			cursor_index = opt_index;
		}
		
		display_clear();

		//display how the buttons works at the bottom of the page
		display_print_line("[Btn1: UP, Btn2: DOWN, Btn3: Sel, Btn4: Back]", 0, max_lines+1);
		display_print_line("->", 0, cursor_index);
		
		for(int8_t i = 0; i < max_index; i++)
		{
			if((i+line_index) <= max_lines)
			{
				print_var(m_menu_current_page->option_values, i, m_menu_current_page->option_type, 
							m_menu_current_page->option_unit, text_pos, i + line_index);
				
				if(m_menu_current_page->show_values)
				{
					if(m_menu_current_page->next_pages != NULL)
					{
						menu_page_t **next_pages = m_menu_current_page->next_pages;
						menu_page_t *next_page = next_pages[i];
						if(next_page != NULL)
						{
							print_var(next_page->option_current_value, 0, next_page->option_type, 
										next_page->option_unit, number_pos, i + line_index);
						}
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
						m_menu_current_page = next_page;
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
