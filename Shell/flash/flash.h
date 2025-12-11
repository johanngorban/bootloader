#ifndef __FR_FLASH_H
#define __FR_FLASH_H

#include "stm32f1xx.h"

typedef struct setting_struct
{
    unsigned char password[20];
    uint8_t wp;
    uint8_t eeprom[256];
} setting_t;

typedef enum
{
    FLASH_OK,
    FLASH_ERROR_ERASE,
    FLASH_ERROR_WRITE,
    FLASH_ERROR_OVERFLOW
} flash_result_t;

void setting_read(setting_t *setting);
flash_result_t setting_save(setting_t *setting);

flash_result_t flash_block_write(uint32_t number, uint32_t *data, uint32_t size);
flash_result_t flash_erase(uint32_t adr, uint32_t size);
flash_result_t flash_copy(uint32_t from, uint32_t to, uint32_t size);

#endif // FLASH
