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
#include "rv003usb.h"
#include "ch32v003_GPIO_branchless.h"
#include <stdio.h>
#include <string.h>

void usb_handle_user_in_request(struct usb_endpoint *e, uint8_t *scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal *ist) {
	if(endp == 0) {
		// Always make empty response for control transfer
		usb_send_empty( sendtok );
	} else if(endp == 1) {
		// Keyboard end point
		// Send the previous payload. We want to send out the response as soon as possible and cannot afford to wait for building the payload
		static uint8_t button_state_prev = 0;
		static uint8_t usb_response[8] = { 0x00 }; // Format: modifiers_keys (1 byte), reserved (1 byte), key_scancodes (6 bytes)
		usb_send_data( usb_response, 8, 0, sendtok );

		// Build the next response. If PA2 is pressed, send out ENTER. Otherwise, send out NONE.
		uint8_t button_state = GPIO_digitalRead(GPIOv_from_PORT_PIN(GPIO_port_A, 2));
		if(button_state && button_state != button_state_prev) {
			usb_response[2] = HID_KEY_ENTER;
		} else {
			usb_response[2] = HID_KEY_NONE;
		}
		button_state_prev = button_state;
	}
}

int main() {
	SystemInit();
	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5µs ( TDDIS )
	usb_setup();

	GPIO_port_enable(GPIO_port_A);
	GPIO_pinMode(GPIOv_from_PORT_PIN(GPIO_port_A, 2), GPIO_pinMode_I_pullUp, GPIO_Speed_In);
	
	while(1) {
		// Do nothing.
	}
}
