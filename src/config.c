/**
 * \file
 * \brief  Example Configuration
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
#include "config.h"
#include "time_utils.h"
#include "jwt/atca_jwt.h"
#include "atca_kit_client.h"

#ifdef CONFIG_USE_STATIC_CONFIG

/* Example Configuration Data  Global Variables */
const char config_demo_ssid[] = "<EXAMPLE_SSID>";
const char config_demo_pass[] = "<EXAMPLE_PASS>";

/** Name of the mqtt service */
const char      config_gcp_host_name[] = "mqtt.googleapis.com";
/** Port of the mqtt service */
const uint32_t  config_gcp_mqtt_port = 443;

const char config_gcp_project_id[] = "<EXAMPLE_PROJECT_ID>";
const char config_gcp_region_id[] = "<EXAMPLE_REGION_ID>";
const char config_gcp_registry_id[] = "<EXAMPLE_REGISTRY_ID>";
const char config_gcp_thing_id[] = "<EXAMPLE_DEVICE_ID>";

#endif /* CONFIG_USE_STATIC_CONFIG */

void config_crypto(void)
{
	/* Configure the default I2C address of the device */
	cfg_ateccx08a_i2c_default.atcai2c.slave_address = 0xB0;

#if BOARD == SAMW25_XPLAINED_PRO
    /* For the ATSAMW25-XPRO Development Board */
    cfg_ateccx08a_i2c_default.atcai2c.bus = 0;
#elif BOARD == SAMG55_XPLAINED_PRO
    /* For the ATSAMG55-XPRO Development board */
    cfg_ateccx08a_i2c_default.atcai2c.bus = 1;
#endif

	/* Detect devices */
	(void)atca_kit_detect_I2c_devices();
}

bool config_ready(void)
{
    return true;
}

/** \brief Populate the buffer with the wifi access point ssid */
int config_get_ssid(char* buf, size_t buflen)
{
    int status = -1;

    if(buf && buflen)
    {
        /* Copy the demo ssid into the buffer */
        if(sizeof(config_demo_ssid) <= buflen)
        {
            memcpy(buf, config_demo_ssid, sizeof(config_demo_ssid));
            status = 0;
        }
    }

    return status;
}

/** \brief Populate the buffer with the wifi access point password */
int config_get_password(char* buf, size_t buflen)
{
    int status = -1;

    if(buf && buflen)
    {
        /* Copy the demo password into the buffer*/
        if(sizeof(config_demo_pass) <= buflen)
        {
            memcpy(buf, config_demo_pass, sizeof(config_demo_pass));
            status = 0;
        }
    }

    return status;
}

/** \brief Populate the buffer with the client id */
int config_get_client_id(char* buf, size_t buflen)
{
    if(buf && buflen)
    {
        int rv;

        rv = snprintf(buf, buflen, "projects/%s/locations/%s/registries/%s/devices/%s", 
            config_gcp_project_id, config_gcp_region_id, config_gcp_registry_id, config_gcp_thing_id);

        if(0 < rv && rv < buflen)
        {
            buf[rv] = 0;
            return 0;
        }
    }
    return -1;
}

/* Populate the buffer with the username */
int config_get_client_username(char* buf, size_t buflen)
{
    if(buf && buflen)
    {
        int rv = snprintf(buf, buflen, "unused");

        if(0 < rv && rv < buflen)
        {
            buf[rv] = 0;
            return 0;
        }
    }
    return -1;
}

/* Populate the buffer with the user's password */
int config_get_client_password(char* buf, size_t buflen)
{
    int rv = -1;

    if(buf && buflen)
    {
        atca_jwt_t jwt;
        
        uint32_t ts = time_utils_get_utc();

        rv = atcab_init(&cfg_ateccx08a_i2c_default);
        if(ATCA_SUCCESS != rv)
        {
            return rv;
        }

        /* Build the JWT */
        rv = atca_jwt_init(&jwt, buf, buflen);
        if(ATCA_SUCCESS != rv)
        {
            return rv;
        }

        if(ATCA_SUCCESS != (rv = atca_jwt_add_claim_numeric(&jwt, "iat", ts)))
        {
            return rv;
        }

        if(ATCA_SUCCESS != (rv = atca_jwt_add_claim_numeric(&jwt, "exp", ts + 86400)))
        {
            return rv;
        }

        if(ATCA_SUCCESS != (rv = atca_jwt_add_claim_string(&jwt, "aud", config_gcp_project_id)))
        {
            return rv;
        }

        rv = atca_jwt_finalize(&jwt, 0);

        atcab_release();
    }
    return rv;
}

/* Get the topic id  where the client will be publishing messages */
int config_get_client_pub_topic(char* buf, size_t buflen)
{
    if(buf && buflen)
    {
        int rv = snprintf(buf, buflen, "/devices/%s/events", config_gcp_thing_id);

        if(0 < rv && rv < buflen)
        {
            buf[rv] = 0;
            return 0;
        }
    }
    return -1;
}

/* Get the topic id  where the client will be publishing messages */
int config_get_client_sub_topic(char* buf, size_t buflen)
{
    if(buf && buflen)
    {
        int rv = snprintf(buf, buflen, "/devices/%s/config", config_gcp_thing_id);

        if(0 < rv && rv < buflen)
        {
            buf[rv] = 0;
            return 0;
        }
    }
    return -1;
}

/* Retrieve the host connection information */
int config_get_host_info(char* buf, size_t buflen, uint16_t * port)
{
    int rv;

    if(!buf || !buflen || !port)
    {
        return -1;
    }

    *port = config_gcp_mqtt_port;

    rv = snprintf(buf, buflen, "%s", config_gcp_host_name);

    if(0 < rv && rv < buflen)
    {
        buf[rv] = 0;
        return 0;
    }
    else
    {
        /* Failed */
        return -1;
    }
}


const uint8_t public_key_x509_header[] = { 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86,
                                           0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A,
                                           0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 
                                           0x42, 0x00, 0x04 };
int config_print_public_key(void)
{
    uint8_t buf[128];
    uint8_t * tmp;
    size_t buf_len = sizeof(buf);
    int i;

    ATCA_STATUS rv = atcab_init(&cfg_ateccx08a_i2c_default);
    if(ATCA_SUCCESS != rv)
    {
        return rv;
    }

    /* Calculate where the raw data will fit into the buffer */
    tmp = buf + sizeof(buf) - ATCA_PUB_KEY_SIZE - sizeof(public_key_x509_header);

    /* Copy the header */
    memcpy(tmp, public_key_x509_header, sizeof(public_key_x509_header));

    /* Get public key without private key generation */
    rv = atcab_get_pubkey(0, tmp + sizeof(public_key_x509_header));

    atcab_release();

    if (ATCA_SUCCESS != rv ) {
        return rv;
    }

    /* Convert to base 64 */
    rv = atcab_base64encode(tmp, ATCA_PUB_KEY_SIZE + sizeof(public_key_x509_header), buf, &buf_len);

    if(ATCA_SUCCESS != rv)
    {
        return rv;
    }

    /* Add a null terminator */
    buf[buf_len] = 0;

    /* Print out the key */
    DEBUG_PRINTF("-----BEGIN PUBLIC KEY-----\r\n%s\r\n-----END PUBLIC KEY-----\r\n", buf);

}