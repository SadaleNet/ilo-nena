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
	while(1) {
		uint32_t button_press_event = button_get_pressed_event();
		for(size_t i=0; i<20; i++) {
			if(button_press_event & (1U << i)) {
				if(i == 18) {
					if(++mode >= KEYBOARD_OUTPUT_MODE_END) {
						mode = 0;
					}
				} else {
					keyboard_write_character(mode, i);
				}
			}
		}

		// Update graphic every 100ms. TODO: remove. It's just a piece of code for testing the display
		if(SysTick->CNT - last_update_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) {
			static int32_t index = 0;
			last_update_tick = SysTick->CNT;
			display_clear();
			display_draw_16(test_image, sizeof(test_image)/sizeof(*test_image), index%(128+8)-8, index%48-16, mode);
			display_set_refresh_flag();
			index++;
		}
	}
}
