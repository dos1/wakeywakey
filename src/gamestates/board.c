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
#define ROWS 8.0

struct Field {
	int id;
	bool dreamy;
	struct Dream {
		struct Tween displacement, size;
		bool good;
		struct Character* content;
	} dream;
};

struct Player {
	int id;
	int position;
	int selected;
	bool active;
	bool visible;
	struct Tween pos;
	ALLEGRO_BITMAP *standby, *moving, *pawn;
};

struct Goose {
	int id;
	int desired;
	int pos;
	bool moving;
	struct Tween position;
	struct Character* character;
	bool flipped;
};

struct GamestateResources {
	// This struct is for every resource allocated and used by your gamestate.
	// It gets created on load and then gets passed around to all other function calls.

	struct Layers {
		ALLEGRO_BITMAP *bg, *ground, *water, *sky;
		struct Character* fg;
	} layers;

	ALLEGRO_BITMAP* cloud[3];
	ALLEGRO_BITMAP* badcloud[3];
	ALLEGRO_BITMAP* goodcloud[3];

	struct Tween camera;

	bool cameraMove, showMenu, started, cutscene;

	ALLEGRO_BITMAP *logo, *menu;

	struct Mouse {
		float x, y;
	} mouse;

	struct Player players[6];

	struct Goose gooses[3];

	struct Player* currentPlayer;

	bool active;

	bool initial;

	struct Field board[(int)COLS * (int)ROWS];

	struct Timeline* timeline;
};

int Gamestate_ProgressCount = 54; // number of loading steps as reported by Gamestate_Load; 0 when missing

static TM_ACTION(HideMenu) {
	TM_RunningOnly;
	data->showMenu = false;
	data->active = true;
	return true;
}

static void ScrollCamera(struct Game* game, struct GamestateResources* data) {
	data->cameraMove = true;
	float cam = GetTweenValue(&data->camera);
	float pos = data->currentPlayer->position;
	if (pos > COLS * ROWS / 2.0) {
		pos += COLS;
	} else {
		pos -= COLS;
	}
	pos /= COLS * ROWS;
	/*	if (pos < 0.33333) {
		pos = 0;
	}
	if (pos >= 0.75) {
		pos = 1;
	}*/
	PrintConsole(game, "before %f", pos);
	pos = round(pos * 2) / 2.0;
	PrintConsole(game, "after %f", pos);
	data->camera = Tween(game, cam, 1.0 - pos, 2.5, TWEEN_STYLE_QUARTIC_IN_OUT);
}

static void PerformSleeping(struct Game* game, struct GamestateResources* data);

static void DoStartGame(struct Game* game, struct GamestateResources* data) {
	data->active = true;
	data->started = true;
	//data->showMenu = false;
	ScrollCamera(game, data);
}

static void NextTurn(struct Game* game, struct GamestateResources* data) {
	int id = data->currentPlayer->id;
	do {
		id++;
		id = id % 6;
	} while (!data->players[id].active);
	data->currentPlayer = &data->players[id];
	ScrollCamera(game, data);
	data->active = true;
}

static void EndTura(struct Game* game, struct Tween* tween, void* d) {
	struct GamestateResources* data = d;
	data->active = true;
	data->currentPlayer->position = data->currentPlayer->selected;
	data->currentPlayer->selected++;
	data->currentPlayer->pos = Tween(game, 0.0, 0.0, 0.0, TWEEN_STYLE_LINEAR);
	if (data->currentPlayer->id == 3) {
		PerformSleeping(game, data);
	} else {
		NextTurn(game, data);
	}
}

static TM_ACTION(StartGame) {
	TM_RunningOnly;
	data->cutscene = false;
	DoStartGame(game, data);
	return true;
}

static TM_ACTION(StartTurn) {
	TM_RunningOnly;
	data->cutscene = false;
	NextTurn(game, data);
	return true;
}

static TM_ACTION(ScrollCamToBottom) {
	switch (action->state) {
		case TM_ACTIONSTATE_START:
			data->camera = Tween(game, GetTweenValue(&data->camera), 0.0, 3.0, TWEEN_STYLE_QUINTIC_OUT);
			data->cameraMove = true;
			return false;
		default:
			return GetTweenPosition(&data->camera) >= 0.8;
	}
}

static void GoToSleep(struct Game* game, struct Tween* tween, void* d) {
	SelectSpritesheet(game, d, "buch");
}

static TM_ACTION(WakeUp) {
	TM_RunningOnly;
	for (int i = 0; i < 3; i++) {
		SelectSpritesheet(game, data->gooses[i].character, "wakeup");
		do {
			data->gooses[i].desired = rand() % (int)COLS;
		} while (data->gooses[i].desired == data->gooses[i].pos);
		data->gooses[i].position = Tween(game, data->gooses[i].pos, data->gooses[i].desired, 1.0 * abs(data->gooses[i].desired - data->gooses[i].pos) * (0.9 + i * 0.1), TWEEN_STYLE_LINEAR);
		data->gooses[i].position.callback = GoToSleep;
		data->gooses[i].position.data = data->gooses[i].character;
	}
	return true;
}

static TM_ACTION(WaitForGeeseToSettle) {
	switch (action->state) {
		case TM_ACTIONSTATE_START:
			for (int i = 0; i < 3; i++) {
				data->gooses[i].flipped = data->gooses[i].desired < data->gooses[i].pos;
			}
			data->gooses[1].flipped = !data->gooses[1].flipped;
			return false;
		case TM_ACTIONSTATE_RUNNING: { // C switch sucks
			bool finished = true;
			for (int i = 0; i < 3; i++) {
				UpdateTween(&data->gooses[i].position, action->delta);
				// TODO: add IsTweenFinished
				if (GetTweenPosition(&data->gooses[i].position) < 1.0) {
					finished = false;
				}
			}
			return finished;
		}
		case TM_ACTIONSTATE_DESTROY:
			for (int i = 0; i < 3; i++) {
				//SelectSpritesheet(game, data->gooses[i].character, "buch");
				data->gooses[i].pos = data->gooses[i].desired;
			}
		default:
			return false;
	}
}

static TM_ACTION(Snort) {
	switch (action->state) {
		case TM_ACTIONSTATE_START:
			for (int i = 0; i < 3; i++) {
				int pos = ((int)ROWS - 1) * (int)COLS + (COLS - 1 - data->gooses[i].pos);
				data->board[pos].dreamy = true;
				data->board[pos].dream.good = rand() % 2;
				data->board[pos].dream.displacement = Tween(game, 0.0, 0.0, 0.0, TWEEN_STYLE_LINEAR); // TODO: add StaticTween helper
				data->board[pos].dream.size = Tween(game, 0.0, 1.0, 2.0, TWEEN_STYLE_ELASTIC_OUT);
				// TODO: data->board[pos].dream.content
			}
			return false;
		case TM_ACTIONSTATE_RUNNING: {
			bool finished = true;
			for (int i = 0; i < 3; i++) {
				int pos = ((int)ROWS - 1) * (int)COLS + (COLS - 1 - data->gooses[i].pos);
				UpdateTween(&data->board[pos].dream.size, action->delta);
				if (GetTweenPosition(&data->board[pos].dream.size) < 1.0) {
					finished = false;
				}
			}
			return finished;
		}
		default:
			return false;
	}
}

static TM_ACTION(MoveDreamsUp) {
	switch (action->state) {
		case TM_ACTIONSTATE_START:
			for (int i = 0; i < COLS * ROWS; i++) {
				if (!data->board[i].dreamy) {
					continue;
				}
				data->board[i].dream.displacement = Tween(game, 0.0, 1.0, 2.0, TWEEN_STYLE_SINE_IN_OUT);
			}
			return false;
		case TM_ACTIONSTATE_RUNNING: {
			bool finished = true;
			for (int i = 0; i < COLS * ROWS; i++) {
				if (!data->board[i].dreamy) {
					continue;
				}
				UpdateTween(&data->board[i].dream.displacement, action->delta);
				if (GetTweenPosition(&data->board[i].dream.displacement) < 1.0) {
					finished = false;
				}
			}
			return finished;
		}
		case TM_ACTIONSTATE_DESTROY:
			for (int i = 0; i < COLS * ROWS; i++) {
				if (!data->board[i].dreamy) {
					continue;
				}
				if (i < COLS) {
					data->board[i].dreamy = false;
				} else {
					int x = i % (int)COLS;
					//int y = i / (int)COLS;
					int diff = x * 2 + 1;
					//if (y % 2) {
					//diff = ((COLS - 1) - x) * 2 + 1;
					//}
					data->board[i - diff].dream = data->board[i].dream;
					data->board[i - diff].dreamy = data->board[i].dreamy;
					data->board[i - diff].dream.displacement = Tween(game, 0.0, 0.0, 0.0, TWEEN_STYLE_LINEAR);
					data->board[i].dreamy = false;
				}
			}
			return false;
		default:
			return false;
	}
}

static void PerformSleeping(struct Game* game, struct GamestateResources* data) {
	data->active = false;
	data->cutscene = true;
	TM_CleanQueue(data->timeline);
	TM_CleanBackgroundQueue(data->timeline);
	TM_AddAction(data->timeline, ScrollCamToBottom, NULL);
	TM_AddAction(data->timeline, WakeUp, NULL);
	TM_AddDelay(data->timeline, 1000);
	TM_AddAction(data->timeline, WaitForGeeseToSettle, NULL);

	TM_AddDelay(data->timeline, 500);
	TM_AddQueuedBackgroundAction(data->timeline, HideMenu, NULL, 500);
	TM_AddAction(data->timeline, Snort, NULL);
	//TM_AddDelay(data->timeline, 2000);
	TM_AddAction(data->timeline, MoveDreamsUp, NULL);
	TM_AddDelay(data->timeline, 500);

	if (!data->started) {
		TM_AddAction(data->timeline, StartGame, NULL);
	} else {
		TM_AddAction(data->timeline, StartTurn, NULL);
	}
}

void Gamestate_Logic(struct Game* game, struct GamestateResources* data, double delta) {
	// Here you should do all your game logic as if <delta> seconds have passed.
	TM_Process(data->timeline, delta);
	if (data->cameraMove) {
		UpdateTween(&data->camera, delta);
		if (GetTweenPosition(&data->camera) >= 1.0) {
			data->cameraMove = false;
		}
	}
	if (data->initial) {
		data->camera.start = 1.0 - sin(al_get_time()) * 0.01;
		data->camera.stop = data->camera.start;
	}

	for (int i = 0; i < 3; i++) {
		AnimateCharacter(game, data->gooses[i].character, delta, 0.9 + 0.1 * i);
	}
	if (!data->active) {
		UpdateTween(&data->currentPlayer->pos, delta);
	}
	AnimateCharacter(game, data->layers.fg, delta, 1.0);
}

void Gamestate_Draw(struct Game* game, struct GamestateResources* data) {
	// Draw everything to the screen here.

	float scroll = GetTweenValue(&data->camera);

	al_clear_to_color(al_map_rgb(255, 255, 255));
	al_draw_bitmap(data->layers.sky, 0, -(1.0 - scroll) * 100, 0);
	float water = 300 + 1080 * scroll * 1.05;
	al_draw_bitmap(data->layers.water, 5624 * Fract(al_get_time() / 92.0), water, 0);
	al_draw_bitmap(data->layers.water, 5624 * Fract(al_get_time() / 92.0) - 5624, water, 0);
	al_draw_bitmap(data->layers.bg, 0, -300 + scroll * 1320, 0);
	al_draw_bitmap(data->layers.ground, 0, -1080 + 1080 * scroll * 0.95 + 1662, 0);

	// TODO: transforms are pretty cumbersome to use and restore; add some utils for them?

	ALLEGRO_TRANSFORM transform, orig = *al_get_current_transform();
	al_identity_transform(&transform);
	al_translate_transform(&transform, 0, -1080 + scroll * 1080);
	al_compose_transform(&transform, &orig);
	al_use_transform(&transform);

	for (int i = 0; i < 2; i++) {
		float x = GetTweenValue(&data->gooses[i].position);
		SetCharacterPosition(game, data->gooses[i].character, 240 * x + 340, 1820 + i * 50, 0);
		data->gooses[i].character->flipX = data->gooses[i].flipped;
		DrawCharacter(game, data->gooses[i].character);
	}

	al_use_transform(&orig);
	SetCharacterPosition(game, data->layers.fg, 0, -1080 + 1080 * scroll * 0.95, 0);
	DrawCharacter(game, data->layers.fg);
	al_use_transform(&transform);

	//data->showMenu = false;

	for (int i = 2; i < 3; i++) {
		float x = GetTweenValue(&data->gooses[i].position);
		SetCharacterPosition(game, data->gooses[i].character, 240 * x + 340, 1820 + i * 50, 0);
		data->gooses[i].character->flipX = data->gooses[i].flipped;
		DrawCharacter(game, data->gooses[i].character);
	}

	if (data->showMenu) {
		DrawCentered(data->logo, 1920 / 2.0, 1080 * 0.45 + cos(al_get_time() * 1.3424 + 0.23246) * 20, 0);
		DrawCenteredScaled(data->menu, 1920 / 2.0, 1080 * 0.8 + sin(al_get_time()) * 20, 0.5, 0.5, 0);
	}

	for (int j = 0; j < ROWS; j++) {
		for (int i = 0; i < COLS; i++) {
			int num = j * (int)COLS + i;
			if (j % 2) {
				num = j * (int)COLS + ((int)COLS - i) - 1;
			}

			float highlighted = 0.0;
			if (data->currentPlayer->position == num) {
				highlighted = 1.0;
			}
			if (data->currentPlayer->selected == num) {
				highlighted = 0.75;
			}
			if (!data->active) {
				highlighted = 0;
			}

			int frame = highlighted ? (floor(fmod(al_get_time() * 3, 3))) : (num % 3);

			float s = sin(al_get_time() * (0.5 + (0.1 * num)) * 0.25) * 10;

			if (j < ROWS - 1) {
				if (!data->showMenu) {
					DrawCenteredTintedScaled(data->cloud[frame], al_premul_rgba(255, 255, 255, 96 + highlighted * (255 - 96)), (i + 1.5) * 1920 / (COLS + 2) + 5, (j + 1.5) * 2160 / (ROWS + 2) + 3 + s, 0.666, 0.666, 0);
				}
			}

			if (data->board[num].dreamy) {
				frame = floor(fmod(al_get_time() * 3 + num, 3));
				DrawCenteredScaled(data->board[num].dream.good ? data->goodcloud[frame] : data->badcloud[frame], (i + 1.5) * 1920 / (COLS + 2) + 5, (j + 1.5 - GetTweenValue(&data->board[num].dream.displacement)) * 2160 / (ROWS + 2) + 3, 0.555 * GetTweenValue(&data->board[num].dream.size), 0.555 * GetTweenValue(&data->board[num].dream.size), 0);
			}

			//al_draw_tinted_scaled_bitmap(data->cloud[num % 3], al_premul_rgba(255, 255, 255, data->board[num].bird ? 255 : 64), (i + 1) * 1920 / (COLS + 2) + 5 - 80, (j + 1) * 2160 / (ROWS + 2) + 3 - 20, 0);

			//al_draw_rounded_rectangle((i + 1) * 1920 / (COLS + 2) + 5, (j + 1) * 2160 / (ROWS + 2) + 3, (i + 2) * 1920 / (COLS + 2) - 5, (j + 2) * 2160 / (ROWS + 2) - 3, 20, 20, al_premul_rgba(0, 0, 0, 64), 2);

			//al_draw_filled_rounded_rectangle((i + 1) * 1920 / (COLS + 2) + 5, (j + 1) * 2160 / (ROWS + 2) + 3, (i + 2) * 1920 / (COLS + 2) - 5, (j + 2) * 2160 / (ROWS + 2) - 3, 20, 20, al_premul_rgba(255, 255, 255, data->board[num].bird ? 255 : 64));
		}
	}

	for (int p = 0; p < 6; p++) {
		struct Player* player = &data->players[p];

		if (!player->active) {
			continue;
		}

		int i = player->position % (int)COLS;
		int j = player->position / (int)COLS;
		if (j % 2) {
			i = COLS - i - 1;
		}

		float x = (i + 1.5) * 1920 / (COLS + 2) - 65 + p * 40;
		float y = sin(p) * 20 + (j + 1.5) * 2160 / (ROWS + 2) - 20;

		if (data->currentPlayer == player) {
			y -= 30;
			y += sin(al_get_time()) * 15;
		}

		int i2 = player->selected % (int)COLS;
		int j2 = player->selected / (int)COLS;
		if (j2 % 2) {
			i2 = COLS - i2 - 1;
		}

		float x2 = (i2 + 1.5) * 1920 / (COLS + 2) - 65 + p * 40;
		float y2 = sin(p) * 20 + (j2 + 1.5) * 2160 / (ROWS + 2) - 20;

		if (data->currentPlayer == player) {
			y2 -= 30;
			y2 += sin(al_get_time()) * 15;
		}

		x = x + (x2 - x) * GetTweenValue(&player->pos);
		y = y + (y2 - y) * GetTweenValue(&player->pos);

		bool flip = j % 2;
		//if (i == (int)COLS - 1) {
		if (player == data->currentPlayer) {
			flip = (i2 < i);
			if (i == (int)COLS - 1) {
				flip = true;
			}
		}
		//}

		if (!data->showMenu) {
			DrawCenteredScaled(data->currentPlayer == player ? player->moving : player->standby, x, y, 0.25, 0.25, flip ? ALLEGRO_FLIP_HORIZONTAL : 0);
		}
	}

	al_use_transform(&orig);

	if (data->started && !data->cutscene) {
		DrawCenteredScaled(data->currentPlayer->pawn, 1920 - 120, 100, 0.5, 0.5, 0);
	}
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
		if (ev->keyboard.keycode == ALLEGRO_KEY_SPACE) {
			data->initial = false;
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_SPACE && !data->started) {
			PerformSleeping(game, data);
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_SPACE && data->started) {
			if (data->active) {
				data->active = false;
				data->currentPlayer->pos = Tween(game, 0.0, 1.0, 1.25, TWEEN_STYLE_BACK_IN_OUT); // TODO: move duration and style around
				data->currentPlayer->pos.callback = EndTura;
				data->currentPlayer->pos.data = data;
			}
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_ENTER && game->config.debug) {
			StopCurrentGamestate(game);
			StartGamestate(game, "board");
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_S && game->config.debug) {
			DoStartGame(game, data);
		}
		if (ev->keyboard.keycode == ALLEGRO_KEY_BACKSPACE && game->config.debug) {
			data->cameraMove = true;
			data->camera = Tween(game, 0.0, 1.0, 3.0, TWEEN_STYLE_QUARTIC_IN_OUT);
		}
		if ((ev->keyboard.keycode == ALLEGRO_KEY_LEFT) || (ev->keyboard.keycode == ALLEGRO_KEY_RIGHT)) {
			//data->board[data->players[0].position].bird = false;
			//data->players[0].position--;
			//data->board[data->players[0].position].bird = true;

			if (!data->active) {
				return;
			}

			if (data->currentPlayer->selected == data->currentPlayer->position + 1) {
				data->currentPlayer->selected++;
			} else {
				data->currentPlayer->selected--;
			}

			//ScrollCamera(game, data);
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
	data->layers.fg = CreateCharacter(game, "fg");
	RegisterSpritesheet(game, data->layers.fg, "shine");
	RegisterSpritesheet(game, data->layers.fg, "stand");
	LoadSpritesheets(game, data->layers.fg, progress);
	data->layers.ground = al_load_bitmap(GetDataFilePath(game, "trawka.png"));
	progress(game);
	data->layers.sky = al_load_bitmap(GetDataFilePath(game, "sky.png"));
	progress(game);
	data->layers.water = al_load_bitmap(GetDataFilePath(game, "water.png"));
	progress(game);
	data->logo = al_load_bitmap(GetDataFilePath(game, "logo.png"));
	progress(game);
	data->menu = al_load_bitmap(GetDataFilePath(game, "menu.png"));
	progress(game);

	for (int i = 0; i < 6; i++) {
		data->players[i].id = i;
		data->players[i].standby = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "pliszka_standbyX.png", 'X', i + 1)));
		progress(game);
		data->players[i].moving = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "pliszka_w_locieX.png", 'X', i + 1)));
		progress(game);
		data->players[i].pawn = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "czapeczka_kolorX.png", 'X', i + 1)));
		progress(game);
	}

	for (int i = 0; i < 3; i++) {
		data->cloud[i] = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "chmurka_z_cieniemX.png", 'X', i + 1)));
		progress(game);
		data->badcloud[i] = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "chmurka_czerwonaX.png", 'X', i + 1)));
		progress(game);
		data->goodcloud[i] = al_load_bitmap(GetDataFilePath(game, PunchNumber(game, "chmurka_zielonaX.png", 'X', i + 1)));
		progress(game);
	}

	for (int i = 0; i < 3; i++) {
		data->gooses[i].character = CreateCharacter(game, PunchNumber(game, "gesX", 'X', i + 1));
		RegisterSpritesheet(game, data->gooses[i].character, "quack");
		RegisterSpritesheet(game, data->gooses[i].character, "sleep");
		RegisterSpritesheet(game, data->gooses[i].character, "stand");
		RegisterSpritesheet(game, data->gooses[i].character, "wakeup");
		RegisterSpritesheet(game, data->gooses[i].character, "walk");
		RegisterSpritesheet(game, data->gooses[i].character, "buch");
		LoadSpritesheets(game, data->gooses[i].character, progress);
	}

	data->timeline = TM_Init(game, data, "rounds");

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
	data->camera = Tween(game, 1.0, 1.0, 0.0, TWEEN_STYLE_LINEAR);
	data->cameraMove = false;
	data->cutscene = false;
	data->showMenu = true;
	data->started = false;
	data->initial = true;

	for (int i = 0; i < 3; i++) {
		SelectSpritesheet(game, data->gooses[i].character, "sleep");
		SetCharacterPosition(game, data->gooses[i].character, 300, 1900, 0);
		data->gooses[i].pos = rand() % (int)COLS;
		data->gooses[i].desired = data->gooses[i].pos;
		data->gooses[i].position = Tween(game, data->gooses[i].pos, data->gooses[i].pos, 0.0, TWEEN_STYLE_LINEAR);
	}

	data->currentPlayer = &data->players[0];

	for (int i = 0; i < 6; i++) {
		data->players[i].id = i;
		data->players[i].position = 0;
		data->players[i].selected = 1;
		data->players[i].pos = Tween(game, 0.0, 0.0, 0.0, TWEEN_STYLE_LINEAR);
	}

	for (int i = 0; i < 4; i++) {
		data->players[i].active = true;
	}
	for (int i = 4; i < 6; i++) {
		data->players[i].active = false;
	}
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
