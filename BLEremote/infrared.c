//
//  infrared.c
//  BLEremote
//
//  Created by Jens Willy Johannsen on 07-05-12.
//  Copyright (c) 2012 Greener Pastures. All rights reserved.
//

#include <avr/io.h>
#include <util/delay.h>
#include "infrared.h"

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

void sendPulse( uint8_t highTime, uint8_t lowTime )
{
	IR_HIGH;
	_delay_us( highTime * 100 - TRIM );
	IR_LOW;
	_delay_us( lowTime * 100 - TRIM );
}

void sendSequence( unsigned char *data )
{
	// Terminate when data = 0
	for( ; *data != 0; data += 2 )
	{
		sendPulse( *data, *(data+1) );
	}
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
	IR_LOW;
}