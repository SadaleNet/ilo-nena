#!/usr/bin/python3

# Copyright 2025 Wong Cho Ching <https://sadale.net>
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
import PIL.Image, PIL.ImageDraw, PIL.ImageFont

if len(sys.argv) < 2:
	print("{sys.argv[0]} <wakalito-7-3-2.yml>")
	exit(1)

WAKALITO_PATH = sys.argv[1]
word_to_codepoint = {}

KEY_MAP = {
	'1': chr(0xF1C89),
	'2': chr(0xF1C9A),
	'3': chr(0xF1C8B),
	'4': chr(0xF1C82),
	'5': chr(0xF1C93),
	'6': chr(0xF1C86),
	'q': chr(0xF1C90),
	'w': chr(0xF1C99),
	'e': chr(0xF1C88),
	'r': chr(0xF1C83),
	't': chr(0xF1C94),
	'y': chr(0xF1C87),
	'a': chr(0xF1C8C),
	's': chr(0xF1C85),
	'd': chr(0xF1C8E),
	'f': chr(0xF1C8D),
	'g': chr(0xF1C9C),
}

# read wakalito input method mapping
with open(WAKALITO_PATH) as f:
	wakalito_mapping = yaml.load(f, yaml.Loader)

for word in wakalito_mapping['matches'][:]:
	# To avoid duplication for the symbols that we're about to add
	if word.get('trigger') in ['3', '6', 'y', 'a', 'g']:
		wakalito_mapping['matches'].remove(word)

	# This one happens on "a a a " in particular. Somehow it has an extra trailing space, to be removed.
	if word.get('replace').strip(' ') != word.get('replace'):
		word['replace'] = word['replace'].strip()

# Add a few special symbols that might have not been specified in the yml file
wakalito_mapping['matches'].insert(0, {'trigger': '3', 'replace': '.', 'word': True})
wakalito_mapping['matches'].insert(0, {'trigger': '6', 'replace': '[', 'word': True})
wakalito_mapping['matches'].insert(0, {'trigger': 'y', 'replace': ']', 'word': True})
wakalito_mapping['matches'].insert(0, {'trigger': 'a', 'replace': ':', 'word': True})
wakalito_mapping['matches'].insert(0, {'trigger': 'g', 'replace': ',', 'word': True})

word_to_wakalito_mapping = {}
multi_word_to_wakalito_mapping = {}
symbol_to_wakalito_mapping = {}

for word in wakalito_mapping['matches']:
	if "trigger" in word:
		triggers = [word["trigger"]]
	elif "triggers" in word:
		triggers = word["triggers"]
	else:
		raise Exception(f"Unable to handle this word due ot lack of trigger: {word}")

	if word['replace'] == '/sp':
		word['replace'] = '■'
	elif word['replace'] == '\n':
		word['replace'] = '↵'
	elif word['replace'] == '...':
		word['replace'] = '…'
	elif word['replace'] == '[':
		word['replace'] = '󱲆'
	elif word['replace'] == ']':
		word['replace'] = '󱲇'
	word['replace'] = word['replace'].replace('(', '<').replace(')', '>')

	entry = []
	for t in triggers:
		skip = False
		for c in t:
			if c not in KEY_MAP:
				skip = True
				break
		if not skip:
			entry.append(''.join([KEY_MAP[c] for c in t]))
	if ' ' in word['replace']:
		multi_word_to_wakalito_mapping[word['replace']] = entry
	elif not word['replace'].isalpha():
		symbol_to_wakalito_mapping[word['replace']] = entry
	else:
		word_to_wakalito_mapping[word['replace']] = entry

word_to_wakalito_mapping.update({k:multi_word_to_wakalito_mapping[k] for k in sorted(multi_word_to_wakalito_mapping)})
word_to_wakalito_mapping.update(symbol_to_wakalito_mapping)

column_counter = 0
height_counter = 0
HEIGHT_LIMIT = 41
for k,v in word_to_wakalito_mapping.items():
	height = (1 + sum([1 if len(i) <= 9 else 2 for i in v]))
	if height_counter + height > HEIGHT_LIMIT:
		print('\n' * (HEIGHT_LIMIT-height_counter), end='')
		height_counter = 0
		column_counter += 1
		if column_counter == 3:
			HEIGHT_LIMIT += 1
	height_counter += height
	print(f"{k} {k.upper() if k.isalpha() else ''}")
	for i in v:
		if len(i) < 9:
			print(f"　{i}")
		elif len(i) == 9:
			print(f"{i}")
		else:
			print(f"　{i}")
print('\n' * (HEIGHT_LIMIT-height_counter), end='')
