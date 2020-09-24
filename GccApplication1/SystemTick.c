/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 ************************************
 System Tick 
 ************************************
*/

/*
TBD:

*/

#include <avr/io.h>
#include <util/atomic.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use them. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"
#include "I2C_Slave.h"
#include "I2C_Device_EEPROM.h"
#include "I2C_Device_ADC.h"
#include "I2C_Device_GPI.h"
#include "TimeStamp.h"

uint32_t g_TimeElapased_msec;

// init for 1 msec tick
extern void SystemTick_Init (void)
{
	
	// Timer/Counter0  configure
	GTCCR = 0x81; // hold Prescaler at reset
	TCNT0 = 0;
	OCR0A = 125-1; // for 1msec tick interrupt
	TIMSK = 1<<OCIE0A;  // Timer/Counter0 Output Compare Match A Interrupt Enable
	TIFR = 1<<OCF0A; // clear Output Compare Flag 0 A
	
	// Normal port operation, OC0A and OC0B are disconnected
	// Set Clear Timer on Compare Match (CTC) mode mode (TOP==OCRA);
	TCCR0A = 0x02;
	
	// Clock Select: clkI/O/64. When clkI/O is 8MHz, Counter/Timer clock is 125KHz
	TCCR0B = 0x03;
	
	GTCCR = 0; // release Prescaler.
	
	g_TimeElapased_msec = 0;
}

//uint32_t timeout_1min = 0;

// system tick 1msec
ISR(TIMER0_COMPA_vect, ISR_BLOCK)
{
	GPI_PeriodicTask (1);			// Elapsed Time: 1 msec
	
	g_TimeElapased_msec++;
	if (g_TimeElapased_msec == 10)
	{
		I2C_Slave_PeriodicTask (10);	// Elapsed Time: 10 msec
		WD_PeriodicTask (10);			// Elapsed Time: 10 msec
		TimeStamp_PeriodicTask (10);	// Elapsed Time: 10 msec
		g_TimeElapased_msec = 0;
	}
	
	/*
	timeout_1min++;
	if (timeout_1min == 60000)
	{
		printf_P (PSTR("> Tick - testing the clock \r\n"));
		timeout_1min=0;
	}
	*/
	
}



