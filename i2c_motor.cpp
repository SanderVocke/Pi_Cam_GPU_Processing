#include "i2c_motor.h"
#include "wiringPiI2C.h"
#include <stdio.h>

#define DEVADDR 0x0B

direction_t gDirection[2] = {FORWARD, FORWARD};
unsigned char gSpeed[2] = {0,0};
int fd = -1;
bool error = false;

#define RETERROR {error = true; return false;}
#define RETSUCCESS {error = false; return true;}
#define NUMTRIES 2

bool doWrite(motor_t motor){
	if(fd == -1) RETERROR;
	int result = -1;
	for(int i=0; ((result<0) && (i<NUMTRIES)); i++){
		result =wiringPiI2CWriteReg8(fd, 
					((motor == LEFT_MOTOR)?2:0) | ((gDirection[(int)motor]==FORWARD)?1:0),
					gSpeed[(int)motor]);
	}
	if(result < 0) RETERROR;
	//if(wiringPiI2CRead(fd)!=0xAA) RETERROR;
	RETSUCCESS;
}

bool startI2CMotor(void){
	fd =  wiringPiI2CSetup(DEVADDR);
	if(fd == -1) RETERROR;
	return doWrite(LEFT_MOTOR);
}

bool setSpeed(motor_t motor, unsigned char speed){
	if(gSpeed[(int)motor] == speed) return !error;
	bool res = doWrite(motor);
	if(!res) RETERROR;
	gSpeed[(int)motor] = speed;
	RETSUCCESS;
}

bool setDirection(motor_t motor, direction_t direction){
	if(gDirection[(int)motor] == direction) return !error;
	bool res = doWrite(motor);
	if(!res) RETERROR;
	gDirection[(int)motor] = direction;
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