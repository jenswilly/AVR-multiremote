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
#include "infrared.h"

#define MODE 3	// 1=record, 2=play back, 3=auto

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
unsigned char EEMEM EECommand[128];
unsigned int EEMEM EEcmdLength;
unsigned int i;

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

void learn()
{
	IRError status;
	unsigned char *data;

	fprintf( &mystdout, "Ready to record...\n\r" );
	
	// Main loop
	for( ;; )
	{
		// Clear the memory buffer
		memset( recordBuffer, 0x00, 128 );
		
		// Read IR sequence
		fprintf( &mystdout, "LEARN\r\n" );
		
		PORTB |= (1<< PB0);		
		status = learnTEST( recordBuffer );
		PORTB &= ~(1<< PB0);
		
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
				fprintf( &mystdout, "%02d-", *data++ );
			fprintf( &mystdout, "00 <end>\r\n" );
			
			// Store command in EEPROM
			i = strlen( (char*)recordBuffer )+1;
			fprintf( &mystdout, "Storing %d bytes in EEPROM\r\n", i );
			eeprom_busy_wait();
			eeprom_write_word( &EEcmdLength, i );
			
			eeprom_busy_wait();
			eeprom_write_block( recordBuffer, EECommand, i+1 );
			
			// Transmit it
			fprintf( &mystdout, "Transmitting\r\n" );
			sendSequence( recordBuffer );
		}
		
		// Wait 3 secs
		_delay_ms( 3000 );
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
	// Setup
	enable_serial();
	DDRB |= (1<< PB0);	// PB0 -> output

	
	// Init IR
	initIR();
		
#if MODE == 1
	learn();
#elif MODE == 2
	playback();
#elif MODE == 3
	fprintf( &mystdout, "Auto mode\n\r" );

	// Start by reading command length from EEPROM
	eeprom_busy_wait();
	i = eeprom_read_word( &EEcmdLength );
	fprintf( &mystdout, "Command length in EEPROM is: %d\r\n", i );
	
	// If length != 255, we have a stored command
	if( i != -1 )
		playback();
	else 
		learn();
#endif


    return 0;
}
