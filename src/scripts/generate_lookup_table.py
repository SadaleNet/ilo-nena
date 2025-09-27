#!/usr/bin/python3

# Copyright 2025 Wong Cho Ching <https:#sadale.net>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import re
import sys
import yaml

if len(sys.argv) < 3:
	print("{sys.argv[0]} <kreativekorp_ucsur_charts_sitelen.html> <wakalito-7-3-2.yml>")
	exit(1)

def c_style_escape(s):
	return repr(s)[1:-1].replace('"', '\\\"')

# code page 0. Fixed with sitelen pona's content
KEYBOARD_SITELEN_PONA_CODEPOINT_START = 0xF1900

# code page 1 and 2. The content are dynamically filled
KEYBOARD_CODEPAGE_1_START = 0xFFFF0000
KEYBOARD_CODEPAGE_2_START = 0xFFFF1000
codepage_0_map = {} # maps codepoint to word
codepage_1 = []
codepage_2 = []

UNICODE_PATH = sys.argv[1]
WAKALITO_PATH = sys.argv[2]
word_to_codepoint = {}

SYMBOL_MAP = {
	'START OF CARTOUCHE': '[',
	'END OF CARTOUCHE': ']',
	'MIDDLE DOT': '.',
	'COLON': ':',
}

# Read sitelen pona unicode mapping (codepage 0)
with open(UNICODE_PATH) as f:
	for line in f.readlines():
		line = line.replace('\n', '')
		matched = re.match("([0-9A-F]+)[ ]+SITELEN PONA (.+)", line)
		if matched is None:
			continue
		codepoint = int(matched.group(1), 16)
		word = matched.group(2)
		if word.startswith('IDEOGRAPH '):
			processed_word = word.replace('IDEOGRAPH ','').lower()
			word_to_codepoint[processed_word] = codepoint
			codepage_0_map[codepoint-KEYBOARD_SITELEN_PONA_CODEPOINT_START] = processed_word
		elif word in SYMBOL_MAP.keys():
			symbol = SYMBOL_MAP[word]
			word_to_codepoint[symbol] = codepoint
			codepage_0_map[codepoint-KEYBOARD_SITELEN_PONA_CODEPOINT_START] = symbol


# read wakalito input method mapping
with open(WAKALITO_PATH) as f:
	wakalito_mapping = yaml.load(f, yaml.Loader)

symbols_defined = {c:False for c in ".[]:,"}


# Add a few special symbols that might have not been specified in the yml file
for word in wakalito_mapping['matches'][:]:
	if word.get('trigger') in ['3', '6', 'y', 'a', 'g']:
		wakalito_mapping['matches'].remove(word)	

wakalito_mapping['matches'].append({'trigger': '3', 'replace': '.', 'word': True})
wakalito_mapping['matches'].append({'trigger': '6', 'replace': '[', 'word': True})
wakalito_mapping['matches'].append({'trigger': 'y', 'replace': ']', 'word': True})
wakalito_mapping['matches'].append({'trigger': 'a', 'replace': ':', 'word': True})
wakalito_mapping['matches'].append({'trigger': 'g', 'replace': ',', 'word': True})


def process_output_word(word):
	ret = None
	if word == '':
		raise Exception("Input word must not be empty")

	if word == '/sp':
		ret = '\u3000'

	if ret == None:
		sitelen_pona_phrase = ''
		for w in word.strip().split(' '):
			codepoint = word_to_codepoint.get(w)
			if codepoint is None:
				sitelen_pona_phrase = None
				break
			sitelen_pona_phrase += chr(codepoint)

		if sitelen_pona_phrase is not None:
			ret = sitelen_pona_phrase

	if ret == None:
		ret = word

	has_unicode = len([i for i in ret if ord(i) > 128]) > 0
	return (has_unicode, ret)

# maps between code page 1's content and their codepoint - ASCII string
word_to_codepoint_codepage_1 = {}
# maps between code page 2's content and their codepoint - unicoed string
word_to_codepoint_codepage_2 = {}
# add virtual codepoints for code page 1 and 2
for word in wakalito_mapping['matches']:
	w = word['replace']
	if w not in word_to_codepoint:
		has_unicode, string = process_output_word(w)
		if not has_unicode:
			word_to_codepoint_codepage_1[w] = len(codepage_1)
			codepage_1.append(string)
		else:
			word_to_codepoint_codepage_2[w] = len(codepage_2)
			codepage_2.append(string)

WAKALITO_KEY_VALUES = ' 123456qwertysdf'
WAKALITO_KEY_VALUES_FULL = ' 123456qwertyasdfg'
wakalito_reversed_mapping = {}

def encode_trigger_as_u52(trigger):
	if len(trigger) > 12:
		return 0

	ret = 0
	complex_mode = False
	for c in trigger:
		if c not in WAKALITO_KEY_VALUES:
			if c not in WAKALITO_KEY_VALUES_FULL or len(trigger) > 10:
				# Invalid input. Returning zero
				return 0
			complex_mode = True
			break

	if complex_mode:
		ret |= (1 << 51)
		shift = 50-5
		for c in trigger:
			ret |= WAKALITO_KEY_VALUES_FULL.find(c) << shift
			shift -= 5
	else:
		shift = 48-4
		for c in trigger:
			key_id = WAKALITO_KEY_VALUES.find(c)
			ret |= key_id << shift
			shift -= 4

	return ret

def encode_trigger_as_u24(trigger):
	if len(trigger) > 6:
		return 0

	ret = 0
	shift = 24-4
	for c in trigger:
		key_id = WAKALITO_KEY_VALUES.find(c)
		if key_id == -1:
			return 0
		ret |= key_id << shift
		shift -= 4
	return ret

def get_codepage_and_codepoint(word):
	if word in word_to_codepoint:
		return (0, word_to_codepoint[word]-KEYBOARD_SITELEN_PONA_CODEPOINT_START)
	elif word in word_to_codepoint_codepage_1:
		return (1, word_to_codepoint_codepage_1[word])
	elif word in word_to_codepoint_codepage_2:
		return (2, word_to_codepoint_codepage_2[word])
	return None

for word in wakalito_mapping['matches']:
	if "trigger" in word:
		triggers = [word["trigger"]]
	elif "triggers" in word:
		triggers = word["triggers"]
	else:
		raise Exception(f"Unable to handle this word due ot lack of trigger: {word}")
	codepage, codepoint = get_codepage_and_codepoint(word['replace'])
	for i in triggers:
		encoded_trigger_u24 = encode_trigger_as_u24(i) if codepage == 0 else 0
		encoded_trigger_u52 = encode_trigger_as_u52(i)
		if encoded_trigger_u52 == 0:
			continue # Unable to encode. Skipping!
		if encoded_trigger_u52 in wakalito_reversed_mapping:
			raise Exception(f"Error: Duplicate trigger word detected: {i}")
		wakalito_reversed_mapping[encoded_trigger_u52] = {"trigger": i, "trigger_u24": encoded_trigger_u24, "trigger_u52": encoded_trigger_u52, "word": c_style_escape(word['replace']), "codepage": codepage, "codepoint": codepoint}

print("// This file was generated with generate_lookup_table.py. Do not manually modify.")
print("// This project's constrained by the flash size. Sorry for the unintuitive design!")
print()

print('#include "lookup.h"')
print('#include <stdint.h>')
print()

print("// codepage 0 - sitelen pona")
print(f"const uint32_t LOOKUP_CODEPAGE_0_START = 0x{KEYBOARD_SITELEN_PONA_CODEPOINT_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_0_LENGTH = {max(word_to_codepoint.values())-KEYBOARD_SITELEN_PONA_CODEPOINT_START+1};")

print("// codepage 1 - ASCII strings")
print(f"const uint32_t LOOKUP_CODEPAGE_1_START = 0x{KEYBOARD_CODEPAGE_1_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_1_LENGTH = {len(codepage_1)};")

print("// codepage 2 - Unicode strings")
print(f"const uint32_t LOOKUP_CODEPAGE_2_START = 0x{KEYBOARD_CODEPAGE_2_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_2_LENGTH = {len(codepage_2)};")
print()

print("const char *LOOKUP_CODEPAGE_0 = ")
codepage_0_size = max(word_to_codepoint.values())-KEYBOARD_SITELEN_PONA_CODEPOINT_START+1
for i in range(codepage_0_size):
	null_terminator = "\\0" if i < codepage_0_size-1 else ""
	if i in codepage_0_map:
		print(f'\t"{codepage_0_map[i]}{null_terminator}"')
	else:
		print(f'\t"{null_terminator}"')
print(";")
print()

print("const char *LOOKUP_CODEPAGE_1 = ")
for i, w in enumerate(codepage_1):
	null_terminator = "\\0" if i < len(codepage_1)-1 else ""
	print(f'\t"{c_style_escape(w)}{null_terminator}"')
print(";")
print()

print("const uint32_t *LOOKUP_CODEPAGE_2[] = {")
for i in codepage_2:
	buf = "\t(const uint32_t[]){"
	for c in i:
		buf += f'0x{ord(c):08X}U,'
	buf += "0},"
	print(buf)
print("};")
print()

print("// Covers the vast majority of the characters. Each entry fits in 32bit.")
print("const struct lookup_compact_entry LOOKUP_COMPACT_TABLE[] = {")

wakalito_reversed_mapping_keys = sorted(wakalito_reversed_mapping)
for k in wakalito_reversed_mapping_keys:
	if wakalito_reversed_mapping[k]['trigger_u24']:
		print(f"\t{{.input = 0x{wakalito_reversed_mapping[k]['trigger_u24']:06X}U, .sitelen_pona_id=0x{wakalito_reversed_mapping[k]['codepoint']:02X}U}}, // {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")

print("};")
print()
print(f"const size_t LOOKUP_COMPACT_TABLE_LENGTH = sizeof(LOOKUP_COMPACT_TABLE)/sizeof(*LOOKUP_COMPACT_TABLE);")
print()

print("// Covers the other characters/strings that requires up to 12 input letters. Can encode Each entry is 64bit.")
print("const struct lookup_full_entry LOOKUP_FULL_TABLE[] = {")
for k in wakalito_reversed_mapping_keys:
	if wakalito_reversed_mapping[k]['trigger_u24'] == 0 and wakalito_reversed_mapping[k]['trigger_u52']:
		print(f"\t{{.input_u52 = 0x{wakalito_reversed_mapping[k]['trigger_u52']:013X}ULL, .codepage={wakalito_reversed_mapping[k]['codepage']}, .code_id=0x{wakalito_reversed_mapping[k]['codepoint']:02X}U}}, // {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")

print("};")
print()

print(f"const size_t LOOKUP_FULL_TABLE_LENGTH = sizeof(LOOKUP_FULL_TABLE)/sizeof(*LOOKUP_FULL_TABLE);")
print()
