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

#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include <stdlib.h>

#define BUTTON_COLUMN_PORT GPIO_port_D
const static uint8_t BUTTON_COLUMN_MAP[] = {0, 2, 3, 4, 5, 6};
#define BUTTON_ROW_PORT GPIO_port_C
const static uint8_t BUTTON_ROW_MAP[] = {5, 6, 7};
size_t button_scan_column = 0;
uint8_t button_state[(sizeof(BUTTON_COLUMN_MAP)/sizeof(*BUTTON_COLUMN_MAP)) * (sizeof(BUTTON_ROW_MAP)/sizeof(*BUTTON_ROW_MAP))] = {0};

void button_init(void) {
	GPIO_port_enable(BUTTON_COLUMN_PORT);
	GPIO_port_enable(BUTTON_ROW_PORT);

	// Output column
	for(size_t i=0; i<sizeof(BUTTON_COLUMN_MAP)/sizeof(*BUTTON_COLUMN_MAP); i++) {
		GPIO_pinMode(GPIOv_from_PORT_PIN(BUTTON_COLUMN_PORT, BUTTON_COLUMN_MAP[i]), GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
	}

	// Input row
	for(size_t i=0; i<sizeof(BUTTON_ROW_MAP)/sizeof(*BUTTON_ROW_MAP); i++) {
		GPIO_pinMode(GPIOv_from_PORT_PIN(BUTTON_ROW_PORT, BUTTON_ROW_MAP[i]), GPIO_pinMode_I_pullUp, GPIO_Speed_In);
	}

	button_scan_column = 0;
	memset(button_state, 0, sizeof(button_state));
}

void button_loop(void) {
	for(size_t i=0; i<(sizeof(BUTTON_ROW_MAP)/sizeof(*BUTTON_ROW_MAP)); i++) {
		button_state[button_scan_column*(sizeof(BUTTON_ROW_MAP)/sizeof(*BUTTON_ROW_MAP))+i] = !GPIO_digitalRead(GPIOv_from_PORT_PIN(BUTTON_ROW_PORT, BUTTON_ROW_MAP[i]));
	}

	if(++button_scan_column >= sizeof(BUTTON_COLUMN_MAP)/sizeof(*BUTTON_COLUMN_MAP)) {
		button_scan_column = 0;
	}
	for(size_t i=0; i<sizeof(BUTTON_COLUMN_MAP)/sizeof(*BUTTON_COLUMN_MAP); i++) {
		GPIO_digitalWrite(GPIOv_from_PORT_PIN(BUTTON_COLUMN_PORT, BUTTON_COLUMN_MAP[i]), high);
	}
	GPIO_digitalWrite(GPIOv_from_PORT_PIN(BUTTON_COLUMN_PORT, BUTTON_COLUMN_MAP[button_scan_column]), low);
}
