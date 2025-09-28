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

static uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX] = {0};
#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))

static size_t input_buffer_index = 0;
static uint32_t codepoint_found = 0;

void refresh_display(void) {
	static uint16_t image[15] = {0};
	display_clear();

	// Blit the input buffer
	for(size_t i=0; i<input_buffer_index; i++) {
		lookup_get_image(image, LOOKUP_CODEPAGE_3_START+input_buffer[i]-1);
		if(i<6) {
			display_draw_16(image, 15, i*16, 0, 0);
		} else {
			display_draw_16(image, 15, (i-6)*16, 16, 0);
		}
	}

	// Bilt the graphic to be output'd
	lookup_get_image(image, codepoint_found);
	display_draw_16(image, 15, 98, 1, DISPLAY_DRAW_FLAG_SCALE_2x);

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

	enum keyboard_output_mode mode = KEYBOARD_OUTPUT_MODE_LATIN;

	while(1) {
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				enum ilonena_key_id key_id = i+1;
				switch(key_id) {
					case ILONENA_KEY_ALA:
						if(input_buffer_index == 0) {
							keyboard_write_codepoint(mode, ' ');
						} else {
							// Lookup the table, then send out the key according to the buffer
							uint32_t codepoint = lookup_search(input_buffer, input_buffer_index);
							if(codepoint > 0) {
								keyboard_write_codepoint(mode, codepoint);
								memset(input_buffer, 0, sizeof(input_buffer));
								input_buffer_index = 0;
								codepoint_found = 0;
								refresh_display();
							}
						}
					break;
					case ILONENA_KEY_PANA:
						// Stealing this button for mode switching for now in before the config page is ready
						// In the future, this button is equivalent to pressing space then press enter
						if(++mode >= KEYBOARD_OUTPUT_MODE_END) {
							mode = 0;
						}
					break;
					case ILONENA_KEY_WEKA:
						if(input_buffer_index == 0) {
							keyboard_write_codepoint(mode, '\b');
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
			}
		}
	}
}
