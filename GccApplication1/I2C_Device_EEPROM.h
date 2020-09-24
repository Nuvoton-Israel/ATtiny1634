/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#ifndef _I2C_DEVICE_EEPROM_H_
#define _I2C_DEVICE_EEPROM_H_


extern void I2C_Device_EEPROM_Init (uint8_t DeviceIndex);

extern void WD_PeriodicTask (uint32_t ElapsedTime /*msec*/);
extern void WD_Touch (void);
extern void WD_Stop (void);

#endif


