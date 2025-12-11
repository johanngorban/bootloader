#ifndef __FR_RTC_H
#define __FR_RTC_H

#include "stm32f1xx.h"

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t date;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
} RtcTypeDef;

uint8_t rtc_init(void);
uint32_t rtc_get_counter(void);
void rtc_set_counter(uint32_t count);
RtcTypeDef sec_to_date(uint32_t sec);
void rtc_set_time(RtcTypeDef dt);

#endif