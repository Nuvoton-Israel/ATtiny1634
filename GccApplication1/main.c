/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

/*
 TBD:
 
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h> // use const and string values stored in flash and not copy them to ram before use them. (see https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)
#include <util/atomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "CoreRegisters.h"
#include "I2C_Slave.h"
#include "I2C_Device_EEPROM.h"
#include "I2C_Device_ADC.h"
#include "I2C_Device_GPI.h"
#include "I2C_Device_SRAM.h"
#include "SoftUART.h"
#include "SystemTick.h"
#include "TimeStamp.h"

#define F_CPU 8000000UL  // 8 MHz
#include <util/delay.h>

/*
 * Note:
 * These below variables are placed in a fix location in the BIN file; at offset 0x70, following the vectors.
 * External flash programmer tool may read these variables to determined file info and version.
*/

/* offset 0xA4 */ static const char string_padding[12]     __attribute__((used)) __attribute__ ((section (".vectors"))) = "";   
/* offset 0xA3 */ static const uint8_t  UART_PIN           __attribute__((used)) __attribute__ ((section (".vectors"))) = SoftUart_PinNum;   
/* offset 0xA2 */ static const uint8_t  I2C_Base_Addr      __attribute__((used)) __attribute__ ((section (".vectors"))) = I2C_Slave_Addr;   // 7-bit I2C base address 
/* offset 0xA0 */ static const uint16_t FW_Version         __attribute__((used)) __attribute__ ((section (".vectors"))) = 0x0004; // 00.04
/* offset 0x90 */ static const char string_time[16]        __attribute__((used)) __attribute__ ((section (".vectors"))) = __TIME__;
/* offset 0x80 */ static const char string_date[16]        __attribute__((used)) __attribute__ ((section (".vectors"))) = __DATE__;
/* offset 0x70 */ static const char string_header[16]      __attribute__((used)) __attribute__ ((section (".vectors"))) = "Nuvoton_RunBMC";

#define I2C_Device_EEPROM_Index 0	// Set EEPROM emulated device to I2C address = I2C_Base_Addr + 0
#define I2C_Device_ADC_Index    1	// Set ADC emulated device to I2C address = I2C_Base_Addr + 1
#define I2C_Device_GPI_Index    2	// Set GPI emulated device to I2C address = I2C_Base_Addr + 2
#define I2C_Device_SRAM_Index   3	// Set SRAM emulated device to I2C address = I2C_Base_Addr + 3


int main(void)
{
	CCP = 0xD8; // Configuration Change Protection Register (Timed Sequences)
	CLKPR = 0; // Set Clock Pre-scale Register to 1.  Use internal 8MHz (CLKPR fuse settings) so CPU clock is 8MHz. 
	
	CCP = 0xD8; // Configuration Change Protection Register (Timed Sequences)
	WDTCSR = (1<<WDE) | (1<<WDP3) |  (1<<WDP0) | (1<<WDIE); // Enable Watchdog Timer for 8 sec; first time-out issue interrupt and next time-out issue chip reset. 
	
	SoftUart_Init (pgm_read_byte(&UART_PIN));
	SystemTick_Init ();
	TimeStamp_Reset ();

	// **********************************
	//          Print System Info 
	// **********************************
	
	printf_P (PSTR("> Build Date: %S; %S. \r\n"), &string_date[0], &string_time[0]);
	
	printf_P (PSTR("> MCUSR:0x%02X; WDTCSR:0x%02X;  \r\n"), MCUSR, WDTCSR);
	EventType = MCUSR & 0x0F;
	if (EventType & 0x01)
		printf_P (PSTR("\t* Power-on Reset occurs \r\n"));
	else 
	{
		if (EventType & 0x02)
			printf_P (PSTR("\t* External Reset occurs \r\n"));
			
		if (EventType & 0x04)
			printf_P (PSTR("\t* Brown-out Reset occurs \r\n"));
			
		if (EventType & 0x08)
			printf_P (PSTR("\t* Watchdog Reset occurs \r\n"));
	}
	TimeStamp_Reset ();
	MCUSR = 0; // clear the value for the next reset cycle. 
	
	//printf_P (PSTR("> SPH:0x%02X; SPL:0x%02X; SREG:0x%02X; \r\n"), SPH, SPL, SREG);
	printf_P (PSTR("> CLKSR:0x%02X; CLKPR:0x%02X;  \r\n"), CLKSR, CLKPR);
	printf_P (PSTR("> 8MHz:  OSCCAL0:0x%02X; OSCTCAL0A:0x%02X; OSCTCAL0B:0x%02X; \r\n"), OSCCAL0, OSCTCAL0A, OSCTCAL0B);
	printf_P (PSTR("> 32KHz: OSCCAL1:0x%02X; \r\n"), OSCCAL1);
	//printf_P (PSTR("> MCUCR:0x%02X; PRR:0x%02X;  \r\n"), MCUCR, PRR);
	//printf_P (PSTR("> GIMSK:0x%02X;  \r\n"), GIMSK);
	//---------------------------------------------------------------------------------------------------------------
	
	I2C_Device_EEPROM_Init (I2C_Device_EEPROM_Index);
	I2C_Device_ADC_Init (I2C_Device_ADC_Index);
	I2C_Device_GPI_Init (I2C_Device_GPI_Index);
	I2C_Device_SRAM_Init (I2C_Device_SRAM_Index);
	I2C_Slave_Init (pgm_read_byte(&I2C_Base_Addr));
	
	// The falling edge of INT0 generates an interrupt request
	SET_BIT_REG (MCUCR, ISC01);
	CLEAR_BIT_REG (MCUCR, ISC00);
	SET_BIT_REG (GIMSK, INT0); // External Interrupt Request INT0 Enable
	SET_BIT_REG (GIFR, INTF0); //  Clear INTF0 bit 
	
	
	sei(); // enable global interrupts 
	
	// main loop 
	while (1)
	{
		__asm__ __volatile__ ("wdr"); // reset (touch) ATtiny1634 Watchdog
	}
		
		
}


ISR(BADISR_vect, ISR_BLOCK)
{
	printf_P (PSTR("> BADISR_vect: *** ERROR *** unexpected interrupt occurs; halting the CPU.\r\n"));
	while (1); // waiting for Watchdog.
}

ISR(WDT_vect, ISR_BLOCK) 
{
	//If WDE is set, WDIE is automatically cleared by hardware when a time-out occurs. Next time-out will reset. 
	EventType |= EVENT_HEARTBEAT;
	printf_P (PSTR("> Watchdog Time-out interrupt \r\n"));	
	TimeStamp_Reset (); // log the even and reset timestamp
}

ISR(INT0_vect, ISR_BLOCK) 
{
	//INTF0 is automatically cleared by hardware on Interrupt. 
	
	uint8_t l_PINC;
	
	// 13/05/2020: Moved core-reset to shorten the time between SPILOAD pulse detect and core reset issued. This to minimized BMC exec time before starting SPI power-cycle. 
	// CORST_N (PA2)
	CLEAR_BIT_REG (PORTA, PA2); // set low
	SET_BIT_REG (DDRA, PA2); // set output
	printf_P (PSTR("> Asserted BMC CORST# . \r\n"));
	
	// Measured: EXTEND_SPILOAD_N pulse: 10 usec generated by nSPILOAD (up to 2V), delay 100 usec 
	_delay_us (100);   
	l_PINC = PINC; // read EXTEND_SPILOAD_N (PC2) value. If value is low, this means the host pull HGPIO7 low.  
	
	if (IS_BIT_SET (l_PINC, PC2))
	{
		printf_P (PSTR("> BMC reset detected. \r\n"));	
		EventType |= EVENT_BMC_RESET_DETECT;
		// if FUP feature need to be disabled (not to enter FUP), wait for PINC.2 to goes high before continue. 
	}
	else
	{
		printf_P (PSTR("> Host force FUP detected. \r\n"));	
		EventType |= EVENT_BMC_ENTER_FUP;
	}
	
	TimeStamp_Reset (); // log the even and reset timestamp
	
	// FWSPI_PWR_EN (PC4)
	printf_P (PSTR("> Turn-off flash power. \r\n"));	
	SET_BIT_REG (DDRC, PC4); // set output
	CLEAR_BIT_REG (PORTC, PC4); // set low   
	
	// Measured: SPI power-down slew rate time: 50 usec, use 5msec 
	
	printf_P (PSTR("> Wait 5 msec ... \r\n"));
	_delay_us (5000);
	
	// FWSPI_PWR_EN (PC4)
	printf_P (PSTR("> Turn-on flash power. \r\n"));
	CLEAR_BIT_REG (DDRC, PC4); // set input (open-drain with external PU)
	while (! IS_BIT_SET (PINC, PC4)); // wait for FWSPI_PWR_EN goes high. Can be use to extend the delay. 
	
	// Measured: SPI power-up slew rate time: 2.3 msec, use 5msec 
	
	printf_P (PSTR("> Wait 5 msec  ... \r\n"));
	_delay_us (5000);
	
	// CORST_N (PA2) 
	printf_P (PSTR("> Release CORST#. \r\n"));
	CLEAR_BIT_REG (DDRA, PA2); // set input (open-drain with external PU)
	while (! IS_BIT_SET (PINA, PA2)); // wait for CORST# to goes high. Can be use to extend the delay (maybe other source keep this signal low). 
	
	SET_BIT_REG (GIFR, INTF0); // 13/05/2020: Fixed double power-cycle issue on module power-up by clear INTF0 that may set duo to unexpected SPILOAD pulse occur before reset was released.   
	
	// Note: ~17 msec internal BMC reset delay before the second SPILOAD pulse and the code running.
		
	if (IS_BIT_SET (l_PINC, PC2))
	{
		// this valid only in BMC_RESET mode since in ENTER_FUP mode the EXTEND_SPILOAD_N pin keep pull low and can't detect the second pulse. 
		
		// There are two method to continue and exist:
		// 1. Wait 20 msec to pass the ~17 msec internal BMC reset delay and mask the second SPILOAD_N pulse.
		// 2. Wait for the second pulse on SPILOAD_N after release CORST#. 
		// If some external signal keep CORST# low, we must use option (2) and wait until the external signal release CORST#; otherwise a nother reset cycle will be ocure (end-less loops of resets) 
		 
		// method 2
		printf_P (PSTR("> Wait for the second pulse on SPILOAD# ... \r\n"));
		while (! IS_BIT_SET (GIFR, INTF0));
		
		/*	
		// method 1
		printf_P (PSTR("> Wait 20 msec  ... \r\n"));
		_delay_us (20000); // wait for end of 17 msec internal reset delay to mask the second SPILOAD pulse, use 20 msec.
		SET_BIT_REG (GIFR, INTF0); //  Clear INTF0 bit caused by the second SPILOAD pulse.
		*/
	}
	
	SET_BIT_REG (GIFR, INTF0); //  Clear INTF0 bit caused by the second SPILOAD pulse.
	printf_P (PSTR("> Done. \r\n"));
	
	WD_Stop();
}
