#ifndef __BOS_UTIL_H
#define __BOS_UTIL_H

#include "stm32f1xx.h"

void init_crc32_table(void);
uint8_t util_get_ver(uint32_t adr, uint32_t size, uint8_t *minor, uint8_t *major, uint8_t *patch, uint32_t *crc);
void dump(unsigned char *block, uint16_t size, uint8_t with_border);
void flash_jump_to_app(void);

#endif // UTILS_I2C
