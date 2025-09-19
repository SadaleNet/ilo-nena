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

#include "keyboard.h"

#include "ch32fun.h"
#include "rv003usb.h"

#define KEYBOARD_MODE_START (HID_KEY_LANG1)
#define KEYBOARD_SITELEN_PONA_CODEPOINT_START (0xF1900)

uint8_t keyboard_out_buffer[32] = {0}; // CONCURRENCY_VARIABLE: written by main loop, read by usb_handle_user_in_request()
size_t keyboard_out_buffer_end_index = 0; // CONCURRENCY_VARIABLE: ditto
size_t keyboard_out_buffer_start_index = 0; // CONCURRENCY_VARIABLE: written by usb_handle_user_in_request(), read by main loop

void usb_handle_user_in_request(struct usb_endpoint *e, uint8_t *scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal *ist) {
	if(endp == 0) {
		// Always make empty response for control transfer
		usb_send_empty( sendtok );
	} else if(endp == 1) {
		asm volatile ("" ::: "memory");
		// Keyboard end point
		// Send the previous payload. We want to send out the response as soon as possible and cannot afford to wait for building the payload
		static uint8_t usb_response[8] = { 0x00 }; // Format: modifiers_keys (1 byte), reserved (1 byte), key_scancodes (6 bytes)
		static enum keyboard_output_mode mode;
		static uint8_t release_sent = 1; // Windows Alt-Code requires releasing the key before pressing the next one.
		usb_send_data( usb_response, 8, 0, sendtok );

		if(keyboard_out_buffer_start_index != keyboard_out_buffer_end_index) {
			uint8_t key_id = keyboard_out_buffer[keyboard_out_buffer_start_index];
			if(key_id >= KEYBOARD_MODE_START) {
				mode = key_id-KEYBOARD_MODE_START;
				release_sent = 1;
				if(++keyboard_out_buffer_start_index >= sizeof(keyboard_out_buffer)) {
					keyboard_out_buffer_start_index = 0;
				}
				memset(usb_response, 0, sizeof(usb_response));
			} else {
				switch(mode) {
					case KEYBOARD_OUTPUT_MODE_LATIN:
						usb_response[2] = keyboard_out_buffer[keyboard_out_buffer_start_index];
						if(++keyboard_out_buffer_start_index >= sizeof(keyboard_out_buffer)) {
							keyboard_out_buffer_start_index = 0;
						}
					break;
					case KEYBOARD_OUTPUT_MODE_WINDOWS:
						usb_response[0] = KEYBOARD_MODIFIER_LEFTALT;
						if(release_sent) {
							usb_response[2] = keyboard_out_buffer[keyboard_out_buffer_start_index];
							release_sent = 0;
							if(++keyboard_out_buffer_start_index >= sizeof(keyboard_out_buffer)) {
								keyboard_out_buffer_start_index = 0;
							}
						} else {
							usb_response[2] = 0x00;
							release_sent = 1;
						}
					break;
				}
			}
		} else {
			memset(usb_response, 0, sizeof(usb_response));
		}
	}
}

const char* KEYBOARD_WORDS_LATIN_MAPPING[] = {
	"a", "akesi", "ala", "alasa", "ali",
	"anpa", "ante", "anu", "awen", "e",
	"en", "esun", "ijo", "ike", "ilo",
	"insa", "jaki", "jan", "jelo", /*"jo"*/ "kijetesantakalu",
};

// Each word's short enough. It's probably not worth using memcpy(). Let's do it character-by-character.
static void keyboard_push_to_out_buffer(uint8_t key_id) {
	asm volatile ("" ::: "memory");
	while((keyboard_out_buffer_end_index+1)%sizeof(keyboard_out_buffer) == keyboard_out_buffer_start_index) {
		// Block until keyboard output buffer is available before inserting the next character
		asm volatile ("" ::: "memory");
	}
	keyboard_out_buffer[keyboard_out_buffer_end_index] = key_id;
	if(++keyboard_out_buffer_end_index >= sizeof(keyboard_out_buffer)) {
		keyboard_out_buffer_end_index = 0;
	}
}

void keyboard_write_character(enum keyboard_output_mode mode, size_t charcter_id) {
	keyboard_push_to_out_buffer(KEYBOARD_MODE_START+mode);

	switch(mode) {
		case KEYBOARD_OUTPUT_MODE_LATIN:
			for(size_t i=0; KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]; i++) {
				keyboard_push_to_out_buffer(KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]-'a'+HID_KEY_A);
			}
		break;
		case KEYBOARD_OUTPUT_MODE_WINDOWS:
		break;
		case KEYBOARD_OUTPUT_MODE_LINUX:
		break;
		case KEYBOARD_OUTPUT_MODE_MACOS:
		break;
	}

	/*keyboard_out_buffer[index] = HID_KEY_KEYPAD_8;
	if(++index >= sizeof(keyboard_out_buffer)) {
		index = 0;
	}
	keyboard_out_buffer[index] = HID_KEY_KEYPAD_5;
	if(++index >= sizeof(keyboard_out_buffer)) {
		index = 0;
	}
	keyboard_out_buffer[index] = HID_KEY_KEYPAD_9;
	if(++index >= sizeof(keyboard_out_buffer)) {
		index = 0;
	}
	keyboard_out_buffer[index] = HID_KEY_KEYPAD_2 + charcter_id;
	if(++index >= sizeof(keyboard_out_buffer)) {
		index = 0;
	}
	keyboard_out_buffer_end_index = index;*/
}

void keyboard_init(void) {
	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5us ( TDDIS )
	usb_setup();
	
	keyboard_out_buffer_start_index = 0;
}
