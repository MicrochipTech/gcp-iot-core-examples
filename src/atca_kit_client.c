/**
 * \brief  Atmel Crypto Auth hardware interface object
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 */

#include <asf.h>
#include <ctype.h>
#include <string.h>
#include "conf_usb.h"
#include "config.h"
#include "basic/atca_basic.h"
#include "atca_kit_client.h"
#include "time_utils.h"

/** \brief version of development kit firmware
 *         that contains AES132 and SHA204 library */
const char VersionKit[] = {1, 0, 5};
const char VersionSha204[] = {1, 3, 0};
const char VersionAes132[] = {1, 1, 0};
const char VersionEcc508[] = {1, 1, 0};      //!< ECC108 string

const char StringSha204[] = "SHA204 ";      //!< SHA204 string
const char StringAes132[] = "AES132 ";      //!< AES132 string
const char StringEcc508[] = "ECC108 ";      //!< ECC108 string

const char StringKitShort[] = "CK101 ";     //!< short string of Microbase kit
const char StringKit[] = "AT88CK101STK ";   //!< long string of Microbase kit

device_info_t device_info[DISCOVER_DEVICE_COUNT_MAX];
uint8_t device_count = 0;

static uint8_t pucUsbRxBuffer[USB_BUFFER_SIZE_RX];
static uint8_t pucUsbTxBuffer[USB_BUFFER_SIZE_TX];
static uint8_t rxPacketStatus = KIT_STATUS_SUCCESS;
static uint16_t rxBufferIndex = 0;

static uint32_t atca_kit_holdoff;

/** \brief This function returns the rx buffer.
 *  \return pointer to the current rx buffer index
 */
uint8_t* atca_kit_get_rx_buffer(void)
{
	return pucUsbRxBuffer;
}

uint8_t* atca_kit_get_tx_buffer(void)
{
	return pucUsbTxBuffer;
}

bool atca_kit_lock(void)
{
    return atca_kit_holdoff;
}

static void atca_kit_counter_set(uint32_t val)
{
    /* Convert to loop time*/
    atca_kit_holdoff = val / TIMER_UPDATE_PERIOD;

    if(val && !atca_kit_holdoff)
    {
        atca_kit_holdoff = 1;
    }
}

/* Must be called on the TIMER_UPDATE_PERIOD */
void atca_kit_timer_update(void)
{
    if(atca_kit_holdoff)
    {
        atca_kit_holdoff--;
    }
}

/** \brief This function converts a nibble to Hex-ASCII.
 * \param[in] nibble nibble value to be converted
 * \return ASCII value
**/
uint8_t atca_kit_convert_nibble_to_ascii(uint8_t nibble)
{
    nibble &= 0x0F;
    if (nibble <= 0x09 )
        nibble += '0';
    else
        nibble += ('A' - 10);
    return nibble;
}

/** \brief This function converts an ASCII character to a nibble.
 * \param[in] ascii ASCII value to be converted
 * \return nibble value
**/
uint8_t atca_kit_convert_ascii_to_nibble(uint8_t ascii)
{
    if ((ascii <= '9') && (ascii >= '0'))
        ascii -= '0';
    else if ((ascii <= 'F' ) && (ascii >= 'A'))
        ascii -= ('A' - 10);
    else if ((ascii <= 'f') && (ascii >= 'a'))
        ascii -= ('a' - 10);
    else
        ascii = 0;
    return ascii;
}

/** \brief This function converts ASCII to binary.
 * \param[in] length number of bytes in buffer
 * \param[in, out] buffer pointer to buffer
 * \return number of bytes in buffer
 */
uint16_t atca_kit_convert_ascii_to_binary(uint16_t length, uint8_t *buffer)
{
	if (length < 2)
		return 0;

	uint16_t i, binIndex;

	for (i = 0, binIndex = 0; i < length; i += 2)
	{
		buffer[binIndex] = atca_kit_convert_ascii_to_nibble(buffer[i]) << 4;
		buffer[binIndex++] |= atca_kit_convert_ascii_to_nibble(buffer[i + 1]);
	}

	return binIndex;
}

device_info_t* atca_kit_get_device_info(uint8_t index) 
{
	if (index >= device_count)
		return NULL;
	return &device_info[index];
}

device_type_t atca_kit_get_device_type(uint8_t index) 
{
	if (index >= device_count)
		return DEVICE_TYPE_UNKNOWN;
	return device_info[index].device_type;
}

ATCA_STATUS atca_kit_detect_I2c_devices()
{
	ATCA_STATUS status = ATCA_NO_DEVICES;
	uint8_t revision[4];
	uint8_t i;

	status = atcab_init( &cfg_ateccx08a_i2c_default );
	if (status != ATCA_SUCCESS)
	{
		return status;
	}

	/* Verify the device by retrieving the revision */
	status = atcab_info(revision);
	for (i=0xB0; i<0xC8 && status; i+=2)
	{
		cfg_ateccx08a_i2c_default.atcai2c.slave_address = i;
		status = atcab_info(revision);
	}

	/* If the loop terminated before finding a device */
	if (status != ATCA_SUCCESS)
	{
		return status;
	}

	device_info[device_count].address = cfg_ateccx08a_i2c_default.atcai2c.slave_address;
	device_info[device_count].bus_type = DEVKIT_IF_I2C;

	switch(revision[2])
	{
		case 0x50:
			device_info[device_count].device_type = DEVICE_TYPE_ECC508A;
			cfg_ateccx08a_i2c_default.devtype = ATECC508A;
			break;
		case 0x60:
			device_info[device_count].device_type = DEVICE_TYPE_ECC608A;
			cfg_ateccx08a_i2c_default.devtype = ATECC608A;

			/* Have to reinit to pick up clock settings if 608 is using lower power modes */
			status = atcab_init( &cfg_ateccx08a_i2c_default );
			if (status != ATCA_SUCCESS)
			{
				return status;
			}
			break;
		default:
			device_info[device_count].device_type = DEVICE_TYPE_ECC108A;
			cfg_ateccx08a_i2c_default.devtype = ATECC108A;
			break;
	}
	
	memcpy(device_info[device_count].dev_rev, revision, sizeof(revision));

	device_count++;
	
	return status;
}

/** \brief This function tries to find SHA204 and / or AES132 devices.
 *
 *         It calls functions for all three interfaces,
 *         SWI, I2C, and SPI. They in turn enter found
 *         devices into the #device_info array.
 * \return interface found
 */
interface_id_t atca_kit_discover_devices()
{
	ATCA_STATUS status = ATCA_NO_DEVICES;
	interface_id_t bus_type;

	device_count = 0;
	memset(device_info, 0, sizeof(device_info));

	status = atca_kit_detect_I2c_devices();

	if (device_count == 0 || status != ATCA_SUCCESS)
		return DEVKIT_IF_UNKNOWN;

	bus_type = device_info[0].bus_type;

	return bus_type;
}

#pragma pack(push, 1)
struct kit_app_datetime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};
#pragma pack(pop)

static uint8_t atca_kit_process_board_app_command(uint8_t * rxbuf, uint16_t rxlen, uint8_t * txbuf, uint16_t * txlen)
{
    ATCA_STATUS status = ATCA_PARSE_ERROR;

    if(!rxbuf || !rxlen)
    {
        return ATCA_BAD_PARAM;
    }

    switch(rxbuf[0])
    {
        case 0:
            /* Set Time */
            if(sizeof(struct kit_app_datetime) < rxlen)
            {
                struct kit_app_datetime * datetime = (struct kit_app_datetime *)&rxbuf[1];
                time_utils_set(datetime->year, datetime->month, datetime->day, datetime->hour, datetime->minute, datetime->minute);
                status = ATCA_SUCCESS;
            }
            break;
        default:
            break;
    }
    return status;
}


/** \brief This function parses kit commands (ASCII) received from a
 * 			PC host and returns an ASCII response.
 * \param[in] commandLength number of bytes in command buffer
 * \param[in] command pointer to ASCII command buffer
 * \param[out] responseLength pointer to number of bytes in response buffer
 * \param[out] response pointer to binary response buffer
 * \param[out] responseIsAscii pointer to response type
 * \return the status of the operation
 */
uint8_t atca_kit_parse_board_commands(uint16_t commandLength, uint8_t *command, 
										uint16_t *responseLength, uint8_t *response, uint8_t *responseIsAscii)
{
	uint8_t status = KIT_STATUS_UNKNOWN_COMMAND;
	uint16_t responseIndex = 0;
	uint16_t deviceIndex;
	uint16_t dataLength = 1;
	uint8_t *rxData[1];
	interface_id_t device_interface = DEVKIT_IF_UNKNOWN;
	device_info_t* dev_info;
	const char *StringInterface[] = {"no_device ", "SPI ", "TWI ", "SWI "};
	const char *pToken = strchr((char *) command, ':');

	if (!pToken)
		return status;

	*responseIsAscii = 1;

	switch(pToken[1]) {
        case 'a':
            /* Application commands */
			status = atca_kit_extract_data_load((const char*)pToken, &dataLength, rxData);
			if (status != KIT_STATUS_SUCCESS)
				break;

            status = atca_kit_process_board_app_command(rxData[0], dataLength, response, responseLength);

            break;

		case 'v':
			// Gets abbreviated board name and, if found, first device type and interface type.
			// response (no device): <kit version>, "no_devices"<status>()
			// response (device found): <kit version>, <device type>, <interface><status>(<address>)			
			break;
		
		case 'f':
			status = atca_kit_extract_data_load((const char*)pToken, &dataLength, rxData);
			if (status != KIT_STATUS_SUCCESS)
				break;

			dataLength = 4; // size of versions + status byte

			switch (*rxData[0]) {
				case 0: // kit version
					strcpy((char *) response, StringKit);
					responseIndex = strlen((char *) response);
					memcpy((char *) (response + responseIndex + 1), VersionKit, dataLength);
					break;

				case 1: // SHA204 library version
					strcpy((char *) response, StringSha204);
					responseIndex = strlen((char *) response);
					memcpy((char *) (response + responseIndex + 1), VersionSha204, dataLength);
					break;

				case 2: // AES132 library version
					strcpy((char *) response, StringAes132);
					responseIndex = strlen((char *) response);
					memcpy((char *) (response + responseIndex + 1), VersionAes132, dataLength);
					break;

				case 3: // ECC508 library version
					strcpy((char *) response, StringEcc508);
					responseIndex = strlen((char *) response);
					memcpy((char *) (response + responseIndex + 1), VersionEcc508, dataLength);
					break;

				default:
					status = KIT_STATUS_INVALID_PARAMS;
					break;
			}
			break;

		case 'd':
			status = atca_kit_extract_data_load((const char*)pToken, &dataLength, rxData);
			if (status != KIT_STATUS_SUCCESS)
				break;

			device_interface = atca_kit_discover_devices();
			deviceIndex = *rxData[0];
			dev_info = atca_kit_get_device_info(deviceIndex);
			if (!dev_info) {
				status = KIT_STATUS_NO_DEVICE;
				break;
			}

			switch (dev_info->device_type) {
				case DEVICE_TYPE_SHA204:
					strcpy((char *) response, StringSha204);
					break;

				case DEVICE_TYPE_AES132:
					strcpy((char *) response, StringAes132);
					break;

				case DEVICE_TYPE_ECC108:
                case DEVICE_TYPE_ECC508A:
                case DEVICE_TYPE_ECC608A:
					strcpy((char *) response, StringEcc508);
					break;

				case DEVICE_TYPE_UNKNOWN:
					strcpy((char *) response, StringInterface[0]);
					status = KIT_STATUS_NO_DEVICE;
					break;

				default:
					strcpy((char *) response, "unknown_device");
					break;
			}


			if (dev_info->bus_type == DEVKIT_IF_UNKNOWN) {
				responseIndex = strlen((char *) response);
				break;
			}
			
			// Append interface type to response.
			strcat((char *) response, StringInterface[device_interface]);
			responseIndex = strlen((char *) response);

			// Append the address (TWI) / index (SWI) of the device.
			// Skip one byte for status.
			dataLength++;
			response[responseIndex + 1] = dev_info->bus_type == DEVKIT_IF_I2C ? dev_info->address : dev_info->device_index;
			break;

		default:
			status = KIT_STATUS_UNKNOWN_COMMAND;
			break;
			
	}
	
	// Append <status>(<data>).
	response[responseIndex] = status;
	*responseLength = atca_kit_create_usb_packet(dataLength, &response[responseIndex]) + responseIndex;
	
	return status;
}

/** \brief Give index of command and response length based on received command.  
 * \param[in] tx_buffer includes command to be sent to device 
 * \param[out] cmd_index is index corresponding to opcode
 * \param[out] rx_length is length of response to be came to device
 * \return ATCA_SUCCESS
 */
uint8_t atca_kit_get_commands_info(uint8_t *tx_buffer, uint8_t *cmd_index, uint16_t *rx_length)
{
	uint8_t status = ATCA_SUCCESS;
	uint8_t opCode = tx_buffer[1];
	uint8_t param1 = tx_buffer[2];
	
	switch (opCode) {
		
		case ATCA_CHECKMAC:
			*cmd_index = CMD_CHECKMAC;
			*rx_length = CHECKMAC_RSP_SIZE;
			break;
		
		case ATCA_COUNTER:
			*cmd_index = CMD_COUNTER;
			*rx_length = COUNTER_RSP_SIZE;
			break;
		
		case ATCA_DERIVE_KEY:
			*cmd_index = CMD_DERIVEKEY;
			*rx_length = DERIVE_KEY_RSP_SIZE;
			break;
		
		case ATCA_ECDH:
			*cmd_index = CMD_ECDH;
			*rx_length = ECDH_RSP_SIZE;
			break;
		
		case ATCA_GENDIG:
			*cmd_index = CMD_GENDIG;
			*rx_length = GENDIG_RSP_SIZE;
			break;
		
		case ATCA_GENKEY:
			*cmd_index = CMD_GENKEY;
			*rx_length = (param1 == GENKEY_MODE_DIGEST)	? GENKEY_RSP_SIZE_SHORT : GENKEY_RSP_SIZE_LONG;
			break;
		
		case ATCA_HMAC:
			*cmd_index = CMD_HMAC;
			*rx_length = HMAC_RSP_SIZE;
			break;
		
		case ATCA_INFO:
			*cmd_index = CMD_INFO;
			*rx_length = INFO_RSP_SIZE;
			break;

		case ATCA_LOCK:
			*cmd_index = CMD_LOCK;
			*rx_length = LOCK_RSP_SIZE;
			break;
		
		case ATCA_MAC:
			*cmd_index = CMD_MAC;
			*rx_length = MAC_RSP_SIZE;
			break;
		
		case ATCA_NONCE:
			*cmd_index = CMD_NONCE;
			*rx_length = (param1 == NONCE_MODE_PASSTHROUGH)	? NONCE_RSP_SIZE_SHORT : NONCE_RSP_SIZE_LONG;
			break;
		
		case ATCA_PAUSE:
			*cmd_index = CMD_PAUSE;
			*rx_length = PAUSE_RSP_SIZE;
			break;
		
		case ATCA_PRIVWRITE:
			*cmd_index = CMD_PRIVWRITE;
			*rx_length = PRIVWRITE_RSP_SIZE;
			break;
		
		case ATCA_RANDOM:
			*cmd_index = CMD_RANDOM;
			*rx_length = RANDOM_RSP_SIZE;
			break;
		
		case ATCA_READ:
			*cmd_index = CMD_READMEM;
			*rx_length = (param1 & 0x80)	? READ_32_RSP_SIZE : READ_4_RSP_SIZE;
			break;
		
		case ATCA_SHA:
			*cmd_index = CMD_SHA;
			*rx_length = (param1 == SHA_MODE_SHA256_END) ? ATCA_RSP_SIZE_32 : ATCA_RSP_SIZE_4;
			break;

		case ATCA_SIGN:
			*cmd_index = CMD_SIGN;
			*rx_length = SIGN_RSP_SIZE;
			break;
		
		case ATCA_UPDATE_EXTRA:
			*cmd_index = CMD_UPDATEEXTRA;
			*rx_length = UPDATE_RSP_SIZE;
			break;
		
		case ATCA_VERIFY:
			*cmd_index = CMD_VERIFY;
			*rx_length = VERIFY_RSP_SIZE;
			break;
		
		case ATCA_WRITE:
			*cmd_index = CMD_WRITEMEM;
			*rx_length = WRITE_RSP_SIZE;
			break;
		
		default:
			break;
		
	}

	return status;
}

/** \brief Actually send a command array and receive a result from device.  
 * \param[in] tx_buffer is buffer to be sent
 * \param[in] rx_buffer is buffer to be received
 * \return ATCA_STATUS
 */
uint8_t atca_kit_send_and_receive(uint8_t *tx_buffer, uint8_t *rx_buffer)
{
	uint8_t status = ATCA_SUCCESS;
	uint8_t cmd_index;
	uint16_t rx_length;
	uint8_t *cmd_buffer;
	ATCADevice  _Device = NULL;
	ATCACommand _CommandObj = NULL;
	ATCAIface   _Iface = NULL;

	do {

		if ( tx_buffer == NULL || rx_buffer == NULL )
			break;

		if ( atca_kit_get_commands_info( tx_buffer, &cmd_index, &rx_length ) != ATCA_SUCCESS )
			break;

		cmd_buffer = (uint8_t *)malloc(tx_buffer[0] + 1);
		memcpy(&cmd_buffer[1], tx_buffer, tx_buffer[0]);
		
		_Device= atcab_get_device();
		_CommandObj = atGetCommands(_gDevice);

        if ((status = atGetExecTime(tx_buffer[1], _CommandObj)) != ATCA_SUCCESS)
        {
            break;
        }

		if ( (status = atcab_wakeup()) != ATCA_SUCCESS )
			break;

		_Device= atcab_get_device();
		_Iface = atGetIFace(_Device);
		
		// send the command
		if ((status = atsend( _Iface, (uint8_t *)cmd_buffer, tx_buffer[0])) != ATCA_SUCCESS )
			break;

		// delay the appropriate amount of time for command to execute
		atca_delay_ms(_CommandObj->execution_time_msec);

		// receive the response
		if ((status = atreceive( _Iface, rx_buffer, &rx_length)) != ATCA_SUCCESS )
			break;

		atcab_idle();

		free((void *)cmd_buffer);

	} while(0);
	
	return status;
	
}

/** \brief Only send a command array.
 * \param[in] tx_buffer is buffer to be sent
 * \return ATCA_STATUS
 */
uint8_t atca_kit_send_command(uint8_t *tx_buffer)
{
	uint8_t status = ATCA_SUCCESS;
	uint8_t cmd_index;
	uint16_t rx_length;
	uint8_t *cmd_buffer;
	ATCADevice  _Device = NULL;
	ATCACommand _CommandObj = NULL;
	ATCAIface   _Iface = NULL;

	do {

		if ( tx_buffer == NULL )
			break;

		if ( atca_kit_get_commands_info( tx_buffer, &cmd_index, &rx_length ) != ATCA_SUCCESS )
			break;

		cmd_buffer = (uint8_t *)malloc(tx_buffer[0] + 1);
		memcpy(&cmd_buffer[1], tx_buffer, tx_buffer[0]);

		_Device= atcab_get_device();
		_CommandObj = atGetCommands(_Device);
		
        if ((status = atGetExecTime(tx_buffer[1], _CommandObj)) != ATCA_SUCCESS)
        {
            break;
        }

		if ( (status = atcab_wakeup()) != ATCA_SUCCESS )
			break;

		_Device= atcab_get_device();
		_Iface = atGetIFace(_Device);
		
		// send the command
		if ( (status = atsend( _Iface, (uint8_t *)cmd_buffer, tx_buffer[0])) != ATCA_SUCCESS )
			break;

		// delay the appropriate amount of time for command to execute
		atca_delay_ms(_CommandObj->execution_time_msec);

		atcab_idle();

		free((void *)cmd_buffer);

	} while(0);
	
	return status;
	
}

/** \brief Only receive a command array.
 * \param[in] size is size to be received
 * \param[out] rx_buffer is buffer that includes data to be received from a device 
 * \return ATCA_STATUS
 */
uint8_t atca_kit_receive_response(uint8_t size, uint8_t *rx_buffer)
{
	uint8_t status = ATCA_SUCCESS;
	uint16_t rxlength = size;	
	ATCADevice  _Device = NULL;
	ATCAIface   _Iface = NULL;

	do {

		if ( rx_buffer == NULL )
			break;

		_Device= atcab_get_device();
		
		if ( (status = atcab_wakeup()) != ATCA_SUCCESS )
			break;

		_Device= atcab_get_device();
		_Iface = atGetIFace(_Device);

		// receive the response
		if ( (status = atreceive( _Iface, rx_buffer, &rxlength)) != ATCA_SUCCESS )
			break;

		atcab_idle();

	} while(0);
	
	return status;
	
}

/** \brief This function parses communication commands (ASCII) received from a
 *         PC host and returns a binary response.
 *
 *         protocol syntax:\n\n
 *         functions for command sequences:\n
 *            v[erify]                            several Communication and Command Marshaling layer functions
 *            a[tomic]                            Wraps "talk" into a Wakeup / Idle.
 *         functions in sha204_comm.c (Communication layer):\n
 *            w[akeup]                            sha204c_wakeup\n
 *            t[alk](command)                     sha204c_send_and_receive\n
 *         functions in sha204_i2c.c / sha204_swi.c (Physical layer):\n
 *            [physical:]s[leep]                  sha204p_sleep\n
 *            [physical:]i[dle]                   sha204p_idle\n
 *            p[hysical]:r[esync]                 sha204p_resync\n
 *            p[hysical]:e[nable]                 sha204p_init\n
 *            p[hysical]:d[isable]                sha204p_disable_interface\n
 *            c[ommand](data)                     sha204p_send_command\n
 *            r[esponse](size)                    sha204p_receive_response\n
 * \param[in] commandLength number of bytes in command buffer
 * \param[in] command pointer to ASCII command buffer
 * \param[out] responseLength pointer to number of bytes in response buffer
 * \param[out] response pointer to binary response buffer
 * \return the status of the operation
 */
uint8_t atca_kit_parse_ecc_commands(uint16_t commandLength, uint8_t *command, uint16_t *responseLength, uint8_t *response)
{
	uint8_t status = KIT_STATUS_SUCCESS;
	uint16_t dataLength;
	uint8_t *data_load[1];
	uint8_t *dataLoad;
	char *pToken = strchr((char *) command, ':');

	*responseLength = 0;

	if (!pToken)
		return status;

	switch (pToken[1]) {
		// Talk (send command and receive response)		
		case 't':
			status = atca_kit_extract_data_load((const char*)pToken + 2, &dataLength, data_load);
			if (status != KIT_STATUS_SUCCESS)
				return status;

			response[SHA204_BUFFER_POS_COUNT] = 0;
			status = atca_kit_send_and_receive(data_load[0], &response[0]);
			if (status != KIT_STATUS_SUCCESS)
				return status;

			*responseLength = response[SHA204_BUFFER_POS_COUNT];
			break;

		// Wakeup
		case 'w':
			status = atcab_wakeup();
			if (status != KIT_STATUS_SUCCESS)
				return status;
			break;

		// Sleep
		case 's':
			status = atcab_sleep();
			if (status != KIT_STATUS_SUCCESS)
				return status;
			break;

		// Idle
		case 'i':
			status = atcab_idle();
			if (status != KIT_STATUS_SUCCESS)
				return status;			
			break;
		
		// Switch whether to wrap a Wakeup / Idle around a "talk" message.
		case 'a':
			status = atca_kit_extract_data_load((const char*)pToken + 2, &dataLength, data_load);
			if (status != KIT_STATUS_SUCCESS)
				return status;
			break;

		// --------- calls functions in sha204_i2c.c and sha204_swi.c  ------------------
		case 'p':
			// ----------------------- "s[ha204]:p[hysical]:" ---------------------------
			pToken = strchr(&pToken[1], ':');
			if (!pToken)
				return status;

			switch (pToken[1]) {
				// Wake-up without receive.
				case 'w':
					status = atcab_wakeup();
					if (status != KIT_STATUS_SUCCESS)
						return status;					
					break;

				case 'c':
					// Send command.
					status = atca_kit_extract_data_load((const char*)pToken + 2, &dataLength, data_load);
					if (status != KIT_STATUS_SUCCESS)
						return status;
					dataLoad = data_load[0];
					status = atca_kit_send_command(dataLoad);				
					break;

				// Receive response.
				case 'r':
					status = atca_kit_extract_data_load((const char*)pToken + 2, &dataLength, data_load);
					if (status != KIT_STATUS_SUCCESS)
						return status;
					// Reset count byte.
					response[SHA204_BUFFER_POS_COUNT] = 0;
					status = atca_kit_receive_response(*data_load[0], response);
					if (status != KIT_STATUS_SUCCESS)
						return status;					
					*responseLength = response[SHA204_BUFFER_POS_COUNT];
					break;

				case 's':
					// -- "s[elect](device index | TWI address)" or "s[leep]" ----------------
					status = atca_kit_extract_data_load((const char*)pToken + 2, &dataLength, data_load);
					if (status == KIT_STATUS_SUCCESS) {
						// Select device (I2C: address; SWI: index into GPIO array).
						dataLoad = data_load[0];
						//cfg_ateccx08a_i2c_default.atcai2c.slave_address = 0xC0;
					} else {
						// Sleep command
						status = atcab_idle();
						if (status != KIT_STATUS_SUCCESS)
							return status;						
					}
					break;

				default:
					status = KIT_STATUS_UNKNOWN_COMMAND;
					break;
					
				} // end physical			
			break;
			
		default:
			status = KIT_STATUS_UNKNOWN_COMMAND;
			break;
	}
	
	return status;
}

/** \brief This function extracts data from a command string and
 * 			converts them to binary.
 *
 * The data load is expected to be in Hex-Ascii and surrounded by parentheses.
 * \param[in] command command string
 * \param[out] dataLength number of bytes extracted
 * \param[out] data pointer to pointer to binary data
 * \return status: invalid parameters or success
 */
uint8_t atca_kit_extract_data_load(const char* command, uint16_t* dataLength, uint8_t** data)
{
	uint8_t status = KIT_STATUS_INVALID_PARAMS;
	if (!command || !dataLength || !data)
		return status;

	char* pToken = strchr(command, '(');
	if (!pToken)
		return status;

	char* dataEnd = strchr(pToken, ')');
	if (!dataEnd)
		// Allow a missing closing parenthesis.
		dataEnd = (char *) command + strlen(command);
	else
		dataEnd--;

	uint16_t asciiLength = (uint16_t) (dataEnd - pToken);
	*data = (uint8_t *) pToken + 1;
	*dataLength = atca_kit_convert_ascii_to_binary(asciiLength, *data);

	return KIT_STATUS_SUCCESS;
}

/** \brief This function converts binary response data to hex-ascii and packs it into a protocol response.
           <status byte> <'('> <hex-ascii data> <')'> <'\n'>
    \param[in] length number of bytes in data load plus one status byte
    \param[in] buffer pointer to data
    \return length of ASCII data
*/
uint16_t atca_kit_create_usb_packet(uint16_t length, uint8_t *buffer)
{
	uint16_t binBufferIndex = length - 1;
	// Size of data load is length minus status byte.
	uint16_t asciiLength = 2 * (length - 1) + 5; // + 5: 2 status byte characters + '(' + ")\n"
	uint16_t asciiBufferIndex = asciiLength - 1;
	uint8_t byteValue;

	// Terminate ASCII packet.
	buffer[asciiBufferIndex--] = KIT_EOP;

	// Append ')'.
	buffer[asciiBufferIndex--] = ')';

	// Convert binary data to hex-ascii starting with the last byte of data.
	while (binBufferIndex)
	{
		byteValue = buffer[binBufferIndex--];
		buffer[asciiBufferIndex--] = atca_kit_convert_nibble_to_ascii(byteValue);
		buffer[asciiBufferIndex--] = atca_kit_convert_nibble_to_ascii(byteValue >> 4);
	}

	// Start data load with open parenthesis.
	buffer[asciiBufferIndex--] = '(';

	// Convert first byte (function return value) to hex-ascii.
	byteValue = buffer[0];
	buffer[asciiBufferIndex--] = atca_kit_convert_nibble_to_ascii(byteValue);
	buffer[asciiBufferIndex] = atca_kit_convert_nibble_to_ascii(byteValue >> 4);

	return asciiLength;
}

/** \brief This function converts binary data to Hex-ASCII.
 * \param[in] length number of bytes to send
 * \param[in] buffer pointer to tx buffer
 * \return new length of data
 */
uint16_t atca_kit_convert_data(uint16_t length, uint8_t *buffer)
{
	if (length > DEVICE_BUFFER_SIZE_MAX_RX) {
		buffer[0] = KIT_STATUS_USB_TX_OVERFLOW;
		length = DEVICE_BUFFER_SIZE_MAX_RX;
	}
	return atca_kit_create_usb_packet(length, buffer);
}

/** \brief Interpret Rx packet, and then execute received command.  
 * \param[in] rx_length is length of received packet 
 * \param[in] txLength is Tx length to be sent to Host
 * returns pointer of buffer to be sent
 */
uint8_t* atca_kit_process_usb_packet(uint16_t rx_length, uint16_t *txLength)
{
	uint8_t status = KIT_STATUS_SUCCESS;
	uint8_t responseIsAscii = 0;
	uint16_t rxLength = rx_length - 1;	// except for a line feed character
	uint8_t* txBuffer = atca_kit_get_tx_buffer();
	uint8_t* pRxBuffer = atca_kit_get_rx_buffer();

	if (rxPacketStatus != KIT_STATUS_SUCCESS) {
		pucUsbTxBuffer[0] = rxPacketStatus;
		*txLength = 1;
		*txLength = atca_kit_convert_data(*txLength, pucUsbTxBuffer);
	}

	memset(pucUsbTxBuffer, 0, sizeof(pucUsbTxBuffer));

	// Process packet.
	for (uint16_t i = 0; i < rxLength; i++)
		pRxBuffer[i] = tolower(pRxBuffer[i]);

	if (pRxBuffer[0] == 'l') {	// lib
		// "lib" as the first field is optional. Move rx pointer to the next field.
		pRxBuffer = memchr(pRxBuffer, ':', rxBufferIndex);
		if (!pRxBuffer)
			status = KIT_STATUS_UNKNOWN_COMMAND;
		else
			pRxBuffer++;
	}

	switch (pRxBuffer[0]) {

		case 's':
		case 'e':			
			status = atca_kit_parse_ecc_commands(rxLength, (uint8_t *)pRxBuffer, txLength, pucUsbTxBuffer + 1);
			break;

		case 'b':
			// board level commands ("b[oard]")
			status = atca_kit_parse_board_commands((uint8_t) rxLength, (uint8_t *)pRxBuffer, txLength, txBuffer, &responseIsAscii);
			break;

		default :
			status = KIT_STATUS_UNKNOWN_COMMAND;
			*txLength = 1;			
			break;
	}

	if (!responseIsAscii) {
		// Copy leading function return byte.
		pucUsbTxBuffer[0] = status;
		// Tell atca_kit_convert_data the correct txLength.
		if (*txLength < DEVICE_BUFFER_SIZE_MAX_RX)
			(*txLength)++;
		*txLength = atca_kit_convert_data(*txLength, pucUsbTxBuffer);
	}

	return txBuffer;
}

/** \brief This handler is to receive and send USB packets with Host over the HID interface.
 */
void atca_kit_main_handler(void)
{
    uint16_t txlen;

    if(g_usb_message_received)
    {
        atca_kit_counter_set(5000);

        //DEBUG_PRINTF("Kit RX: %d, %s\r\n", g_usb_buffer_length, pucUsbRxBuffer);

        atca_kit_process_usb_packet(g_usb_buffer_length, &txlen);
        g_usb_buffer_length = 0;
        
        //DEBUG_PRINTF("Kit TX: %d, %s\r\n", txlen, pucUsbTxBuffer);
        usb_send_response_message(pucUsbTxBuffer, txlen);

        g_usb_message_received--;
    }
}

