#include "audio_visualizer.h"
#include "arm_math.h"

/* Maps value from input range to output range
 * Note that this function will saturate input value that is outside on input range
 */
static int32_t map(float val, float in_min, float in_max, float out_min, float out_max);

/* TODO: Make visualization better, add more functions for different visualizations */
void visualize_fft(const float* fft_res, size_t fft_size)
{
    /* These values are taken from my observations
     *so i wouldn't rely on them very much
     */
    const float low_frequency_threshold = 0.00007;
    const float medium_frequency_threshold = 0.00002;
    const float high_frequency_threshold = 0.00002;

    float low_mean;
    float medium_mean;
    float high_mean;

    /* Find mean value for low, medium and hight frequencies */
    arm_mean_f32(&fft_res[fft_size / 3 * 0], fft_size / 3, &low_mean);
    arm_mean_f32(&fft_res[fft_size / 3 * 1], fft_size / 3, &medium_mean);
    arm_mean_f32(&fft_res[fft_size / 3 * 2], fft_size / 3, &high_mean);

    /* Map mean value to LED color */
    uint8_t r = map(low_mean, 0, low_frequency_threshold, 0, 128);
    uint8_t g = map(medium_mean, 0, medium_frequency_threshold, 0, 128);
    uint8_t b = map(high_mean, 0, high_frequency_threshold, 0, 128);

    /* Set LEDs */
    ws2812_set_all_leds(r, g, b);

    /* Update LEDs */
    ws2812_update();
}

static int32_t map(float val, float in_min, float in_max, float out_min, float out_max)
{
    if(val > in_max)
    {
        val = in_max;
    }
    else if (val < in_min){
        val = in_min;
    }

    float in_range = in_max - in_min;
    float out_range = out_max - out_min;
    float in_out_ratio = out_range / in_range;
    return (((val - in_min) * in_out_ratio) + out_min);
}
