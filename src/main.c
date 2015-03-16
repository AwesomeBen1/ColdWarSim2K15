// Made by Ben Chapman-Kish up to and on 2015-03-16
#include "pebble.h"
// The possibilities of segfaults, memory overflows, and memory leaks are features, by the way

/*#include "time.h"
#include "stdlib.h"
#include "unistd.h" ... uint sleep(1);*/

typedef struct {
 char *action;
 char *description;
} ColdWarOption;

ColdWarOption options[] = {
	{ .action = "Nuke Them", .description = "What could go wrong?"},
	{ .action = "Fight Proxy War", .description = "No open hostilities here."},
	{ .action = "Send Spies", .description = "Get that precious intel."},
	{ .action = "Ask To Not Nuke", .description = "Please don't hurt us!"}
};
#define NUM_OPTIONS = sizeof(options) / sizeof(ColdWarOption)

static Window *s_initial_splash, *s_main_window, *s_menu_window, *s_end_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_info_layer, *s_stats_layer;
static BitmapLayer *s_splash_layer, *s_background_layer, *s_end_layer;
static GBitmap *s_splash_bitmap, *s_background_bitmap, *s_end_win_bitmap, *s_end_lose_bitmap;
static bool player_won, end_game = false;
static int randnum, nonukestreak = 0;
static int stats[5] = {1,1,1,1,100}; // Your power, their power, your smarts, their smarts, tensions



static void post_turn_event(void) {
	window_stack_remove(s_menu_window, true);
	static char s_stats_buffer[128];
	snprintf(s_stats_buffer, sizeof(s_stats_buffer), "Your Power: %d\nTheir Power: %d\n\
	Your Smarts: %d\nTheir Smarts: %d\nTensions: %d", stats[0],stats[1],stats[2],stats[3],stats[4]);
	text_layer_set_text(s_stats_layer, s_stats_buffer);
	if (end_game) {
		window_stack_push(s_end_window, true);
	} else {
		if (stats[4] < 1) {
			end_game = true;
			player_won = true;
			stats[4] = 0;
			text_layer_set_text(s_info_layer, "Tensions are so low you both demilitarize!");
    } else if (stats[4] > 500) {
			end_game = true;
			player_won = false;
			stats[4] *= 100;
			if (stats[1] + stats[3] > (stats[0] + stats[2]) * 3) {
				text_layer_set_text(s_info_layer, "They nuke you and you can't retaliate!");
			} else {
				text_layer_set_text(s_info_layer, "They nuke you and you nuke them back.");
			}
		} else {
			for (uint i = 0 ; i < 4 ; i++) {
				APP_LOG(APP_LOG_LEVEL_DEBUG, "i: %d, item: %d", i, stats[i]);
				stats[i] += rand() % 10 + 1;
				if (stats[i] < 1) {
					stats[i] = 1;
				}
			}
		}
	
		snprintf(s_stats_buffer, sizeof(s_stats_buffer), "Your Power: %d\nTheir Power: %d\n\
		Your Smarts: %d\nTheir Smarts: %d\nTensions: %d", stats[0],stats[1],stats[2],stats[3],stats[4]);
		text_layer_set_text(s_stats_layer, s_stats_buffer);
		if (end_game) {
			window_stack_push(s_end_window, true);
		}
	}
}



static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return 4;
}

static int16_t menu_get_header_height(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
	// Draw title text in the section header
	menu_cell_basic_header_draw(ctx, cell_layer, "What do?");
}

static void menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // This draws the cell with the appropriate message
	ColdWarOption *option = &options[cell_index->row];
	menu_cell_basic_draw(ctx, cell_layer, option->action, option->description, NULL);
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item will receive the select action
	switch (cell_index->row) {
		case 0:
			stats[4] = (stats[4] + (rand() % 90 + 10)) * (rand() % 990 + 10);
			end_game=true;
			if (stats[0] + stats[2] > (stats[1] + stats[3] * 3) + 10) {
				text_layer_set_text(s_info_layer, "You destroyed them and won the cold war!");
				player_won=true;
			} else {
				text_layer_set_text(s_info_layer, "You nuke each other! Ever heard of MAD?");
				player_won=false;
			}
			nonukestreak = 0;
			break;
		case 1:
			randnum = rand() % 10;
			if (randnum < 4) {
				text_layer_set_text(s_info_layer, "You won the proxy war! You gain power!");
				stats[0] += (rand() % 71 + 10);
				stats[4] += (rand() % 31 + 10);
			} else if (randnum < 8) {
				text_layer_set_text(s_info_layer, "They won the proxy war and gain power.");
				stats[2] += (rand() % 71 + 10);
				stats[4] += (rand() % 31 + 10);
			} else {
				text_layer_set_text(s_info_layer, "The proxy country becomes independant.");
				stats[4] -= (rand() % 31 + 10);
			}
			nonukestreak = 0;
			break;
		case 2:
			randnum = rand() % 10;
			if (randnum < 4) {
				text_layer_set_text(s_info_layer, "You gain valuable intel about them.");
				stats[2] += (rand() % 71 + 10);
			} else if (randnum < 8) {
				text_layer_set_text(s_info_layer, "Your spies are caught and they are angry.");
				stats[4] += (rand() % 71 + 30);
			} else {
				text_layer_set_text(s_info_layer, "Your spies defect and tell them secrets.");
				stats[3] += (rand() % 71 + 10);
				stats[4] += (rand() % 31 + 10);
			}
			nonukestreak = 0;
			break;
		case 3:
			randnum = rand() % 10;
			if ((stats[4] > 300 || randnum > 6 ) || nonukestreak >= 3) {
				text_layer_set_text(s_info_layer, "Your plea incites them to make more nukes.");
				stats[1] += (rand() % 11 + 1);
				stats[4] += (rand() % 21 + 10);
			} else if (stats[4] > 150 || randnum > 2) {
				text_layer_set_text(s_info_layer, "You both agree to cut down on the nukes.");
				stats[0] -= (rand() % 11 + 5);
				stats[1] -= (rand() % 11 + 5);
				stats[4] -= (rand() % 41 + 20);
			} else {
				text_layer_set_text(s_info_layer, "They remove some nukes close to you.");
				stats[1] -= (rand() % 11 + 1);
				stats[4] -= (rand() % 21 + 10);
			}
			nonukestreak += 1;
			break;
	}
	post_turn_event();
}



static void start_game_handler(ClickRecognizerRef recognizer, void *context) {
	window_stack_remove(s_initial_splash, false);
	window_stack_push(s_main_window, false);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (!end_game) {
		window_stack_push(s_menu_window, true);
	}
}

static void click_to_start(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, start_game_handler);
};

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
};



static void initial_splash_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	s_splash_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SPLASH_SCREEN);
	s_splash_layer = bitmap_layer_create(layer_get_bounds(window_layer));
	bitmap_layer_set_bitmap(s_splash_layer, s_splash_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_splash_layer));
}

static void initial_splash_unload(Window *window) {
	gbitmap_destroy(s_splash_bitmap);
	bitmap_layer_destroy(s_splash_layer);
}

static void main_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	
	s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GAME_BACKGROUND);
	s_background_layer = bitmap_layer_create(layer_get_bounds(window_layer));
	bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
	
	s_info_layer = text_layer_create(GRect(5, 40, 144-5, 168-40));
	layer_add_child(window_layer, text_layer_get_layer(s_info_layer));
	text_layer_set_text(s_info_layer, "Uh oh, it looks like you're in a cold war.");
	
	s_stats_layer = text_layer_create(GRect(5, 75, 144-5, 168-75));
	layer_add_child(window_layer, text_layer_get_layer(s_stats_layer));
	static char s_stats_buffer[128];
	snprintf(s_stats_buffer, sizeof(s_stats_buffer), "Your Power: %d\nTheir Power: %d\n\
	Your Smarts: %d\nTheir Smarts: %d\nTensions: %d", stats[0],stats[1],stats[2],stats[3],stats[4]);
	text_layer_set_text(s_stats_layer, s_stats_buffer);
}

static void main_window_unload(Window *window) {
	gbitmap_destroy(s_background_bitmap);
	bitmap_layer_destroy(s_background_layer);
	text_layer_destroy(s_info_layer);
	text_layer_destroy(s_stats_layer);
}

static void menu_window_load(Window *window) {
  // Now we prepare to initialize the menu layer
  Layer *window_layer = window_get_root_layer(window);

  // Create the menu layer
  s_menu_layer = menu_layer_create(layer_get_frame(window_layer));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_get_num_rows,
    .get_header_height = menu_get_header_height,
    .draw_header = menu_draw_header,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
}

static void end_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	s_end_layer = bitmap_layer_create(layer_get_bounds(window_layer));
	s_end_win_bitmap = gbitmap_create_with_resource(RESOURCE_ID_END_SCREEN_WIN);
	s_end_lose_bitmap = gbitmap_create_with_resource(RESOURCE_ID_END_SCREEN_LOSE);
	if (player_won) {
		bitmap_layer_set_bitmap(s_end_layer, s_end_win_bitmap);
	} else {
		bitmap_layer_set_bitmap(s_end_layer, s_end_lose_bitmap);
	}
	layer_add_child(window_layer, bitmap_layer_get_layer(s_end_layer));
}

static void end_window_unload(Window *window) {
	gbitmap_destroy(s_end_win_bitmap);
	gbitmap_destroy(s_end_lose_bitmap);
	bitmap_layer_destroy(s_end_layer);
}



static void init() {
  srand(time(NULL));
	
	s_initial_splash = window_create();
	window_set_click_config_provider(s_initial_splash, click_to_start);
	window_set_window_handlers(s_initial_splash, (WindowHandlers) {
    .load = initial_splash_load,
    .unload = initial_splash_unload,
  });
	
	s_main_window = window_create();
	window_set_click_config_provider(s_main_window, click_config_provider);
	window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
	
	s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers) {
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
	
	s_end_window = window_create();
  window_set_window_handlers(s_end_window, (WindowHandlers) {
    .load = end_window_load,
    .unload = end_window_unload,
  });
	
  window_stack_push(s_initial_splash, true);
}

static void deinit() {
	window_destroy(s_initial_splash);
	window_destroy(s_main_window);
	window_destroy(s_menu_window);
	window_destroy(s_end_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
