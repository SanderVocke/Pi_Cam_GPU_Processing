#ifndef __I2C_MOTOR_H__
#define __I2C_MOTOR_H__

#include "common.h"
#define MAX_SPEED 150
#define MAX_B_SPEED 50

typedef enum motor{
	LEFT_MOTOR,
	RIGHT_MOTOR
}motor_t;

typedef enum direction{
	FORWARD,
	BACKWARD
}direction_t;

bool startI2CMotor(void);
bool setSpeed(motor_t motor, unsigned char speed);
bool setDirection(motor_t motor, direction_t direction);
bool setSpeedDir(motor_t motor, direction_t direction, unsigned char speed);
bool stopI2CMotor(void);
unsigned char getSpeed(motor_t motor);
direction_t getDirection(motor_t motor);

#endif