#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdlib.h>
#include <util/twi.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "uart_functions.h"
#include "si4734.h"
#include "twi_master.h"

extern uint16_t current_fm_freq;
volatile uint8_t STC_interrupt;  //flag bit to indicate tune or seek is done

ISR(INT7_vect){  STC_interrupt = TRUE;  }


int main(){

    init_twi();

    DDRE  |= 0x08;
    PORTE |= 0x08;
    
    DDRE  |= 0x04; //Port E bit 2 is active high reset for radio
    PORTE |= 0x04; //radio reset is on at powerup (active high)
    
    EICRB |= (1<<ISC71) | (1<ISC70);
    EIMSK |= (1<<INT7);
    
    //hardware reset of Si4734
    PORTE &= ~(1<<PE7); //int2 initially low to sense TWI mode
    DDRE  |= 0x80;      //turn on Port E bit 7 to drive it low
    PORTE |=  (1<<PE2); //hardware reset Si4734
    _delay_us(200);     //hold for 200us, 100us by spec
    PORTE &= ~(1<<PE2); //release reset
    _delay_us(30);      //5us required because of my slow I2C translators I suspect
    //Si code in "low" has 30us delay...no explaination
    DDRE  &= ~(0x80);   //now Port E bit 7 becomes input from the radio interrupt
    
    sei();

    fm_pwr_up(); //powerup the radio as appropriate
    current_fm_freq = 9990;
    fm_tune_freq();
    
    while(1){
    }//while


}//main
