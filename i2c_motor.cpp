#include "i2c_motor.h"
#include "wiringPiI2C.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define DEVADDR 0x0B

// I2C definitions

#define I2C_SLAVE	0x0703
#define I2C_SMBUS	0x0720	/* SMBus-level access */

#define I2C_SMBUS_READ	1
#define I2C_SMBUS_WRITE	0

// SMBus transaction types

#define I2C_SMBUS_QUICK		    0
#define I2C_SMBUS_BYTE		    1
#define I2C_SMBUS_BYTE_DATA	    2 
#define I2C_SMBUS_WORD_DATA	    3
#define I2C_SMBUS_PROC_CALL	    4
#define I2C_SMBUS_BLOCK_DATA	    5
#define I2C_SMBUS_I2C_BLOCK_BROKEN  6
#define I2C_SMBUS_BLOCK_PROC_CALL   7		/* SMBus 2.0 */
#define I2C_SMBUS_I2C_BLOCK_DATA    8

#define I2C_SMBUS_BLOCK_MAX	32	/* As specified in SMBus standard */	
#define I2C_SMBUS_I2C_BLOCK_MAX	32	/* Not specified but we use same structure */

direction_t gDirection[2] = {FORWARD, FORWARD};
unsigned char gSpeed[2] = {0,0};
int fd = -1;
bool error = false;

#define RETERROR {DBG("I2C ERROR!"); error = true; return false;}
#define RETSUCCESS {error = false; return true;}
#define NUMTRIES 2

struct i2c_smbus_ioctl_data
{
  char read_write ;
  uint8_t command ;
  int size ;
  union i2c_smbus_data *data ;
} ;

union i2c_smbus_data
{
  uint8_t  byte ;
  uint16_t word ;
  uint8_t  block [I2C_SMBUS_BLOCK_MAX + 2] ;	// block [0] is used for length + one more for PEC
} ;


static inline int getData(int fd, union i2c_smbus_data *data)
{
  struct i2c_smbus_ioctl_data args ;

  args.read_write = I2C_SMBUS_READ ;
  args.command    = 0 ;
  args.size       = 8;
  args.data       = data ;
  data->block[0] = 3;
  return ioctl (fd, I2C_SMBUS, &args) ;
}

bool doWrite(motor_t motor){
	if(fd == -1) RETERROR;
	int result = -1;
	union i2c_smbus_data data;
	for(int i=0; ((result<0) && (i<NUMTRIES)); i++){
		result =wiringPiI2CWriteReg8(fd, 
					((motor == LEFT_MOTOR)?2:0) | ((gDirection[(int)motor]==FORWARD)?1:0),
					gSpeed[(int)motor]);		
		getData(fd, &data);
		if(motor == LEFT_MOTOR){
			if((data.block[1]&1) && gDirection[(int)motor]==BACKWARD) {result = -1; DBG("LEFT dir mismatch 1");}
			if(!(data.block[1]&1) && gDirection[(int)motor]==FORWARD) {result = -1; DBG("LEFT dir mismatch 2");}
			if(data.block[2] != gSpeed[(int)motor]) {result = -1; DBG("LEFT speed mismatch");}
		}
		else{
			if((data.block[1]&2) && gDirection[(int)motor]==BACKWARD) {result = -1; DBG("RIGHT dir mismatch 1");}
			if(!(data.block[1]&2) && gDirection[(int)motor]==FORWARD) {result = -1; DBG("RIGHT dir mismatch 2");}
			if(data.block[3] != gSpeed[(int)motor]) {result = -1; DBG("RIGHT speed mismatch");}
		}
	}
	if(result < 0) RETERROR;
	//if(wiringPiI2CRead(fd)!=0xAA) RETERROR;
	//i2c_smbus_data data;
	//getData(fd, &data);
	//DBG("I2C Write: %d %d", ((motor == LEFT_MOTOR)?2:0) | ((gDirection[(int)motor]==FORWARD)?1:0), gSpeed[(int)motor]);
	//DBG("I2C Read: %d %d %d", data.block[1], data.block[2], data.block[3]);
	RETSUCCESS;
}

bool startI2CMotor(void){
	fd =  wiringPiI2CSetup(DEVADDR);
	if(fd == -1) RETERROR;
	return doWrite(LEFT_MOTOR);
}

bool setSpeed(motor_t motor, unsigned char speed){
	if(gSpeed[(int)motor] == speed) return !error;
	gSpeed[(int)motor] = speed;
	//DBG("setSpeed %s %d", (motor == LEFT_MOTOR) ? "LEFT" : "RIGHT", speed);
	bool res = doWrite(motor);
	if(!res) RETERROR;
	RETSUCCESS;
}

bool setDirection(motor_t motor, direction_t direction){
	if(gDirection[(int)motor] == direction) return !error;
	gDirection[(int)motor] = direction;
	bool res = doWrite(motor);
	if(!res) RETERROR;	
	RETSUCCESS;
}

bool setSpeedDir(motor_t motor, direction_t direction, unsigned char speed){
	return (setDirection(motor, direction) && setSpeed(motor, speed));
}

bool stopI2CMotor(void){
	return true;
}

unsigned char getSpeed(motor_t motor){
	return gSpeed[(int)motor];
}

direction_t getDirection(motor_t motor){
	return gDirection[(int)motor];
}