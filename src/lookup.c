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

#include "lookup.h"
#include "lookup_generated.h"
#include "keyboard.h"
#include <stdlib.h>
#include <stdint.h>

static uint32_t encode_input_buffer_as_u24(uint8_t input_buffer[6], size_t input_buffer_length) {
	uint32_t ret = 0;
	uint64_t shift = 24-4;
	for(size_t i=0; i<input_buffer_length; i++) {
		uint32_t key_id = input_buffer[i];
		if(key_id == ILONENA_KEY_A || key_id >= ILONENA_KEY_G) {
			// Invalid input. Returning 0.
			return 0;
		} else if(key_id > ILONENA_KEY_A) {
			key_id--;
		}
		ret |= key_id << shift;
		shift -= 4;
	}
	return ret;
}

uint32_t lookup_search(uint8_t input_buffer[12], size_t input_buffer_length) {
	if(input_buffer_length <= 0 || input_buffer_length > 12) {
		return 0;
	}

	if(input_buffer_length < 6) {
		uint32_t target = encode_input_buffer_as_u24(input_buffer, input_buffer_length);
		for(size_t i=0; i<sizeof(LOOKUP_COMPACT_TABLE)/sizeof(*LOOKUP_COMPACT_TABLE); i++) {
			if(target == LOOKUP_COMPACT_TABLE[i].input) {
				return KEYBOARD_SITELEN_PONA_CODEPOINT_START + LOOKUP_COMPACT_TABLE[i].sitelen_pona_id;
			}
		}
	} else {
		uint32_t target_upper = encode_input_buffer_as_u24(input_buffer, 6);
		uint32_t target_lower = encode_input_buffer_as_u24(&input_buffer[6], input_buffer_length-6);
		for(size_t i=0; i<sizeof(LOOKUP_DOUBLE_TABLE)/sizeof(*LOOKUP_DOUBLE_TABLE); i++) {
			if(target_upper == LOOKUP_DOUBLE_TABLE[i].input_upper && target_lower == LOOKUP_DOUBLE_TABLE[i].input_lower) {
				return KEYBOARD_SITELEN_PONA_CODEPOINT_START + LOOKUP_DOUBLE_TABLE[i].sitelen_pona_id;
			}
		}
	}
	return 0;
}
