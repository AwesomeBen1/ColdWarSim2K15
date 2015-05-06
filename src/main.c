// Cold War Simulator 2K15
// Made by Ben Chapman-Kish from 2015-03-14 to 2015-05-05
#include "pebble.h"
// The possibilities of segfaults, memory overflows, and memory leaks are features, by the way
// Features to add: None! All my original ideas are now incorporated into the app.

#define TIMER_MS 2200
#define FIRST_RUN_KEY 1
	
#ifdef PBL_PLATFORM_BASALT
	#define TOP_LAYER_Y_OFFSET 9
#else
	#define TOP_LAYER_Y_OFFSET 0
#endif

typedef struct {
 char *name;
 char *desc;
} ColdWarAction;
ColdWarAction actions[] = {
	{ .name = "Nuke Them", .desc = "What could go wrong?"},
	{ .name = "Fight Proxy War", .desc = "No open hostilities here."},
	{ .name = "Send Spies", .desc = "Get that precious intel."},
	{ .name = "Ask To Not Nuke", .desc = "Please don't hurt us!"},
	{ .name = "Produce Nukes", .desc = "I have the power!"},
	{ .name = "Push Research", .desc = "Conscript smart people."}
};
#define NUM_ACTIONS sizeof(actions) / sizeof(ColdWarAction)
#define NUM_OUTCOMES 23
#define NUM_OPTIONS 2
#define NUM_SECTIONS 2

// Ugly, but how else can I do it?
static char *histactions[] = {"Nuked Them", "Fought Proxy War", \
	"Sent Spies", "Asked No Nukes", "Produced Nukes", "Researched"};
static char *histoutcomes[NUM_ACTIONS][NUM_OUTCOMES] = {
	{ "You defeated them",
	"They nuked you back" },
	{ "Armistice signed",
	"Became independant",
	"You won",
	"They won",
	"Your pyhrric victory",
	"Their pyhhric victory" },
	{ "You gained intel",
	"Spies were caught",
	"Killed their spies",
	"Spies were killed",
	"Spies defected" },
	{ "You angered them",
	"They declined",
	"You compromised",
	"They complied" },
	{ "They didn't notice",
	"They made nukes too",
	"You were blockaded" },
	{ "Caught their spies",
	"No notable results",
	"Discovery made" }
};

/* After four days of work, I finally made the history array work.
I have no idea why it's so hard to just make an array that can be resized... */
typedef struct {
 int action;
 int outcome;
} ColdWarHistory;
static ColdWarHistory *history;
static bool hist_list_exists = false;
static Window *s_initial_splash, *s_main_window, *s_menu_window, *s_hist_window, *s_help_window, *s_end_window;
static MenuLayer *s_menu_layer, *s_hist_layer;
static TextLayer *s_info_layer, *s_stats_layer, *s_help_title_layer, *s_help_layer;
static ScrollLayer *s_scroll_layer;
static BitmapLayer *s_splash_layer, *s_background_layer, *s_end_layer;
static GBitmap *s_splash_bitmap, *s_background_bitmap, *s_end_win_bitmap, *s_end_lose_bitmap;
static AppTimer *s_timer;
static bool player_won, hist_show_turns = false, end_game = false;
static int randnum, last_action = -1, turn = 0, end_outcome = 0, nonukestreak = 0;
static int stats[5] = {1,1,1,1,100}; // Your power, their power, your smarts, their smarts, tensions
static int actionstreak[2] = {0,0};
static char s_menu_header_buffer[32], happened_on[16];



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
	for (uint i = 0 ; i < 4 ; ++i) {
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
	/* The history functionality took a couple days of intense researching and trial-and-error
	to get working with dynamic memory allocation and string arrays, but mostly the former. */
	if (hist_list_exists) {
		void* temp = realloc(history, (turn + 1) * sizeof(ColdWarHistory));
		if(!temp)
		{
    	free(history);
			APP_LOG(APP_LOG_LEVEL_ERROR, "Error: Can't allocate memory for history array.");
			return;
		}
		history = temp;
	} else {
		history = (ColdWarHistory *) malloc(sizeof(ColdWarHistory));
		hist_list_exists = true;
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding to history: %d, %d", action, outcome);
	history[turn].action = action;
	history[turn].outcome = outcome;
}

static uint8_t actionrepeats(int action) {
	if (actionstreak[0] == action) {
		return actionstreak[1];
	}
	return 0;
}

static void take_action(int action) {
	switch (action) {
		case 0:
			// If nukes start getting thrown around, tensions soar, cuz global destruction imminent, duh
			stats[4] = (stats[4] + randrange(10, 100)) * randrange(10, 1000) + randrange(0, 100);
			end_game = true;
			end_outcome = 0;
			if (stats[0] + stats[2] > (stats[1] + stats[3] * 3) + 10) {
				// Unrealistic for one side to nuke the other and survive, but it's gameplay incentive
				text_layer_set_text(s_info_layer, "You destroyed them and won the cold war!");
				player_won = true;
				add_to_hist(0,0);
			} else {
				// This is why the real cold war ended peacefully. Mutually assured destruction.
				text_layer_set_text(s_info_layer, "You nuke each other! Ever heard of MAD?");
				player_won = false;
				add_to_hist(0,1);
			}
			break;
		case 1:
			randnum = rand() % 8;
			// So many proxy wars in the cold war and they all ended in different ways...
			if (randnum < 1) {
				text_layer_set_text(s_info_layer, "The conflict ends with an armistice.");
				stats[4] += randrange(-10, 10);
				add_to_hist(1,0);
			} else if (randnum < 2) {
				text_layer_set_text(s_info_layer, "The proxy country becomes independant.");
				stats[4] -= randrange(10, 40);
				add_to_hist(1,1);
			} else if (randnum < 4) {
				text_layer_set_text(s_info_layer, "You won the proxy war! You gain power!");
				stats[0] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(1,2);
			} else if (randnum < 6) {
				text_layer_set_text(s_info_layer, "They won the proxy war and gain power.");
				stats[1] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(1,3);
			} else if (randnum < 7) {
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
			randnum = rand() % 8;
			// These outcomes are reminiscent of CIA vs. KGB fiction
			if (randnum < 1 || (randnum < 3 && actionrepeats(2) <= 3)) {
				text_layer_set_text(s_info_layer, "You gain valuable intel about them.");
				stats[2] += randrange(10, 80);
				add_to_hist(2,0);
			} else if (randnum < 5) {
				text_layer_set_text(s_info_layer, "Your spies are caught and they are angry.");
				stats[4] += randrange(30, 100);
				add_to_hist(2,1);
			} else if (randnum < 6) {
				text_layer_set_text(s_info_layer, "Your spies kill some of their spies.");
				stats[3] -= randrange(5, 25);
				stats[4] += randrange(30, 100);
				add_to_hist(2,2);
			} else if (randnum < 7) {
				text_layer_set_text(s_info_layer, "Your spies are assassinated.");
				stats[2] -= randrange(10, 25);
				stats[4] += randrange(20, 80);
				add_to_hist(2,3);
			} else {
				// Sort of like what happened with Igor Gouzenko... kind of
				text_layer_set_text(s_info_layer, "Your spies defect and tell them secrets.");
				stats[3] += randrange(10, 80);
				stats[4] += randrange(10, 40);
				add_to_hist(2,4);
			}
			break;
		case 3:
			randnum = rand() % 6;
			if (stats[4] > 300 || randnum > 4 || actionrepeats(3) >= 5) {
				text_layer_set_text(s_info_layer, "Your plea incites them to make more nukes.");
				stats[1] += randrange(5, 20);
				stats[4] += randrange(10, 30);
				add_to_hist(3,0);
			} else if (stats[4] > 150 || randnum > 3 || (nonukestreak >= 3 && randnum > 2)) {
				text_layer_set_text(s_info_layer, "They politefully decline your request.");
				stats[4] += randrange(-5, 5);
				add_to_hist(3,1);
			} else if (randnum > 1) {
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
		case 4:
			randnum = rand() % 10;
			// Nuclear stockpiles increased quickly in the cold war	
			if (randnum > 6 || (stats[4] < 300 && randnum > 5)) {
				text_layer_set_text(s_info_layer, "You secretly add nukes to your arsenal.");
				stats[0] += randrange(20, 60);
				add_to_hist(4,0);
			} else if (randnum > 2) {
				text_layer_set_text(s_info_layer, "They realize and make nukes themselves.");
				stats[0] += randrange(20, 60);
				stats[1] += randrange(20, 60);
				stats[4] += randrange(20, 60);
				add_to_hist(4,1);
			} else {
				// Also like the October Crisis
				text_layer_set_text(s_info_layer, "They blockade your nuclear shipments.");
				stats[0] += randrange(-5, 5);
				stats[4] += randrange(30, 100);
				add_to_hist(4,2);
			}
			break;
		case 5:
		randnum = rand() % 4;
			// How nuclear weapons were even discovered in the first place
			if (randnum < 1 || (randnum < 2 && stats[2] > 150)) {
				text_layer_set_text(s_info_layer, "Spies are caught in your research team!");
				stats[2] += randrange(20, 60);
				stats[3] += randrange(20, 60);
				stats[4] += randrange(30, 80);
				add_to_hist(5,0);
			} else if (randnum < 3) {
				text_layer_set_text(s_info_layer, "Nothing of importance is discovered.");
				add_to_hist(5,1);
			} else {
				text_layer_set_text(s_info_layer, "Your research finds a useful discovery.");
				stats[2] += randrange(20, 60);
				add_to_hist(5,2);
			}
			break;
		default:
			break;
	}
	if (actionstreak[0] == last_action) {
		++actionstreak[1];
	} else {
		actionstreak[0] = last_action;
		actionstreak[1] = 1;
	}
	++turn;
}



static uint16_t menu_get_num_sections(MenuLayer *menu_layer, void *data) {
  if (end_game) {
		return NUM_SECTIONS - 1;
	} else {
		return NUM_SECTIONS;
	}
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
	if (end_game || section_index == 1) {
		return NUM_OPTIONS;
	} else {
		return NUM_ACTIONS;
	}
	return 0;
}

static int16_t menu_get_header_height(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
	if (end_game) {
		// App options at the end of the game
		snprintf(s_menu_header_buffer, sizeof(s_menu_header_buffer), "Turn %d: Game Over!", turn);
		menu_cell_basic_header_draw(ctx, cell_layer, s_menu_header_buffer);
	} else if (section_index == 1) {
		// App options
		menu_cell_basic_header_draw(ctx, cell_layer, "Other Stuffs");
	} else {
		// Actions to take
		snprintf(s_menu_header_buffer, sizeof(s_menu_header_buffer), "Turn %d: What Do?", (turn + 1));
		menu_cell_basic_header_draw(ctx, cell_layer, s_menu_header_buffer);
	}
}

static void menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // This draws the cell with the appropriate message
	if (end_game || cell_index->section == 1) {
		switch (cell_index->row) {
      case 0:
        menu_cell_basic_draw(ctx, cell_layer, "Play New Game", NULL, NULL);
        break;
			case 1:
        menu_cell_basic_draw(ctx, cell_layer, "Instructions", NULL, NULL);
        break;
    }
	} else {
		ColdWarAction *action = &actions[cell_index->row];
		menu_cell_basic_draw(ctx, cell_layer, action->name, action->desc, NULL);
	}
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	if (end_game || cell_index->section == 1) {
		switch (cell_index->row) {
			case 0:
				free(history);
				end_game = hist_list_exists = false;
				last_action = -1;
				turn = end_outcome = nonukestreak = 0;
				stats[0] = stats[1] = stats[2] = stats[3] = 1;
				stats[4] = 100;
				update_stats_text();
				text_layer_set_text(s_info_layer, "Uh oh, it looks like you're in a cold war.");
				window_stack_remove(s_menu_window, true);
				break;
			case 1:
				window_stack_push(s_help_window, true);
				break;
		}
	} else {
		// Use the row to specify which item will receive the select action	
			last_action = cell_index->row;
			take_action(last_action);
			update_stats_text();
			window_stack_remove(s_menu_window, true);
			post_turn_event();
	}
}



static uint16_t hist_get_num_sections(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t hist_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
	return turn;
}

static void hist_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	int histindex = turn - cell_index->row - 1;
	ColdWarHistory *histitem = &history[histindex];
	char *action = histactions[histitem->action];
	if (hist_show_turns) {
		snprintf(happened_on, sizeof(happened_on), "Turn %d", (turn - cell_index->row));
		menu_cell_basic_draw(ctx, cell_layer, action, happened_on, NULL);
	} else {
		char *outcome = histoutcomes[histitem->action][histitem->outcome];
		menu_cell_basic_draw(ctx, cell_layer, action, outcome, NULL);
	}
}

static void hist_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	hist_show_turns = !hist_show_turns;
	layer_mark_dirty(menu_layer_get_layer(menu_layer));
}



static void timer_start_game(void *data) {
  window_stack_remove(s_initial_splash, false);
	window_stack_push(s_main_window, false);
	bool firstrun = persist_exists(FIRST_RUN_KEY) ? persist_read_bool(FIRST_RUN_KEY) : true;
	if (firstrun) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "First time running app, creating persistent storage bool");
		persist_write_bool(FIRST_RUN_KEY, false);
		window_stack_push(s_help_window, true);
	}
}

static void start_game_handler(ClickRecognizerRef recognizer, void *context) {
	app_timer_cancel(s_timer);
	window_stack_remove(s_initial_splash, false);
	window_stack_push(s_main_window, false);
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	window_stack_push(s_menu_window, true);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (last_action >= 0) {
		window_stack_push(s_hist_window, true);
	}
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (!end_game && (last_action >= 0)) {
		take_action(last_action);
		update_stats_text();
		post_turn_event();
	}
}

static void close_help_handler(ClickRecognizerRef recognizer, void *context) {
	window_stack_remove(s_help_window, true);
}



static void click_to_start(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, start_game_handler);
};

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
};

static void help_click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, close_help_handler);
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
	
	s_info_layer = text_layer_create(GRect(5, 40+TOP_LAYER_Y_OFFSET, 144-5, 168-40-TOP_LAYER_Y_OFFSET));
	text_layer_set_background_color(s_info_layer, GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_info_layer));
	text_layer_set_text(s_info_layer, "Uh oh, it looks like you're in a cold war.");
	
	s_stats_layer = text_layer_create(GRect(5, 75+TOP_LAYER_Y_OFFSET, 144-5, 168-75-TOP_LAYER_Y_OFFSET));
	text_layer_set_background_color(s_stats_layer, GColorClear);
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
		.get_num_sections = menu_get_num_sections,
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
		.get_num_sections = hist_get_num_sections,
		.get_num_rows = hist_get_num_rows,
    .draw_row = hist_draw_row,
		.select_click = hist_select
  });
	menu_layer_set_click_config_onto_window(s_hist_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_hist_layer));
}

static void hist_window_unload(Window *window) {
  menu_layer_destroy(s_hist_layer);
}

static void help_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

	// Initialize the scroll layer
  s_scroll_layer = scroll_layer_create(layer_get_frame(window_layer));
  // This binds the scroll layer to the window so that up and down map to scrolling
  // You may use scroll_layer_set_callbacks to add or override interactivity
  scroll_layer_set_click_config_onto_window(s_scroll_layer, window);
	// Got this to work thanks to http://redd.it/26t34c
	scroll_layer_set_callbacks(s_scroll_layer, (ScrollLayerCallbacks){
    .click_config_provider = help_click_config_provider
  });
	
	// Initialize the text layer
	s_help_title_layer = text_layer_create(GRect(4, 0, 144-8, 100));
	text_layer_set_text(s_help_title_layer, "Welcome to Cold War Simulator 2K15!");
	text_layer_set_font(s_help_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
	s_help_layer = text_layer_create(GRect(4, 40, 144-8, 2000));
	text_layer_set_text(s_help_layer, "Your objective is to defeat your rival faction, \
	referred to in this game as \"them\", either through force or peacefully.\n\n\
	To defeat them by force, nuke them when your power and smarts is at least thrice \
	their power and smarts. The conflict will also end peacefully when the tensions reach zero.\n\n\
	From the main screen, you can press select to view your actions and options. \
	Every action has outcomes, both good and bad, that are based on luck as well \
	as the \"stats\": the values visible on the main screen. \n\n\
	From the main screen, you can also press up to view the turn history \
	and press down to repeat the last action you took. \
	In the history screen, press the select button to switch between \
	viewing the outcome of an action and what turn it happened on.\n\n\
	Good luck and have fun!\n\n\
	© Ben Chapman-Kish 2015");
	text_layer_set_font(s_help_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	
	// Trim text layer and scroll content to fit text box
	GSize max_size_1 = text_layer_get_content_size(s_help_layer);
	GSize max_size_2 = text_layer_get_content_size(s_help_title_layer);
  text_layer_set_size(s_help_layer, GSize(max_size_1.w, max_size_1.h + 4));
	text_layer_set_size(s_help_title_layer, GSize(max_size_2.w, max_size_2.h + 4));
  scroll_layer_set_content_size(s_scroll_layer, GSize(144, max_size_1.h + max_size_2.h + 18));
	// Add the layers for display
	scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_help_title_layer));
	scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_help_layer));

  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));
}

static void help_window_unload(Window *window) {
	text_layer_destroy(s_help_layer);
	text_layer_destroy(s_help_title_layer);
  scroll_layer_destroy(s_scroll_layer);
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
	
	s_help_window = window_create();
  window_set_window_handlers(s_help_window, (WindowHandlers) {
    .load = help_window_load,
    .unload = help_window_unload,
  });
	
	s_end_window = window_create();
  window_set_window_handlers(s_end_window, (WindowHandlers) {
    .load = end_window_load,
    .unload = end_window_unload,
  });
	
  window_stack_push(s_initial_splash, true);
}

static void deinit() {
	if (hist_list_exists) {
		free(history);
	}
	window_destroy(s_initial_splash);
	window_destroy(s_main_window);
	window_destroy(s_menu_window);
	window_destroy(s_hist_window);
	window_destroy(s_help_window);
	window_destroy(s_end_window);
}

int main(void) {
	init();
  app_event_loop();
  deinit();
}
