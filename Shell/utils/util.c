#include "util.h"
#include "config.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

#define CRC32_POLY 0x04C11DB7
#define CRC32_POLY_R 0xEDB88320

static uint32_t crc32_table[256];

// Инициализация таблицы CRC32
void init_crc32_table(void)
{
    uint32_t i, j, cr;
    
    for (i = 0; i < 256; ++i) {
        cr = i;
        for (j = 8; j > 0; --j) {
            cr = cr & 0x00000001 ? (cr >> 1) ^ CRC32_POLY_R : (cr >> 1);
        }
        crc32_table[i] = cr;
    }
}

uint8_t util_get_ver(uint32_t adr, uint32_t size, uint8_t *major, uint8_t *minor, uint8_t *patch, uint32_t *crc)
{
    uint32_t i, j, ver_addr = 0;
    uint8_t n = 0, temp[4];
    char data[20];

    *crc = 0xFFFFFFFF;
    __disable_irq();
    for (i = adr; i < adr + size; i++)
    {
        for (j = 0; j < 4; j++) {
            temp[j] = *(volatile uint8_t *)(i + j);
        }
        if (temp[0] == 0x56 && temp[1] == 0x45 && temp[2] == 0x52 && temp[3] == 0x3A)
        {
            ver_addr = i;
        }
        if (temp[0] == 0x1a && temp[1] == 0x1a)
        {
            break;
        }
        if (temp[0] == 0xff && temp[1] == 0xff && temp[2] == 0xff && temp[3] == 0xff)
        {
            break;
        }
        *crc = (*crc >> 8) ^ crc32_table[(*crc ^ temp[0]) & 0xff];
    }
    __enable_irq();
    for (i = 0; i < sizeof(data); i++)
    {
        data[n++] = *(__IO uint8_t *)(i + ver_addr + 4);
    }

    char *maj = strtok(data, ".");
    char *min = strtok(NULL, ".");
    char *pat = strtok(NULL, ".");

    *major = atoi(maj);
    *minor = atoi(min);
    *patch = atoi(pat);

		*crc = ~*crc;
		
    return 1;
}

void dump(unsigned char *block, uint16_t size, uint8_t with_border)
{
    int i, j;
    unsigned char c;
    cli_out(CRLF);
    if (with_border)
    {
        cli_out("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  |  0123456789abcdef" CRLF);
    }
    if (!block)
    {
        return;
    }
    for (i = 0; i < size; i += 16)
    {
        if (with_border)
        {
            cli_out("%02X:  ", i);
        }
        for (j = 0; j < 16; j++)
            if (i + j < size)
            {
                c = block[i + j] & 0xFF;
                cli_out("%02x ", c);
            }
            else
            {
                cli_out("-- ");
            }
        cli_out("|  ");
        for (j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                c = block[i + j];
                if (!isgraph(c))
                    c = '.';
                cli_out("%c", c);
            }
            else
            {
                cli_out(" ");
            }
        }

        cli_out(CRLF);
    }
}

void flash_jump_to_app(void)
{
		__disable_irq();
    typedef void (*fnc_ptr)(void);
    fnc_ptr jump_to_app;
    jump_to_app = (fnc_ptr) * (volatile uint32_t *)(FLASH_ADDR_APP + 4u);
    HAL_DeInit();
		HAL_RCC_DeInit();
		HAL_UART_DeInit(&huart1);
    RCC->APB1RSTR = 0xFFFFFFFFU;
    RCC->APB1RSTR = 0x00;
    RCC->APB2RSTR = 0xFFFFFFFFU;
    RCC->APB2RSTR = 0x00;
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 0;
    __set_BASEPRI(0);
    __set_CONTROL(0);
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICPR[1] = 0xFFFFFFFF;
    NVIC->ICER[2] = 0xFFFFFFFF;
    NVIC->ICPR[2] = 0xFFFFFFFF;
    __set_MSP(*(volatile uint32_t *)FLASH_ADDR_APP);
    SCB->VTOR = FLASH_ADDR_APP;
    jump_to_app();
}