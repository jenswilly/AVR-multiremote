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
#include "24c_eeprom.h"
#include "i2cmaster.h"

#define RED_ON		PORTB |= (1<< PB1); PORTB &= ~(1<< PB0);
#define GREEN_ON	PORTB |= (1<< PB0); PORTB &= ~(1<< PB1);
#define YELLOW_ON	PORTB |= (1<< PB0) | (1<< PB1);	
#define ALL_OFF		PORTB &= ~(1<< PB0) & ~(1<< PB1);

#if defined( DEBUG )
#define DEBUG_PRINT( stream, msg, ... ) fprintf( stream, msg,  ##__VA_ARGS__ )
#else
#define DEBUG_PRINT( stream, msg, ... )
#endif

// FSM states
enum States
{
	State_NOOP,
	State_Learn,
	State_Send,
	State_Dump,
	State_DidDisconnect,
	State_SendTestCmd,
	State_SendTestCmd2,
	State_DidConnect
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

unsigned char recordBuffer[256];
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
			state = State_DidDisconnect;
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
		else if( usartBuffer[0] == 'C' )
		{
			// Connected
			state = State_DidConnect;
		}
		

		// (Unknown commands are ignored)
		
		// Reset RX buffer
		usartBufPtr = 0;
	}
}

static inline int addressForCommand( uint8_t commandNumber )
{
	return commandNumber * 2 * 128;	// EEPROM address is simple page size times command index times two (since each command takes up
}

void learn()
{
	IRError status;
#ifdef DEBUG
	unsigned int *data;
	unsigned int i;
#endif

	// Clear the memory buffer
	memset( recordBuffer, 0x00, 256 );

	DEBUG_PRINT( &mystdout, "Ready to record...\n\r" );
			
	// Read IR sequence
	DEBUG_PRINT( &mystdout, "LEARN\r\n" );
	
	YELLOW_ON;	
	status = learnIR( recordBuffer );
	ALL_OFF;
	
	if( status != IRError_NoError )
	{
		// Error:
		DEBUG_PRINT( &mystdout, "Error: %d\n\r", status );
		
		// Restore saved code
		readData( addressForCommand( currentCommand ), recordBuffer, 256 );
		DEBUG_PRINT( &mystdout, "Restored byte sequence from EEPROM" );
		
		// Flash RED
		RED_ON;
		_delay_ms( 500 );
		GREEN_ON;	// We switch back to green since we know we're connected. (Otherwise we couldn't send a code.)
	}
	else
	{
		// No error: print data
		DEBUG_PRINT( &mystdout, "Read code: " );
#ifdef DEBUG
		data = (unsigned int*)recordBuffer;
		i=0;
		while( *data )
		{
			DEBUG_PRINT( &mystdout, "%04x-", *data++ );
			DEBUG_PRINT( &mystdout, "%04x ", *data++ );
			i += 2;
		}
#endif
		DEBUG_PRINT( &mystdout, "00 <end>\r\n" );
		
		// Store command in EEPROM
		DEBUG_PRINT( &mystdout, "Storing %d bytes in EEPROM at address %d... ", i, addressForCommand( nextCommand ));
		writePage( addressForCommand( nextCommand ), recordBuffer, 128 );		// First page
		writePage( addressForCommand( nextCommand )+128	, recordBuffer+128, 128 );	// Second page
		DEBUG_PRINT( &mystdout, "Done.\r\n" );
		
		// Flash GREEN twice
		ALL_OFF;
		_delay_ms( 100 );
		GREEN_ON;			// Flash
		_delay_ms( 100 );
		ALL_OFF;
		_delay_ms( 100 );
		GREEN_ON;			// Flash
		_delay_ms( 100 );
		ALL_OFF;
		_delay_ms( 100 );
		GREEN_ON;			// Remain on (since we're connected)
	}
}

int commandLength( unsigned char *ptr )
{
	unsigned int *data = (unsigned int*)ptr;
	unsigned int i=0;
	
	while( *data++ )
		i++;
	
	return i;
}

int main(void)
{
#ifdef DEBUG
	unsigned int* data;
	int i;
#endif
	
	// Setup
	enable_serial();
	i2c_init();
	sei();
	DDRB |= (1<< PB0);	// PB0 -> output for debugging GREEN
	DDRB |= (1<< PB1);	// PB1 -> output for debugging RED
	DDRD |= (1<< PD5);	// OC0B/PD5 -> output
	
	// Zero the USART buffer
	memset( usartBuffer, '0', RX_BUF_SIZE );
	
	// Init IR
	initIR();
		
	DEBUG_PRINT( &mystdout, "BLE command mode\n\r" );

	RED_ON;
	_delay_ms( 200 );
	YELLOW_ON;
	_delay_ms( 200 );
	GREEN_ON;
	_delay_ms( 200 );
	ALL_OFF;

	// Pull-up on I2C pins PC4 and PC5. Oops – someone forgot to put those resistors on the PCB…
	PORTC |= (1<< PC4) | (1<< PC5);
	
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
					readData( addressForCommand( nextCommand ), recordBuffer, 256 );
					DEBUG_PRINT( &mystdout, "Read %d pairs from EEPROM at address %d for command %d\r\n", commandLength( recordBuffer ), addressForCommand( nextCommand ), nextCommand );
					
					// We now have correct command loaded
					currentCommand = nextCommand;
				}
				
				// Do we have valid data for the specified command?
				if( recordBuffer[0] == 0xFF )
				{
					// No, we don't: flash RED
					RED_ON;
					_delay_ms( 100 );
					ALL_OFF;
					_delay_ms( 100 );
					RED_ON;
					_delay_ms( 100 );
					ALL_OFF;
					_delay_ms( 100 );
					GREEN_ON;
					
					DEBUG_PRINT( &mystdout, "No IR code stored – not transmitting.\r\n" );
				}
				else
				{
					// Yes, we do: send it
					RED_ON;
					DEBUG_PRINT( &mystdout, "Transmitting...\r\n" );
					sendSequence2( recordBuffer );
					GREEN_ON;
				}
				break;

			case State_DidDisconnect:
				// Disconnect: turn off GREEN
				ALL_OFF;
				break;
				
			case State_DidConnect:
				// Connect: turn on GREEN
				GREEN_ON;
				break;
				
			default:
				break;
		}
		
		// Go to idle state
		state = State_NOOP;
	}

    return 0;
}
