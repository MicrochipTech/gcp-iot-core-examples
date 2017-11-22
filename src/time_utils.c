/**
 * \file
 * \brief  Date/Time Utilities (RTC)
 *
 * \copyright (c) 2017 Microchip Technology Inc. and its subsidiaries.
 *            You may use this software and any derivatives exclusively with
 *            Microchip products.
 *
 * \page License
 * 
 * (c) 2017 Microchip Technology Inc. and its subsidiaries. You may use this
 * software and any derivatives exclusively with Microchip products.
 * 
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
 * WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
 * 
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIPS TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 * 
 * MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
 * TERMS.
 */

#include "asf.h"
#include "time_utils.h"

/* Globals */
static bool g_time_set;

#if SAM0
extern struct rtc_module    rtc_instance;
#endif

uint32_t time_utils_convert(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{
    uint32_t ret = 0;

    //January and February are counted as months 13 and 14 of the previous year
    if(month <= 2)
    {
        month += 12;
        year -= 1;
    }
     
    //Convert years to days
    ret = (365 * year) + (year / 4) - (year / 100) + (year / 400);
    //Convert months to days
    ret += (30 * month) + (3 * (month + 1) / 5) + day;
    //Unix time starts on January 1st, 1970
    ret -= 719561;
    //Convert days to seconds
    ret *= 86400;
    //Add hours, minutes and seconds
    ret += (3600 * hour) + (60 * minute) + second;
     
    return ret;
}

uint32_t time_utils_get_utc(void)
{
    if(g_time_set)
    {
#if SAM0
        return rtc_count_get_count(&rtc_instance);
#elif SAM
        uint32_t year;
        uint32_t month;
        uint32_t day;
        uint32_t hour;
        uint32_t minute;
        uint32_t second;

        rtc_get_date(RTC, &year, &month, &day, NULL);
        rtc_get_time(RTC, &hour, &minute, &second);
        
        return time_utils_convert(year, month, day, hour, minute, second);
#endif
    }
    else
    {
        return 0;
    }
}

void time_utils_set(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{
#if SAM0
    uint32_t ts = time_utils_convert(year, month, day, hour, minute, second);
    rtc_count_set_count(&rtc_instance, ts);
#elif SAM
    rtc_set_date(RTC, year, month, day, 1);
    rtc_set_time(RTC, hour, minute, second);
#endif

    g_time_set = true;
}
