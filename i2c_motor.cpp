#include "i2c_motor.h"
#include "wiringPiI2C.h"

#define DEVADDR 0x0B

direction_t gDirection[2] = {FORWARD, FORWARD};
unsigned char gSpeed[2] = {0,0};
int fd = -1;

bool doWrite(motor_t motor){
	if(fd == -1) return false;
	int result = wiringPiI2CWriteReg8(fd, 
					((motor == LEFT_MOTOR)?2:0) | ((gDirection[(int)motor]==FORWARD)?1:0),
					gSpeed[(int)motor]);
	return (result != -1);
}

bool startI2CMotor(void){
	fd =  wiringPiI2CSetup(DEVADDR);
	return (fd != -1);
}

bool setSpeed(motor_t motor, unsigned char speed){
	if(gSpeed[(int)motor] == speed) return true;
	gSpeed[(int)motor] = speed;
	return doWrite(motor);
}

bool setDirection(motor_t motor, direction_t direction){
	if(gDirection[(int)motor] == direction) return true;
	gDirection[(int)motor] = direction;
	return doWrite(motor);
}

bool setSpeedDir(motor_t motor, direction_t direction, unsigned char speed){
	if(!setDirection(motor, direction)) return false;
	if(!setSpeed(motor, speed)) return false;
	return true;
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