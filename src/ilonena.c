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

#include "button.h"
#include "display.h"
#include "keyboard.h"
#include "tim2_task.h"

#include "ch32fun.h"
#include "rv003usb.h"
#include <stdio.h>
#include <string.h>

static uint16_t test_image[] = {
	0xFFFF, 0xFFFF, 0xC003, 0xC003,
	0xC003, 0xC003, 0xFFFF, 0xFFFF
};

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
	ILONENA_KEY_A,
	ILONENA_KEY_S,
	ILONENA_KEY_D,
	ILONENA_KEY_F,
	ILONENA_KEY_G,
	ILONENA_KEY_ALA,
	ILONENA_KEY_WEKA,
	ILONENA_KEY_PANA,
};

struct __attribute__((__packed__)) ilonena_input_mapping {
	uint64_t input;
	uint32_t codepoint;
};

uint64_t encode_input_buffer_as_u64(uint8_t input_buffer[12], size_t input_buffer_length) {
	uint64_t ret = 0;
	uint64_t big_shift = 32;
	uint64_t shift = 32-5;
	for(size_t i=0; i<input_buffer_length; i++) {
		ret |= (uint64_t)input_buffer[i] << (big_shift+shift);
		shift -= 5;
		if(shift < 0) {
			big_shift = 0;
			shift = 32-5;
		}
	}
	return ret;
}

const struct ilonena_input_mapping WAKALITO_LOOKUP_TABLE[] = {
	{.input = 0x800000000000000ULL, .codepoint=0xf1921UL}, // 1 -> la
	{.input = 0x840000000000000ULL, .codepoint=0xf193aUL}, // 11 -> mun
	{.input = 0x842100000000000ULL, .codepoint=0xf191dUL}, // 1111 -> kon
	{.input = 0x8c0000000000000ULL, .codepoint=0xf1920UL}, // 13 -> kute
	{.input = 0x8c6840000000000ULL, .codepoint=0xf1962UL}, // 133ww -> soweli
	{.input = 0x8c6842040000000ULL, .codepoint=0xf1962UL}, // 133wwww -> soweli
	{.input = 0xa04000000000000ULL, .codepoint=0xf1903UL}, // 1w2 -> alasa
	{.input = 0xa04a00000000000ULL, .codepoint=0xf1903UL}, // 1w2r -> alasa
	{.input = 0xa10318000000000ULL, .codepoint=0xf1962UL}, // 1ww33 -> soweli
	{.input = 0xa10840c18000000ULL, .codepoint=0xf1962UL}, // 1wwww33 -> soweli
	{.input = 0x1046342042000000ULL, .codepoint=0xf1962UL}, // 2133wwww -> soweli
	{.input = 0x1080000000000000ULL, .codepoint=0xf1956UL}, // 22 -> sama
	{.input = 0x1084818c00000000ULL, .codepoint=0xf1951UL}, // 222w33 -> pipi
	{.input = 0x1090000000000000ULL, .codepoint=0xf19a0UL}, // 22w -> pake
	{.input = 0x1090210000000000ULL, .codepoint=0xf1958UL}, // 22w22 -> selo
	{.input = 0x1090800000000000ULL, .codepoint=0xf193dUL}, // 22ww -> nanpa
	{.input = 0x1092000000000000ULL, .codepoint=0xf1922UL}, // 22e -> lape
	{.input = 0x1094000000000000ULL, .codepoint=0xf1916UL}, // 22r -> kama
	{.input = 0x10c0000000000000ULL, .codepoint=0xf192cUL}, // 23 -> lon
	{.input = 0x1200000000000000ULL, .codepoint=0xf1968UL}, // 2w -> taso
	{.input = 0x1204000000000000ULL, .codepoint=0xf1950UL}, // 2w2 -> pini
	{.input = 0x1204300000000000ULL, .codepoint=0xf195fUL}, // 2w23 -> sinpin
	{.input = 0x1204800000000000ULL, .codepoint=0xf1942UL}, // 2w2w -> nimi
	{.input = 0x1210000000000000ULL, .codepoint=0xf1965UL}, // 2ww -> supa
	{.input = 0x1210800000000000ULL, .codepoint=0xf195bUL}, // 2www -> sijelo
	{.input = 0x1210840000000000ULL, .codepoint=0xf1958UL}, // 2wwww -> selo
	{.input = 0x1240000000000000ULL, .codepoint=0xf1946UL}, // 2e -> ona
	{.input = 0x1244000000000000ULL, .codepoint=0xf1932UL}, // 2e2 -> mani
	{.input = 0x1284000000000000ULL, .codepoint=0xf1908UL}, // 2r2 -> awen
	{.input = 0x1284b00000000000ULL, .codepoint=0xf192bUL}, // 2r2t -> loje
	{.input = 0x1288000000000000ULL, .codepoint=0xf1972UL}, // 2r4 -> walo
	{.input = 0x1296200000000000ULL, .codepoint=0xf192bUL}, // 2rt2 -> loje
	{.input = 0x12c6000000000000ULL, .codepoint=0xf1970UL}, // 2t3 -> uta
	{.input = 0x12ce000000000000ULL, .codepoint=0xf1936UL}, // 2tq -> moku
	{.input = 0x12d4200000000000ULL, .codepoint=0xf192bUL}, // 2tr2 -> loje
	{.input = 0x1380000000000000ULL, .codepoint=0xf1947UL}, // 2s -> open
	{.input = 0x1840000000000000ULL, .codepoint=0xf1920UL}, // 31 -> kute
	{.input = 0x1880000000000000ULL, .codepoint=0xf192cUL}, // 32 -> lon
	{.input = 0x1890200000000000ULL, .codepoint=0xf1938UL}, // 32w2 -> monsi
	{.input = 0x18c4212400000000ULL, .codepoint=0xf1901UL}, // 33222e -> akesi
	{.input = 0x18d0210800000000ULL, .codepoint=0xf1951UL}, // 33w222 -> pipi
	{.input = 0x18d4800000000000ULL, .codepoint=0xf1974UL}, // 33rw -> waso
	{.input = 0x18dc318000000000ULL, .codepoint=0xf190fUL}, // 33s33 -> insa
	{.input = 0x1980000000000000ULL, .codepoint=0xf1938UL}, // 36 -> monsi
	{.input = 0x19c0000000000000ULL, .codepoint=0xf1959UL}, // 3q -> seme
	{.input = 0x1a06000000000000ULL, .codepoint=0xf197bUL}, // 3w3 -> kipisi
	{.input = 0x1a10000000000000ULL, .codepoint=0xf1982UL}, // 3ww -> meso
	{.input = 0x1a40000000000000ULL, .codepoint=0xf1931UL}, // 3e -> mama
	{.input = 0x1a94000000000000ULL, .codepoint=0xf1966UL}, // 3rr -> suwi
	{.input = 0x1b86000000000000ULL, .codepoint=0xf1905UL}, // 3s3 -> anpa
	{.input = 0x1bc0000000000000ULL, .codepoint=0xf1978UL}, // 3d -> namako
	{.input = 0x1c00000000000000ULL, .codepoint=0xf1957UL}, // 3f -> seli
	{.input = 0x2000000000000000ULL, .codepoint=0xf1928UL}, // 4 -> lili
	{.input = 0x2046000000000000ULL, .codepoint=0xf197aUL}, // 413 -> oko
	{.input = 0x2094000000000000ULL, .codepoint=0xf1926UL}, // 42r -> lete
	{.input = 0x2100000000000000ULL, .codepoint=0xf1963UL}, // 44 -> suli
	{.input = 0x2108000000000000ULL, .codepoint=0xf194bUL}, // 444 -> pan
	{.input = 0x2108400000000000ULL, .codepoint=0xf197dUL}, // 4444 -> monsuta
	{.input = 0x2114400000000000ULL, .codepoint=0xf1971UL}, // 44r4 -> utala
	{.input = 0x2140000000000000ULL, .codepoint=0xf1967UL}, // 45 -> tan
	{.input = 0x214a000000000000ULL, .codepoint=0xf194eUL}, // 455 -> pilin
	{.input = 0x214a429400000000ULL, .codepoint=0xf1945UL}, // 455455 -> olin
	{.input = 0x2200000000000000ULL, .codepoint=0xf1907UL}, // 4w -> anu
	{.input = 0x2210200000000000ULL, .codepoint=0xf191bUL}, // 4ww2 -> kiwen
	{.input = 0x2280000000000000ULL, .codepoint=0xf1902UL}, // 4r -> ala
	{.input = 0x2284000000000000ULL, .codepoint=0xf191bUL}, // 4r2 -> kiwen
	{.input = 0x2288000000000000ULL, .codepoint=0xf1971UL}, // 4r4 -> utala
	{.input = 0x2288a00000000000ULL, .codepoint=0xf191dUL}, // 4r4r -> kon
	{.input = 0x2294200000000000ULL, .codepoint=0xf194fUL}, // 4rr2 -> pimeja
	{.input = 0x2800000000000000ULL, .codepoint=0xf190dUL}, // 5 -> ike
	{.input = 0x289c000000000000ULL, .codepoint=0xf1981UL}, // 52s -> soko
	{.input = 0x2900000000000000ULL, .codepoint=0xf1967UL}, // 54 -> tan
	{.input = 0x2948000000000000ULL, .codepoint=0xf194eUL}, // 554 -> pilin
	{.input = 0x2948529000000000ULL, .codepoint=0xf1945UL}, // 554554 -> olin
	{.input = 0x2948900000000000ULL, .codepoint=0xf196fUL}, // 554e -> unpa
	{.input = 0x2956000000000000ULL, .codepoint=0xf194eUL}, // 55t -> pilin
	{.input = 0x2956800000000000ULL, .codepoint=0xf191aUL}, // 55tw -> kili
	{.input = 0x2956900000000000ULL, .codepoint=0xf196fUL}, // 55te -> unpa
	{.input = 0x2a10000000000000ULL, .codepoint=0xf1940UL}, // 5ww -> nena
	{.input = 0x2a10b00000000000ULL, .codepoint=0xf194aUL}, // 5wwt -> palisa
	{.input = 0x2a10b10000000000ULL, .codepoint=0xf1987UL}, // 5wwt2 -> misikeke
	{.input = 0x2a16800000000000ULL, .codepoint=0xf194aUL}, // 5wtw -> palisa
	{.input = 0x2a16810000000000ULL, .codepoint=0xf1987UL}, // 5wtw2 -> misikeke
	{.input = 0x2a40000000000000ULL, .codepoint=0xf1933UL}, // 5e -> meli
	{.input = 0x2a56000000000000ULL, .codepoint=0xf191cUL}, // 5et -> ko
	{.input = 0x2ac0000000000000ULL, .codepoint=0xf1929UL}, // 5t -> linja
	{.input = 0x2aca000000000000ULL, .codepoint=0xf193eUL}, // 5t5 -> nasa
	{.input = 0x2aca300000000000ULL, .codepoint=0xf1985UL}, // 5t53 -> lanpan
	{.input = 0x2aca800000000000ULL, .codepoint=0xf191aUL}, // 5t5w -> kili
	{.input = 0x2acab00000000000ULL, .codepoint=0xf196aUL}, // 5t5t -> telo
	{.input = 0x2acab40000000000ULL, .codepoint=0xf197fUL}, // 5t5tw -> jasima
	{.input = 0x3800000000000000ULL, .codepoint=0xf192dUL}, // q -> luka
	{.input = 0x3896000000000000ULL, .codepoint=0xf1936UL}, // q2t -> moku
	{.input = 0x38c0000000000000ULL, .codepoint=0xf1959UL}, // q3 -> seme
	{.input = 0x39ce000000000000ULL, .codepoint=0xf1910UL}, // qqq -> jaki
	{.input = 0x3a1e000000000000ULL, .codepoint=0xf1919UL}, // qwd -> kepeken
	{.input = 0x3a40000000000000ULL, .codepoint=0xf1949UL}, // qe -> pali
	{.input = 0x3ac4000000000000ULL, .codepoint=0xf1936UL}, // qt2 -> moku
	{.input = 0x3bd0000000000000ULL, .codepoint=0xf1919UL}, // qdw -> kepeken
	{.input = 0x3c00000000000000ULL, .codepoint=0xf194cUL}, // qf -> pana
	{.input = 0x4000000000000000ULL, .codepoint=0xf1973UL}, // w -> wan
	{.input = 0x4044000000000000ULL, .codepoint=0xf1903UL}, // w12 -> alasa
	{.input = 0x4044800000000000ULL, .codepoint=0xf1943UL}, // w12w -> noka
	{.input = 0x4044a00000000000ULL, .codepoint=0xf1903UL}, // w12r -> alasa
	{.input = 0x4050000000000000ULL, .codepoint=0xf1943UL}, // w1w -> noka
	{.input = 0x4080000000000000ULL, .codepoint=0xf194dUL}, // w2 -> pi
	{.input = 0x4084000000000000ULL, .codepoint=0xf19a0UL}, // w22 -> pake
	{.input = 0x4084218c00000000ULL, .codepoint=0xf1951UL}, // w22233 -> pipi
	{.input = 0x4090000000000000ULL, .codepoint=0xf190aUL}, // w2w -> en
	{.input = 0x4090200000000000ULL, .codepoint=0xf1942UL}, // w2w2 -> nimi
	{.input = 0x4090800000000000ULL, .codepoint=0xf195bUL}, // w2ww -> sijelo
	{.input = 0x4090840000000000ULL, .codepoint=0xf1958UL}, // w2www -> selo
	{.input = 0x40c0000000000000ULL, .codepoint=0xf1944UL}, // w3 -> o
	{.input = 0x40c6000000000000ULL, .codepoint=0xf1900UL}, // w33 -> a
	{.input = 0x40d0000000000000ULL, .codepoint=0xf1982UL}, // w3w -> meso
	{.input = 0x4100000000000000ULL, .codepoint=0xf1941UL}, // w4 -> ni
	{.input = 0x4110000000000000ULL, .codepoint=0xf1963UL}, // w4w -> suli
	{.input = 0x4114000000000000ULL, .codepoint=0xf1979UL}, // w4r -> kin
	{.input = 0x4114800000000000ULL, .codepoint=0xf1979UL}, // w4rw -> kin
	{.input = 0x4140000000000000ULL, .codepoint=0xf1986UL}, // w5 -> n
	{.input = 0x4150000000000000ULL, .codepoint=0xf1940UL}, // w5w -> nena
	{.input = 0x4150b00000000000ULL, .codepoint=0xf194aUL}, // w5wt -> palisa
	{.input = 0x4150b10000000000ULL, .codepoint=0xf1987UL}, // w5wt2 -> misikeke
	{.input = 0x4156558000000000ULL, .codepoint=0xf197fUL}, // w5t5t -> jasima
	{.input = 0x4200000000000000ULL, .codepoint=0xf196eUL}, // ww -> tu
	{.input = 0x4204000000000000ULL, .codepoint=0xf1965UL}, // ww2 -> supa
	{.input = 0x4204200000000000ULL, .codepoint=0xf193dUL}, // ww22 -> nanpa
	{.input = 0x4208000000000000ULL, .codepoint=0xf1918UL}, // ww4 -> ken
	{.input = 0x420a000000000000ULL, .codepoint=0xf1940UL}, // ww5 -> nena
	{.input = 0x4210000000000000ULL, .codepoint=0xf193cUL}, // www -> mute
	{.input = 0x4210f00000000000ULL, .codepoint=0xf1925UL}, // wwwd -> len
	{.input = 0x4214000000000000ULL, .codepoint=0xf1983UL}, // wwr -> epiku
	{.input = 0x4216000000000000ULL, .codepoint=0xf192fUL}, // wwt -> lupa
	{.input = 0x4240000000000000ULL, .codepoint=0xf1934UL}, // we -> mi
	{.input = 0x4246000000000000ULL, .codepoint=0xf1900UL}, // we3 -> a
	{.input = 0x4250000000000000ULL, .codepoint=0xf1975UL}, // wew -> wawa
	{.input = 0x4252000000000000ULL, .codepoint=0xf1917UL}, // wee -> kasi
	{.input = 0x4252250000000000ULL, .codepoint=0xf1923UL}, // wee2r -> laso
	{.input = 0x4252a10000000000ULL, .codepoint=0xf1923UL}, // weer2 -> laso
	{.input = 0x4280000000000000ULL, .codepoint=0xf193fUL}, // wr -> nasin
	{.input = 0x4288000000000000ULL, .codepoint=0xf19a3UL}, // wr4 -> powe
	{.input = 0x42cab28000000000ULL, .codepoint=0xf197fUL}, // wt5t5 -> jasima
	{.input = 0x42d0000000000000ULL, .codepoint=0xf192fUL}, // wtw -> lupa
	{.input = 0x42d0500000000000ULL, .codepoint=0xf194aUL}, // wtw5 -> palisa
	{.input = 0x42d0510000000000ULL, .codepoint=0xf1987UL}, // wtw52 -> misikeke
	{.input = 0x42d6800000000000ULL, .codepoint=0xf195aUL}, // wttw -> sewi
	{.input = 0x43c0000000000000ULL, .codepoint=0xf190eUL}, // wd -> ilo
	{.input = 0x43ce000000000000ULL, .codepoint=0xf1919UL}, // wdq -> kepeken
	{.input = 0x4800000000000000ULL, .codepoint=0xf190cUL}, // e -> ijo
	{.input = 0x4840000000000000ULL, .codepoint=0xf1904UL}, // e1 -> ale
	{.input = 0x4880000000000000ULL, .codepoint=0xf1924UL}, // e2 -> lawa
	{.input = 0x4884218c00000000ULL, .codepoint=0xf1901UL}, // e22233 -> akesi
	{.input = 0x4884800000000000ULL, .codepoint=0xf197eUL}, // e22w -> tonsi
	{.input = 0x4890000000000000ULL, .codepoint=0xf1930UL}, // e2w -> ma
	{.input = 0x4890200000000000ULL, .codepoint=0xf197eUL}, // e2w2 -> tonsi
	{.input = 0x48c0000000000000ULL, .codepoint=0xf192eUL}, // e3 -> lukin
	{.input = 0x48d2900000000000ULL, .codepoint=0xf1939UL}, // e3ee -> mu
	{.input = 0x4914000000000000ULL, .codepoint=0xf1937UL}, // e4r -> moli
	{.input = 0x4914450000000000ULL, .codepoint=0xf1937UL}, // e4r4r -> moli
	{.input = 0x4940000000000000ULL, .codepoint=0xf1933UL}, // e5 -> meli
	{.input = 0x49c0000000000000ULL, .codepoint=0xf1949UL}, // eq -> pali
	{.input = 0x4a00000000000000ULL, .codepoint=0xf195eUL}, // ew -> sina
	{.input = 0x4a04000000000000ULL, .codepoint=0xf196bUL}, // ew2 -> tenpo
	{.input = 0x4a04200000000000ULL, .codepoint=0xf197eUL}, // ew22 -> tonsi
	{.input = 0x4a1082a842103180ULL, .codepoint=0xf1980UL}, // ewww5rwww33 -> kijetesantakalu
	{.input = 0x4a12000000000000ULL, .codepoint=0xf1917UL}, // ewe -> kasi
	{.input = 0x4a40000000000000ULL, .codepoint=0xf195cUL}, // ee -> sike
	{.input = 0x4a46900000000000ULL, .codepoint=0xf1939UL}, // ee3e -> mu
	{.input = 0x4a50000000000000ULL, .codepoint=0xf1917UL}, // eew -> kasi
	{.input = 0x4a50250000000000ULL, .codepoint=0xf1923UL}, // eew2r -> laso
	{.input = 0x4a50a10000000000ULL, .codepoint=0xf1923UL}, // eewr2 -> laso
	{.input = 0x4a52000000000000ULL, .codepoint=0xf191fUL}, // eee -> kulupu
	{.input = 0x4a52300000000000ULL, .codepoint=0xf1939UL}, // eee3 -> mu
	{.input = 0x4a52a00000000000ULL, .codepoint=0xf19a1UL}, // eeer -> apeja
	{.input = 0x4a54900000000000ULL, .codepoint=0xf19a1UL}, // eere -> apeja
	{.input = 0x4a80000000000000ULL, .codepoint=0xf1911UL}, // er -> jan
	{.input = 0x4a88a20000000000ULL, .codepoint=0xf1937UL}, // er4r4 -> moli
	{.input = 0x4a92900000000000ULL, .codepoint=0xf19a1UL}, // eree -> apeja
	{.input = 0x4ac0000000000000ULL, .codepoint=0xf1932UL}, // et -> mani
	{.input = 0x4ac4000000000000ULL, .codepoint=0xf1913UL}, // et2 -> jo
	{.input = 0x4ad2000000000000ULL, .codepoint=0xf193bUL}, // ete -> musi
	{.input = 0x4b80000000000000ULL, .codepoint=0xf1981UL}, // es -> soko
	{.input = 0x4bc0000000000000ULL, .codepoint=0xf1964UL}, // ed -> suno
	{.input = 0x4bd4200000000000ULL, .codepoint=0xf1912UL}, // edr2 -> jelo
	{.input = 0x4c00000000000000ULL, .codepoint=0xf196cUL}, // ef -> toki
	{.input = 0x4c04000000000000ULL, .codepoint=0xf1984UL}, // ef2 -> kokosila
	{.input = 0x5000000000000000ULL, .codepoint=0xf1927UL}, // r -> li
	{.input = 0x5080000000000000ULL, .codepoint=0xf191eUL}, // r2 -> kule
	{.input = 0x5084000000000000ULL, .codepoint=0xf1969UL}, // r22 -> tawa
	{.input = 0x5084200000000000ULL, .codepoint=0xf191eUL}, // r222 -> kule
	{.input = 0x5084b00000000000ULL, .codepoint=0xf192bUL}, // r22t -> loje
	{.input = 0x5088000000000000ULL, .codepoint=0xf1972UL}, // r24 -> walo
	{.input = 0x5088a00000000000ULL, .codepoint=0xf194fUL}, // r24r -> pimeja
	{.input = 0x5090948000000000ULL, .codepoint=0xf1923UL}, // r2wee -> laso
	{.input = 0x5092848000000000ULL, .codepoint=0xf1923UL}, // r2ewe -> laso
	{.input = 0x5096200000000000ULL, .codepoint=0xf192bUL}, // r2t2 -> loje
	{.input = 0x50d4000000000000ULL, .codepoint=0xf1966UL}, // r3r -> suwi
	{.input = 0x5100000000000000ULL, .codepoint=0xf1906UL}, // r4 -> ante
	{.input = 0x5104000000000000ULL, .codepoint=0xf1926UL}, // r42 -> lete
	{.input = 0x5114400000000000ULL, .codepoint=0xf191dUL}, // r4r4 -> kon
	{.input = 0x5200000000000000ULL, .codepoint=0xf193fUL}, // rw -> nasin
	{.input = 0x5206300000000000ULL, .codepoint=0xf1974UL}, // rw33 -> waso
	{.input = 0x5210000000000000ULL, .codepoint=0xf1983UL}, // rww -> epiku
	{.input = 0x5240000000000000ULL, .codepoint=0xf1914UL}, // re -> kala
	{.input = 0x5252900000000000ULL, .codepoint=0xf19a1UL}, // reee -> apeja
	{.input = 0x5254000000000000ULL, .codepoint=0xf1935UL}, // rer -> mije
	{.input = 0x5280000000000000ULL, .codepoint=0xf1909UL}, // rr -> e
	{.input = 0x5286000000000000ULL, .codepoint=0xf1966UL}, // rr3 -> suwi
	{.input = 0x5294a00000000000ULL, .codepoint=0xf197dUL}, // rrrr -> monsuta
	{.input = 0x5380000000000000ULL, .codepoint=0xf196dUL}, // rs -> tomo
	{.input = 0x5800000000000000ULL, .codepoint=0xf1954UL}, // t -> pona
	{.input = 0x5886000000000000ULL, .codepoint=0xf1970UL}, // t23 -> uta
	{.input = 0x588e000000000000ULL, .codepoint=0xf1936UL}, // t2q -> moku
	{.input = 0x5894200000000000ULL, .codepoint=0xf192bUL}, // t2r2 -> loje
	{.input = 0x58a0000000000000ULL, .codepoint=0xf1915UL}, // t2f -> kalama
	{.input = 0x5940000000000000ULL, .codepoint=0xf1929UL}, // t5 -> linja
	{.input = 0x5956000000000000ULL, .codepoint=0xf193eUL}, // t5t -> nasa
	{.input = 0x5956500000000000ULL, .codepoint=0xf196aUL}, // t5t5 -> telo
	{.input = 0x5956540000000000ULL, .codepoint=0xf197fUL}, // t5t5w -> jasima
	{.input = 0x5a0a000000000000ULL, .codepoint=0xf190bUL}, // tw5 -> esun
	{.input = 0x5a0a800000000000ULL, .codepoint=0xf194aUL}, // tw5w -> palisa
	{.input = 0x5a0a810000000000ULL, .codepoint=0xf1987UL}, // tw5w2 -> misikeke
	{.input = 0x5a10000000000000ULL, .codepoint=0xf192fUL}, // tww -> lupa
	{.input = 0x5a10500000000000ULL, .codepoint=0xf194aUL}, // tww5 -> palisa
	{.input = 0x5a10510000000000ULL, .codepoint=0xf1987UL}, // tww52 -> misikeke
	{.input = 0x5a4a000000000000ULL, .codepoint=0xf191cUL}, // te5 -> ko
	{.input = 0x5ac0000000000000ULL, .codepoint=0xf1977UL}, // tt -> wile
	{.input = 0x5ad0000000000000ULL, .codepoint=0xf195aUL}, // ttw -> sewi
	{.input = 0x60c0000000000000ULL, .codepoint=0xf195fUL}, // y3 -> sinpin
	{.input = 0x7000000000000000ULL, .codepoint=0xf1953UL}, // s -> poki
	{.input = 0x7080000000000000ULL, .codepoint=0xf1947UL}, // s2 -> open
	{.input = 0x70c0000000000000ULL, .codepoint=0xf1952UL}, // s3 -> poka
	{.input = 0x70c6000000000000ULL, .codepoint=0xf190fUL}, // s33 -> insa
	{.input = 0x7280000000000000ULL, .codepoint=0xf196dUL}, // sr -> tomo
	{.input = 0x7800000000000000ULL, .codepoint=0xf192aUL}, // d -> lipu
	{.input = 0x78c0000000000000ULL, .codepoint=0xf1978UL}, // d3 -> namako
	{.input = 0x78c6000000000000ULL, .codepoint=0xf1960UL}, // d33 -> sitelen
	{.input = 0x78c6300000000000ULL, .codepoint=0xf1960UL}, // d333 -> sitelen
	{.input = 0x7904300000000000ULL, .codepoint=0xf1988UL}, // d423 -> ku
	{.input = 0x7904982c00000000ULL, .codepoint=0xf1988UL}, // d42eft -> ku
	{.input = 0x7914000000000000ULL, .codepoint=0xf1948UL}, // d4r -> pakala
	{.input = 0x79d0000000000000ULL, .codepoint=0xf1919UL}, // dqw -> kepeken
	{.input = 0x7a00000000000000ULL, .codepoint=0xf190eUL}, // dw -> ilo
	{.input = 0x7a0e000000000000ULL, .codepoint=0xf1919UL}, // dwq -> kepeken
	{.input = 0x7a10800000000000ULL, .codepoint=0xf1925UL}, // dwww -> len
	{.input = 0x7a60b00000000000ULL, .codepoint=0xf1955UL}, // deft -> pu
	{.input = 0x7a88000000000000ULL, .codepoint=0xf1948UL}, // dr4 -> pakala
	{.input = 0x7bc0000000000000ULL, .codepoint=0xf197cUL}, // dd -> leko
	{.input = 0x7c00000000000000ULL, .codepoint=0xf1961UL}, // df -> sona
	{.input = 0x8000000000000000ULL, .codepoint=0xf195dUL}, // f -> sin
	{.input = 0x80c0000000000000ULL, .codepoint=0xf1957UL}, // f3 -> seli
	{.input = 0x81c0000000000000ULL, .codepoint=0xf194cUL}, // fq -> pana
	{.input = 0x8200000000000000ULL, .codepoint=0xf1976UL}, // fw -> weka
	{.input = 0x8244000000000000ULL, .codepoint=0xf1984UL}, // fe2 -> kokosila
	{.input = 0x83c0000000000000ULL, .codepoint=0xf1961UL}, // fd -> sona
};

int wakalito_search_lookup_table(uint8_t input_buffer[12], size_t input_buffer_length) {
	uint64_t target = encode_input_buffer_as_u64(input_buffer, input_buffer_length);
	for(size_t i=0; i<sizeof(WAKALITO_LOOKUP_TABLE)/sizeof(*WAKALITO_LOOKUP_TABLE); i++) {
		if(target == WAKALITO_LOOKUP_TABLE[i].input) {
			return i;
		}
	}
	return -1;
}

int main() {
	SystemInit();

	// Enable interrupt nesting for rv003usb software USB library
	__set_INTSYSCR( __get_INTSYSCR() | 0x02 );

	keyboard_init();
	button_init();
	display_init();
	tim2_task_init(); // Runs button_loop() and display_loop() with TIM2 interrupt

	uint32_t last_update_tick = SysTick->CNT;
	enum keyboard_output_mode mode = KEYBOARD_OUTPUT_MODE_LATIN;
	const uint32_t codepoints[20] = {
		'A', ',', '!', ' ', 'a', '\b',
		0x1F595, 0x414, 0xFC, 0x5C4C, 0x20AC, 0x2192,
		0xF1900U, 0xF1901U, 0xF1902U, 0xF1903U, 0xF1900U+19, 0xF1900U+20,
		0xF1906U, '\n'};

	uint8_t input_buffer[12] = {0};
	#define INPUT_BUFFER_SIZE (sizeof(input_buffer)/sizeof(*input_buffer))
	size_t input_buffer_index = 0;
	int32_t y_pos = 0;

	while(1) {
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				enum ilonena_key_id key_id = i+1;
				switch(key_id) {
					case ILONENA_KEY_ALA:
						if(input_buffer_index == 0) {
							keyboard_write_codepoint(mode, ' ');
						} else {
							// Lookup the table, then send out the key according to the buffer
							int index = wakalito_search_lookup_table(input_buffer, input_buffer_index);
							if(index > 0) {
								keyboard_write_codepoint(mode, WAKALITO_LOOKUP_TABLE[index].codepoint);
								memset(input_buffer, 0, sizeof(input_buffer));
								input_buffer_index = 0;
							}
						}
					break;
					case ILONENA_KEY_PANA:
						// Stealing this button for mode switching for now in before the config page is ready
						// In the future, this button is equivalent to pressing space then press enter
						if(++mode >= KEYBOARD_OUTPUT_MODE_END) {
							mode = 0;
						}
					break;
					case ILONENA_KEY_WEKA:
						if(input_buffer_index == 0) {
							keyboard_write_codepoint(mode, '\b');
						} else {
							input_buffer_index--;
							y_pos = wakalito_search_lookup_table(input_buffer, input_buffer_index)/8;
						}
					break;
					default:
						if(input_buffer_index < INPUT_BUFFER_SIZE) {
							input_buffer[input_buffer_index++] = key_id;
							y_pos = wakalito_search_lookup_table(input_buffer, input_buffer_index)/8;
						} else {
							// Bufferoverflow. Let's do nothing!
						}
					break;
				}
			}
		}

		// Update graphic every 100ms. TODO: remove. It's just a piece of code for testing the display
		if(SysTick->CNT - last_update_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) {
			last_update_tick = SysTick->CNT;
			display_clear();
			display_draw_16(test_image, sizeof(test_image)/sizeof(*test_image), input_buffer_index*8, y_pos, mode);
			display_set_refresh_flag();
		}
	}
}
