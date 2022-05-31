#ifndef __AUDIO_VISUALIZER_H__
#define __AUDIO_VISUALIZER_H__

#include "ws2812.h"

typedef enum {
    VISUALIZATION_MODE_MAP_RGB,
    VISUALIZATION_MODE_SNAKE_FLOW,
    VISUALIZATION_MODE_SNAKE_FLOW_BIDIRECTIONAL,
    VISUALIZATION_MODE_MAX
} visualization_mode_t;

void visualize_fft(const float* fft_res, size_t fft_size, visualization_mode_t visualization_mode);

#endif /* __AUDIO_VISUALIZER_H__ */
