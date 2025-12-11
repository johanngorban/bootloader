#ifndef BOS_XMODEM_H
#define BOS_XMODEM_H

#include "stm32f1xx.h"

#define XMODEM_UART_TIMEOUT ((uint16_t)2000u)
#define XMODEM_PACKET_128_SIZE ((uint16_t)128u)
#define XMODEM_PACKET_1024_SIZE ((uint16_t)1024u)
#define XMODEM_PACKET_CRC_SIZE ((uint16_t)2u)
#define XMODEM_PACKET_DATA_INDEX ((uint16_t)2u)
#define XMODEM_SOH ((uint8_t)0x01u)    // Start Of Header (128 bytes)
#define XMODEM_STX ((uint8_t)0x02u)    // Start Of Header (1024 bytes)
#define XMODEM_EOT ((uint8_t)0x04u)    // End Of Transmission
#define XMODEM_ACK ((uint8_t)0x06u)    // Acknowledge
#define XMODEM_NAK ((uint8_t)0x15u)    // Not Acknowledge
#define XMODEM_CAN ((uint8_t)0x18u)    // Cancel
#define XMODEM_C ((uint8_t)0x43u)      // ASCII "C", to notify the host, we want to use CRC16
#define XMODEM_CTRL_C ((uint8_t)0x03u) // ^c отмена

typedef enum
{
    XMODEM_OK = 0,
    XMODEM_WITE,
    XMODEM_ERROR_CRC,
    XMODEM_ERROR_NUMBER,
    XMODEM_ERROR_UART,
    XMODEM_ERROR_UNKNOWN,
    XMODEM_CANCEL
} xmodem_result_t;

xmodem_result_t xmodem_handler(void);

#endif // XMODEM_H
