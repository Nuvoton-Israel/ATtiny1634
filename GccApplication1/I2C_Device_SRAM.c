/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
************************************************************************
 Virtual I2C SRAM 
************************************************************************

 Protocol: 
 Write: <I2C Address + W> <Address MSB> <Address LSB> <Data 0> .... <Data n>
 Read:  <I2C Address + W> <Address MSB> <Address LSB> <I2C Address + R> <Data 0> .... <Data n>
 
 Memory area is wraparound.

 * Support Byte and Page Read. 
 * Support Byte and Page Write. 
*/

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#include <avr/io.h>
#include <util/atomic.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use them. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"
#include "I2C_Slave.h"
#include "TimeStamp.h"

#define F_CPU 8000000UL  // 8 MHz
#include <util/delay.h>

static uint8_t Generic_SRAM [512]; // set to 512 or 256

#define WRITE_PHASE_ADDR 0
#define WRITE_PHASE_DATA 1
static uint8_t  g_Write_Phase;
static uint16_t g_Current_Addr; 
static uint8_t Write_Buffer [2]; // 2 byte address (up to 64KB)

static uint8_t I2C_Device_SRAM_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte,  uint8_t NumOfByteUsed);

//--------------------------------------------------------------------------
extern void I2C_Device_SRAM_Init (uint8_t DeviceIndex)
{
	g_Current_Addr = 0;
	pI2C_Device_Func[DeviceIndex] = (I2C_DEVICE_FUNC) I2C_Device_SRAM_Func;
	printf_P (PSTR("> I2C_Device_SRAM_Init; register to I2C device index %u; \r\n"), DeviceIndex);
}

//---------------------------------------------------------------------------------------------
static uint8_t I2C_Device_SRAM_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed)
{
	uint8_t ResponseType = I2C_ACK;
	
	switch (Status)
	{
		case I2C_RD_START:
		case I2C_RD_BUFF_EMPTY: 
		
			g_Current_Addr += NumOfByteUsed; // to support continues read cycles, increase address according to amount of bytes read.
			g_Current_Addr &= (sizeof (Generic_SRAM)-1); // limit address to 512 bytes, wraparound the address.
			*pBuffer = &Generic_SRAM[g_Current_Addr];
			*MaxNumOfByte = MIN (sizeof (Generic_SRAM) - g_Current_Addr, 0x80);
			
			//printf_P (PSTR("> SRAM read: g_Current_Addr=0x%X, MaxNumOfByte=%u  \r\n"),g_Current_Addr, *MaxNumOfByte);
			
			break;
		
		case I2C_RD_ERROR: // we assume master use the bytes up-to NumOfByteUsed; we don't care about the error. 
		case I2C_RD_STOP:
			g_Current_Addr += NumOfByteUsed; // to support continues read cycles, increase address according to amount of bytes read.
			break;
			
		case I2C_WR_START:
			g_Write_Phase = WRITE_PHASE_ADDR;
			*pBuffer = (uint8_t *) Write_Buffer;
			*MaxNumOfByte = sizeof (Write_Buffer);
			break;
	
		case I2C_WR_ERROR: 
			break;  // we don't care about errors 
			
		case I2C_WR_STOP: 
			break;
		
		case I2C_WR_BUFF_FULL:
			if (g_Write_Phase == WRITE_PHASE_ADDR)
			{
				//  write address phase is completed (x2 address bytes was received), prepare to receive the data; 
				g_Current_Addr = (uint16_t)Write_Buffer[0]<<8  | (uint16_t)Write_Buffer[1];
				g_Write_Phase = WRITE_PHASE_DATA;
			}
			else
			{
				//  write data phase
				g_Current_Addr += NumOfByteUsed; 
			}
			
			g_Current_Addr &= (sizeof (Generic_SRAM)-1); // limit address to 512 bytes, wraparound the address.
			*pBuffer = &Generic_SRAM[g_Current_Addr];
			*MaxNumOfByte = MIN (sizeof (Generic_SRAM) - g_Current_Addr, 0x80);
			
			//printf_P (PSTR("> SRAM write: g_Current_Addr=0x%X, MaxNumOfByte=%u, NumOfByteUsed=%u \r\n"),g_Current_Addr, *MaxNumOfByte, NumOfByteUsed);
			
			break;
			
		default:
			printf_P (PSTR("> I2C_Device_SRAM_Func: *** ERROR *** Unknown status. \r\n"));
			break;
	}
	
	return (ResponseType);
}
//--------------------------------------------------------------------------
