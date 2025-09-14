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

#include "ch32fun.h"
#include "rv003usb.h"
#include <stdio.h>
#include <string.h>

volatile uint8_t key_to_be_sent = HID_KEY_NONE;

void usb_handle_user_in_request(struct usb_endpoint *e, uint8_t *scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal *ist) {
	if(endp == 0) {
		// Always make empty response for control transfer
		usb_send_empty( sendtok );
	} else if(endp == 1) {
		// Keyboard end point
		// Send the previous payload. We want to send out the response as soon as possible and cannot afford to wait for building the payload
		static uint8_t usb_response[8] = { 0x00 }; // Format: modifiers_keys (1 byte), reserved (1 byte), key_scancodes (6 bytes)
		usb_send_data( usb_response, 8, 0, sendtok );

		// Build the next response by sending out the key
		usb_response[2] = key_to_be_sent;
	}
}

static uint16_t test_image[] = {
	0xFFFF, 0xFFFF, 0xC003, 0xC003,
	0xC003, 0xC003, 0xFFFF, 0xFFFF
};

int main() {
	SystemInit();

	// Enable interrupt nesting for rv003usb software USB library
	__set_INTSYSCR( __get_INTSYSCR() | 0x02 );

	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5µs ( TDDIS )
	usb_setup();

	button_init();
	display_init();

	uint32_t last_update_tick = SysTick->CNT;
	while(1) {
		uint8_t found = 0;
		uint32_t button_state = button_get_state();
		for(size_t i=0; i<20; i++) {
			if(button_state & (1U << i)) {
				key_to_be_sent = HID_KEY_A + i;
				found = 1;
				break;
			}
		}
		if(!found) {
			key_to_be_sent = HID_KEY_NONE;
		}

		// Update graphic every 100ms. TODO: remove. It's just a piece of code for testing the display
		if(SysTick->CNT - last_update_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) {
			static int32_t index = 0;
			last_update_tick = SysTick->CNT;
			display_clear();
			display_draw_16(test_image, sizeof(test_image)/sizeof(*test_image), index%(128+8)-8, index%48-16, index%4 | index%2);
			display_set_refresh_flag();
			index++;
		}

		display_loop();

		Delay_Ms(1);
	}
}
