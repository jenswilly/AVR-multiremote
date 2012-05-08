# Bluetooth LE Multi-remote project readme

This is the firmware source code for the Bluetooth LE enabled multi-remote controller project. Read more about the project (including schematics and board layouts) here: [http://atomslagstyrken.dk/arduino/tag/remotecontrol/](http://atomslagstyrken.dk/arduino/tag/remotecontrol/).

## Compilation

If you are using a Mac with Xcode you can open the .xcodeproj file and select either "build", "flash" or "fuse" from the targets dropdown and then choose Product -> Build.

As the names indicate, "build" will just compile the project (handy for verifying the syntax), "flash" will compile and flash to the device and "fuse" will set the devices fuses as specified in the Makefile.

If you are _not_ using a Mac with Xcode, just use the normal AVR Libc build commands: `make flash` or `make fuse`.

## Configuration

Edit the Makefile to specify programmer and port. I am using an AVRISP mkII on the USB port.

# Disclaimer

Use at your own risk. Misuse of the code may cause geopolitical instability in susceptible regions. And the code is most likely not complete yet...

# Implementation notes

## IR code format

Since I have already written some functions for sending [RC-5](http://www.sbprojects.com/knowledge/ir/rc5.php) IR codes, I might as well support those.

But I probably not make a _parser_ that can convert raw data with timing information into an RC-5 code. Instead, I'll just use raw on/off timing pairs. I would like to use one byte for each time code and if I use 100 µs resolution I will get from 0.1 ms to 25.5 ms in .1 ms intervals. I think that might be good enough – but time will tell :)

## Storage

IR codes will have differing lengths. And the correct way of storing those in the EEPROM would involve some kind of _allocation table_ and stuff. But instead of messing around with all that I've decided to go for a super-simple solution there every IR code uses a fixed size (128 or 256 bytes – something that is a whole multiple of the EEPROM chip's page size). That way I can easily access the code for a specific index.

The data will be in raw time-on, time-off format and terminated by a 0 value (since a 0 ms pulse will never occur).

Thus, for my LG television which uses the [NEC1](http://www.sbprojects.com/knowledge/ir/nec.php) protocol, an "off" command (address 0x04, command 0xC5) can be stored like this:

	90, 45,
	6, 6, 6, 6, 6, 17, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 17, 6, 17, 6, 6, 6, 17, 6, 17, 6, 17, 6, 17, 6, 17,
	6, 17, 6, 6, 6, 17, 6, 6, 6, 6, 6, 6, 6, 17, 6, 17,
	6, 6, 6, 17, 6, 6, 6, 17, 6, 17, 6, 17, 6, 6, 6, 6
	
For a total of 66 bytes for a code that basically consists of two (!) bytes. Way inefficient! I know.

## Recording IR codes

The algorithm for recording IR signals is based on [this tutorial](http://www.ladyada.net/learn/sensors/ir.html) from Ladyada.  
I use Timer0 on a 100 kHz frequency to keep count of 0.01 ms (10 µs) intervals ("ticks"). If I get more than 2500 ticks, more than 25 ms has passed and we have exceeded the longest time interval we can store in one byte (almost at least – the real max is 25.5).
When waiting for the first transition to LOW (meaning a 38 kHz signal has been detected) I allow up to 200 overflows to occur (for a time of 5 seconds) since I don't store that time anyway.  
After that I store the time interval in .1 ms increments in a data buffer and increase the buffer pointer for every pin LOW (=pulse present) and pin HIGH (=no pulse) state. If an overflow occurs while waiting for a pin LOW state I interpret that as a "signal ended" event (even though it might just be a long no-pulse interval) and overwrite the last pin HIGH data with 0x00 and stop the recording.
