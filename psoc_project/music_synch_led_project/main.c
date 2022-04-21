#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ws2812_lib/ws2812.h"
#include "helper_utils.h"

/* Defines for blinky LEDs task */
#define BLINKY_LEDS_TASK_NAME       ("Blinky LEDs task")
#define BLINKY_LEDS_TASK_STACK_SIZE (2 *1024)
#define BLINKY_LEDS_TASK_PRIORITY   (5)

void blinky_leds_task(void *arg);

int main(void)
{
    cy_rslt_t cy_res;
    BaseType_t rtos_res;

    /* Initialize the device and board peripherals */
    cy_res = cybsp_init();
    CY_ASSERT(CY_RSLT_SUCCESS == cy_res);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Clear screen */
    printf("\033[2J");
    printf("\r\n***** LED control app started *****\r\n\n");
    printf("Build date %s and time %s\r\n\n", __DATE__, __TIME__);

    /* Create FreeRTOS task */
    rtos_res = xTaskCreate(blinky_leds_task, BLINKY_LEDS_TASK_NAME, BLINKY_LEDS_TASK_STACK_SIZE, NULL, BLINKY_LEDS_TASK_PRIORITY, NULL);
    ASSERT_WITH_PRINT(pdPASS == rtos_res, "%s didn't started!\r\n", BLINKY_LEDS_TASK_NAME);

    vTaskStartScheduler();

    for(;;)
    {
        /* vTaskStartScheduler never returns */
    }
}

void blinky_leds_task(void *arg)
{
    (void)arg;
    ws2818_res_t ws_res;

    printf("%s started!\r\n", BLINKY_LEDS_TASK_NAME);

    /* MISO and SCLK are not needed sothey are not connected (NC) */
    ws_res = ws2812_init(CYBSP_A0, NC, NC);
    ASSERT_WITH_PRINT(ws2812_success == ws_res, "ws2812_init failed\r\n");

    for(;;)
    {
        ws2812_set_all_leds(255, 0, 0);    // Set all LEDs to RED at max brighness
        ws2812_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ws2812_set_all_leds(0, 255, 0);    // Set all LEDs to GREEN at max brighness
        ws2812_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ws2812_set_all_leds(0, 0, 255);    // Set all LEDs to BLUE at max brighness
        ws2812_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
