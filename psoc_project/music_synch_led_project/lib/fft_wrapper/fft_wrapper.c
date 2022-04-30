#include "fft_wrapper.h"
#include <stdio.h>

/* 0 -> FFT, 1 -> IFFT */
#define IFFT_FLAG               (0)
/* Minimum and maximum supported FFT length */
#define MIN_SUPPORTED_FFT_SIZE  (32)
#define MAX_SUPPORTED_FFT_SIZE  (4096)

/* Generates sin wave. Used for testing */
static void generate_sin_wave(int32_t* res, size_t length);

void compute_rfft(arm_rfft_fast_instance_f32* fft_obj, int32_t* input, float* res, size_t fft_size)
{
    /* Convert Q31 to float */
    /* arm_q31_to_float does the following:
     *      pDst[i] = float(pSrc[i])
     * So it should be ok to reuse same memory to save some space
     */
    arm_q31_to_float(input, (float*)input, fft_size);

    /* Calculate FFT */
    arm_rfft_fast_f32(fft_obj, (float*)input, res, IFFT_FLAG);

    /* Calculate magnitude */
    /* arm_cmplx_mag_f32 for every element does thefollowing:
     *      pDst[n] = sqrt(pSrc[(2*n)+0]^2 + pSrc[(2*n)+1]^2);
     *
     * So:
     *      pDst[0] is created from pSrc[0] and pSrc[1]
     *      pDst[1] is created from pSrc[2] and pSrc[2]
     *      pDst[2] is created from pSrc[4] and pSrc[5]
     *
     * So it should be safe to use same buffer as input ad output
     * parameters to save some memory
     */
    /* TODO: arm_cmplx_mag_squared_f32 and arm_cmplx_mag_f32 do the same thing
     * except that arm_cmplx_mag_squared_f32 does not calculate sqrt() so it
     * executes faster, but it will make results more spread out.
     * Need to check if this change is applicable.
     */
    arm_cmplx_mag_f32(res, res, fft_size);
}

arm_status measure_fft_performance(cyhal_timer_t* timer_obj)
{
    arm_status arm_res;

    /* Buffers for input signal and FFT result */
    int32_t input_signal[MAX_SUPPORTED_FFT_SIZE];
    float fft_res[MAX_SUPPORTED_FFT_SIZE];

    /* Print to make results standout */
    printf("\r\n\n");
    printf("####################### FFT performance testing results #######################\r\n");
    printf("\r\nFFT size\tduration (us)\r\n");

    for(size_t fft_size = MIN_SUPPORTED_FFT_SIZE; fft_size <= MAX_SUPPORTED_FFT_SIZE; fft_size *= 2)
    {
        /* Generate input signal */
        generate_sin_wave(input_signal, fft_size);

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

        /* Compute FFT */
        compute_rfft(&fft_tests_obj, input_signal, fft_res, fft_size);

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

static void generate_sin_wave(int32_t* res, size_t length)
{
    /* Generate sin wave with frequency = length */
    float frequency = (2 * PI) / length;
    int32_t amplitude = INT32_MAX;
    for (size_t i = 0; i < length; i++)
    {
        res[i] = amplitude * sin(frequency * i);
    }
}
