#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include "hd44780.h"
#include "lm73_functions.h"
#include "twi_master.h"

char    lcd_string_array_h[16];  //holds a string to refresh the LCD
char    lcd_string_array_l[16];  //holds a string to refresh the LCD

extern uint8_t lm73_wr_buf[2]; 
extern uint8_t lm73_rd_buf[2]; 
div_t    l_result, h_result;  //double result;
static uint16_t lm73_temp;                      //a place to assemble the temperature from the lm73

void temp();
