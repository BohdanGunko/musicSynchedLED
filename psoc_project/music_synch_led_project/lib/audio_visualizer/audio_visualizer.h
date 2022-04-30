#ifndef __AUDIO_VISUALIZER_H__
#define __AUDIO_VISUALIZER_H__

#include "ws2812.h"

void visualize_fft(const float* fft_res, size_t fft_size);

#endif /* __AUDIO_VISUALIZER_H__ */
