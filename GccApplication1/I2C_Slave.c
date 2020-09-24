/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"
#include "I2C_Slave.h"

static uint8_t g_DeviceIndex;
static uint8_t g_ActualByteCount;
static uint8_t g_MaxByteCount;
static uint8_t g_Status;
static uint8_t *g_pBuffer;

static volatile uint32_t I2C_TimeOut = 0;
I2C_DEVICE_FUNC pI2C_Device_Func [4]  = {NULL, NULL, NULL, NULL};

//---------------------------------------------------------------------------------------------
extern void I2C_Slave_Init (uint8_t BaseAddr)
{
	//  TWI module Init
	CLEAR_BIT_REG (TWSCRA, TWEN);   // Disable TWI
	CLEAR_BIT_REG (PRR, PRTWI); // disable Power Reduction Two-Wire Interface, if any. 
	WRITE_REG (TWSA,  BaseAddr<<1 | 0); // Set Slave addresses; and disable general call address recognition
	WRITE_REG (TWSAM, 0x03<<1 | 0); // set address mask to emulate x4 I2c devices;
	SET_BIT_REG (TWSCRA, TWDIE);   // Enable Interrupt when TWSSRA.TWDIF flag is set (Data).
	SET_BIT_REG (TWSCRA, TWASIE);  // Enable Interrupt when TWSSRA.TWASIFflag is set (Address match; Stop condition if TWSIE is set).
	SET_BIT_REG (TWSCRA, TWSIE);   // Enable the stop condition detector to set TWSSRA.TWASIF flag.
	CLEAR_BIT_REG (TWSCRA, TWPME); // Disable Promiscuous Mode (software address match); use TWSA register to determine which address to recognize.
	CLEAR_BIT_REG (TWSCRA, TWSME); // Disable Auto Acknowledge on buffer read (Smart Mode).
	I2C_TimeOut = 0;
	g_ActualByteCount = 0;
	g_DeviceIndex = 0;
	SET_BIT_REG (TWSCRA, TWEN);   // Enable TWI
	printf_P (PSTR("> I2C slave module Init. Slave base address: 0x%x; Emulate x4 I2C devices. \r\n"), BaseAddr);
}
//---------------------------------------------------------------------------------------------
extern void I2C_Slave_PeriodicTask (uint32_t ElapsedTime /*msec*/)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if (I2C_TimeOut != 0)
		{
			if (I2C_TimeOut > ElapsedTime)
				I2C_TimeOut -= ElapsedTime;
			else
			{
				I2C_TimeOut = 0;
				printf_P (PSTR("> I2C Timeout. Restart I2C slave module.  \r\n"));
				CLEAR_BIT_REG (TWSCRA, TWEN); // Disable TWI
				SET_BIT_REG (TWSCRA, TWEN);   // Enable TWI
			}
		}
	}
}
//---------------------------------------------------------------------------------------------


ISR(TWI_SLAVE_vect, ISR_BLOCK)
{
	uint8_t reg_TWSSRA = TWSSRA;
	uint8_t reg_TWSD = TWSD;
	uint8_t l_TWAA;
	
	//printf_P (PSTR("> I2C Interrupt: (TWSSRA:0x%x); "), reg_TWSSRA);
	
	//----------------------------------------------------------------------------
	if (IS_BIT_SET(reg_TWSSRA, TWASIF))
	{
		//printf_P (PSTR("TWASIF; "));
		if ( (IS_BIT_SET(reg_TWSSRA, TWC)) || (IS_BIT_SET(reg_TWSSRA, TWBE)) )
		{// bus error.
			//printf_P (PSTR("Bus Collision or Bus Error (last ByteCount:%u); \r\n"), g_ActualByteCount);
			if (pI2C_Device_Func[g_DeviceIndex] != NULL)
				pI2C_Device_Func[g_DeviceIndex](g_Status|I2C_ERROR, NULL, NULL, g_ActualByteCount); 
			I2C_TimeOut = 0;
			g_ActualByteCount = 0;
		}
		else if (IS_BIT_SET(reg_TWSSRA, TWAS))
		{// start or re-start detected.
			
			if (g_ActualByteCount != 0)
			{// star detected (re-start transaction w/o exec stop)
				//printf_P (PSTR("Restart (last ByteCount:%u);"), g_ActualByteCount);
				if (pI2C_Device_Func[g_DeviceIndex] != NULL)
					pI2C_Device_Func[g_DeviceIndex] (g_Status|I2C_STOP, NULL, NULL, g_ActualByteCount);  // end any open transaction before start a new one.
			}
			
			g_DeviceIndex = (reg_TWSD>>1) & 0x03;
			I2C_TimeOut = I2C_TIME_OUT;
			g_ActualByteCount = 0;
			g_MaxByteCount = 0;
			g_Status = READ_BIT_REG (reg_TWSSRA, TWDIR); // 1:I2C_RD; 0:I2C_WR
			
			//-------------------------------------------------
			if (pI2C_Device_Func[g_DeviceIndex] != NULL)
				l_TWAA = pI2C_Device_Func[g_DeviceIndex] (g_Status|I2C_START, &g_pBuffer, &g_MaxByteCount, 0); 
			else
				l_TWAA = I2C_NACK;  // Send 'NACK' response for address match  
			
			WRITE_BIT_REG (TWSCRB, TWAA, l_TWAA);
			//-----------------------------------------------
		}
		else
		{// stop detected 
			if (pI2C_Device_Func[g_DeviceIndex] != NULL)
				pI2C_Device_Func[g_DeviceIndex] (g_Status|I2C_STOP, NULL, NULL, g_ActualByteCount); 
			I2C_TimeOut = 0;
			g_ActualByteCount = 0;
		}
		
		TWSSRA = 1<<TWASIF; // clear flag // also send response (after address match) according to TWAA bit value.
	}
	//----------------------------------------------------------------------------
	if (IS_BIT_SET(reg_TWSSRA, TWDIF))
	{
		if (IS_BIT_SET(reg_TWSSRA, TWDIR))
		{// master read mode
			
			if ( (IS_BIT_SET(reg_TWSSRA, TWRA)) && (g_ActualByteCount>0) )  // master action is valid from the second byte interrupt
			{
				// when master NACK the previous data, do not reload I2C buffer with a new data since master stop reading.
				TWSD = 0xFF; // send 'FF' to master it master continue to read after it's own NACK.
			}
			else
			{
				// The first read buffer allocation can be done on I2C_START state or on I2C_BUFF.
				// When I2C_BUFF event is send to device emulation, this means the I2C module sent g_MaxByteCount to the master.  
				
				if ( (g_ActualByteCount >= g_MaxByteCount) && (pI2C_Device_Func[g_DeviceIndex] != NULL) )
				{
					// request the device to allocate a read buffer 
					pI2C_Device_Func[g_DeviceIndex] (I2C_RD_BUFF_EMPTY, &g_pBuffer, &g_MaxByteCount, g_ActualByteCount);
					g_ActualByteCount = 0;
				}
								
				if (g_ActualByteCount >= g_MaxByteCount)
				{ 
					// no more data to send to master.
					TWSD = 0xFF;  // send 'FF' to master.
				}
				else
				{
					// reload byte to send to master
					TWSD = *g_pBuffer;
					g_ActualByteCount++;
					g_pBuffer++;
				}
			}
		}
		else
		{// master write mode
			
			// The first write buffer allocation *MUST* be done in I2C_WR_START state.
			// When I2C_WR_BUFF_FULL event is send to device emulation, this means the I2C module received g_MaxByteCount from the master and hold the bus before sending the action for this last byte.
			// e.g., when g_MaxByteCount is 3, the I2C_WR_BUFF_FULL event occur after receiving 3 bytes from the master and just before sending the response. The emulate device can ACK or NACK the third byte. 
			
			if (g_ActualByteCount >= g_MaxByteCount)
			{
				l_TWAA = I2C_NACK;  // Send 'NACK' on the next action 
			}
			else
			{
				l_TWAA = I2C_ACK; // Send 'ACK' on the next action 
				*g_pBuffer = reg_TWSD;
				g_ActualByteCount++;
				g_pBuffer++;
			}
			
			if ( (g_ActualByteCount >= g_MaxByteCount) && (pI2C_Device_Func[g_DeviceIndex] != NULL) )
			{
				// request the device to allocate a new write buffer; 
				// device return response type (NACK or ACK) for this cycle. 
				l_TWAA = pI2C_Device_Func[g_DeviceIndex] (I2C_WR_BUFF_FULL, &g_pBuffer, &g_MaxByteCount, g_ActualByteCount);
				g_ActualByteCount = 0;
			}
			
			WRITE_BIT_REG (TWSCRB, TWAA, l_TWAA);
		}
		
		//----------------------------------------------------------
		// ????? Accessing TWSD will clear the slave interrupt flags
		TWSSRA = 1<<TWDIF; // clear flag // also executed Acknowledge action (while master transmit) according to TWAA bit value.
		I2C_TimeOut = I2C_TIME_OUT; 
	}
	//----------------------------------------------------------------------------
}

