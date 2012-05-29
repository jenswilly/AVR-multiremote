//
//  infrared.c
//  BLEremote
//
//  Created by Jens Willy Johannsen on 07-05-12.
//  Copyright (c) 2012 Greener Pastures. All rights reserved.
//

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "infrared.h"

extern FILE mystdout;

// The following variables are used when learning IR codes
volatile unsigned int pulseDuration;
volatile unsigned int pulseBufPr;
volatile unsigned int pulseOverflow;

unsigned int* pulseBuffer;

/* Sends a high-low pulse
 * Specify times for high duration and low duration in ms
 */
void sendPulseD( double highTime, double lowTime )
{
	IR_HIGH;
	_delay_ms( highTime );
	IR_LOW;
	_delay_ms( lowTime );
}

/* Sends a byte
 * LSB first
 */
void sendNECByte( uint8_t data )
{
	// LSB first
	int i;
	for( i=0; i<8; i++ )
		if( (data >> i) & 0x01 )
			// Send "1" bit
			sendPulseD( 0.36, 1.48 );
		else
			// Send "0" bit
			sendPulseD( 0.36, 0.36 );
}

/* Sends the command byte
 * followed by the inverted command byte
 */
void sendNECCommand( uint8_t data )
{
	sendNECByte( data );
	sendNECByte( ~data );
}

/* JVC commands consist of a 8.4 - 4.2 ms lead-in followed by one address byte (LSB) and one command byte (LSB).
 * Logical 1 is 0.56 - 1.54 ms
 * Logical 0 is 0.56 - 0.49 ms
 *
 * Protocol description: http://www.sbprojects.com/knowledge/ir/jvc.php
 * Rotel DVD remote codes can be found here: http://www.remotecentral.com/cgi-bin/codes/rotel/rdv-1080/
 * Commands are specified as 0000 (learned) 006d (38 kHz) 0001 (one seq 1 pair) 0011 (17 seq 2 pairs – last is lead out)
 *  followed by 013e 009e (lead-in) and 0014 003c (logical 1) and 0014 0014 (logical 0).
 */
void sendJVCByte( uint8_t data )
{
	
}

/* Sends a pulse specified as 0.01 ms durations
 */
void sendPulse( unsigned int highTime, unsigned int lowTime )
{
	IR_HIGH;
	_delay_us( highTime * TICK_DURATION - TRIM );
	IR_LOW;
	_delay_us( lowTime * TICK_DURATION - TRIM );
}

/* Timer-based method for sending a command sequence.
 * TIMER1 is set up in CTC mode with the same period as when learning IR codes.
 * pulseDuration is set to the learned values and decreased on every compare match interrupt.
 * When pulseDuration is at zero, the IR signal is toggled and the data pointer is increased to next value.
 * If the pointer points to zero, the IR is set low and the timer stopped.
 */
void sendSequence2( unsigned char *data )
{
	// Point to sequence data
	pulseBuffer = (unsigned int*)data;	// Typecast to int* so increments work
	
	// Configure TIMER1 with the same sample interval as when learning.
	OCR1A = TICK_OCR;						// Use same period as the sampling interval
	TCCR1B = (1<< WGM12) | TICK_PRESCALER1;	// WGM mode 4: CTC w/ OCR1A as TOP and prescaler to match sampling interval period
	TIMSK1 = (1<< OCIE1A);					// Enable output compare A match interrupt
	
	// IR high for the first value
	IR_HIGH;
	
	// Set pulseDuration to first value
	pulseDuration = *pulseBuffer++;
}

/* Sends a complete data sequence.
 * Pass an array of byte pairs where the first byte is ON time in 0.1 ms and the next byte is OFF time in 0.1 ms.
 * The sequence is terminated with a 0x00 byte.
 */
void sendSequence( unsigned char *data )
{
	unsigned int *ptr = (unsigned int*)data;	// Typecast to int* so increments work correctly
	
	// Terminate when data = 0
	for( ; *ptr != 0; ptr += 2 )
	{
		sendPulse( *ptr, *(ptr+1) );
	}
}

/* Timer1 Compare Match interrupt handler
 * This is used when sending commands.
 */
ISR( TIMER1_COMPA_vect )
{
	// No: decrease counter and toggle IR if we're at zero
	if( --pulseDuration == 0 )
	{
		IR_TOGGLE;
	
		// Set new duration. Duration is specified in TICK_DURATION periods.
		pulseDuration = *pulseBuffer++;

		// Are we at end of sequence?
		if( pulseDuration == 0 )
		{
			// Yes: IR low
			IR_LOW;
			
			// Stop timer
			TCCR1B = 0;
		}
		pulseDuration++;	// Increase pulse duration by one
	}
}

/* Timer0 Compare Match interrupt handler
 */
ISR( TIMER0_COMPA_vect )
{
	// Increase pulse duration
	pulseDuration++;
	
	// Check for pulse duration overflow
	if( pulseDuration >= MAXPULSE )
	{
		pulseOverflow++;
		pulseDuration = 0;
	}
}

/* Record an IR signal and store it in the specified data buffer
 */
IRError learnIR( unsigned char *data )
{
	unsigned int *ptr = (unsigned int*)data;	// Typecast to unsigned int*
	
	IRError status = IRError_NoError;
	
	DDRD |= (1<< PD6);	// PD6/OC0A -> output
	DDRB |= (1<< PB1);	// PB1/OC1A -> output
	
	// Initialize Timer0 for the specified sample interval (we're not sending IR codes while we're learning so we might as well use the same timer instead of hogging one more timer).
	TCCR0A = (1<< WGM01);		// CTC mode
	TCCR0B = TICK_PRESCALER;	// Start with prescaler 1
	OCR0A = TICK_OCR;			// Sample interval
	TIMSK0 = (1<< OCIE0A);		// Enable OCRA interrupt 
	sei();
	
	// Init
	pulseDuration = 0;
	pulseBufPr = 0;
	pulseOverflow = 0;
	
	// Wait for initial signal
	while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT)) && pulseOverflow < TIMEOUT_COUNT )
		;

	// Check for timeout
	if( pulseOverflow >= TIMEOUT_COUNT )
	{
		status = IRError_NoSignal;
		goto done;
	}

	while( 1 )
	{
		// Wait for LOW pulse (=pin HIGH) or pulse overflow
		cli();
		pulseDuration = 0;
		pulseOverflow = 0;
		sei();
		while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT)) == 0 && pulseOverflow == 0 )
			;
		
		if( pulseOverflow > 0 )
		{
			// HIGH signal too long to store in one byte
			status = IRError_HighPulseTooLong;
			goto done;
		}

		// Store HIGH value
		ptr[ pulseBufPr++ ] = pulseDuration;
		
		// Wait for HIGH pulse (=pin LOW) or pulse overflow
		cli();
		pulseDuration = 0;
		sei();
		while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT)) && pulseOverflow == 0 )
			;
		
		if( pulseOverflow > 0 )
		{
			// LOW overflow is interpreted as "signal end"
			break;
			goto done;
		}
		
		// Store LOW value
		ptr[ pulseBufPr++ ] = pulseDuration;
	}
	
	// Terminate with 0
	ptr[ pulseBufPr ] = 0;

done:
	// Re-initialize for IR sending
	initIR();

	return status;
}

/* Initializes the PWM timer */
void initIR()
{
	DDRD |= (1<< PD5);							// OC0B/PD5 as output
	OCR0A = OCR0A_VALUE;						// PWM frequency
	OCR0B = OCR0B_VALUE;						// Duty cycle
	TCCR0A &= ~(1<< COM0B1);					// Clear OC0B on Compare Match, set OC0B at BOTTOM, (non-inverting mode)
	TCCR0A = (1<< WGM01) | (1<< WGM00);			// WGM mode 7: Fast PWM with OCR0A as TOP
	TCCR0B = (1<< WGM02) | PRESCALER_FLAGS;		// WGM mode 7 (cont'd) and prescaler flags
	TIMSK0 = 0;									// No interrupts
	IR_LOW;
}