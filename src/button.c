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

#include "tim2_task.h"
#include "ch32fun.h"

// Number of debounces required for the button state recorded as pressed/released
// The debounce state is initialized as zero, incremented if a press is detected, decremented else.
// The state is capped at [-BUTTON_DEBOUNCE_THRESHOLD, BUTTON_DEBOUNCE_THRESHOLD]
// Once it hits either BUTTON_DEBOUNCE_THRESHOLD or negative BUTTON_DEBOUNCE_THRESHOLD, it'd be recorded.
#define BUTTON_DEBOUNCE_THRESHOLD (4)

// Number of waiting required to register a button held event.
#define BUTTON_HELD_THRESHOLD (1000) // 1 second

// Column input config
#define BUTTON_COLUMN_GPIO_PORT (GPIOD)
// input, pull-up/pull-down mode
#define BUTTON_COLUMN_CFGLR_FLAG ((GPIO_CFGLR_CNF0_1|GPIO_CFGLR_CNF2_1|GPIO_CFGLR_CNF3_1|GPIO_CFGLR_CNF4_1|GPIO_CFGLR_CNF5_1|GPIO_CFGLR_CNF6_1))
#define BUTTON_COLUMN_CFGLR_MASK ((GPIO_CFGLR_MODE0|GPIO_CFGLR_MODE2|GPIO_CFGLR_MODE3|GPIO_CFGLR_MODE4|GPIO_CFGLR_MODE5|GPIO_CFGLR_MODE6) | (GPIO_CFGLR_CNF0|GPIO_CFGLR_CNF2|GPIO_CFGLR_CNF3|GPIO_CFGLR_CNF4|GPIO_CFGLR_CNF5|GPIO_CFGLR_CNF6))
// pull up
#define BUTTON_COLUMN_BSHR_FLAG (GPIO_BSHR_BS0|GPIO_BSHR_BS2|GPIO_BSHR_BS3|GPIO_BSHR_BS4|GPIO_BSHR_BS5|GPIO_BSHR_BS6)
// column mapping
const static uint32_t BUTTON_COLUMN_INDR_MASK_MAP[] = {GPIO_INDR_IDR0, GPIO_INDR_IDR2, GPIO_INDR_IDR3, GPIO_INDR_IDR4, GPIO_INDR_IDR5, GPIO_INDR_IDR6};
#define BUTTON_COLUMN_COUNT (sizeof(BUTTON_COLUMN_INDR_MASK_MAP)/sizeof(*BUTTON_COLUMN_INDR_MASK_MAP))

// Row output config
#define BUTTON_ROW_GPIO_PORT (GPIOC)
// output, open-drain, 2Mhz
#define BUTTON_ROW_CFGLR_FLAG ((GPIO_CFGLR_MODE5_1|GPIO_CFGLR_MODE6_1|GPIO_CFGLR_MODE7_1) | (GPIO_CFGLR_CNF5_0|GPIO_CFGLR_CNF6_0|GPIO_CFGLR_CNF7_0))
#define BUTTON_ROW_CFGLR_MASK ((GPIO_CFGLR_MODE5|GPIO_CFGLR_MODE6|GPIO_CFGLR_MODE7) | (GPIO_CFGLR_CNF5|GPIO_CFGLR_CNF6|GPIO_CFGLR_CNF7))
// row mapping: output LOW to the selected row and HIGH for other rows
#define BUTTON_ROW_BSHR_BS (GPIO_BSHR_BS5|GPIO_BSHR_BS6|GPIO_BSHR_BS7)
const static uint32_t BUTTON_ROW_BSHR_MASK_MAP[] = {
	(BUTTON_ROW_BSHR_BS&~GPIO_BSHR_BS7)|GPIO_BSHR_BR7,
	(BUTTON_ROW_BSHR_BS&~GPIO_BSHR_BS6)|GPIO_BSHR_BR6,
	(BUTTON_ROW_BSHR_BS&~GPIO_BSHR_BS5)|GPIO_BSHR_BR5,
};
#define BUTTON_ROW_COUNT (sizeof(BUTTON_ROW_BSHR_MASK_MAP)/sizeof(*BUTTON_ROW_BSHR_MASK_MAP))

// Dedicated button config (the buttons that's dedicated GPIO pin)
#define BUTTON_DEDICATED_GPIO_PORT (GPIOA)
// input, pull-up/pull-down mode
#define BUTTON_DEDICATED_CFGLR_FLAG (GPIO_CFGLR_CNF1_1|GPIO_CFGLR_CNF2_1)
#define BUTTON_DEDICATED_CFGLR_MASK ((GPIO_CFGLR_MODE1|GPIO_CFGLR_MODE2) | (GPIO_CFGLR_CNF1|GPIO_CFGLR_CNF2))
// pull up
#define BUTTON_DEDICATED_BSHR_FLAG (GPIO_BSHR_BS1|GPIO_BSHR_BS2)
const static uint32_t BUTTON_DEDICATED_INDR_MASK_MAP[] = {GPIO_INDR_IDR1, GPIO_INDR_IDR2};
#define BUTTON_DEDICATED_COUNT (sizeof(BUTTON_DEDICATED_INDR_MASK_MAP)/sizeof(*BUTTON_DEDICATED_INDR_MASK_MAP))

uint32_t button_state = 0; // CONCURRENCY_VARIABLE: written/read by button_loop() via TIM2 ISR, read by button_get_state()
uint32_t button_press_event = 0; // CONCURRENCY_VARIABLE: written/read by button_loop() via TIM2 ISR, read by button_get_pressed_event()
uint32_t button_held_event = 0; // CONCURRENCY_VARIABLE: written/read by button_loop() via TIM2 ISR, read by button_get_held_event()

void button_init(void) {
	// Initialize the state variable(s)
	button_state = 0;
	button_press_event = 0;

	// Enable clock for GPIOA, GPIOC and GPIOD
	RCC->APB2PCENR |= (RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN);

	// Configure row as output, open-drain, 2Mhz
	BUTTON_ROW_GPIO_PORT->CFGLR = (BUTTON_ROW_GPIO_PORT->CFGLR & ~BUTTON_ROW_CFGLR_MASK) | BUTTON_ROW_CFGLR_FLAG;
	// Write to the row for the first scan
	BUTTON_ROW_GPIO_PORT->BSHR = BUTTON_ROW_BSHR_MASK_MAP[button_state];

	// Configure column as input, pull-up
	BUTTON_COLUMN_GPIO_PORT->CFGLR = (BUTTON_COLUMN_GPIO_PORT->CFGLR & ~BUTTON_COLUMN_CFGLR_MASK) | BUTTON_COLUMN_CFGLR_FLAG;
	BUTTON_COLUMN_GPIO_PORT->BSHR = BUTTON_COLUMN_BSHR_FLAG;

	// Configure dedicated button as input, pull-up
	BUTTON_DEDICATED_GPIO_PORT->CFGLR = (BUTTON_DEDICATED_GPIO_PORT->CFGLR & ~BUTTON_DEDICATED_CFGLR_MASK) | BUTTON_DEDICATED_CFGLR_FLAG;
	BUTTON_DEDICATED_GPIO_PORT->BSHR = BUTTON_DEDICATED_BSHR_FLAG;
}

static uint32_t button_handle_debounce(uint8_t reading, uint32_t state, int8_t debounce[], size_t index) {
	// Increases debounce count if button is pressed, decrease else
	// The button press/release is only recorded if a threshold is reached
	if(reading) {
		if(++debounce[index] >= BUTTON_DEBOUNCE_THRESHOLD) {
			debounce[index] = BUTTON_DEBOUNCE_THRESHOLD;
			state = (state & ~(1U << index)) | (1 << index);
		}
	} else {
		if(--debounce[index] <= -BUTTON_DEBOUNCE_THRESHOLD) {
			debounce[index] = -BUTTON_DEBOUNCE_THRESHOLD;
			state = (state & ~(1U << index));
		}
	}
	return state;
}

void button_loop(void) {
	static size_t button_scan_row = 0;
	// positive is pressed count, negative is released count
	static int8_t button_debounce[BUTTON_ROW_COUNT*BUTTON_COLUMN_COUNT+BUTTON_DEDICATED_COUNT] = {0};

	uint32_t button_state_prev = button_state;
	// read from the columns
	uint32_t col_reading = BUTTON_COLUMN_GPIO_PORT->INDR;
	for(size_t i=0; i<BUTTON_COLUMN_COUNT; i++) {
		button_state = button_handle_debounce(
			!(col_reading & BUTTON_COLUMN_INDR_MASK_MAP[i]), button_state,
			button_debounce, BUTTON_COLUMN_COUNT*button_scan_row+i
		);
	}

	// Resets row index when it overflows
	if(++button_scan_row >= BUTTON_ROW_COUNT) {
		button_scan_row = 0;

		// Also read the dedicated button state
		uint32_t dedicated_reading = BUTTON_DEDICATED_GPIO_PORT->INDR;
		for(size_t i=0; i<BUTTON_DEDICATED_COUNT; i++) {
			button_state = button_handle_debounce(
				!(dedicated_reading & BUTTON_DEDICATED_INDR_MASK_MAP[i]), button_state,
				button_debounce, BUTTON_ROW_COUNT*BUTTON_COLUMN_COUNT+i
			);
		}
	}
	// write to the rows
	BUTTON_ROW_GPIO_PORT->BSHR = BUTTON_ROW_BSHR_MASK_MAP[button_scan_row];

	// Handle button held event
	static uint32_t button_held[BUTTON_ROW_COUNT*BUTTON_COLUMN_COUNT+BUTTON_DEDICATED_COUNT] = {0};
	for(size_t i=0; i<BUTTON_ROW_COUNT*BUTTON_COLUMN_COUNT+BUTTON_DEDICATED_COUNT; i++) {
		if(button_state & (1<<i)) {
			if(++button_held[i] == BUTTON_HELD_THRESHOLD){
				button_held_event |= (1<<i);
			}
		} else {
			button_held[i] = 0;
		}
	}

	// Perform compiler-level "refresh" of button_press_event because it might be modified by button_get_pressed_event()
	asm volatile ("" ::: "memory");
	// Truth table for just pressed event:
	// A B -> C
	// 0 0 -> 0
	// 0 1 -> 1
	// 1 0 -> 0
	// 1 1 -> 0
	// With the truth table above, we OR that into the button_press_event variable.
	button_press_event |= (button_state ^ button_state_prev) & button_state;
}

uint32_t button_get_state(void) {
	asm volatile ("" ::: "memory");
	return button_state;
}

uint32_t button_get_pressed_event(void) {
	tim2_task_pause();
	asm volatile ("" ::: "memory");
	uint32_t ret = button_press_event;
	button_press_event = 0;
	tim2_task_resume();
	return ret;
}

uint32_t button_get_held_event(void) {
	tim2_task_pause();
	asm volatile ("" ::: "memory");
	uint32_t ret = button_held_event;
	button_held_event = 0;
	tim2_task_resume();
	return ret;
}
