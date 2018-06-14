#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include "lm73_functions.h"
#include "uart_functions_m48.h"
#include "twi_master.h"

extern uint8_t lm73_wr_buf[2];
extern uint8_t lm73_rd_buf[2];
char lcd_string_array[16];
char rx_char;

div_t    r_l_result, r_h_result;  //double result;
char    r_string_array_h[16];  //holds a string to refresh the LCD
char    r_string_array_l[16];  //holds a string to refresh the LCD

/***********************************************************************/
/*                                main                                 */
/***********************************************************************/
int main (){
 
uint16_t lm73_temp;                                      //a place to assemble the temperature from the lm73

//tcnt2_init();
init_twi();                                              //initalize TWI (twi_master.h)
uart_init();                                             //initilize UART
    
sei();
//set LM73 mode for reading temperature by loading pointer register
    
lm73_wr_buf[0] = LM73_PTR_TEMP;                         //load lm73_wr_buf[0] with temperature pointer address
twi_start_wr(LM73_ADDRESS, lm73_wr_buf, 2);             //start the TWI write process

while(1){
  twi_start_rd(LM73_ADDRESS, lm73_rd_buf, 2);           //read temperature data from LM73 (2 bytes)
  _delay_ms(2);                                         //wait for it to finish
  lm73_temp = lm73_rd_buf[0];                           //save high temperature byte into lm73_temp
  lm73_temp = lm73_temp << 8;                           //shift it into upper byte
  lm73_temp |= lm73_rd_buf[1];                          //"OR" in the low temp byte to lm73_temp
//  itoa(lm73_temp,lcd_string_array,10);

    r_h_result = div(lm73_temp , 128);
    itoa(r_h_result.quot, r_string_array_h, 10); //convert to string in array with itoa() from avr-libc
    r_l_result = div((r_h_result.rem*100), 128);
    itoa(r_l_result.quot, r_string_array_l, 10); //convert to string in array with itoa
    
    if( uart_getc() == 'A'){
        uart_puts(r_string_array_h);
        uart_putc('.');
        uart_puts(r_string_array_l);
        uart_putc('C');
        uart_putc('\0');
    }
    
} //while
} //main
