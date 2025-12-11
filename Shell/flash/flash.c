#include "flash.h"
#include "config.h"
#include "cli.h"

/*записывет блок данных data, размер блока size, номер блока number*/
flash_result_t flash_block_write(uint32_t number, uint32_t *data, uint32_t size)
{
    uint32_t length = size / 4;
    uint32_t address = (uint32_t)FLASH_ADDR_DISK + ((number - 1) * size);
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < length; i++)
    {
        if (address >= (FLASH_ADDR_DISK + APP_SIZE))
        {
            HAL_FLASH_Lock();
            return FLASH_ERROR_OVERFLOW;
        }
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return FLASH_ERROR_WRITE;
        }
        if (data[i] != *(volatile uint32_t *)address)
        {
            HAL_FLASH_Lock();
            return FLASH_ERROR_WRITE;
        }
        address += 4;
    }
    HAL_FLASH_Lock();
    return FLASH_OK;
}

/* стирает APP_SIZE блков начиная с FLASH_ADDR_DISK*/
flash_result_t flash_erase(uint32_t adr, uint32_t size)
{
    // Разблокируем флеш-память для доступа к регистрам управления флеш
    HAL_FLASH_Unlock();

    // Заполняем структуру для стирания
    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = adr;
    erase_init.Banks = FLASH_BANK_1;

    // Вычисляем количество страниц, которые нужно стереть
    uint32_t startPage = adr / FLASH_PAGE_SIZE;
    uint32_t endPage = (uint32_t)((adr + size - 1) / FLASH_PAGE_SIZE);
    uint32_t totalPages = endPage - startPage + 1;

    // Проверяем на выход за пределы допустимого диапазона (предполагаем, что FLASH_PAGE_SIZE и FLASH_BANK1_END определены)
    if ((startPage >= (FLASH_BANK1_END / FLASH_PAGE_SIZE)) || (totalPages > (FLASH_BANK1_END / FLASH_PAGE_SIZE - startPage)))
    {
        HAL_FLASH_Lock();
        return FLASH_ERROR_ERASE;
    }

    erase_init.NbPages = (uint8_t)totalPages;
    uint32_t error = 0u;

    // Выполняем операцию стирания и проверяем результат
    if (HAL_FLASHEx_Erase(&erase_init, &error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        // Опционально можно мапировать специфический статус HAL на пользовательские коды ошибок на основе 'error'.
        return FLASH_ERROR_ERASE;
    }

    // Блокируем флеш-память для предотвращения доступа к регистрами управления флеш
    HAL_FLASH_Lock();

    return FLASH_OK;
}

void print_progress(size_t done, size_t total) {
    int width = 50;
    float ratio = (float)done / total;
    int c = ratio * width;

    cli_out("\r[");
    for (int i = 0; i < c; i++) {
        cli_out("=");
    }
    for (int i = c; i < width; i++) {
        cli_out(" ");
    }
    cli_out("] %3d%%", (int)(ratio * 100));
}

flash_result_t flash_copy(uint32_t from, uint32_t to, uint32_t size)
{
    if (flash_erase(to, size) != FLASH_OK)
    {
        return FLASH_ERROR_ERASE;
    }
    uint32_t data;
    uint32_t length = size / 4;
    HAL_FLASH_Unlock();
    __disable_irq();
    for (uint32_t i = 0; i < length; i++)
    {
        data = *(volatile uint32_t *)(from + i * 4);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, to + i * 4, data) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return FLASH_ERROR_WRITE;
        }
        if ((i % 1000) == 0)
        {
            print_progress(i, length);
        }
    }
    __enable_irq();
    HAL_FLASH_Lock();
    if (flash_erase(from, size) != FLASH_OK)
    {
        return FLASH_ERROR_ERASE;
    }
    print_progress(length, length);
    return FLASH_OK;
}