#include "pebble.h"
#include "main.h"
#include "measure.h"
#include "calibrate_page.h"
#include "help_page.h"


#define GRAPH_HEIGHT	30

static Window *window;
static GRect window_frame;
static Layer *graph_layer;
static Layer *icon_layer;

GFont font_huge, font_large, font_medium, font_tiny, font_symbols, font_symbols_small;

static kiss_fft_scalar *cur_data;
static uint32_t cur_num_samples;
static kiss_fft_scalar cur_offset;
static Measurement measurement;
static float final_weight = -1;

const char *icon_plus = "5";
const char *icon_minus = "7";
const char *icon_clock = "t";
const char *icon_stop = "1";
const char *icon_settings = "e";
const char *icon_check = "c";
const char *icon_question = "?";

static const char *help_text_first = "\
Welcome\n\n\
Thank you for trying Pebble Scale. This app lets you weigh objects using the accelerometer in Pebble. \
Don't expect miracles in accuracy but here is how it works:\n\n\
1. Hold the object in the hand where you wear Pebble.\n\
2. Start measuring by pressing the middle button.\n\
3. Move your hand up and down repeatedly, making sure to keep a constant effort. This means the heavier the item is, the slower your hand will move.\n\
4. Your Pebble will buzz shortly when the measurement has been taken.\n\n\
Have fun weighing things and be sure to let me know how it works for you at jens.elstner@keepzer.com!\n\n\n\
Click here ------>\n\n\n\
now to continue with calibration\n";
static const char *help_text_main = "\
Weighing\n\n\
1. Hold the object in the hand where you are wearing Pebble.\n\
2. Press the middle button to start.\n\
3. Move your hand up and down, making sure that it takes the same effort like during calibration. The better you can keep the same effort, the more accurate your measurement will be.\n\
4. Pebble will buzz shortly to let you know when a value was measured and show the weight on the screen.\n\
5. Repeat steps to measure another weight.\n";
static const char *text_main_need_calibration = "\
Not enough calibration values.\n\n\
Proceed to calibration --->";
static const char *text_main_can_measure = "Start --->";
static const char *text_main_weight_result = "%dg";
static const char *text_main_measurement_failed = "Measurement failed. You might need more calibration values.";
static const char *text_main_measure_hint = "Move hand up and down in a steady motion";


/**
	Measure handlers
**/

void handle_measure(kiss_fft_scalar *data, uint32_t num_samples, kiss_fft_scalar offset, Measurement m) {
	cur_data = data;
	cur_num_samples = num_samples;
	cur_offset = offset;
	measurement = m;
	layer_mark_dirty(graph_layer);
}
void handle_final(Measurement m) {
	// calculate weight using coefficients
	final_weight = beta[0] * m.amp + beta[1] * m.freq + beta[2];
	if (final_weight < 0)
		final_weight = -2;

	// stop measuring and update layers
	stop_measure();
	layer_mark_dirty(graph_layer);
	layer_mark_dirty(icon_layer);
	// vibrate to let the user know
	vibes_short_pulse();
}

static void graph_layer_update_callback(Layer *me, GContext *ctx) {
	GRect frame = layer_get_frame(me);
	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_text_color(ctx, GColorWhite);
	char str[24], str2[16];
	GRect text_frame = GRect(3, 0, frame.size.w - 3, frame.size.h);
	if (!is_measuring()) {
		GRect center_frame = GRect(0, 0, frame.size.w, frame.size.h - 20);
		if (calibrations_count < 3) {
			graphics_draw_text(ctx, text_main_need_calibration, font_medium, text_frame, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		} else if (final_weight >= 0) {
			snprintf(str, sizeof(str), text_main_weight_result, (int) final_weight);
			center_text(ctx, str, font_huge, center_frame);
		} else if (final_weight == -1) {
			center_text(ctx, text_main_can_measure, font_large, center_frame);
		} else if (final_weight == -2) {
			graphics_draw_text(ctx, text_main_measurement_failed, font_medium, text_frame, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		}
		return;
	}
	if (cur_data == NULL)
		return;

	// display graph
	const int16_t mid = frame.size.h - GRAPH_HEIGHT;
	const float step = (float) cur_num_samples / (float) frame.size.w;
	for (int i = 0; i < frame.size.w; i++) {
		int16_t h = (int32_t) (cur_data[(int)(i * step)] - cur_offset) * GRAPH_HEIGHT / SAMP_MAX;
		int16_t y;
		if (h >= 0)
			y = mid - h;
		else {
			y = mid;
			h = -h;
		}
		graphics_fill_rect(ctx, GRect(i, y, 1, h), 0, GCornerNone);
	}
	graphics_fill_rect(ctx, GRect(0, frame.size.h - 2 * GRAPH_HEIGHT, frame.size.w, 1), 0, GCornerNone);
	dashed_line_h(ctx, GPoint(0, frame.size.h - 1.75 * GRAPH_HEIGHT), frame.size.w, 1, 1);
	dashed_line_h(ctx, GPoint(0, frame.size.h - GRAPH_HEIGHT), frame.size.w, 2, 2);
	dashed_line_h(ctx, GPoint(0, frame.size.h - 0.25 * GRAPH_HEIGHT), frame.size.w, 1, 1);
	
	// display text
	if (measurement.confidence <= 0.2) {
		graphics_draw_text(ctx, text_main_measure_hint, font_medium, text_frame, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	} else {
		floatStr(str, measurement.freq, 2);
		floatStr(str2, measurement.amp, 2);
		graphics_draw_text(ctx, str, font_large, GRect(frame.size.w/2, 0, frame.size.w/2, 50), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
		graphics_draw_text(ctx, str2, font_large, GRect(0, 0, frame.size.w/2, 50), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	}
}

static void icon_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_text_color(ctx, GColorWhite);
	// up
	graphics_draw_text(ctx, icon_question, font_symbols_small, GRect(0, 0, frame.size.w, frame.size.w), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	// select
	if (calibrations_count >= 3) {
		const char *cText = icon_clock;
		if (is_measuring())
			cText = icon_stop;
		graphics_draw_text(ctx, cText, font_symbols, GRect(0, frame.size.h / 2 - 18, frame.size.w, frame.size.w), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	}
	// down
	graphics_draw_text(ctx, icon_settings, font_symbols, GRect(0, frame.size.h - 30, frame.size.w, frame.size.w), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void click_handler(ClickRecognizerRef recognizer, void *context) {
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_UP:
			if (is_measuring())
				stop_measure();
			help_page_open(help_text_main, NULL);
			break;
		case BUTTON_ID_DOWN:
			if (is_measuring())
				stop_measure();
			calibrate_page_open();
			break;
		case BUTTON_ID_SELECT:
			if (is_measuring())
				stop_measure();
			else if (calibrations_count >= 3)
				start_measure((MeasureHandler) handle_measure, (FinalMeasureHandler) handle_final);
			layer_mark_dirty(graph_layer);
			layer_mark_dirty(icon_layer);
			break;
		default:
			break;
	}
}
void help_handler_first_steps(ClickRecognizerRef recognizer, void *context) {
	help_page_close();
	main_page_open();
	calibrate_page_open();
}
void click_config(Window *window) {
	window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, click_handler);
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);
	window_set_click_config_provider(window, (ClickConfigProvider) click_config);

	// init display stuff
	int w = window_frame.size.w - 30;
  graph_layer = layer_create(GRect(0, 0, w, window_frame.size.h));
  layer_set_update_proc(graph_layer, graph_layer_update_callback);
  layer_add_child(window_layer, graph_layer);
  icon_layer = layer_create(GRect(w, 0, 30, window_frame.size.h));
  layer_set_update_proc(icon_layer, icon_layer_update_callback);
  layer_add_child(window_layer, icon_layer);
}

static void window_unload(Window *window) {
	layer_destroy(icon_layer);
	layer_destroy(graph_layer);
}

void main_page_open() {
	if (window == NULL) {
		window = window_create();
		window_set_window_handlers(window, (WindowHandlers) {
			.load = window_load,
			.unload = window_unload
		});
		window_set_background_color(window, GColorBlack);
	}
	window_stack_push(window, true);
}
void main_page_close() {
	if (window == NULL)
		return;
	window_stack_remove(window, true);
	window_destroy(window);
	window = NULL;
}

static void init(void) {
	font_huge = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
	font_large = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	font_medium = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	font_tiny = fonts_get_system_font(FONT_KEY_GOTHIC_14);
	font_symbols = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNICONS_28));
	font_symbols_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNICONS_18));
	
	init_measure();
	calibrations_load();

	// show first run help when no calibrations are present
	if (calibrations_count == 0) {
		help_page_open(help_text_first, (ClickHandler) help_handler_first_steps);
	} else {
		main_page_open();
	}
}

static void deinit(void) {
	fonts_unload_custom_font(font_symbols);
	fonts_unload_custom_font(font_symbols_small);
	clean_measure();
	calibrate_page_close();
	help_page_close();
	main_page_close();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
