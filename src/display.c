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

#include "display.h"
#include "ch32fun.h"
#include <stdlib.h>

#define DISPLAY_I2C_ADDR (0x3C)
#define DISPLAY_I2C_CLOCKRATE (100000) // SSD1306 supports up to 400kHz
#define DISPLAY_I2C_ERROR_FLAGS (I2C_STAR1_PECERR|I2C_STAR1_OVR|I2C_STAR1_AF|I2C_STAR1_ARLO|I2C_STAR1_BERR)

// Adapted from the sequence in the appendix of the SSD1306 specs
const static uint8_t display_init_array[] =
{
	0x00, // Control byte: the following bytes are to be treated as commands
	0xA8, 0x3F, // Set MUX ratio to 64MUX (0b111111)
	0xD3, 0x00, // Set display offset to 0
	0x40, // Set display start line to 0
	0xA1, // Set segment remap (column address 127 is mapped to SEG0)
	0xC8, // Set COM output scan direction to reverse (remapped mode. Scan from COM[N-1] to COM0)
	0xDA, 0x22, // Set COM pins hardware configuration (Sequential COM pin, Enable COM Left/Right remap)
	0x81, 0x7F, // Set Contrast Control (127)
	0xA4, // Entire Display on (0xA4 is display on, 0xA5 is display off)
	0xA6, // Non-inverted display (0xA7 is inverted display)
	0xD5, 0x80, // Set oscillator frequency (Fosc=1000b, Fdiv=0000b)
	0x8D, 0x14, // Enable charge pump regulator
	0xAF, // Display ON
};


#define DISPLAY_DATA_COMMAND_SIZE (20)
#define DISPLAY_DATA_SIZE (DISPLAY_WIDTH*4)

// CONCURRENCY_VARIABLE: read by display_loop() via TIM2 ISR, written by display_clear() / display_draw_*()
static uint8_t display_data_array[DISPLAY_DATA_COMMAND_SIZE+DISPLAY_DATA_SIZE] = {
	0x00, 0x00, 0x00, // Padding to make the graphc RAM area align with uint32_t
	0x80, 0x20, 0x80, 0x21, // Set memory addressing mode (Vertical addressing mode)
	0x80, 0x21, 0x80, 0x00, 0x80, 0x7F, // Setup column start and end address (0..127)
	0x80, 0x22, 0x80, 0x00, 0x80, 0x03, // Setup page start and end address (0..3)
	0x40, // all of the subsequent bytes are for OLED graphic RAM data.
	// 512 bytes of zeros (generated with Python `print((('0x00, '*16)[:-1] + '\n') * 32)`)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static uint8_t *const display_data_dma_start_address = &display_data_array[3];
static uint32_t *const display_data_buffer = (uint32_t*)&display_data_array[20];

#define DISPLAY_REFRESH_FLAG_INIT (1U<<0)
#define DISPLAY_REFRESH_FLAG_GRAPHIC (1U<<1)
uint8_t display_refresh_flag = 0; // CONCURRENCY_VARIABLE: written/read by display_loop() via TIM2 ISR, written/read by display_set_refresh_flag() / display_is_idle()

enum display_loop_step {
	DISPLAY_LOOP_STEP_IDLE,
	DISPLAY_LOOP_STEP_WAIT_TRANSFER, // common wait state shared by many SEND_ states
	DISPLAY_LOOP_STEP_WAIT_BUS_IDLE,
	DISPLAY_LOOP_STEP_SEND_START_BIT,
	DISPLAY_LOOP_STEP_SEND_ADDRESS,
	DISPLAY_LOOP_STEP_SEND_DATA_DMA,
	DISPLAY_LOOP_STEP_WAIT_DMA,
	DISPLAY_LOOP_STEP_SEND_END_BIT,
	DISPLAY_LOOP_STEP_SUCCESS,
	DISPLAY_LOOP_STEP_RESET_I2C_SETUP,
	DISPLAY_LOOP_STEP_RESET_I2C_CHECK_ERROR,
	DISPLAY_LOOP_STEP_RESET_I2C_SCL_HIGH,
};

#define DISPLAY_WAIT_SCL_FOR_I2C_RESET (FUNCONF_SYSTEM_CORE_CLOCK/DISPLAY_I2C_CLOCKRATE)
#define DISPLAY_WAIT_BUS_IDLE_TIMEOUT (FUNCONF_SYSTEM_CORE_CLOCK/1000 *3) // 3ms
#define DISPLAY_TRANSFER_TIMEOUT (FUNCONF_SYSTEM_CORE_CLOCK/1000 *3) // 3ms. For start bit, address and stop bit.
#define DISPLAY_DMA_TIMEOUT (FUNCONF_SYSTEM_CORE_CLOCK/1000 * 100) // 100ms. It takes 48ms to transfer 530 bytes at 100kHz.

// DISPLAY_CFGLR_FLAG: PC1 and PC2, 2Mhz output, open-drain alternative mode
// DISPLAY_CFGLR_FLAG_I2C_RESET: Same as above except that PC1 (SDA) is floating input mode. For bitbanging I2C reset
#define DISPLAY_GPIO_PORT (GPIOC)
#define DISPLAY_CFGLR_FLAG ((GPIO_CFGLR_MODE1_1|GPIO_CFGLR_MODE2_1) | (GPIO_CFGLR_CNF1_0|GPIO_CFGLR_CNF1_1|GPIO_CFGLR_CNF2_0|GPIO_CFGLR_CNF2_1))
#define DISPLAY_CFGLR_FLAG_I2C_RESET ((GPIO_CFGLR_MODE2_1) | (GPIO_CFGLR_CNF1_0|GPIO_CFGLR_CNF2_0))
#define DISPLAY_CFGLR_MASK ((GPIO_CFGLR_MODE1|GPIO_CFGLR_MODE2) | (GPIO_CFGLR_CNF1|GPIO_CFGLR_CNF2))

static void display_i2c_bus_init(void) {
	DISPLAY_GPIO_PORT->CFGLR = (DISPLAY_GPIO_PORT->CFGLR & ~DISPLAY_CFGLR_MASK) | DISPLAY_CFGLR_FLAG;
	// Set clock rate in standard I2C mode. Enable DMA mode. Enable ACK mode. Enable I2C
	// The reference manual on I2C clock rate is terrible. I just followed whatever openwch does.
	// Apparently I2C1->CKCFGR.CCR is some sort of clock divider, and the I2C1->CTLR2.FREQ doesn't do much
	// See also: https://kiedontaa.blogspot.com/2024/04/the-confusing-i2c-bit-rate-register-of.html
	#if (DISPLAY_I2C_CLOCKRATE > 100000)
		// I2C fast mode
		I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(DISPLAY_I2C_CLOCKRATE*3))&I2C_CKCFGR_CCR) | I2C_CKCFGR_FS;
	#else
		// I2C standard mode
		I2C1->CKCFGR = (FUNCONF_SYSTEM_CORE_CLOCK/(DISPLAY_I2C_CLOCKRATE*2))&I2C_CKCFGR_CCR;
	#endif
	I2C1->CTLR2 = ((FUNCONF_SYSTEM_CORE_CLOCK/1000000)&I2C_CTLR2_FREQ) | I2C_CTLR2_DMAEN;
	I2C1->CTLR1 = (I2C_CTLR1_ACK | I2C_CTLR1_PE); // Do I2C enable the last!
}

void display_loop(void)
{
	asm volatile ("" ::: "memory");
	static enum display_loop_step display_loop_step = DISPLAY_LOOP_STEP_IDLE;
	static enum display_loop_step display_loop_step_next;
	static uint32_t display_loop_step_start_waiting_tick;
	static uint16_t display_loop_step_expected_i2c_star1;
	static uint16_t display_loop_step_expected_i2c_star2;
	static uint8_t display_loop_step_reset_i2c_on_error;
	static uint8_t display_refresh_flag_processing;

	// Haters gonna hate. Using goto label here makes the code much cleaner than using do-while.
	process_again:
	switch(display_loop_step) {
		case DISPLAY_LOOP_STEP_IDLE:
			if(display_refresh_flag) {
				if(display_refresh_flag & DISPLAY_REFRESH_FLAG_INIT) {
					DMA1_Channel6->MADDR = (uint32_t)display_init_array;
					DMA1_Channel6->CNTR = sizeof(display_init_array);
					display_refresh_flag_processing = DISPLAY_REFRESH_FLAG_INIT;
				} else if(display_refresh_flag & DISPLAY_REFRESH_FLAG_GRAPHIC) {
					DMA1_Channel6->MADDR = (uint32_t)display_data_dma_start_address;
					DMA1_Channel6->CNTR = sizeof(display_data_array)-(display_data_dma_start_address-display_data_array);
					display_refresh_flag_processing = DISPLAY_REFRESH_FLAG_GRAPHIC;
				}

				// Clear I2C error flags and DMA error flag
				// The reason to clear it here is that, the error flags can get triggered in any moment
				// By clearing it at the beginning of the transfer, we make sure that the flags always get cleared upon retry.
				I2C1->STAR1 &= ~DISPLAY_I2C_ERROR_FLAGS;
				DMA1->INTFCR = DMA_CTEIF6;

				display_loop_step_reset_i2c_on_error = 0;
				display_loop_step_start_waiting_tick = SysTick->CNT;
				display_loop_step = DISPLAY_LOOP_STEP_WAIT_BUS_IDLE;
				goto process_again;
			}
		break;
		case DISPLAY_LOOP_STEP_WAIT_TRANSFER:
		{
			// Rationale not to use interrupt for handling completion of sending I2C start bit, address and stop bit:
			// Reason 1:
			// The I2C event flags are complicated. It might require multiple interrupt triggers to
			// reach the desired event flags, making it not worth it performane-wise.
			// Reason 2:
			// I2C/DMA interrupt events doesn't have timeout handling. I'd need a timer for that, which isn't worth it.
			// Reason 3:
			// In this project, I don't intend to refresh the display often so the rate that
			// the I2C communication mechanism get triggered is rare, making performance less important

			// Must read STAR1 first, then read STAR2. Otherwise STAR2.ADDR won't get reset by hardware
			uint16_t star1 = I2C1->STAR1;
			asm volatile ("" ::: "memory"); // prevent compiler from reordering the read between STAR1 and STAR2
			uint16_t star2 = I2C1->STAR2;
			if ((star1 & DISPLAY_I2C_ERROR_FLAGS) ||
				SysTick->CNT - display_loop_step_start_waiting_tick >= DISPLAY_TRANSFER_TIMEOUT) {
				// First attempt to recover by sending an I2C end bit. If that failed, perform I2C bus reset.
				if(!display_loop_step_reset_i2c_on_error) {
					display_loop_step = DISPLAY_LOOP_STEP_SEND_END_BIT;
				} else {
					display_loop_step = DISPLAY_LOOP_STEP_RESET_I2C_SETUP;
				}
				goto process_again;
			} else if(star1 == display_loop_step_expected_i2c_star1 && star2 == display_loop_step_expected_i2c_star2) {
				display_loop_step = display_loop_step_next;
				goto process_again;
			}
		}
		break;
		case DISPLAY_LOOP_STEP_WAIT_BUS_IDLE:
			if(!(I2C1->STAR2 & I2C_STAR2_BUSY)) {
				display_loop_step = DISPLAY_LOOP_STEP_SEND_START_BIT;
				goto process_again;
			} else if (SysTick->CNT - display_loop_step_start_waiting_tick >= DISPLAY_WAIT_BUS_IDLE_TIMEOUT) {
				display_loop_step = DISPLAY_LOOP_STEP_RESET_I2C_SETUP;
				goto process_again;
			}
		break;
		case DISPLAY_LOOP_STEP_SEND_START_BIT:
			I2C1->CTLR1 |= I2C_CTLR1_START;

			display_loop_step_expected_i2c_star1 = I2C_STAR1_SB;
			display_loop_step_expected_i2c_star2 = I2C_STAR2_MSL|I2C_STAR2_BUSY;
			display_loop_step_next = DISPLAY_LOOP_STEP_SEND_ADDRESS;
			display_loop_step_start_waiting_tick = SysTick->CNT;
			display_loop_step = DISPLAY_LOOP_STEP_WAIT_TRANSFER;
		break;
		case DISPLAY_LOOP_STEP_SEND_ADDRESS:
			I2C1->DATAR = DISPLAY_I2C_ADDR<<1;

			display_loop_step_expected_i2c_star1 = I2C_STAR1_ADDR|I2C_STAR1_TXE;
			display_loop_step_expected_i2c_star2 = I2C_STAR2_MSL|I2C_STAR2_BUSY|I2C_STAR2_TRA;
			display_loop_step_next = DISPLAY_LOOP_STEP_SEND_DATA_DMA;
			display_loop_step_start_waiting_tick = SysTick->CNT;
			display_loop_step = DISPLAY_LOOP_STEP_WAIT_TRANSFER;
		break;
		case DISPLAY_LOOP_STEP_SEND_DATA_DMA:
			DMA1_Channel6->CFGR |= DMA_CFGR6_EN;
			display_loop_step_start_waiting_tick = SysTick->CNT;
			display_loop_step = DISPLAY_LOOP_STEP_WAIT_DMA;
		break;
		case DISPLAY_LOOP_STEP_WAIT_DMA:
		{
			uint8_t go_to_next_step = 0;
			if ((DMA1->INTFR & DMA_TEIF6) ||
				SysTick->CNT - display_loop_step_start_waiting_tick >= DISPLAY_DMA_TIMEOUT) {
				// If the DMA couldn't be completed properly, I assume that the I2C bus is fucked up.
				// Let's reset that I2C bus, just in case.
				display_loop_step_reset_i2c_on_error = 1;
				go_to_next_step = 1;
			} else if(DMA1->INTFR & DMA_TCIF6) {
				DMA1->INTFCR = DMA_CTCIF6;
				go_to_next_step = 1;
			}

			if(go_to_next_step) {
				DMA1_Channel6->CFGR &= ~DMA_CFGR6_EN;
				display_loop_step = DISPLAY_LOOP_STEP_SEND_END_BIT;
				goto process_again;
			}
		}
		break;
		case DISPLAY_LOOP_STEP_SEND_END_BIT:
			I2C1->CTLR1 |= I2C_CTLR1_STOP;

			display_loop_step_expected_i2c_star1 = 0;
			display_loop_step_expected_i2c_star2 = 0;
			display_loop_step_next = display_loop_step_reset_i2c_on_error ? DISPLAY_LOOP_STEP_RESET_I2C_SETUP : DISPLAY_LOOP_STEP_SUCCESS;
			display_loop_step_start_waiting_tick = SysTick->CNT;
			display_loop_step_reset_i2c_on_error = 1;
			display_loop_step = DISPLAY_LOOP_STEP_WAIT_TRANSFER;
		break;
		case DISPLAY_LOOP_STEP_SUCCESS:
			display_refresh_flag &= ~display_refresh_flag_processing;
			display_loop_step = DISPLAY_LOOP_STEP_IDLE;
			goto process_again;
		break;
		case DISPLAY_LOOP_STEP_RESET_I2C_SETUP:
			DISPLAY_GPIO_PORT->CFGLR = (DISPLAY_GPIO_PORT->CFGLR & ~DISPLAY_CFGLR_MASK) | DISPLAY_CFGLR_FLAG_I2C_RESET;
			// DISPLAY_GPIO_PORT->BSHR = GPIO_BSHR_BS1; // SDA high (implicit because on-bus pull-up. Do not set. This pin is in input mode)
			DISPLAY_GPIO_PORT->BSHR = GPIO_BSHR_BS2; // SCL high
			display_loop_step_start_waiting_tick = SysTick->CNT;
			display_loop_step = DISPLAY_LOOP_STEP_RESET_I2C_CHECK_ERROR;
		break;
		case DISPLAY_LOOP_STEP_RESET_I2C_CHECK_ERROR:
			// Send pulses of SCL until the I2C line isn't busy anymore
			if(SysTick->CNT - display_loop_step_start_waiting_tick >= DISPLAY_WAIT_SCL_FOR_I2C_RESET) {
				if((DISPLAY_GPIO_PORT->INDR & GPIO_INDR_IDR1) == 0) { // Check SDA status
					DISPLAY_GPIO_PORT->BSHR = GPIO_BSHR_BR2; // SCL low
					display_loop_step_start_waiting_tick = SysTick->CNT;
					display_loop_step = DISPLAY_LOOP_STEP_RESET_I2C_SCL_HIGH;
				} else {
					// Great! With the the pulses sent, now that the error's gone!
					// Reset I2C peripheral
					I2C1->CTLR1 |= I2C_CTLR1_SWRST;
					I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;
					// Configure I2C peripheral again after resetting
					// Also configure GPIO
					display_i2c_bus_init();
					// Resend the init sequence for the OLED
					display_refresh_flag |= DISPLAY_REFRESH_FLAG_INIT;
					display_loop_step = DISPLAY_LOOP_STEP_IDLE;
					goto process_again;
				}
			}
		break;
		case DISPLAY_LOOP_STEP_RESET_I2C_SCL_HIGH:
			// Wait for the time to send the next clock, then set SCL high
			if(SysTick->CNT - display_loop_step_start_waiting_tick >= DISPLAY_WAIT_SCL_FOR_I2C_RESET) {
				DISPLAY_GPIO_PORT->BSHR = GPIO_BSHR_BS2; // SCL high
				display_loop_step_start_waiting_tick = SysTick->CNT;
				display_loop_step = DISPLAY_LOOP_STEP_RESET_I2C_CHECK_ERROR;
			}
		break;
	}
}

void display_clear(void) {
	memset(display_data_buffer, 0, DISPLAY_DATA_SIZE);
}

void display_draw_16(const uint16_t *image, uint8_t w, int32_t x, int32_t y, uint8_t flags) {
	if(flags & DISPLAY_DRAW_FLAG_SCALE_2x) {
		w *= 2;
	}
	for(int32_t i=0; i<w; i++) {
		if(x+i >= DISPLAY_WIDTH) {
			break;
		} else if(x+i < 0) {
			continue;
		}
		size_t image_index = (flags & DISPLAY_DRAW_FLAG_SCALE_2x) ? i/2 : i;
		uint32_t image_to_be_shown = (flags & DISPLAY_DRAW_FLAG_INVERT) ? (uint16_t)~image[image_index] : image[image_index];
		if(flags & DISPLAY_DRAW_FLAG_SCALE_2x) {
			uint32_t image_original = image_to_be_shown;
			image_to_be_shown = 0;
			for(size_t j=0; j<16; j++) {
				if(image_original & (1 << j)) {
					image_to_be_shown |= 0x03 << (j*2);
				}
			}
		}

		if(y > 0) {
			display_data_buffer[x+i] |= image_to_be_shown << y;
		} else {
			display_data_buffer[x+i] |= image_to_be_shown >> -y;
		}
	}
}

void display_init(void) {
	RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;

	display_i2c_bus_init();

	// DMA initialization
	RCC->AHBPCENR |= RCC_DMA1EN;
	DMA1_Channel6->CFGR = DMA_CFGR6_MINC | DMA_CFGR6_DIR; // increment memory, read from memory
	DMA1_Channel6->PADDR = (uint32_t)(&I2C1->DATAR);

	// Initialize state variables
	display_refresh_flag = DISPLAY_REFRESH_FLAG_INIT;
	display_clear();
}

void display_set_refresh_flag(void) {
	// Make sure that the display_data_buffer changes are written and would be seen by the DMAs
	asm volatile("fence ow,ow");

	// Not sure if the write operation is atomic. Disabling interrupts just in case.
	__disable_irq();
	asm volatile ("" ::: "memory");
	display_refresh_flag |= DISPLAY_REFRESH_FLAG_GRAPHIC;
	__enable_irq();
}

uint8_t display_is_idle(void) {
	asm volatile ("" ::: "memory");
	return !display_refresh_flag;
}
