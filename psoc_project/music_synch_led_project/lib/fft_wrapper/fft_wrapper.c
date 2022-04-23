#include "fft_wrapper.h"

#define FFT_FLAG    (0) /* 0 -> FFT, 1 -> IFFT */

static arm_rfft_fast_instance_f32 fft_obj;
static float complex_fft[FFT_SIZE];

arm_status rfft_init()
{
    return arm_rfft_fast_init_f32(&fft_obj, FFT_SIZE);
}

void compute_rfft(float* input, float* res)
{
    /* input is real, output is interleaved real and complex */
    arm_rfft_fast_f32(&fft_obj, input, complex_fft, FFT_FLAG);

    /* Compute squared magnitede.
     * Note that this should be faster then arm_cmplx_mag_f32 as it does not compute sqrt()
     */
    arm_cmplx_mag_squared_f32(complex_fft, res, FFT_SIZE_HALF);
}
