/* Hardware */
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
/* Free RTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/* Application configuration */
#include "app_config.h"
/* Libraries */
#include <stdio.h>
#include "ws2812.h"
#include "fft_wrapper.h"
#include "audio_visualizer.h"

/* Defines for blinky LEDs task */
#define BLINKY_LEDS_TASK_NAME       ("Blinky LEDs task")
#define BLINKY_LEDS_TASK_STACK_SIZE (2 * 1024)
#define BLINKY_LEDS_TASK_PRIORITY   (5)

/* Macro to convert sample rate to sample period in nanoseconds */
#define SAMPLE_RATE_TO_PERIOD_NS(hz)  ((uint32_t)(((float)1000000000) / ((float)(hz))))

/* Macro that prints given text when assertion fails */
#define ASSERT_WITH_PRINT(x, ...)   if(!(x)) { printf(__VA_ARGS__ ); CY_ASSERT(0); }

/* Used by GDB for better debugging experience */
UBaseType_t __attribute__((used)) uxTopUsedPriority;
#define uxTopReadyPriority uxTopUsedPriority

/* ADC Object */
static cyhal_adc_t adc_obj;

/* ADC Channel Object */
static cyhal_adc_channel_t adc_chan_obj;

/* ADC configuration */
static const cyhal_adc_config_t adc_cfg = {
    .continuous_scanning = true,    /* Continuous Scanning is enabled to increase performance */
    .average_count = 1,             /* Averaging is disabled */
    .vref = CYHAL_ADC_REF_INTERNAL, /* CYHAL_ADC_REF_INTERNAL is 1.2V */
    .vneg = CYHAL_ADC_VNEG_VSSA,
    .resolution = 12u,
    .ext_vref = NC,
    .bypass_pin = NC
};

/* ADC channel configuration */
static const cyhal_adc_channel_config_t adc_chan_cfg = {
    .enable_averaging = false,
    .min_acquisition_ns = SAMPLE_RATE_TO_PERIOD_NS(AUDIO_SAMPLING_RATE),
    .enabled = true                 /* Sample this channel when ADC performs a scan */
};

/* Timer is used to measure performance */
static cyhal_timer_t timer_obj;

/* Timer config */
static cyhal_timer_cfg_t timer_cfg = {
    .is_continuous = true,
    .direction = CYHAL_TIMER_DIR_UP,
    .is_compare = false,
    .period = UINT32_MAX,
    .compare_value = UINT32_MAX,
    .value = 0
};

/* CMSIS DSP library FFT object */
arm_rfft_fast_instance_f32 fft_obj;

/* Semaphore for synchronization between task and IRQ */
static SemaphoreHandle_t audio_sampling_semaphore = NULL;

/* Buffer for audio signal */
/* It need to be 2 * FFT_SIZE because while one part of the buffer is
 * processed other part is getting written ADC */
static int32_t audio_buffer[FFT_SIZE * 2];

/* FFT result will have length of FFT_SIZE_HALF, but fft wrapper
 * internally uses result buffer for temporary conversions/results
 * to save some space so result buffer must have same size as input buffer.
 * Only first half of the buffer will contain meaningful data, other half
 * will have "garbage" data.
 */
static float fft_res[FFT_SIZE];

static TaskHandle_t led_task_handle;

void blinky_leds_task(void* arg);
static void adc_event_handler(void* arg, cyhal_adc_event_t event);
static cy_rslt_t app_init(void);
static cy_rslt_t adc_init(void);

int main(void)
{
    cy_rslt_t cy_res;
    BaseType_t rtos_res;

    /* Used by GDB for better debugging experience */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    /* Initialize the device and board peripherals */
    cy_res = cybsp_init();
    CY_ASSERT(CY_RSLT_SUCCESS == cy_res);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("\r\n***** LED control app started *****\r\n\n");
    printf("Build date %s and time %s\r\n\n", __DATE__, __TIME__);

    /* Initialize application related HW and SW */
    cy_res = app_init();
    ASSERT_WITH_PRINT(CY_RSLT_SUCCESS == cy_res, "app_init failed!\r\n");

    /* Measure FFT performance before starting RToS to get more accurate results */
#if MEASURE_PERFORMANCE == 1
    arm_status arm_res;
    arm_res = measure_fft_performance(&timer_obj);
    ASSERT_WITH_PRINT(ARM_MATH_SUCCESS == arm_res, "measure_fft_performance failed!\r\n");
#endif

    /* Create semaphore */
    audio_sampling_semaphore = xSemaphoreCreateBinary();

    /* Create FreeRTOS task */
    rtos_res = xTaskCreate(blinky_leds_task, BLINKY_LEDS_TASK_NAME, BLINKY_LEDS_TASK_STACK_SIZE, NULL, BLINKY_LEDS_TASK_PRIORITY, &led_task_handle);
    ASSERT_WITH_PRINT(pdPASS == rtos_res, "%s didn't started!\r\n", BLINKY_LEDS_TASK_NAME);

    vTaskStartScheduler();

    for(;;)
    {
        /* vTaskStartScheduler never returns */
    }
}

void blinky_leds_task(void* arg)
{
    (void)arg;
    ws2818_res_t ws_res;
    cy_rslt_t cy_res;
    /* While one part of the buffer is processed other part is getting written ADC
     * then they are swapped */
    uint8_t active_adc_buffer = 0;
    uint8_t active_uart_buffer = (active_adc_buffer + 1) % 2;

    /* TODO: ws2812_init() ideally should be in app_init() but for some reasons
     * when it is called from app_init() SPI transfer complete interrupt is never raised.
     * This is probably some freeRTOS specific thing */
    /* Initialize ws2812 library */
    /* MISO and SCLK are not needed so they are not connected (NC) */
    ws_res = ws2812_init(WS2812_LEDS_PIN, NC, NC);
    ASSERT_WITH_PRINT(ws2812_success == ws_res, "ws2812_init failed\r\n");

    printf("%s started!\r\n", BLINKY_LEDS_TASK_NAME);

    for(;;)
    {
#if MEASURE_PERFORMANCE == 1
        cyhal_timer_stop(&timer_obj);
        cyhal_timer_reset(&timer_obj);
        cyhal_timer_start(&timer_obj);
#endif

        cy_res = cyhal_adc_read_async(&adc_obj, FFT_SIZE, &audio_buffer[FFT_SIZE * active_adc_buffer]);
        ASSERT_WITH_PRINT(CY_RSLT_SUCCESS == cy_res, "cyhal_adc_read_async failed!\r\n");

#if MEASURE_PERFORMANCE == 1
        uint32_t red_async_duration = cyhal_timer_read(&timer_obj);
#endif

        /* Calculate FFT */
        compute_rfft(&fft_obj, &audio_buffer[FFT_SIZE * active_uart_buffer], fft_res, FFT_SIZE);

#if MEASURE_PERFORMANCE == 1
        uint32_t fft_duration = cyhal_timer_read(&timer_obj) - red_async_duration;
#endif
        /* Visualize FFT */
        /* TODO: Visualization has low FPS when MEASURE_PERFORMANCE is 0.
         * Adding delay instead of printf() does not help. I believe that this is
         * RToS related problem, but need to check and fix this issue.
         */
        /* TODO: implement mode switching */
        visualize_fft(fft_res, FFT_SIZE_HALF, VISUALIZATION_MODE_SNAKE_FLOW_BIDIRECTIONAL);

        /* Swap buffers */
        uint8_t swap_tmp = active_uart_buffer;
        active_uart_buffer = active_adc_buffer;
        active_adc_buffer = swap_tmp;

#if MEASURE_PERFORMANCE == 1
        uint32_t visualization_duration = cyhal_timer_read(&timer_obj) - fft_duration;
#endif

        /* Semaphore will be released in ADC IRQ once cyhal_adc_read_async completes */
        xSemaphoreTake(audio_sampling_semaphore, portMAX_DELAY);

#if MEASURE_PERFORMANCE == 1
        uint32_t semaphore_wait_duration = cyhal_timer_read(&timer_obj) - visualization_duration;
        cyhal_timer_stop(&timer_obj);

        printf("\r\nPerformance measurements:\r\n");
        printf("ADC read async  %lu\r\n", red_async_duration);
        printf("FFT             %lu\r\n", fft_duration);
        printf("Visualization   %lu\r\n", visualization_duration);
        printf("Semaphore take  %lu\r\n", semaphore_wait_duration);
        printf("Total           %lu\r\n", semaphore_wait_duration + fft_duration + visualization_duration + red_async_duration);
#endif
    }
}

static cy_rslt_t app_init(void)
{
    cy_rslt_t cy_res;
    arm_status arm_res;

    /* Initialize ADC */
    cy_res = adc_init();
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Initialize timer */
    cy_res = cyhal_timer_init(&timer_obj, NC, NULL);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Configure timer */
    cy_res = cyhal_timer_configure(&timer_obj, &timer_cfg);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Initialize FFT instance for CMSIS DSP library */
    arm_res = arm_rfft_fast_init_f32(&fft_obj, FFT_SIZE);
    if(ARM_MATH_SUCCESS != arm_res)
    {
        return (!CY_RSLT_SUCCESS);
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t adc_init(void)
{
    cy_rslt_t cy_res;

    /* Initialize ADC */
    cy_res = cyhal_adc_init(&adc_obj, AUDIO_SAMPLING_PIN, NULL);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Initialize ADC channel */
    cy_res = cyhal_adc_channel_init_diff(&adc_chan_obj, &adc_obj, AUDIO_SAMPLING_PIN,
                                          CYHAL_ADC_VNEG, &adc_chan_cfg);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Update ADC configuration */
    cy_res = cyhal_adc_configure(&adc_obj, &adc_cfg);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return cy_res;
    }

    /* Register adc callback function */
    cyhal_adc_register_callback(&adc_obj, &adc_event_handler, NULL);

    /* Subscribe to the async read complete event */
    cyhal_adc_enable_event(&adc_obj, CYHAL_ADC_ASYNC_READ_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* TODO: Use DMA for cyhal_adc_read_async */
    // cy_res = cyhal_adc_set_async_mode(&adc_obj, CYHAL_ASYNC_DMA, 3);
    // if(CY_RSLT_SUCCESS != cy_res)
    // {
    //     return cy_res;
    // }

    return CY_RSLT_SUCCESS;
}

static void adc_event_handler(void* arg, cyhal_adc_event_t event)
{
    (void)arg;
    if(0u != (event & CYHAL_ADC_ASYNC_READ_COMPLETE))
    {
        BaseType_t yield_required = pdFALSE;
        xSemaphoreGiveFromISR(audio_sampling_semaphore, &yield_required);
        portYIELD_FROM_ISR(yield_required);
    }
}
