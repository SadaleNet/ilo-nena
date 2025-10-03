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
#include "tim2_task.h"

#include "ch32fun.h"
#include "rv003usb.h"
#include <stdio.h>
#include <string.h>

#define FIRMWARE_REVISION (0)

static enum {
	ILONENA_MODE_TITLE_SCREEN,
	ILONENA_MODE_INPUT,
	ILONENA_MODE_CONFIG,
} ilonena_mode = ILONENA_MODE_TITLE_SCREEN;

struct ilonena_config {
	enum keyboard_output_mode output_mode;
	uint8_t ascii_punctuation;
};

static struct ilonena_config ilonena_config = {.output_mode=KEYBOARD_OUTPUT_MODE_LATIN, .ascii_punctuation=0};
static struct ilonena_config ilonena_config_prev;

static uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX] = {0};
#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))
#define TITLE_SCREEN_TIMEOUT (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 5000) // 5000ms. Must be longer than BUTTON_HELD_THRESHOLD
#define NOT_FOUND_BLINK_DURATION (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) // 100ms

static size_t input_buffer_index = 0;
static uint32_t codepoint_found = 0;
static uint8_t codepoint_not_found = 0; // for blinking in case the codepoint isn't found
static uint32_t codepoint_not_found_blink_start_tick = 0; // for determining when to stop blinking

void refresh_display(void) {
	static uint16_t image[LOOKUP_IMAGE_WIDTH+1] = {0};
	display_clear();

	switch(ilonena_mode) {
		case ILONENA_MODE_TITLE_SCREEN:
		{
			lookup_get_image(image, 0xF190E); // ILO in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0, 0, DISPLAY_DRAW_FLAG_SCALE_2x);
			lookup_get_image(image, 0xF1940); // NENA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 1*32, 0, DISPLAY_DRAW_FLAG_SCALE_2x);

			lookup_get_image(image, 0xF193D); // NANPA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 6*16, 16, 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_0_START+FIRMWARE_REVISION);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 16, 0);
		}
		break;
		case ILONENA_MODE_INPUT:
		{
			// Blit the input buffer
			for(size_t i=0; i<input_buffer_index; i++) {
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+input_buffer[i]-1);
				if(i<6) {
					display_draw_16(image, LOOKUP_IMAGE_WIDTH, i*16, 0, 0);
				} else {
					display_draw_16(image, LOOKUP_IMAGE_WIDTH, (i-6)*16, 16, 0);
				}
			}

			// Bilt the graphic to be output'd
			lookup_get_image(image, codepoint_found);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 98, 1, (codepoint_not_found ? DISPLAY_DRAW_FLAG_INVERT : 0) | DISPLAY_DRAW_FLAG_SCALE_2x);
		}
		break;
		case ILONENA_MODE_CONFIG:
			// Drawing with LOOKUP_IMAGE_WIDTH+1 for making the inverted border visible

			// Display config of output mode selection (Latin, Windows, Linux, Macos)
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_1);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0*16, 0, 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_LATIN);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+1*16, 0, ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_LATIN ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_WINDOWS);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+2*16, 0, ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_WINDOWS ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_LINUX);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+3*16, 0, ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_LINUX ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_MAC);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+4*16, 0, ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_MACOS ? DISPLAY_DRAW_FLAG_INVERT : 0);

			// Display config of punctuation mode selection (Latin, Windows, Linux, Macos)
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_Q);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0*16, 16, 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_PART1);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+1*16, 16, ilonena_config.ascii_punctuation ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_PART2);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+2*16, 16, ilonena_config.ascii_punctuation ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_SITELEN_PONA_PART1);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+3*16, 16, !ilonena_config.ascii_punctuation ? DISPLAY_DRAW_FLAG_INVERT : 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_SITELEN_PONA_PART2);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+4*16, 16, !ilonena_config.ascii_punctuation ? DISPLAY_DRAW_FLAG_INVERT : 0);

			lookup_get_image(image, 0xF1976); // WEKA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 6*16, 16, 0);
			lookup_get_image(image, 0xF194C); // PANA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 16, 0);
		break;
	}

	display_set_refresh_flag();
}

int main() {
	SystemInit();

	// Enable interrupt nesting for rv003usb software USB library
	__set_INTSYSCR( __get_INTSYSCR() | 0x02 );

	keyboard_init();
	button_init();
	display_init();
	tim2_task_init(); // Runs button_loop() and display_loop() with TIM2 interrupt

	uint8_t display_refresh_required = 1; // Set it to 1 for showing the title screen
	uint32_t title_screen_timeout_start_counting_tick = SysTick->CNT;

	while(1) {
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				enum ilonena_key_id key_id = i+1;
				reprocess_key:
				switch(ilonena_mode) {
					case ILONENA_MODE_TITLE_SCREEN:
						if(key_id == ILONENA_KEY_WEKA) {
							// Reset timeout if WEKA key is pressed.
							// That's because holding WEKA key would enter permanent config mode
							title_screen_timeout_start_counting_tick = SysTick->CNT;
						} else {
							ilonena_mode = ILONENA_MODE_INPUT;
							// Required for keys like PANA or ALA, which doesn't update the screen in ILONENA_MODE_INPUT
							display_refresh_required = 1;
							goto reprocess_key;
						}
					break;
					case ILONENA_MODE_INPUT:
						switch(key_id) {
							case ILONENA_KEY_ALA:
							case ILONENA_KEY_PANA:
								if(input_buffer_index == 0) {
									if(key_id == ILONENA_KEY_PANA) {
										keyboard_write_codepoint(ilonena_config.output_mode, '\n');
									} else {
										keyboard_write_codepoint(ilonena_config.output_mode, ' ');
									}
								} else {
									// Lookup the table, then send out the key according to the buffer
									uint32_t codepoint = lookup_search(input_buffer, input_buffer_index);
									if(codepoint > 0) {
										if(ilonena_config.ascii_punctuation) {
											// Using ASCII alternative punctuations
											switch(codepoint){
												case 0xF1990: codepoint = '['; break;
												case 0xF1991: codepoint = ']'; break;
												case 0xF199C: codepoint = '.'; break;
												case 0xF199D: codepoint = ':'; break;
											}
										}
										keyboard_write_codepoint(ilonena_config.output_mode, codepoint);
										if(key_id == ILONENA_KEY_PANA) {
											keyboard_write_codepoint(ilonena_config.output_mode, '\n');
										}
										memset(input_buffer, 0, sizeof(input_buffer));
										input_buffer_index = 0;
										codepoint_found = 0;
										display_refresh_required = 1;
									} else {
										codepoint_not_found = 1;
										codepoint_not_found_blink_start_tick = SysTick->CNT;
										display_refresh_required = 1;
									}
								}
							break;
							case ILONENA_KEY_WEKA:
								if(input_buffer_index == 0) {
									keyboard_write_codepoint(ilonena_config.output_mode, '\b');
								} else {
									codepoint_not_found = 0;
									input_buffer[input_buffer_index--] = 0;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									display_refresh_required = 1;
								}
							break;
							default:
								if(input_buffer_index < INPUT_BUFFER_SIZE) {
									codepoint_not_found = 0;
									input_buffer[input_buffer_index++] = key_id;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									display_refresh_required = 1;
								} else {
									// Bufferoverflow. Let's do nothing!
								}
							break;
						}
					break;
					case ILONENA_MODE_CONFIG:
						switch(key_id) {
							case ILONENA_KEY_1:
								if(++ilonena_config.output_mode >= KEYBOARD_OUTPUT_MODE_END) {
									ilonena_config.output_mode = 0;
								}
								display_refresh_required = 1;
							break;
							case ILONENA_KEY_Q:
								ilonena_config.ascii_punctuation = !ilonena_config.ascii_punctuation;
								display_refresh_required = 1;
							break;
							case ILONENA_KEY_WEKA:
								ilonena_config = ilonena_config_prev;
							// Fallthrough
							case ILONENA_KEY_PANA:
								ilonena_mode = ILONENA_MODE_INPUT;
								display_refresh_required = 1;
							break;
							default:
								// Do nothing!
							break;
						}
					break;
				}
			}
		}

		uint32_t button_held_event = button_get_held_event();
		if(ilonena_mode == ILONENA_MODE_INPUT) {
			if(button_held_event & (1<<(ILONENA_KEY_ALA-1))) {
				// If ALA is held, enter config mode
				ilonena_config_prev = ilonena_config;
				ilonena_mode = ILONENA_MODE_CONFIG;
				display_refresh_required = 1;
			}
		}


		// Automatically exit title screen after idling for a while
		if(ilonena_mode == ILONENA_MODE_TITLE_SCREEN && SysTick->CNT - title_screen_timeout_start_counting_tick >= TITLE_SCREEN_TIMEOUT) {
			ilonena_mode = ILONENA_MODE_INPUT;
			display_refresh_required = 1;
		}

		// Handle end of blinking in case invalid input sequence is found
		if(codepoint_not_found && SysTick->CNT - codepoint_not_found_blink_start_tick >= NOT_FOUND_BLINK_DURATION) {
			codepoint_not_found = 0;
			display_refresh_required = 1;
		}

		// When display refresh flag is set, only draw on the the display buffer and kick off the DMA while
		// there's no data transfer to the display is going on. Updating the display buffer while the DMA is reading it
		// would cause inconsistent pixels being displayed.
		if(display_refresh_required && display_is_idle()) {
			display_refresh_required = 0;
			refresh_display();
		}
	}
}
