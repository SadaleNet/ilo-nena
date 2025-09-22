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

WAKALITO_KEY_VALUES = ' 123456qwertyasdfg'
wakalito_reversed_mapping = {}

def encode_trigger_as_u64(trigger):
	ret = 0
	big_shift = 32
	shift = 32-5
	for c in trigger:
		ret |= WAKALITO_KEY_VALUES.find(c) << (big_shift + shift)
		shift -= 5
		if shift < 0:
			if big_shift == 0:
				raise Exception(f"Trigger word too long. Unable to handle: {trigger}")
			big_shift = 0
			shift = 32-5
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
			encoded_trigger = encode_trigger_as_u64(i)
			if encoded_trigger in wakalito_reversed_mapping:
				raise Exception(f"Duplicate trigger word detected. Unable to handle: {word} vs {wakalito_reversed_mapping[encoded_trigger]}")
			wakalito_reversed_mapping[encoded_trigger] = {"trigger": i, "word": word['replace'], "codepoint": word_to_codepoint[toki_pona_word_ideograph]}
	else:
		print(f"unable to handle: {word}", file=sys.stderr)

wakalito_reversed_mapping_keys = sorted(wakalito_reversed_mapping)
for k in wakalito_reversed_mapping_keys:
	print(f"{{.input = {hex(k)}ULL, .codepoint={hex(wakalito_reversed_mapping[k]['codepoint'])}UL}}, // {wakalito_reversed_mapping[k]['trigger']} -> {wakalito_reversed_mapping[k]['word']}")

