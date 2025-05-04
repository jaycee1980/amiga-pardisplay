/*******************************************************************************************************************//**
	DiagROM Parallel Port display
	\author Peter Mulholland
***********************************************************************************************************************/

#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/cpufunc.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

//----------------------------------------------------------------------------------------------------------------------
/*
	ATtiny48/88

	lfuse 0x6E - internal 8MHz oscillator, divided to 1MHz
	hfuse 0xDF - brownout detection disabled, SPI programming enabled

	PA0		input, BUSY pin of parallel port
	PA1		input, PAPER OUT pin of parallel port

	PB0		output, digit 1 enable
	PB1		output, digit 2 enable
	PB6-7	input, parallel port D6-D7

	PC0-5	input, parallel port D0-D5

	PD0-7	output, segment anodes

*/

//#define _DEBUG

//----------------------------------------------------------------------------------------------------------------------
// PROGMEM data

// Digit segment definitions, 1 = segment ON
const uint8_t c_digits[] PROGMEM = {
//    .GFEDCBA
	0b00111111,		// 0
	0b00000110,		// 1
	0b01011011,		// 2
	0b01001111,		// 3
	0b01100110,		// 4
	0b01101101,		// 5
	0b01111101,		// 6
	0b00000111,		// 7
	0b01111111,		// 8
	0b01101111,		// 9
	0b01110111,		// A
	0b01111100,		// b
	0b01011000,		// c
	0b01011110,		// d
	0b01111001,		// E
	0b01110001		// F
};

//----------------------------------------------------------------------------------------------------------------------
// ISRs

ISR(TIMER0_OVF_vect)
{
#ifdef _DEBUG
	// Make a pulse on our debugging pin
	PINB = _BV(PINB5);
	_NOP();
	PINB = _BV(PINB5);
#endif

	// Cause an immediate overflow to make our interrupts occur with no further division
	TCNT0 = 255;
}

//----------------------------------------------------------------------------------------------------------------------
// Main

int main(void)
{
	uint8_t state = 0;		// Current digit being displayed
	uint8_t val = 0;		// The value last read from the parallel port

	// Disable the watchdog timer
	wdt_disable();

	// We're only using Timer 0 - turn the rest off
	PRR = _BV(PRTWI) | _BV(PRTIM1) | _BV(PRSPI) | _BV(PRADC);

	// Set port A so all pins are inputs. Set pullups on unused inputs PA2 and PA3
	PORTA = _BV(PORTA2) | _BV(PORTA3);
	DDRA  = 0;

#ifndef _DEBUG
	// Set port B so PB0 and PB1 are outputs, all others are inputs.
	// PB0-1 should be low, PB6-PB7 have no pullups
	PORTB = _BV(PORTB2) | _BV(PORTB3) | _BV(PORTB4) | _BV(PORTB5);
	DDRB  = _BV(DDB0) | _BV(DDB1);
#else
	// Make PB5 (the SCK pin) an output for debugging the interrupt speed
	PORTB = _BV(PORTB2) | _BV(PORTB3) | _BV(PORTB4);
	DDRB = _BV(DDB0) | _BV(DDB1) | _BV(DDB5);
#endif

	// Set port C so PC0-5 are inputs with no pullups
	PORTC = _BV(PORTC6) | _BV(PORTC7);
	DDRC  = 0;

	// Set port D to all outputs, initially low
	PORTD = 0;
	DDRD  = 0xFF;

	// Wait for port states to synchronise
	_NOP(); _NOP();

	// Set up Timer 0 interrupt so we wake up at regular intervals
	TIFR0 = 0;								// Clear timer flags
	TCNT0 = 255;							// Immediate overflow
	TIMSK0 = _BV(TOIE0);					// Timer overflow interrupt on
	TCCR0A = (_BV(CS01) | _BV(CS00));		// 1MHz / 64 = 15.625KHz

	// Set sleep mode to Idle so that the Timer0 overflow interrupt can wake us up
	set_sleep_mode(SLEEP_MODE_IDLE);

	// Interrupts on!
	sei();

	// Loop
	for (;;)
	{
		uint8_t digit = 0;

		// Turn off both digits
		PORTB &= ~(_BV(PORTB0) | _BV(PORTB1));

		if (state == 0)
		{
			// read the parallel port value
			val = (PINB & 0xC0) | (PINC & 0x3F);

			// Display first digit
			digit = pgm_read_byte(&c_digits[val >> 4]);

			if (PINA & _BV(PINA0))			// if BUSY pin of parallel port is high
				digit |= 0x80;				// Turn on the decimal point

			PORTD = digit;
			PORTB |= _BV(PORTB0);
		}
		else
		{
			// Display second digit
			digit = pgm_read_byte(&c_digits[val & 0x0F]);

			if (PINA & _BV(PINA1))			// if POUT pin of parallel port is high
				digit |= 0x80;				// Turn on the decimal point

			PORTD = digit;
			PORTB |= _BV(PORTB1);
		}

		// Toggle digit to show
		state = !state;

		// Until the next interrupt
		sleep_enable();
		sleep_cpu();
	}
	
	// We never get here!
	return 0;
}
