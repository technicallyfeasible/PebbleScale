#include "pebble.h"
#include "main.h"
#include "measure.h"
#include "calibrate_page.h"
	

#define GRAPH_HEIGHT	30

static Window *window;
static GRect window_frame;
static Layer *graph_layer;
static Layer *icon_layer;

GFont font_large;
GFont font_medium;
GFont font_tiny;
GFont font_symbols;

static kiss_fft_scalar *cur_data;
static uint32_t cur_num_samples;
static kiss_fft_scalar cur_offset;
static Measurement measurement;

const char *icon_plus = "5";
const char *icon_minus = "7";
const char *icon_clock = "t";
const char *icon_stop = "1";
const char *icon_settings = "e";


void handle_measure(kiss_fft_scalar *data, uint32_t num_samples, kiss_fft_scalar offset, Measurement m) {
	cur_data = data;
	cur_num_samples = num_samples;
	cur_offset = offset;
	measurement = m;
	layer_mark_dirty(graph_layer);
}
void handle_final(Measurement m) {
	
}

static void dashed_line(GContext *ctx, GPoint p, int width, int l1, int l2) {
	int x = p.x + width;
	while (p.x < x) {
		for (int i = 0; i < l1 && p.x < x; i++, p.x++)
			graphics_draw_pixel(ctx, p);
		p.x += l2;
	}
}

static void graph_layer_update_callback(Layer *me, GContext *ctx) {
	if (!is_measuring() || cur_data == NULL)
		return;
	GRect frame = layer_get_frame(me);

	// display graph
	const int16_t mid = frame.size.h - GRAPH_HEIGHT;
	const float step = (float) cur_num_samples / (float) frame.size.w;
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorWhite);
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
	dashed_line(ctx, GPoint(0, frame.size.h - 1.75 * GRAPH_HEIGHT), frame.size.w, 1, 1);
	dashed_line(ctx, GPoint(0, frame.size.h - GRAPH_HEIGHT), frame.size.w, 2, 2);
	dashed_line(ctx, GPoint(0, frame.size.h - 0.25 * GRAPH_HEIGHT), frame.size.w, 1, 1);

	// display text
	char str[16], str2[16];
	floatStr(str, measurement.freq, 2);
	floatStr(str2, measurement.amp, 2);
	graphics_draw_text(ctx, str, font_large, GRect(frame.size.w/2, 0, frame.size.w/2, 50), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
	graphics_draw_text(ctx, str2, font_large, GRect(0, 0, frame.size.w/2, 50), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void icon_layer_update_callback(Layer *me, GContext *ctx) {
	const GRect frame = layer_get_frame(me);
	graphics_context_set_fill_color(ctx, GColorWhite);
	// up
	//graphics_draw_text(ctx, "5", font_symbols, GRect(0, 0, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	// select
	const char *cText = icon_clock;
	if (is_measuring())
		cText = icon_stop;
	graphics_draw_text(ctx, cText, font_symbols, GRect(0, frame.size.h / 2 - 15, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	// down
	graphics_draw_text(ctx, icon_settings, font_symbols, GRect(0, frame.size.h - 30, frame.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void click_handler_select(ClickRecognizerRef recognizer, void *context) {
	if (is_measuring())
		stop_measure();
	else
		start_measure((MeasureHandler) handle_measure, (FinalMeasureHandler) handle_final);
	layer_mark_dirty(icon_layer);
}
void click_handler_down(ClickRecognizerRef recognizer, void *context) {
	if (is_measuring())
		stop_measure();
	calibrate_page_open();
}

void click_config(Window *window) {
	window_single_click_subscribe(BUTTON_ID_SELECT, click_handler_select);
	window_single_click_subscribe(BUTTON_ID_DOWN, click_handler_down);
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);
	window_set_click_config_provider(window, (ClickConfigProvider) click_config);

	// init display stuff
	int w = window_frame.size.w - 20;
  graph_layer = layer_create(GRect(0, 0, w, window_frame.size.h));
  layer_set_update_proc(graph_layer, graph_layer_update_callback);
  layer_add_child(window_layer, graph_layer);
  icon_layer = layer_create(GRect(w, 0, 20, window_frame.size.h));
  layer_set_update_proc(icon_layer, icon_layer_update_callback);
  layer_add_child(window_layer, icon_layer);
}

static void window_unload(Window *window) {
	layer_destroy(icon_layer);
	layer_destroy(graph_layer);
}

static void init(void) {
	font_large = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	font_medium = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	font_tiny = fonts_get_system_font(FONT_KEY_GOTHIC_14);
	font_symbols = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNICONS_28));
	
	init_measure();
	calibrations_load();

	window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);
}

static void deinit(void) {
	fonts_unload_custom_font(font_symbols);
	clean_measure();
	calibrate_page_close();
	window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
