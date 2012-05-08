/*
 *  main.c
 *  BLEremote
 *
 *  Created by Jens Willy Johannsen on 07-05-12.
 *  Copyright Greener Pastures 2012. All rights reserved.
 *
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include "infrared.h"

/* Precompiler stuff for calculating USART baud rate value 
 * Otherwise, use this page: http://www.wormfood.net/avrbaudcalc.php?postbitrate=9600&postclock=12&bit_rate_table=on
 */
//#define USART_BAUDRATE 2400	// error 0.2% for 1.5 MHz
#define USART_BAUDRATE 9600	// error 0.2%
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1) 

// Serial stream
static int uart_putchar( char c, FILE *stream );
FILE mystdout = FDEV_SETUP_STREAM( uart_putchar, NULL, _FDEV_SETUP_WRITE );

unsigned char testCmd[] = 
{
	90, 45,
	06, 06, 06, 06, 06, 17, 06, 06, 06, 06, 06, 06, 06, 06, 06, 06,
	06, 17, 06, 17, 06, 06, 06, 17, 06, 17, 06, 17, 06, 17, 06, 17,
	06, 17, 06, 06, 06, 17, 06, 06, 06, 06, 06, 06, 06, 17, 06, 17,
	06, 06, 06, 17, 06, 06, 06, 17, 06, 17, 06, 17, 06, 06, 06, 06, 
	06, 11, 00
};

unsigned char recordBuffer[128];

// Enables USART comm
// Remember to call this method after waking up
void enable_serial()
{
	UCSR0B |= (1<< RXEN0) | (1<< TXEN0);			// Enable TX and RX
	//	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);		// 8N1 serial format. Set by default
	UBRR0H = (BAUD_PRESCALE >> 8) & 0x0F;			// High part of baud rate masked so bits 15:12 are writted as 0 as per the datasheet (19.10.5)
	UBRR0L = BAUD_PRESCALE;							// Low part of baud rate. Writing to UBBRnL updates baud prescaler
}

static int uart_putchar(char c, FILE *stream)
{
	// Wait until we're ready to send
	while( !(UCSR0A & (1<< UDRE0)))
		;
	
	// Send byte
	UDR0 = c;
	return 0;
}

int main(void)
{
	IRError status;
	unsigned char *data;
	
	// Setup
	enable_serial();
	DDRB |= (1<< PB0);	// PB0 -> output

	
	// Init IR
	initIR();
	
	fprintf( &mystdout, "Ready...\n\r" );
	// Main loop
	for( ;; )
	{
		// Send sample "off" command
		// sendSequence( testCmd );
		
		status = learnIR( recordBuffer );
		if( status != IRError_NoError )
		{
			// Error:
			fprintf( &mystdout, "Error: %d\n\r", status );
		}
		else
		{
			// No error: print data
			fprintf( &mystdout, "Read code: " );
			data = recordBuffer;
			while( *data )
				fprintf( &mystdout, "%02x", *data++ );
			fprintf( &mystdout, "<end>\r\n" );
		}
		
		// Wait 3 secs
		_delay_ms( 3000 );
	}

    return 0;
}
