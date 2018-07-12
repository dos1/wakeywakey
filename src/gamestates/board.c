/*! \file empty.c
 *  \brief Empty gamestate.
 */
/*
 * Copyright (c) Sebastian Krzyszkowiak <dos@dosowisko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../common.h"
#include <libsuperderpy.h>

#define COLS 6.0
#define ROWS 12.0

struct Field {
	bool bird;
	bool cloud;
};

struct Player {
	int position;
	bool active;
	bool visible;
};

struct Cloud {
	int position;
};

struct Goose {
	int position;
};

struct GamestateResources {
	// This struct is for every resource allocated and used by your gamestate.
	// It gets created on load and then gets passed around to all other function calls.

	struct Layers {
		ALLEGRO_BITMAP *bg, *fg, *water, *sky;
	} layers;

	struct Tween camera;

	bool cameraMove, showMenu, started;

	ALLEGRO_BITMAP *logo, *menu;

	struct Mouse {
		float x, y;
	} mouse;

	struct Player players[4];

	struct Player* currentPlayer;

	struct Field board[(int)COLS * (int)ROWS];
};

int Gamestate_ProgressCount = 6; // number of loading steps as reported by Gamestate_Load; 0 when missing

static void HideMenu(struct Game* game, struct Tween* tween, void* d) {
	struct GamestateResources* data = d;
	data->showMenu = false;
}

static void ScrollCamera(struct Game* game, struct GamestateResources* data) {
	data->cameraMove = true;
	float cam = GetTweenValue(&data->camera);
	float pos = data->currentPlayer->position;
	pos /= COLS * ROWS;
	data->camera = Tween(game, cam, 1.0 - pos, 2.5, TWEEN_STYLE_QUARTIC_IN_OUT);
}

void Gamestate_Logic(struct Game* game, struct GamestateResources* data, double delta) {
	// Here you should do all your game logic as if <delta> seconds have passed.
	if (data->cameraMove) {
		UpdateTween(&data->camera, delta);
		if (GetTweenPosition(&data->camera) >= 1.0) {
			data->cameraMove = false;
		}
	}
}

void Gamestate_Draw(struct Game* game, struct GamestateResources* data) {
	// Draw everything to the screen here.

	float y = GetTweenValue(&data->camera);

	al_draw_bitmap(data->layers.sky, 0, -1080 + 1080 * y * 0.1, 0);
	al_draw_bitmap(data->layers.water, 1920 * Fract(al_get_time() / 6.0), 1439 + -1080 + 1080 * y * 1.2, 0);
	al_draw_bitmap(data->layers.water, 1920 * Fract(al_get_time() / 6.0) - 1920, 1439 + -1080 + 1080 * y * 1.2, 0);
	al_draw_bitmap(data->layers.bg, 0, 943 + -1080 + 1080 * y * 1.1, 0);

	// TODO: transforms are pretty cumbersome to use and restore; add some utils for them?

	ALLEGRO_TRANSFORM transform, orig = *al_get_current_transform();
	al_identity_transform(&transform);
	al_translate_transform(&transform, 0, -1080 + y * 1080);
	al_compose_transform(&transform, &orig);
	al_use_transform(&transform);

	if (data->showMenu) {
		al_draw_bitmap(data->logo, 0, 0, 0);
		al_draw_bitmap(data->menu, 565, 574, 0);
	} else {
		for (int j = 0; j < ROWS; j++) {
			for (int i = 0; i < COLS; i++) {
				int num = j * (int)COLS + i;
				if (j % 2) {
					num = j * (int)COLS + ((int)COLS - i) - 1;
				}
				al_draw_filled_rounded_rectangle((i + 1) * 1920 / (COLS + 2) + 5, (j + 1) * 2160 / (ROWS + 2) + 3, (i + 2) * 1920 / (COLS + 2) - 5, (j + 2) * 2160 / (ROWS + 2) - 3, 20, 20, al_premul_rgba(0, 255, 255, data->board[num].bird ? 255 : 64));
			}
		}
	}

	al_use_transform(&orig);
	al_draw_bitmap(data->layers.fg, 0, 423 + -1080 + 1080 * y * 1.5, 0);
}

void Gamestate_ProcessEvent(struct Game* game, struct GamestateResources* data, ALLEGRO_EVENT* ev) {
	// Called for each event in Allegro event queue.
	// Here you can handle user input, expiring timers etc.
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE)) {
		UnloadCurrentGamestate(game); // mark this gamestate to be stopped and unloaded
		// When there are no active gamestates, the engine will quit.
	}

	if (ev->type == ALLEGRO_EVENT_MOUSE_AXES) {
		data->mouse.x = Clamp(0, 1, (ev->mouse.x - game->_priv.clip_rect.x) / (double)game->_priv.clip_rect.w);
		data->mouse.y = Clamp(0, 1, (ev->mouse.y - game->_priv.clip_rect.y) / (double)game->_priv.clip_rect.h);
	}

	if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
		if (ev->keyboard.keycode == ALLEGRO_KEY_SPACE && !data->started) {
			data->cameraMove = true;
			data->camera.callback = HideMenu;
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_ENTER && game->config.debug) {
			StopCurrentGamestate(game);
			StartGamestate(game, "board");
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_BACKSPACE && game->config.debug) {
			data->cameraMove = true;
			data->camera = Tween(game, 0.0, 1.0, 3.0, TWEEN_STYLE_QUARTIC_IN_OUT);
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_LEFT && game->config.debug) {
			data->board[data->players[0].position].bird = false;
			data->players[0].position--;
			data->board[data->players[0].position].bird = true;
			ScrollCamera(game, data);
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_RIGHT && game->config.debug) {
			data->board[data->players[0].position].bird = false;
			data->players[0].position++;
			data->board[data->players[0].position].bird = true;
			ScrollCamera(game, data);
		}
	}
}

void* Gamestate_Load(struct Game* game, void (*progress)(struct Game*)) {
	// Called once, when the gamestate library is being loaded.
	// Good place for allocating memory, loading bitmaps etc.
	//
	// NOTE: There's no OpenGL context available here. If you want to prerender something,
	// create VBOs, etc. do it in Gamestate_PostLoad.

	struct GamestateResources* data = calloc(1, sizeof(struct GamestateResources));
	progress(game); // report that we progressed with the loading, so the engine can move a progress bar

	data->layers.bg = al_load_bitmap(GetDataFilePath(game, "bg.png"));
	progress(game);
	data->layers.fg = al_load_bitmap(GetDataFilePath(game, "fg.png"));
	progress(game);
	data->layers.sky = al_load_bitmap(GetDataFilePath(game, "sky.png"));
	progress(game);
	data->layers.water = al_load_bitmap(GetDataFilePath(game, "water.png"));
	progress(game);
	data->logo = al_load_bitmap(GetDataFilePath(game, "logo.png"));
	progress(game);
	data->menu = al_load_bitmap(GetDataFilePath(game, "menu.png"));

	return data;
}

void Gamestate_Unload(struct Game* game, struct GamestateResources* data) {
	// Called when the gamestate library is being unloaded.
	// Good place for freeing all allocated memory and resources.
	free(data);
}

void Gamestate_Start(struct Game* game, struct GamestateResources* data) {
	// Called when this gamestate gets control. Good place for initializing state,
	// playing music etc.
	data->camera = Tween(game, 1.0, 0.0, 3.0, TWEEN_STYLE_QUINTIC_OUT);
	data->camera.data = data;
	data->cameraMove = false;
	data->showMenu = true;
	data->started = false;

	data->board[0].bird = true;
	data->players[0].position = 0;

	data->currentPlayer = &data->players[0];
}

void Gamestate_Stop(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets stopped. Stop timers, music etc. here.
}

// Optional endpoints:

void Gamestate_PostLoad(struct Game* game, struct GamestateResources* data) {
	// This is called in the main thread after Gamestate_Load has ended.
	// Use it to prerender bitmaps, create VBOs, etc.
}

void Gamestate_Pause(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets paused (so only Draw is being called, no Logic nor ProcessEvent)
	// Pause your timers and/or sounds here.
}

void Gamestate_Resume(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets resumed. Resume your timers and/or sounds here.
}

void Gamestate_Reload(struct Game* game, struct GamestateResources* data) {
	// Called when the display gets lost and not preserved bitmaps need to be recreated.
	// Unless you want to support mobile platforms, you should be able to ignore it.
}
