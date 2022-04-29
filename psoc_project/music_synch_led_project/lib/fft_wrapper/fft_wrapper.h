#ifndef __FFT_WRAPPER_H__
#define __FFT_WRAPPER_H__

#include "arm_math.h"
/* TODO: check if this include is needed */
#include "arm_const_structs.h"
#include "app_config.h"
#include "cyhal.h"

arm_status rfft_init();
void compute_rfft(float* input, float* res);
arm_status measure_fft_performance(cyhal_timer_t* timer_obj);

#endif /* __FFT_WRAPPER_H__ */
