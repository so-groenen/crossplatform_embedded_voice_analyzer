
#include <cstdio>
#include "pico/stdlib.h"
 


// MIT License

// Copyright (c) 2025 Tim Berglund

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifdef CYW43_WL_GPIO_LED_PIN
    #include "pico/cyw43_arch.h"
#endif

int pico_led_init(void) 
{
#ifdef CYW43_WL_GPIO_LED_PIN
    return cyw43_arch_init();
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

void pico_set_led(bool led_on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
}


extern "C" {
    #include "FreeRTOSConfig.h"
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
    #include "timers.h"
}

static void vSenderTask(void *pvParameters);
static void vReceiverTask(void *pvParameters);
static void vTimerCallback(TimerHandle_t xTimer);

static TimerHandle_t xTimer;
static QueueHandle_t xQueue;
static TaskHandle_t xSenderTask;
static TaskHandle_t xReceiverTask;

typedef struct {
    uint32_t messageData;
    // Add other fields as needed
} Message_t;


static void vTimerCallback(TimerHandle_t xTimer) {
    // Wake up sender task using task notification
    xTaskNotifyGive(xSenderTask);
}

static void vSenderTask(void *pvParameters) {
    Message_t message;
    uint32_t counter = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        message.messageData = counter++;

        if (xQueueSend(xQueue, &message, 0) != pdPASS) {
            printf("Sender task queue is full\n");
        }
    }
}


static void vReceiverTask(void *pvParameters) {
    Message_t receivedMessage;

    for(;;) {
        if (xQueueReceive(xQueue, &receivedMessage, portMAX_DELAY) == pdPASS) {
            pico_set_led(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            pico_set_led(false);
        }
    }
}


void rtosSetup() {
    // Create queue
    xQueue = xQueueCreate(5, sizeof(Message_t));

    if(xQueue != NULL) {
        // Create sender task
        xTaskCreate(
            vSenderTask,
            "Pointless Sender",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 1,
            &xSenderTask
        );

        // Create receiver task
        xTaskCreate(
            vReceiverTask,
            "LED Blinker",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 1,
            &xReceiverTask
        );

        // Create timer (1 second period)
        xTimer = xTimerCreate(
            "Timer",
            pdMS_TO_TICKS(1000),
            pdTRUE,  // Auto-reload timer
            0,       // Timer ID
            vTimerCallback
        );

        if(xTimer != NULL) {
            xTimerStart(xTimer, 0);
        }
    }
}

int main() {
    stdio_init_all();

    pico_led_init();
    pico_set_led(true);
    sleep_ms(1000);
    pico_set_led(false);

    printf("FreeRTOS\n");
    rtosSetup();

    printf("STARTING SCHEDULER\n");
    vTaskStartScheduler();

    // "We should never reach here"
    printf("SCHEDULER STARTED\n");
    while(1)
    {
        configASSERT(0);
    }
}