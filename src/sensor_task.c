/**
 * \file
 * \brief  Sensor Monitoring and Logic
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
#include "thermo5_click.h"
#include "fan_click.h"

#include "sensor_task.h"
#include "time_utils.h"

#ifdef CONFIG_USE_JSON_LIB
#include "parson.h"
#endif

static uint16_t g_temp_speed_map[] = {
    0,      0,
    24000,  1000,
    24500,  1500,
    25000,  2000,
    25500,  2500,
    26000,  3000,
    27000,  4000
};

static uint16_t override_speed;
static uint32_t override_end;

static uint16_t get_speed_from_map(uint16_t temp)
{
    uint16_t i;
    uint16_t target;
    for(i=0; i < sizeof(g_temp_speed_map)/sizeof(g_temp_speed_map[0]); i += 2 )
    {
        if(temp > g_temp_speed_map[i])
        {
            target = g_temp_speed_map[i+1];
        }
    }
    return target;
}

#ifdef CONFIG_USE_JSON_LIB
bool update_settings_from_json(JSON_Array * json_map)
{
    JSON_Array * json_array_element = NULL;
    uint8_t i;

    for(i=0; i< json_array_get_count(json_map); i++)
    {
        json_array_element = json_array_get_array(json_map,i);
        g_temp_speed_map[i*2] = json_array_get_number(json_array_element, 0);
        g_temp_speed_map[i*2+1] = json_array_get_number(json_array_element, 1);
    }
    for(i=i*2;i < sizeof(g_temp_speed_map)/2; i+= 2)
    {
        g_temp_speed_map[i] = UINT16_MAX;
    }
}

bool override_from_json(JSON_Object * json_override_object)
{

    override_speed = json_object_get_number(json_override_object, "fan-speed");
    override_end = time_utils_get_utc() + json_object_get_number(json_override_object, "duration");
}

#endif

#define TEMP_SENSOR_SAMPLES    4
static uint16_t g_temp_buffer[TEMP_SENSOR_SAMPLES];
static uint8_t g_temp_buf_idx;

static uint16_t get_averaged_temp(void)
{
    uint32_t avg;
    uint8_t i;

    g_temp_buffer[g_temp_buf_idx++] = th5_read_sensor(0);
    if(TEMP_SENSOR_SAMPLES <= g_temp_buf_idx)
    {
        g_temp_buf_idx = 0;
    }
    avg = g_temp_buffer[0];
    for(i=1; i<TEMP_SENSOR_SAMPLES; i++ )
    {
        avg += g_temp_buffer[i];
    }
    
    return (avg / TEMP_SENSOR_SAMPLES);
}

static void write_map_to_buffer(uint8_t *buffer, size_t buflen)
{
    uint16_t i;
    size_t bytes_writen;
    
    if(!buffer || !buflen)
    {
        return;
    }

    buffer[0] = '[';
    buffer++;

    for(i=0; i < sizeof(g_temp_speed_map)/sizeof(g_temp_speed_map[0]) && 0 < buflen; i += 2)
    {
        bytes_writen = snprintf((char*)buffer, buflen, "[%d,%d],", g_temp_speed_map[i]/1000, g_temp_speed_map[i+1]);
        buffer += bytes_writen;
        buflen -= bytes_writen;
    }

    if(0 < buflen)
    {
        buffer--;
        *buffer = ']';
    }
}

uint32_t sensor_get_temperature(void)
{
#ifdef CONFIG_SENSOR_SIMULATOR
	if (24000 > g_temp_buffer[0] || 30000 < g_temp_buffer[0])
	{
		g_temp_buffer[0] = 24000;
	}
	else
	{
		g_temp_buffer[0] += 1000;
	}
	return g_temp_buffer[0];
#else
	return th5_read_sensor(0);
#endif
}

uint16_t sensor_get_fan_speed(void)
{
#ifdef CONFIG_SENSOR_SIMULATOR
	return get_speed_from_map(g_temp_buffer[0]);
#else
	return fan_click_get_tach();
#endif
}

 void sensor_task(void)
{
#ifndef CONFIG_SENSOR_SIMULATOR
    uint16_t temp;
    uint16_t speed;

    if(override_end <= time_utils_get_utc())
    {
        /* Get new sensor reading and target */
        temp = get_averaged_temp();
        speed = get_speed_from_map(temp);
    }
    else
    {
        speed = override_speed;
    }

    /* Set new target */
    fan_click_set_target_tach(speed);
#endif
}