/* Copyright (c) 2017 Nordic Semiconductor. All Rights Reserved.
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

#ifndef MENU_H
#define MENU_H

#include "boards.h"
#include "display.h"

#define BUTTON_UP				  BUTTON_1
#define BUTTON_DOWN				  BUTTON_3
#define BUTTON_SEL				  BUTTON_2
#define BUTTON_BACK				  BUTTON_4

void test_begin(bool continuous);
void set_all_parameters(test_params_t *params);
uint32_t phy_str(uint8_t phy);
uint8_t button_read(void);

void get_test_params(test_params_t *params);
void menu_print(void);

#endif //MENU_H
