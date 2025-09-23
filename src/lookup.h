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

#ifndef ILONENA_LOOKUP_H
#define ILONENA_LOOKUP_H

#include <stdint.h>
#include <stdlib.h> // For size_t

#define LOOKUP_INPUT_LENGTH_MAX (12)

enum ilonena_key_id {
	ILONENA_KEY_NONE,
	ILONENA_KEY_1,
	ILONENA_KEY_2,
	ILONENA_KEY_3,
	ILONENA_KEY_4,
	ILONENA_KEY_5,
	ILONENA_KEY_6,
	ILONENA_KEY_Q,
	ILONENA_KEY_W,
	ILONENA_KEY_E,
	ILONENA_KEY_R,
	ILONENA_KEY_T,
	ILONENA_KEY_Y,
	ILONENA_KEY_A, // colon
	ILONENA_KEY_S,
	ILONENA_KEY_D,
	ILONENA_KEY_F,
	ILONENA_KEY_G, // comma
	ILONENA_KEY_ALA,
	ILONENA_KEY_WEKA,
	ILONENA_KEY_PANA,
};

// Compact entry: stores up to 6 input sequence (cannot store colon nor comma in input sequence), can only output a 8-bit sitelen pona id
struct __attribute__((__packed__)) lookup_compact_entry {
	uint32_t input:24;
	uint8_t sitelen_pona_id; // ID starting from KEYBOARD_SITELEN_PONA_CODEPOINT_START
};

// Double entry: stores up to 12 input sequence (cannot store colon nor comma in input sequence), can only output a 8-bit sitelen pona id
struct __attribute__((__packed__)) lookup_double_entry {
	uint32_t input_upper:24;
	uint8_t padding;
	uint32_t input_lower:24;
	uint8_t sitelen_pona_id; // ID starting from KEYBOARD_SITELEN_PONA_CODEPOINT_START
};

uint32_t lookup_search(uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX], size_t input_buffer_length);

#endif
