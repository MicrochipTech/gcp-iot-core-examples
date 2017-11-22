/**
 * \file
 * \brief  Thermo 5 Click Driver (EMC1414)
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

#define THERMO5_ADDR   0x4C

#define INT_DIODE      0x0029
#define EXT_DIODE1     0x0110
#define EXT_DIODE2     0x2324
#define EXT_DIODE3     0x2A2B

#if SAM0

#include "cryptoauthlib.h"
#include "hal/hal_samd21_i2c_asf.h"
extern ATCAI2CMaster_t *i2c_hal_data[MAX_I2C_BUSES];

static uint8_t th5_read_reg(uint8_t reg)
{
    struct i2c_master_packet packet = {
        .address            = THERMO5_ADDR,
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

#elif SAM
static uint8_t th5_read_reg(uint8_t reg)
{
    uint8_t rxdata;
    twi_packet_t packet = {
        .chip        = THERMO5_ADDR,
        .addr        = { reg },
        .addr_length = 1,
        .buffer      = &rxdata,
        .length      = sizeof(rxdata)
    };

    twi_master_read(TWI4, &packet);

    return rxdata;
}
#endif

static const uint16_t diode_sensors[4] = {
    INT_DIODE,
    EXT_DIODE1,
    EXT_DIODE2,
    EXT_DIODE3
};

uint32_t th5_read_sensor(uint8_t sensor)
{
    uint16_t temp_raw;
    uint32_t temp = UINT32_MAX;

#if !SAMG
    ATCA_STATUS status = atcab_init(&cfg_ateccx08a_i2c_default);
    if(ATCA_SUCCESS != status)
    {
        return temp;
    }
#endif

    if(sensor < 4)
    {
        temp_raw = th5_read_reg(diode_sensors[sensor] & 0xFF);
        temp_raw |= th5_read_reg(diode_sensors[sensor] >> 8) << 8;

        temp = (temp_raw >> 5);
        temp *= 125;
    }

#if !SAMG
    atcab_release();
#endif

    return temp;
}
