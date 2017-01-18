
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
