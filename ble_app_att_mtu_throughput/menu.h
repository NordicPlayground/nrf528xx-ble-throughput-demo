
#ifndef MENU_H
#define MENU_H

#include "boards.h"

#define BUTTON_UP				  BUTTON_1
#define BUTTON_DOWN				  BUTTON_3
#define BUTTON_SEL				  BUTTON_2
#define BUTTON_BACK				  BUTTON_4

uint32_t phy_str(uint8_t phy);
uint8_t button_read(void);
void menu_print(void);

#endif //MENU_H
