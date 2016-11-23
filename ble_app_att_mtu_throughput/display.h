
#ifndef DISPLAY_H
#define DISPLAY_H

#include "nrf.h"

typedef struct
{
    uint16_t att_mtu;                   /**< GATT ATT MTU, in bytes. */
    uint16_t conn_interval;             /**< Connection interval expressed in units of 1.25 ms. */
    bool     data_len_ext_enabled;      /**< Data length extension status. */
    bool     conn_evt_len_ext_enabled;  /**< Connection event length extension status. */
} test_params_t;

typedef struct
{
	uint16_t kb_transfer_size;
	uint16_t kB_transfered;
	
} transfer_data_t;

bool display_init(void);
void display_test(void);

void display_draw_nordic_logo(void);

void display_draw_test_run_screen(transfer_data_t *transfer_data);

void display_print_line_inc(char * line);
void display_print_line(char * line, uint32_t x_pos, uint8_t line_nr);
void display_show(void);
void display_clear(void);

#endif //DISPLAY_H
