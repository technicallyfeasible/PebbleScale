#include <pebble.h>
#include "help_page.h"

	
static Window *help_window = NULL;
static GRect window_frame;
static ScrollLayer *scroll_layer;
static TextLayer *text_layer;
static const char *help_text;
static ClickHandler help_select_callback;

/**
	Click handler
**/

static void help_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (help_select_callback != NULL)
		help_select_callback(recognizer, context);
}
static void help_click_config(Window *window) {
	window_single_click_subscribe(BUTTON_ID_SELECT, help_click_handler);
}


/**
	Window setup and teardown
**/

static int16_t max(int16_t a, int16_t b) {
	if (a >= b)
		return a;
	return b;
}
static void help_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);

	// init layers
  scroll_layer = scroll_layer_create(window_frame);
  layer_add_child(window_layer, scroll_layer_get_layer(scroll_layer));
  text_layer = text_layer_create(GRect(5, 3, window_frame.size.w - 10, window_frame.size.h * 10));
  scroll_layer_add_child(scroll_layer, text_layer_get_layer(text_layer));
	text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text(text_layer, help_text);
	GSize size = text_layer_get_content_size(text_layer);
	size.h = max(window_frame.size.h, size.h + 10);
	text_layer_set_size(text_layer, size);
	scroll_layer_set_content_size(scroll_layer, size);
	scroll_layer_set_click_config_onto_window(scroll_layer, window);
	scroll_layer_set_callbacks(scroll_layer, (ScrollLayerCallbacks) { (ClickConfigProvider) help_click_config, NULL });
}

static void help_window_unload(Window *window) {
	text_layer_destroy(text_layer);
	scroll_layer_destroy(scroll_layer);
}


void help_page_open(const char *text, ClickHandler select_callback) {
	help_text = text;
	help_select_callback = select_callback;
	if (help_window == NULL) {
		help_window = window_create();
		window_set_window_handlers(help_window, (WindowHandlers) {
			.load = help_window_load,
			.unload = help_window_unload
		});
	}
	window_stack_push(help_window, true);
}

void help_page_close() {
	if (help_window == NULL)
		return;
	window_stack_remove(help_window, true);
	window_destroy(help_window);
	help_window = NULL;
}
