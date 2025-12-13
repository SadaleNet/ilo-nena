// Copyright 2025 Wong Cho Ching <https://sadale.net>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "button.h"
#include "display.h"
#include "lookup.h"
#include "keyboard.h"
#include "optionbytes.h"
#include "tim2_task.h"
#include "watchdog.h"

#include "ch32fun.h"
#include "rv003usb.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define FIRMWARE_REVISION (1)

// The kind of screen being shown by the device
static enum {
	ILONENA_MODE_TITLE_SCREEN,
	ILONENA_MODE_GAME,
	ILONENA_MODE_TIMEOUT,
} ilonena_mode = ILONENA_MODE_TITLE_SCREEN;

#define INPUT_TIMEOUT (3) // 3 seconds
#define INPUT_TIMEOUT_DISPLAY_DURATION (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 1000) // 1000ms
#define NOT_FOUND_BLINK_DURATION (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) // 100ms. In case no glyph has been found for the input sequence, the screen blinks.

// Each sitelen pona glyph can be typed by a certain input sequence. This input buffer stores that sequence
// Upon a match between the input sequence and the built-in table, a glyph can be tytped out.
static uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX] = {0};
#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))

#define JUMP_PERIOD (1000)

uint8_t game_kijetesantakalu_y;
uint8_t game_jaki_x[4]; // Up to 4 stones. value of 255 means no stone.
uint8_t game_jaki_y[4]; // Up to 4 stones. value of 255 means no stone.
uint32_t GAME_ITEM_MAP[4] = {0xF1910, 0xF1948, 0xF190D, 0xF1971}; // jaki, pakala, ike, utala
uint32_t game_next_jaki_move_tick;
uint32_t game_jaki_move_interval;
uint8_t game_moli;

void refresh_display(void) {
	static uint16_t image[LOOKUP_IMAGE_WIDTH+1] = {0}; // The +1 here is for showing image with DISPLAY_DRAW_FLAG_INVERT, which has an extra column
	display_clear();

	switch(ilonena_mode) {
		case ILONENA_MODE_TITLE_SCREEN:
			lookup_get_image(image, 0xF193B); // MUSI in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 0, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
			lookup_get_image(image, 0xF1980); // KIJETESANTAKALU in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 1*32+8, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
		break;
		case ILONENA_MODE_GAME:
			// Draw stones
			for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
				if(game_jaki_x[i] == 255) {
					continue;
				}
				if(game_moli && game_jaki_x[i] < 32) {
					continue;
				}
				lookup_get_image(image, GAME_ITEM_MAP[i]);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH, game_jaki_x[i], game_jaki_y[i], 0);
			}

			if(game_moli) {
				// Bilt the graphic to be output'd
				lookup_get_image(image, 0xF1937);
				// Drawing with LOOKUP_IMAGE_WIDTH+1 for making the inverted output square
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0, 1, DISPLAY_DRAW_FLAG_INVERT | DISPLAY_DRAW_FLAG_SCALE_2x);
			} else {
				// Draw kijetesantakalu
				lookup_get_image(image, 0xF1980);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH, 0, game_kijetesantakalu_y, 0);
			}
		break;
		case ILONENA_MODE_TIMEOUT:
			// Draw nothing
		break;
	}

	display_set_refresh_flag();
}

void clear_input_buffer(void) {
	memset(input_buffer, 0, sizeof(input_buffer));
}

void game_start(uint32_t systick_now) {
	ilonena_mode = ILONENA_MODE_GAME;
	game_kijetesantakalu_y = 0;
	for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
		game_jaki_x[i] = 255;
		game_jaki_y[i] = 255;
	}
	game_next_jaki_move_tick = systick_now;
	game_jaki_move_interval = 1000000;
	game_moli = 0;
}

int main() {
	// Kickoff the watchdog as early as possible
	watchdog_init();

	SystemInit();

	// Enable interrupt nesting for rv003usb software USB library
	__set_INTSYSCR( __get_INTSYSCR() | 0x02 );

	keyboard_init();
	button_init();
	display_init();
	tim2_task_init(); // Runs button_loop() and display_loop() with TIM2 interrupt

	uint32_t systick_now = SysTick->CNT;

	// For clearing OLED after a timeout for protection against OLED burnout
	// has to make a separate variable because 300s is a long wait and
	// the last_input_tick math would overflow.
	uint32_t last_input_tick = systick_now;
	uint32_t seconds_elapsed_since_last_input = 0;
	game_moli = 1;

	watchdog_feed();

	while(1) {
		systick_now = SysTick->CNT;
		uint32_t button_press_event = button_get_pressed_event();

		if(button_press_event) {
			switch(ilonena_mode) {
				case ILONENA_MODE_GAME:
					if(!game_moli) {
						game_kijetesantakalu_y = game_kijetesantakalu_y ? 0 : 16;
					}
				break;
				default:
					game_start(systick_now);
				break;
			}
			// The OLED would be turned off after idling for a while
			// Purpose: OLED burn-out protection
			// Reset OLED timeout counter if either there's an input event, or nothing's being displayed
			last_input_tick = systick_now;
			seconds_elapsed_since_last_input = 0;
		}

		if(ilonena_mode == ILONENA_MODE_GAME) {
			if(!game_moli) {
				if(systick_now >= game_next_jaki_move_tick) {
					for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
						if(game_jaki_x[i] != 255) {
							game_jaki_x[i]--;
						}
					}
					game_next_jaki_move_tick = systick_now + game_jaki_move_interval;
						game_jaki_move_interval = game_jaki_move_interval*999/1000;
				}

				for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
					if(game_jaki_x[i] < 16 && game_jaki_y[i] == game_kijetesantakalu_y) {
						game_moli = 1;
						break;
					}
				}

				uint8_t can_spwan = 1;
				for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
					if(game_jaki_x[i] != 255 && game_jaki_x[i] > DISPLAY_WIDTH-40) {
						can_spwan = 0;
						break;
					}
				}
				if(can_spwan) {
					for(size_t i=0; i<sizeof(game_jaki_x)/sizeof(*game_jaki_x); i++) {
						if(game_jaki_x[i] == 255) {
							game_jaki_x[i] = DISPLAY_WIDTH;
							game_jaki_y[i] = systick_now%2 ? 0 : 16;
							break;
						}
					}
				}
			}
		}

		// Increment seconds_elapsed_since_last_input every second of idle
		while(game_moli && systick_now - last_input_tick >= FUNCONF_SYSTEM_CORE_CLOCK) {
			last_input_tick += FUNCONF_SYSTEM_CORE_CLOCK;
			if(++seconds_elapsed_since_last_input >= INPUT_TIMEOUT) {
				// After idling for INPUT_TIMEOUT amount of seconds, show timeout screen (i.e. black screen)
				ilonena_mode = ILONENA_MODE_TIMEOUT;
				clear_input_buffer();
			}
		}

		// Always display the latest screen upon the screen communication is idle.
		if(display_is_idle()) {
			refresh_display();
		}

		// Feed the watchdog at the end of main loop
		watchdog_feed();
	}
}
