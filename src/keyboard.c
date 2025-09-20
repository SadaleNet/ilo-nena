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
#include <stdio.h>

#define KEYBOARD_MODE_START (HID_KEY_LANG1)
#define KEYBOARD_SITELEN_PONA_CODEPOINT_START (0xF1900U)

uint8_t keyboard_out_buffer[32] = {0}; // CONCURRENCY_VARIABLE: written by main loop, read by usb_handle_user_in_request()
size_t keyboard_out_buffer_write_index = 0; // CONCURRENCY_VARIABLE: ditto
size_t keyboard_out_buffer_read_index = 0; // CONCURRENCY_VARIABLE: written by usb_handle_user_in_request(), read by main loop

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
		// The procedure of sending a key goes like this: key 1 press -> key 1 release -> key 2 press -> key 2 release and so on.
		// This is required in case two consecutive identical keys are to be typed
		static uint8_t key_release_sent = 1;
		usb_send_data( usb_response, 8, 0, sendtok );

		if(keyboard_out_buffer_read_index != keyboard_out_buffer_write_index) {
			uint8_t key_id = keyboard_out_buffer[keyboard_out_buffer_read_index];
			if(key_id >= KEYBOARD_MODE_START) {
				memset(usb_response, 0, sizeof(usb_response));
				mode = key_id-KEYBOARD_MODE_START;
				switch(mode) {
					case KEYBOARD_OUTPUT_MODE_LINUX:
						// Send CTRL+SHIFT+U prefix
						usb_response[0] = KEYBOARD_MODIFIER_LEFTCTRL|KEYBOARD_MODIFIER_LEFTSHIFT;
						usb_response[2] = HID_KEY_U;
						key_release_sent = 0; // Need to release the key combo before typing the next key.
					break;
					case KEYBOARD_OUTPUT_MODE_LATIN:
					case KEYBOARD_OUTPUT_MODE_WINDOWS:
					case KEYBOARD_OUTPUT_MODE_MACOS:
						// Do nothing. Can type the next key right away
						key_release_sent = 1;
					break;
					case KEYBOARD_OUTPUT_MODE_IDLE:
						// The purposes of this state is to force releasing all keys.
						// It is a bad idea to release the key whenever (keyboard_out_buffer_read_index == keyboard_out_buffer_write_index)
						// because in case the main loop is lagging, the condition can be reached before main loop had finished filling the buffer.
						// Therefore, the keys are only released upon MODE_IDLE is detected.
					break;
				}
				if(++keyboard_out_buffer_read_index >= sizeof(keyboard_out_buffer)) {
					keyboard_out_buffer_read_index = 0;
				}
			} else {
				switch(mode) {
					case KEYBOARD_OUTPUT_MODE_WINDOWS:
					case KEYBOARD_OUTPUT_MODE_MACOS:
						// Windows Alt-Code requires releasing the key before pressing the next one
						// Not sure about Mac but let's use the same handling for simplicity
						// BTW, the Mac OS Option key is equivalent to Alt key.
						usb_response[0] = KEYBOARD_MODIFIER_LEFTALT;
					break;
					case KEYBOARD_OUTPUT_MODE_LATIN:
					case KEYBOARD_OUTPUT_MODE_LINUX:
						usb_response[0] = 0x00;
					break;
					case KEYBOARD_OUTPUT_MODE_IDLE:
						while(1); // Should never happen!
					break;
				}
				if(key_release_sent) {
					usb_response[2] = keyboard_out_buffer[keyboard_out_buffer_read_index];
					key_release_sent = 0;
					if(++keyboard_out_buffer_read_index >= sizeof(keyboard_out_buffer)) {
						keyboard_out_buffer_read_index = 0;
					}
					key_release_sent = 0;
				} else {
					if(mode == KEYBOARD_OUTPUT_MODE_LINUX) {
						// Release the CTRL+SHIFT of the CTRL+SHIFT+U
						usb_response[0] = 0x00;
					} else {
						// For Windows/Mac OS mode, need to keep holding the Alt/Option key.
						// That's why we don't change the status of usb_response[0] here.
					}
					usb_response[2] = 0x00;
					key_release_sent = 1;
				}
			}
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
	while((keyboard_out_buffer_write_index+1)%sizeof(keyboard_out_buffer) == keyboard_out_buffer_read_index) {
		// Block until keyboard output buffer is available before inserting the next character
		asm volatile ("" ::: "memory");
	}
	keyboard_out_buffer[keyboard_out_buffer_write_index] = key_id;
	if(++keyboard_out_buffer_write_index >= sizeof(keyboard_out_buffer)) {
		keyboard_out_buffer_write_index = 0;
	}
}

static void keyboard_push_hex_to_out_buffer(uint32_t codepoint) {
	static const uint8_t keyboard_hex_to_hid_key[16] = {
		HID_KEY_0, HID_KEY_1, HID_KEY_2, HID_KEY_3,
		HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_7,
		HID_KEY_8, HID_KEY_9, HID_KEY_A, HID_KEY_B,
		HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F,
	};
	uint8_t handled_leading_zeros = 0;
	// reverse iteration from 7 to 0 inclusive using unsigned integer
	for(uint32_t i=8; i-->0; ) {
		uint8_t digit = (codepoint & (0xF<<(i*4))) >> (i*4);
		if(!handled_leading_zeros && !digit) {
			continue; // Do not type out leading zeros
		}
		keyboard_push_to_out_buffer(keyboard_hex_to_hid_key[digit]);
		handled_leading_zeros = 1;
	}
}

void keyboard_write_character(enum keyboard_output_mode mode, size_t charcter_id) {
	keyboard_push_to_out_buffer(KEYBOARD_MODE_START+mode);
	uint32_t codepoint = KEYBOARD_SITELEN_PONA_CODEPOINT_START+charcter_id;

	switch(mode) {
		case KEYBOARD_OUTPUT_MODE_LATIN:
			for(size_t i=0; KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]; i++) {
				keyboard_push_to_out_buffer(KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]-'a'+HID_KEY_A);
			}
		break;
		case KEYBOARD_OUTPUT_MODE_WINDOWS:
		{
			char str_base10[16];
			snprintf(str_base10, sizeof(str_base10), "%ld", codepoint);
			for(size_t i=0; str_base10[i]; i++) {
				if(str_base10[i] == '0') {
					keyboard_push_to_out_buffer(HID_KEY_KEYPAD_0);
				} else {
					keyboard_push_to_out_buffer(str_base10[i]-'1'+HID_KEY_KEYPAD_1);
				}
			}
		}
		break;
		case KEYBOARD_OUTPUT_MODE_LINUX:
		{
			keyboard_push_hex_to_out_buffer(codepoint);
			keyboard_push_to_out_buffer(HID_KEY_SPACE); // Press space after complete entering the unicode
		}
		break;
		case KEYBOARD_OUTPUT_MODE_MACOS:
			// Send hex in UTF-16 format
			uint32_t utf16_codepoint;
			if(codepoint <= 0xFFFF) {
				utf16_codepoint = codepoint;
			} else if (codepoint <= 0x10FFFF) {
				uint32_t codepoint_base = codepoint - 0x10000U;
				utf16_codepoint = ((0xD800 | ((codepoint_base & (0x3FF<<10)) >> 10)) << 16) | (0xDC00 | (codepoint_base & 0x3FF));
			} else {
				// Outside of UTF-16's range. Let's fill in an emoji.
				utf16_codepoint = 0xD83DDD95;
			}
			keyboard_push_hex_to_out_buffer(utf16_codepoint);
		break;
		case KEYBOARD_OUTPUT_MODE_IDLE:
			while(1); // Should never happen!
		break;
	}

	keyboard_push_to_out_buffer(KEYBOARD_MODE_START+KEYBOARD_OUTPUT_MODE_IDLE); // Release all keys
}

void keyboard_init(void) {
	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5us ( TDDIS )
	usb_setup();
	
	keyboard_out_buffer_read_index = 0;
}
