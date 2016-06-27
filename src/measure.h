#pragma once
#include "_kiss_fft_guts.h"
#include "kiss_fftr.h"

#pragma pack(push, 4)
typedef struct {
  float weight;
  float freq;
  float amp;
	float confidence;
} Measurement;
#pragma pack(pop)
#define Measurement(c, f, a) ((Measurement){(0), (f), (a), (c)})

typedef void (*MeasureHandler)(kiss_fft_scalar *data, uint32_t num_samples, kiss_fft_scalar offset, Measurement measurement);
typedef void (*FinalMeasureHandler)(Measurement measurement);

bool is_measuring();

void init_measure();
void clean_measure();

void start_measure(MeasureHandler measureHandler, FinalMeasureHandler finalHandler);
void stop_measure();
