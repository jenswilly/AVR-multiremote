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

// FSM states
enum States
{
	State_NOOP,
	State_Learn,
	State_Send,
	State_Dump,
	State_SendTestCmd,
	State_SendTestCmd2
};
volatile enum States state = State_NOOP;
volatile uint8_t nextCommand = 0;
uint8_t currentCommand = 0;

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
unsigned char testCmd2[] = {89,44,06,05,05,05,06,16,06,05,05,05,05,05,06,05,06,05,06,16,06,16,06,05,05,16,06,16,06,16,05,16,05,16,06,05,06,05,05,05,06,16,06,05,05,05,05,05,06,05,05,16,05,16,06,16,05,05,05,16,05,16,06,16,06,16,06,00};

unsigned char recordBuffer[128];
unsigned char EEMEM EECommand1[128];
unsigned char EEMEM EECommand2[128];
unsigned char EEMEM EECommand3[128];
unsigned char EEMEM EECommand4[128];
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
		// Parse command number
		// NOTE: This *may* result in an outdated or garbage value if no command number has been sent (e.g. for 'T' and 'Y' commands).
		nextCommand = (usartBuffer[2] - '0')*100 + (usartBuffer[3] - '0')*10 + (usartBuffer[4] - '0');
		
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
		else if( usartBuffer[0] == 'D' )
		{
			// Write signal to serial
			state = State_Dump;
		}
		else if( usartBuffer[0] == 'T' )
		{
			// Write signal to serial
			state = State_SendTestCmd;
		}
		else if( usartBuffer[0] == 'Y' )
		{
			// Write signal to serial
			state = State_SendTestCmd2;
		}
		

		// (Unknown commands are ignored)
		
		// Reset RX buffer
		usartBufPtr = 0;
	}
}

static inline void* addressForCommand( uint8_t commandNumber )
{
	/// TEMP – should be a simple calculation instead
	if( commandNumber == 1 )
		return EECommand1;
	else if( commandNumber == 2 )
		return EECommand2;
	else if( commandNumber == 3 )
		return EECommand3;
	else if( commandNumber == 4 )
		return EECommand4;
	
	/// TODO: no error handling or check for in-range command numbers...
	else
		return 0;
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
		eeprom_read_block( recordBuffer, addressForCommand( currentCommand ), 128 );
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
		fprintf( &mystdout, "Storing %d bytes in EEPROM at address %p\r\n", i, addressForCommand( nextCommand ));
		eeprom_busy_wait();
		eeprom_write_word( &EEcmdLength, i );
		
		eeprom_busy_wait();
		eeprom_write_block( recordBuffer, addressForCommand( nextCommand ), i+1 );
	}
}

int main(void)
{
	unsigned char* data;
	
	// Setup
	enable_serial();
	sei();
	DDRB |= (1<< PB0);	// PB0 -> output for debugging LED

	// Zero the USART buffer
	memset( usartBuffer, '0', RX_BUF_SIZE );
	
	// Init IR
	initIR();
		
	fprintf( &mystdout, "BLE command mode\n\r" );

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
				// Send sequence: have we already loaded the specified command?
				if( currentCommand != nextCommand )
				{
					// No: load it from EEPROM into SRAM first
					eeprom_read_block( recordBuffer, addressForCommand( nextCommand ), 128 );
					fprintf( &mystdout, "Read %d bytes from EEPROM at address %p for command %d\r\n", strlen( (char*)recordBuffer )+1, addressForCommand( nextCommand ), nextCommand );
					
					// We now have correct command loaded
					currentCommand = nextCommand;
				}
				
				// Do we have valid data for the specified command?
				if( recordBuffer[0] == 0xFF )
					// No, we don't
					fprintf( &mystdout, "No IR code stored – not transmitting.\r\n" );
				else
				{
					// Yes, we do: send it
					PORTB |= (1<< PB0);		
					fprintf( &mystdout, "Transmitting...\r\n" );
					sendSequence( recordBuffer );
					PORTB &= ~(1<< PB0);
				}
				break;
				
			case State_Dump:
				fprintf( &mystdout, "Stored code: " );
				data = recordBuffer;
				while( *data )
					fprintf( &mystdout, "%02d-", *data++ );
				fprintf( &mystdout, "00 <end>\r\n" );
				break;
				
			case State_SendTestCmd:
				PORTB |= (1<< PB0);		
				fprintf( &mystdout, "Transmitting hardcoded test command...\r\n" );
				sendSequence( testCmd );
				PORTB &= ~(1<< PB0);
				
			case State_SendTestCmd2:
				PORTB |= (1<< PB0);		
				fprintf( &mystdout, "Transmitting hardcoded test command...\r\n" );
				sendSequence( testCmd2 );
				PORTB &= ~(1<< PB0);
				
			default:
				break;
		}
		
		// Go to idle state
		state = State_NOOP;
	}

    return 0;
}
