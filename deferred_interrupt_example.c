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

/*------------------basic deferred interrupt with a binary semaphore -----------------------------------------*/
/* The following code example defers an interrupt with a binary semaphore. 
Task 16 provides a 10ms pulse every 85 ms on pin 16.
Pin 21 in configured to generate an interrupt on a rising edge.
Pin 20 is toggled by Task 20 when it takes the binary semaphore. 
Jumper pin 16 to 21.
You should be able to see pin 20 toggling in response to the source signal rising edges every 85ms.
*/

xSemaphoreHandle gpio21Sem;

void clear_isr_gpio21(void);

static void gpio_isr(void){
   clear_isr_gpio21(); 
   xSemaphoreGiveFromISR(gpio21Sem, NULL); 
}
/*register the isr handler */
int gpio_isr_init(void){
    if ( isr_register(IRQ_GPIO0, GPIO_PRIORITY, (0x1U << 0x3U), gpio_isr) != 0)
        return -1;
    return 0;
}

void cputime(unsigned long numofms) { volatile unsigned long i = 0; for(i=0;i<numofms*1851;i++); }

void task20(void *pParam) {
(void) pParam;
  portTickType xLastWakeTime;
  const portTickType xFrequency = 2;
     xLastWakeTime = xTaskGetTickCount();
     for(;;)
     {    
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
        while( xSemaphoreTake( gpio21Sem, portMAX_DELAY ) == pdTRUE )
        gpio_pin_toggle(20);
     }
}
void task16(void *pParam) {
(void) pParam;
  portTickType xLastWakeTime;
  const portTickType xFrequency = 85;//1 tick is 1ms
     xLastWakeTime = xTaskGetTickCount();
     for( ;; )
     {       
        vTaskDelayUntil(&xLastWakeTime,xFrequency);      
        gpio_pin_set(16, 1);
        cputime(10);
        gpio_pin_set(16, 0);      
    }
}
void main(void) {
	gpio_pin_init(GPIO_20, OUT, GPIO_PIN_PULL_UP);
 	gpio_pin_init(GPIO_16, OUT, GPIO_PIN_PULL_UP);
 
 	// enable GPIO pin 21 for interrupt
 	gpio_pin_init(GPIO_21, IN, GPIO_PIN_PULL_NON);
 	gpio_pin_isr_init(GPIO_21, GPREN);
	gpio_isr_init();
	
	// task creation	    	 
    	xTaskCreate(task20, "GPIO20", 128, NULL, 2, NULL);
    	xTaskCreate(task16, "GPIO16", 128, NULL, 1, NULL);
    	
    	// semaphores
    	gpio21Sem = xSemaphoreCreateBinary(); 
    			
	vTaskStartScheduler();
	while(1);
}
void vApplicationIdleHook( void ){}
void vApplicationTickHook( void ){}