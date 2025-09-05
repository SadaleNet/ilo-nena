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
#include <stdlib.h>

// Column output config
#define BUTTON_COLUMN_GPIO_PORT (GPIOD)
// output, push-pull, 2Mhz
#define BUTTON_COLUMN_CFGLR_FLAG (GPIO_CFGLR_MODE0_1|GPIO_CFGLR_MODE2_1|GPIO_CFGLR_MODE3_1|GPIO_CFGLR_MODE4_1|GPIO_CFGLR_MODE5_1|GPIO_CFGLR_MODE6_1)
#define BUTTON_COLUMN_CFGLR_MASK ((GPIO_CFGLR_MODE0|GPIO_CFGLR_MODE2|GPIO_CFGLR_MODE3|GPIO_CFGLR_MODE4|GPIO_CFGLR_MODE5|GPIO_CFGLR_MODE6) | (GPIO_CFGLR_CNF0|GPIO_CFGLR_CNF2|GPIO_CFGLR_CNF3|GPIO_CFGLR_CNF4|GPIO_CFGLR_CNF5|GPIO_CFGLR_CNF6))
// column mapping: output LOW to the selected column and HIGH for other columns
#define BUTTON_COLUMN_BSHR_BS (GPIO_BSHR_BS0|GPIO_BSHR_BS2|GPIO_BSHR_BS3|GPIO_BSHR_BS4|GPIO_BSHR_BS5|GPIO_BSHR_BS6)
const static uint32_t BUTTON_COLUMN_BSHR_MASK_MAP[] = {
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS0)|GPIO_BSHR_BR0,
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS2)|GPIO_BSHR_BR2,
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS3)|GPIO_BSHR_BR3,
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS4)|GPIO_BSHR_BR4,
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS5)|GPIO_BSHR_BR5,
	(BUTTON_COLUMN_BSHR_BS&~GPIO_BSHR_BS6)|GPIO_BSHR_BR6,
	};
#define BUTTON_COLUMN_COUNT (sizeof(BUTTON_COLUMN_BSHR_MASK_MAP)/sizeof(*BUTTON_COLUMN_BSHR_MASK_MAP))

// Row input config
#define BUTTON_ROW_GPIO_PORT (GPIOC)
// input, pull-up/pull-down mode
#define BUTTON_ROW_CFGLR_FLAG (GPIO_CFGLR_CNF5_1|GPIO_CFGLR_CNF6_1|GPIO_CFGLR_CNF7_1)
#define BUTTON_ROW_CFGLR_MASK ((GPIO_CFGLR_MODE5|GPIO_CFGLR_MODE6|GPIO_CFGLR_MODE7) | (GPIO_CFGLR_CNF5|GPIO_CFGLR_CNF6|GPIO_CFGLR_CNF7))
// pull up
#define BUTTON_ROW_BSHR_FLAG (GPIO_BSHR_BS5|GPIO_BSHR_BS6|GPIO_BSHR_BS7)
// row mapping
const static uint32_t BUTTON_ROW_INDR_MASK_MAP[] = {GPIO_INDR_IDR7, GPIO_INDR_IDR6, GPIO_INDR_IDR5};
#define BUTTON_ROW_COUNT (sizeof(BUTTON_ROW_INDR_MASK_MAP)/sizeof(*BUTTON_ROW_INDR_MASK_MAP))

size_t button_scan_column = 0;
uint8_t button_state[BUTTON_COLUMN_COUNT*BUTTON_ROW_COUNT] = {0};

void button_init(void) {
	// Enable clock for GPIOC and GPIOD
	RCC->APB2PCENR |= (RCC_IOPCEN | RCC_IOPDEN);

	// Configure column as output, push-pull, 2Mhz
	BUTTON_COLUMN_GPIO_PORT->CFGLR = (BUTTON_COLUMN_GPIO_PORT->CFGLR & ~BUTTON_COLUMN_CFGLR_MASK) | (BUTTON_COLUMN_CFGLR_FLAG);

	// Configure row as input, pull-up
	BUTTON_ROW_GPIO_PORT->CFGLR = (BUTTON_ROW_GPIO_PORT->CFGLR & ~BUTTON_ROW_CFGLR_MASK) | (BUTTON_ROW_CFGLR_FLAG);
	BUTTON_ROW_GPIO_PORT->BSHR = BUTTON_ROW_BSHR_FLAG;

	// Initialize the state variables
	button_scan_column = 0;
	memset(button_state, 0, sizeof(button_state));
}

void button_loop(void) {
	// read from the rows
	uint32_t row_reading = BUTTON_ROW_GPIO_PORT->INDR;
	for(size_t i=0; i<BUTTON_ROW_COUNT; i++) {
		button_state[BUTTON_COLUMN_COUNT*i+button_scan_column] = !(row_reading & BUTTON_ROW_INDR_MASK_MAP[i]);
	}

	// write to the columns
	if(++button_scan_column >= BUTTON_COLUMN_COUNT) {
		button_scan_column = 0;
	}
	BUTTON_COLUMN_GPIO_PORT->BSHR = BUTTON_COLUMN_BSHR_MASK_MAP[button_scan_column];
}
