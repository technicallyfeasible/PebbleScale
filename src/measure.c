#include <pebble.h>
#include "main.h"
#include "measure.h"

#define SAMPLE_RATE ACCEL_SAMPLING_100HZ
#define NUM_POINTS (2*SAMPLE_RATE)
#define MAX_VALUE 4500

static bool measure_running;
static int next_draw;

static kiss_fft_scalar fft_zero;
static kiss_fftr_cfg fft_cfg;
static kiss_fft_scalar fft_in[NUM_POINTS];
static kiss_fft_cpx fft_out[NUM_POINTS];

static MeasureHandler callback = NULL;
static FinalMeasureHandler final_callback = NULL;

static Measurement avg_m;
static int16_t avg_m_count;
static float lastAvgF;

	
static void do_measure() {
	// do fft
	kiss_fftr(fft_cfg, (kiss_fft_scalar*) fft_in, fft_out);
	
	kiss_fft_scalar offset = fft_out[0].r;
		
	// get scale
	int maxF = 0, avg = 0;
	kiss_fft_scalar max = 0, pt;
	for (int i = 0; i < (NUM_POINTS / 2); i++) {
		pt = fft_out[i].r;
		if (pt < 0) {
			pt = -pt;
			fft_out[i].r = pt;
		}
		if (i == 0) continue;
		avg += pt;
		if (pt > max) {
			max = pt;
			maxF = i;
		}
	}
	avg /= (NUM_POINTS/2 - 1);
	
	// adjust the maximum to the best value within a range of +- 3
	int mini = maxF - 2, maxi = maxF + 2;
	if (mini < 1) mini = 1;
	if (maxi >= NUM_POINTS / 2) maxi = NUM_POINTS/2 - 1;
	float sum = 0, avgF = 0;
	for (int i = mini; i < maxi; i++) {
		sum += (float) fft_out[i].r;
		avgF += i * (float) fft_out[i].r;
	}
	avgF /= sum;
	float outerSum = 0;
	for (int i = 1; i < mini; i++)
		outerSum += (float) fft_out[i].r;
	for (int i = maxi + 1; i < NUM_POINTS / 2; i++)
		outerSum += (float) fft_out[i].r;

	// frequency is: (sampling_rate/2) * maxF / NUM_POINTS
	float freq = (float)(SAMPLE_RATE * avgF) / (2 * NUM_POINTS);
	float amp = (float) max * (MAX_VALUE / (1000.0 * SAMP_MAX));
	float confidence = sum / outerSum;
	char str[16], str2[16], str3[16];
	floatStr(str, confidence, 2);
	floatStr(str2, amp, 2);
	floatStr(str3, freq, 2);
//APP_LOG(APP_LOG_LEVEL_DEBUG, "C: %s, A: %s, F: %s", str, str2, str3);
	if (callback != NULL) {
		callback(fft_in, NUM_POINTS, offset, Measurement(confidence, freq, amp));
	}
	
	// if a final value is needed then accumulate
	if (final_callback != NULL) {
		// keep measuring while confidence > 1
		if (confidence < 0.5) {
APP_LOG(APP_LOG_LEVEL_DEBUG, "reset: confidence");
			// reset all values
			avg_m_count = 0;
		} else {
			// add another value if freq and amp stayed within 10%
			float df = abs(avgF - lastAvgF);
			//float da = abs(amp - avg_m.amp);
			if (avg_m_count == 0) {
				avg_m.freq = 0;
				avg_m.amp  = 0;
			} else if (df > 1) {
APP_LOG(APP_LOG_LEVEL_DEBUG, "reset: amp/freq");
				avg_m_count = 0;
				avg_m.freq = 0;
				avg_m.amp  = 0;
				;
			}
			lastAvgF = avgF;
			avg_m_count++;
			avg_m.freq += freq;
			avg_m.amp += amp;

			char stra[16], stra2[16], stra3[16];
APP_LOG(APP_LOG_LEVEL_DEBUG, "%s: C: %d, A: %s, F: %s, F: %s", str, avg_m_count, floatStr(stra, avg_m.amp, 2), floatStr(stra2, avg_m.freq / avg_m_count, 2), floatStr(stra3, freq, 2));

			// if we have 3 good values then invoke the callback
			if (avg_m_count >= 3) {
				avg_m.freq /= avg_m_count;
				avg_m.amp /= avg_m_count;
				avg_m_count = 0;
				final_callback(avg_m);
			}
		}
	}
}


static void accel_callback(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
	// move graph forward by number of samples
	memcpy(fft_in, &fft_in[num_samples], (NUM_POINTS - num_samples) * sizeof(kiss_fft_scalar));
	// add new measurements scaled into fft range
	int j = 0;
	for (uint i = NUM_POINTS - num_samples; i < NUM_POINTS; i++, j++) {
		int32_t val = (((int32_t) data[j].z * SAMP_MAX / MAX_VALUE));
		if (val > SAMP_MAX) val = SAMP_MAX;
		if (val < -SAMP_MAX) val = -SAMP_MAX;
		fft_in[i] = val;
	}
	if (next_draw++ >= 4) {
		next_draw = 0;
		do_measure();
	}
}

bool is_measuring() {
	return measure_running;
}

void start_measure(MeasureHandler measureHandler, FinalMeasureHandler finalHandler) {
	callback = measureHandler;
	final_callback = finalHandler;
	avg_m_count = 0;
	if (measure_running)
		return;
	measure_running = true;
  accel_raw_data_service_subscribe(25, (AccelRawDataHandler) accel_callback);
  accel_service_set_sampling_rate(SAMPLE_RATE);
}
void stop_measure() {
	callback = NULL;
	if (!measure_running)
		return;
	measure_running = false;
  accel_data_service_unsubscribe();
}

void init_measure() {
	// init FFT stuff
	memset(&fft_zero, 0, sizeof(fft_zero));
	fft_cfg = kiss_fftr_alloc(NUM_POINTS, 0, 0, 0);
}
void clean_measure() {
	stop_measure();
	free(fft_cfg);
	kiss_fft_cleanup();
}
