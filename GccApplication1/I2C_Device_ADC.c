/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 *************************************
 Virtual I2C 8-bit 8-Channels ADC 
 *************************************
 
 Compatible to ADS7830 and NCD9830.
 
 Support VREF:
 * 3.3V (bit 3 in the command byte is clear). 
 * 2.5V (bit 3 in the command byte is set). 
*/

/*
TBD:
1. Upgrade to 12-bit device emulation (Note that ATtiny1634 support 10-bit)
2. Add negative values (16-bit two's complement) for diff conversion.
3. Add thresholds and interrupt support.  (Issue: interrupt is shared with virtual GPI module)
*/

#include <avr/io.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use them. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"

#include "I2C_Slave.h"

static void ADC_Init (void);
static void ADC_Settings (uint8_t MuxSelect, uint8_t RefSelect);
static void ADC_Start_Convert (void);
static uint8_t ADC_Read_Data (void);

static uint8_t ADC_SE_Convert (void);
static uint8_t ADC_DIFF_Convert (void);
static uint8_t ADC_Convert (void);


static uint8_t I2C_Device_ADC_Cmd;
static uint8_t I2C_Device_ADC_Value;

#define VREF_VCC      0 // VCC pin used as analog reference, disconnected from PA0 (AREF) --- that is 3.3V at Nuvoton RunBMC module.
#define VREF_EXTERNAL 1 // External voltage reference at PA0 (AREF) pin --- that is 2.5V at Nuvoton RunBMC module.
#define VREF_INTERNAL 2 // Internal 1.1V voltage reference

static uint8_t g_Mux;
static uint8_t g_Vref;
static uint8_t g_IsSingleEnded;

static uint8_t I2C_Device_ADC_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed);

/*
ATtiny1634 to RunBMC connectivity: 
RunBMC Header:	ADC8	ADC9	ADC10	ADC11	ADC12	ADC13	ADC14	ADC15		
ATtiny1634:		ADC0	ADC1	ADC2	ADC3	ADC4	ADC5	ADC8	ADC9
g_Mux:          0		4		1		5		2		6		3		7 
*/
//static uint8_t ADC_Channel_Assignment [8] = {0, 1, 2, 3, 4, 5, 8, 9}; // Input: RunBMC ADC Ch offset to 8; Output: ATtiny1634 ADC Ch
static uint8_t ADC_Channel_Assignment [8] = {0, 2, 4, 8, 1, 3, 5, 9}; // Input: RunBMC ADC Ch offset to 8; Output: ATtiny1634 ADC Ch

//--------------------------------------------------------------------------
extern void I2C_Device_ADC_Init (uint8_t DeviceIndex)
{
	g_Mux = ADC_Channel_Assignment[0]; 
	g_IsSingleEnded = 0;
	g_Vref = VREF_EXTERNAL;
	
	ADC_Init ();
	
	pI2C_Device_Func[DeviceIndex] = (I2C_DEVICE_FUNC) I2C_Device_ADC_Func;  
	printf_P (PSTR("> I2C_Device_ADC_Init; register to I2C device index %u; \r\n"), DeviceIndex);
}

//--------------------------------------------------------------------------
static uint8_t I2C_Device_ADC_Func (uint8_t Status, /*out*/ uint8_t **pBuffer,  /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUsed)
{
	uint8_t ResponseType = I2C_ACK;
	
	switch (Status)
	{
		case I2C_RD_START: // Note that read is trigger the convert.
		case I2C_RD_BUFF_EMPTY: // continues read will convert gain the same channel.  
			I2C_Device_ADC_Value = ADC_Convert ();
			*pBuffer = &I2C_Device_ADC_Value;
			*MaxNumOfByte = sizeof (I2C_Device_ADC_Value);
			//printf_P (PSTR("> I2C_Device_ADC_Func; send value:%u \r\n"), I2C_Device_ADC_Value);
			break;
		
		case I2C_RD_STOP:  //  nothing to do.
		case I2C_RD_ERROR: //  we don't care about the error.
			break;
		
		case I2C_WR_START:
			*pBuffer = &I2C_Device_ADC_Cmd;
			*MaxNumOfByte = sizeof (I2C_Device_ADC_Cmd);
			break;
			
		case I2C_WR_BUFF_FULL: 
			*MaxNumOfByte = 0; // no more bytes are allow 
			// continue parse the fist byte. 
			// Option: move 'I2C_WR_BUFF_FULL' to 'I2C_WR_START'. In this case only the last byte will be parsed. 
		
		case I2C_WR_STOP:
		case I2C_WR_ERROR: // we don't care about the error.
			if (NumOfByteUsed == 1 /*command byte was written*/)
			{
				//printf_P (PSTR("> I2C_Device_ADC_Func; "));
				
				if ( IS_BIT_SET (I2C_Device_ADC_Cmd, 3) ) 
				{
					//printf_P (PSTR("VREF_AREF_PIN; "));
					g_Vref = VREF_EXTERNAL; // 2.5V on RunBMC. 
				}
				else 
				{
					//printf_P (PSTR("VREF_VCC_PIN; "));
					g_Vref = VREF_VCC; // 3.3V on RunBMC. 
				}
	
				g_Mux = (I2C_Device_ADC_Cmd >> 4) & 0x7;
				
				g_IsSingleEnded = (I2C_Device_ADC_Cmd >> 7) & 0x1;
				//printf_P (PSTR("Mux:%u; IsSingleEnded:%u; \r\n"), g_Mux, g_IsSingleEnded);
				
				// Note: write does not start conversion, only read does. 
			}
			break;
		
		default:
			printf_P (PSTR("> I2C_Device_ADC_Func: *** ERROR *** Unknown status. \r\n"));
			break;
	}
	
	return (ResponseType);
}
//--------------------------------------------------------------------------

//---------------------------------------------
static uint8_t ADC_SE_Convert (void)
{
	uint8_t results;

	ADC_Settings (ADC_Channel_Assignment[g_Mux], g_Vref);
	ADC_Start_Convert ();
	results = ADC_Read_Data();
	//printf_P (PSTR("> ADC_SE_Convert: RunBMC_Ch:%u; uC_Ch:%u; Value:%u; \r\n"), g_Mux, ADC_Channel_Assignment[g_Mux], results);
	return results;
}
//---------------------------------------------
static uint8_t ADC_DIFF_Convert (void)
{
	uint8_t Mux_p, Mux_n;
	uint8_t results_p, results_n, results_diff;
	
	//printf_P (PSTR("> ADC_DIFF_Convert: "));
	
	Mux_p = g_Mux;
	Mux_n = g_Mux ^ 0x8;
	

	ADC_Settings (ADC_Channel_Assignment[Mux_p], g_Vref);
	ADC_Start_Convert ();
	results_p = ADC_Read_Data();
	//printf_P (PSTR("p: RunBMC_Ch:%u; uC_Ch:%u; Value:%u; "), Mux_p, ADC_Channel_Assignment[Mux_p], results_p);
	
	ADC_Settings (ADC_Channel_Assignment[Mux_n], g_Vref);
	ADC_Start_Convert ();
	results_n = ADC_Read_Data();
	//printf_P (PSTR("n: RunBMC_Ch:%u; uC_Ch:%u; Value:%u; "), Mux_n, ADC_Channel_Assignment[Mux_n], results_n);
	
	
	if (IS_BIT_CLEARED (g_Mux, 2))
	{
		results_diff = results_p - results_n;
		//printf_P (PSTR("p-n Value:%u; \r\n"), results_diff);	
	}
	else
	{
		results_diff = results_n - results_p;
		//printf_P (PSTR("n-p Value:%u; \r\n"), results_diff);
	}
	
	return results_diff;
}
//---------------------------------------------
static uint8_t ADC_Convert (void)
{
	if (g_IsSingleEnded)
		return ADC_SE_Convert();
	else
		return ADC_DIFF_Convert();
}
//---------------------------------------------


/*
Voltage reference and input channel selections will not go into effect until ADEN is set.
	
When reference voltage is changed, the next conversion will take 25 ADC clock cycles.
	
The first ADC conversion result after switching reference voltage source may be inaccurate, and the user is advised to discard this result.
		
It is recommended to force the ADC to perform a long conversion when changing multiplexer or voltage reference settings.
This can be done by first turning off the ADC, then changing reference settings and then turn on the ADC.
Alternatively, the first conversion results after changing reference settings should be discarded.
	
After multiplexer switching to internal voltage reference the ADC requires a settling time of 1ms before measurements are stable.
Conversions starting before this may not be reliable. The ADC must be enabled during the settling time.
Lior: maybe duo to high impedance of the internal voltage reference.
	
When measuring temperature,	the internal voltage reference must be selected as ADC reference source.
When enabled, the ADC converter can	be used in single conversion mode to measure the voltage over the temperature sensor.
	
*/
//---------------------------------------------
static void ADC_Init (void)
{
	CLEAR_BIT_REG (PRR, PRADC); // Disable Power Reduction ADC, if any.
	ADCSRA = (1<<ADEN) | 5; // ADC Enable; ADC Prescaler to 32 (8MHz / 32 = 250KHz);
	ADCSRB = (1<<ADLAR); // ADC Left Adjust (use only 8-bit on ADCH)
	DIDR0 = 0; // do not Disable Digital Input; no need to reduce power consumption.
	DIDR1 = 0;
	DIDR2 = 0;
}
//---------------------------------------------	
static void ADC_Settings (uint8_t MuxSelect, uint8_t RefSelect)
{
	CLEAR_BIT_REG (ADCSRA, ADEN); // use to force the ADC to perform a long conversion
	RefSelect &= 0x03;  // 0 to 3 
	MuxSelect &= 0x0F;  // 0 to F 
	ADMUX = (RefSelect << REFS0) | (MuxSelect << MUX0); 
	SET_BIT_REG (ADCSRA, ADEN); 
}
//---------------------------------------------
static void ADC_Start_Convert (void)
{
	while ( IS_BIT_SET(ADCSRA, ADSC) );
	SET_BIT_REG (ADCSRA, ADSC); 
}
//---------------------------------------------
static uint8_t ADC_Read_Data (void)
{
	while ( IS_BIT_SET(ADCSRA, ADSC) );
	return (ADCH);
}
//---------------------------------------------





