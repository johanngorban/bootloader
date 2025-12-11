#ifndef __FR_USB_EEPROM_H
#define __FR_USB_EEPROM_H

#include "stm32f1xx.h"

void emul_eeprom_set_byte(uint16_t adr, uint8_t val);
uint8_t emul_eeprom_get_byte(uint16_t adr);
void usb_eeprom_dump(void);
void usb_eeprom_clear(void);

#endif // usb_eeprom
