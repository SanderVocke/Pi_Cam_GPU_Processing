#include "i2c_motor.h"

direction_t gDirection[2] = {FORWARD, FORWARD};
unsigned char gSpeed[2] = {0,0};

bool startI2CMotor(void){
	return true;
}

bool setSpeed(motor_t motor, unsigned char speed){
	if(gSpeed[(int)motor] == speed) return true;
	gSpeed[(int)motor] = speed;
	return true;
}

bool setDirection(motor_t motor, direction_t direction){
	if(gDirection[(int)motor] == direction) return true;
	gDirection[(int)motor] = direction;
	return true;
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