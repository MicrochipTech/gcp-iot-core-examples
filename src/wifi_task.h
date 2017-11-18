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

#ifndef WIFI_TASK_H_
#define WIFI_TASK_H_

/* WIFI Control Configuration */

/** Maximum Allowed SSID Length */
#define WIFI_MAX_SSID_SIZE  32

/** Maximum Allowed Password Length */
#define WIFI_MAX_PASS_SIZE  64

/** Maximum Static RX buffer to use */
#define WIFI_BUFFER_SIZE    (1500)

/* Wait times are specified in milliseconds */
#define WIFI_COUNTER_NO_WAIT            0
#define WIFI_COUNTER_GET_TIME_WAIT      10000
#define WIFI_COUNTER_CONNECT_WAIT       10000
#define WIFI_COUNTER_RECONNECT_WAIT     30000

/* WIFI Control API */
void wifi_task(void);
void wifi_timer_update(void);
int wifi_is_ready(void);
int wifi_is_busy(void);
int wifi_has_error(void);

/* WIFI Feature API */
void wifi_request_time(void);

/* WIFI Socket Handling API */
int wifi_connect(char * host, int port);
int wifi_read_data(uint8_t *read_buffer, uint32_t read_length, uint32_t timeout_ms);
int wifi_send_data(uint8_t *send_buffer, uint32_t send_length, uint32_t timeout_ms);

#endif /* WIFI_TASK_H_ */