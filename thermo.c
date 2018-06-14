#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include "hd44780.h"
#include "lm73_functions.h"
#include "twi_master.h"
#include "thermo.h"

void temp(){
    
lm73_wr_buf[0] = LM73_PTR_TEMP;                 //load lm73_wr_buf[0] with temperature pointer address
twi_start_wr(LM73_ADDRESS, lm73_wr_buf, 2);     //start the TWI write process


    twi_start_rd(LM73_ADDRESS, lm73_rd_buf, 2);   //read temperature data from LM73 (2 bytes)
  _delay_ms(2);                                 //wait for it to finish
  lm73_temp = lm73_rd_buf[0];                   //save high temperature byte into lm73_temp
  lm73_temp = lm73_temp << 8;                   //shift it into upper byte
  lm73_temp |= lm73_rd_buf[1];                  //"OR" in the low temp byte to lm73_temp
                                                //lm73_temp = lm73_temp / 128;
    
    
    h_result = div(lm73_temp , 128);
    itoa(h_result.quot, lcd_string_array_h, 10); //convert to string in array with itoa() from avr-libc
    l_result = div((h_result.rem*100), 128);
    itoa(l_result.quot, lcd_string_array_l, 10); //convert to string in array with itoa() from avr-libc

}
