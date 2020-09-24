/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#ifndef _I2C_DEVICE_GPI_H_
#define _I2C_DEVICE_GPI_H_


extern void I2C_Device_GPI_Init (uint8_t DeviceIndex);
extern void GPI_PeriodicTask (uint32_t ElapsedTime /*msec*/);
#endif


