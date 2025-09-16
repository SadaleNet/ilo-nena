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

#define TIM2_INTERVAL_US (1000) // The approximate interval between each run of the task. Actual interval would be slightly longer than that.

void INTERRUPT_DECORATOR TIM2_IRQHandler(void) {
	// For performance, we just set the interrupt flags to zero. We're not gonna use TIM2 interrupt flags for anything else anyway
	// TIM2->INTFR &= TIM_UIF;
	TIM2->INTFR = 0;
	button_loop();

	// Start the timer again. This is required because we're in single-shot mode.
	// Purpose of using single-shot mode:
	// We set the keyscan column output, then we must wait for a delay, then we read the row input in next timer interrupt.
	// If we used continuous mode instead of single-shot, in case a higher priority interrupt preempted and takes a long time,
	// our current timer interrupt would get triggered again right after it ended, which would eliminate the required delay
	TIM2->CTLR1 |= TIM_CEN;

	// Do the display loop after starting the timer. Unlike button_loop(), display_loop() isn't wait-sensitive.
	display_loop();
}

void tim2_task_init(void) {
	// Enable clock for TIM2
	RCC->APB1PCENR |= RCC_TIM2EN;
	// Enble interrupt when update flag is active
	TIM2->DMAINTENR = TIM_UIE;
	// Set timer interval
	// Example (FUNCONF_SYSTEM_CORE_CLOCK=48000000, TIM2_INTERVAL_US=1000): 48000000/48/1000 = 1000Hz
	TIM2->PSC = (FUNCONF_SYSTEM_CORE_CLOCK/1000000-1);
	TIM2->ATRLR = (TIM2_INTERVAL_US-1);
	// Single-shot mode, only set update flag when the timer overflows. Also start timer!
	TIM2->CTLR1 = TIM_OPM | TIM_URS | TIM_CEN;

	// PFIC: For TIM2_IRQHandler, enable preemption for the interrupt. Also enable the interrupt.
	PFIC->IPRIOR[TIM2_IRQn] = 0x80;
	PFIC->IENR[TIM2_IRQn/32] |= (1<<(TIM2_IRQn%32));
}
