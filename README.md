# Alarm-Clock-with-FM-Radio
Alarm clock with FM radio, auto brigtness control, and room tempreature measurements.<br />
The project uses atmega128 to perform clock calculation and to display the time on a seven segment display.<br />
In addition, it configures ADC with light sensor to determine the brightness of the display. atmega128 also controls si4734 FM radio module for radio access and LM73 temperature sensor locally. It is also connected to another microcontroller(atmega48) through usart to receive remote temperature. 
