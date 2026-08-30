//starting code for robot chasses control using RPI4B with freeRTOS
//authored by Jared Hull
//converted by MS to current RPI4B freeRTOS port MS 7/28/22
//note you cannot use GPIO (pins) 14 and 15. They are used as a UART port counsel connection.
//pins means GPIO below, they are not the connector pins.

/**< C libraries includes*/
#include <stddef.h>
#include <stdint.h>
//#include <stdio.h>
/**< FreeRTOS port includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
/**< Drivers includes*/
#include "gpio.h"

/*
 * Input commands and pin mounts
 */

#define FORWARD 'F'
#define LEFT    'L'
#define BACK    'B'
#define RIGHT   'R'
#define STOP    'S'

//Adjust to add intermediate speeds
#define IDLE    0
#define SPEED   1

//Adjust to reverse motor polarity
int LEFT_MOTOR  = 1;
int RIGHT_MOTOR = 0;

//Testing for RPI5

//Motor Control 
#define EN1 20  //pwm pin, left motor
#define EN2 21  //pwm pin, right motor
#define A_1 6   //Y1, left motor positive
#define A_2 13  //Y2, left motor negative
#define A_3 19  //Y3, right motor positive
#define A_4 26  //Y4, right motor negative

//Pins for your distance sensor
#define TRIG 9
#define ECHO 11

//Task monitor trace pins for tasks 1 thru 3

#define T1_PIN 16
#define T2_PIN 5
#define T3_PIN 0

//LCD pins. Feel free to change.
#define RS 2
#define RW 3
#define E_ 4
#define D4 17
#define D5 27
#define D6 22
#define D7 10

//moveRobot as used in lab 1
void moveRobot(char command);

//Helper function for moveRobot as used in lab 1
void motorControl(int ifLeftMotor, char command);

//

/****
		TODO: LCD function declarations. See GPIO pin settings above.






	****/

int DISTANCE_IN_TICKS = 0;


//Task 1 is implemented for you. It interfaces with the distance sensor and 
//calculates the number of ticks the echo line is high after trigger, storing that value in
//the global variable DISTANCE_IN_TICKS

//You may need to alter some values as described in the lab documentation.

void task1() {
	portTickType xLastWakeTime;
	const portTickType xFrequency = 200 / portTICK_RATE_MS;
	
	xLastWakeTime = xTaskGetTickCount();

	while(1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		
		//IN TASK
		gpio_pin_set(T1_PIN, 1);
		//

		gpio_pin_set(TRIG, 1);

		vTaskDelay(1);

		gpio_pin_set(TRIG, 0);
		while(gpio_pin_read(ECHO) == 0);
		
		portTickType curr = xTaskGetTickCount();
		while(gpio_pin_read(ECHO) == 1);
		portTickType traveltime_in_ticks = xTaskGetTickCount() - curr;

		DISTANCE_IN_TICKS = traveltime_in_ticks;
		
		//END TASK
		gpio_pin_set(T1_PIN, 0);
	}
}

/****
		TODO: Add additional tasks here





	****/


int main(void) {
	gpio_pin_init(T1_PIN, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(T2_PIN, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(T3_PIN, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_set(T1_PIN, 0);
	gpio_pin_set(T2_PIN, 0);
	gpio_pin_set(T3_PIN, 0);

	gpio_pin_init(EN1, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(EN2, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(A_1, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(A_2, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(A_3, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(A_4, OUT, GPIO_PIN_PULL_UP);

	gpio_pin_init(TRIG, OUT, GPIO_PIN_PULL_UP);
	gpio_pin_init(ECHO, IN, GPIO_PIN_PULL_NON);
	gpio_pin_set(TRIG, 0);
	
	/****
		TODO: Initialize LCD here





	****/

	//initFB();

	//DisableInterrupts();
	//InitInterruptController();

	xTaskCreate(task1, "t1", 128, NULL, 2, NULL);
	/****
		TODO: Create more tasks here

	****/

	//set to 0 for no debug, 1 for debug, or 2 for GCC instrumentation (if enabled in config)
	//loaded = 1;

	vTaskStartScheduler();

	/*
	 *	We should never get here, but just in case something goes wrong,
	 *	we'll place the CPU into a safe loop.
	 */
	while(1) {
		;
	}
}

/****
		TODO: Add LCD functions here.






	****/

void moveRobot(char command) {
    switch(command) {
        case FORWARD:
            motorControl(LEFT_MOTOR, FORWARD);
            motorControl(RIGHT_MOTOR, FORWARD);
            break;
        case LEFT:
            motorControl(LEFT_MOTOR, STOP);
            motorControl(RIGHT_MOTOR, FORWARD);
            break; 
        case BACK:
            motorControl(LEFT_MOTOR, BACK);
            motorControl(RIGHT_MOTOR, BACK);
            break; 
        case RIGHT:
            motorControl(LEFT_MOTOR, FORWARD);
            motorControl(RIGHT_MOTOR, STOP);
            break;
        case STOP:
            motorControl(LEFT_MOTOR, STOP);
            motorControl(RIGHT_MOTOR, STOP);
            break;
        default:
            break;
    }   
}

void motorControl(int ifLeftMotor, char command) {
    int enable      = ifLeftMotor ? EN1 : EN2;
    int motorPos    = ifLeftMotor ? A_1 : A_3;
    int motorNeg    = ifLeftMotor ? A_2 : A_4;

    switch (command) {
        case FORWARD:
            gpio_pin_set(enable, SPEED);
            gpio_pin_set(motorPos, 1);
            gpio_pin_set(motorNeg, 0);
            break;
        case BACK:
            gpio_pin_set(enable, SPEED);
            gpio_pin_set(motorPos, 0);
            gpio_pin_set(motorNeg, 1);
            break;    
        case STOP:
            gpio_pin_set(motorPos, 0);
            gpio_pin_set(motorNeg, 0);
            break;
        default:
            break;           
    }
}

void vApplicationIdleHook( void ){}
void vApplicationTickHook( void ){}