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
#include "keyboard.h"
#include <stdlib.h>
#include <stdint.h>

uint64_t encode_input_buffer_as_u52(uint8_t input_buffer[12], size_t input_buffer_length) {
	uint64_t ret_u52 = 0;
	if(input_buffer_length > 12) {
		// Input sequence too long. Returning 0
		return 0;
	}

	// Determine if complex mode is required
	for(size_t i=0; i<input_buffer_length; i++) {
		if(input_buffer[i] == ILONENA_KEY_A || input_buffer[i] >= ILONENA_KEY_G) {
			ret_u52 |= LOOKUP_FULL_ENTRY_COMPLEX_MODE;
			if(input_buffer_length > 10) {
				// Input sequence too long for complex mode. Returning 0
				return 0;
			}
			break;
		}
	}

	if(ret_u52 & LOOKUP_FULL_ENTRY_COMPLEX_MODE) {
		// Complex mode encoding
		uint64_t shift = 50-5;
		for(size_t i=0; i<input_buffer_length; i++) {
			ret_u52 |= (uint64_t)input_buffer[i] << shift;
			shift -= 5;
		}
	} else {
		// Simple mode encoding
		uint64_t shift = 48-4;
		for(size_t i=0; i<input_buffer_length; i++) {
			uint32_t key_id = input_buffer[i];
			if(key_id > ILONENA_KEY_A) {
				key_id--;
			}
			ret_u52 |= (uint64_t)key_id << shift;
			shift -= 4;
		}
	}

	return ret_u52;
}

static uint32_t encode_input_buffer_as_u24(uint8_t input_buffer[6], size_t input_buffer_length) {
	if(input_buffer_length > 6) {
		// Invalid input. Returning 0.
		return 0;
	}

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

	uint32_t ret = 0;
	uint32_t target = encode_input_buffer_as_u24(input_buffer, input_buffer_length);
	// Only check COMPACT_TABLE if the input criteria has been met (such that encode_input_buffer_as_u24() returns a valid value)
	if(target != 0) {
		for(size_t i=0; i<LOOKUP_COMPACT_TABLE_LENGTH; i++) {
			if(target == LOOKUP_COMPACT_TABLE[i].input) {
				ret = LOOKUP_CODEPAGE_0_START + LOOKUP_COMPACT_TABLE[i].sitelen_pona_id;
			}
		}
	}

	// Couldn't find the entry in LOOKUP_COMPACT_TABLE. Let's check the other more complicated table
	if(!ret) {
		uint64_t target = encode_input_buffer_as_u52(input_buffer, input_buffer_length);
		for(size_t i=0; i<LOOKUP_FULL_TABLE_LENGTH; i++) {
			if(target == LOOKUP_FULL_TABLE[i].input_u52) {
				switch(LOOKUP_FULL_TABLE[i].codepage) {
					case 0:
						ret = LOOKUP_CODEPAGE_0_START + LOOKUP_FULL_TABLE[i].code_id;
					break;
					case 1:
						ret = LOOKUP_CODEPAGE_1_START + LOOKUP_FULL_TABLE[i].code_id;
					break;
					case 2:
						ret = LOOKUP_CODEPAGE_2_START + LOOKUP_FULL_TABLE[i].code_id;
					break;
					case 3:
						ret = 0; // Reserved for future use
					break;
				}
			}
		}
	}
	return ret;
}

const char* lookup_get_ascii_string(uint8_t codepage, size_t index) {
	const char *ret = NULL;
	switch(codepage) {
		case 0:
			ret = LOOKUP_CODEPAGE_0;
		break;
		case 1:
			ret = LOOKUP_CODEPAGE_1;
		break;
		default:
			return NULL;
	}

	// Scroll past #index amount of NULL terminators, then return the string
	while(index--) {
		while(*ret++);
	}
	return ret;
}

const uint32_t* lookup_get_unicode_string(uint8_t codepage, size_t index) {
	if(codepage == 2) {
		return LOOKUP_CODEPAGE_2[index];
	}
	return NULL;
}
