// Cold War Simulator 2K15
// Made by Ben Chapman-Kish from 2015-03-14 to 2015-03-19
#include "pebble.h"
// The possibilities of segfaults, memory overflows, and memory leaks are features, by the way
// Features to add: up button to see game history, ability to restart the game

#define TIMER_MS 3000

typedef struct {
 char *name;
 char *desc;
} ColdWarAction;
ColdWarAction actions[] = {
	{ .name = "Nuke Them", .desc = "What could go wrong?"},
	{ .name = "Fight Proxy War", .desc = "No open hostilities here."},
	{ .name = "Send Spies", .desc = "Get that precious intel."},
	{ .name = "Ask To Not Nuke", .desc = "Please don't hurt us!"}
};
#define NUM_ACTIONS sizeof(actions) / sizeof(ColdWarAction)

typedef struct {
 int *action;
 int *outcome;
} ColdWarHistory;
ColdWarHistory history[0] = {};

static Window *s_initial_splash, *s_main_window, *s_menu_window, *s_hist_window, *s_end_window;
static MenuLayer *s_menu_layer, *s_hist_layer;
static TextLayer *s_info_layer, *s_stats_layer;
static BitmapLayer *s_splash_layer, *s_background_layer, *s_end_layer;
static GBitmap *s_splash_bitmap, *s_background_bitmap, *s_end_win_bitmap, *s_end_lose_bitmap;
static AppTimer *s_timer;
static bool player_won, end_game = false;
static int randnum, last_action, turn = 0, end_outcome = 0, nonukestreak = 0;
static int stats[5] = {1,1,1,1,100}; // Your power, their power, your smarts, their smarts, tensions



static int randrange(int min_val, int max_val) {
	// It's like random.randrange in python, but inclusive, and without default parameters!
	return (rand() % (1 + max_val - min_val)) + min_val;
}

static void update_stats_text(void) {
	static char s_stats_buffer[256];
	snprintf(s_stats_buffer, sizeof(s_stats_buffer), "Your Power: %d\nTheir Power: %d\n\
	Your Smarts: %d\nTheir Smarts: %d\nTensions: %d", stats[0],stats[1],stats[2],stats[3],stats[4]);
	/* I wish there was a more elegant way to pass all items in the stats array.
	In python you can pass all items in a list by doing *list in a function,
	But in C, an asterisk before a variable dereferences it. */
	text_layer_set_text(s_stats_layer, s_stats_buffer);
}

static void timer_show_end(void *data) {
	/* Timer callbacks: Because psleep didn't work.
	I would have passed end_outcome as a parameter, but I don't think I can. */
	switch (end_outcome) {
		case 1:
			text_layer_set_text(s_info_layer, "Tensions are so low you both demilitarize!");
			break;
		case 2:
			stats[4] = (stats[4] + randrange(10, 100)) * randrange(10, 1000) + randrange(0, 100);
			text_layer_set_text(s_info_layer, "They nuke you and you can't retaliate!");
			break;
		case 3:
			stats[4] = (stats[4] + randrange(10, 100)) * randrange(10, 1000) + randrange(0, 100);
			text_layer_set_text(s_info_layer, "They nuke you and you nuke them back.");
			break;
		default:
			break;
	}
	window_stack_push(s_end_window, true);
}

static void post_turn_event(void) {
	for (uint i = 0 ; i < NUM_ACTIONS ; ++i) {
		stats[i] += randrange(1, 10); // stats increase by natural progression
		if (stats[i] < 1) {
			stats[i] = 1; // stats can't be less than one!
		}
	}
	if (end_game) {
		app_timer_register(TIMER_MS, timer_show_end, NULL);
	} else {
		if (stats[4] < 1) { // If tensions are low enough, everybody wins!
			end_game = true;
			player_won = true;
			stats[4] = 0;
			update_stats_text();
			end_outcome = 1;
			app_timer_register(TIMER_MS, timer_show_end, NULL);
    } else if (stats[4] > 500) { // If tensions are high enough, they nuke you anyway!
			end_game = true;
			player_won = false;
			update_stats_text();
			if (stats[1] + stats[3] > (stats[0] + stats[2]) * 3) { // But are you able to retaliate?
				end_outcome = 2;
			} else {
				end_outcome = 3;
			}
			app_timer_register(TIMER_MS, timer_show_end, NULL);
		} else {
			update_stats_text();
		}
	}
}

static void add_to_hist(int action, int outcome) {
	/*void* newMem = realloc(oldMem, newSize);
	if(!newMem)
	{
    // handle error
	}
	oldMem = newMem;*/
}

static void take_action(int action) {
	switch (action) {
		case 0:
			// If nukes start getting thrown around, tensions soar, cuz global destruction imminent, duh
			stats[4] = (stats[4] + randrange(10, 100)) * randrange(10, 1000) + randrange(0, 100);
			end_game=true;
			if (stats[0] + stats[2] > (stats[1] + stats[3] * 3) + 10) {
				// Unrealistic for one side to nuke the other and survive, but it's gameplay incentive
				text_layer_set_text(s_info_layer, "You destroyed them and won the cold war!");
				player_won=true;
				add_to_hist(0,0);
			} else {
				// This is why the real cold war ended peacefully. Mutually assured destruction.
				text_layer_set_text(s_info_layer, "You nuke each other! Ever heard of MAD?");
				player_won=false;
				add_to_hist(0,1);
			}
			break;
		case 1:
			randnum = rand() % 16;
			// So many proxy wars in the cold war and they all ended in different ways...
			if (randnum < 2) {
				text_layer_set_text(s_info_layer, "The conflict ends with an armistice.");
				stats[4] += randrange(-10, 10);
				add_to_hist(1,0);
			} else if (randnum < 4) {
				text_layer_set_text(s_info_layer, "The proxy country becomes independant.");
				stats[4] -= randrange(10, 40);
				add_to_hist(1,1);
			} else if (randnum < 8) {
				text_layer_set_text(s_info_layer, "You won the proxy war! You gain power!");
				stats[0] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(1,2);
			} else if (randnum < 12) {
				text_layer_set_text(s_info_layer, "They won the proxy war and gain power.");
				stats[1] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(1,3);
			} else if (randnum < 14) {
				text_layer_set_text(s_info_layer, "You win the proxy war, but at great cost.");
				stats[0] += randrange(-10, 10);
				stats[1] -= randrange(0, 20);
				stats[4] += randrange(10, 40);
				add_to_hist(1,4);
			} else {
				text_layer_set_text(s_info_layer, "They win the proxy war at a great cost.");
				stats[0] -= randrange(0, 20);
				stats[1] += randrange(-10, 10);
				stats[4] += randrange(10, 40);
				add_to_hist(1,5);
			}
			break;
		case 2:
			randnum = rand() % 12;
			// These outcomes are reminiscent of CIA vs. KGB fiction
			if (randnum < 4) {
				text_layer_set_text(s_info_layer, "You gain valuable intel about them.");
				stats[2] += randrange(10, 80);
				add_to_hist(2,0);
			} else if (randnum < 8) {
				text_layer_set_text(s_info_layer, "Your spies are caught and they are angry.");
				stats[4] += randrange(30, 100);
				add_to_hist(2,1);
			} else if (randnum < 10) {
				text_layer_set_text(s_info_layer, "Your spies kill some of their spies.");
				stats[3] -= randrange(5, 25);
				stats[4] += randrange(30, 100);
				add_to_hist(2,2);
			} else {
				// Sort of like what happened with Igor Gouzenko... kind of
				text_layer_set_text(s_info_layer, "Your spies defect and tell them secrets.");
				stats[3] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(2,3);
			}
			break;
		case 3:
			randnum = rand() % 12;
			if (stats[4] > 300 || randnum > 8 || nonukestreak >= 4) {
				text_layer_set_text(s_info_layer, "Your plea incites them to make more nukes.");
				stats[1] += randrange(5, 20);
				stats[4] += randrange(10, 30);
				add_to_hist(3,0);
			} else if (stats[4] > 150 || randnum > 6 || (nonukestreak >= 2 && randnum > 4)) {
				text_layer_set_text(s_info_layer, "They politefully decline your request.");
				stats[4] += randrange(-5, 5);
				add_to_hist(3,1);
			} else if (randnum > 2) {
				// It's just like the October Crisis!
				text_layer_set_text(s_info_layer, "You both agree to cut down on the nukes.");
				stats[0] -= randrange(5, 20);
				stats[1] -= randrange(5, 20);
				stats[4] -= randrange(20, 70);
				add_to_hist(3,2);
			} else {
				text_layer_set_text(s_info_layer, "They remove some nukes close to you.");
				stats[1] -= randrange(5, 20);
				stats[4] -= randrange(10, 40);
				add_to_hist(3,3);
			}
			break;
		default:
			break;
	}
	if (action == 3) {
		++nonukestreak;
	} else {
		nonukestreak = 0;
	}
	++turn;
}



static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return NUM_ACTIONS;
}

static int16_t menu_get_header_height(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
	static char s_menu_header_buffer[32];
	snprintf(s_menu_header_buffer, sizeof(s_menu_header_buffer), "Turn %d: What Do?", (turn + 1));
	// Draw title text in the section header
	menu_cell_basic_header_draw(ctx, cell_layer, s_menu_header_buffer);
}

static void menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // This draws the cell with the appropriate message
	ColdWarAction *action = &actions[cell_index->row];
	menu_cell_basic_draw(ctx, cell_layer, action->name, action->desc, NULL);
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item will receive the select action
	last_action = cell_index->row;
	take_action(last_action);
	update_stats_text();
	window_stack_remove(s_menu_window, true);
	post_turn_event();
}



static uint16_t hist_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return sizeof(history) / sizeof(ColdWarHistory);
}

static void hist_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // This draws the cell with the appropriate message
	ColdWarHistory *histitem = &history[cell_index->row];
	menu_cell_basic_draw(ctx, cell_layer, histitem->action, histitem->outcome, NULL);
}

/*static void hist_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  
}*/



static void timer_start_game(void *data) {
  window_stack_remove(s_initial_splash, false);
	window_stack_push(s_main_window, false);
}

static void start_game_handler(ClickRecognizerRef recognizer, void *context) {
	app_timer_cancel(s_timer);
	window_stack_remove(s_initial_splash, false);
	window_stack_push(s_main_window, false);
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (end_game) {
		window_stack_push(s_end_window, true);
	} else {
		window_stack_push(s_menu_window, true);
	}
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (!end_game && last_action) {
		take_action(last_action);
		update_stats_text();
		post_turn_event();
	}
}



static void click_to_start(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, start_game_handler);
};

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
};



static void initial_splash_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	s_splash_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SPLASH_SCREEN);
	s_splash_layer = bitmap_layer_create(layer_get_bounds(window_layer));
	bitmap_layer_set_bitmap(s_splash_layer, s_splash_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_splash_layer));
	s_timer = app_timer_register(TIMER_MS, timer_start_game, NULL);
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
	update_stats_text();
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
    .select_click = menu_select
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
	menu_layer_destroy(s_menu_layer);
}

static void hist_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  s_hist_layer = menu_layer_create(layer_get_frame(window_layer));
  
	menu_layer_set_callbacks(s_hist_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = hist_get_num_rows,
    .draw_row = hist_draw_row,
    //.select_click = hist_select
  });

	menu_layer_set_click_config_onto_window(s_hist_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_hist_layer));
}

static void hist_window_unload(Window *window) {
  menu_layer_destroy(s_hist_layer);
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
	
	s_hist_window = window_create();
  window_set_window_handlers(s_hist_window, (WindowHandlers) {
    .load = hist_window_load,
    .unload = hist_window_unload,
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
	window_destroy(s_hist_window);
	window_destroy(s_end_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
