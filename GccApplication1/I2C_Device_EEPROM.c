/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
************************************************************************
 Virtual I2C EEPROM 64KB
************************************************************************
 Memory Map:
 * 0x0000..0x007F: RW: (128 bytes) part of ATtiny1634 EEPROM (via 'write enable' sequence).
 * 0x0080..0x00FF: RO: (128 bytes) part of ATtiny1634 EEPROM (logging area).
 * 0x0100..0x013F: RO: (64 bytes) software info 
 * 0x1000..0x14FF: RO: (1280 bytes) ATtiny1634 Data Memory (SRAM) and Register Files.
 * 0x3000..0x3004: RW: (5 bytes) WatchDog Module (via 'write enable' sequence)
 * 0x4000..0x7FFF: RO: (16KB) ATtiny1634 Flash.
 * 0x8000..0x8003: RW: (4 bytes) 'write enable' module. 
 
 unused sections are reserved and return 0xEE.

 * Support Byte and Page Read. 
 * Byte Write is supported for partial areas and for some only after 'write enable' sequence. 
 * Page Write is not supported. 
 * 'write enable' sequence: 
    > issue write to 0x8000 with required address[15:8] 
	> issue write to 0x8001 with required address[7:0] 
	> issue write to 0x8002 with required data[7:0]    
	> issue write to address with required data.
	Note: Repeat this for each byte write. any other writes reset 'write enable'. 
*/


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

static uint8_t g_WD_Cfg; 
static uint32_t g_WD_TimeOut;  // in msec  

static uint8_t Write_Buffer [3]; // 2 byte address (up to 64KB) + 1 byte data 
static uint8_t Read_Buffer [16]; // 16 byte of page read
static uint16_t g_Current_Addr; 

static uint16_t g_WriteEnable_Addr; // write enable sequence is required to allow byte write; 
static uint8_t  g_WriteEnable_Data;

static uint8_t I2C_Device_EEPROM_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte,  uint8_t NumOfByteUsed);

//--------------------------------------------------------------------------
extern void I2C_Device_EEPROM_Init (uint8_t DeviceIndex)
{
	g_Current_Addr = 0;
	g_WriteEnable_Addr = 0;
	g_WriteEnable_Data = 0;
	g_WD_Cfg = 0;
	g_WD_TimeOut = 0;
	pI2C_Device_Func[DeviceIndex] = (I2C_DEVICE_FUNC) I2C_Device_EEPROM_Func;
	printf_P (PSTR("> I2C_Device_EEPROM_Init; register to I2C device index %u; \r\n"), DeviceIndex);
}
//---------------------------------------------------------------------------------------------
extern void WD_Touch (void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		g_WD_TimeOut = ((uint32_t)1<<(g_WD_Cfg>>4)) * 1000;
	}
}
//---------------------------------------------------------------------------------------------
extern void WD_Stop (void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		g_WD_Cfg = 0;
		g_WD_TimeOut = 0;
	}
}
//---------------------------------------------------------------------------------------------
extern void WD_PeriodicTask (uint32_t ElapsedTime /*msec*/)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if (g_WD_TimeOut != 0)
		{
			if (g_WD_TimeOut > ElapsedTime)
			g_WD_TimeOut -= ElapsedTime;
			else
			{
				printf_P (PSTR("> WD Timeout.\r\n"));
				EventType |= EVENT_BMC_WD;
				TimeStamp_Reset();
				
				if (g_WD_Cfg & 0x01) 
				{
					// CORST_N (PA2)
					printf_P (PSTR("> Asserted BMC CORST#.\r\n"));
					CLEAR_BIT_REG (PORTA, PA2); // set low
					SET_BIT_REG (DDRA, PA2); // set output
					_delay_us (10);
					CLEAR_BIT_REG (DDRA, PA2); // set input (external PU)
				}
				else if (g_WD_Cfg & 0x02)
				{
					// PORST_N (PA1)
					printf_P (PSTR("> Asserted BMC PORST#.\r\n"));
					CLEAR_BIT_REG (PORTA, PA1); // set low
					SET_BIT_REG (DDRA, PA1); // set output
					_delay_us (10);
					CLEAR_BIT_REG (DDRA, PA1); // set input (external PU)
				}
			
				WD_Stop();
			}
		}
	}
}
//---------------------------------------------------------------------------------------------


static uint8_t I2C_Device_EEPROM_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed)
{
	uint8_t ResponseType = I2C_ACK;
	
	switch (Status)
	{
		case I2C_RD_START:
		case I2C_RD_BUFF_EMPTY: 
		
			memset ((void*)Read_Buffer, 0xEE, sizeof(Read_Buffer)); // for reserved sections 
			g_Current_Addr += NumOfByteUsed; // to support continues read cycles, increase address according to amount of bytes read.
			*pBuffer = (uint8_t *) Read_Buffer;
			*MaxNumOfByte = sizeof (Read_Buffer);
			
			if ( g_Current_Addr <= 0x00FF ) // ATtiny1634 EEPROM
				eeprom_read_block ((void*)Read_Buffer, (const void*)(g_Current_Addr&0x00FF), sizeof(Read_Buffer));
				
			else if ( (g_Current_Addr >= 0x0100) && (g_Current_Addr <= 0x013F) ) // software info 
				memcpy_P ((void*)Read_Buffer, (const void*)(0x70 + (g_Current_Addr&0x3F)), sizeof(Read_Buffer));
				
			else if ( (g_Current_Addr >= 0x3000) && (g_Current_Addr <= 0x3004) ) // WD module registers
			{
				uint8_t temp [5];
				temp [0] = g_WD_Cfg;
				memcpy ((void*)&temp[1], (const void*)(&g_WD_TimeOut) , sizeof(g_WD_TimeOut));
				memcpy ((void*)Read_Buffer, (const void*)(&temp[g_Current_Addr&0x1]) , sizeof(temp));
			}
		
			else if ( (g_Current_Addr >= 0x1000) && (g_Current_Addr <= 0x14FF) )  // ATtiny1634 Data Memory (SRAM) and Register Files 
				memcpy ((void*)Read_Buffer, (const void*)(g_Current_Addr&0x04FF), sizeof(Read_Buffer));
				
			else if ( (g_Current_Addr >= 0x4000) && (g_Current_Addr <= 0x7FFF) ) // ATtiny1634 Flash
				memcpy_P ((void*)Read_Buffer, (const void*)(g_Current_Addr&0x3FFF), sizeof(Read_Buffer));
			
			else if ( (g_Current_Addr >= 0x8000) && (g_Current_Addr <= 0x8003) ) // 'write enable' module registers
			{
				uint8_t temp [4];
				temp [0] = g_WriteEnable_Addr;
				temp [1] = g_WriteEnable_Addr>>8;
				temp [2] = g_WriteEnable_Data;
				temp [3] =0;
				memcpy ((void*)Read_Buffer, (const void*)(&temp[g_Current_Addr&0x3]) , sizeof(temp));
			}
			
			break;
		
		case I2C_RD_ERROR: // we assume master use the bytes up-to NumOfByteUsed; we don't care about the error. 
		case I2C_RD_STOP:
			g_Current_Addr += NumOfByteUsed; // to support continues read cycles, increase address according to amount of bytes read.
			break;
			
		case I2C_WR_START:
			*pBuffer = (uint8_t *) Write_Buffer;
			*MaxNumOfByte = sizeof (Write_Buffer);
			break;
	
		case I2C_WR_ERROR: 
			break;  // disregard this write cycle
			
		case I2C_WR_STOP: 
			if (NumOfByteUsed == 2)
			{ //  write cycle include x2 address bytes, update the address.
				g_Current_Addr = (uint16_t)Write_Buffer[0]<<8  | (uint16_t)Write_Buffer[1];
			}
			break;
		
		case I2C_WR_BUFF_FULL:
			//  write cycle is complete (x2 address and x1 data bytes was received), if allow, program the data; 
		
			*MaxNumOfByte = 0; // no more bytes are allow according EEPROM protocol (byte write only); disregard next write cycle by NACK, if any.
			
			//  write cycle include x2 address bytes, update the address.  
			g_Current_Addr = (uint16_t)Write_Buffer[0]<<8  | (uint16_t)Write_Buffer[1];
		
			if (g_Current_Addr == 0x8000)
			{ // 'write enable' sequence: address [15:8]
				g_WriteEnable_Addr = (uint16_t)Write_Buffer[2] << 8;
			}
			
			else if (g_Current_Addr == 0x8001)
			{ // 'write enable' sequence: address [7:0]
				g_WriteEnable_Addr |= Write_Buffer[2];
			}
			
			else if (g_Current_Addr == 0x8002)
			{ // 'write enable' sequence: data [7:0]
				g_WriteEnable_Data = Write_Buffer[2];
			}
			
			else if ( (g_Current_Addr == g_WriteEnable_Addr) && (Write_Buffer[2] == g_WriteEnable_Data) )
			{ // 'write enable' sequence must be update previous to this write cycle 
				//printf_P (PSTR("> write allow:  data:0x%02X; Addr:0x%x; \r\n"),  Write_Buffer[2], g_Current_Addr);
				
				if ( (g_Current_Addr >= 0x0038) && (g_Current_Addr <= 0x007F) )
				{ // we allow write only within 0x38-0x7F range where 0x40-0x7F are reserved for user defined.
					eeprom_write_byte ((uint8_t *)(g_Current_Addr&0x00FF), Write_Buffer[2]);
				}
				else if (g_Current_Addr == 0x3000)  // WD config register 
				{
					if (g_WD_Cfg==0)
						g_WD_Cfg = Write_Buffer[2];
						
					WD_Touch();
				}
						
				g_WriteEnable_Addr = 0;
				g_WriteEnable_Data = 0;
			}
				
			else
			{
				g_WriteEnable_Addr = 0;
				g_WriteEnable_Data = 0;
				ResponseType = I2C_NACK;
				printf_P (PSTR("> EERPOM Unauthorized write:  data:0x%02X; Addr:0x%x; \r\n"),  Write_Buffer[2], g_Current_Addr);
			}
			break;
			
		default:
			printf_P (PSTR("> I2C_Device_EEPROM_Func: *** ERROR *** Unknown status. \r\n"));
			break;
	}
	
	return (ResponseType);
}
//--------------------------------------------------------------------------
