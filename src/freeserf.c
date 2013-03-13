/*
 * freeserf.c - Main program source.
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "freeserf.h"
#include "freeserf_endian.h"
#include "serf.h"
#include "flag.h"
#include "building.h"
#include "player.h"
#include "map.h"
#include "game.h"
#include "gui.h"
#include "popup.h"
#include "panel.h"
#include "viewport.h"
#include "interface.h"
#include "gfx.h"
#include "data.h"
#include "sdl-video.h"
#include "misc.h"
#include "debug.h"
#include "log.h"
#include "audio.h"
#include "savegame.h"
#include "version.h"

/* TODO This file is one big of mess of all the things that should really
   be separated out into smaller files.  */

#define DEFAULT_SCREEN_WIDTH  800
#define DEFAULT_SCREEN_HEIGHT 600

#ifndef DEFAULT_LOG_LEVEL
# ifndef NDEBUG
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_DEBUG
# else
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_INFO
# endif
#endif


static unsigned int tick;

static int game_loop_run;

static frame_t screen_frame;
static frame_t cursor_buffer;

static interface_t interface;


/* Facilitates quick lookup of offsets following a spiral pattern in the map data.
   The columns following the second are filled out by setup_spiral_pattern(). */
static int spiral_pattern[] = {
	0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Initialize the global spiral_pattern. */
static void
init_spiral_pattern()
{
	static const int spiral_matrix[] = {
		1,  0,  0,  1,
		1,  1, -1,  0,
		0,  1, -1, -1,
		-1,  0,  0, -1,
		-1, -1,  1,  0,
		0, -1,  1,  1
	};

	game.spiral_pattern = spiral_pattern;

	for (int i = 0; i < 49; i++) {
		int x = spiral_pattern[2 + 12*i];
		int y = spiral_pattern[2 + 12*i + 1];

		for (int j = 0; j < 6; j++) {
			spiral_pattern[2+12*i+2*j] = x*spiral_matrix[4*j+0] + y*spiral_matrix[4*j+2];
			spiral_pattern[2+12*i+2*j+1] = x*spiral_matrix[4*j+1] + y*spiral_matrix[4*j+3];
		}
	}
}


static void
player_interface_init()
{
	game.frame = &screen_frame;
	int width = sdl_frame_get_width(game.frame);
	int height = sdl_frame_get_height(game.frame);

	interface_init(&interface);
	gui_object_set_size((gui_object_t *)&interface, width, height);
	gui_object_set_displayed((gui_object_t *)&interface, 1);
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
static void
init_spiral_pos_pattern()
{
	int *pattern = game.spiral_pattern;

	if (game.spiral_pos_pattern == NULL) {
		game.spiral_pos_pattern = malloc(295*sizeof(map_pos_t));
		if (game.spiral_pos_pattern == NULL) abort();
	}

	for (int i = 0; i < 295; i++) {
		int x = pattern[2*i] & game.map.col_mask;
		int y = pattern[2*i+1] & game.map.row_mask;

		game.spiral_pos_pattern[i] = MAP_POS(x, y);
	}
}

/* Reset various global map/game state. */
static void
reset_game_objs()
{
	game.map_water_level = 20;
	game.map_max_lake_area = 14;

	game.update_map_last_anim = 0;
	game.update_map_counter = 0;
	game.update_map_16_loop = 0;
	game.update_map_initial_pos = 0;
	/* game.field_54 = 0; */
	/* game.field_56 = 0; */
	game.next_index = 0;

	/* loops */
	memset(game.flg_bitmap, 0, ((game.max_flg_cnt-1) / 8) + 1);
	memset(game.buildings_bitmap, 0, ((game.max_building_cnt-1) / 8) + 1);
	memset(game.serfs_bitmap, 0, ((game.max_serf_cnt-1) / 8) + 1);
	memset(game.inventories_bitmap, 0, ((game.max_inventory_cnt-1) / 8) + 1);

	game.max_ever_flag_index = 0;
	game.max_ever_building_index = 0;
	game.max_ever_serf_index = 0;
	game.max_ever_inventory_index = 0;

	/* Create NULL-serf */
	serf_t *serf;
	game_alloc_serf(&serf, NULL);
	serf->state = SERF_STATE_NULL;
	serf->type = 0;
	serf->animation = 0;
	serf->counter = 0;
	serf->pos = -1;

	/* Create NULL-flag (index 0 is undefined) */
	game_alloc_flag(NULL, NULL);

	/* Create NULL-building (index 0 is undefined) */
	building_t *building;
	game_alloc_building(&building, NULL);
	building->bld = 0;
}

/* Initialize map constants for the given map size (map_cols, map_rows). */
static void
init_map_vars()
{
	const int map_size_arr[] = {
		16, 30, 55, 90,
		150, 220, 350, 500
	};

	/* game.split |= BIT(3); */

	if (game.map.cols < 64 || game.map.rows < 64) {
		/* game.split &= ~BIT(3); */
	}

	map_init_dimensions(&game.map);

	game.map_regions = (game.map.cols >> 5) * (game.map.rows >> 5);
	game.map_max_serfs_left = game.map_regions * 500;
	game.map_62_5_times_regions = (game.map_regions * 500) >> 3;

	int active_players = 0;
	for (int i = 0; i < 4; i++) {
		if (game.pl_init[0].face != 0) active_players += 1;
	}

	game.map_field_4A = game.map_max_serfs_left -
		active_players * game.map_62_5_times_regions;
	game.map_gold_morale_factor = 10 * 1024 * active_players;
	game.map_field_52 = map_size_arr[game.map_size];
}

/* Initialize AI parameters. */
static void
init_ai_values(player_t *player, int face)
{
	const int ai_values_0[] = { 13, 10, 16, 9, 10, 8, 6, 10, 12, 5, 8 };
	const int ai_values_1[] = { 10000, 13000, 16000, 16000, 18000, 20000, 19000, 18000, 30000, 23000, 26000 };
	const int ai_values_2[] = { 10000, 35000, 20000, 27000, 37000, 25000, 40000, 30000, 50000, 35000, 40000 };
	const int ai_values_3[] = { 0, 36, 0, 31, 8, 480, 3, 16, 0, 193, 39 };
	const int ai_values_4[] = { 0, 30000, 5000, 40000, 50000, 20000, 45000, 35000, 65000, 25000, 30000 };
	const int ai_values_5[] = { 60000, 61000, 60000, 65400, 63000, 62000, 65000, 63000, 64000, 64000, 64000 };

	player->ai_value_0 = ai_values_0[face-1];
	player->ai_value_1 = ai_values_1[face-1];
	player->ai_value_2 = ai_values_2[face-1];
	player->ai_value_3 = ai_values_3[face-1];
	player->ai_value_4 = ai_values_4[face-1];
	player->ai_value_5 = ai_values_5[face-1];
}

/* Initialize player_t objects. */
static void
reset_player_settings()
{
	game.winning_player = -1;
	/* TODO ... */
	game.max_next_index = 33;

	/* TODO */

	for (int i = 0; i < 4; i++) {
		player_t *player = game.player[i];
		memset(player, 0, sizeof(player_t));
		player->flags = 0;

		player_init_t *init = &game.pl_init[i];
		if (init->face != 0) {
			player->flags |= BIT(6); /* Player active */
			if (init->face < 12) { /* AI player */
				player->flags |= BIT(7); /* Set AI bit */
				/* TODO ... */
				game.max_next_index = 49;
			}

			player->player_num = i;
			/* player->field_163 = 0; */
			player->build = 0;
			/*player->field_163 |= BIT(0);*/

			player->building = 0;
			player->castle_flag = 0;
			player->cont_search_after_non_optimal_find = 7;
			player->field_110 = 4;
			player->knights_to_spawn = 0;
			/* OBSOLETE player->spawn_serf_want_knight = 0; **/
			player->total_land_area = 0;

			/* TODO ... */

			player->last_anim = 0;

			player->serf_to_knight_rate = 20000;
			player->serf_to_knight_counter = 0x8000;

			player->knight_occupation[0] = 0x10;
			player->knight_occupation[1] = 0x21;
			player->knight_occupation[2] = 0x32;
			player->knight_occupation[3] = 0x43;

			player_reset_food_priority(player);
			player_reset_planks_priority(player);
			player_reset_steel_priority(player);
			player_reset_coal_priority(player);
			player_reset_wheat_priority(player);
			player_reset_tool_priority(player);

			player_reset_flag_priority(player);
			player_reset_inventory_priority(player);

			player->current_sett_5_item = 8;
			player->current_sett_6_item = 15;

			player->military_max_gold = 0;
			player->military_gold = 0;
			player->inventory_gold = 0;

			player->timers_count = 0;

			player->castle_knights_wanted = 3;
			player->castle_knights = 0;
			/* TODO ... */
			player->serf_index = 0;
			/* TODO ... */
			player->initial_supplies = init->supplies;
			player->reproduction_reset = (60 - init->reproduction) * 50;
			player->ai_intelligence = 1300*init->intelligence + 13535;

			if (/*init->face != 0*/1) { /* Redundant check */
				if (init->face < 12) { /* AI player */
					init_ai_values(player, init->face);
				}
			}

			player->reproduction_counter = player->reproduction_reset;
			/* TODO ... */
			for (int i = 0; i < 26; i++) player->resource_count[i] = 0;
			for (int i = 0; i < 24; i++) {
				player->completed_building_count[i] = 0;
				player->incomplete_building_count[i] = 0;
			}

			/* TODO */

			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 112; j++) player->player_stat_history[i][j] = 0;
			}

			for (int i = 0; i < 26; i++) {
				for (int j = 0; j < 120; j++) player->resource_count_history[i][j] = 0;
			}

			for (int i = 0; i < 27; i++) player->serf_count[i] = 0;
		}
	}

	if (BIT_TEST(game.split, 6)) { /* Coop mode */
		/* TODO ... */
	}
}

/* Initialize global stuff. */
static void
init_player_settings()
{
	game.anim = 0;
	/* TODO ... */
}

/* Initialize global counters for game updates. */
static void
init_game_globals()
{
	memset(game.player_history_index, '\0', sizeof(game.player_history_index));
	memset(game.player_history_counter, '\0', sizeof(game.player_history_counter));

	game.resource_history_index = 0;
	game.game_tick = 0;
	game.anim = 0;
	/* TODO ... */
	game.game_stats_counter = 0;
	game.history_counter = 0;
	game.anim_diff = 0;
	/* TODO */
}

/* In target, replace any character from needle with replacement character. */
static void
strreplace(char *target, const char *needle, char replace)
{
	for (int i = 0; target[i] != '\0'; i++) {
		for (int j = 0; needle[j] != '\0'; j++) {
			if (needle[j] == target[i]) {
				target[i] = replace;
				break;
			}
		}
	}
}

static int
save_game(int autosave)
{
	int r;

	/* Build filename including time stamp. */
	char name[128];
	time_t t = time(NULL);

	struct tm *tm = localtime(&t);
	if (tm == NULL) return -1;

	if (!autosave) {
		r = strftime(name, sizeof(name), "%c.save", tm);
		if (r == 0) return -1;
	} else {
		r = strftime(name, sizeof(name), "autosave-%c.save", tm);
		if (r == 0) return -1;
	}

	/* Substitute problematic characters. These are problematic
	   particularly on windows platforms, but also in general on FAT
	   filesystems through any platform. */
	/* TODO Possibly use PathCleanupSpec() when building for windows platform. */
	strreplace(name, "\\/:*?\"<>| ", '_');

	FILE *f = fopen(name, "wb");
	if (f == NULL) return -1;

	r = save_text_state(f);
	if (r < 0) return -1;

	fclose(f);

	LOGI("main", "Game saved to `%s'.", name);

	return 0;
}

/* Update global anim counters based on game_tick.
   Note: anim counters control the rate of updates in
   the rest of the game objects (_not_ just gfx animations). */
static void
anim_update_and_more()
{
	/* TODO ... */
	game.old_anim = game.anim;
	game.anim = game.game_tick >> 16;
	game.anim_diff = game.anim - game.old_anim;

	int anim_xor = game.anim ^ game.old_anim;

	/* Viewport animation does not care about low bits in anim */
	if (anim_xor >= 1 << 3) {
		viewport_t *viewport = interface_get_top_viewport(&interface);
		gui_object_set_redraw((gui_object_t *)viewport);
	}

	if ((game.anim & 0xffff) == 0 && game.game_speed > 0) {
		int r = save_game(1);
		if (r < 0) LOGW("main", "Autosave failed.");
	}

	if (BIT_TEST(game.svga, 3)) { /* Game has started */
		/* TODO */

		if (interface.return_timeout < game.anim_diff) {
			interface.msg_flags |= BIT(4);
			interface.msg_flags &= ~BIT(3);
			interface.return_timeout = 0;
		} else {
			interface.return_timeout -= game.anim_diff;
		}
	}
}

/* Update game_tick. */
static void
update_game_tick()
{
	/*game.field_208 += 1;*/
	game.game_tick += game.game_speed;

	/* Update player input: This is done from the SDL main loop instead. */

	/* TODO pcm sounds ... */
}

/* Initialize map parameters from mission number. */
static int
load_map_spec()
{
	/* Only the three first are available for now. */
	const map_spec_t mission[] = {
		{
			/* Mission 1: START */
			.rnd = {{ 0x6d6f, 0xf7f0, 0xc8d4 }},

			.pl_0_supplies = 35,
			.pl_0_reproduction = 30,

			.pl_1_face = 1,
			.pl_1_intelligence = 10,
			.pl_1_supplies = 5,
			.pl_1_reproduction = 30,
		}, {
			/* Mission 2: STATION */
			.rnd = {{ 0x60b9, 0xe728, 0xc484 }},

			.pl_0_supplies = 30,
			.pl_0_reproduction = 40,

			.pl_1_face = 2,
			.pl_1_intelligence = 12,
			.pl_1_supplies = 15,
			.pl_1_reproduction = 30,

			.pl_2_face = 3,
			.pl_2_intelligence = 14,
			.pl_2_supplies = 15,
			.pl_2_reproduction = 30
		}, {
			/* Mission 3: UNITY */
			.rnd = {{ 0x12ab, 0x7a4a, 0xe483 }},

			.pl_0_supplies = 30,
			.pl_0_reproduction = 30,

			.pl_1_face = 2,
			.pl_1_intelligence = 18,
			.pl_1_supplies = 10,
			.pl_1_reproduction = 25,

			.pl_2_face = 4,
			.pl_2_intelligence = 18,
			.pl_2_supplies = 10,
			.pl_2_reproduction = 25
		}
	};

	int m = game.mission_level;

	game.pl_init[0].face = 12;
	game.pl_init[0].supplies = mission[m].pl_0_supplies;
	game.pl_init[0].intelligence = 40;
	game.pl_init[0].reproduction = mission[m].pl_0_reproduction;

	game.pl_init[1].face = mission[m].pl_1_face;
	game.pl_init[1].supplies = mission[m].pl_1_supplies;
	game.pl_init[1].intelligence = mission[m].pl_1_intelligence;
	game.pl_init[1].reproduction = mission[m].pl_1_reproduction;

	game.pl_init[2].face = mission[m].pl_2_face;
	game.pl_init[2].supplies = mission[m].pl_2_supplies;
	game.pl_init[2].intelligence = mission[m].pl_2_intelligence;
	game.pl_init[2].reproduction = mission[m].pl_2_reproduction;

	game.pl_init[3].face = mission[m].pl_3_face;
	game.pl_init[3].supplies = mission[m].pl_3_supplies;
	game.pl_init[3].intelligence = mission[m].pl_3_intelligence;
	game.pl_init[3].reproduction = mission[m].pl_3_reproduction;

	/* TODO ... */

	memcpy(&game.init_map_rnd, &mission[m].rnd,
	       sizeof(random_state_t));

	int map_size = 3;

	game.init_map_rnd.state[0] ^= 0x5a5a;
	game.init_map_rnd.state[1] ^= 0xa5a5;
	game.init_map_rnd.state[2] ^= 0xc3c3;

	return map_size;
}

/* Start a new game. */
static void
start_game()
{
	/* Initialize map */
	game.map_size = load_map_spec();

	game.map.col_size = 5 + game.map_size/2;
	game.map.row_size = 5 + (game.map_size - 1)/2;
	game.map.cols = 1 << game.map.col_size;
	game.map.rows = 1 << game.map.row_size;

	game.split &= ~BIT(2); /* Not split screen */
	game.split &= ~BIT(6); /* Not coop mode */
	game.split &= ~BIT(5); /* Not demo mode */

	game.svga |= BIT(3); /* Game has started. */
	game.game_speed = DEFAULT_GAME_SPEED;

	init_map_vars();
	reset_game_objs();

	init_spiral_pos_pattern();
	map_init();
	map_init_minimap();

	reset_player_settings();

	init_player_settings();
	init_game_globals();
}


/* One iteration of game_loop(). */
static void
game_loop_iter()
{
	/* TODO music and sound effects */

	anim_update_and_more();

	/* TODO ... */

	game_update();

	/* TODO ... */

	interface.flags &= ~BIT(4);
	interface.flags &= ~BIT(7);

	/* TODO */

	/* Undraw cursor */
	sdl_draw_frame(interface.pointer_x-8, interface.pointer_y-8,
		       sdl_get_screen_frame(), 0, 0, &cursor_buffer, 16, 16);

	gui_object_redraw((gui_object_t *)&interface, game.frame);

	/* TODO very crude dirty marking algortihm: mark everything. */
	sdl_mark_dirty(0, 0, sdl_frame_get_width(game.frame),
		       sdl_frame_get_height(game.frame));

	/* ADDITIONS */

	/* Restore cursor buffer */
	sdl_draw_frame(0, 0, &cursor_buffer,
		       interface.pointer_x-8, interface.pointer_y-8,
		       sdl_get_screen_frame(), 16, 16);

	/* Mouse cursor */
	gfx_draw_transp_sprite(interface.pointer_x-8,
			       interface.pointer_y-8,
			       DATA_CURSOR, sdl_get_screen_frame());

	sdl_swap_buffers();
}


/* The length of a game tick in miliseconds. */
#define TICK_LENGTH  20

/* How fast consequtive mouse events need to be generated
   in order to be interpreted as click and double click. */
#define MOUSE_SENSITIVITY  600


void
game_loop_quit()
{
	game_loop_run = 0;
}

/* game_loop() has been turned into a SDL based loop.
   The code for one iteration of the original game_loop is
   in game_loop_iter. */
static void
game_loop()
{
	/* FPS */
	int fps = 0;
	int fps_ema = 0;
	int fps_target = 100;
	const float ema_alpha = 0.003;

	int drag_button = 0;

	unsigned int last_down[3] = {0};
	unsigned int last_click[3] = {0};

	unsigned int current_ticks = SDL_GetTicks();
	unsigned int accum = 0;
	unsigned int accum_frames = 0;

	SDL_Event event;
	gui_event_t ev;

	game_loop_run = 1;
	while (game_loop_run) {
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_MOUSEBUTTONUP:
				if (drag_button == event.button.button) {
					ev.type = GUI_EVENT_TYPE_DRAG_END;
					ev.x = event.button.x;
					ev.y = event.button.y;
					ev.button = drag_button;
					gui_object_handle_event((gui_object_t *)&interface, &ev);

					drag_button = 0;
				}

				ev.type = GUI_EVENT_TYPE_BUTTON_UP;
				ev.x = event.button.x;
				ev.y = event.button.y;
				ev.button = event.button.button;
				gui_object_handle_event((gui_object_t *)&interface, &ev);

				if (event.button.button <= 3 &&
				    current_ticks - last_down[event.button.button-1] < MOUSE_SENSITIVITY) {
					ev.type = GUI_EVENT_TYPE_CLICK;
					ev.x = event.button.x;
					ev.y = event.button.y;
					ev.button = event.button.button;
					gui_object_handle_event((gui_object_t *)&interface, &ev);

					if (current_ticks - last_click[event.button.button-1] < MOUSE_SENSITIVITY) {
						ev.type = GUI_EVENT_TYPE_DBL_CLICK;
						ev.x = event.button.x;
						ev.y = event.button.y;
						ev.button = event.button.button;
						gui_object_handle_event((gui_object_t *)&interface, &ev);
					}

					last_click[event.button.button-1] = current_ticks;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				ev.type = GUI_EVENT_TYPE_BUTTON_DOWN;
				ev.x = event.button.x;
				ev.y = event.button.y;
				ev.button = event.button.button;
				gui_object_handle_event((gui_object_t *)&interface, &ev);

				if (event.button.button <= 3) last_down[event.button.button-1] = current_ticks;
				break;
			case SDL_MOUSEMOTION:
				if (drag_button == 0) {
					/* Move pointer normally. */
					if (event.motion.x != interface.pointer_x || event.motion.y != interface.pointer_y) {
						/* Undraw cursor */
						sdl_draw_frame(interface.pointer_x-8, interface.pointer_y-8,
							       sdl_get_screen_frame(), 0, 0, &cursor_buffer, 16, 16);

						interface.pointer_x = min(max(0, event.motion.x), interface.pointer_x_max);
						interface.pointer_y = min(max(0, event.motion.y), interface.pointer_y_max);

						/* Restore cursor buffer */
						sdl_draw_frame(0, 0, &cursor_buffer,
							       interface.pointer_x-8, interface.pointer_y-8,
							       sdl_get_screen_frame(), 16, 16);
					}
				}

				for (int button = 1; button <= 3; button++) {
					if (event.motion.state & SDL_BUTTON(button)) {
						if (drag_button == 0) {
							drag_button = button;

							ev.type = GUI_EVENT_TYPE_DRAG_START;
							ev.x = event.motion.x;
							ev.y = event.motion.y;
							ev.button = drag_button;
							gui_object_handle_event((gui_object_t *)&interface, &ev);
						}

						ev.type = GUI_EVENT_TYPE_DRAG_MOVE;
						ev.x = event.motion.x;
						ev.y = event.motion.y;
						ev.button = drag_button;
						gui_object_handle_event((gui_object_t *)&interface, &ev);
						break;
					}
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_q &&
				    (event.key.keysym.mod & KMOD_CTRL)) {
					game_loop_quit();
					break;
				}

				switch (event.key.keysym.sym) {
					/* Map scroll */
				case SDLK_UP: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, 0, -1);
				}
					break;
				case SDLK_DOWN: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, 0, 1);
				}
					break;
				case SDLK_LEFT: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, -1, 0);
				}
					break;
				case SDLK_RIGHT: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, 1, 0);
				}
					break;

					/* Panel click shortcuts */
				case SDLK_1: {
					panel_bar_t *panel = interface_get_panel_bar(&interface);
					panel_bar_activate_button(panel, 0);
				}
					break;
				case SDLK_2: {
					panel_bar_t *panel = interface_get_panel_bar(&interface);
					panel_bar_activate_button(panel, 1);
				}
					break;
				case SDLK_3: {
					panel_bar_t *panel = interface_get_panel_bar(&interface);
					panel_bar_activate_button(panel, 2);
				}
					break;
				case SDLK_4: {
					panel_bar_t *panel = interface_get_panel_bar(&interface);
					panel_bar_activate_button(panel, 3);
				}
					break;
				case SDLK_5: {
					panel_bar_t *panel = interface_get_panel_bar(&interface);
					panel_bar_activate_button(panel, 4);
				}
					break;

					/* Game speed */
				case SDLK_PLUS:
				case SDLK_KP_PLUS:
					if (game.game_speed < 0xffff0000) game.game_speed += 0x10000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
					break;
				case SDLK_MINUS:
				case SDLK_KP_MINUS:
					if (game.game_speed >= 0x10000) game.game_speed -= 0x10000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
					break;
				case SDLK_0:
					game.game_speed = 0x20000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
					break;
				case SDLK_p:
					if (game.game_speed == 0) game_pause(0);
					else game_pause(1);
					break;

					/* Audio */
				case SDLK_s:
					sfx_enable(!sfx_is_enabled());
					break;
				case SDLK_m:
					midi_enable(!midi_is_enabled());
					break;

					/* Misc */
				case SDLK_ESCAPE:
					if (BIT_TEST(interface.click, 7)) { /* Building road */
						interface_build_road_end(&interface);
					} else if (interface.clkmap != 0) {
						interface_close_popup(&interface);
					}
					break;

					/* Debug */
				case SDLK_g:
					interface.viewport.layers ^= VIEWPORT_LAYER_GRID;
					break;
				case SDLK_j: {
					int current = 0;
					for (int i = 0; i < 4; i++) {
						if (interface.player == game.player[i]) {
							current = i;
							break;
						}
					}

					for (int i = (current+1) % 4; i != current; i = (i+1) % 4) {
						if (PLAYER_IS_ACTIVE(game.player[i])) {
							interface.player = game.player[i];
							LOGD("main", "Switched to player %i.", i);
							break;
						}
					}
				}
					break;
				case SDLK_z:
					if (event.key.keysym.mod & KMOD_CTRL) {
						save_game(0);
					}
					break;

				default:
					break;
				}
				break;
			case SDL_QUIT:
				game_loop_quit();
				break;
			}
		}

		unsigned int new_ticks = SDL_GetTicks();
		int delta_ticks = new_ticks - current_ticks;
		current_ticks = new_ticks;

		accum += delta_ticks;
		while (accum >= TICK_LENGTH) {
			/* This is main_timer_cb */
			tick += 1;
			/* In original, deep_tree is called which will call update_game.
			   Here, update_game is just called directly. */
			update_game_tick();

			/* FPS */
			fps = 1000*((float)accum_frames / accum);
			if (fps_ema > 0) fps_ema = ema_alpha*fps + (1-ema_alpha)*fps_ema;
			else if (fps > 0) fps_ema = fps;

			accum -= TICK_LENGTH;
			accum_frames = 0;
		}

		game_loop_iter();
#if 0
		draw_green_string(6, 10, sdl_get_screen_frame(), "FPS");
		draw_green_number(10, 10, sdl_get_screen_frame(), fps_ema);
#endif

		accum_frames += 1;

		/* Reduce framerate to target */
		if (fps_target > 0) {
			int delay = 0;
			if (fps_ema > 0) delay = (1000/fps_target) - (1000/fps_ema);
			if (delay > 0) SDL_Delay(delay);
		}
	}
}

static int
load_game(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		LOGE("main", "Unable to open save game file: `%s'.", path);
		return -1;
	}

	int r = load_text_state(f);
	if (r < 0) {
		LOGW("main", "Unable to load save game, trying compatability mode.");

		fseek(f, 0, SEEK_SET);
		r = load_v0_state(f);
		if (r < 0) {
			LOGE("main", "Failed to load save game.");
			fclose(f);
			return -1;
		}
	}

	fclose(f);

	init_spiral_pos_pattern();
	map_init_minimap();

	return 0;
}

static void
load_serf_animation_table()
{
	/* The serf animation table is stored in big endian
	   order in the data file.

	   * The first uint32 is the byte length of the rest
	   of the table (skipped below).
	   * Next is 199 uint32s that are offsets from the start
	   of this table to an animation table (one for each
	   animation).
	   * The animation tables are of varying lengths.
	   Each entry in the animation table is three bytes
	   long. First byte is used to determine the serf body
	   sprite. Second byte is a signed horizontal sprite
	   offset. Third byte is a signed vertical offset.
	*/
	game.serf_animation_table = ((uint32_t *)gfx_get_data_object(DATA_SERF_ANIMATION_TABLE, NULL)) + 1;

	/* Endianess convert from big endian. */
	for (int i = 0; i < 199; i++) {
		game.serf_animation_table[i] = be32toh(game.serf_animation_table[i]);
	}
}

/* Allocate global memory before game starts. */
static void
allocate_global_memory()
{
	/* Players */
	game.player[0] = malloc(sizeof(player_t));
	if (game.player[0] == NULL) abort();

	game.player[1] = malloc(sizeof(player_t));
	if (game.player[1] == NULL) abort();

	game.player[2] = malloc(sizeof(player_t));
	if (game.player[2] == NULL) abort();

	game.player[3] = malloc(sizeof(player_t));
	if (game.player[3] == NULL) abort();

	/* TODO this should be allocated on game start according to the
	   map size of the particular game instance. */
	int max_map_size = 10;
	game.max_serf_cnt = (0x1f84 * (1 << max_map_size) - 4) / 0x81;
	game.max_flg_cnt = (0x2314 * (1 << max_map_size) - 4) / 0x231;
	game.max_building_cnt = (0x54c * (1 << max_map_size) - 4) / 0x91;
	game.max_inventory_cnt = (0x54c * (1 << max_map_size) - 4) / 0x3c1;

	/* Serfs */
	game.serfs = malloc(game.max_serf_cnt * sizeof(serf_t));
	if (game.serfs == NULL) abort();

	game.serfs_bitmap = malloc(((game.max_serf_cnt-1) / 8) + 1);
	if (game.serfs_bitmap == NULL) abort();

	/* Flags */
	game.flgs = malloc(game.max_flg_cnt * sizeof(flag_t));
	if (game.flgs == NULL) abort();

	game.flg_bitmap = malloc(((game.max_flg_cnt-1) / 8) + 1);
	if (game.flg_bitmap == NULL) abort();

	/* Buildings */
	game.buildings = malloc(game.max_building_cnt * sizeof(building_t));
	if (game.buildings == NULL) abort();

	game.buildings_bitmap = malloc(((game.max_building_cnt-1) / 8) + 1);
	if (game.buildings_bitmap == NULL) abort();

	/* Inventories */
	game.inventories = malloc(game.max_inventory_cnt * sizeof(inventory_t));
	if (game.inventories == NULL) abort();

	game.inventories_bitmap = malloc(((game.max_inventory_cnt-1) / 8) + 1);
	if (game.inventories_bitmap == NULL) abort();

	/* Setup screen frame */
	frame_t *screen = sdl_get_screen_frame();
	sdl_frame_init(&screen_frame, 0, 0, sdl_frame_get_width(screen),
		       sdl_frame_get_height(screen), screen);

	/* Setup cursor occlusion buffer */
	sdl_frame_init(&cursor_buffer, 0, 0, 16, 16, NULL);
}

/* Initialize interface configuration. */
static void
init_global_config()
{
	/* TODO load saved configuration */
	game.cfg_left = 0x39;
	game.cfg_right = 0x39;
	audio_set_volume(75);
}

#define MAX_DATA_PATH      1024
#define DEFAULT_DATA_FILE  "SPAE.PA"

/* Load data file from path is non-NULL, otherwise search in
   various standard paths. */
static int
load_data_file(const char *path)
{
	/* Use specified path. If something was specified
	   but not found, this function should fail without
	   looking anywhere else. */
	if (path != NULL) {
		LOGI("main", "Looking for game data in `%s'...", path);
		int r = gfx_load_file(path);
		if (r < 0) return -1;
		return 0;
	}

	/* Use default data file if none was specified. */
	char cp[MAX_DATA_PATH];
	char *env;

	/* Look in home */
	if ((env = getenv("XDG_DATA_HOME")) != NULL &&
	    env[0] != '\0') {
		snprintf(cp, sizeof(cp), "%s/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
#ifdef _WIN32
	} else if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
		snprintf(cp, sizeof(cp),
			 "%s/.local/share/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
#endif
	} else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
		snprintf(cp, sizeof(cp),
			 "%s/.local/share/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
	}

	if (path != NULL) {
		LOGI("main", "Looking for game data in `%s'...", path);
		int r = gfx_load_file(path);
		if (r >= 0) return 0;
	}

	/* TODO look in DATADIR, getenv("XDG_DATA_DIRS") or
	   if not set look in /usr/local/share:/usr/share". */

	/* Look in current directory */
	LOGI("main", "Looking for game data in `%s'...", DEFAULT_DATA_FILE);
	int r = gfx_load_file(DEFAULT_DATA_FILE);
	if (r >= 0) return 0;

	return -1;
}


#define USAGE					\
	"Usage: %s [-g DATA-FILE]\n"
#define HELP							\
	USAGE							\
	" -d NUM\t\tSet debug output level\n"			\
	" -f\t\tFullscreen mode (CTRL-q to exit)\n"		\
	" -g DATA-FILE\tUse specified data file\n"		\
	" -h\t\tShow this help text\n"				\
	" -l FILE\tLoad saved game\n"				\
	" -m MAP\t\tSelect world map (1-3)\n"			\
	" -p\t\tPreserve map bugs of the original game\n"	\
	" -r RES\t\tSet display resolution (e.g. 800x600)\n"	\
	" -t GEN\t\tMap generator (0 or 1)\n"

int
main(int argc, char *argv[])
{
	int r;

	char *data_file = NULL;
	char *save_file = NULL;

	int screen_width = DEFAULT_SCREEN_WIDTH;
	int screen_height = DEFAULT_SCREEN_HEIGHT;
	int fullscreen = 0;
	int game_map = 1;
	int map_generator = 0;
	int preserve_map_bugs = 0;

	int log_level = DEFAULT_LOG_LEVEL;

	int opt;
	while (1) {
		opt = getopt(argc, argv, "d:fg:hl:m:pr:t:");
		if (opt < 0) break;

		switch (opt) {
		case 'd':
		{
			int d = atoi(optarg);
			if (d >= 0 && d < LOG_LEVEL_MAX) {
				log_level = d;
			}
		}
			break;
		case 'f':
			fullscreen = 1;
			break;
		case 'g':
			data_file = malloc(strlen(optarg)+1);
			if (data_file == NULL) exit(EXIT_FAILURE);
			strcpy(data_file, optarg);
			break;
		case 'h':
			fprintf(stdout, HELP, argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			save_file = malloc(strlen(optarg)+1);
			if (save_file == NULL) exit(EXIT_FAILURE);
			strcpy(save_file, optarg);
			break;
		case 'm':
			game_map = atoi(optarg);
			break;
		case 'p':
			preserve_map_bugs = 1;
			break;
		case 'r':
		{
			char *hstr = strchr(optarg, 'x');
			if (hstr == NULL) {
				fprintf(stderr, USAGE, argv[0]);
				exit(EXIT_FAILURE);
			}
			screen_width = atoi(optarg);
			screen_height = atoi(hstr+1);
		}
			break;
		case 't':
			map_generator = atoi(optarg);
			break;
		default:
			fprintf(stderr, USAGE, argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* Set up logging */
	log_set_file(stdout);
	log_set_level(log_level);

	LOGI("main", "freeserf %s", FREESERF_VERSION);

	r = load_data_file(data_file);
	if (r < 0) {
		LOGE("main", "Could not load game data.");
		exit(EXIT_FAILURE);
	}

	free(data_file);

	gfx_data_fixup();

	LOGI("main", "SDL init...");

	r = sdl_init();
	if (r < 0) exit(EXIT_FAILURE);

	/* TODO move to right place */
	midi_play_track(MIDI_TRACK_0);

	/*gfx_set_palette(DATA_PALETTE_INTRO);*/
	gfx_set_palette(DATA_PALETTE_GAME);

	LOGI("main", "SDL resolution %ix%i...", screen_width, screen_height);

	r = sdl_set_resolution(screen_width, screen_height, fullscreen);
	if (r < 0) exit(EXIT_FAILURE);

	game.svga |= BIT(7); /* set svga mode */

	game.mission_level = game_map - 1; /* set game map */
	game.map_generator = map_generator;
	game.map_preserve_bugs = preserve_map_bugs;

	/* Init globals */
	init_global_config();
	allocate_global_memory();
	player_interface_init();

	/* Initialize global lookup tables */
	init_spiral_pattern();
	load_serf_animation_table();

	/* Either load a save game if specified or
	   start a new game. */
	if (save_file != NULL) {
		int r = load_game(save_file);
		if (r < 0) exit(EXIT_FAILURE);
		free(save_file);
	} else {
		start_game();
	}

	/* Move viewport to initial position */
	viewport_move_to_map_pos(&interface.viewport, interface.map_cursor_pos);

	/* Start game loop */
	game_loop();

	LOGI("main", "Cleaning up...");

	/* Clean up */
	audio_cleanup();
	sdl_deinit();
	gfx_unload();

	return EXIT_SUCCESS;
}
