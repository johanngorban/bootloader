#include "xmodem.h"
#include "flash.h"
#include "cli.h"
#include "config.h"

extern UART_HandleTypeDef CONSOLE_UART;

uint8_t xmodem_byte_rx, xmodem_byte_tx, xmodem_packet_number = 1;
uint8_t xmodem_packet_data[XMODEM_PACKET_1024_SIZE], xmodem_packet_num[XMODEM_PACKET_DATA_INDEX], xmodem_packet_crc[XMODEM_PACKET_CRC_SIZE];
uint8_t xmodem_first_packet_received = 0;
uint32_t xmodem_flash_address, xmodem_block_number = 1;

static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0u;
    while (length)
    {
        length--;
        crc = crc ^ ((uint16_t)*data++ << 8u);
        for (uint8_t i = 0u; i < 8u; i++)
        {
            if (crc & 0x8000u)
            {
                crc = (crc << 1u) ^ 0x1021u;
            }
            else
            {
                crc = crc << 1u;
            }
        }
    }
    return crc;
}

xmodem_result_t xmodem_process_packet(uint8_t ch)
{
    if (ch != XMODEM_SOH && ch != XMODEM_STX)
    {
        return XMODEM_ERROR_UNKNOWN;
    }
    uint16_t size = (ch == XMODEM_SOH) ? XMODEM_PACKET_128_SIZE : XMODEM_PACKET_1024_SIZE;
    if (HAL_UART_Receive(&CONSOLE_UART, xmodem_packet_num, XMODEM_PACKET_DATA_INDEX, HAL_MAX_DELAY) != HAL_OK)
    {
        return XMODEM_ERROR_UART;
    }
    if (HAL_UART_Receive(&CONSOLE_UART, xmodem_packet_data, size, HAL_MAX_DELAY) != HAL_OK)
    {
        return XMODEM_ERROR_UART;
    }
    if (HAL_UART_Receive(&CONSOLE_UART, xmodem_packet_crc, XMODEM_PACKET_CRC_SIZE, HAL_MAX_DELAY) != HAL_OK)
    {
        return XMODEM_ERROR_UART;
    }

    if ((xmodem_packet_num[0] + xmodem_packet_num[1]) != 0xFF)
    {
        return XMODEM_ERROR_NUMBER;
    }
    if (xmodem_calc_crc(&xmodem_packet_data[0u], size) != ((xmodem_packet_crc[0] << 8) | xmodem_packet_crc[1]))
    {
        return XMODEM_ERROR_CRC;
    }
    if (xmodem_packet_num[0] != xmodem_packet_number)
    {
        return XMODEM_ERROR_NUMBER;
    }
    if (xmodem_first_packet_received == 0)
    {
        xmodem_first_packet_received = 1;
        flash_result_t res = flash_erase(FLASH_ADDR_DISK, APP_SIZE);
        if (res != FLASH_OK)
        {
            xmodem_byte_tx = XMODEM_CAN;
            HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
            HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
            HAL_Delay(100);
            cli_out("\r\n[XMODEM] Flash erase error, %d\r\n");
            return XMODEM_ERROR_UNKNOWN;
        }
    }
    flash_result_t res = flash_block_write(xmodem_block_number, (uint32_t *)&xmodem_packet_data[0u], (uint32_t)size);
    if (res != FLASH_OK)
    {
        xmodem_byte_tx = XMODEM_CAN;
        HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
        HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
        HAL_Delay(100);
        return XMODEM_ERROR_UNKNOWN;
    }
    xmodem_block_number++;
    xmodem_packet_number++;
    return XMODEM_OK;
}

xmodem_result_t xmodem_handler(void)
{
    if (HAL_UART_Receive(&CONSOLE_UART, &xmodem_byte_rx, 1, XMODEM_UART_TIMEOUT) != HAL_OK)
    {
        xmodem_byte_tx = 'C';
        HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
        return XMODEM_WITE;
    }
    if (xmodem_byte_rx == XMODEM_CAN || (xmodem_block_number == 1 && xmodem_byte_rx == XMODEM_CTRL_C))
    {
        xmodem_first_packet_received = 0;
        xmodem_packet_number = 1;
        xmodem_block_number = 1;
        return XMODEM_CANCEL;
    }
    if (xmodem_byte_rx == XMODEM_EOT)
    {
        xmodem_byte_tx = XMODEM_ACK;
        HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
        HAL_Delay(100);
        cli_out("\r\n[XMODEM] File upload, %d\r\n", xmodem_block_number * 1024);
        xmodem_first_packet_received = 0;
        xmodem_packet_number = 1;
        return XMODEM_OK;
    }
    xmodem_result_t res = xmodem_process_packet(xmodem_byte_rx);
    if (res != XMODEM_OK)
    {
        cli_out("\r\n[XMODEM] Error, %d\r\n", xmodem_packet_number);
        xmodem_byte_tx = XMODEM_NAK;
        HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
        return res;
    }
    xmodem_byte_tx = XMODEM_ACK;
    HAL_UART_Transmit(&CONSOLE_UART, &xmodem_byte_tx, 1, XMODEM_UART_TIMEOUT);
    return XMODEM_WITE;
}