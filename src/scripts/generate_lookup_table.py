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

if len(sys.argv) < 4:
	print("{sys.argv[0]} <kreativekorp_ucsur_charts_sitelen.html> <wakalito-7-3-2.yml> <lekolili15x15.ttf>")
	exit(1)

#####################
## TEXT GENERATION ##
#####################

def c_style_escape(s):
	return repr(s)[1:-1].replace('"', '\\\"')

# code page 0. Fixed with sitelen pona's content
KEYBOARD_SITELEN_PONA_CODEPOINT_START = 0xF1900
KEYBOARD_CODEPAGE_0_START = KEYBOARD_SITELEN_PONA_CODEPOINT_START

# code page 1 and 2. The content are dynamically filled
KEYBOARD_CODEPAGE_1_START = 0xFFFF0000
KEYBOARD_CODEPAGE_2_START = 0xFFFF1000

# code page 3 - for showing internal images that won't be sent out to the USB keyboard interface
KEYBOARD_CODEPAGE_3_START = 0xFFFF2000
codepage_0_map = {} # maps codepoint to word
codepage_1 = []
codepage_2 = []

UNICODE_PATH = sys.argv[1]
WAKALITO_PATH = sys.argv[2]
FONT_PATH = sys.argv[3]
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


for word in wakalito_mapping['matches'][:]:
	# To avoid duplication for the symbols that we're about to add
	if word.get('trigger') in ['3', '6', 'y', 'a', 'g']:
		wakalito_mapping['matches'].remove(word)

	# This one happens on "a a a " in particular. Somehow it has an extra trailing space, to be removed.
	if word.get('replace').strip(' ') != word.get('replace'):
		word['replace'] = word['replace'].strip()

# Add a few special symbols that might have not been specified in the yml file
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
# maps between code page 2's content and their codepoint - unicode string
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
print("// The content in this file were generated using files from external source, including the follows:")
print("// 1. UCSUR's entry of sitelen pona - https://www.kreativekorp.com/ucsur/charts/sitelen.html")
print("// 2. The yml file of nasin sitelen Wakalito published to archive.org - https://archive.org/details/nasin_sitelen_Wakalito")
print("// 3. leko lili 15x15 font - https://toki.pona.billsmugs.com/lipu-tenpo/2022-05-15-sitelen_pona/ . For the missing glyphs, I've drawn them right inside the source file.")
print()
print("// The license of the UCSUR's entry is unknown. But I don't see a reason why I can't use it. It's a standard so it's meant to be used.")
print("// As for the license of nasin sitelen Wakalito's input mapping, I've talked with the author of Wakalito.")
print("// They're ok with me making a Wakalito keybaord as long as it isn't made with profit in mind.")
print("// The license of font is CC0, a.k.a public domain.")
print()

print('#include "lookup.h"')
print('#include <stdint.h>')
print()

codepage_0_size = max(word_to_codepoint.values())-KEYBOARD_SITELEN_PONA_CODEPOINT_START+1
print("// codepage 0 - sitelen pona")
print(f"const uint32_t LOOKUP_CODEPAGE_0_START = 0x{KEYBOARD_SITELEN_PONA_CODEPOINT_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_0_LENGTH = {codepage_0_size};")

print("// codepage 1 - ASCII strings")
print(f"const uint32_t LOOKUP_CODEPAGE_1_START = 0x{KEYBOARD_CODEPAGE_1_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_1_LENGTH = {len(codepage_1)};")

print("// codepage 2 - Unicode strings")
print(f"const uint32_t LOOKUP_CODEPAGE_2_START = 0x{KEYBOARD_CODEPAGE_2_START:08X}U;")
print(f"const size_t LOOKUP_CODEPAGE_2_LENGTH = {len(codepage_2)};")
print()

print("// Intentionally not using array of string to save FLASH space. Can save 4 bytes for each entry this way.")
print("const char *LOOKUP_CODEPAGE_0 = ")
for i in range(codepage_0_size):
	null_terminator = "\\0" if i < codepage_0_size-1 else ""
	if i in codepage_0_map:
		print(f'\t"{codepage_0_map[i]}{null_terminator}"')
	else:
		print(f'\t"{null_terminator}"')
print(";")
print()

print("// Intentionally not using array of string to save FLASH space. Can save 4 bytes for each entry this way.")
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




#####################
## FONT GENERATION ##
#####################

font = PIL.ImageFont.truetype(FONT_PATH, 16)

def get_font_data_by_rendering(codepoint):
	image = PIL.Image.new('1', (16, 16), color=0)
	draw = PIL.ImageDraw.Draw(image)
	draw.text((0, -1), chr(codepoint), font=font, fill=1)

	data = image.getdata()

	output_data = [[0 for i in range(16)] for i in range(16)]
	for i, pixel in enumerate(data):
		x = i%16
		y = i//16
		output_data[y][x] = pixel
	image.close()
	return output_data

def build_font_data(s):
	s = s[1:-1] # Remove the first \n amd the final \n

	output_data = [[0 for i in range(16)] for i in range(16)]
	for y, row in enumerate(s.split('\n')):
		for x, pixel in enumerate(row):
			if x < 16 and y < 16:
				output_data[y][x] = 1 if pixel == 'X' else 0

	return output_data

def font_data_to_u8_array(data):
	image_u16 = [0 for i in range(15)]
	for y, row in enumerate(data):
		if y >= 15:
			continue
		for x, pixel in enumerate(row):
			if x >= 15:
				continue
			image_u16[x] |= (pixel << y)

	ret = b''
	for u16 in image_u16:
		ret += (u16 & 0xFF).to_bytes()
		ret += ((u16 >> 8) & 0xFF).to_bytes()
	return ret


def font_compress(data):
	image_data = [data[i*2] | (data[i*2+1] << 8) for i in range(len(data)//2)]

	# Detect if the image is symmetric
	mirrored = True
	for i in range(7):
		if image_data[i] != image_data[14-i]:
			mirrored = False

	# Detect for black border on the sides. i.e. cropping
	start_column = 0
	for i in range(8):
		if image_data[i] == 0 and image_data[14-i] == 0:
			start_column += 1
		else:
			break

	if start_column == 8:
		return (0).to_bytes() # empty image. just return zero

	data_nibbles = [] # each array element has a 4-bit data

	# dictionary-based compression. The dictionary stores most recent 8 recorded columns
	# Since each column is 15 pixels and each column is 16bit, we use the bit0 to tell
	# if the column is using dictionary or a direct definition
	dictionary = [None for i in range(8)]
	dictionary_index = 0
	for u16 in image_data[start_column:(15-start_column if not mirrored else 8)]:
		if u16 in dictionary:
			# Use dictionary
			data_nibbles.append((dictionary.index(u16) << 1) | 0x01)
		else:
			# Direct definition of the column content
			dictionary[dictionary_index%8] = u16
			dictionary_index += 1
			u16 <<= 1 # the first bit is zero, indicating that the column isn't compressed
			data_nibbles.append(u16 & 0xF)
			data_nibbles.append((u16 >> 4) & 0xF)
			data_nibbles.append((u16 >> 8) & 0xF)
			data_nibbles.append((u16 >> 12) & 0xF)

	# Pad it to a byte
	if len(data_nibbles)%2 != 0:
		data_nibbles.append(0x0)

	# Validation
	payload_size = len(data_nibbles)//2
	if payload_size > 0b11111:
		raise Exception("payload too large!")

	for nibble in data_nibbles:
		if nibble < 0x0 or nibble > 0xF:
			raise Exception("data_nibbles in unexpected range")

	ret = ((start_column<<5)|payload_size).to_bytes() # first byte is metadata
	# the subsequent bytes are compressed data
	for i in range(payload_size):
		ret += (data_nibbles[i*2] | (data_nibbles[i*2+1] << 4)).to_bytes()
	return ret

def font_decompress(data):
	start_col = (data[0] & 0xE0) >> 5
	payload_length = data[0] & 0x1F

	image = [0x0000 for i in range(15)]
	if payload_length > 0:
		data_nibbles = []
		for b in data[1:]:
			data_nibbles.append(b & 0xF)
			data_nibbles.append((b >> 4) & 0xF)
		col = start_col
		end_col = 14-start_col # inclusive!

		dictionary = [None for i in range(8)]
		dictionary_index = 0
		i = 0
		while i < len(data_nibbles) and col <= end_col:
			if data_nibbles[i] & 0x1:
				# dictionary-mapped column
				image[col] = dictionary[data_nibbles[i]>>1]
				col += 1
				i += 1
			elif i+1 < len(data_nibbles):
				# non-compressed column. Just read it directly.
				image[col] |= data_nibbles[i]
				image[col] |= data_nibbles[i+1] << 4
				image[col] |= data_nibbles[i+2] << 8
				image[col] |= data_nibbles[i+3] << 12
				image[col] >>= 1
				dictionary[dictionary_index%8] = image[col]
				dictionary_index += 1
				col += 1
				i += 4
			else:
				break # Padding nibble!

		# Symmetric image! Let's draw the second half that's mirrored with the first half.
		if col == 8:
			for i in range(col, end_col+1):
				image[i] = image[14-i]

	ret = b''
	for u16 in image:
		ret += (u16 & 0xFF).to_bytes()
		ret += ((u16 & 0xFF00)>>8).to_bytes()
	return ret

font_data = {}

for i in range(0xF1900, 0xF1988+1):
	font_data[i] = get_font_data_by_rendering(i)

# Replacement character for missing glyphs
font_data[0] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[0xF1990] = build_font_data('''
______XXXXX____-
_____X_________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
_____X_________-
______XXXXX____-
----------------
''')

font_data[0xF1991] = build_font_data('''
____XXXXX______-
_________X_____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
_________XX____-
____XXXXX______-
----------------
''')

font_data[0xF199C] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______X_______-
______XXX______-
_______X_______-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')


font_data[0xF199D] = build_font_data('''
_______________-
_______________-
_______________-
_______X_______-
______XXX______-
_______X_______-
_______________-
_______________-
_______________-
_______X_______-
______XXX______-
_______X_______-
_______________-
_______________-
_______________-
----------------
''')

font_data[0xF19A0] = build_font_data('''
XXXXXXXXXXXXXXX-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
----------------
''')

font_data[0xF19A1] = build_font_data('''
__XXX_____XXX__-
_X___X___X___X_-
X_____X_X_____X-
X_____X_X_____X-
X_____X_X_____X-
_X___X___X___X_-
__XXX_XXX_XXX__-
____XX___XX____-
__XX__XXX__XX__-
XX___X___X___XX-
____X_____X____-
____X_____X____-
____X_____X____-
_____X___X_____-
______XXX______-
----------------
''')

font_data[0xF19A2] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
XXXXX_____XXXXX-
_______________-
_______________-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______________-
_______________-
_______________-
----------------
''')

font_data[0xF19A3] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
______X_X______-
_______X_______-
______X_X______-
_______________-
_______________-
_______________-
XXXXXXXXXXXXXXX-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["\n"]] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
______________X-
___X__________X-
__X___________X-
_X____________X-
XXXXXXXXXXXXXXX-
_X_____________-
__X____________-
___X___________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["-"]] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_____XXXXX_____-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1['"']] = build_font_data('''
_____X___X_____-
_____X___X_____-
_____X___X_____-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1[","]] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_________X_____-
________X______-
_______X_______-
______X________-
_____X_________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["__"]] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
XXXXXXXXXXXXXXX-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["..."]] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
___X___X___X___-
__XXX_XXX_XXX__-
___X___X___X___-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1[":)"]] = build_font_data('''
________X______-
_________X_____-
__________X____-
____X______X___-
___XXX_____X___-
____X______X___-
___________X___-
___________X___-
___________X___-
____X______X___-
___XXX_____X___-
____X______X___-
__________X____-
_________X_____-
________X______-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1[":("]] = build_font_data('''
___________X___-
__________X____-
_________X_____-
____X___X______-
___XXX__X______-
____X___X______-
________X______-
________X______-
________X______-
____X___X______-
___XXX__X______-
____X___X______-
_________X_____-
__________X____-
___________X___-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1[":|"]] = build_font_data('''
__________X____-
__________X____-
__________X____-
____X_____X____-
___XXX____X____-
____X_____X____-
__________X____-
__________X____-
__________X____-
____X_____X____-
___XXX____X____-
____X_____X____-
__________X____-
__________X____-
__________X____-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1[":v"]] = build_font_data('''
_______________-
_______________-
_______________-
___X___________-
__XXX_X_____X__-
___X__X_____X__-
_______X___X___-
_______X___X___-
________X_X____-
___X____X_X____-
__XXX____X_____-
___X_____X_____-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["oke"]] = build_font_data('''
_______________-
____________X__-
____________X__-
___________X___-
___________X___-
_____XXXXXX____-
____X_____X____-
___X_____X_X___-
___X_X___X_X___-
___X__X_X__X___-
___X__X_X__X___-
___X___X___X___-
____X__X__X____-
_____XXXXX_____-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["isipin"]] = build_font_data('''
_______X_______-
_X_____X_____X_-
__X____X____X__-
___X_______X___-
_____XXXXX_____-
____X_____X____-
___X_______X___-
__X_________X__-
__XXXXXXXXXXXXX-
__X_________X__-
__X_________X__-
__X_________X__-
___X_______X___-
____X_____X____-
_____XXXXX_____-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["kapesi"]] = build_font_data('''
______XXX______-
_____X_X_X_____-
____X__X__X____-
___X___X___X___-
___XXXXXXXXX___-
___X___X___X___-
____X__X__X____-
_____X_X_X_____-
______XXX______-
_______X_______-
______XXX______-
_____X___X_____-
____X_____X____-
___X_______X___-
__XXXXXXXXXXX__-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["kiki"]] = build_font_data('''
__X______X_____-
__XX____X_XX___-
__X_XX_X____X__-
__X___X______XX-
__X________XX__-
___X____XXX____-
___X______X____-
____X______X___-
_____XX_____X__-
___XX________XX-
_XX_____X___X__-
X___X___X___X__-
_XXXX__X_XX__X_-
____X_X____XXX_-
_____X_______X_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["linluwi"]] = build_font_data('''
_XXXXXXXXXXXXX_-
_X___________X_-
_X___________X_-
_X_X___X___X_X_-
_XXXX_XXX_XXXX_-
_X_X___X___X_X_-
_X___________X_-
_X___________X_-
_X_XXX_______X_-
_XX___X______X_-
_X_____X_____X_-
_X_____XX___XX_-
_X_____X_XXX_X_-
_X_____X_____X_-
_X_____X_____X_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["mulapisu"]] = build_font_data('''
_______X_______-
_______X_______-
______X_X______-
______X_X______-
_____X___X_____-
_____X___X_____-
____X__X__X____-
____X_X_X_X____-
___X___X___X___-
___X_______X___-
__X__X___X__X__-
__X_X_X_X_X_X__-
_X___X___X___X_-
_X___________X_-
XXXXXXXXXXXXXXX-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["Pingo"]] = build_font_data('''
XXXXXXXXXX_____-
__X_______XX___-
__X_________X__-
__X__________X_-
__X___________X-
__X___________X-
__X__________X_-
__X_________X__-
__XXXXXXXXXX___-
__X____________-
__X____________-
__X____________-
__X____________-
__X____________-
_XXX___________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["unu"]] = build_font_data('''
____XXXX_______-
___X____XX_____-
____XX____X____-
______X____X___-
______X____X___-
______X____X___-
____XX____X____-
___X____XX_____-
____XXXXX______-
_______X_______-
______XXX______-
_____X___X_____-
____X_____X____-
___X_______X___-
__XXXXXXXXXXX__-
----------------
''')

font_data[KEYBOARD_CODEPAGE_1_START+word_to_codepoint_codepage_1["wa"]] = build_font_data('''
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______________-
_______________-
___X___X___X___-
___X___X___X___-
___X___X___X___-
____X_X_X_X____-
_____X___X_____-
----------------
''')


font_data[KEYBOARD_CODEPAGE_2_START+word_to_codepoint_codepage_2["/sp"]] = build_font_data('''
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
XXXXXXXXXXXXXXX-
----------------
''')

font_data[KEYBOARD_CODEPAGE_2_START+word_to_codepoint_codepage_2["a a a"]] = build_font_data('''
_______X_______-
_______X_______-
_______________-
______XXXX_____-
_____X___X_____-
_____X___X_____-
__X__X__XX__X__-
__X___XX_X__X__-
__X_________X__-
_______________-
__XXXX____XXXX_-
_X___X___X___X_-
_X___X___X___X_-
_X__XX___X__XX_-
__XX_X____XX_X_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_2_START+word_to_codepoint_codepage_2["mi sona ala"]] = build_font_data('''
______XX_______-
_____X__X______-
_____X__X______-
_____XXX_______-
_____X_________-
______X________-
___X___________-
X__X__X_X_____X-
_X___X___X___X_-
__________X_X__-
_XXXXX_____X___-
_X___X____X_X__-
_X___X___X___X_-
_X___X__X_____X-
_XXXXX_________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+0] = build_font_data('''
___    _______-
____XXXX_______-
________XX_____-
__________X____-
__________X____-
___________X___-
___________X___-
___________X___-
___________X___-
___________X___-
__________X____-
__________X____-
________XX_____-
____XXXX_______-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+1] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_XXXXXXXXXXXXX_-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+2] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
______XXX______-
_____XXXXX_____-
_____XXXXX_____-
_____XXXXX_____-
______XXX______-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+3] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_____X_________-
____X__________-
___X___________-
__X___X_____X__-
___X___X___X___-
____X___X_X____-
_____X___X_____-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+4] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_____XXXXX_____-
___XX_____XX___-
__X_________X__-
__X_________X__-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+5] = build_font_data('''
_______________-
____XXXXXXX____-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____X__________-
____XXXXXXX____-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+6] = build_font_data('''
_______________-
_______XXXX____-
______X____X___-
_____X______X__-
____X_______X__-
____X_______X__-
___X_________X_-
___X_________X_-
___X_________X_-
___X_________X_-
___X_________X_-
_X_X_________X_-
__XX_________X_-
___X_________X_-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+7] = build_font_data('''
_______________-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______X_______-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+8] = build_font_data('''
_______________-
______XXX______-
____XX___XX____-
___X_______X___-
__X_________X__-
__X_________X__-
_X___________X_-
_X___________X_-
_X___________X_-
__X_________X__-
__X_________X__-
___X_______X___-
____XX___XX____-
______XXX______-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+9] = build_font_data('''
_______________-
_______________-
_______________-
_________X_____-
_____X____X____-
____X_X____X___-
___X___X____X__-
__X_____X__X___-
__________X____-
_________X_____-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+10] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
X_____________X-
X_____________X-
X_____________X-
_X___________X_-
_X___________X_-
__X_________X__-
___XX_____XX___-
_____XXXXX_____-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+11] = build_font_data('''
_______________-
____XXXXXXX____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
__________X____-
____XXXXXXX____-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+12] = build_font_data('''
_______________-
_______________-
_______________-
_______X_______-
______XXX______-
_______X_______-
_______________-
_______________-
_______________-
_______X_______-
______XXX______-
_______X_______-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+13] = build_font_data('''
_______________-
_______________-
_______________-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_XXXXXXXXXXXXX_-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+14] = build_font_data('''
_______X_______-
_______X_______-
_______X_______-
_______________-
_______________-
_____XXXXX_____-
_____X___X_____-
XXX__X___X__XXX-
_____X___X_____-
_____XXXXX_____-
_______________-
_______________-
_______X_______-
_______X_______-
_______X_______-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+15] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______X_______-
_X_____X_____X_-
__X____X____X__-
___X___X___X___-
____X_____X____-
_______________-
_______________-
_______________-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+16] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_______________-
_________X_____-
________X______-
_______X_______-
______X________-
_____X_________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+17] = build_font_data('''
_______________-
_______X_______-
______XXX______-
______XXX______-
_____XX_XX_____-
_____XX_XX_____-
____XX___XX____-
____XX___XX____-
___XXXXXXXXX___-
___XXXXXXXXX___-
__XX_______XX__-
__XX_______XX__-
_XX_________XX_-
_XX_________XX_-
_XX_________XX_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+18] = build_font_data('''
_______________-
_______________-
__________XXXXX-
_____XX_XXXXXXX-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_______________-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_XXXXXX_XXXXXXX-
_____XX_XXXXXXX-
__________XXXXX-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+19] = build_font_data('''
_______________-
______XXX______-
_____XXXXX_____-
____X_XX_XX____-
____XX__XXX____-
____X____XX____-
___XXXXXXXXX___-
___XXX___XXX___-
__XXX_____XXX__-
__XX______XXX__-
__XX______XXXX_-
__XX_____XXXXX_-
_X__XXXXXXX__X_-
_X___XXXXXX___X-
__XXX_XXXX_XXX_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+20] = build_font_data('''
_______________-
________XX_____-
_______XXX_____-
______XXX______-
_______________-
__XXXX___XXXX__-
_XXXXXXXXXXXXX_-
_XXXXXXXXXXXXXX-
_XXXXXXXXXXXXXX-
_XXXXXXXXXXXXXX-
_XXXXXXXXXXXXXX-
__XXXXXXXXXXXX_-
__XXXXXXXXXXXX_-
___XXXXXXXXXX__-
____XXX__XXX___-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+21] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
_______________-
__________XX___-
__________XX___-
_______________-
_______________-
_______________-
_______________-
___XX_____XX___-
___XX_____XX___-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+22] = build_font_data('''
_______________-
_XXX_______XXX_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_XXX_______XXX_-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+23] = build_font_data('''
_______________-
_______________-
_______________-
_______________-
__________X____-
_________XXX___-
__________X____-
____X__________-
___XXX_________-
____X__________-
__________X____-
_________XXX___-
__________X____-
_______________-
_______________-
----------------
''')

font_data[KEYBOARD_CODEPAGE_3_START+24] = build_font_data('''
_______________-
___XXX___XXX___-
__X_________X__-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
_X___________X_-
__X_________X__-
___XXX___XXX___-
----------------
''')

def print_codepoint_font_image(codepoint):
	print('\t', end='')
	for b in font_compress(font_data_to_u8_array(font_data.get(codepoint, font_data[0]))):
		print(f"0x{b:02X}, ", end='')
	print(f"// U+{codepoint:X}")


# font compression-decompression validation. Make sure that the compression function actually works for all codepoints
validate_compression_algorithm = True
if validate_compression_algorithm:
	original_length = 0
	compressed_length = 0
	codepoints = []
	for i in range(codepage_0_size):
		codepoints.append(KEYBOARD_CODEPAGE_0_START+i)
	for i in range(len(codepage_1)):
		codepoints.append(KEYBOARD_CODEPAGE_1_START+i)
	for i in range(len(codepage_2)):
		codepoints.append(KEYBOARD_CODEPAGE_2_START+i)
	codepoint = KEYBOARD_CODEPAGE_3_START
	while font_data.get(codepoint) is not None:
		codepoints.append(codepoint)
		codepoint += 1
	payload = b''
	for i in codepoints:
		original = font_data_to_u8_array(font_data.get(i, font_data[0]))
		compressed = font_compress(original)
		decompressed = font_decompress(compressed)
		original_length += len(original)
		compressed_length += len(compressed)
		if original != decompressed:
			print('Compression error! Codepoint: '+ hex(i))
			print(' '.join([f'{b:02X}' for b in original]))
			print(' '.join([f'{b:02X}' for b in decompressed]))
			assert(False)


print("// The content below is the compressed font data. The font size is 15x15.")
print()

print("const uint8_t FONT_CODEPAGE_0[] = {")
for i in range(codepage_0_size):
	print_codepoint_font_image(KEYBOARD_CODEPAGE_0_START+i)
print("};")

print("const uint8_t FONT_CODEPAGE_1[] = {")
for i in range(len(codepage_1)):
	print_codepoint_font_image(KEYBOARD_CODEPAGE_1_START+i)
print("};")

print("const uint8_t FONT_CODEPAGE_2[] = {")
for i in range(len(codepage_2)):
	print_codepoint_font_image(KEYBOARD_CODEPAGE_2_START+i)
print("};")

print("const uint8_t FONT_CODEPAGE_3[] = {")
codepoint = KEYBOARD_CODEPAGE_3_START
while font_data.get(codepoint) is not None:
	print_codepoint_font_image(codepoint)
	codepoint += 1
print("};")
