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

/* Sends a pulse specified as 0.1 ms durations
 */
void sendPulse( uint8_t highTime, uint8_t lowTime )
{
	IR_HIGH;
	_delay_us( highTime * 100 - TRIM );
	IR_LOW;
	_delay_us( lowTime * 100 - TRIM );
}

/* Sends a complete data sequence.
 * Pass an array of byte pairs where the first byte is ON time in 0.1 ms and the next byte is OFF time in 0.1 ms.
 * The sequence is terminated with a 0x00 byte.
 */
void sendSequence( unsigned char *data )
{
	// Terminate when data = 0
	for( ; *data != 0; data += 2 )
	{
		sendPulse( *data, *(data+1) );
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

IRError learnTEST( unsigned char data[] )
{
	unsigned int buffer[ 128 ];
	int i;
	
	IRError status = IRError_NoError;
	
	DDRD |= (1<< PD6);	// PD6/OC0A -> output
	DDRB |= (1<< PB1);	// PB1/OC1A -> output
	
	// Initialize Timer0 for the specified sample interval (we're not sending IR codes while we're learning so we might as well use the same timer instead of hogging one more timer).
	TCCR0A = (1<< WGM01);	// CTC mode
	TCCR0B = (1<< CS00);	// Start with prescaler 1
	OCR0A = TICK_OCR;		// Sample interval
	TIMSK0 = (1<< OCIE0A);	// Enable OCRA interrupt 
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
		buffer[ pulseBufPr++ ] = pulseDuration;
		
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
		buffer[ pulseBufPr++ ] = pulseDuration;
	}
	
	// Terminate with 0
	data[ pulseBufPr ] = 0;

done:
	initIR();
	
	// Did we get some data?
	if( pulseBufPr > 0 )
	{
		// Yes: convert it from raw sample times to .1 ms intervals
		for( i=0; i<pulseBufPr; i += 2 )
		{
			data[i] = buffer[i] * TICK_DURATION / 100;
			data[i+1] = buffer[i+1] * TICK_DURATION / 100;
		}
	}
	return status;
}


/* Record an IR signal and store it in the specified data buffer
 */
IRError learnIR( unsigned char data[] )
{
	IRError status = IRError_NoError;
	
	// Initialize Timer0 for the specified sample interval (we're not sending IR codes while we're learning so we might as well use the same timer instead of hogging one more timer).
	TCCR0B = 0;					// Timer initially stopped
	OCR0A = TICK_OCR;			// Sample interval
	TCCR0A = (1<< WGM01);		// WGM mode 2: CTC with OCR0A as TOP
	TIMSK0 = (1<< OCIE0A);		// Enable OCRA interrupt 
	sei();
	
	// Init
	pulseDuration = 0;
	pulseBufPr = 0;
	pulseOverflow = 0;

	// Start timer
	TCNT0 = 0;
	TCCR0B = TICK_PRESCALER;	// Start with specified prescaler

	// Wait for HIGH pulse (=pin LOW) or timeout
	while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT)) && pulseOverflow < TIMEOUT_COUNT )
		;
	
	// Start pulse duration counter immediately
	pulseDuration = 0;
	
	// Check for timeout
	if( pulseOverflow >= TIMEOUT_COUNT )
	{
		status = IRError_NoSignal;
		goto quit;
	}
	
	// Reset overflow counter
	pulseOverflow = 0;
	
	while( 1 )
	{
		/// DEBUG1: LED on
		PORTB |= (1<< PB0);

		// Wait for LOW pulse (=pin HIGH) or pulse overflow
		while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT)) == 0 )
			;
		
		// Store HIGH value
		data[ pulseBufPr++ ] = pulseDuration * TICK_DURATION / 100;	// Convert from sample interval to 100 µsecs
		
		// Restart pulse duration counter
		pulseDuration = 0;
		
		// And now check for overflow
		if( pulseOverflow > 0 )
		{
			// Overflow
			status = IRError_HighPulseTooLong;
			fprintf( &mystdout, "Overflow: %d; Duration: %d; BufPtr: %d\n\r", pulseOverflow, pulseDuration, pulseBufPr );
			goto quit;
		}
		
		/// DEBUG1: LED off
		PORTB &= ~(1<< PB0);

		// Wait for HIGH pulse (=pin LOW) or pulse overflow
		while( (IRSENSOR_PIN & (1<< IRSENSOR_BIT))  )
			;
		
		// Store LOW value
		data[ pulseBufPr++ ] = pulseDuration * TICK_DURATION / 100;	// Convert from sample interval to 100 µsecs
		
		// Restart pulse duration counter
		pulseDuration = 0;
		
		// And check for overflow
		if( pulseOverflow > 0 )
		{
			// Overflow when LOW is simply interpreted as "signal ended"
			break;
		}
	}
	
	// Replace last LOW value with 0x00 to terminate
	data[ pulseBufPr-1 ] = 0;
	
quit:
	// Re-initialize Timer0 for IR PWM
	initIR();
	
	/// DEBUG2
	PORTB &= ~(1<< PB0);
	
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