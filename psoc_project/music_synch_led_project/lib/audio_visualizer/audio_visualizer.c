#include "audio_visualizer.h"
#include "arm_math.h"

/* Buffer for LEDs. Used by visualization functions that
 * need to keep track of leds state */
static led_color_t leds[WS2812_LEDS_COUNT];

/* Maps value from input range to output range
 * Note that this function will saturate input value that is outside on input range */
static int32_t map(float val, float in_min, float in_max, float out_min, float out_max);
static led_color_t fft_to_fgb(const float* fft_res, size_t fft_size);

static void visualize_mode_map_rgb(const float* fft_res, size_t fft_size);
static void visualize_mode_snake_flow(const float* fft_res, size_t fft_size);
static void visualize_mode_snake_flow_bidirectional(const float* fft_res, size_t fft_size);

/* TODO: Make visualization better, add more functions for different visualizations */
void visualize_fft(const float* fft_res, size_t fft_size, visualization_mode_t visualization_mode)
{
    switch (visualization_mode)
    {
    case VISUALIZATION_MODE_MAP_RGB:
        visualize_mode_map_rgb(fft_res, fft_size);
        break;
    case VISUALIZATION_MODE_SNAKE_FLOW:
        visualize_mode_snake_flow(fft_res, fft_size);
        break;
    case VISUALIZATION_MODE_SNAKE_FLOW_BIDIRECTIONAL:
        visualize_mode_snake_flow_bidirectional(fft_res, fft_size);
        break;
    default:
        /* Endless loop to be safe */
        while (1) {}
        break;
    }
}

static void visualize_mode_map_rgb(const float* fft_res, size_t fft_size)
{
    led_color_t led_color;

    /* Get LEDs colour value */
    led_color = fft_to_fgb(fft_res, fft_size);

    /* Set LEDs */
    ws2812_set_all_leds(led_color.r, led_color.g, led_color.b);

    /* Update LEDs */
    ws2812_update();
}

static void visualize_mode_snake_flow(const float* fft_res, size_t fft_size)
{
    led_color_t led_color;

    /* Get LEDs colour value */
    led_color = fft_to_fgb(fft_res, fft_size);

    /* Shift LEDs */
    for (size_t i = WS2812_LEDS_COUNT - 1; i > 0; i--)
    {
        leds[i] = leds[i - 1];
        ws2812_set_led(i, leds[i].r, leds[i].g, leds[i].b);
    }

    /* Update RGB values of the first LED */
    leds[0] = led_color;
    ws2812_set_led(0, leds[0].r, leds[0].g, leds[0].b);

    /* Update LEDs */
    ws2812_update();
}

static void visualize_mode_snake_flow_bidirectional(const float* fft_res, size_t fft_size)
{
    led_color_t led_color;

    /* Get LEDs colour value */
    led_color = fft_to_fgb(fft_res, fft_size);

    /* If number of LEDs is even then we need to subtract 1
     * to have midpoint with equal number of leds on both sides.
     * In this case one LED is unused but this simplifies the
     * rest of the code. */
#if (WS2812_LEDS_COUNT % 2) == 0
    uint32_t half_leds = (WS2812_LEDS_COUNT - 1) / 2;
#else
    uint32_t half_leds = WS2812_LEDS_COUNT / 2;
#endif

    /* Shift LEDs */
    for (size_t i = half_leds; i >= 1; i--)
    {
        leds[half_leds + i] = leds[half_leds + i - 1];
        leds[half_leds - i] = leds[half_leds - i + 1];
    }

    /* Update RGB values of the first LED */
    leds[half_leds] = led_color;

    /* Set value for each LED */
    for (size_t i = 0; i < WS2812_LEDS_COUNT; i++)
    {
        ws2812_set_led(i, leds[i].r, leds[i].g, leds[i].b);
    }

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

static led_color_t fft_to_fgb(const float* fft_res, size_t fft_size)
{
    /* These values are taken from my observations
     * so i wouldn't rely on them very much */
    const float low_frequency_threshold = 0.00007;
    const float medium_frequency_threshold = 0.00002;
    const float high_frequency_threshold = 0.00002;

    float low_mean;
    float medium_mean;
    float high_mean;

    led_color_t res;

    /* Find mean value for low, medium and hight frequencies */
    arm_mean_f32(&fft_res[fft_size / 3 * 0], fft_size / 3, &low_mean);
    arm_mean_f32(&fft_res[fft_size / 3 * 1], fft_size / 3, &medium_mean);
    arm_mean_f32(&fft_res[fft_size / 3 * 2], fft_size / 3, &high_mean);

    /* Map mean value to LED color */
    res.r = map(low_mean, 0, low_frequency_threshold, 0, 255);
    res.g = map(medium_mean, 0, medium_frequency_threshold, 0, 255);
    res.b = map(high_mean, 0, high_frequency_threshold, 0, 255);

    return res;
}
