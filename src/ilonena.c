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

#define FIRMWARE_REVISION (0)

// The kind of screen being shown by the device
static enum {
	ILONENA_MODE_TITLE_SCREEN,
	ILONENA_MODE_INPUT,
	ILONENA_MODE_CONFIG,
	ILONENA_MODE_INPUT_TIMEOUT,
	ILONENA_MODE_OPTBYTE_ERROR_SCREEN, // Option byte write error
} ilonena_mode = ILONENA_MODE_TITLE_SCREEN;

#define TITLE_SCREEN_TIMEOUT (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 5000) // 5000ms. Must be longer than BUTTON_HELD_THRESHOLD
#define INPUT_TIMEOUT (300) // 300 seconds
#define INPUT_TIMEOUT_DISPLAY_DURATION (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 1000) // 1000ms
#define NOT_FOUND_BLINK_DURATION (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) // 100ms. In case no glyph has been found for the input sequence, the screen blinks.

// The configuration in use. It has to be exactly 2 bytes so that it can be stored as option byte.
struct ilonena_config {
	// Avoid modifying the order of variable for backward compatibility of the config
	enum keyboard_output_mode output_mode:3;
	// extra_trailing_space for output_mode=KEYBOARD_OUTPUT_MODE_LATIN, sitelen_pona else.
	// unable to make a union for that because it's a bitfield.
	uint8_t sitelen_pona_punctuation_or_extra_trailing_space:1;
	uint16_t padding:12; // Pad to 16bits
} __attribute__((packed));

static_assert(sizeof(struct ilonena_config) == 2, "Size of struct ilonena_config must be 2 bytes so that it could be stored into the optoin bytes.");

static struct ilonena_config ilonena_config = {.output_mode=KEYBOARD_OUTPUT_MODE_LATIN, .sitelen_pona_punctuation_or_extra_trailing_space=0};
static struct ilonena_config ilonena_config_prev;

// Each sitelen pona glyph can be typed by a certain input sequence. This input buffer stores that sequence
// Upon a match between the input sequence and the built-in table, a glyph can be tytped out.
static uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX] = {0};
#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))

static size_t input_buffer_index = 0;
static uint32_t codepoint_found = 0;
static uint8_t codepoint_not_found = 0; // for blinking in case the codepoint isn't found
static uint32_t codepoint_not_found_blink_start_tick = 0; // for determining when to stop blinking
static uint8_t persistent_config = 1; // 1 if the config scene would save to optbyte permanently. 0 if config won't be persist after reboot
static uint32_t config_error_code = 0; // The error code to be displayed in case the config failed to get saved into the option bytes

void refresh_display(void) {
	static uint16_t image[LOOKUP_IMAGE_WIDTH+1] = {0}; // The +1 here is for showing image with DISPLAY_DRAW_FLAG_INVERT, which has an extra column
	display_clear();

	switch(ilonena_mode) {
		case ILONENA_MODE_TITLE_SCREEN:
			lookup_get_image(image, 0xF190E); // ILO in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 0, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
			lookup_get_image(image, 0xF1940); // NENA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 1*32+8, 1, DISPLAY_DRAW_FLAG_SCALE_2x);

			lookup_get_image(image, 0xF193D); // NANPA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 6*16, 16, 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_0_START+FIRMWARE_REVISION);
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 16, 0);
		break;
		case ILONENA_MODE_INPUT:
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
			// Drawing with LOOKUP_IMAGE_WIDTH+1 for making the inverted output square
			display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 98, 1, (codepoint_not_found ? DISPLAY_DRAW_FLAG_INVERT : 0) | DISPLAY_DRAW_FLAG_SCALE_2x);
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

			// Display config of punctuation mode selection
			if(ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_LATIN) {
				// With extra trailing space, or without
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_Q);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0*16, 16, 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_TRAILING_SPACE_PART1);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+1*16, 16, ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_TRAILING_SPACE_PART2);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+2*16, 16, ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_PART1);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+3*16, 16, !ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, 0); // Empty glyph
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+4*16, 16, !ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
			} else {
				// With sitelen pona punctuation, or with ASCII punctuation
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_Q);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 0*16, 16, 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_SITELEN_PONA_PART1);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+1*16, 16, ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_SITELEN_PONA_PART2);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+2*16, 16, ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_PART1);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+3*16, 16, !ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
				lookup_get_image(image, LOOKUP_CODEPAGE_3_START+INTERNAL_IMAGE_PUNCTUATION_LATIN_PART2);
				display_draw_16(image, LOOKUP_IMAGE_WIDTH+1, 4+4*16, 16, !ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space ? DISPLAY_DRAW_FLAG_INVERT : 0);
			}

			lookup_get_image(image, 0xF1976); // WEKA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 6*16, 16, 0);
			lookup_get_image(image, 0xF194C); // PANA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 16, 0);

			// Draw AWEN on top-right corner if we're in persistent_config mode
			if(persistent_config) {
				lookup_get_image(image, 0xF1908); // AWEN in UCSUR, code page 0.
				display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 0, 0);
			}
		break;
		case ILONENA_MODE_INPUT_TIMEOUT:
			lookup_get_image(image, 0xF196B); // TENPO in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 1*32-4, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
			lookup_get_image(image, 0xF1922); // LAPE in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 2*32+4, 1, DISPLAY_DRAW_FLAG_SCALE_2x);
		break;
		case ILONENA_MODE_OPTBYTE_ERROR_SCREEN:
			lookup_get_image(image, 0xF1948); // PAKALA in UCSUR, code page 0
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 0*32, 0, DISPLAY_DRAW_FLAG_SCALE_2x);
			lookup_get_image(image, 0xF1900); // PAKALA in UCSUR, code page 0
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 1*32, 0, DISPLAY_DRAW_FLAG_SCALE_2x);

			lookup_get_image(image, 0xF193D); // NANPA in UCSUR, code page 0.
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 6*16, 16, 0);
			lookup_get_image(image, LOOKUP_CODEPAGE_0_START+config_error_code); // error code in UCSUR
			display_draw_16(image, LOOKUP_IMAGE_WIDTH, 7*16, 16, 0);
		break;
	}

	display_set_refresh_flag();
}

void clear_input_buffer(void) {
	memset(input_buffer, 0, sizeof(input_buffer));
	input_buffer_index = 0;
	codepoint_found = 0;
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
	
	// Load settings from option bytes
	uint16_t optbyte_data = optionbytes_get_data();
	memcpy(&ilonena_config, &optbyte_data, sizeof(ilonena_config));

	uint32_t systick_now = SysTick->CNT;
	uint8_t display_refresh_required = 1; // Set it to 1 for showing the title screen
	uint32_t title_screen_timeout_start_counting_tick = systick_now;
	uint32_t input_screen_timeout_start_counting_tick;

	// For clearing OLED after a timeout for protection against OLED burnout
	// has to make a separate variable because 300s is a long wait and
	// the last_input_tick math would overflow.
	uint32_t last_input_tick = systick_now;
	uint32_t seconds_elapsed_since_last_input = 0;

	watchdog_feed();

	while(1) {
		systick_now = SysTick->CNT;
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				enum ilonena_key_id key_id = i+1;
				reprocess_key:
				switch(ilonena_mode) {
					case ILONENA_MODE_TITLE_SCREEN:
						if(key_id == ILONENA_KEY_WEKA) {
							// Reset timeout if WEKA key is pressed.
							// That's because holding WEKA key would enter persistent_config mode
							// If we switched to ILONENA_MODE_INPUT like standard button handling, then
							// the user would be unable to enter persistent_config mode
							title_screen_timeout_start_counting_tick = systick_now;
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
								// ALA (space) or PANA (enter) has been pressed! Let's handle it!
								if(input_buffer_index == 0) {
									// If the input buffer is empty, send out either ENTER or SPACE
									if(key_id == ILONENA_KEY_PANA) {
										keyboard_write_codepoint(ilonena_config.output_mode, '\n');
									} else {
										keyboard_write_codepoint(ilonena_config.output_mode, ' ');
									}
								} else {
									// Input buffer has content on it.
									// Search the lookup table, then send out the key according to the input buffer's content
									uint32_t codepoint = lookup_search(input_buffer, input_buffer_index);
									if(codepoint > 0) {
										if(ilonena_config.output_mode == KEYBOARD_OUTPUT_MODE_LATIN) {
											if(ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space) {
												// Force send trailing space for symbols like comma, dash, period, etc.
												// This is useful when you're not using a sitelen pona font.
												// Example: "mi pilin e ni : tenpo ni la , ona li moli . "
												keyboard_write_codepoint(KEYBOARD_OUTPUT_MODE_LATIN_WITH_TRAILING_SPACE, codepoint);
											} else {
												// Do not force sending trailing space for symbols.
												// It looks more compact than the former option when you're using a sitelen pona font that
												// comes with autmoatic conversion between ASCII and sitelen pona glyphs (font ligature).
												// However, it looks terrible if displayed in ASCII
												// Example: "mi pilin e ni :tenpo ni la ,ona li moli ."
												keyboard_write_codepoint(KEYBOARD_OUTPUT_MODE_LATIN, codepoint);
											}
										} else {
											if(!ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space) {
												// Using ASCII punctuations instead of sitelen pona punctuations
												switch(codepoint) {
													case 0xF1990: codepoint = '['; break;
													case 0xF1991: codepoint = ']'; break;
													case 0xF199C: codepoint = '.'; break;
													case 0xF199D: codepoint = ':'; break;
													default: break; // No conversion required for other codepoints
												}
											}
											// Write out the sitelen pona glyph in Windows/Linux/Mac mode
											// (by sending out WinCompose/CTRL+SHIFT+U/HexInputMethod Unicode sequence)
											keyboard_write_codepoint(ilonena_config.output_mode, codepoint);
										}
										if(key_id == ILONENA_KEY_PANA) {
											// Send a trailing enter if the enter key had been pressed
											keyboard_write_codepoint(ilonena_config.output_mode, '\n');
										}
										// The input buffer is sent to the computer. Time to clear it and update display.
										clear_input_buffer();
										display_refresh_required = 1;
									} else {
										// Show visual feedback that the glyph hasn't been found.
										codepoint_not_found = 1;
										codepoint_not_found_blink_start_tick = systick_now;
										display_refresh_required = 1;
									}
								}
							break;
							case ILONENA_KEY_WEKA:
								if(input_buffer_index == 0) {
									// Send backspace if the input buffer's empty
									keyboard_write_codepoint(ilonena_config.output_mode, '\b');
								} else {
									// Remove a character from the input buffer
									codepoint_not_found = 0;
									input_buffer[input_buffer_index--] = 0;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									display_refresh_required = 1;
								}
							break;
							default:
								if(input_buffer_index < INPUT_BUFFER_SIZE) {
									// Append a character to the input buffer
									codepoint_not_found = 0;
									input_buffer[input_buffer_index++] = key_id;
									codepoint_found = lookup_search(input_buffer, input_buffer_index);
									display_refresh_required = 1;
								} else {
									// Input buffer overflow. Let's ignore the extra input being supplied! :P
								}
							break;
						}
					break;
					case ILONENA_MODE_CONFIG:
						switch(key_id) {
							case ILONENA_KEY_1:
								// Cycle thru the avaialble output_mode
								if(++ilonena_config.output_mode >= KEYBOARD_OUTPUT_MODE_END) {
									ilonena_config.output_mode = 0;
								}
								display_refresh_required = 1;
							break;
							case ILONENA_KEY_Q:
								// Cycle thru the avaialble sitelen_pona_punctuation_or_extra_trailing_space (true or false)
								ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space = !ilonena_config.sitelen_pona_punctuation_or_extra_trailing_space;
								display_refresh_required = 1;
							break;
							case ILONENA_KEY_WEKA:
								// Discard the changes by reverting it.
								ilonena_config = ilonena_config_prev;
								ilonena_mode = ILONENA_MODE_INPUT;
								display_refresh_required = 1;
							break;
							case ILONENA_KEY_PANA:
								// Apply the changes (by not reverting the changes)
								ilonena_mode = ILONENA_MODE_INPUT;
								display_refresh_required = 1;
								if(persistent_config) {
									// In persistent_config, also write to the option bytes
									uint16_t optbyte_data;
									memcpy(&optbyte_data, &ilonena_config, sizeof(ilonena_config));
									config_error_code = optionbytes_write_data(optbyte_data);
									if(config_error_code) {
										// Option Bytes write error occurred!
										// Let's show the error screen instead of getting back to input mode
										ilonena_mode = ILONENA_MODE_OPTBYTE_ERROR_SCREEN;
									}
								}
							break;
							default:
								// For any other invalid keys, we ignore that by doing nothing.
							break;
						}
					break;
					case ILONENA_MODE_INPUT_TIMEOUT:
						// Ignore all input! This mode would exit on its own after waiting for a while
					break;
					case ILONENA_MODE_OPTBYTE_ERROR_SCREEN:
						// Ignore all input! The user is permanently stuck in this mode until power cycle.
						// This mode should only happen extremely rarely.
					break;
				}
			}
		}

		// If we ever end up in ILONENA_MODE_INPUT, we would no longer offer persistent_config mode.
		// The only way to enter persistent mode is to hold the WEKA button in the title screen.
		if(ilonena_mode == ILONENA_MODE_INPUT) {
			persistent_config = 0;
		}

		// For either input more or config mode, the OLED would be turned off after idling for a while
		// Purpose: OLED burn-out protection
		if(ilonena_mode == ILONENA_MODE_INPUT || ilonena_mode == ILONENA_MODE_CONFIG) {
			// Reset OLED timeout counter if either there's an input event, or nothing's being displayed
			if(button_press_event || (ilonena_mode == ILONENA_MODE_INPUT && input_buffer_index == 0)) {
				last_input_tick = systick_now;
				seconds_elapsed_since_last_input = 0;
			}

			// Increment seconds_elapsed_since_last_input every second of idle
			while(systick_now - last_input_tick >= FUNCONF_SYSTEM_CORE_CLOCK) {
				last_input_tick += FUNCONF_SYSTEM_CORE_CLOCK;
				if(++seconds_elapsed_since_last_input >= INPUT_TIMEOUT) {
					// After idling for INPUT_TIMEOUT amount of seconds, show timeout screen
					ilonena_mode = ILONENA_MODE_INPUT_TIMEOUT;
					clear_input_buffer();
					display_refresh_required = 1;
					input_screen_timeout_start_counting_tick = systick_now;
				}
			}
		} else {
			// Keep resetting OLED timeout counter if we're in non-input modes
			// Particularly, this is requried for the ILONENA_MODE_TITLE_SCREEN -> ILONENA_MODE_CONFIG transition.
			// Without this piece of code, the transition would cause the timeout to be triggered immediately
			last_input_tick = systick_now;
			seconds_elapsed_since_last_input = 0;
		}

		// Enter config mode if certain button is held
		uint32_t button_held_event = button_get_held_event();
		if((ilonena_mode == ILONENA_MODE_INPUT && (button_held_event & (1<<(ILONENA_KEY_ALA-1)))) || // If ALA is held, enter standard config mode (persistent_config=0)
			(ilonena_mode == ILONENA_MODE_TITLE_SCREEN && (button_held_event & (1<<(ILONENA_KEY_WEKA-1)))) // If WEKA is held, enter persistent_config mode (persistent_config=1)
			) {
			ilonena_config_prev = ilonena_config;
			ilonena_mode = ILONENA_MODE_CONFIG;
			display_refresh_required = 1;
		}

		// Automatically exit title screen after idling for a while
		if(ilonena_mode == ILONENA_MODE_TITLE_SCREEN && systick_now - title_screen_timeout_start_counting_tick >= TITLE_SCREEN_TIMEOUT) {
			ilonena_mode = ILONENA_MODE_INPUT;
			display_refresh_required = 1;
		}

		// Automatically exit input timeout screen (ILONENA_MODE_INPUT_TIMEOUT) after briefly showing for INPUT_TIMEOUT_DISPLAY_DURATION
		// The input timeout screen is used for informing the user that the OLED is turning of due to timeout
		// Always return to ILONENA_MODE_INPUT after timeout, even if the timeout was triggered from ILONENA_MODE_CONFIG
		if(ilonena_mode == ILONENA_MODE_INPUT_TIMEOUT && systick_now - input_screen_timeout_start_counting_tick >= INPUT_TIMEOUT_DISPLAY_DURATION) {
			ilonena_mode = ILONENA_MODE_INPUT;
			display_refresh_required = 1;
		}

		// Handle end of blinking in case invalid input sequence is found
		if(codepoint_not_found && systick_now - codepoint_not_found_blink_start_tick >= NOT_FOUND_BLINK_DURATION) {
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

		// Feed the watchdog at the end of main loop
		watchdog_feed();
	}
}
