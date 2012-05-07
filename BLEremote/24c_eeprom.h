//
//  24c_eeprom.h
//  BLEremote
//
//  Created by Jens Willy Johannsen <jens@jenswilly.dk> http://atomslagstyrken.dk/arduino on 07-05-12.
//  Copyright (c) 2012 Greener Pastures. All rights reserved.
//

#include <avr/io.h>

/**
@defgroup jwj_24ceeprom 24C EEPROM library
 @brief Utility functions for using 24C series EEPROMs.
 
 @code #include <24c_eeprom.h> @endcode

 24C EEPROM Library
 
 The 24C EEPROM library contains helper functions for using the 24C (specifically, M24C64) series EEPROMs.
 
@note
 This library requires I2C functions compatible with <a href="http://jump.to/fleury">Peter Fleury's I2C (TWI) Master Software Library</a>!

 @author Jens Willy Johannsen <jens@jenswilly.dk> http://atomslagstyrken.dk/arduino
 
 */

/**@{*/

/** Defines the I2C device address of the EEPROM device. This address must already be shifted one byte to the left to make room for the read/write bit. So for a device where the upper four bits are configured as 1010 (e.g. M24C64) and the Chip Enable pins E2:E0 are all tied to ground, the final address is 0xA0. */
#define EEPROM_ADDRESS 0xA0

/** Writes a single byte to the specified destination address.
 @param address The 16 bit memory address to write to.
 @param data The 8 bit value to store.
 */
void writeByte( int address, uint8_t data );

/** Writes a 32 byte page to the specified address.
 @param address The 16 bit memory address to write to.
 @param data Pointer to the data to write.
 @param len Length of data to write. **NOTE: This may not exceed 32 bytes**
 
 A page is 32 bytes long. When writing a page, an internal memory address pointer is increased for every byte. But only the last 5 bits are increased. So if the data is longer than 32 bytes or the address is not aligned to the start of a page, the address pointer will wrap around to the start of the page and continue writing.
 
 @warning To align the address to the start of a page, the last five bits of the address _must_ be 0. If this is not the case, the address pointer will wrap around to the start of the page and continue so the data written will not be in the same sequence as the original data.
 */
void writePage( int address, unsigned char *data, uint8_t len );

/** Reads one byte from the specified address.
 @param address The 16 bit address to read from.
 @return The 8 bit value stored at the specified address.
 */
uint8_t readByte( int address );

/** Reads the byte at the current memory address. The address pointer is incremented after reading.
 @returns The 8 bit value stored at the current memory address.
 */
uint8_t readCurrentByte();


/**
 @brief Reads sequential data.
 @param address The 16 bit memory address to start reading from.
 @param data Pointer to a buffer to receive read data.
 @param len Number of bytes to read.
 
 This function reads `len` bytes from the specified address and forward.
 */
void readData( int address, unsigned char *data, int len );
/**@}*/