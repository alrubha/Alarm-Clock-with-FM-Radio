// lm73_functions.c       
// Roger Traylor 11.28.10

#include <util/twi.h>
#include "lm73_functions.h"
#include <util/delay.h>
#include "thermo.h"

uint8_t lm73_wr_buf[2];
uint8_t lm73_rd_buf[2];

//********************************************************************************

uint8_t  lm73_temp_convert(uint16_t lm73_temp, uint8_t f_not_c){

    if(f_not_c == 1){
        lm73_temp = lm73_temp * (9/5) + 32;
    }
    
    else if(f_not_c == 2){
        lm73_temp = lm73_temp * (9/5) + 32;
    }
    
}
