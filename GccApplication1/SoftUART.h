/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#ifndef _SOFT_UART_H_
#define _SOFT_UART_H_

#define SoftUart_PinNum ((uint8_t)0x05) // pin number on port C to output the UART TX.

extern int SoftUart_PutChar_Stream (char var, FILE *stream);
extern void SoftUart_Init (uint8_t PinNum);

#endif


