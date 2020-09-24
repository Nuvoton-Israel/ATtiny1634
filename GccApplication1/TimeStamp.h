/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#ifndef _TIME_STAMP_H_
#define _TIME_STAMP_H_

#define EVENT_MICRO_PORF			0x01 // Micro-controller Power-on Reset occur
#define EVENT_MICRO_EXTRF			0x02 // Micro-controller External Reset occur
#define EVENT_MICRO_BORF			0x04 // Micro-controller Brown-out Reset occur
#define EVENT_MICRO_WDRF			0x08 // Micro-controller Watchdog Reset occur
#define EVENT_BMC_WD				0x10 // BMC Watchdog Reset issued
#define EVENT_BMC_RESET_DETECT		0x20 // BMC flash power-cycle issued (duo to BMC reset detected)
#define EVENT_BMC_ENTER_FUP			0x40 // BMC enter FUP (duo to Host requested)
#define EVENT_HEARTBEAT				0x80 // generic heartbeat

extern void TimeStamp_Reset (void);
extern void TimeStamp_PeriodicTask (uint32_t ElapsedTime /*msec*/);

extern uint8_t EventType;

#endif


