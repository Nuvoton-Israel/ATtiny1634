/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 ************************************************************************
 Virtual I2C Port Expander with 8 Inputs and Maskable Transition Detection
 ************************************************************************
 
 Compatible to MAX7319.
 
 * By default all interrupts are mask (the interrupt mask register is set to 0x00).
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

static uint8_t Read_Buffer [2]; // status of input ports + transition flags

static uint8_t g_GPI_CurrentValue;
static uint8_t g_GPI_PrevValue;
static uint8_t g_GPI_InterruptMask;
static uint8_t g_GPI_Transition;
static uint8_t g_GPI_EnableInterrupt;

static uint8_t I2C_Device_GPI_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed);

//----------------------------------------------------------------------------------
/*
RunBMC Header:	GPI0	GPI1	GPI2	GPI3	GPI4	GPI5	GPI6	GPI7			
ATtiny1634:		PA3		PA4		PA5		PA6		PA7		PB0		PB3		PC0

NPCM7mnx:		GPIO38 (as INT#)
ATtiny1634:		PB2
*/
//----------------------------------------------------------------------------------
static void GPI_Init (void)
{
	// we assume all GPIOs are default input after reset. 
	// we assume external PU exist on INT# so no glitch will appear now.
	SET_BIT_REG (PORTB,PB2); // Set PB2 high (disable interrupt)
	SET_BIT_REG (DDRB,PB2); // Set PB2 output (Push-Pull)
}
//----------------------------------------------------------------------------------
static uint8_t GPI_ReadState (void)
{
	uint8_t value = 0;
	
	uint8_t l_PORTA = PINA;
	uint8_t l_PORTB = PINB;
	uint8_t l_PORTC = PINC;
	
	value |=  ((l_PORTC>>0)&0x1)<<7 ; // PC0 ==> GPI7
	value |=  ((l_PORTB>>3)&0x1)<<6 ; // PB3 ==> GPI6
	value |=  ((l_PORTB>>0)&0x1)<<5 ; // PB0 ==> GPI5
	value |=  ((l_PORTA>>3)&0x1F)<<0 ; // PA3..PA7 ==> GPI0..GPI4
	
	return (value);
}
//----------------------------------------------------------------------------------
extern void GPI_PeriodicTask (uint32_t ElapsedTime /*msec*/)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		g_GPI_CurrentValue = GPI_ReadState ();
		g_GPI_Transition |= g_GPI_CurrentValue ^ g_GPI_PrevValue;
	
		if (g_GPI_CurrentValue ^ g_GPI_PrevValue)
		//printf_P (PSTR("> GPI_PeriodicTask; Transition detected; PrevValue:0x%x; CurrentValue:0x%x; Transition:0x%x. \r\n"), g_GPI_PrevValue, g_GPI_CurrentValue, g_GPI_Transition);
	
		if ( (g_GPI_EnableInterrupt == 1) /*&& (IS_BIT_SET(PINB, PB2))*/ && ((g_GPI_Transition & g_GPI_InterruptMask) != 0) )
		{
			CLEAR_BIT_REG (PORTB, PB2); // Set PB2 low to issue interrupt to host
			//printf_P (PSTR("> GPI_PeriodicTask; issue interrupt to host. \r\n"));	
		}
	
		g_GPI_PrevValue = g_GPI_CurrentValue;
	}
}
//----------------------------------------------------------------------------------
extern void I2C_Device_GPI_Init (uint8_t DeviceIndex)
{
	g_GPI_Transition = 0;
	g_GPI_InterruptMask = 0;
	g_GPI_EnableInterrupt = 1;
	
	GPI_Init ();
	
	pI2C_Device_Func[DeviceIndex] = (I2C_DEVICE_FUNC) I2C_Device_GPI_Func;  
	printf_P (PSTR("> I2C_Device_GPI_Init; register to I2C device index %u; \r\n"), DeviceIndex);
}

//--------------------------------------------------------------------------
static uint8_t I2C_Device_GPI_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed)
{
	uint8_t ResponseType = I2C_ACK;
	
	switch (Status)
	{
		case I2C_RD_START: 
		case I2C_RD_BUFF_EMPTY: // continues master read wills sample inputs over again.  
			SET_BIT_REG (PORTB, PB2); // Set PB2 high (disable interrupt)
			g_GPI_EnableInterrupt = 0; // disable interrupt assertion while reading 
			GPI_PeriodicTask (0); // sample inputs and update variables
			Read_Buffer [0] = g_GPI_CurrentValue;
			Read_Buffer [1] = g_GPI_Transition;
			//printf_P (PSTR("> I2C_Device_GPI_Func; Read; PrevValue:0x%x; CurrentValue:0x%x; Transition:0x%x; Mask:0x%x. \r\n"), g_GPI_PrevValue, g_GPI_CurrentValue, g_GPI_Transition, g_GPI_InterruptMask);
			g_GPI_Transition = 0; // reset transitions flags
			*pBuffer = &(Read_Buffer[0]);
			*MaxNumOfByte = sizeof (Read_Buffer);
			break;
		
		case I2C_RD_STOP:  //  nothing to do.
		case I2C_RD_ERROR: //  we don't care about the error.
			g_GPI_EnableInterrupt = 1; // enable interrupt assertion.
			GPI_PeriodicTask (0); // sample inputs and update variables
			break;
		
		case I2C_WR_START:
		case I2C_WR_BUFF_FULL: // support continues writes, will overwrite. 
			SET_BIT_REG (PORTB, PB2); // Set PB2 high (disable interrupt)
			g_GPI_EnableInterrupt = 0;  // disable interrupt assertion while writing
			*pBuffer = &g_GPI_InterruptMask;
			*MaxNumOfByte = sizeof (g_GPI_InterruptMask);
			break;
			
		case I2C_WR_STOP:
		case I2C_WR_ERROR: // we don't care about the error.
			g_GPI_EnableInterrupt = 1;  // enable interrupt assertion.
			GPI_PeriodicTask (0); // sample inputs and update variables
			break;
		
		default:
			printf_P (PSTR("> I2C_Device_GPI_Func: *** ERROR *** Unknown status. \r\n"));
			break;
	}
	
	return (ResponseType);
}
//--------------------------------------------------------------------------
