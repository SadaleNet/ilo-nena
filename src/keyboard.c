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

// How long it takes until we give up asserting the lock state. Unit depends on the USB polling frequency
#define KEYBOADRD_LOCK_CHANGE_TIMEOUT (100)

// For keyboard_ascii_to_keycode, hold right shift if this flag exists
#define KEYHID_SFT (0x80)

// Table for converting ASCII-ish codepoints to HID_KEY_*, with KEYHID_SFT indicating requirement of holding shift
// The codepoint 0x10~0x19 are stolen for outputting numpad keys, which is different from ASCII standard.
const uint8_t keyboard_ascii_to_keycode[128] = {
	// 0X
	0, 0, 0, 0, 0, 0, 0, 0, HID_KEY_BACKSPACE, HID_KEY_TAB, HID_KEY_ENTER, 0, 0, HID_KEY_ENTER, 0, 0,
	// 1X (The first 10 digits had been stolen for numpad keys)
	HID_KEY_KEYPAD_0, HID_KEY_KEYPAD_1, HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_3,
	HID_KEY_KEYPAD_4, HID_KEY_KEYPAD_5, HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_7,
	HID_KEY_KEYPAD_8, HID_KEY_KEYPAD_9, 0, HID_KEY_ESCAPE,
	0, 0, 0, 0,
	// 2X
	HID_KEY_SPACE, KEYHID_SFT|HID_KEY_1, KEYHID_SFT|HID_KEY_APOSTROPHE, KEYHID_SFT|HID_KEY_3,
	KEYHID_SFT|HID_KEY_4, KEYHID_SFT|HID_KEY_5, KEYHID_SFT|HID_KEY_7, HID_KEY_APOSTROPHE,
	KEYHID_SFT|HID_KEY_9, KEYHID_SFT|HID_KEY_0, KEYHID_SFT|HID_KEY_8, KEYHID_SFT|HID_KEY_EQUAL,
	HID_KEY_COMMA, HID_KEY_MINUS, HID_KEY_PERIOD, HID_KEY_SLASH,
	// 3X
	HID_KEY_0, HID_KEY_1, HID_KEY_2, HID_KEY_3,
	HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_7,
	HID_KEY_8, HID_KEY_9, KEYHID_SFT|HID_KEY_SEMICOLON, HID_KEY_SEMICOLON, KEYHID_SFT|HID_KEY_COMMA,
	HID_KEY_EQUAL, KEYHID_SFT|HID_KEY_PERIOD, KEYHID_SFT|HID_KEY_SLASH,
	// 4X
	KEYHID_SFT|HID_KEY_2, KEYHID_SFT|HID_KEY_A, KEYHID_SFT|HID_KEY_B, KEYHID_SFT|HID_KEY_C,
	KEYHID_SFT|HID_KEY_D, KEYHID_SFT|HID_KEY_E, KEYHID_SFT|HID_KEY_F, KEYHID_SFT|HID_KEY_G,
	KEYHID_SFT|HID_KEY_H, KEYHID_SFT|HID_KEY_I, KEYHID_SFT|HID_KEY_J, KEYHID_SFT|HID_KEY_K,
	KEYHID_SFT|HID_KEY_L, KEYHID_SFT|HID_KEY_M, KEYHID_SFT|HID_KEY_N, KEYHID_SFT|HID_KEY_O,
	// 5X
	KEYHID_SFT|HID_KEY_P, KEYHID_SFT|HID_KEY_Q, KEYHID_SFT|HID_KEY_R, KEYHID_SFT|HID_KEY_S,
	KEYHID_SFT|HID_KEY_T, KEYHID_SFT|HID_KEY_U, KEYHID_SFT|HID_KEY_V, KEYHID_SFT|HID_KEY_W,
	KEYHID_SFT|HID_KEY_X, KEYHID_SFT|HID_KEY_Y, KEYHID_SFT|HID_KEY_Z, HID_KEY_BRACKET_LEFT,
	HID_KEY_BACKSLASH, HID_KEY_BRACKET_RIGHT, KEYHID_SFT|HID_KEY_6, KEYHID_SFT|HID_KEY_MINUS,
	// 6X
	HID_KEY_GRAVE, HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F, HID_KEY_G,
	HID_KEY_H, HID_KEY_I, HID_KEY_J, HID_KEY_K, HID_KEY_L, HID_KEY_M, HID_KEY_N, HID_KEY_O,
	// 7X
	HID_KEY_P, HID_KEY_Q, HID_KEY_R, HID_KEY_S,
	HID_KEY_T, HID_KEY_U, HID_KEY_V, HID_KEY_W,
	HID_KEY_X, HID_KEY_Y, HID_KEY_Z, KEYHID_SFT|HID_KEY_BRACKET_LEFT,
	KEYHID_SFT|HID_KEY_BACKSLASH, KEYHID_SFT|HID_KEY_BRACKET_RIGHT, KEYHID_SFT|HID_KEY_GRAVE, HID_KEY_DELETE
};
#define KEYBOARD_MODE_START (sizeof(keyboard_ascii_to_keycode)/sizeof(*keyboard_ascii_to_keycode))

// Ring buffer format: See keyboard_ascii_to_keycode if the value is <KEYBOARD_MODE_START. Otherwise see enum keyboard_output_mode.
// It's written in the main loop and read from the usb_handle_user_in_request() in ISR
// Example: {KEYBOARD_OUTPUT_MODE_LATIN, 'a', 'k', 'e', 's', 'i', KEYBOARD_OUTPUT_MODE_END}
//   Effect: Type out "akesi" on keyboard.
// Example: {KEYBOARD_OUTPUT_MODE_LINUX, '1', 'f', '5', '9', '5', ' ', KEYBOARD_OUTPUT_MODE_END}
//   Effect: Type out CTRL+SHIFT+U, then "1f595 " on keyboard. That'd output U+1F595 on linux.
// Lock keys such as Caps lock and Num lock are automatically toggled during the operation because
// the unicode-input mechanism only works when the locks are in correct state.
uint8_t keyboard_out_buffer[32] = {0}; // CONCURRENCY_VARIABLE: written by main loop, read by usb_handle_user_in_request()
size_t keyboard_out_buffer_write_index = 0; // CONCURRENCY_VARIABLE: ditto
size_t keyboard_out_buffer_read_index = 0; // CONCURRENCY_VARIABLE: written by usb_handle_user_in_request(), read by main loop

uint8_t keyboard_locks_indicator = 0; // Not a concurrent variable. Used in usb_handle_user_data() and usb_handle_user_in_request(), both handled in the same ISR

// Grab the LED indicator of the keyboard. Purpose: To assert Num lock, Caps lock, etc. for entering unicode if needed
void usb_handle_user_data(struct usb_endpoint *e, int current_endpoint, uint8_t *data, int len, struct rv003usb_internal *ist) {
	if (len > 0) {
		keyboard_locks_indicator = data[0];
	}
}

// For toggling lock buttons based on the current lock state and the targeted lock state.
uint8_t usb_handle_user_in_request_toggle_locks(uint8_t usb_response[8], uint8_t lock_indicator_current, uint8_t lock_indicator_target, uint8_t lock_indicator_target_mask) {
	uint8_t lock_change_required = (lock_indicator_current ^ lock_indicator_target) & lock_indicator_target_mask;

	size_t usb_index = 2;
	if(lock_change_required & KEYBOARD_LED_NUMLOCK) {
		usb_response[usb_index++] = HID_KEY_NUM_LOCK;
	}
	if(lock_change_required & KEYBOARD_LED_CAPSLOCK) {
		usb_response[usb_index++] = HID_KEY_CAPS_LOCK;
	}
	if(lock_change_required & KEYBOARD_LED_SCROLLLOCK) {
		usb_response[usb_index++] = HID_KEY_SCROLL_LOCK;
	}
	// No idea on how to handle COMPOSE key nor KANA key.
	// if(lock_change_required & KEYBOARD_LED_COMPOSE) {
	//	usb_response[usb_index++] = ???;
	// }
	// if(lock_change_required & KEYBOARD_LED_KANA) {
	// 	usb_response[usb_index++] = ???;
	// }
	return lock_change_required;
}

// For sending key signals when the USB hosts request for it.
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
		// The Num Lock, Caps Lock, etc. that we're gonna modify
		static uint8_t lock_indicator_target = 0;
		static uint8_t lock_indicator_target_mask = 0;
		static uint8_t lock_indicator_original = 0;
		static uint32_t lock_release_wait_counter = 0;
		static enum {
			// Wait for a command (i.e. enum keyboard_output_mode) in the output buffer
			// If a command is detected, toggle lock keys (Num lock, Caps lock, etc.)
			KEY_STEP_WAIT_COMMAND,
			// Release the lock keys. Also wait for the lock key toggle to take effect
			KEY_STEP_TOGGLE_LOCKS_WAIT,
			// Press the modifier keys. For Windows and Mac, it's ALT and it's held. For Linux, it's CTRL+SHIFT+U. For Latin, this step is skipped
			KEY_STEP_PRESS_MODIFIER_KEYS,
			// Release the modifier key so that it'd get taken. For Windows/Mac, the modifier key is to be held and this step is skipped.
			// For Latin, this step is also skipped.
			KEY_STEP_RELEASE_MODIFIER_KEYS,
			// Wait for ASCII characters or KEYBOARD_OUTPUT_MODE_END. Send key presses based on the ASCII character received in the output buffer.
			// Upon KEYBOARD_OUTPUT_MODE_END is received, go to the next step
			KEY_STEP_SEND_KEYS,
			// Release the ALT key for Windows/Mac. Skipped for Linux/Latin
			KEY_STEP_RELEASE_MODIFIER_KEYS_2,
			// Toggle lock key so that it restore back to original state
			KEY_STEP_TOGGLE_LOCKS_2,
			// Release the lock keys. Also wait for the lock key toggle to take effect
			KEY_STEP_TOGGLE_LOCKS_2_WAIT,
		} key_step = KEY_STEP_WAIT_COMMAND;

		// Make a response first! The USB host can't wait
		usb_send_data(usb_response, 8, 0, sendtok);

		// After making the response based on the previous usb_response value, we can slowly build the next usb_response
		uint8_t buffer_read_next_index = 0; // Read the next index after the current one has been processed
		switch(key_step) {
			case KEY_STEP_WAIT_COMMAND:
			{
				uint8_t key_id = keyboard_out_buffer[keyboard_out_buffer_read_index];
				if(keyboard_out_buffer_read_index != keyboard_out_buffer_write_index) {
					// Press the lock keys (num lock, caps lock, etc.) state according to the mode received
					mode = key_id-KEYBOARD_MODE_START;
					switch(mode) {
						case KEYBOARD_OUTPUT_MODE_LATIN:
						case KEYBOARD_OUTPUT_MODE_LINUX:
						case KEYBOARD_OUTPUT_MODE_MACOS:
							// Need to ensure Capslock is inactive
							lock_indicator_target = 0;
							lock_indicator_target_mask = KEYBOARD_LED_CAPSLOCK;
						break;
						case KEYBOARD_OUTPUT_MODE_WINDOWS:
							// Need to ensure Numlock is active
							lock_indicator_target = KEYBOARD_LED_NUMLOCK;
							lock_indicator_target_mask = KEYBOARD_LED_NUMLOCK;
						break;
						case KEYBOARD_OUTPUT_MODE_END:
						default:
							while(1); // Should never reach here!
						break;
					}
					lock_indicator_original = keyboard_locks_indicator;
					usb_handle_user_in_request_toggle_locks(usb_response, keyboard_locks_indicator, lock_indicator_target, lock_indicator_target_mask);
					lock_release_wait_counter = KEYBOADRD_LOCK_CHANGE_TIMEOUT; // Timeout for waiting for the target lock state
					key_step = KEY_STEP_TOGGLE_LOCKS_WAIT;
					buffer_read_next_index = 1;
				}
			}
			break;
			case KEY_STEP_TOGGLE_LOCKS_WAIT:
				// Release lock keys and wait for the lock state to be restored (or timeout)
				memset(usb_response, 0, sizeof(usb_response));
				if(!(--lock_release_wait_counter) || (keyboard_locks_indicator & lock_indicator_target_mask) == lock_indicator_target) {
					if(mode == KEYBOARD_OUTPUT_MODE_LATIN) {
						// For latin mode, we skip KEY_STEP_PRESS_MODIFIER_KEYS and KEY_STEP_RELEASE_MODIFIER_KEYS
						key_step = KEY_STEP_SEND_KEYS;
					} else {
						key_step = KEY_STEP_PRESS_MODIFIER_KEYS;
					}
				}
			break;
			case KEY_STEP_PRESS_MODIFIER_KEYS:
				// Press the modifier keys according to the mode
				switch(mode) {
					case KEYBOARD_OUTPUT_MODE_MACOS:
					case KEYBOARD_OUTPUT_MODE_WINDOWS:
						// The Mac OS Option key is equivalent to KEYBOARD_MODIFIER_LEFTALT
						usb_response[0] = KEYBOARD_MODIFIER_LEFTALT;
						key_step = KEY_STEP_SEND_KEYS;
					break;
					case KEYBOARD_OUTPUT_MODE_LINUX:
						// Send CTRL+SHIFT+U prefix
						usb_response[0] = KEYBOARD_MODIFIER_LEFTCTRL|KEYBOARD_MODIFIER_LEFTSHIFT;
						usb_response[2] = HID_KEY_U;
						key_step = KEY_STEP_RELEASE_MODIFIER_KEYS;
					break;
					case KEYBOARD_OUTPUT_MODE_LATIN:
					case KEYBOARD_OUTPUT_MODE_END:
					default:
						while(1); // Should never happen!
					break;
				}
			break;
			case KEY_STEP_RELEASE_MODIFIER_KEYS:
				// Release the modifier keys
				memset(usb_response, 0, sizeof(usb_response));
				key_step = KEY_STEP_SEND_KEYS;
			break;
			case KEY_STEP_SEND_KEYS:
			{
				uint8_t key_id = keyboard_out_buffer[keyboard_out_buffer_read_index];
				if(keyboard_out_buffer_read_index != keyboard_out_buffer_write_index) {
					if(key_id >= KEYBOARD_MODE_START) {
						// Detect end of input sequence, release keys and go to the next step
						if((key_id-KEYBOARD_MODE_START) == KEYBOARD_OUTPUT_MODE_END) {
							// Update the lock_indicator_target for restoring to the original lock state
							lock_indicator_target = lock_indicator_original & lock_indicator_target_mask;
							switch(mode) {
								case KEYBOARD_OUTPUT_MODE_LATIN:
								case KEYBOARD_OUTPUT_MODE_LINUX:
									// Skip releasing modifier key and just go toggle the lock keys
									key_step = KEY_STEP_TOGGLE_LOCKS_2;
								break;
								case KEYBOARD_OUTPUT_MODE_WINDOWS:
								case KEYBOARD_OUTPUT_MODE_MACOS:
									key_step = KEY_STEP_RELEASE_MODIFIER_KEYS_2;
								break;
								break;
								case KEYBOARD_OUTPUT_MODE_END:
								default:
									while(1); // Should never reach here
								break;
							}
						} else {
							while(1); // Should never reach here
						}
						buffer_read_next_index = 1;
					} else {
						// Process the input sequence
						// For each key, press it once, then release it before processing the next key.
						if(key_release_sent) {
							// Send out the key. Hold right shift if needed
							if(keyboard_ascii_to_keycode[key_id] & KEYHID_SFT) {
								usb_response[0] |= KEYBOARD_MODIFIER_RIGHTSHIFT;
							}
							usb_response[2] = (keyboard_ascii_to_keycode[key_id] & ~KEYHID_SFT);
							key_release_sent = 0;
						} else {
							// Release the right shift and the key
							usb_response[0] &= ~KEYBOARD_MODIFIER_RIGHTSHIFT;
							usb_response[2] = HID_KEY_NONE;
							key_release_sent = 1;
							buffer_read_next_index = 1;
						}
					}
				}
			}
			break;
			case KEY_STEP_RELEASE_MODIFIER_KEYS_2:
				// Stop holding the modifier keys (i.e. ALT for Windows and Mac)
				usb_response[0] = 0x00;
				key_step = KEY_STEP_TOGGLE_LOCKS_2;
			break;
			case KEY_STEP_TOGGLE_LOCKS_2:
				// Press the lock keys
				usb_handle_user_in_request_toggle_locks(usb_response, keyboard_locks_indicator, lock_indicator_target, lock_indicator_target_mask);
				lock_release_wait_counter = KEYBOADRD_LOCK_CHANGE_TIMEOUT; // Timeout for waiting for the target lock state
				key_step = KEY_STEP_TOGGLE_LOCKS_2_WAIT;
			break;
			case KEY_STEP_TOGGLE_LOCKS_2_WAIT:
				// Release lock keys and wait for the lock state to be restored (or timeout)
				memset(usb_response, 0, sizeof(usb_response));
				if(!(--lock_release_wait_counter) || (keyboard_locks_indicator & lock_indicator_target_mask) == lock_indicator_target) {
					key_step = KEY_STEP_WAIT_COMMAND;
				}
			break;
		}
		if(buffer_read_next_index) {
			if(++keyboard_out_buffer_read_index >= sizeof(keyboard_out_buffer)) {
				keyboard_out_buffer_read_index = 0;
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

// Push a mode or ASCII key into the output buffer
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

// Push a lower-case hex value to the output buffer
static void keyboard_push_hex_to_out_buffer(uint32_t codepoint) {
	uint8_t handled_leading_zeros = 0;
	// reverse iteration from 7 to 0 inclusive using unsigned integer
	for(uint32_t i=8; i-->0; ) {
		uint8_t digit = (codepoint & (0xF<<(i*4))) >> (i*4);
		if(!handled_leading_zeros && !digit) {
			continue; // Do not type out leading zeros
		}
		if(digit < 10) {
			keyboard_push_to_out_buffer('0'+digit);
		} else {
			keyboard_push_to_out_buffer('a'+digit-10);
		}
		handled_leading_zeros = 1;
	}
}

void keyboard_write_codepoint(enum keyboard_output_mode mode, uint32_t codepoint) {
	// Force latin mode for first 128 codepoints
	if(codepoint <= 0x7F) {
		mode = KEYBOARD_OUTPUT_MODE_LATIN;
	}
	// Send start of packet with mode information
	keyboard_push_to_out_buffer(KEYBOARD_MODE_START+mode);

	switch(mode) {
		case KEYBOARD_OUTPUT_MODE_LATIN:
			if(codepoint <= 0x7F) {
				// direct output - no conversion needed
				keyboard_push_to_out_buffer(codepoint);
			} else if(codepoint >= KEYBOARD_SITELEN_PONA_CODEPOINT_START &&
				codepoint < KEYBOARD_SITELEN_PONA_CODEPOINT_START+sizeof(KEYBOARD_WORDS_LATIN_MAPPING)/sizeof(*KEYBOARD_WORDS_LATIN_MAPPING)) {
				// Convert sitelen pona codepoint to sitelen Lasin
				uint32_t charcter_id = codepoint-KEYBOARD_SITELEN_PONA_CODEPOINT_START;
				for(size_t i=0; KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]; i++) {
					keyboard_push_to_out_buffer(KEYBOARD_WORDS_LATIN_MAPPING[charcter_id][i]);
				}
			} else {
				// Unsupported codepoint. Let's output a questionmark.
				keyboard_push_to_out_buffer('?');
			}
		break;
		case KEYBOARD_OUTPUT_MODE_WINDOWS:
		{
			uint8_t base10_digits_reserved[10]; // For 32bit unsigned integer codepoint, the max value is 4294967295, which is 10 digits
			size_t base10_digits_index = 0;
			uint32_t unparsed_number = codepoint;
			// Parse the codepoint into base10_digits_reserved[]
			while(unparsed_number) {
				base10_digits_reserved[base10_digits_index++] = (unparsed_number % 10);
				unparsed_number /= 10;
			}
			// Send a numpad 0 (0x10) as prefix to tell Windows that unicode follows
			// Without it, Windows might take the value as non-unicode codepoint and may output an unexpected symbol of other codepages
			keyboard_push_to_out_buffer(0x10);
			// Send each of the digit we just parsed with numpad keys (0x10~0x19 are numpad keys)
			for(size_t i=base10_digits_index; i-->0; ) { // Reverse iteration with unsigned integer i
				keyboard_push_to_out_buffer(0x10+base10_digits_reserved[i]);
			}
		}
		break;
		case KEYBOARD_OUTPUT_MODE_LINUX:
		{
			// Send the codepoint as hex
			keyboard_push_hex_to_out_buffer(codepoint);
			keyboard_push_to_out_buffer(' '); // Press space after complete entering the unicode
		}
		break;
		case KEYBOARD_OUTPUT_MODE_MACOS:
			// Send the codepoint as hex in UTF-16 encoding
			uint32_t utf16_codepoint;
			if(codepoint <= 0xFFFF) {
				utf16_codepoint = codepoint;
			} else if (codepoint <= 0x10FFFF) {
				uint32_t codepoint_base = codepoint - 0x10000U;
				utf16_codepoint = ((0xD800 | ((codepoint_base & (0x3FF<<10)) >> 10)) << 16) | (0xDC00 | (codepoint_base & 0x3FF));
			} else {
				// Outside of UTF-16's range. Let's fill in a middle finger emoji
				utf16_codepoint = 0xD83DDD95;
			}
			keyboard_push_hex_to_out_buffer(utf16_codepoint);
		break;
		case KEYBOARD_OUTPUT_MODE_END:
			while(1); // Should never happen!
		break;
	}

	// Send end of packet with mode information
	keyboard_push_to_out_buffer(KEYBOARD_MODE_START+KEYBOARD_OUTPUT_MODE_END);
}

void keyboard_init(void) {
	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5us ( TDDIS )
	usb_setup();
}
