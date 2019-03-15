//Created: 2/28/2019 7:04:33 PM
//For use on an ATMega on Atmel

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h> //for rand
#include <stdlib.h> //for rand
#include <time.h> //for rand
#include <string.h>
#include "io.c"
#include "timer.h"
#include "queue.h"
#include "pwm.h"
#include "a2d.h"

//state machine
enum Zen_States {NewGame, Wait, GameTime, End} state;
	
//x and y keep track of the physical board coordinates. xCoord and yCoord keep track of regular x and y coordinates.
unsigned char x[4] = {0x3F, 0xCF, 0xF3, 0xFC}; //LEFT TO RIGHT (0's are lightup for some reason fyi)
unsigned char y[4] = {0xC0, 0x30, 0x0C, 0x03}; //90' LEFT TO RIGHT (1's** are lightup fyi)_
unsigned char xCoord[4];

//game variables
unsigned char button = 0;
unsigned short tileCnt = 0;
unsigned char rightTile = 0;
short tick = 0;

//PWM music (dedicated to Caryn)
unsigned char note = 0;
double notes[8] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00 , 493.88, 523.25};
double sans[40] = {587.33, 587.33, 1174.66, 880.00, 830.61, 783.99, 698.46, 587.33, 698.46, 783.99,
				   523.25, 523.25, 1174.66, 880.00, 830.61, 783.99, 698.46, 587.33, 698.46, 783.99,
				   493.88, 493.88, 1174.66, 880.00, 830.61, 783.99, 698.46, 587.33, 698.46, 783.99,
				   466.16, 466.16, 1174.66, 880.00, 830.61, 783.99, 698.46, 587.33, 698.46, 783.99};
				   
//////////////////
//PWM FUNCTIONS
//////////////////
void debounce(char period) {
	while(period > 0) {
		while(!TimerFlag);
		TimerFlag = 0;
		period--;
	}
}
void playNote() {
	set_PWM(sans[note]);
	debounce(30);
	set_PWM(0);
	if (note >= 39) {
		note = 0;
	}
	else {
		note++;
	}
}

//////////////////
//MATRIX FUNCTIONS
//////////////////
//passes in port A0-A3 to shift register connected to led matrix, and port C0-C3
void transmit_x(unsigned char data) {
	for (unsigned char i = 0; i < 8; i++) {
		// Sets SRCLR to 1 allowing data to be set. Also clears SRCLK in preparation of sEnding dataS
		PORTC = 0x08;
		// set SER = next bit of data to be sent.
		PORTC |= ((data >> i) & 0x01); //or operator
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTC |= 0x04;
	}
	// set RCLK = 1. Rising edge copies data from the “Shift” register to the “Storage” register
	PORTC |= 0x02;
	// clears all lines in preparation of a new transmission
	PORTC = 0;
}
void transmit_y(unsigned char data) {
	for (unsigned char i = 0; i < 8; i++) {
		// Sets SRCLR to 1 allowing data to be set. Also clears SRCLK in preparation of sEnding dataS
		PORTC = 0x80;
		// set SER = next bit of data to be sent.
		PORTC |= ((data >> i) & 0x01) << 4; //or operator
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTC |= 0x40;
	}
	// set RCLK = 1. Rising edge copies data from the “Shift” register to the “Storage” register
	PORTC |= 0x20;
	// clears all lines in preparation of a new transmission
	PORTC = 0;
}
unsigned char randX() {
	unsigned char i = (rand() % 5);
	
	if (i == 1) {
		return 0;
	}
	else if (i == 2) {
		return 1;
	}
	else if (i == 3) {
		return 2;
	}
	else if (i == 4) {
		return 3;
	}
	
	return 0; //shouldn't reach here
}
unsigned char getXCoord() {
	if (xCoord[0] == 0) {
		return 1;
	}
	else if (xCoord[0] == 1) {
		return 2;
	}
	else if (xCoord[0] == 2) {
		return 4;
	}
	else if (xCoord[0] == 3) {
		return 16; //skip PB3 b/c of PWM
	}
	return 0; //shouldn't reach
} //convert x coordinate to button value
void displayMatrix() {
	for (unsigned i = 0; i < 4; i++) {
		transmit_x(x[xCoord[i]]);
		transmit_y(y[i]);
		
		while(!TimerFlag); //needed to make 'multiple' tiles
		TimerFlag = 0;
	}
}
void moveDown() {
	xCoord[0] = xCoord[1];
	xCoord[1] = xCoord[2];
	xCoord[2] = xCoord[3];
	xCoord[3] = randX();
	
	rightTile = getXCoord(); //update
}//moves tiles down and creates new tile if needed

//////////////////
//LCD FUNCTIONS
//////////////////
void displayLCD() {
	LCD_ClearScreen();
	LCD_DisplayString(1, "Score: ");
	
	if (tileCnt > 99) { //accounts for > 100
		LCD_Cursor(8);
		LCD_WriteData((tileCnt / 100) + '0');
		LCD_Cursor(9);
		LCD_WriteData(((tileCnt % 100) / 10) + '0');
		LCD_Cursor(10);
		LCD_WriteData(((tileCnt % 100) % 10) + '0');
	}
	else if (tileCnt > 9) { //accounts for > 10
		LCD_Cursor(8);
		LCD_WriteData((tileCnt / 10) + '0');
		LCD_Cursor(9);
		LCD_WriteData(((tileCnt % 10)) + '0');
	}
	else {
		LCD_Cursor(8);
		LCD_WriteData(tileCnt + '0');
	}
}
void displayFinalScore() {
	LCD_DisplayString(1, "Final Score: ");
	
	if (tileCnt > 99) { //accounts for > 100
		LCD_Cursor(14);
		LCD_WriteData((tileCnt / 100) + '0');
		LCD_Cursor(15);
		LCD_WriteData(((tileCnt % 100) / 10) + '0');
		LCD_Cursor(16);
		LCD_WriteData(((tileCnt % 100) % 10) + '0');
	}
	else if (tileCnt > 9) { //accounts for > 10
		LCD_Cursor(14);
		LCD_WriteData((tileCnt / 10) + '0');
		LCD_Cursor(15);
		LCD_WriteData(((tileCnt % 10)) + '0');
	}
	else {
		LCD_Cursor(14);
		LCD_WriteData(tileCnt + '0');
	}
	

}

//////////////////
//GENERAL FUNCTIONS
//////////////////
// Pins on PORTA are used as input for A2D conversion// The default channel is 0 (PA0)// The value of pinNum determines the pin on PORTA// used for A2D conversion// Valid values range between 0 and 7, where the value// represents the desired pin for A2D conversionvoid Set_A2D_Pin(unsigned char pinNum) {ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;// Allow channel to stabilizestatic unsigned char i = 0;for ( i=0; i<15; i++ ) { asm("nop"); } }
void flashString(char* string) {
	unsigned char i = 1;
	while (i <= 10) {
		if (!(i % 2)) {
			LCD_ClearScreen();
			set_PWM(0);
		}
		else {
			LCD_DisplayString(1, string);
			set_PWM(329.63);
		}
		while (tick < 50) {
			while(!TimerFlag);
			TimerFlag = 0;
			tick++;
		}
		tick = 0;
		i++;
	}
	tick = 0; //reset
}
void getJoystickInput(unsigned short adc) {
	if (adc < 536) {
		if (rightTile == 1 || rightTile == 2) {
			button = rightTile;
		}
		else {
			button = 20; //arbitrary wrong value (thanks Caryn)
		}
	}
	else if (adc > 536) {
		if (rightTile == 4 || rightTile == 16) {
			button = rightTile;
		}
		else {
			button = 130; //arbitrary wrong value (BIRTHDAY)
		}
	}
}

//////////////////
//MAIN FUNCTIONS
//////////////////
void Intro() {
    LCD_DisplayString(1, "* you're gonna have a bad time.");

	for (unsigned i = 0; i < 7; ++i) {
		set_PWM(293.66);
		debounce(15);

		set_PWM(329.63);
		debounce(15);
	}
	set_PWM(0);
	
    LCD_DisplayString(1, "Press any buttonto play!");
	
    while (!button) {
        button = (~PINB & 0x17); //button input (skip PB3 for PWM)
		tick++; //seed value
    }
	while (button) {button = (~PINB & 0x17);} //debounce
}

void Zen() {
    switch(state) { // Transition
        case NewGame:
			tileCnt = 0; //reset everything
            tick = 0;  
			note = 0;
			
			//set up board
            for (unsigned i = 0; i < 4; i++) {
	            xCoord[i] = randX(); 
            }
            rightTile = getXCoord(); //get tile one's button location

			LCD_DisplayString(1, "Press the tile  to start");

			state = Wait;
            break;
            
        case Wait:
            //correct tile starts game
			if (button == rightTile) {
				state = GameTime;
			}
            break;
        
        case GameTime:
			//end if wrong tile
			if (button != rightTile && button != 0) {
				state = End; //lost, so exit loop
				break;
			}
            break;
            
        case End:
			//wait until a button is pressed
			while (!button) {
				button = (~PINB & 0x17);
			}
			state = NewGame;
            break;
            
        default:
            state = NewGame;
            break;
    } // Transition }
    
    switch(state) { // State Action     
        case NewGame:
            break;

        case Wait:            
			displayMatrix();
            break;
        
        case GameTime:
			//correct tile
			if (button == rightTile) {
				playNote();
				tileCnt++;
				moveDown();
				displayLCD(); //only rly need to show if changed
				while (button != 0) {button = (~PINB & 0x17);} //debounce
			}
			displayMatrix();
			button = (~PINB & 0x17); //button input (skip PB3 for PWM)
			break;

        case End:
			//show score
			transmit_x(0); transmit_y(0);
			displayFinalScore();
			
			//error noise
			set_PWM(27.50);
			debounce(30);
			while (button != 0) {button = (~PINB & 0x17);} //debounce
			set_PWM(0);
			break;
            
        default:
            break;
    } // State Action
}



int main(void)
{
    DDRA = 0x00; PORTA = 0xFF; // A2D conversion A0, A1
    DDRB = 0xE8; PORTB = 0x17; // buttons(PB0, PB1, PB2, PB4), PWM is PB3, and LCD data lines(PB5-PB6)
    DDRC = 0xFF; PORTC = 0x00; // shift registers 1 + 2
    DDRD = 0xFF; PORTD = 0x00; // LCD data lines (output)

	unsigned short joystick = ADC;
	state = NewGame; //Zen
	
    LCD_init();
	A2D_init();
    PWM_on();
    TimerSet(5); //.05 seconds
    TimerOn();

    Intro(); //starts game
    srand(tick); //seed with time it took to press the first button to be random

    while(1) {             
        button = (~PINB & 0x17);
		joystick = ADC;

		//basically, lazy mode where you just try to get the correct side
		if (joystick != 536) {
			getJoystickInput(joystick);
			debounce(30);
		}
		
        Zen();
	}
}