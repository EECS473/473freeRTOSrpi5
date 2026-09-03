/**< C libraries includes*/
#include <stddef.h>
#include <stdint.h>

/**< FreeRTOS port includes*/
#include "FreeRTOS.h"
#include "task.h"

/**< Drivers includes*/
#include "gpio.h"

/* This task toggles GPIO21 every 1 ms. */
void task1(void *pParam) {
    (void)pParam;
    portTickType xLastWakeTime;
    const portTickType xFrequency = 1; // 1 ms task frequency
    int outputState = 0;

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        outputState = !outputState;
        gpio_pin_set(GPIO_21, outputState);
    }
}

void main(void) {
    gpio_pin_init(GPIO_21, OUT, GPIO_PIN_PULL_UP);
    gpio_pin_set(GPIO_21, 0);

    xTaskCreate(task1, "GPIO21", 128, NULL, 1, NULL);
    vTaskStartScheduler();

    while (1) {
        ;
    }
}

void vApplicationIdleHook(void) {}
void vApplicationTickHook(void) {}