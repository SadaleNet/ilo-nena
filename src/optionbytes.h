#include <stdint.h>

uint16_t optionbytes_get_data(void);
uint32_t optionbytes_write_data(uint16_t data); // Return value: 0 is OK, non-zero for checksum error.
