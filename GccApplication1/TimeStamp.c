/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 ************************************
   Time-Stamp Func 
 ************************************
*/

/*
TBD:

*/

#include <avr/io.h>
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use them. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"
#include "TimeStamp.h"

#define F_CPU 8000000UL  // 8 MHz
#include <util/delay.h>

uint32_t TimeStamp_Linear; // time in msec from last event
uint8_t  TimeStamp_LOG;  // time in 'LOG' from last event
uint32_t TimeStamp_Delay; // step count-down in msec
uint32_t TimeStamp_Step; // step in msec

// The 'LOG' TimeStamp (TimeStamp_LOG, 8-bit) use exponentially step size with 1.05 log factor. This 'LOG' value (8-bit) with event type (8-bit) are store in flash for events record. 
// On each event and after recording into the flash, TimeStamp counters are all restart. 
// Before TimeStamp wrap-around, EVENT_HEARTBEAT is generate. EVENT_HEARTBEAT use to store LOG value before the wrap-around.
// Flash size of 128 bytes are used to store up to 64 events in cyclic. 

// On reset: 
// * TimeStamp_LOG: 0
// * TimeStamp_Linear: 0 msec (0x0000_0000)
// * TimeStamp_Step: 1000 msec (0x0000_03E8; 1 sec)

// On the last tick before wrap-around (using exponentially step size with 1.05 log factor):
// * TimeStamp_LOG: 251 // **250
// * TimeStamp_Linear: 4164299688 msec (0xF836_2BA8; 48.2 days)
// * TimeStamp_Step:    208215984 msec (0x0C69_1FB0; 57.8 hours)

uint8_t EventType = 0;

extern void TimeStamp_PeriodicTask (uint32_t ElapsedTime /*msec*/)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		TimeStamp_Linear += ElapsedTime;
		
		if (TimeStamp_Delay > ElapsedTime)
			TimeStamp_Delay -= ElapsedTime;
		else
		{
			TimeStamp_LOG++;
			//printf_P (PSTR("> TimeStamp = %lu msec (%u LOG); Step=%lu;  \r\n"), TimeStamp_Linear, TimeStamp_LOG, TimeStamp_Step);
			if (TimeStamp_LOG == 250)
			{
				EventType |= EVENT_HEARTBEAT;
				TimeStamp_Reset ();
			}
			// calculate the next step delay in the 'LOG'
			TimeStamp_Step += (TimeStamp_Step * 5) / 100; // 1.05 log factor
			TimeStamp_Delay += TimeStamp_Step - ElapsedTime;
		}
	}
}

//---------------------------------------------------------------------------
static void Log_Event (void)
{
	if  (EventType == 0)
	return;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		uint8_t index;
		uint16_t data = ((uint16_t)TimeStamp_LOG) << 8 | (uint16_t)EventType;
		
		// log event are store in EEPROM address 0x80...0xFF.
		// each log event compose from two bytes:
		// * Time (in 'LOG') @ 8bit
		// * EventType @ 8bit
		
		// look for 0xFFFF ('end of file'); if not find, use index 0.
		for (index = 0; index < 64; index++)
		{
			if (eeprom_read_word((uint16_t *)(0x80+(index*2)))==0xFFFF)
			break;
		}
		index &= 0x3F;
		
		printf_P (PSTR("> Event: Type 0x%02X, TimeStamp = %lu msec (%u LOG); Store Index: 0x%02X;  \r\n"), EventType, TimeStamp_Linear, TimeStamp_LOG, index);
		
		// erase next location
		eeprom_write_word ((uint16_t *)(0x80+(((index+1)&(0x3F))*2)), 0xFFFF); // 13/05/2020: fixed index wrap-around.
		
		// update current location
		eeprom_write_word ((uint16_t *)(0x80+((index)*2)), data);
		
		EventType = 0;
	}
}
//---------------------------------------------------------------------------
extern void TimeStamp_Reset (void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		Log_Event ();
		TimeStamp_Linear = 0;
		TimeStamp_LOG = 0;
		TimeStamp_Step = 1000; // in msec // start with 1 sec step.
		TimeStamp_Delay = TimeStamp_Step;
	}
}
//---------------------------------------------------------------------------




