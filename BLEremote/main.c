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

usigned char recordBuffer[128];

int main(void)
{
	IRError status;
	
	// Init IR
	initIR();
	
	// Main loop
	for( ;; )
	{
		// Send sample "off" command
		// sendSequence( testCmd );
		
		status = learnIR();
		if( status != IRError_NoError )
		{
			// Error:
		}
		else
		{
			// No error: print data
		}
		
		// Wait 3 secs
		_delay_ms( 3000 );
	}

    return 0;
}
