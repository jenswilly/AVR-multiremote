//
//  24c_eeprom.c
//  BLEremote
//
//  Created by Jens Willy Johannsen on 07-05-12.
//  Copyright (c) 2012 Greener Pastures. All rights reserved.
//

#include "24c_eeprom.h"
#include "i2cmaster.h"

/* Writes a single byte to the specified address */
void writeByte( int address, uint8_t data )
{
	i2c_start_wait( EEPROM_ADDRESS + I2C_WRITE );
	i2c_write( address >> 8 );	// MSB of address
	i2c_write( address );		// LSB of address
	i2c_write( data );			// Data
	i2c_stop();
}

/* Reads a single byte from the specified address */
uint8_t readByte( int address )
{
	uint8_t data;
	
	// Random access read
	i2c_start_wait( EEPROM_ADDRESS + I2C_WRITE );
	i2c_write( address >> 8 );	// MSB of address
	i2c_write( address );		// LSB of address
	i2c_rep_start( EEPROM_ADDRESS + I2C_READ );
	data = i2c_readNak();		// We don't want more than one byte
	i2c_stop();
	
	return data;	
}

/* Reads current address byte */
uint8_t readCurrentByte()
{
	uint8_t data;
	
	i2c_start_wait( EEPROM_ADDRESS + I2C_READ );
	data = i2c_readNak();		// We don't want more than one byte
	i2c_stop();
	
	return data;
}

/* Reads sequential data from the specified address */
void readData( int address, unsigned char *data, int len )
{
	int i;

	i2c_start_wait( EEPROM_ADDRESS + I2C_WRITE );
	i2c_write( address >> 8 );	// MSB of address
	i2c_write( address );		// LSB of address

	// Start reading
	i2c_rep_start( EEPROM_ADDRESS + I2C_READ );
	for( i = 0; i < len; i++ )
		*data++ = i2c_read( (i < len-1) );	// Send ACK as long as read<len – i.e. we want another byte
	
	i2c_stop();
}

/* Write up to 128 bytes. NB: heed warnings about adderss alignment and data size. */
void writePage( int address, unsigned char *data, uint8_t len )
{
	int i;
	
	i2c_start_wait( EEPROM_ADDRESS + I2C_WRITE );
	i2c_write( address >> 8 );	// MSB of address
	i2c_write( address );		// LSB of address

	// Write data
	for( i = 0; i < len; i++ )
		i2c_write( *data++ );
	
	i2c_stop();
}