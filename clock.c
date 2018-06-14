#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>
#include "music.h"
#include "hd44780.h"
#include "thermo.h"
#include "twi_master.h"
#include "lm73_functions.h"
#include "uart_functions.h"
#include "si4734.h"


uint8_t dec_to_7seg[12] = {0x03,0x9F,0x25,0x0D,0x99,0x49,0x41,0x1F,0x01,0x09,0xFF};		//dec to hex for 7segs
uint8_t segment_data[5] = {0xFF,0xFF,0xFF,0xFF,0xFF};						//values sent to 7segs
uint8_t segment_data_a[5] = {0xFF,0xFF, 0xFF,0xFF,0xFF};					//alarm data
uint8_t segment_data_f[4] = {0xFF,0xFF,0xFF,0xFF};               			        // values sent for freq
static uint8_t sec = 0;										//seconds
uint8_t encoder_val = 0;									//the value read from encoder
int mins = 0;											//minutes
int hrs = 12;											//hours
int hrsa = 0;
int minsa = 0;
uint8_t ampm = 0;										//if 0 -> am, 1 -> pm
uint8_t ampma = 0;										//if 0 -> am, 1 -> pm
uint16_t adc_result;										//holds adc result
uint8_t b0 = 0;                                         					// set time button
uint8_t b1 = 0;                                        						// set alarm button
uint8_t b2 = 0;                                        						// snooze
uint8_t b3 = 0;                                        						// turn off alarm
uint8_t b4 = 0;                                                             			// radio mode
uint8_t b5 = 0;                                                             			// volume control
uint8_t s_t = 0;                                     						// snooze time
char rem_temp[16];
char rx_char;
volatile uint8_t  rcv_rdy;
extern uint16_t current_fm_freq;
volatile uint8_t STC_interrupt;  //flag bit to indicate tune or seek is done
uint16_t freq = 9990;
//********************************************************************
//				Initilize TCNT0     interrupt
//********************************************************************
void tcnt0_init(){
				
    TCCR0 |= (1 << CS00);									// normal mode, no prescaling
    ASSR  |= (1 << AS0);									// use ext oscillator
    TIMSK |= (1 << TOIE0); 									// allow interrupt on overflow
    
}

//********************************************************************
//				Initilize TCNT2    brightness
//********************************************************************

void tcnt2_init(){
    
    TCCR2 |= (1 << WGM21) | (1 << WGM20)| (1 << COM21)| (1 << CS20);				//fast pwm, no prescalling
    OCR2 = 0;											//to determine the brigtness
    
}

//********************************************************************
//				Initilize TCNT3 for volume control
//********************************************************************

void tcnt3_init(){
    
    TCCR3A |= (1<<COM3A1)|(1<<COM3A0)|(1<<WGM30);
    TCCR3B |= (1<<WGM32)|(1<<CS30);
    TCCR3C = 0x00;
    OCR3A = 255;
    
}

//********************************************************************
//				Initilize adc
//********************************************************************

void adc_init(){
    
//    DDRF  &= ~(_BV(DDF7));									 //make port F bit 7 is ADC input
//    PORTF &= ~(_BV(PF7));  									 //port F bit 7 pullups must be off
    
    ADMUX |= (1 << REFS0) | (1 << MUX0) | (1 << MUX1) | (1 << MUX2); 			         //single-ended, input PORTF bit 7, right adjusted, 10 bits
    
    ADCSRA |= (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); 				 //ADC enabled, don't start yet, single shot mode
    
    
}

//********************************************************************
//				read adc
//********************************************************************

void adc(){
    
    ADCSRA |= (1 << ADSC); 									//poke ADSC and start conversion
    
    while(bit_is_clear(ADCSRA,ADIF)){} 								//spin while interrupt flag not set
    
    ADCSRA |= (1 << ADIF); 	      								//its done, clear flag by writing a one
    
    adc_result = ADC;    				             				//read the ADC output as 16 bits
    
}

//********************************************************************
//				brigtness control
//********************************************************************

void b_cntrl(){
    
    if(adc_result >= 900){OCR2 = 0;} 								// room brightness
    else if (adc_result >= 800){OCR2 = 25;}							// dark
    else if (adc_result >= 700){OCR2 = 50;}							// dark
    else if (adc_result >= 600){OCR2 = 75;}							// dark
    else if (adc_result >= 500){OCR2 = 100;}						// dark
    else if (adc_result >= 400){OCR2 = 125;}						// dark
    else if (adc_result >= 300){OCR2 = 150;}						// dark
    else if (adc_result >= 200){OCR2 = 175;}						// dark
    else if (adc_result >= 100){OCR2 = 200;}						// dark
    else if (adc_result >= 50){OCR2 = 225;}						// dark


}

//*********************************************************************
//                                spi_init
//*********************************************************************
void spi_init(){
    
    DDRF  |= 0x08; 										//port F bit 3 is enable for LCD
    PORTF &= 0xF7;  							   			//port F bit 3 is initially low

    DDRB = 0xFF;										//PORTB output for seg0-2 + pwm F7
    PORTB |= _BV(PB1);  									//port B initalization for SPI, SS_n off
    
  	 SPCR   = (1<<SPE) | (1<<MSTR);          						//master mode, clk low on idle, leading edge sample
  	 SPSR   = (1<<SPI2X);                    						//choose double speed operation
    
}

//******************************************************************************
//                            chk_buttons
//Checks the state of the button number passed to it. It shifts in ones till
//the button is pushed. Function returns a 1 only once per debounced button
//push so a debounce and toggle function can be implemented at the same time.
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"
//Expects active low pushbuttons on PINA port.  Debounce time is determined by
//external loop delay times 12.
//
//******************************************************************************
uint8_t chk_buttons(uint8_t button) {
    static uint16_t state[] = {0,0,0,0,0,0,0,0}; 				                //holds present state
    state[button] = (state[button] << 1) | (! bit_is_clear(PINA, button)) | 0xE000;
    if (state[button] == 0xF000) return 1;
    return 0;
}

//*****************************************************************************
//				button_read
//******************************************************************************
void button_read(){
    
    DDRA = 0x00;
    PORTB = 0x70;
    PORTA = 0xFF;
    
    if(chk_buttons(0)){
    switch (b0){        	// set time button
            
        case 0:
        b0 = 1;			// on

        break;
            
        case 1:
        b0 = 0;			// off

        break;
            
        }
    }

    if(chk_buttons(1)){
        switch (b1){        	// set alarm button
                
            case 0:
                b1 = 1;		// on
                break;
                
            case 1:
                b1 = 0;		// off
                if(hrsa != 0 || minsa != 0){
                    
                    clear_display();
                    string2lcd(" ALARM Armed");
                    cursor_home();
                    
                }
                if(hrsa == hrs && minsa == mins && ampma == ampm && sec == 0){
                    
                    OCR3A = 0;
                    music_on();
                    lcd_dis();
                    
                }

                break;
                
        }
    }

    if(chk_buttons(2)){
        switch (b2){       	 // snooze
                
            case 0:
                b2 = 1;		// on
                break;
                
            case 1:
                b2 = 0;		// off
                break;
                
        }
    }
    
    if(chk_buttons(3)){
        switch (b3){     	// alarm off or on
                
            case 0:
                b3 = 1;		// on
                break;
                
            case 1:
                b3 = 0;		// off
                minsa = hrsa = ampma = 0;
                break;
                
        }
    }
 
    if(chk_buttons(4)){
        switch (b4){     	// radio off or on
                
            case 0:
                b4 = 1;		// on
                radio_call();
                break;
                
            case 1:
                b4 = 0;		// off
                radio_pwr_dwn();
                break;
                
        }
    }

    if(chk_buttons(5)){
  
        b5++;
        if(b5 == 5){
        
            b5 = 0;
            
        }
        
    }

    DDRA = 0xFF;
    
}

//******************************************************************************
// This function reads the state of encoder's nobs to and save the value in
// encoder_val
//******************************************************************************
void encoder_in(){
    
    SPDR = 0x00;							// send dummy data to run clock
    while (bit_is_clear(SPSR,SPIF)){} 					// wait till 8 clock cycles are done
    PORTE &= ~0x40;		  					// send low to load
    PORTE |= 0x40;          						// send high to shift
    encoder_val = SPDR;							// save the value from encoder
}

//******************************************************************************
//				set_time
//******************************************************************************
void set_time(){
    
    static uint8_t e_prev = 0;
    static uint8_t e_curr = 0;
    
    e_prev = e_curr;
    e_curr = encoder_val;
    
    if(b0 == 1){
        
        if(e_curr != e_prev){
            if((e_prev == 0xFE) && (e_curr == 0xFC)){
                mins++;
                if(mins > 59){
                    mins = 0;
                }
            }
            else if ((e_prev == 0xFD) && (e_curr == 0xFC)){
                mins--;
                if(mins < 0){
                    mins = 59;
                }
            }
        }
        
        if(e_curr != e_prev){
            if((e_prev == 0xFB) && (e_curr == 0xF3)){
                hrs++;
                
                
                        if(hrs > 12){
                            hrs = 1;
                            ampm++;
                            segment_data[2] = 0xDF;
                            if(ampm > 1){ ampm = 0;
                                segment_data[2] = 0xFF;
                            }
                        }
                }
            else if ((e_prev == 0xF7) && (e_curr == 0xF3)){
                hrs--;
                
                        if(hrs < 1){
                            hrs = 12;
                            ampm++;
                            segment_data[2] = 0xDF;
                            if(ampm > 1){ ampm = 0;
                                segment_data[2] = 0xFF;
                            }
                        }
                }
        }
    }
}

//******************************************************************************
//				set_time alarm
//******************************************************************************
void set_time_a(){
    
    static uint8_t e_prev_a = 0;
    static uint8_t e_curr_a = 0;
    
    e_prev_a = e_curr_a;
    e_curr_a = encoder_val;
    
    if(b1 == 1){
        
        if(e_curr_a!= e_prev_a){
            if((e_prev_a == 0xFE) && (e_curr_a == 0xFC)){
                minsa++;
                if(minsa > 59){
                    minsa = 0;
                }
            }
            else if ((e_prev_a == 0xFD) && (e_curr_a == 0xFC)){
                minsa--;
                if(minsa < 0){
                    minsa = 59;
                }
            }
        }
        
        if(e_curr_a != e_prev_a){
            if((e_prev_a == 0xFB) && (e_curr_a == 0xF3)){
                hrsa++;
                
                        if(hrsa > 12){
                            hrsa = 1;
                            ampma++;
                            segment_data_a[2] = 0xDF;
                            if(ampma > 1){ ampma = 0;
                                segment_data_a[2] = 0xFF;
                            }			
                        }
          
            }
            else if ((e_prev_a == 0xF7) && (e_curr_a == 0xF3)){
                hrsa--;

                if(hrsa < 1){
                            hrsa = 12;
                            ampma++;
                            segment_data_a[2] = 0xDF;
                            if(ampma > 1){ ampma = 0;
                                segment_data_a[2] = 0xFF;
                            }			
                        }
            }
        }
    } 

}



//********************************************************************
//			segsum
//********************************************************************

void segsum(uint8_t hours, uint8_t minutes){
    
    uint8_t split_count[] = {0,0,0,0};
    split_count[0] = minutes % 10;
    split_count[1] = (minutes/10) % 10;
    split_count[2] = hours % 10;
    split_count[3] = (hours/10) % 10;
    
    if(hours == 0){
        split_count[3] = 0;
        split_count[2] = 0;
    }
    else if(hours <= 9){
        split_count[3] = 0;
    }
    
    if(minutes == 0){
        split_count[1] = 0;
        split_count[0] = 0;
    }
    else if(minutes <= 9){
        split_count[1] = 0;
    }
    
 
    
    switch (b1){
    
    case 0:
            segment_data[0] = dec_to_7seg[split_count[3]];				//thousands
            segment_data[1] = dec_to_7seg[split_count[2]];				//hundreds
            segment_data[3] = dec_to_7seg[split_count[1]];				//tens
            segment_data[4] = dec_to_7seg[split_count[0]];				//ones
        break;
    case 1:
            segment_data_a[0] = dec_to_7seg[split_count[3]];				//thousands
            segment_data_a[1] = dec_to_7seg[split_count[2]];				//hundreds
            segment_data_a[3] = dec_to_7seg[split_count[1]];				//tens
            segment_data_a[4] = dec_to_7seg[split_count[0]];				//ones
        break;
    }
}

//********************************************************************
//			seg_out
//********************************************************************

void seg_out(uint8_t a, uint8_t b, uint8_t c, uint8_t d ,uint8_t e ){
    
    DDRA = 0xFF;  //output
    
    PORTB = 0x00; //digit zero  on
    if(hrsa != 0 || minsa != 0){
    PORTA = a ^ 0x01;
    }
    else
        PORTA = a;

    _delay_us(500);
    
    PORTB = 0x10; //digit one   on
    PORTA = b;
    _delay_us(500);
    
    PORTB = 0x20; //colon, blinking 
    PORTA = c;
    _delay_us(500);
    
    PORTB = 0x30; //digit two   on
    PORTA = d;
    _delay_us(500);
    
    PORTB = 0x40; //digit three on
    PORTA = e;
    _delay_us(500);
    
}

//********************************************************************
//			Time ->count time
//********************************************************************
void time(){
    
    if(sec == 60){
        
        mins++;
        sec = 0;
    }
    
    if(mins > 59){
      
        mins = 0;
        hrs++;
        
        if(hrs > 12){
            
            hrs = 1;
            ampm++;
            segment_data[2] = 0xDF;
            
            if(ampm > 1){
            ampm = 0;
            segment_data[2] = 0xFF;
                
            }			
        }
    }
    
    segsum(hrs,mins);
    if(b1 == 1){segsum(hrsa, minsa);}			    // only when set alarm is on
    //alarm();
}

//********************************************************************
//			Display
//********************************************************************
void lcd_dis(){
    
    clear_display();
    string2lcd(" ALARM!");
    cursor_home();
    
}

void local_temp(){
    
            line2_col1();
            string2lcd(" L");
            string2lcd(lcd_string_array_h);  		   //write upper half
            char2lcd('.');                                 //write decimal point
            string2lcd(lcd_string_array_l);  		   //write lower half
            char2lcd('C');
            //cursor_home();

}

ISR(USART0_RX_vect){
    static  uint8_t  i;
    rx_char = UDR0;              			   //get character
    rem_temp[i++]=rx_char;  				   //store in array
    							   //if entire string has arrived, set flag, reset index
    if(rx_char == '\0'){
        rcv_rdy=1;
        rem_temp[--i]  = (' ');			           //clear the count field
        rem_temp[i+1]  = (' ');
        rem_temp[i+2]  = (' ');
        i=0;
    }
}

//********************************************************************
//			Time ->count time
//********************************************************************
void alarm(){
    
    if(hrsa == hrs && minsa == mins && ampma == ampm && sec == 0){
       
        OCR3A = 0;
        music_on();
        lcd_dis();
        
    }

    if(b2 == 1){            // snooze
        music_off();
        OCR3A = 255;
    }
    
    if(s_t == 10){          // change to 600 for 10 minutes
        b2 = 0;
        OCR3A = 0;
        music_on();
        s_t = 0;
    }
    
    if(b3 == 1){	   // alarm off
        b3 = 0;
        music_off();
        OCR3A = 255;
        clear_display();
    }
}


//********************************************************************
//				volume control ->tcnt3
//********************************************************************
void vol(){

    switch (b5){
            
        case 0:
            OCR3A = 100;
            break;
            
        case 1:
            OCR3A = 150;
            break;
            
        case 2:
            OCR3A = 200;	
            break;
            
        case 3:
            OCR3A = 0;	
            break;
            
        case 4:
            OCR3A = 50;	
            break;
            
    }
}

/*********************************************************************/
/*                             TIMER1_COMPA                          */
/*Oscillates pin7, PORTD for alarm tone output                       */
/*********************************************************************/

ISR(TIMER1_COMPA_vect) {
    PORTD ^= ALARM_PIN;        			//flips the bit, creating a tone
    if(beat >= max_beat) {     			//if we've played the note long enough
        notes++;               			//move on to the next note
        play_song(song, notes);			//and play it
    }
}

/*********************************************************************/
/*                             For Radio                             */
/*                                                                   */
/*********************************************************************/


void radio_init(){

    DDRE  |= 0x04; 
    PORTE |= 0x04; 
    
    EICRB |= (1<<ISC71) | (1<<ISC70);
    EIMSK |= (1<<INT7);
    
    PORTE &= ~(1<<PE7); 
    DDRE  |= 0x80;      
    PORTE |=  (1<<PE2); 
    _delay_us(200);     
    PORTE &= ~(1<<PE2); 
    _delay_us(30);    
    
    DDRE  &= ~(0x80);  

}

void tune_freq(){
    
    
    static uint8_t e_prev_r = 0;
    static uint8_t e_curr_r = 0;
    
    e_prev_r = e_curr_r;
    e_curr_r = encoder_val;
    if(b4 == 1){
            if(e_curr_r != e_prev_r){
                if((e_prev_r == 0xFE) && (e_curr_r == 0xFC)){
                    freq += 20;
                    
                    if(freq > 10790){
                    
                        freq = 8810;
                    
                    }//if
                    
                }//if
                else if ((e_prev_r == 0xFD) && (e_curr_r == 0xFC)){
                    freq -= 20;
                    
                    if(freq < 8800){
                        
                        freq = 10790;
                        
                    }//if

                }//else if
            }// outer if
        }//b4 if
}

void radio_call(){

    if(b4 == 1){                // if radio mode is on turn on
    
    OCR3A = 0;                  // set volume
    fm_pwr_up();                //powerup the radio as appropriate
    current_fm_freq = freq;
    fm_tune_freq();
    }
    
}

void segout_freq(){

    if(b4 == 1){     // if in radio mode and freq tuning output freq on led
    
        
        uint8_t split_count_f[] = {0,0,0,0};
        split_count_f[0] = (freq/10) % 10;
        split_count_f[1] = (freq/100) % 10;
        split_count_f[2] = (freq/1000) % 10;
        split_count_f[3] = (freq/10000) % 10;
        
        DDRA = 0xFF;  //output
        
        PORTB = 0x00; //digit zero  on
        PORTA = dec_to_7seg[split_count_f[0]];
        _delay_us(500);
        
        PORTB = 0x10; //digit one   on
        PORTA = dec_to_7seg[split_count_f[1]] ^ 0x01;
        _delay_us(500);
        
        PORTB = 0x20; //colon, blinking
        PORTA = 0xFF;
        _delay_us(500);
        
        PORTB = 0x30; //digit two   on
        PORTA = dec_to_7seg[split_count_f[2]];
        _delay_us(500);
        
        PORTB = 0x40; //digit three on
        PORTA = dec_to_7seg[split_count_f[3]];
        _delay_us(500);

        
    }//if
}

ISR(INT7_vect){
    STC_interrupt = TRUE;
}

//********************************************************************
//			ISR
//********************************************************************
ISR(TIMER0_OVF_vect){
    
    static uint8_t count = 0;
    count++;
    
//        set_time();
//        set_time_a();
        adc();
    
    if(count % 8 == 0) {
        beat++;
    }
    
    if((count % 128) == 0){
        sec++;
        uart_putc('A');
        local_temp();
        if(b2 == 1){
            s_t++; 		                // snooze time
        }
        
        segment_data[2] ^= 0xC0;		// this turn on and off colons every second
        
    }
    
}



//********************************************************************
//			Main
//********************************************************************
int main(){
    
    DDRE = 0xFF;
    DDRD = 0x0C;	 					
    
    
    spi_init();                                                     // for encoder
    lcd_init();
    tcnt0_init();                                                   // to count time
    tcnt2_init();                                                   // brigtness
    tcnt3_init();                                                   // volume
    adc_init();                                                     // brigtness
    music_init(0);
    init_twi();                                                     //initalize TWI (twi_master.h)
    uart_init();
    radio_init();                                                   // setup the radio
    sei();                                                          // enable gobal interrupt
    

    while(1){
        
        button_read();						    // check the state of buttons
        encoder_in();						    // check the state of encoder
        b_cntrl();	         		  	            // control brigtness
        set_time();
        set_time_a();
        tune_freq();
        time();
        alarm();
        temp();
        
        
        vol();

        if(rcv_rdy==1){
            
            string2lcd(" R");
            string2lcd(rem_temp);
            rcv_rdy=0;
            cursor_home();

        }//if

//        if(hrsa != 0 && minsa != 0){
//        
//            //clear_display();
//            string2lcd(" Alarm is Armed");
//            cursor_home();
//        }
        
        
       if(b1 == 0 && b4 == 0){
        seg_out(segment_data[4], segment_data[3],segment_data[2], segment_data[1],segment_data[0]);
        }
        else if(b1 == 1 && b4 == 0){
        seg_out(segment_data_a[4], segment_data_a[3],segment_data_a[2], segment_data_a[1],segment_data_a[0]);
        }
        segout_freq();
    
    }
}



