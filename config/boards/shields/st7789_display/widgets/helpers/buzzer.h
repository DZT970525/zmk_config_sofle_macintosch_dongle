/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

typedef enum {
	beep,
	funkytown,
	mario,
	megalovania,
	oneup,
	coin,
	reversed_coin,
	coin_polyphonic,
	crazy_coin,
	theme_change_song,
	connected_song,
	disconnected_song,
	error_song,
	notification_song,
	startup_song,
	powerd_down_song,
	snake_game_intro,
	snake_eat_sound,
	no_song,
} SongName;

int app_buzzer_init(void);
void play_once(SongName song_name);

void play_theme_change_song(void);
void play_connected_song(void);
void play_disconnected_song(void);
void play_error_song(void);
void play_notification_song(void);
void play_startup_song(void);
void play_powerd_down_song(void);
void play_snake_eat_sound(void);
void play_snake_game_intro(void);