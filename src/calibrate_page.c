#include <pebble.h>
#include "calibrate_page.h"

	
static Window *calibrate_window = NULL;
static GRect window_frame;
static Layer *text_layer;
static Layer *graph_layer;
static Layer *icon_layer;

static int16_t weight;
Measurement calibrations[MAX_CALIBRATIONS];
int16_t calibrations_count;
float beta[3] = { -250, -250, 1000 };

static const int32_t storage_calibrations_count = 0xAFFFF + 10;
static const int32_t storage_calibrations = 0xAFFFF + 11;

// calibrations number is modified by index in graph_layer_update, needs to be changed when changing text
static char *text_calibrate_initial = "\
1. Select weight (+ / -)\n\
2. Start calibration with middle button\n\
At least 3 more values needed...\n\n\
Long-press middle to delete all values.";


void calibrate_handle_measure(kiss_fft_scalar *data, uint32_t num_samples, kiss_fft_scalar offset, Measurement m) {
	m.weight = weight;
	calibrations[calibrations_count] = m;
	
	layer_mark_dirty(text_layer);
	layer_mark_dirty(graph_layer);
}

void calibrate_handle_final(Measurement m) {
	// store the measurement
	// check if same weight already exists and average both if it does
	Measurement *mf = NULL;
	for (int i = 0; i < calibrations_count; i++) {
		if (calibrations[i].weight != weight)
			continue;
		mf = &calibrations[i];
		mf->amp = (mf->amp + m.amp) / 2;
		mf->freq = (mf->freq + m.freq) / 2;
		mf->confidence = (mf->confidence + m.confidence) / 2;
		break;
	}
	if (mf == NULL) {
		m.weight = weight;
		calibrations[calibrations_count++] = m;
	}
	// store
	calibrations_save();
	
	// stop measuring and update layers
	stop_measure();
	layer_mark_dirty(text_layer);
	layer_mark_dirty(graph_layer);

	// vibrate to let the user know
	vibes_short_pulse();
}

static void text_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_fill_color(ctx, GColorWhite);
	char str[16];
	snprintf(str, sizeof(str), "%dg", weight);
	graphics_draw_text(ctx, str, font_medium, frame, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

	if (calibrations_count >= 3) {
		if (is_measuring()) {
			Measurement m = calibrations[calibrations_count];
			floatStr(str, m.amp, 2);
			int len = strlen(str);
			str[len++] = '-';
			floatStr(str + len, m.freq, 2);
		} else {
			Measurement *m;
			float maxWeight = 1, maxFreq = 0.01, maxAmp = 0.01;
			for (int i = 0; i < calibrations_count; i++) {
				m = &calibrations[i];
				if (m->weight > maxWeight)
					maxWeight = m->weight;
				if (m->freq > maxFreq)
					maxFreq = m->freq;
				if (m->amp > maxAmp)
					maxAmp = m->amp;
			}

			floatStr(str, maxFreq, 2);
			int len = strlen(str);
			str[len++] = ' ';
			str[len++] = '/';
			str[len++] = ' ';
			floatStr(str + len, maxAmp, 2);
		}
		graphics_draw_text(ctx, str, font_tiny, GRect(0, frame.size.h - 15, frame.size.w, 15), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
	}
}
static void graph_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_fill_color(ctx, GColorWhite);
	if (!is_measuring() && calibrations_count < 3) {
		text_calibrate_initial[74] = '0' + (3 - calibrations_count);
		graphics_draw_text(ctx, text_calibrate_initial, font_tiny, GRect(3, 0, frame.size.w - 3, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		return;
	}
	
	// graph area boundaries
	graphics_fill_rect(ctx, GRect(0, 0, frame.size.w, 1), 0, GCornerNone);
	graphics_fill_rect(ctx, GRect(frame.size.w - 1, 0, 1, frame.size.h), 0, GCornerNone);

	int count = calibrations_count;
	if (is_measuring())
		count++;

	Measurement *m;
	float maxWeight = 1, minFreq = calibrations[0].freq, maxFreq = 0.01, minAmp = calibrations[0].amp, maxAmp = 0.01;
	for (int i = 0; i < count; i++) {
		m = &calibrations[i];
		if (m->weight > maxWeight)
			maxWeight = m->weight;
		if (m->freq < minFreq) minFreq = m->freq;
		if (m->freq > maxFreq) maxFreq = m->freq;
		if (m->amp < minAmp) minAmp = m->amp;
		if (m->amp > maxAmp) maxAmp = m->amp;
	}
	// draw current measure circle
	graphics_context_set_stroke_color(ctx, GColorWhite);
	for (int i = 0; i < count; i++) {
		m = &calibrations[i];
		uint16_t r = 5 * m->weight / maxWeight;
		if (r < 1) r = 1;
		int16_t x = (5 + (m->freq - minFreq) * (frame.size.w - 10) / (maxFreq - minFreq));
		int16_t y = (frame.size.h - 5 - (m->amp - minAmp) * (frame.size.h - 10) / (maxAmp - minAmp));
		if (i == calibrations_count)
			graphics_draw_circle(ctx, GPoint(x, y), r);
		else
			graphics_fill_circle(ctx, GPoint(x, y), r);
	}

	// draw lines of constant weight for steps of 50g
	if (calibrations_count >= 3) {
		char str[16];
		float w = -50;
		do {
			w += 50;
			// invert weight = beta[0] * amp + beta[1] * freq + beta[2] for constant weight
			// try top area first, if frequency is outside range then calculate amp instead
			float topAmp = maxAmp;
			float topFreq = (w - beta[0] * maxAmp - beta[2]) / beta[1];
			if (topFreq < minFreq || topFreq > maxFreq) {
				if (topFreq < minFreq) topFreq = minFreq;
				else topFreq = maxFreq;
				topAmp = (w - beta[1] * topFreq - beta[2]) / beta[0];
			}
			if (topAmp < minAmp)
				break;
			if (topFreq > maxFreq) 
				continue;
			GPoint p1 = GPoint(5 + (topFreq - minFreq) * (frame.size.w - 10) / (maxFreq - minFreq), frame.size.h - 5 - (topAmp - minAmp) * (frame.size.h - 10) / (maxAmp - minAmp));
			// try bottom area, if frequency is outside range then calculate amp instead
			float bottomAmp = minAmp;
			float bottomFreq = (w - beta[0] * minAmp - beta[2]) / beta[1];
			if (bottomFreq < minFreq || bottomFreq > maxFreq) {
				if (bottomFreq > maxFreq) bottomFreq = maxFreq;
				else bottomFreq = minFreq;
				bottomAmp = (w - beta[1] * bottomFreq - beta[2]) / beta[0];
			}
			if (bottomFreq < minFreq)
				break;
			if (bottomAmp > maxAmp)
				continue;
			GPoint p2 = GPoint(5 + (bottomFreq - minFreq) * (frame.size.w - 10) / (maxFreq - minFreq), frame.size.h - 5 - (bottomAmp - minAmp) * (frame.size.h - 10) / (maxAmp - minAmp));
			draw_line(ctx, p1, p2, 1, 2);
			// draw weight over line
			center_text_point(ctx, floatStr(str, w, 0), font_tiny, GPoint((p1.x + p2.x) / 2, (p1.y + p2.y) / 2));
		} while (w <= 1000);
	}
}
static void icon_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_fill_color(ctx, GColorWhite);
	// up
	graphics_draw_text(ctx, icon_plus, font_symbols, GRect(0, 0, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	// select
	const char *cText = icon_clock;
	if (is_measuring())
		cText = icon_stop;
	graphics_draw_text(ctx, cText, font_symbols, GRect(0, frame.size.h / 2 - 18, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	// down
	graphics_draw_text(ctx, icon_minus, font_symbols, GRect(0, frame.size.h - 30, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

/**
	Click handler
**/

static void calibrate_click_handler_select(ClickRecognizerRef recognizer, void *context) {
	if (is_measuring())
		stop_measure();
	else
		start_measure((MeasureHandler) calibrate_handle_measure, (FinalMeasureHandler) calibrate_handle_final);
	layer_mark_dirty(icon_layer);
}
static void calibrate_click_handler_updown(ClickRecognizerRef recognizer, void *context) {
	// cannot change weight while measuring
	if (is_measuring())
		return;
	// change weight up or down
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_UP:
			weight += 10;
			layer_mark_dirty(text_layer);
			break;
		case BUTTON_ID_DOWN:
			if (weight >= 10) {
				weight -= 10;
				layer_mark_dirty(text_layer);
			} else if (weight > 0) {
				weight = 0;
				layer_mark_dirty(text_layer);
			}
			break;
		default:
			break;
	}
}
static void calibrate_click_handler_long_select(ClickRecognizerRef recognizer, void *context) {
	if (is_measuring())
		return;
	weight = 0;
	calibrations_count = 0;
	calibrations_save();
	layer_mark_dirty(text_layer);
	layer_mark_dirty(graph_layer);
}
static void calibrate_click_handler_long_select_release(ClickRecognizerRef recognizer, void *context) {
}
static void calibrate_click_config(Window *window) {
	window_single_click_subscribe(BUTTON_ID_SELECT, calibrate_click_handler_select);
	window_long_click_subscribe(BUTTON_ID_SELECT, 1000, calibrate_click_handler_long_select, calibrate_click_handler_long_select_release);
	window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, calibrate_click_handler_updown);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, calibrate_click_handler_updown);
}


/**
	Window setup and teardown
**/

static void calibrate_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);
	window_set_click_config_provider(window, (ClickConfigProvider) calibrate_click_config);

	weight = 0;
	
	// init display stuff
	int w = window_frame.size.w - 20;
  text_layer = layer_create(GRect(3, 0, w - 3, window_frame.size.h - w));
  layer_set_update_proc(text_layer, text_layer_update_callback);
  layer_add_child(window_layer, text_layer);

	graph_layer = layer_create(GRect(0, window_frame.size.h - w, w, w));
  layer_set_update_proc(graph_layer, graph_layer_update_callback);
  layer_add_child(window_layer, graph_layer);

  icon_layer = layer_create(GRect(w, 0, window_frame.size.w - w, window_frame.size.h));
  layer_set_update_proc(icon_layer, icon_layer_update_callback);
  layer_add_child(window_layer, icon_layer);
}

static void calibrate_window_unload(Window *window) {
	stop_measure();
	layer_destroy(text_layer);
	layer_destroy(graph_layer);
	layer_destroy(icon_layer);
}


void calibrate_page_open() {
	if (calibrate_window == NULL) {
		calibrate_window = window_create();
		window_set_window_handlers(calibrate_window, (WindowHandlers) {
			.load = calibrate_window_load,
			.unload = calibrate_window_unload
		});
		window_set_background_color(calibrate_window, GColorBlack);
	}
	window_stack_push(calibrate_window, true);
}

void calibrate_page_close() {
	if (calibrate_window == NULL)
		return;
	window_stack_remove(calibrate_window, true);
	window_destroy(calibrate_window);
	calibrate_window = NULL;
}

void fit_data() {
	// do a linear fit of calibration data to weights:
	// A*amp + F*freq = weight
	// find coefficients with least error (gauss-newton)
	// need at least two points to get a calibration
	if (calibrations_count < 3) {
		// reset beta as well
		beta[0] = -250;
		beta[1] = -250;
		beta[2] = 1000;
		return;
	}

	float maxAmp = 0.01, maxFreq = 0.01;
	
	// calculate components of JT*J and invert afterwards
	float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, i = 0;
	for (int j = 0; j < calibrations_count; j++) {
		Measurement *m = &calibrations[j];
		a += m->amp * m->amp;
		b += m->amp * m->freq;
		c += m->amp;
		d += m->freq * m->amp;
		e += m->freq * m->freq;
		f += m->freq;
		g += m->amp;
		h += m->freq;
		i += 1;
		
		if (m->amp > maxAmp) maxAmp = m->amp;
		if (m->freq > maxFreq) maxFreq = m->freq;
	}
	float det = a * e * i + b * f * g + c * d * h - c * e * g - b * d * i - a * f * h;
	float inv[9] = { 
		(e * i - f * h) / det,
		-(b * i - c * h) / det,
		(b * f - c * e) / det,
		-(d * i - f * g) / det,
		(a * i - c * g) / det,
		-(a * f - c * d) / det,
		(d * h - e * g) / det,
		-(a * h - b * g) / det,
		(a * e - b * d) / det
	};
	
	// start with initial parameter guess and iterate a few times
	char str1[8], str2[8], str3[8], str4[8];
	APP_LOG(APP_LOG_LEVEL_DEBUG, "A: %s, F: %s, W: %s", floatStr(str1, beta[0], 2), floatStr(str2, beta[1], 2), floatStr(str3, beta[2], 2));
	for (int k = 0; k < 5; k++) {
		// calculate JT*r(ß)
		float A = 0, F = 0, W = 0;
		for (int j = 0; j < calibrations_count; j++) {
			Measurement *m = &calibrations[j];
			float rj = m->weight - beta[0] * m->amp - beta[1] * m->freq - beta[2];
			A -= m->amp * rj;
			F -= m->freq * rj;
			W -= rj;
		}
		// multiply with inverse and subtract from old ß guess to get new guess
		beta[0] -= A * inv[0] + F * inv[1] + W * inv[2];
		beta[1] -= A * inv[3] + F * inv[4] + W * inv[5];
		beta[2] -= A * inv[6] + F * inv[7] + W * inv[8];
		APP_LOG(APP_LOG_LEVEL_DEBUG, "A: %s, F: %s, W: %s", floatStr(str1, beta[0], 2), floatStr(str2, beta[1], 2), floatStr(str3, beta[2], 2));
	}

	APP_LOG(APP_LOG_LEVEL_DEBUG, "w(0,0): %s, w(0,maxF): %s, w(maxA,0): %s, w(maxA,maxF): %s", floatStr(str1, beta[2], 2), floatStr(str2, beta[1]*maxFreq+beta[2], 2), floatStr(str3, beta[0]*maxAmp+beta[2], 2), floatStr(str4, beta[0]*maxAmp+beta[1]*maxFreq+beta[2], 2));
}

void calibrations_save() {
	persist_write_int(storage_calibrations_count, calibrations_count);
	if (calibrations_count > 0) {
		persist_write_data(storage_calibrations, calibrations, sizeof(Measurement) * calibrations_count);
	}
	fit_data();
}
void calibrations_load() {
	calibrations_count = 0;
	if (!persist_exists(storage_calibrations_count))
		return;
	calibrations_count = persist_read_int(storage_calibrations_count);
	persist_read_data(storage_calibrations, calibrations, sizeof(calibrations));
	fit_data();
}
