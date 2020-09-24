/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 ************************************
 Software UART (TX only) bit-banging 
 1 start, 1 stop, 8 bit, 115200. 
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
#include "SoftUART.h"

#define F_CPU 8000000UL  // 8 MHz
#include <util/delay.h>

uint8_t g_pin_num_on_PC; 

static FILE mystd = FDEV_SETUP_STREAM(SoftUart_PutChar_Stream, NULL, _FDEV_SETUP_WRITE);

//--------------------------------------------------------------------------------------
// Software UART: 1 start, 1 stop, 8 bit, 115200. 
// UART start-bit value: '0'; stop-bit value: '1'
// Byte data is send LSB first, MSB last. 
// STRAT -> LSB....MSB -> STOP 
//--------------------------------------------------------------------------------------
extern int SoftUart_PutChar_Stream (char var, FILE *stream) 
{
	uint16_t mask = 0x001; 
	uint16_t data = ((uint16_t)var << 1) | 0x200;  // add 2 bits, start bit '0' and stop bit '1' 


	uint8_t port_val = PORTC; 
	uint8_t port_val_high = port_val | ((uint8_t)1<<g_pin_num_on_PC) ;
	uint8_t port_val_low  = port_val & ~((uint8_t)1<<g_pin_num_on_PC) ;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) 
	{
		while (1)
		{
			if ((data&mask)==0)
			{
				PORTC = port_val_low ; 
			}
			else
			{
				PORTC = port_val_high;
			}
			_delay_us (7); // ~ 8.6 usec for 115200 bps 
		
			if (mask==0x200)
				break;
		
			mask = mask << 1;
		}
	}
	
	return (0);
}
//--------------------------------------------------------------------------------------
extern void SoftUart_Init (uint8_t PinNum)
{
	stdout = &mystd;
	stdin = &mystd;
	
	g_pin_num_on_PC = PinNum;
	
	/* Define directions outputs for port pins and set outputs high */
	SET_BIT_REG (DDRC, g_pin_num_on_PC);
	SET_BIT_REG (PORTC, g_pin_num_on_PC);
	_delay_us (1000); // 1 msec
	
	printf_P (PSTR(" \r\n"));
	printf_P (PSTR(" *********************  \r\n"));
	printf_P (PSTR(" Hello from ATtiny1634  \r\n"));
	printf_P (PSTR(" *********************  \r\n"));
	printf_P (PSTR(" \r\n"));
}
//--------------------------------------------------------------------------------------


