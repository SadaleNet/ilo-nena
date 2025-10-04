#include "ch32fun.h"
#include <assert.h>
#include <stdint.h>

static uint8_t optionbytes_get_verified_byte(uint16_t data) {
	if((data & 0xFF) != (~(data >> 8) & 0xFF)) { // USER != ~nUSER
		return 0;
	}
	return data & 0xFF;
}

static uint16_t optionbytes_compute_upper_byte(uint8_t data) {
	return ((~data) << 8) | data;
}

uint16_t optionbytes_get_data(void) {
	return optionbytes_get_verified_byte(OB->Data0) | (optionbytes_get_verified_byte(OB->Data1) << 8);
}

uint32_t optionbytes_write_data(uint16_t data) {
	uint8_t ret = 0;
	// The pending option bytes to be written
	uint16_t pending_optbytes[6] = {
		optionbytes_compute_upper_byte(0xA5), // RDPR
		optionbytes_compute_upper_byte(0xF7), // USER, same as default as shown on the specs of CH32V003.
		optionbytes_compute_upper_byte(data & 0xFF), // Data0
		optionbytes_compute_upper_byte(data >> 8), // Data1
		optionbytes_compute_upper_byte(0xFF), // WPR0
		optionbytes_compute_upper_byte(0xFF), // WPR1
	};

	// Flash write cycle conservation:
	// Only perform flashing if the pending flash content is different from the existying one.
	uint8_t same_content = 1;
	static_assert(sizeof(pending_optbytes)%4 == 0, "Size of pending_optbytes is not divisible by 4. The for loop below won't work. Please rewrite.");
	for(size_t i=0; i<sizeof(pending_optbytes)/4; i++) {
		if(((uint32_t*)OB_BASE)[i] != ((uint32_t*)pending_optbytes)[i]) {
			same_content = 0;
			break;
		}
	}
	if(same_content) {
		return 0;
	}

	// 16.5.3 User Option Bytes Erasure
	// 1) Check the LOCK bit of FLASH_CTLR register, if it is 1, you need to execute the "Unlock Flash" operation.
	if(FLASH->CTLR & FLASH_CTLR_LOCK) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}

	// 2) Check the BSY bit of the FLASH_STATR register to confirm that there is no programming operation in progress.
	while(FLASH->STATR & FLASH_BUSY); 

	// 3) Check the OBWRE bit of FLASH_CTLR register, if it is 0, it is necessary to execute the operation of "user option bytes unlock".
	if(!(FLASH->CTLR & FLASH_CTLR_OPTWRE)) {
		FLASH->OBKEYR = FLASH_KEY1;
		FLASH->OBKEYR = FLASH_KEY2;
	}

	// 4) Set the OBER bit of FLASH_CTLR register to '1', after that set the STAT bit of FLASH_CTLR register to '1' to enable the user option bytes erase.
	FLASH->CTLR |= FLASH_CTLR_OPTER;
	FLASH->CTLR |= FLASH_CTLR_STRT;

	// 5) Wait for the BYS bit to become '0' or the EOP bit of FLASH_STATR register to be '1' to indicate the end of erase, and clear the EOP bit to 0
	while(FLASH->STATR & FLASH_BUSY);
	FLASH->STATR |= FLASH_STATR_EOP; // write 1 to clear 0

	// 6) Read and erase the address data checksum.
	// (Skipped. We will read the checksum after we program the option bytes)

	// 7) End to clear the OBER bit to 0.
	FLASH->CTLR &= ~FLASH_CTLR_OPTER;

	// 16.5.2 User Option Bytes Programming
	// 1) Check the LOCK bit of FLASH_CTLR register, if it is 1, you need to execute the "Unlock Flash" operation.
	// (skipped. We've already done that for flash erasure)

	// 2) Check the BSY bit of the FLASH_STATR register to confirm that there are no other programming operations in progress.
	// (skipped. We've already done that for flash erasure)

	// 3) Set the OBPG bit of FLASH_CTLR register to '1', after that set the STAT bit of FLASH_CTLR register to '1' to turn on the user option bytes programming.
	FLASH->CTLR |= FLASH_CTLR_OPTPG;
	FLASH->CTLR |= FLASH_CTLR_STRT;

	// 4) Set the OBPG bit of FLASH_CTLR register to '1'.
	FLASH->CTLR |= FLASH_CTLR_STRT; // Not sure on why the specs tell me to do that again. Could be a typo. But I guess there's no harm to do that.

	// 5~7) Loop for each pending option byte:
	for(size_t i=0; i<sizeof(pending_optbytes)/sizeof(*pending_optbytes); i++) {
		// 5) Write the half word (2 bytes) to be programmed to the specified address.
		((uint16_t*)OB_BASE)[i] = pending_optbytes[i];
		// 6) Wait for the BYS bit to become '0' or the EOP bit of FLASH_STATR register to be '1' to indicate the end of programming, and clear the EOP bit to 0.
		while(FLASH->STATR & FLASH_BUSY);
		FLASH->STATR |= FLASH_STATR_EOP; // write 1 to clear 0
		// 7) Read the programmed address data checksum.
		if(((uint16_t*)OB_BASE)[i] != pending_optbytes[i]) {
			ret |= (1<<i); // If the checksum's wrong, report the error when the function returns
		}
	}
	
	// 8) Continue programming you can repeat steps 5-7 and end programming to clear the OBPG bit to 0.
	FLASH->CTLR &= ~FLASH_CTLR_OPTPG;

	// Lock OBWRE again. Write 0 to lock for this one.
	FLASH->CTLR &= ~FLASH_CTLR_OPTWRE;
	// Lock the flash again. Write 1 to lock for this one.
	FLASH->CTLR |= FLASH_CTLR_LOCK;

	return ret;
}
