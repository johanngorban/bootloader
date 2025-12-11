#include "usb_eeprom.h"
#include "util.h"
#include "log.h"
#include "flash.h"

extern setting_t setting;

void emul_eeprom_set_byte(uint16_t adr, uint8_t val)
{
	if (adr == 0xf100 && val == 0xFF)
	{
		setting_save(&setting);
	}
	if (adr < 0xFF)
	{
		setting.eeprom[adr] = val;
	}
	else
	{
		mes(OK, "EEPROM", "set %x adr %x" CRLF, val, adr);
	}
}

uint8_t emul_eeprom_get_byte(uint16_t adr)
{
	if (adr < 0xFF)
	{
		return setting.eeprom[adr];
	}
	mes(OK, "EEPROM", "get %x adr %x" CRLF, adr, adr);
	return 0xFF;
}

void usb_eeprom_clear(void)
{
	for (uint8_t i = 0; i < 0xFF; i++)
	{
		setting.eeprom[i] = 0xFF;
	}
	setting_save(&setting);
}

void usb_eeprom_dump(void)
{
	dump(setting.eeprom, 256, 1);
}