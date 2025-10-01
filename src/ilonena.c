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

static enum {
	ILONENA_MODE_INPUT,
	ILONENA_MODE_CONFIG,
} ilonena_mode = ILONENA_MODE_INPUT;

struct ilonena_config {
	enum keyboard_output_mode output_mode;
	uint8_t ascii_punctuation;
};

static struct ilonena_config ilonena_config = {.output_mode=KEYBOARD_OUTPUT_MODE_LATIN, .ascii_punctuation=0};
static struct ilonena_config ilonena_config_prev;

static uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX] = {0};
#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))

static size_t input_buffer_index = 0;
static uint32_t codepoint_found = 0;

void refresh_display(void) {
	static uint16_t image[LOOKUP_IMAGE_WIDTH+1] = {0};
	display_clear();

	switch(ilonena_mode) {
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
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 98, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
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

	while(1) {
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				enum ilonena_key_id key_id = i+1;
				switch(ilonena_mode) {
					case ILONENA_MODE_INPUT:
						switch(key_id) {
							case ILONENA_KEY_ALA:
							case ILONENA_KEY_PANA:
								if(input_buffer_index == 0) {
									keyboard_write_codepoint(ilonena_config.output_mode, ' ');
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
										refresh_display();
									}
								}
							break;
							case ILONENA_KEY_WEKA:
								if(input_buffer_index == 0) {
									keyboard_write_codepoint(ilonena_config.output_mode, '\b');
								} else {
									input_buffer[input_buffer_index--] = 0;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									refresh_display();
								}
							break;
							default:
								if(input_buffer_index < INPUT_BUFFER_SIZE) {
									input_buffer[input_buffer_index++] = key_id;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									refresh_display();
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
								refresh_display();
							break;
							case ILONENA_KEY_Q:
								ilonena_config.ascii_punctuation = !ilonena_config.ascii_punctuation;
								refresh_display();
							break;
							case ILONENA_KEY_WEKA:
								ilonena_config = ilonena_config_prev;
							// Fallthrough
							case ILONENA_KEY_PANA:
								ilonena_mode = ILONENA_MODE_INPUT;
								refresh_display();
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
				refresh_display();
			}
		}
	}
}
