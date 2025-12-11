#include "rtc.h"

#define BASE_YEAR 1970
#define SECONDS_IN_DAY 86400

static uint8_t rtc_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t is_leap_year(uint16_t year)
{
    return (((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0));
}

uint8_t rtc_init(void)
{
    uint32_t cntr = 0;
    if (RTC->PRLL != 0x7FFF)
    {
        RCC->BDCR &= ~RCC_BDCR_RTCEN;
    }
    if (!(RCC->BDCR & RCC_BDCR_RTCEN))
    {
        RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
        PWR->CR |= PWR_CR_DBP;
        RCC->BDCR |= RCC_BDCR_BDRST;
        RCC->BDCR &= ~RCC_BDCR_BDRST;
        RCC->BDCR |= RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;
        RCC->BDCR |= RCC_BDCR_LSEON;
        while ((RCC->BDCR & RCC_BDCR_LSEON) != RCC_BDCR_LSEON)
        {
            if (++cntr == 144000000)
            {
                return 0;
            }
        }
        BKP->RTCCR |= 3;
        cntr = 0;
        while (!(RTC->CRL & RTC_CRL_RTOFF))
        {
            if (++cntr == 144000000)
            {
                return 0;
            }
        }
        RTC->CRL |= RTC_CRL_CNF;
        RTC->PRLL = 0x7FFF;
        RTC->CRL &= ~RTC_CRL_CNF;
        cntr = 0;
        while (!(RTC->CRL & RTC_CRL_RTOFF))
        {
            if (++cntr == 144000000)
            {
                return 0;
            }
        }
        RTC->CRL &= (uint16_t)~RTC_CRL_RSF;
        cntr = 0;
        while ((RTC->CRL & RTC_CRL_RSF) != RTC_CRL_RSF)
        {
            if (++cntr == 144000000)
            {
                return 0;
            }
        }
        PWR->CR &= ~PWR_CR_DBP;
    }
    return 1;
}

uint32_t rtc_get_counter(void)
{
    return (uint32_t)((RTC->CNTH << 16) | RTC->CNTL);
}

void rtc_set_counter(uint32_t count)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    while (!(RTC->CRL & RTC_CRL_RTOFF))
        ;
    RTC->CRL |= RTC_CRL_CNF;
    RTC->CNTH = count >> 16;
    RTC->CNTL = count;
    RTC->CRL &= ~RTC_CRL_CNF;
    while (!(RTC->CRL & RTC_CRL_RTOFF))
        ;
    PWR->CR &= ~PWR_CR_DBP;
}

RtcTypeDef sec_to_date(uint32_t sec)
{
    RtcTypeDef dt = {0};
    uint32_t days = sec / SECONDS_IN_DAY;
    uint32_t remaining_seconds = sec % SECONDS_IN_DAY;

    // Определяем год
    dt.year = BASE_YEAR;
    while (1)
    {
        uint16_t days_in_year = is_leap_year(dt.year) ? 366 : 365;
        if (days >= days_in_year)
        {
            days -= days_in_year;
            dt.year++;
        }
        else
        {
            break;
        }
    }

    // Определяем месяц
    dt.month = 1;
    while (1)
    {
        uint8_t days_in_month;
        if (dt.month == 2 && is_leap_year(dt.year))
        {
            days_in_month = 29;
        }
        else
        {
            days_in_month = rtc_days[dt.month - 1];
        }

        if (days >= days_in_month)
        {
            days -= days_in_month;
            dt.month++;
        }
        else
        {
            break;
        }
    }

    // Остальные значения
    dt.date = days + 1;
    dt.hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    dt.minutes = remaining_seconds / 60;
    dt.seconds = remaining_seconds % 60;
    return dt;
}

void rtc_set_time(RtcTypeDef dt)
{
    uint32_t result = 0;

    // Рассчитать общее количество дней для полных лет
    for (uint16_t year = BASE_YEAR; year < dt.year; year++)
    {
        result += (is_leap_year(year) ? 366 : 365) * SECONDS_IN_DAY;
    }

    // Рассчитать количество дней в полных месяцах текущего года
    for (uint8_t month = 1; month < dt.month; month++)
    {
        if (month == 2 && is_leap_year(dt.year))
        {
            result += 29 * SECONDS_IN_DAY;
        }
        else
        {
            result += (uint32_t)rtc_days[month - 1] * SECONDS_IN_DAY;
        }
    }

    // Добавить дни текущего месяца
    result += (uint32_t)(dt.date - 1) * SECONDS_IN_DAY;
    // Добавить часы, минуты и секунды текущего дня
    result += (uint32_t)dt.hours * 60 * 60;
    result += (uint32_t)dt.minutes * 60;
    result += dt.seconds;

    // Установить счетчик RTC
    rtc_set_counter(result);
}