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

static const int32_t storage_calibrations_count = 0xAFFFF + 10;
static const int32_t storage_calibrations = 0xAFFFF + 11;


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
static void graph_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_fill_color(ctx, GColorWhite);
	
	// graph area boundaries
	graphics_fill_rect(ctx, GRect(0, 0, frame.size.w, 1), 0, GCornerNone);
	graphics_fill_rect(ctx, GRect(frame.size.w - 1, 0, 1, frame.size.h), 0, GCornerNone);

	int count = calibrations_count;
	if (is_measuring())
		count++;

	Measurement *m;
	float maxWeight = 1, maxFreq = 0.01, maxAmp = 0.01;
	for (int i = 0; i < count; i++) {
		m = &calibrations[i];
		if (m->weight > maxWeight)
			maxWeight = m->weight;
		if (m->freq > maxFreq)
			maxFreq = m->freq;
		if (m->amp > maxAmp)
			maxAmp = m->amp;
	}
//APP_LOG(APP_LOG_LEVEL_DEBUG, "%d, %d", (int) maxWeight, (int)(maxFreq * 100));
	// draw current measure circle
	graphics_context_set_stroke_color(ctx, GColorWhite);
	for (int i = 0; i < count; i++) {
		m = &calibrations[i];
		uint16_t r = 5 * m->weight / maxWeight;
		if (r < 1) r = 1;
		int16_t x = (3 + m->freq * (frame.size.w - 6) / maxFreq);
		int16_t y = (frame.size.h - 3 - m->amp * (frame.size.h - 6) / maxAmp);
		if (i == calibrations_count)
			graphics_draw_circle(ctx, GPoint(x, y), r);
		else
			graphics_fill_circle(ctx, GPoint(x, y), r);
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
	graphics_draw_text(ctx, cText, font_symbols, GRect(0, frame.size.h / 2 - 15, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
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
static void calibrate_click_config(Window *window) {
	window_single_click_subscribe(BUTTON_ID_SELECT, calibrate_click_handler_select);
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
  text_layer = layer_create(GRect(0, 0, w, window_frame.size.h - w));
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
		//window_set_fullscreen(calibrate_window, true);
		window_set_window_handlers(calibrate_window, (WindowHandlers) {
			.load = calibrate_window_load,
			.unload = calibrate_window_unload
		});
	}
	window_stack_push(calibrate_window, true);
  window_set_background_color(calibrate_window, GColorBlack);
}

void calibrate_page_close() {
	if (calibrate_window == NULL)
		return;
	window_destroy(calibrate_window);
	calibrate_window = NULL;
}

void calibrations_save() {
	status_t s = persist_write_int(storage_calibrations_count, calibrations_count);
	if (calibrations_count > 0) {
		int w = persist_write_data(storage_calibrations, calibrations, sizeof(Measurement) * calibrations_count);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "written: %d, %d", w, (int) s);
	}
}
void calibrations_load() {
	calibrations_count = 0;
	if (!persist_exists(storage_calibrations_count))
		return;
	calibrations_count = persist_read_int(storage_calibrations_count);
	int w = persist_read_data(storage_calibrations, calibrations, sizeof(calibrations));
	APP_LOG(APP_LOG_LEVEL_DEBUG, "read: %d", w);
}
