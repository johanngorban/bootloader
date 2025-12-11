#include "board.h"
#include "shell.h"
#include "config.h"
#include "cli.h"
#include "util.h"
#include "flash.h"
#include "cmsis_armcc.h"

void board_init(void)
{
    init_crc32_table();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0); // 12V power off
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, 1); // Switch to MCPU
    HAL_Delay(100);
    cli_out("\x1b[2J\x1b[1;1HBOOT VER %s" CRLF, APP_VERSION);
    HAL_Delay(100);
    uint8_t minor, major, patch;
    uint32_t crc;
    util_get_ver(FLASH_ADDR_APP, APP_SIZE, &major, &minor, &patch, &crc);
    cli_out("current software    ver: %d.%d.%d crc: 0x%x" CRLF, major, minor, patch, crc);
    /*if (major != 0 && HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_11))
    {
        cli_out("Update software start" CRLF);
        if (flash_copy(FLASH_ADDR_DISK, FLASH_ADDR_APP, APP_SIZE) == FLASH_OK)
        {
            flash_jump_to_app();
            return;
        }
        cli_out("ERROR");
    }
    uint32_t is_boot = *(volatile uint32_t *)0x40006C08;
    util_get_ver(FLASH_ADDR_APP, APP_SIZE, &major, &minor, &patch, &crc);
    if (major != 0 && HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_11) && !is_boot)
    {
        flash_jump_to_app();
    }
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    *(volatile uint32_t *)0x40006C08 = 0;
    PWR->CR &= ~PWR_CR_DBP;*/
    shell_init();
}

void board_handler(void)
{
    shell_handler();
}