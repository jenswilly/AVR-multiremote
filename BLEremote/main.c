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
#include <string.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "infrared.h"

#define MODE 3	// 1=record, 2=play back, 3=auto

// FSM states
enum States
{
	State_NOOP,
	State_Learn,
	State_Send
};
volatile enum States state = State_NOOP;
volatile int cmdNumber;

/* Precompiler stuff for calculating USART baud rate value 
 * Otherwise, use this page: http://www.wormfood.net/avrbaudcalc.php?postbitrate=9600&postclock=12&bit_rate_table=on
 */
//#define USART_BAUDRATE 2400	// error 0.2% for 1.5 MHz
#define USART_BAUDRATE 9600	// error 0.2%
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1) 
#define RX_BUF_SIZE	20
unsigned char usartBuffer[RX_BUF_SIZE];		// USART receive buffer
volatile unsigned char usartBufPtr=0;		// USART buffer pointer

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
unsigned char EEMEM EECommand[128];
unsigned int EEMEM EEcmdLength;
unsigned int i;

// Enables USART comm
void enable_serial()
{
	UCSR0B |= (1<< RXEN0) | (1<< TXEN0);	// Enable TX and RX (8N1 serial format set by default)
	UBRR0H = (BAUD_PRESCALE >> 8) & 0x0F;	// High part of baud rate masked so bits 15:12 are writted as 0 as per the datasheet (19.10.5)
	UBRR0L = BAUD_PRESCALE;					// Low part of baud rate. Writing to UBBRnL updates baud prescaler
	UCSR0B |= (1<< RXCIE0);					// Enable USART RX interrupt
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

// Interrupt handler for USART receive complete
ISR( USART_RX_vect ) 
{ 
	// Prevent buffer overflow
	if( usartBufPtr >= RX_BUF_SIZE-1 )
		usartBufPtr = 0;
	
	// Grab the data and but it into the buffer and increase pointer
	usartBuffer[ usartBufPtr++ ] = UDR0;
	
	// TMP: Echo char to serial stream
	fputc( usartBuffer[ usartBufPtr-1 ], &mystdout );
	if( usartBuffer[ usartBufPtr-1 ] == 0x0A )
		fputc( '\r', &mystdout );
	
	// Check for "terminate command" byte (0x0A, \n, LF)
	if( usartBuffer[ usartBufPtr-1 ] == 0x0A )
	{
		if( usartBuffer[0] == 'S' )
		{
			// Send command
			state = State_Send;
		}
		else if( usartBuffer[0] == 'L' )
		{
			// Learn signal
			state = State_Learn;
		}

		// (Unknown commands are ignored)
		
		// Reset RX buffer
		usartBufPtr = 0;
	}
}

void learn()
{
	IRError status;
	unsigned char *data;

	// Clear the memory buffer
	memset( recordBuffer, 0x00, 128 );

	fprintf( &mystdout, "Ready to record...\n\r" );
			
	// Read IR sequence
	fprintf( &mystdout, "LEARN\r\n" );
	
	PORTB |= (1<< PB0);		
	status = learnIR( recordBuffer );
	PORTB &= ~(1<< PB0);
	
	if( status != IRError_NoError )
	{
		// Error:
		fprintf( &mystdout, "Error: %d\n\r", status );
		
		// Restore saved code
		eeprom_read_block( recordBuffer, EECommand, 128 );
		fprintf( &mystdout, "Restored %d byte sequence from EEPROM:", strlen( (char*)recordBuffer )+1 );
	}
	else
	{
		// No error: print data
		fprintf( &mystdout, "Read code: " );
		data = recordBuffer;
		while( *data )
			fprintf( &mystdout, "%02d-", *data++ );
		fprintf( &mystdout, "00 <end>\r\n" );
		
		// Store command in EEPROM
		i = strlen( (char*)recordBuffer )+1;
		fprintf( &mystdout, "Storing %d bytes in EEPROM\r\n", i );
		eeprom_busy_wait();
		eeprom_write_word( &EEcmdLength, i );
		
		eeprom_busy_wait();
		eeprom_write_block( recordBuffer, EECommand, i+1 );
	}
}

void playback()
{
	unsigned char *data;

	fprintf( &mystdout, "Ready to play back...\n\r" );
	
	// Read command from eeprom
	eeprom_busy_wait();
	eeprom_read_block( recordBuffer, EECommand, 128 );
	fprintf( &mystdout, "Read %d bytes from EEPROM:", strlen( (char*)recordBuffer )+1 );
	data = recordBuffer;
	
	for( i=0; *data && i<128; i++, data++ )
		fprintf( &mystdout, "%02d-", *data++ );
	fprintf( &mystdout, "00 <end>\r\n" );
	// Main loop
	for( ;; )
	{
		// Send sequence
		fprintf( &mystdout, "Transmitting...\r\n" );
		sendSequence( recordBuffer );
		
		// Wait
		_delay_ms( 3000 );
	}
}

int main(void)
{
	int cmdLength=0;
	
	// Setup
	enable_serial();
	sei();
	DDRB |= (1<< PB0);	// PB0 -> output for debugging LED

	// Init IR
	initIR();
		
#if MODE == 1
	learn();
#elif MODE == 2
	playback();
#elif MODE == 3
	fprintf( &mystdout, "BLE command mode\n\r" );

	// Start by reading command length from EEPROM
	cmdLength = eeprom_read_word( &EEcmdLength );
	fprintf( &mystdout, "Command length in EEPROM is: %d\r\n", cmdLength );
	
	// Read command sequence from EEPROM
	eeprom_read_block( recordBuffer, EECommand, 128 );
	fprintf( &mystdout, "Read %d bytes from EEPROM:", strlen( (char*)recordBuffer )+1 );

	// Main loop
	for( ;; )
	{
		// Wait until a command has been received
		while( state == State_NOOP )
			;
		
		// Что делать?
		switch( state )
		{
			case State_Learn:
				// Lean IR code
				learn();
				break;
				
			case State_Send:
				// Send sequence: do we have a sequence loaded?
				if( recordBuffer[0] == 0xFF )
					// No, we don't
					fprintf( &mystdout, "No IR code stored – not transmitting.\r\n" );
				else
				{
					// Yes, we do: send it
					fprintf( &mystdout, "Transmitting...\r\n" );
					sendSequence( recordBuffer );
				}
				break;
				
			default:
				break;
		}
		
		// Go to idle state
		state = State_NOOP;
	}
#endif


    return 0;
}
