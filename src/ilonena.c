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

const uint32_t* OUTPUT_TABLE[] = {
	(const uint32_t[]){126, 40670, 24515, 36896, 29289, 20465, 27138, 37096, 126, 32, 0}, // ~點心造物俱樂部~
	(const uint32_t[]){25105, 23628, 20320, 32769, 27597, 33, 32, 0}, // 我屌你老母!
	(const uint32_t[]){20166, 34903, 33, 32, 0}, // 仆街!
	(const uint32_t[]){20890, 23478, 21111, 33, 32, 0}, // 冚家剷!
	(const uint32_t[]){22057, 33, 32, 22909, 26578, 21568, 33, 32, 0}, // 嘩! 好柒呀!
	(const uint32_t[]){20570, 20060, 40169, 22050, 63, 32, 0}, // 做乜鳩嘢?

	(const uint32_t[]){25910, 30382, 21862, 33, 32, 0}, // 收皮啦!
	(const uint32_t[]){21780, 25754, 20418, 25499, 63, 32, 0}, // 唔撚係掛?
	(const uint32_t[]){21670, 63, 32, 21448, 24190, 25754, 27491, 21902, 33, 32, 0}, // 咦? 又幾撚正喎!
	(const uint32_t[]){25095, 40169, 20180, 33, 32, 0}, // 戇鳩仔!
	(const uint32_t[]){21779, 33, 32, 25653, 31528, 26578, 22021, 33, 32, 0}, // 唓! 搵笨柒嘅!
	(const uint32_t[]){21710, 21568, 33, 32, 26159, 40169, 26086, 21862, 33, 32, 0}, // 哎呀! 是鳩旦啦!

	(const uint32_t[]){22909, 25754, 24758, 21834, 33, 32, 0}, // 好撚悶啊!
	(const uint32_t[]){21834, 33, 32, 29609, 25754, 23436, 22217, 33, 32, 0}, // 啊! 玩撚完囉!
	(const uint32_t[]){20060, 40169, 37117, 20871, 26194, 21862, 33, 32, 0}, // 乜鳩都冇晒啦!
	(const uint32_t[]){20941, 25754, 27515, 25105, 22217, 33, 32, 0}, // 凍撚死我囉!
	(const uint32_t[]){22810, 40169, 39192, 33, 32, 0}, // 多鳩餘!
};
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
							case ILONENA_KEY_PANA:
								keyboard_write_codepoint(ilonena_config.output_mode, '\n');
							break;
							case ILONENA_KEY_ALA:
								keyboard_write_codepoint(ilonena_config.output_mode, ' ');
							break;
							case ILONENA_KEY_WEKA:
								keyboard_write_codepoint(ilonena_config.output_mode, '\b');
							break;
							default:
							{
								const uint32_t *ptr = OUTPUT_TABLE[i];
								while(*ptr) {
									keyboard_write_codepoint(ilonena_config.output_mode, *ptr);
									ptr++;
								}
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
