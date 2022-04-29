#include "fft_wrapper.h"
#include <stdio.h>

/* 0 -> FFT, 1 -> IFFT */
#define IFFT_FLAG               (0)
/* Minimum and maximum supported FFT length */
#define MIN_SUPPORTED_FFT_SIZE  (32)
#define MAX_SUPPORTED_FFT_SIZE  (4096)

static arm_rfft_fast_instance_f32 fft_obj;
static float complex_fft[FFT_SIZE];

static void generate_sin_signal(int32_t* res, size_t length);

arm_status rfft_init()
{
    return arm_rfft_fast_init_f32(&fft_obj, FFT_SIZE);
}

void compute_rfft(float* input, float* res)
{
    /* input is real, output is interleaved real and complex */
    arm_rfft_fast_f32(&fft_obj, input, complex_fft, IFFT_FLAG);

    /* Compute squared magnitede.
     * Note that this should be faster then arm_cmplx_mag_f32 as it does not compute sqrt()
     */
    arm_cmplx_mag_squared_f32(complex_fft, res, FFT_SIZE_HALF);
}

arm_status measure_fft_performance(cyhal_timer_t* timer_obj)
{
    arm_status arm_res;

    /* Buffers for input signal and FFT result */
    int32_t input_signal[MAX_SUPPORTED_FFT_SIZE];
    float input_signal_float[MAX_SUPPORTED_FFT_SIZE];
    float fft_res[MAX_SUPPORTED_FFT_SIZE];
    float fft_mag[MAX_SUPPORTED_FFT_SIZE];

    /* Print to make results standout */
    printf("\r\n\n");
    printf("####################### FFT performance testing results #######################\r\n");
    printf("\r\nFFT size\tduration (us)\r\n");

    for(size_t fft_size = MIN_SUPPORTED_FFT_SIZE; fft_size <= MAX_SUPPORTED_FFT_SIZE; fft_size *= 2)
    {
        /* Generate input signal */
        generate_sin_signal(input_signal, fft_size);

        /* Create and initialize FFT structure */
        arm_rfft_fast_instance_f32 fft_tests_obj;
        arm_res = arm_rfft_fast_init_f32(&fft_tests_obj, fft_size);
        if(ARM_MATH_SUCCESS != arm_res)
        {
            return arm_res;
        }

        /* Reset the timer to avoid overflow */
        cyhal_timer_stop(timer_obj);
        cyhal_timer_reset(timer_obj);
        cyhal_timer_start(timer_obj);

        /* Convert Q31 to float */
        arm_q31_to_float(input_signal, input_signal_float, fft_size);

        /* Calculate FFT */
        arm_rfft_fast_f32(&fft_tests_obj, input_signal_float, fft_res, IFFT_FLAG);

        /* Calculate magnitude */
        arm_cmplx_mag_f32(fft_res, fft_mag, fft_size);

        /* Read timer value */
        uint32_t fft_duration = cyhal_timer_read(timer_obj);

        /* Print FFT performance results */
        printf("%u\t\t%lu\r\n", fft_size, fft_duration);
    }

    /* Print to make results standout */
    printf("\r\n###############################################################################\r\n");
    printf("\r\n\n");

    return ARM_MATH_SUCCESS;
}

static void generate_sin_signal(int32_t* res, size_t length)
{
    /* Generate sin wave with frequency = length */
    float frequency = (2 * PI) / length;
    int32_t amplitude = INT32_MAX;
    for (size_t i = 0; i < length; i++)
    {
        res[i] = amplitude * sin(frequency * i);
    }
}
