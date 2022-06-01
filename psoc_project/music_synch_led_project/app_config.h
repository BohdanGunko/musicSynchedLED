#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/* Number of LEDs */
#define WS2812_LEDS_COUNT   (30 * 6)

/* Pin to which ws2812 data line is connected */
#define WS2812_LEDS_PIN     (CYBSP_A0)

/* FFT_SIZE Must be a power of 2 in range from 16 to 4096 */
#define FFT_SIZE            (1024)
#define FFT_SIZE_HALF       (FFT_SIZE / 2)

/* Minimum and maximum supported FFT length */
#define MIN_SUPPORTED_FFT_SIZE  (32)
#define MAX_SUPPORTED_FFT_SIZE  (4096)

/* Pin to sample audio signal from */
#define AUDIO_SAMPLING_PIN  (CYBSP_A1)

/* Sample rate for ADC in Hz */
#define AUDIO_SAMPLING_RATE (44100)

/* Whether to measure performance */
#define MEASURE_PERFORMANCE (1)

#endif /* __APP_CONFIG_H__ */
