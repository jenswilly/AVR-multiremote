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
#include "infrared.h"

unsigned char testCmd[] = 
{
	90, 45,
	06, 06, 06, 06, 06, 17, 06, 06, 06, 06, 06, 06, 06, 06, 06, 06,
	06, 17, 06, 17, 06, 06, 06, 17, 06, 17, 06, 17, 06, 17, 06, 17,
	06, 17, 06, 06, 06, 17, 06, 06, 06, 06, 06, 06, 06, 17, 06, 17,
	06, 06, 06, 17, 06, 06, 06, 17, 06, 17, 06, 17, 06, 06, 06, 06, 
	06, 11, 00
};

int main(void)
{
	// Init IR
	initIR();
	
	// Main loop
	for( ;; )
	{
		// Lead-in
//		sendPulseD( 8.8, 4.3 );
//		
//		// Address 04, NEC1 protocol
//		sendNECByte( 0x04 );
//		sendNECByte( ~0x04);
//		
//		// Command C5: discrete power-off
//		sendNECCommand( 0xC5 );
//		
//		// Lead-out
//		sendPulseD( 0.36, 80 );
		

		// Send sample "off" command
		sendSequence( testCmd );
		
		// Wait 3 secs
		_delay_ms( 3000 );
	}

    return 0;
}
