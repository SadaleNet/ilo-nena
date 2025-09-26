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
	uint32_t input:24; // each key takes 4 bits. Cannot encode colon nor comma.
	uint8_t sitelen_pona_id; // ID starting from KEYBOARD_SITELEN_PONA_CODEPOINT_START
};

#define LOOKUP_FULL_ENTRY_COMPLEX_MODE ((uint64_t)1ULL<<51) // Complex mode allows encoding colon and comma, at the cost of shorter input limit

// Full entry: stores either up to 12 input sequence (w/o colon nor comma) or 10 input sequence (w/ colon or comma), can output id of 3 different pages.
// To output a string, use virtual codepoints
struct __attribute__((__packed__)) lookup_full_entry {
	uint64_t input_u52:52; // Stores either a) 12 input sequence without colon nor comma or b) 10 input sequence with colon or comma.
	uint8_t codepage:2; // 0 - sitelen pona table. 1 - ASCII string table. 2 - Unicode string table. 3 - reserved
	uint8_t padding:2; // reserved
	uint8_t code_id;
};

uint32_t lookup_search(uint8_t input_buffer[LOOKUP_INPUT_LENGTH_MAX], size_t input_buffer_length);

// These variables are defined in generated.c
extern const uint32_t LOOKUP_CODEPAGE_0_START;
extern const size_t LOOKUP_CODEPAGE_0_LENGTH;
extern const uint32_t LOOKUP_CODEPAGE_1_START;
extern const size_t LOOKUP_CODEPAGE_1_LENGTH;
extern const uint32_t LOOKUP_CODEPAGE_2_START;
extern const size_t LOOKUP_CODEPAGE_2_LENGTH;
extern const char *LOOKUP_CODEPAGE_0[];
extern const char *LOOKUP_CODEPAGE_1[];
extern const uint32_t *LOOKUP_CODEPAGE_2[];

extern const struct lookup_compact_entry LOOKUP_COMPACT_TABLE[];
extern const size_t LOOKUP_COMPACT_TABLE_LENGTH;
extern const struct lookup_full_entry LOOKUP_FULL_TABLE[];
extern const size_t LOOKUP_FULL_TABLE_LENGTH;

#endif
