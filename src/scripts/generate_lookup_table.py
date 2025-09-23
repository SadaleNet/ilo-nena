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

KEYBOARD_SITELEN_PONA_CODEPOINT_START = 0xF1900

UNICODE_PATH = sys.argv[1]
WAKALITO_PATH = sys.argv[2]
word_to_codepoint = {}

with open(UNICODE_PATH) as f:
	for line in f.readlines():
		line = line.replace('\n', '')
		matched = re.match("([0-9A-F]+)[ ]+SITELEN PONA (.+)", line)
		if matched is None:
			continue
		codepoint = int(matched.group(1), 16)
		word = matched.group(2)
		word_to_codepoint[word] = codepoint


with open(WAKALITO_PATH) as f:
	wakalito_mapping = yaml.load(f, yaml.Loader)

WAKALITO_KEY_VALUES = ' 123456qwertysdf'
wakalito_reversed_mapping = {}

def encode_trigger_as_u24(trigger):
	if len(trigger) > 6:
		raise Exception(f"Trigger sequence too long. Unable to handle: {trigger}")

	ret = 0
	shift = 24-4
	for c in trigger:
		key_id = WAKALITO_KEY_VALUES.find(c)
		if key_id == -1:
			raise Exception(f"Trigger sequence contains invalid character. Unable to handle: {trigger}")
		ret |= key_id << shift
		shift -= 4
	return ret

for word in wakalito_mapping['matches']:
	if "trigger" in word:
		triggers = [word["trigger"]]
	elif "triggers" in word:
		triggers = word["triggers"]
	else:
		raise Exception(f"Unable to handle this word due ot lack of trigger: {word}")
	toki_pona_word_ideograph = f"IDEOGRAPH {word['replace'].upper()}"

	if toki_pona_word_ideograph in word_to_codepoint:
		for i in triggers:
			encoded_trigger1 = encode_trigger_as_u24(i[0:6])
			encoded_trigger2 = encode_trigger_as_u24(i[6:12])
			index = tuple(map(lambda c: WAKALITO_KEY_VALUES.find(c), i))
			if index in wakalito_reversed_mapping:
				raise Exception(f"Duplicate trigger word detected. Unable to handle: {word} vs {wakalito_reversed_mapping[index]}")
			wakalito_reversed_mapping[index] = {"trigger": i, "trigger_enc1": encoded_trigger1, "trigger_enc2": encoded_trigger2, "word": word['replace'], "codepoint": word_to_codepoint[toki_pona_word_ideograph]}
	else:
		print(f"unable to handle: {word}", file=sys.stderr)

print("// This file is generated with generate_lookup_table.py. Do not manually modify.")
print()

print('#include "lookup.h"')
print()
print("// Coverts the vast majority of the characters. Each entry fits in 32bit.")
print("const struct lookup_compact_entry LOOKUP_COMPACT_TABLE[] = {")

wakalito_reversed_mapping_keys = sorted(wakalito_reversed_mapping)
for k in wakalito_reversed_mapping_keys:
	if len(wakalito_reversed_mapping[k]['trigger']) <= 6:
		print(f"\t{{.input = 0x{wakalito_reversed_mapping[k]['trigger_enc1']:06X}U, .sitelen_pona_id=0x{wakalito_reversed_mapping[k]['codepoint']-KEYBOARD_SITELEN_PONA_CODEPOINT_START:02X}U}}, // {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")

print("};")
print()

print("const struct lookup_double_entry LOOKUP_DOUBLE_TABLE[] = {")
for k in wakalito_reversed_mapping_keys:
	if len(wakalito_reversed_mapping[k]['trigger']) > 6 and len(wakalito_reversed_mapping[k]['trigger']) <= 12:
		print(f"\t{{.input_upper = 0x{wakalito_reversed_mapping[k]['trigger_enc1']:06X}U, .input_lower = 0x{wakalito_reversed_mapping[k]['trigger_enc2']:06X}U, .sitelen_pona_id=0x{wakalito_reversed_mapping[k]['codepoint']-KEYBOARD_SITELEN_PONA_CODEPOINT_START:02X}U}}, // {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")

print("};")
print()

for k in wakalito_reversed_mapping_keys:
	if len(wakalito_reversed_mapping[k]['trigger']) > 12:
		print(f"// Unable to encode into table - trigger word too long: {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")
