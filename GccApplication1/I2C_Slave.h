/*
 * Nuvoton RunBMC Module Project
 *
 * Created: 1/28/2019 6:43:01 PM
 * Author : lior.albaz@Nuvoton.com
 */ 

#ifndef _I2C_SLAVE_H_
#define _I2C_SLAVE_H_

#define I2C_Slave_Addr ((uint8_t)0x70) // base address to emulate x4 I2C devices. Address value must aline to 4.

// I2C_Slave_PeriodicTask() can be use in main loop or system tick to restart I2C slave module in case of time-out. 
// Time-out value is update on each I2C action interrupt to I2C_TIME_OUT or 0 to disable the time-out.
#define I2C_TIME_OUT (uint32_t)100  //  msec 
extern void I2C_Slave_Init (uint8_t BaseAddr);
extern void I2C_Slave_PeriodicTask (uint32_t ElapsedTime /*msec*/); 

//--------------------------------------------
// Callback function for emulated devices 
//--------------------------------------------
// Status: [input] see define below. 
// pBuffer: [output] pointer to write or read buffer.
// MaxNumOfByte: [output] pointer to write or read buffer size in bytes.
// NumOfByteUse: [input] return number of byte sent or received 
// return: response type (I2C_NACK or I2C_ACK). Relevant on I2C_WR_START, I2C_RD_START and I2C_WR_BUFF_FULL states. 
typedef uint8_t (*I2C_DEVICE_FUNC)(uint8_t Status, /*out*/ uint8_t **pBuffer, /*out*/ uint8_t *MaxNumOfByte, uint8_t NumOfByteUse);

//-------------------------------------------------
// I2C slave action for call-back function
//-------------------------------------------------
#define I2C_RD 0x01
#define I2C_WR 0x00

#define I2C_START	0x10
#define I2C_BUFF	0x20
#define I2C_STOP	0x30
#define I2C_ERROR	0x40

#define I2C_WR_START		(I2C_START|I2C_WR)	// write transaction started.
												// response type for address state is according to func return value (I2C_NACK or I2C_ACK).  
												// func device must allocate buffer at this state. if not assign a buffer (MaxNumOfByte=0), the slave will response with NACK to master on the next byte.
#define I2C_RD_START		(I2C_START|I2C_RD)	// read transaction start. 	
												// response type for address state is according to func return value (I2C_NACK or I2C_ACK).  
												// func device may allocate buffer at this state. if not assign a buffer (MaxNumOfByte=0), I2C_BUFF even will occur next.
#define I2C_WR_BUFF_FULL	(I2C_BUFF|I2C_WR) 	// issue on master write after reciveing MaxNumOfByte from master and before response the last byte; 
												// response type for MaxNumOfByte byte is according to func return value (I2C_NACK or I2C_ACK). 
												// if not assign a new buffer (MaxNumOfByte=0), the slave will response with NACK to master on the next byte (MaxNumOfByte+1).
												// NumOfByteUse is reset. 
#define I2C_RD_BUFF_EMPTY	(I2C_BUFF|I2C_RD)	// issue on master read after sent MaxNumOfByte to master; 
												// if not reassign a buffer (MaxNumOfByte=0), the slave will return 0xFF value to master on next byte. 
												// NumOfByteUse is reset. 
#define I2C_WR_STOP			(I2C_STOP|I2C_WR)	// write transaction ended with stop or re-start; NumOfByteUse represend number of bytes receiver. 	
#define I2C_RD_STOP			(I2C_STOP|I2C_RD)	// read transaction ended with stop or re-start;  NumOfByteUse represend number of bytes sent. 
#define I2C_WR_ERROR		(I2C_ERROR|I2C_WR)	// write transaction ended with bus error; NumOfByteUse represend number of bytes receiver. 	
#define I2C_RD_ERROR		(I2C_ERROR|I2C_RD)  // read transaction ended with bus error; NumOfByteUse represend number of bytes sent. 

#define I2C_NACK	1
#define I2C_ACK		0


// default is NULL for all pointers
extern I2C_DEVICE_FUNC pI2C_Device_Func [4];

#endif
	