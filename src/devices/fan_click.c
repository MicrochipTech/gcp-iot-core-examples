/**
 * \file
 * \brief  Fan Click Driver (EMC2301)
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
#include "fan_click.h"

uint8_t g_fan_click_ready = false;
uint8_t g_fan_click_cfg1_cached;

#if SAM0

#include "cryptoauthlib.h"
#include "hal/hal_samd21_i2c_asf.h"
extern ATCAI2CMaster_t *i2c_hal_data[MAX_I2C_BUSES];

static uint8_t fan_read_reg(uint8_t reg)
{
    struct i2c_master_packet packet = {
        .address            = EMC2301_I2C_ADDR,
        .data_length        = sizeof(reg),
        .data               = &reg,
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    /* Write Register Address */
    i2c_master_write_packet_wait_no_stop(&(i2c_hal_data[cfg_ateccx08a_i2c_default.atcai2c.bus]->i2c_master_instance), &packet);

    /* Read data from Register */
    i2c_master_read_packet_wait(&(i2c_hal_data[cfg_ateccx08a_i2c_default.atcai2c.bus]->i2c_master_instance), &packet);

    return reg;
}

static void fan_write_reg(uint8_t reg, uint8_t txdata)
{
    uint8_t buf[2];


    struct i2c_master_packet packet = {
        .address            = EMC2301_I2C_ADDR,
        .data_length        = sizeof(buf),
        .data               = buf,
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    buf[0] = reg;
    buf[1] = txdata;

    /* Write Data to Register */
    i2c_master_write_packet_wait(&(i2c_hal_data[cfg_ateccx08a_i2c_default.atcai2c.bus]->i2c_master_instance), &packet);
}

#elif SAM

static uint8_t fan_read_reg(uint8_t reg)
{
    uint8_t rxdata;
    twi_packet_t packet = {
        .chip        = EMC2301_I2C_ADDR,
        .addr        = { reg },
        .addr_length = 1,
        .buffer      = &rxdata,
        .length      = sizeof(rxdata)
    };

    twi_master_read(TWI4, &packet);

    return rxdata;
}

static void fan_write_reg(uint8_t reg, uint8_t txdata)
{
    twi_packet_t packet = {
        .chip        = EMC2301_I2C_ADDR,
        .addr        = { reg },
        .addr_length = 1,
        .buffer      = &txdata,
        .length      = sizeof(txdata)
    };

    twi_master_write(TWI4, &packet);
}
#endif


int fan_click_init( void )
{
    uint8_t pid = fan_read_reg(EMC2301_PRODUCT_ID);
    uint8_t mid = fan_read_reg(EMC2301_MANUFACTUERE_ID);

    if(0x37 != pid || 0x5D != mid)
    {
        return -1;
    }

    /* To enable the FSC Algorithm
        1. Set the Spin Up Configuration Register to the Spin Up Level and Spin Time desired.
        2. Set the Fan Step Register to the desired step size.
        3. Set the Fan Minimum Drive Register to the minimum drive value that will maintain fan operation.
        4. Set the Update Time and Edges options in the Fan Configuration Register.
        5. Set the Valid TACH Count Register to the highest tach count that indicates the fan is spinning.
            Refer to AN17.4 RPM to TACH Counts Conversion for examples and tables for supported RPM
            ranges (500, 1k, 2k, 4k).
        6. Set the TACH Target Register to the desired tachometer count.
        7. Enable the RPM-based Fan Speed Control Algorithm by setting the EN_ALGO bit. */

    //fan_write_reg(EMC2301_FAN_SETTING, 0xFF);
    fan_write_reg(EMC2301_FAN_CONFIG1, 0xAE);

    fan_write_reg(EMC2301_FAN_MIN_DRIVE, 0x35);

    fan_write_reg(EMC2301_FAN_VALID_TACH, 0xF6);

    fan_write_reg(EMC2301_FAN_SPINUP, 0xE8);

    fan_write_reg(EMC2301_PWM_POLARITY, 0x00);

    g_fan_click_cfg1_cached = fan_read_reg(EMC2301_FAN_CONFIG1);

    g_fan_click_ready = true;
    return 0;
}

void fan_click_config_enableFSC( bool fsc )
{
    if(g_fan_click_ready)
    {
        uint8_t fan_config = fan_read_reg(EMC2301_FAN_CONFIG1);

        fan_config &= 0x7F;
        fan_config |= fsc << 7;

        fan_write_reg(EMC2301_FAN_CONFIG1, fan_config);
    }
}

static uint16_t fan_click_count_to_rpm(uint32_t count)
{
    /* To convert from count to rpm where:
        p = # of poles of the motor
        n = # of edges observed per rotation
        m = multiplier defined by the range bits
        f = measurement frequency
        
        RPM = (n - 1) * m * f * 60 / (p * c)
    */

    /* Load the constants */
    uint32_t rpm = EMC2301_CLOCK_FREQ_HZ * 60 / EMC2301_FAN_POLES;

    /* Edges */
    rpm *= 2 * (1 + EMC2301_FAN_CONFIG1_EDGES_VALUE(g_fan_click_cfg1_cached));
    
    /* Multiplier */
    rpm *= 1 << EMC2301_FAN_CONFIG1_RANGE_VALUE(g_fan_click_cfg1_cached);

    /* Convert */
    rpm /= count;

    return (uint16_t)rpm;
}

static uint16_t fan_click_rpm_to_count(uint32_t rpm)
{
    /* To convert from count to rpm where:
        p = # of poles of the motor
        n = # of edges observed per rotation
        m = multiplier defined by the range bits
        f = measurement frequency
        
        RPM = (n - 1) * m * f * 60 / (p * c)
    */
    /* Load the constants */
    uint32_t count = EMC2301_CLOCK_FREQ_HZ * 60 / EMC2301_FAN_POLES;

    /* Edges */
    count *= 2 * (1 + EMC2301_FAN_CONFIG1_EDGES_VALUE(g_fan_click_cfg1_cached));
    
    /* Multiplier */
    count *= 1 << EMC2301_FAN_CONFIG1_RANGE_VALUE(g_fan_click_cfg1_cached);

    /* Convert */
    count /= rpm;

    return (uint16_t)count;
}

void fan_click_set_target_tach( uint16_t tach )
{
#if !SAMG
    ATCA_STATUS status = atcab_init(&cfg_ateccx08a_i2c_default);

    if(ATCA_SUCCESS != status)
    {
        return;
    }
#endif

    if(g_fan_click_ready)
    {
        if(0 == tach)
        {
            fan_write_reg(EMC2301_TACH_TARGET_LOW, 0xFF);
            fan_write_reg(EMC2301_TACH_TARGET_HIGH, 0xFF);
        }
        else
        {
            tach = fan_click_rpm_to_count(tach);

            fan_write_reg(EMC2301_TACH_TARGET_LOW, tach << 3);
            fan_write_reg(EMC2301_TACH_TARGET_HIGH, tach >> 5);
        }
    }
    else
    {
        fan_click_init();
    }

#if !SAMG
    atcab_release();
#endif
}

uint16_t fan_click_get_tach( void )
{
    uint16_t ret = UINT16_MAX;

#if !SAMG
    ATCA_STATUS status = atcab_init(&cfg_ateccx08a_i2c_default);

    if(ATCA_SUCCESS != status)
    {
        return ret;
    }
#endif

    if(g_fan_click_ready)
    {
        /* Check if the fan is spinning first */
        ret = fan_read_reg(EMC2301_FAN_SETTING);

        if (ret)
        {
            uint32_t speed;

            speed = fan_read_reg(EMC2301_TACH_READING_HIGH) << 5;
            speed |= fan_read_reg(EMC2301_TACH_READING_LOW) >> 3;

            ret = fan_click_count_to_rpm(speed);
        }
    }
    else
    {
        fan_click_init();
    }

#if !SAMG
    atcab_release();
#endif
    return ret;
}