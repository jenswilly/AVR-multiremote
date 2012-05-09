//
//  infrared.h
//  BLEremote
//
//  Created by Jens Willy Johannsen on 07-05-12.
//  Copyright (c) 2012 Greener Pastures. All rights reserved.
//

#ifndef BLEremote_infrared_h
#define BLEremote_infrared_h

/**
 @defgroup jwj_infrared IR Functions
 @brief Functions for sending IR codes.
 
 @code #include "infrared.h" @endcode
 
 IR Functions
 
 These functions send the IR codes as sequences of on-off pulses.
 
 Timer0 is used to generate a 38 kHz PWM signal with a duty cycle of 1/3 on the OC0B (PD5) pin.
 
 Calculate the correct OCR0A and prescaler values at http://www.et06.dk/atmega_timers/.
 @see OCR0A_VALUE
 @see OCR0B_VALUE
 
 @author Jens Willy Johannsen <jens@jenswilly.dk> http://atomslagstyrken.dk/arduino
 
 */

/**@{*/

/** Macros for switching PWM output on – i.e. setting the IR pulse _high_. */
#define IR_HIGH TCCR0A |= (1<< COM0B1)

/** Macros for switching PWM output off – i.e. setting the IR pulse _low_. */
#define IR_LOW TCCR0A &= ~(1<< COM0B1)

/** Sets the TOP value for the 8 bit Timer0 for a PWM frequency of 38 kHz (or as close as possible. Values can be calculated here: http://www.et06.dk/atmega_timers/ */
#define OCR0A_VALUE 53

/** Sets the OCR value for duty cycle. This should be about 1/3 of the OCR0A value. */
#define OCR0B_VALUE 18

/** Define the prescaler flags to match the OCR0A value for a PWM frequency of 38 kHz. Values can be calculated here: http://www.et06.dk/atmega_timers/ */
#define PRESCALER_FLAGS (1<< CS01)

/** The trim value is a number that is _subtracted_ from the specified time durations in order to compensate for the extra CPU cycles used in control loops etc. This value is really best determined by measuring the on/off times on an oscilloscope and adjusting the value (higher values = shorter durations) until the measured duration matches the specified duration. Nominally, the value is in microseconds. */
#define TRIM 300

/** The PINx used for reading the IR input signal. Configure this to match your hardware setup.
 @see IRSENSOR_PIN
 */
#define IRSENSOR_PIN PINB

/** The Pxy used for reading the IR input signal. Configure this to match your hardware setup. Obviously, the bit should match the PINx used.
 @see IRSENSOR_PIN
 */
#define IRSENSOR_BIT PB2

/** Maximum number of ticks (one tick equals TICK_DURATION µs for either a HIGH or LOW pulse. */
#define MAXPULSE 2500

/** Sample period length in microseconds. At every tick the IR input signal is sampled. This must correspond to the @link TICK_OCR @endlink value. */
#define TICK_DURATION 10

/** Number of MAXPULSE durations allowed until timeout occurs. This is used when waiting for the initial signal. If a time equal to TIMEOUT_COUNT * MAXPULSE * TICK_DURATION microseconds passes, a timeout occurs. */
#define TIMEOUT_COUNT 200

/** Output compare value for the 8 bit Timer0 to match the TICK_DURATION. Values can be calculated here: http://www.et06.dk/atmega_timers/
 */
#define TICK_OCR 0xA0

/** Prescaler for the TICK_OCR value to match the TICK_DURATION.
 */
#define TICK_PRESCALER (1<< CS00)

/** Error codes for the learnIR() function
 */
typedef enum {
	/** No error - operation suceeded */
	IRError_NoError = 0,
	/** The signal length exceeds 64 byte pairs and so cannot fit in the 128 byte data buffer */
	IRError_SigTooLong = 1,
	/** No signal was detected */
	IRError_NoSignal = 2,
	/** A HIGH pulse duration exceeded 25 ms */
	IRError_HighPulseTooLong = 3,
	/** A LOW pulse duration exceeded 25 ms */
	IRError_LowPulseTooLong = 4
} IRError;

/** Sends an on-off pulse.
 @param highTime Time in milliseconds for the *ON* part of the pulse.
 @param lowTime Time in milliseconds for the *OFF* part of the pulse.
 
 This method uses `double` parameters for the time specification.
 @see sendPulse
 */
void sendPulseD( double highTime, double lowTime );

/** Sends an on-off pulse.
 @param highTime Time in 100 µs for the *ON* part of the pulse.
 @param lowTime Time in 100 µs for the *OFF* part of the pulse.
 
 This method uses byte integer parameters for the time specification. The times are in 100 µs. Thus, a value of 16 will translate to 1.6 ms.
 Maximum duration for either on or off is 25.5 ms.
 
 @see sendPulseD
 */
void sendPulse( uint8_t highTime, uint8_t lowTime );

/** Sends a NEC1 byte.
 @deprecated Not up-to-date: timing has been manually trimmed.
 */
void sendNECByte( uint8_t data );

/** Sends a NEC1 command byte.
 
 Sends the byte followed by the inverted byte.
 @deprecated Not up-to-date: timing has been manually trimmed.
 */
void sendNECCommand( uint8_t data );

/** Sends a IR pulse sequence
 
 The IR data passed in the `data` parameter must consist of an array of byte pairs terminated by 0x00.
 The first byte is ON time in 0.1 ms and the second byte is the OFF time in 0.1 ms.
 
 Since the array consists of byte pairs terminated by a single 0x00 byte, the length will be an uneven number (although an extra 0x00 will make no difference).
 
 @param data A pointer to a 0x00 terminated array of byte pairs consisting of ON and OFF time values in 0.1 ms.
 @see TRIM
 */
void sendSequence( unsigned char *data );

/** Initializes Timer0 for 38 kHz PWM.
 @note Pin OC0B (PD5) is configured as output and used for the PWM signal.
 */
void initIR();

/** Reads an IR code and stores the on-off time pairs in the specified data buffer.
 
 The signal will be stored as byte pairs where the first byte is ON time in 0.1 ms and the second byte is OFF time in 0.1 ms. The sequence is terminated by a 0x00 byte.
 
 If the signal length exceeds 128 bytes, an @link IRError_SigTooLong @endlink is returned.
 @param data A pointer to 128 bytes large memory block for storing the recorded IR signal.
 @return 0 if a valid signal was recorded. Otherwise a @link IRError @endlink value is returned.
 @retval 0 A valid signal was recorded and stored in the data buffer.
 */ 
IRError learnIR( unsigned char data[] );

/**@}*/

#endif
