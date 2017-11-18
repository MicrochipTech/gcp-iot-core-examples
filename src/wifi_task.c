/**
 * \file
 * \brief  WIFI Management and Abstraction (WINC1500)
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
#include "tiny_state_machine.h"
#include "config.h"
#include "cryptoauthlib.h"
#include "time_utils.h"
#include "wifi_task.h"
#include "client_task.h"
#include "MQTTClient.h"

/* Include WINC1500 driver */
#include "driver/include/m2m_wifi.h"
#include "driver/include/m2m_periph.h"
#include "driver/include/m2m_ssl.h"
#include "driver/include/m2m_types.h"

#ifdef CONFIG_WIFI_DEBUG
#define WIFI_PRINTF(f, ...)  printf(f, ##__VA_ARGS__)
#else
#define WIFI_PRINTF(...)  __NOP()
#endif

/* Global structures */
static struct _g_wifi_context {
    tiny_state_ctx      state;      /**< Must be the first element */
	uint32_t            holdoff;
    uint32_t            host;
    SOCKET              sock;
    uint8_t             rxbuf[WIFI_BUFFER_SIZE];
    uint32_t            rxloc;
    uint32_t            txlen;
} g_wifi_context;

/* Check if the timeout has elapsed */
static inline bool wifi_counter_finished(void)
{
    return (0 == g_wifi_context.holdoff);
}

/* Set timeout in milliseconds */
static void wifi_counter_set(uint32_t val)
{
    /* Convert to loop time*/
    g_wifi_context.holdoff = val / WIFI_UPDATE_PERIOD;

    if(val && !g_wifi_context.holdoff)
    {
        g_wifi_context.holdoff = 1;
    }
}

/* Must be called on the WIFI_UPDATE_PERIOD */
void wifi_timer_update(void)
{
    if(g_wifi_context.holdoff)
    {
        g_wifi_context.holdoff--;
    }
}

/* Define the WIFI states */
enum {
    WIFI_STATE_INIT = 0,    /**< Should always start with 0 */
    WIFI_STATE_TLS_INIT,
    WIFI_STATE_CONNECT,
    WIFI_STATE_WAIT,
    WIFI_STATE_READY,
    WIFI_STATE_TIMEOUT,
    WIFI_STATE_ERROR        /**< Error states can be anywhere but are recommended at the end */
} WIFI_STATES;

int wifi_is_ready(void)
{
    return (WIFI_STATE_READY == g_wifi_context.state.state);
}

int wifi_is_busy(void)
{
    return (WIFI_STATE_WAIT == g_wifi_context.state.state);
}

int wifi_has_error(void)
{
    return (WIFI_STATE_ERROR == g_wifi_context.state.state);
}

static void wifi_state_update(void* ctx, uint32_t next, uint32_t wait)
{
    struct _g_wifi_context * pCtx = ctx;

    WIFI_PRINTF("%s(%u) -> %s(%u)\r\n", tiny_state_name(ctx, pCtx->state.state), 
        pCtx->state.state, tiny_state_name(ctx, next), next);

    /* Set the new state */
    tiny_state_update(ctx, next);

    /* Set the holdoff/wait */
    wifi_counter_set(wait);
}

static sint8 wifi_print_winc_version(void)
{
    sint8       status;
    tstrM2mRev  wifi_version;

    // Get the WINC1500 WIFI module firmware version information
    status = m2m_wifi_get_firmware_version(&wifi_version);
    if(M2M_SUCCESS == status)
    {
        WIFI_PRINTF("WINC1500: Chip ID: 0x%08lX\r\n", wifi_version.u32Chipid);
        WIFI_PRINTF("WINC1500: Firmware Version: %u.%u.%u\r\n",
            wifi_version.u8FirmwareMajor, wifi_version.u8FirmwareMinor,
            wifi_version.u8FirmwarePatch);
        WIFI_PRINTF("WINC1500: Firmware Min Driver Version: %u.%u.%u\r\n",
            wifi_version.u8DriverMajor, wifi_version.u8DriverMinor,
            wifi_version.u8DriverPatch);
        WIFI_PRINTF("WINC1500: Driver Version: %d.%d.%d\r\n",
            M2M_RELEASE_VERSION_MAJOR_NO, M2M_RELEASE_VERSION_MINOR_NO,
            M2M_RELEASE_VERSION_PATCH_NO);
    }
    return status;
}

static void wifi_socket_handler_cb(SOCKET sock, uint8 u8Msg, void * pvMsg)
{
    tstrSocketConnectMsg *socket_connect_message = NULL;
    tstrSocketRecvMsg *socket_receive_message = NULL;
    sint16 *bytes_sent = NULL;
    
    // Check for the WINC1500 WIFI socket events
    switch (u8Msg)
    {
        case SOCKET_MSG_CONNECT:
        socket_connect_message = (tstrSocketConnectMsg*)pvMsg;
        if (socket_connect_message != NULL)
        {
            if (socket_connect_message->s8Error != SOCK_ERR_NO_ERROR)
            {
                wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
            }
            else
            {
                wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
            }
        }
        break;
        
        case SOCKET_MSG_RECV:
        case SOCKET_MSG_RECVFROM:
        socket_receive_message = (tstrSocketRecvMsg*)pvMsg;
        if (socket_receive_message != NULL)
        {
            if (socket_receive_message->s16BufferSize >= 0)
            {
                /* The message was received */
                if (socket_receive_message->u16RemainingSize == 0)
                {
                    wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
                }
            }
            else
            {
                if (socket_receive_message->s16BufferSize == SOCK_ERR_TIMEOUT)
                {
                    /* A timeout has occurred */
                    wifi_state_update(&g_wifi_context, WIFI_STATE_TIMEOUT, WIFI_COUNTER_NO_WAIT);
                }
                else
                {
                    /* An error has occurred */
                    wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
                }
            }
        }
        break;
        
        case SOCKET_MSG_SEND:
        bytes_sent = (sint16*)pvMsg;
        
        if (*bytes_sent <= 0 || *bytes_sent > (int32_t)g_wifi_context.txlen)
        {
            // Seen an odd instance where bytes_sent is way more than the requested bytes sent.
            // This happens when we're expecting an error, so were assuming this is an error
            // condition.

            wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
        }
        else if (*bytes_sent == g_wifi_context.txlen)
        {
            /* The message was sent */
            wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
        }
        break;

        default:
        // Do nothing
        break;
    }
}

static void wifi_resolve_handler_cb(uint8* pu8DomainName, uint32 u32ServerIP)
{
    if (u32ServerIP != 0)
    {
        g_wifi_context.host = u32ServerIP;

        /* Return to ready state */
        wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
    }
    else
    {
        /* Failed to Resolve */
        wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
    }
}

static void wifi_tls_handler_cb(uint8 u8MsgType, void * pvMsg)
{

}

static void wifi_app_cb_process_connection(void *pvMsg)
{
    tstrM2mWifiStateChanged* msg = (tstrM2mWifiStateChanged*)pvMsg;
    
    if(M2M_WIFI_CONNECTED == msg->u8CurrState)
    {
        m2m_wifi_enable_sntp(1);
        WIFI_PRINTF("WINC1500 WIFI: Connected to the WIFI access point\r\n");
    }
    else if(M2M_WIFI_DISCONNECTED == msg->u8CurrState)
    {
        WIFI_PRINTF("WINC1500 WIFI: Disconnected from the WIFI access point\r\n");

        wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
    }
    else
    {
        WIFI_PRINTF("WINC1500 WIFI: Unknown connection status: %d\r\n", msg->u8ErrCode);
    }
}

static void wifi_app_cb_process_dhcp(void *pvMsg)
{
    tstrM2MIPConfig* ip_config = (tstrM2MIPConfig*)pvMsg;
    uint8_t * ip_address = (uint8_t*)&ip_config->u32StaticIP;
    
    WIFI_PRINTF("WINC1500 WIFI: Device IP Address: %u.%u.%u.%u\r\n",
        ip_address[0], ip_address[1], ip_address[3], ip_address[4]);

    /* Transition to ready state */
    wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
}

static void wifi_app_cb_process_time(void *pvMsg)
{
    tstrSystemTime * msg = (tstrSystemTime *)pvMsg;

    WIFI_PRINTF("WINC1500 WIFI: Device Time:       %02d/%02d/%02d %02d:%02d:%02d\r\n",
        msg->u16Year, msg->u8Month, msg->u8Day,
        msg->u8Hour, msg->u8Minute, msg->u8Second);

    if(msg->u16Year && msg->u8Month && msg->u8Day)
    {
        /* Check if it has already been set before */
        if(!time_utils_get_utc())
        {
            time_utils_set(msg->u16Year, msg->u8Month, msg->u8Day,
            msg->u8Hour, msg->u8Minute, msg->u8Second);
        }
    }

    /* Return to ready state */
    wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
}

struct _wifi_app_cb {
    uint8 _id;
    void (*_func)(void* msg);
};

static struct _wifi_app_cb wifi_app_cb_list[] = {
	{ M2M_WIFI_RESP_CON_STATE_CHANGED, wifi_app_cb_process_connection },
	//{ M2M_WIFI_RESP_CONN_INFO, NULL},
    { M2M_WIFI_REQ_DHCP_CONF, wifi_app_cb_process_dhcp},
	//{ M2M_WIFI_REQ_WPS, NULL},
	//{ M2M_WIFI_RESP_IP_CONFLICT, NULL},
	//{ M2M_WIFI_RESP_SCAN_DONE, NULL},
	//{ M2M_WIFI_RESP_SCAN_RESULT, NULL},
	//{ M2M_WIFI_RESP_CURRENT_RSSI, NULL},
	//{ M2M_WIFI_RESP_CLIENT_INFO, NULL},
	//{ M2M_WIFI_RESP_PROVISION_INFO, NULL},
	//{ M2M_WIFI_RESP_DEFAULT_CONNECT, NULL},
    { M2M_WIFI_RESP_GET_SYS_TIME, wifi_app_cb_process_time},
	
	/* In case Ethernet/Bypass mode is defined */
	//{ M2M_WIFI_RESP_ETHERNET_RX_PACKET, NULL},
	
	/* In case monitoring mode is used */
	//{ M2M_WIFI_RESP_WIFI_RX_PACKET, NULL},
};

/* WIFI's main callback function handler, for handling the M2M_WIFI events 
   received on the WIFI interface. Such notifications are received in response 
   to WIFI/P2P operations */
static void wifi_app_cb(uint8 u8MsgType, void * pvMsg)
{
    uint8_t i;
    for(i=0; i<sizeof(wifi_app_cb_list)/sizeof(wifi_app_cb_list[0]); i++)
    {
        if(u8MsgType == wifi_app_cb_list[i]._id)
        {
            if(wifi_app_cb_list[i]._func)
            {
                wifi_app_cb_list[i]._func(pvMsg);
            }
            break;
        }
    }
}

/* Handle the WIFI initialization state */
static void wifi_state_init(void * ctx)
{
    tstrWifiInitParam wifi_paramaters;

    /* Wait until the device has been "provisioned" or is configured to run */
    if(!config_ready())
    {
        return;
    }

    /* Perform the BSP Initialization for the WIFI module */
    nm_bsp_init();

    /* Set the WIFI configuration attributes */
    m2m_memset((uint8*)&wifi_paramaters, 0, sizeof(wifi_paramaters));
    wifi_paramaters.pfAppWifiCb = wifi_app_cb;

    /* Initialize the WINC1500 WIFI module */
    if(M2M_SUCCESS != m2m_wifi_init(&wifi_paramaters))
    {
        WIFI_PRINTF("m2m_wifi_init failed\n");
        return;
    }

    /* Configure the WIFI IO lines */
    m2m_periph_pullup_ctrl( M2M_PERIPH_PULLUP_DIS_HOST_WAKEUP |
                            M2M_PERIPH_PULLUP_DIS_SD_CMD_SPI_SCK |
                            M2M_PERIPH_PULLUP_DIS_SD_DAT0_SPI_TXD, false);

    /* Initialize the WINC1500 WIFI socket handler */
    socketInit();

    /* Register the WIFI socket callbacks */
    registerSocketCallback(wifi_socket_handler_cb, wifi_resolve_handler_cb);

    /* Print the current WINC software */
    if(M2M_SUCCESS != wifi_print_winc_version())
    {
        WIFI_PRINTF("Failed to retrieve WINC firmware version\r\n");
        return;
    }

    /* Move to the next state */
    wifi_state_update(ctx, WIFI_STATE_TLS_INIT, WIFI_COUNTER_NO_WAIT);
}

/* Handle the TLS setup state */
static void wifi_state_tls_init(void * ctx)
{
    // Initialize the WINC1500 SSL module
    if(M2M_SUCCESS != m2m_ssl_init(wifi_tls_handler_cb))
    {
        WIFI_PRINTF("m2m_ssl_init failed\r\n");
        return;
    }

    // Set the active WINC1500 TLS cipher suites
    //         wifi_status = m2m_ssl_set_active_ciphersuites(SSL_ECC_ONLY_CIPHERS);
    if(M2M_SUCCESS != m2m_ssl_set_active_ciphersuites(SSL_ENABLE_ALL_SUITES))
    {
        WIFI_PRINTF("m2m_ssl_set_active_ciphersuites failed\r\n");
        return;
    }

    /* Move to the next state */
    wifi_state_update(ctx, WIFI_STATE_CONNECT, WIFI_COUNTER_NO_WAIT);
}

/* Initialize the network connection */
static void wifi_state_connect(void * ctx)
{
    char ssid[WIFI_MAX_SSID_SIZE];
    char pass[WIFI_MAX_PASS_SIZE];
    int32_t status;

    do
    {
        /* Get the WIFI SSID */
        status = config_get_ssid(ssid, sizeof(ssid));
        if(status)
        {
            WIFI_PRINTF("Failed to get SSID: %d\r\n", status);
            break;
        }

        /* Get the WIFI Password */
        status = config_get_password(pass, sizeof(pass));
        if(status)
        {
            break;
        }

        /* Check the password length returned. If it's non zero then use it - if not assume an open AP */
        if (strlen(pass) > 0)
        {
            status = m2m_wifi_connect(ssid, strlen(ssid), M2M_WIFI_SEC_WPA_PSK, pass, M2M_WIFI_CH_ALL);
        }
        else
        {
            status = m2m_wifi_connect(ssid, strlen(ssid), M2M_WIFI_SEC_OPEN, pass, M2M_WIFI_CH_ALL);
        }
    } while(false);

    if (M2M_SUCCESS == status)
    {
        /* Move to the next state */
        wifi_state_update(ctx, WIFI_STATE_WAIT, WIFI_COUNTER_CONNECT_WAIT);
    }
    else
    {
        /* Go to the error state */
        wifi_state_update(ctx, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
    }
}

/* Wait last command to finish */
static void wifi_state_wait(void * ctx)
{
    if(wifi_counter_finished())
    {
        /* Command failed to finish in the given timeout */
        wifi_state_update(ctx, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
    }
}

/* Idle state - Run when no commands are pending  */
static void wifi_state_ready(void * ctx)
{
    /* Do Nothing */
}

/* Last command timed out */
static void wifi_state_timeout(void * ctx)
{
    /* Do Nothing */
}

/* Handle the generic error state */
static void wifi_state_error(void * ctx)
{
    if(wifi_counter_finished())
    {
        WIFI_PRINTF("Retrying Connection\r\n");
        wifi_state_update(ctx, WIFI_STATE_CONNECT, WIFI_COUNTER_NO_WAIT);
    }
}

/* Define the state machine - must be indexed by the defined state */
tiny_state_def g_wifi_states[] =
{
    TINY_STATE_DEF(WIFI_STATE_INIT,             &wifi_state_init),
    TINY_STATE_DEF(WIFI_STATE_TLS_INIT,         &wifi_state_tls_init),
    TINY_STATE_DEF(WIFI_STATE_CONNECT,          &wifi_state_connect),
    TINY_STATE_DEF(WIFI_STATE_WAIT,             &wifi_state_wait),
    TINY_STATE_DEF(WIFI_STATE_READY,            &wifi_state_ready),
    TINY_STATE_DEF(WIFI_STATE_TIMEOUT,          &wifi_state_timeout),
    TINY_STATE_DEF(WIFI_STATE_ERROR,            &wifi_state_error)
};

/* WIFI State Controller */
void wifi_task(void)
{
    if(!g_wifi_context.state.count)
    {
        /* Perform the Initialization */
        tiny_state_init(&g_wifi_context, g_wifi_states, sizeof(g_wifi_states)/sizeof(g_wifi_states[0]), WIFI_STATE_INIT);
    }

    /* Run the state machine*/
    tiny_state_driver(&g_wifi_context);

    /* Handle WINC1500 pending events */
    m2m_wifi_handle_events(NULL);
}

/* Used internally for blocking calls */
static inline void wifi_task_block_until_done(void)
{
    do
    {
        wifi_task();
    } while (wifi_is_busy());
}

/* Request Time from NTP servers and update clock */
void wifi_request_time(void)
{
    if(wifi_is_ready())
    {
        m2m_wifi_get_sytem_time();
        wifi_state_update(&g_wifi_context, WIFI_STATE_WAIT, WIFI_COUNTER_GET_TIME_WAIT);
    }
}

/* Request the winc to resolve a name */
static void wifi_resolve_host(char * host)
{
    g_wifi_context.host = 0;

    if(MQTTCLIENT_SUCCESS == gethostbyname((uint8*)host))
    {
        wifi_state_update(&g_wifi_context, WIFI_STATE_WAIT, WIFI_COUNTER_GET_TIME_WAIT);
    }
    else
    {
        wifi_state_update(&g_wifi_context, WIFI_STATE_ERROR, WIFI_COUNTER_RECONNECT_WAIT);
    }
}

/* Connect to a host and create a socket - blocking call */
int wifi_connect(char * host, int port)
{
    int status = MQTTCLIENT_FAILURE;
    SOCKET new_socket = SOCK_ERR_INVALID;
    struct sockaddr_in socket_address;
    int optval;

    if(!wifi_is_ready())
    {
        return status;
    }

    /* Send the resolve command */
    wifi_resolve_host(host);

    /* Wait for the command to complete or timeout */
    wifi_task_block_until_done();

    /* Check for failures */
    if(!wifi_is_ready())
    {
        return status;
    }

    // Create the socket
    new_socket = socket(AF_INET, SOCK_STREAM, 1);
    if (new_socket < 0)
    {
        /* Failed to create the socket */
        return status;
    }
        
    /* Set the socket information */
    socket_address.sin_family      = AF_INET;
    socket_address.sin_addr.s_addr = g_wifi_context.host;
    socket_address.sin_port        = _htons(port);

    optval = 1;
    setsockopt(new_socket, SOL_SSL_SOCKET, SO_SSL_ENABLE_SESSION_CACHING,
        &optval, sizeof(optval));
    
#if 0
    /* For testing if the correct root certificates have not been loaded into the WINC */
    optval = 1;
    setsockopt(new_socket, SOL_SSL_SOCKET, SO_SSL_BYPASS_X509_VERIF,
        &optval, sizeof(optval));
#endif

    /* Connect to the specified host */
    status = connect(new_socket, (struct sockaddr*)&socket_address,
        sizeof(socket_address));
    if (status != SOCK_ERR_NO_ERROR)
    {
        /* Close the socket */
        close(new_socket);
        return status;
    }

    /* */
    wifi_state_update(&g_wifi_context, WIFI_STATE_WAIT, WIFI_COUNTER_CONNECT_WAIT);

    /* Wait for the command to complete or timeout */
    wifi_task_block_until_done();

    /* Check for failures */
    if(!wifi_is_ready())
    {
        /* Close the socket */
        close(new_socket);
        return MQTTCLIENT_FAILURE;
    }

    /* Save the socket for use */
    g_wifi_context.sock = new_socket;

    return MQTTCLIENT_SUCCESS;
}

/* Read data from a socket - blocking call */
int wifi_read_data(uint8_t *read_buffer, uint32_t read_length, uint32_t timeout_ms)
{
    int status = MQTTCLIENT_FAILURE;
    
    if(!wifi_is_ready())
    {
        return status;
    }

    if ((WIFI_BUFFER_SIZE - g_wifi_context.rxloc) >= read_length)
    {
        status = MQTTCLIENT_SUCCESS;

        /* Get the data from the existing received buffer */
        memcpy(&read_buffer[0], &g_wifi_context.rxbuf[g_wifi_context.rxloc], read_length);

        g_wifi_context.rxloc += read_length;
    }
    else
    {
        g_wifi_context.rxloc = 0;
        memset(&g_wifi_context.rxbuf[0], 0, WIFI_BUFFER_SIZE);

        /* Receive the incoming message */
        if(MQTTCLIENT_SUCCESS != (status = recv(g_wifi_context.sock, g_wifi_context.rxbuf, WIFI_BUFFER_SIZE, timeout_ms)))
        {
            return status;
        }

        wifi_state_update(&g_wifi_context, WIFI_STATE_WAIT, timeout_ms + WIFI_UPDATE_PERIOD);

        /* Wait for the command to complete or timeout */
        wifi_task_block_until_done();

        /* Check for failures */
        if(!wifi_is_ready())
        {
            status = MQTTCLIENT_FAILURE;
            if(!wifi_has_error())
            {
                /* Timed out but we aren't going to retry */
                wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
            }
        }
        else
        {
            status = MQTTCLIENT_SUCCESS;
            memcpy(&read_buffer[0], &g_wifi_context.rxbuf[0], read_length);
            g_wifi_context.rxloc += read_length;
        }
    }
    
    return ((status == MQTTCLIENT_SUCCESS) ? (int)read_length : status);
}

/* Send data to a socket - blocking call */
int wifi_send_data(uint8_t *send_buffer, uint32_t send_length, uint32_t timeout_ms)
{
    int status = MQTTCLIENT_FAILURE;
    
    if(!wifi_is_ready())
    {
        return status;
    }

    status = send(g_wifi_context.sock, send_buffer, send_length, 0);
    g_wifi_context.txlen = send_length;

    wifi_state_update(&g_wifi_context, WIFI_STATE_WAIT, WIFI_COUNTER_GET_TIME_WAIT);

    /* Wait for the command to complete or timeout */
    wifi_task_block_until_done();

    /* Check for failures */
    if(!wifi_is_ready())
    {
        status = MQTTCLIENT_FAILURE;
        if(!wifi_has_error())
        {
            /* Timed out but we aren't going to retry */
            wifi_state_update(&g_wifi_context, WIFI_STATE_READY, WIFI_COUNTER_NO_WAIT);
        }
    }
    
    return ((status == MQTTCLIENT_SUCCESS) ? (int)send_length : status);
}
