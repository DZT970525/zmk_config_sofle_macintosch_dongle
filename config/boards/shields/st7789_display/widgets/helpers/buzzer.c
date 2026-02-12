/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_buzzer, LOG_LEVEL_DBG);

#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include "settings.h"
#include "buzzer.h"
#include "pwm.h"

#define BUZZER_STACK 1024

#define FUNKYTOWN_NOTES 13
#define MARIO_NOTES	37
#define GOLIOTH_NOTES 21
#define MEGALOVANIA_NOTES 22
#define ONEUP_NOTES 6
#define COIN_NOTES 2
#define BEEP_NOTES 1
#define CRAZY_COIN_NOTES 48


#ifdef CONFIG_USE_BUZZER

static const struct pwm_dt_spec sBuzzer = PWM_DT_SPEC_GET(DT_CHOSEN(zephyr_buzzer));

SongName song = no_song;

static Sound coin_song[COIN_NOTES] = {
    {.note = B6, .duration = EIGTH},
    {.note = E7, .duration = HALF},
};

static Sound crazy_coin_song[CRAZY_COIN_NOTES] = {
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},
    {.note = B5, .duration = SIXTY_FOURTH},
    {.note = B6, .duration = SIXTY_FOURTH},

    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
    {.note = E6, .duration = SIXTY_FOURTH},
    {.note = E7, .duration = SIXTY_FOURTH},
};

static Sound reversed_coin_song[COIN_NOTES] = {
    {.note = E7, .duration = EIGTH},
    {.note = B6, .duration = HALF},
};

static Sound beep_song[BEEP_NOTES] = {
    {.note = 1000, .duration = 100},
};

static Sound oneup_song[ONEUP_NOTES] = {
    {.note = E5, .duration = DOTTED_EIGTH},
    {.note = G5, .duration = DOTTED_EIGTH},
    {.note = E6, .duration = DOTTED_EIGTH},
    {.note = C6, .duration = DOTTED_EIGTH},
    {.note = D6, .duration = DOTTED_EIGTH},
    {.note = G6, .duration = HALF},
};

static Sound megalovania_song[MEGALOVANIA_NOTES] = {
    {.note = D4, .duration = EIGTH},
    {.note = D4, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},
    {.note = D5, .duration = SIXTEENTH + EIGTH}, // dotted EIGTH
    {.note = REST, .duration = SIXTEENTH},
    {.note = Db5, .duration = SIXTEENTH},
    {.note = C5, .duration = SIXTEENTH},
    {.note = Db5, .duration = SIXTEENTH},
    {.note = C5, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},

    // Repeat the same pattern once
    {.note = D4, .duration = EIGTH},
    {.note = D4, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},
    {.note = D5, .duration = SIXTEENTH + EIGTH}, // dotted EIGTH
    {.note = REST, .duration = SIXTEENTH},
    {.note = Db5, .duration = SIXTEENTH},
    {.note = C5, .duration = SIXTEENTH},
    {.note = Db5, .duration = SIXTEENTH},
    {.note = C5, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},
    {.note = A4, .duration = EIGTH},
};

static Sound funkytown_song[FUNKYTOWN_NOTES] = {
	{.note = C5, .duration = QUARTER},
	{.note = REST, .duration = EIGTH},
	{.note = C5, .duration = QUARTER},
	{.note = Bb4, .duration = QUARTER},
	{.note = C5, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = G4, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = G4, .duration = QUARTER},
	{.note = C5, .duration = QUARTER},
	{.note = F5, .duration = QUARTER},
	{.note = E5, .duration = QUARTER},
	{.note = C5, .duration = QUARTER}};

static Sound mario_song[MARIO_NOTES] = {
	{.note = E6, .duration = QUARTER},
	{.note = REST, .duration = EIGTH},
	{.note = E6, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = E6, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = C6, .duration = QUARTER},
	{.note = E6, .duration = HALF},
	{.note = G6, .duration = HALF},
	{.note = REST, .duration = QUARTER},
	{.note = G4, .duration = WHOLE},
	{.note = REST, .duration = WHOLE},
	/* break in sound */
	{.note = C6, .duration = HALF},
	{.note = REST, .duration = QUARTER},
	{.note = G5, .duration = HALF},
	{.note = REST, .duration = QUARTER},
	{.note = E5, .duration = HALF},
	{.note = REST, .duration = QUARTER},
	{.note = A5, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = B5, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = Bb5, .duration = QUARTER},
	{.note = A5, .duration = HALF},
	{.note = G5, .duration = QUARTER},
	{.note = E6, .duration = QUARTER},
	{.note = G6, .duration = QUARTER},
	{.note = A6, .duration = HALF},
	{.note = F6, .duration = QUARTER},
	{.note = G6, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = E6, .duration = QUARTER},
	{.note = REST, .duration = QUARTER},
	{.note = C6, .duration = QUARTER},
	{.note = D6, .duration = QUARTER},
	{.note = B5, .duration = QUARTER}};

	

/* Thread plays song on buzzer */

K_SEM_DEFINE(buzzer_initialized_sem, 0, 1); /* Wait until buzzer is ready */

extern void buzzer_thread(void *d0, void *d1, void *d2)
{
	/* Block until buzzer is available */
	k_sem_take(&buzzer_initialized_sem, K_FOREVER);
	while (1) {
		if (!snake_settings_get_mute()) {
			switch (song) {
			case beep: play_song(beep_song, BEEP_NOTES); break;
			case funkytown: play_song(funkytown_song, FUNKYTOWN_NOTES); break;
			case mario: play_song(mario_song, MARIO_NOTES); break;
			case megalovania: play_song(megalovania_song, MEGALOVANIA_NOTES); break;
			case oneup: play_song(oneup_song, ONEUP_NOTES); break;
			case coin: play_song(coin_song, COIN_NOTES); break;
			case reversed_coin: play_song(reversed_coin_song, COIN_NOTES); break;
			case crazy_coin: play_song(crazy_coin_song, CRAZY_COIN_NOTES); break;
            case theme_change_song: run_theme_change_song(); break;
            case connected_song: run_connected_song(); break;
            case disconnected_song: run_disconnected_song(); break;
            case error_song: run_error_song(); break;
            case notification_song: run_notification_song(); break;
            case startup_song: run_startup_song(); break;
            case powerd_down_song: run_powerd_down_song(); break;
            case snake_game_intro: run_snake_game_intro(); break;
            case snake_eat_sound: run_snake_eat_sound(); break;
			default: break;
			}
		}

		/* Sleep thread until awoken externally */
		k_sleep(K_FOREVER);
	}
}

K_THREAD_DEFINE(buzzer_tid, BUZZER_STACK, buzzer_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

int app_buzzer_init(void)
{
	if (!device_is_ready(sBuzzer.dev)) {
		return -ENODEV;
	}
	k_sem_give(&buzzer_initialized_sem);
	return 0;
}

void play_once(SongName song_name) {
	song = song_name;
	k_wakeup(buzzer_tid);
}

void play_theme_change_song(void) {
    song = theme_change_song;
    k_wakeup(buzzer_tid);
}

void play_connected_song(void) {
    song = connected_song;
    k_wakeup(buzzer_tid);
}

void play_disconnected_song(void) {
    song = disconnected_song;
    k_wakeup(buzzer_tid);
}

void play_error_song(void) {
    song = error_song;
    k_wakeup(buzzer_tid);
}

void play_notification_song(void) {
    song = notification_song;
    k_wakeup(buzzer_tid);
}

void play_startup_song(void) {
    song = startup_song;
    k_wakeup(buzzer_tid);
}

void play_powerd_down_song(void) {
    song = powerd_down_song;
    k_wakeup(buzzer_tid);
}

void play_snake_game_intro(void) {
    song = snake_game_intro;
    k_wakeup(buzzer_tid);
}

void play_snake_eat_sound(void) {
    song = snake_eat_sound;
    k_wakeup(buzzer_tid);
}

// #####################################################################################

// void run_snake_game_intro() {
//     play_slide_with_vibrato(C4, G5, 800, 20.0f, 5.0f);  // rising retro sweep
//     play_trill(G5, B5, 300, 12);                        // playful trill
//     play_sound((Sound){ .note = E6, .duration = 150 }); // landing tone
//     play_bitcrushed_ramp(C6, G6, 300);                  // jagged retro rise
// 	stop_pwm();
// }

void run_snake_game_intro() {
    // 1. Glissando sweep up (smooth pitch ramp)
    play_glissando(C4, C6, 400);

    // 2. Trill between C6 and D6 – musical expression
    play_trill(C6, D6, 250, 12);

    // 3. Bitcrushed retro chirp from A4 to E5
    play_bitcrushed_ramp(A4, E5, 300);

    // 4. Slide down quickly – like falling
    play_slide(E5, C4, 300);

    // 5. Dual note "chime" at the end
    play_dual_note_simulated(C5, G5, 300);

    // 6. Fast stutter on final C5 to emphasize "ready"
    play_tremolo(C5, 250, 16.0f);

    stop_pwm();
}

// #####################################################################################

void run_snake_eat_sound() {
    // Start with a trill and finish with a quick upward slide
    play_trill(E6, G6, 80, 20);                 // short, playful trill
    play_slide(G6, A6, 50);                     // a quick upward slide
	stop_pwm();
}


// #####################################################################################

#define THEME_CHANGED_NOTES 3
static Sound theme_changed_sound[THEME_CHANGED_NOTES] = {
    {.note = C5, .duration = EIGTH},
    {.note = E5, .duration = EIGTH},
    {.note = G5, .duration = QUARTER}
};

// void play_theme_change_song(void) {
//     play_slide(theme_changed_sound[0], theme_changed_sound[0], EIGTH + EIGTH);
//     play_sound(theme_changed_sound[2]);
    
//     // Stop the sound after done
// 	stop_pwm();
// }
void run_theme_change_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_slide(theme_changed_sound[0].note, theme_changed_sound[1].note, EIGTH + EIGTH);
    play_sound(theme_changed_sound[2]);
    
    // Stop the sound after done
	stop_pwm();
}


// #####################################################################################

#define CONNECTED_NOTES 2
static Sound connected_sound[CONNECTED_NOTES] = {
    {.note = C6, .duration = SIXTEENTH},
    {.note = E6, .duration = EIGTH}
};

void run_connected_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_dual_note_simulated(connected_sound[0].note, connected_sound[1].note, EIGTH);
    
    // Stop the sound after done
	stop_pwm();
}


// #####################################################################################

#define DISCONNECTED_NOTES 3
static Sound disconnected_sound[DISCONNECTED_NOTES] = {
    {.note = E5, .duration = EIGTH},
    {.note = C5, .duration = EIGTH},
    {.note = A4, .duration = QUARTER}
};

void run_disconnected_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_sound(disconnected_sound[0]);
    play_sound(disconnected_sound[1]);
    play_sound(disconnected_sound[2]);
    
    // Stop the sound after done
	stop_pwm();
}


// #####################################################################################

#define ERROR_NOTES 2
static Sound error_sound[ERROR_NOTES] = {
    {.note = C4, .duration = EIGTH},
    {.note = Gb4, .duration = EIGTH}
};

void run_error_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_dual_note_simulated(error_sound[0].note, error_sound[1].note, EIGTH);
    
    // Stop the sound after done
	stop_pwm();
}

// #####################################################################################

#define NOTIFICATION_NOTES 1
static Sound notification_sound[NOTIFICATION_NOTES] = {
    {.note = G5, .duration = QUARTER}
};

void run_notification_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_sound_with_vibrato(error_sound[0], 6.0f, 5.0f);
    
    // Stop the sound after done
	stop_pwm();
}

// #####################################################################################


#define STARTUP_NOTES 6

static Sound startup_sound[STARTUP_NOTES] = {
    {.note = C5, .duration = SIXTEENTH},
    {.note = E5, .duration = SIXTEENTH},
    {.note = G5, .duration = EIGTH},
    {.note = REST, .duration = SIXTEENTH},
    {.note = C6, .duration = QUARTER},   // Bright final tone
    {.note = REST, .duration = EIGTH}
};

void run_startup_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_slide(C4, C5, EIGTH);
    play_sound(startup_sound[1]);
    play_sound(startup_sound[2]);
    play_sound(startup_sound[3]);
    play_sound(startup_sound[4]);
    play_sound(startup_sound[5]);
    
    // Stop the sound after done
	stop_pwm();
}

// #####################################################################################

#define POWER_DOWN_NOTES 5

static Sound power_down_sound[POWER_DOWN_NOTES] = {
    {.note = C6, .duration = SIXTEENTH},
    {.note = G5, .duration = SIXTEENTH},
    {.note = E5, .duration = SIXTEENTH},
    {.note = C5, .duration = EIGTH},
    {.note = A3, .duration = QUARTER}, // soft final low tone
};

void run_powerd_down_song(void) {
    if (snake_settings_get_mute()) {
        return;
    }
    play_sound(power_down_sound[0]);
    play_sound(power_down_sound[1]);
    play_sound(power_down_sound[2]);
    play_sound(power_down_sound[3]);
    play_slide_with_vibrato(C6, A3, HALF, 6.0f, 5.0f);
    
    // Stop the sound after done
	stop_pwm();
}

// #####################################################################################


#define SUCCESS_NOTES 2
static Sound success_sound[SUCCESS_NOTES] = {
    {.note = E5, .duration = SIXTEENTH},
    {.note = C5, .duration = EIGTH}
};
//Optional: Use play_dual_note_simulated(C5, E5, EIGTH) for a harmonic "ding."

#define CANCEL_NOTES 2
static Sound cancel_sound[CANCEL_NOTES] = {
    {.note = Bb4, .duration = SIXTEENTH},
    {.note = G4, .duration = EIGTH}
};

#define CHARGING_NOTES 4
static Sound charging_sound[CHARGING_NOTES] = {
    {.note = A4, .duration = SIXTEENTH},
    {.note = C5, .duration = SIXTEENTH},
    {.note = E5, .duration = EIGTH},
    {.note = G5, .duration = QUARTER}
};
// Can also be played with slight play_slide_with_vibrato transitions between steps.

#define BATTERY_FULL_NOTES 5
static Sound battery_full_sound[BATTERY_FULL_NOTES] = {
    {.note = G4, .duration = SIXTEENTH},
    {.note = C5, .duration = SIXTEENTH},
    {.note = E5, .duration = SIXTEENTH},
    {.note = G5, .duration = SIXTEENTH},
    {.note = C6, .duration = QUARTER}
};

#define WARNING_NOTES 2
static Sound warning_sound[WARNING_NOTES] = {
    {.note = F4, .duration = EIGTH},
    {.note = A4, .duration = QUARTER}
};

#define CRITICAL_ERROR_NOTES 4
static Sound critical_error_sound[CRITICAL_ERROR_NOTES] = {
    {.note = C5, .duration = SIXTEENTH},
    {.note = Gb4, .duration = SIXTEENTH},
    {.note = E4, .duration = EIGTH},
    {.note = C3, .duration = HALF}
};
// Could also be: play_slide_with_vibrato(C5, C3, WHOLE, 12.0f, 4.0f);

#define WIFI_SEARCH_NOTES 3
static Sound wifi_search_sound[WIFI_SEARCH_NOTES] = {
    {.note = G4, .duration = SIXTEENTH},
    {.note = A4, .duration = SIXTEENTH},
    {.note = Bb4, .duration = SIXTEENTH}
};
//Play this in a loop while searching, optionally increasing pitch subtly on each loop.

#define WIFI_CONNECTED_NOTES 3
static Sound wifi_connected_sound[WIFI_CONNECTED_NOTES] = {
    {.note = D5, .duration = SIXTEENTH},
    {.note = F5, .duration = SIXTEENTH},
    {.note = A5, .duration = EIGTH}
};

#define LOW_BATTERY_NOTES 3
static Sound low_battery_sound[LOW_BATTERY_NOTES] = {
    {.note = C4, .duration = SIXTEENTH},
    {.note = REST, .duration = SIXTEENTH},
    {.note = C4, .duration = QUARTER}
};
// Could add a vibrato here to make it "shaky" like a dying battery :play_sound_with_vibrato((Sound){.note = C4, .duration = QUARTER}, 12.0f, 4.0f);


#else

int app_buzzer_init(void)
{
	return 0;
}

void play_once(SongName song_name)
{
	return;
}

void play_theme_change_song(void) {
    return;
}

void play_connected_song(void) {
    return;
}

void play_disconnected_song(void) {
    return;
}

void play_error_song(void) {
    return;
}

void play_notification_song(void) {
    return;
}

void play_startup_song(void) {
    return;
}

void play_powerd_down_song(void) {
    return;
}

void play_snake_game_intro(void) {
    return;
}

void run_snake_eat_sound() {
    return;
}

#endif