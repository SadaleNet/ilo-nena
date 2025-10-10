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

// Comments extracted from CH32V003 Reference Manual

void watchdog_init(void) {
	// This configure a watchdog with maximum timeout. i.e. 128kHz / 256 / 0xFFF = 0.122Hz = interval 8.19 seconds

	// 1) Counting time base: IWDG clock source LSI, set the LSI crossover value clock as the counting time base
	// of IWDG through the IWDG_PSCR register. The operation method first writes 0x5555 to the
	// IWDG_CTLR register, and then modifies the crossover value in the IWDG_PSCR register. the PVU bit
	// in the IWDG_STATR register indicates the update status of the crossover value, and the crossover value
	// can be modified and read out only when the update is completed.
	// (LSI is 128kHz)
	IWDG->CTLR = 0x5555;
	IWDG->PSCR = 0x7; // 111: Divided by 256
	// (I've tried waiting for !(IWDG->STATR & IWDG_PVU) but it got stuck. Looks like that PVU would only get reset later on)

	// 2) Reload value: Used to update the current value of the counter in the standalone watchdog and the counter
	// is decremented by this value. The RVU bit in the IWDG_STATR register indicates the update status of
	// the reload value, and the IWDG_RLDR register can be modified and read out only when the update is
	// completed.

	// (Same as default value. Skipped!)
	// IWDG->CTLR = 0x5555;
	// IWDG->RLDR = 0xFFF; // 111: Divided by 256
	// (For the same reason as PVU, don't wait for !(IWDG->STATR & IWDG_RVU) here.)

	// 3) Watchdog enable: write 0xCCCC to the IWDG_CTLR register to enable the watchdog function.
	IWDG->CTLR = 0xCCCC;
}

void watchdog_feed(void) {
	// 4) Feed the dog: i.e., flush the current counter value before the watchdog counter decrements to 0 to prevent
	// a system reset from occurring. Write 0xAAAA to the IWDG_CTLR register to allow the hardware to
	// update the IWDG_RLDR register value to the watchdog counter. This action needs to be executed
	// regularly after the watchdog function is turned on, otherwise a watchdog reset action will occur.
	IWDG->CTLR = 0xAAAA;
}
