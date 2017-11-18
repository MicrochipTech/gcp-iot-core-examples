/**
 * \file
 * \brief  Client Application (MQTT) Task & Controller
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
#include "client_task.h"
#include "wifi_task.h"
#include "sensor_task.h"
#include "tiny_state_machine.h"
#include "time_utils.h"

#include "thermo5_click.h"
#include "fan_click.h"

#include "MQTTClient.h"

#ifdef CONFIG_USE_JSON_LIB
#include "parson.h"
#endif

#ifdef CONFIG_CLIENT_DEBUG
#define CLIENT_PRINTF(f, ...)  printf(f, ##__VA_ARGS__)
#else
#define CLIENT_PRINTF(...)  __NOP()
#endif

/* Global structures */
static struct _g_client_context {
    tiny_state_ctx      state;      /**< Must be the first element */
    uint32_t            holdoff;
    Network             mqtt_net;
    MQTTClient          mqtt_client;
    uint8_t             mqtt_rx_buf[CLIENT_MQTT_RX_BUF_SIZE];
    uint8_t             mqtt_tx_buf[CLIENT_MQTT_TX_BUF_SIZE];
    uint8_t             sub_topic[100];
    uint16_t            update_period;
} g_client_context;

/* Helper functions */
static bool client_counter_finished(void* pCtx)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;

    return (0 == ctx->holdoff);
}

static void client_counter_set(void* pCtx, uint32_t val)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;

    /* Convert to loop time*/
    ctx->holdoff = val / CLIENT_UPDATE_PERIOD;

    if(val && !ctx->holdoff)
    {
        ctx->holdoff = 1;
    }
}

/* Must be called on the CLIENT_UPDATE_PERIOD */
void client_timer_update(void)
{
    if(g_client_context.holdoff)
    {
        g_client_context.holdoff--;
    }
}

/** \brief Get a unique message id - Simply implemented as a counter */
static uint16_t client_get_message_id(void)
{
    static uint16_t message_id = 0;

    message_id++;
    
    if (message_id == (UINT16_MAX - 1))
    {
        message_id = 1;
    }

    return message_id;
}

/** \brief Publish a telemetry event */
static void client_publish_message(MQTTClient* mqtt_client)
{
    int status = MQTTCLIENT_FAILURE;
    MQTTMessage message;
    char json_message[256];
    uint32_t ts = time_utils_get_utc();
    uint32_t temp = th5_read_sensor(0);
    uint32_t speed = fan_click_get_tach();
    char topic[100];

    if(config_get_client_pub_topic(topic, sizeof(topic)))
    {
        CLIENT_PRINTF("Failed to get topic string");
        return;
    }

    snprintf(json_message, sizeof(json_message), "{ \"timestamp\": %u, \"temperature\": %d.%02d, \"fan-speed\": %d }", ts, temp/1000, temp % 1000,  speed);

    message.qos      = QOS1;
    message.retained = 0;
    message.dup      = 0;
    message.id       = client_get_message_id();
    
    message.payload = (void*)json_message;
    message.payloadlen = strlen(json_message);

    CLIENT_PRINTF("Publishing MQTT Message %s\r\n", json_message);

    status = MQTTPublish(mqtt_client, topic, &message);
    if (status != MQTTCLIENT_SUCCESS)
    {
        CLIENT_PRINTF("Failed to publish the MQTT message: %d\r\n", status);
    }
}

/** \brief Receive and process a message from the host */
static void client_process_message(MessageData *data)
{
#ifdef CONFIG_USE_JSON_LIB
    JSON_Value *json_message_value = NULL;
    JSON_Object *json_message_object = NULL;
    JSON_Object *json_override_object = NULL;
    JSON_Array * json_array_settings = NULL;
    uint32_t update_rate;

    json_message_value   = json_parse_string((char*)data->message->payload);
    json_message_object  = json_value_get_object(json_message_value);

    json_array_settings = json_object_get_array(json_message_object, "fan-speed-map");

    if(json_array_settings)
    {
        update_settings_from_json(json_array_settings);
    }

    json_override_object = json_object_get_object(json_message_object, "override");

    if(json_override_object)
    {
        override_from_json(json_override_object);
    }

    update_rate = json_object_get_number(json_message_object, "update-rate");

    if(1000 < update_rate)
    {
        g_client_context.update_period = update_rate;
    }
#endif
}

/************************************************************************/
/* STATE MACHINE PROCESSING                                             */
/************************************************************************/

/* Define the client states */
enum {
    CLIENT_STATE_INIT = 0,    /**< Should always start with 0 */
    CLIENT_STATE_GET_TIME,
    CLIENT_STATE_CONNECT,
    CLIENT_STATE_RUN,
    CLIENT_STATE_ERROR        /**< Error states can be anywhere but are recommended at the end */
} CLIENT_STATES;

static void client_state_update(void* pCtx, uint32_t next, uint32_t wait)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;

    CLIENT_PRINTF("%s(%u) -> %s(%u)\r\n", tiny_state_name(ctx, ctx->state.state),
    ctx->state.state, tiny_state_name(ctx, next), next);

    /* Set the new state */
    tiny_state_update(ctx, next);

    /* Set the holdoff/wait */
    client_counter_set(pCtx, wait);
}

/* Initialize the client */
static void client_state_init(void * pCtx)
{
    struct _g_client_context * ctx = (struct _g_client_context *)pCtx;
    
    /* Initialize the Paho MQTT Network Structure */
    ctx->mqtt_net.mqttread  = &mqtt_packet_read;
    ctx->mqtt_net.mqttwrite = &mqtt_packet_write;

    /* Initialize the Paho MQTT Client Structure */
    MQTTClientInit(&ctx->mqtt_client, &ctx->mqtt_net, CLIENT_MQTT_TIMEOUT_MS,
        ctx->mqtt_tx_buf, CLIENT_MQTT_TX_BUF_SIZE,
        ctx->mqtt_rx_buf, CLIENT_MQTT_RX_BUF_SIZE);

    ctx->update_period = CLIENT_REPORT_PERIOD_DEFAULT;

    /* Move to the next state */
    client_state_update(pCtx, CLIENT_STATE_GET_TIME, 0);
}

/* Check/Get Time */
static void client_state_get_time(void* pCtx)
{
    if(client_counter_finished(pCtx))
    {
        if(!time_utils_get_utc())
        {
            wifi_request_time();
            client_counter_set(pCtx, WIFI_COUNTER_GET_TIME_WAIT);
        }
        else
        {
            client_state_update(pCtx, CLIENT_STATE_CONNECT, 0);
        }
    }
}

static int client_connect_socket(void* pCtx)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;
    uint16_t port;

    if(config_get_host_info((char*)ctx->mqtt_rx_buf, CLIENT_MQTT_RX_BUF_SIZE, &port))
    {
        /* Failed */
        return -1;
    }

    if(wifi_connect((char*)ctx->mqtt_rx_buf, port))
    {
        /* Failed */
        return -1;
    }

    return 0;
}

/* Connect the MQTT Client to the host */
static int client_connect(void* pCtx)
{
    MQTTPacket_connectData mqtt_options = MQTTPacket_connectData_initializer;
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;
    size_t buf_bytes_remaining = CLIENT_MQTT_RX_BUF_SIZE;

    mqtt_options.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL_S;
    mqtt_options.cleansession = 1;

    /* Client ID String */
    mqtt_options.clientID.cstring = (char*)&ctx->mqtt_rx_buf[0];
    if(config_get_client_id(mqtt_options.clientID.cstring, buf_bytes_remaining))
    {
        return MQTTCLIENT_FAILURE;
    }

    /* Username String */
    mqtt_options.username.cstring = mqtt_options.clientID.cstring + strlen(mqtt_options.clientID.cstring) + 1;
    buf_bytes_remaining -= (mqtt_options.username.cstring - mqtt_options.clientID.cstring);
    if(config_get_client_username(mqtt_options.username.cstring, buf_bytes_remaining))
    {
        return MQTTCLIENT_FAILURE;
    }

    /* Password String */
    mqtt_options.password.cstring = mqtt_options.username.cstring + strlen(mqtt_options.username.cstring) + 1;
    buf_bytes_remaining -= (mqtt_options.password.cstring - mqtt_options.username.cstring);
    if(config_get_client_password(mqtt_options.password.cstring, buf_bytes_remaining))
    {
        return MQTTCLIENT_FAILURE;
    }

    return MQTTConnect(&ctx->mqtt_client, &mqtt_options);
}

/* Subscribe to a topic */
static int client_subscribe(void* pCtx)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;
    int status = MQTTCLIENT_FAILURE;

    status = config_get_client_sub_topic((char*)ctx->sub_topic, sizeof(ctx->sub_topic));
    if (status != MQTTCLIENT_SUCCESS)
    {
        CLIENT_PRINTF("Failed to load the subscription topic name");
        return status;
    }

    status = MQTTSubscribe(&ctx->mqtt_client, ctx->sub_topic, QOS1, &client_process_message);

    return status;
}

/* Connect to the host */
static void client_state_connect(void* pCtx)
{
    if(client_counter_finished(pCtx))
    {
        /* Update Hold-off */
        client_counter_set(pCtx, WIFI_COUNTER_GET_TIME_WAIT);

        /* Resolve the host and connect the socket */
        if(client_connect_socket(pCtx))
        {
            return;
        }

        /* Connect the MQTT client */
        if(client_connect(pCtx))
        {
            CLIENT_PRINTF("MQTT Client Failed to Connect\r\n");
            return;
        }
                        
        /* Connect the MQTT client */
        if(client_subscribe(pCtx))
        {
            CLIENT_PRINTF("MQTT Subscription Failed\r\n");
            return;
        }

        /* Move to the next state */
        client_state_update(pCtx, CLIENT_STATE_RUN, 0);
    }
}

/* Client is connected */
static void client_state_run(void * pCtx)
{
    struct _g_client_context* ctx = (struct _g_client_context*)pCtx;

    if(wifi_has_error())
    {
        client_state_update(pCtx, CLIENT_STATE_INIT, 0);
    }

    if(client_counter_finished(pCtx))
    {
        client_counter_set(pCtx, ctx->update_period);

        client_publish_message(&ctx->mqtt_client);
    }
    else
    {
        /* Wait for incoming update messages */
        if(MQTTYield(&ctx->mqtt_client, MQTT_YEILD_TIMEOUT_MS))
        {
            /* Failure */
        }
   }
}

/* Wait for the client to connect successfully */
static void client_state_error(void * pCtx)
{

    /* Move to the next state */
    client_state_update(pCtx, CLIENT_STATE_INIT, 0);
}

/* Define the state machine - must be indexed by the defined state */
tiny_state_def g_client_states[] =
{
    TINY_STATE_DEF(CLIENT_STATE_INIT,       &client_state_init),
    TINY_STATE_DEF(CLIENT_STATE_GET_TIME,   &client_state_get_time),
    TINY_STATE_DEF(CLIENT_STATE_CONNECT,    &client_state_connect),
    TINY_STATE_DEF(CLIENT_STATE_RUN,        &client_state_run),
    TINY_STATE_DEF(CLIENT_STATE_ERROR,      &client_state_error),
};

/* Assume a 100ms time basis */
void client_task(void)
{
    if(!g_client_context.state.count)
    {
	    /* Perform the Initialization */
	    tiny_state_init(&g_client_context, g_client_states, sizeof(g_client_states)/sizeof(g_client_states[0]), CLIENT_STATE_INIT);
    }

    /* Run the state machine*/
    tiny_state_driver(&g_client_context);
}
