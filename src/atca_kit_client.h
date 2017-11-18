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

#ifndef ATCA_KIT_CLIENT_H
#define ATCA_KIT_CLIENT_H

#include <stdint.h>
#include "cryptoauthlib.h"

#define KIT_MESSAGE_SIZE_MAX        (1024)

#define SHA204_RSP_SIZE_MIN          ((uint8_t)  4)  //!< minimum number of bytes in response
#define SHA204_RSP_SIZE_MAX          ((uint8_t) 100)  //!< maximum size of response packet

#define SHA204_BUFFER_POS_COUNT      (0)             //!< buffer index of count byte in command or response
#define SHA204_BUFFER_POS_DATA       (1)             //!< buffer index of data in response

#define USB_BUFFER_SIZE_TX                  (2300)	// larger than 2*(1024 + 100)
#define USB_BUFFER_SIZE_RX                  (2300)

//! Commands and responses are terminated by this character.
#define KIT_EOP							        '\n'

//! number of characters in response excluding data
#define KIT_RESPONSE_COUNT_NO_DATA              (5)

//! Every device response byte is converted into two hex-ascii characters.
#define KIT_CHARS_PER_BYTE                      (2)

#define DEVICE_BUFFER_SIZE_MAX_RX   (uint8_t) ((USB_BUFFER_SIZE_TX - KIT_RESPONSE_COUNT_NO_DATA) / KIT_CHARS_PER_BYTE)

#define DISCOVER_DEVICE_COUNT_MAX              (1)

// I2C address for device programming and initial communication
#define FACTORY_INIT_I2C			(uint8_t)(0xC0)	// Initial I2C address is set to 0xC0 in the factory
#define DEVICE_I2C					(uint8_t)(0xB0)			// Device I2C Address to program device to

// AES132 library occupies codes between 0x00 and 0xB4.
// SHA204 library occupies codes between 0xD0 and 0xF7.
enum {
	KIT_STATUS_SUCCESS             = 0x00,
	KIT_STATUS_UNKNOWN_COMMAND     = 0xC0,
	KIT_STATUS_USB_RX_OVERFLOW     = 0xC1,
	KIT_STATUS_USB_TX_OVERFLOW     = 0xC2,
	KIT_STATUS_INVALID_PARAMS      = 0xC3,
	KIT_STATUS_INVALID_IF_FUNCTION = 0xC4,
	KIT_STATUS_NO_DEVICE           = 0xC5
};

//! USB packet state.
enum usb_packet_state
{
	PACKET_STATE_IDLE,
	PACKET_STATE_TAKE_DATA,
	PACKET_STATE_END_OF_VALID_DATA,
	PACKET_STATE_OVERFLOW
};

//! enumeration for device types
typedef enum {
	DEVICE_TYPE_UNKNOWN,   //!< unknown device
	DEVICE_TYPE_CM,        //!< CM, currently not supported
	DEVICE_TYPE_CRF,       //!< CRF, currently not supported
	DEVICE_TYPE_CMC,       //!< CMC, currently not supported
	DEVICE_TYPE_SA100S,    //!< SA100S, can be discovered, but is currently not supported
	DEVICE_TYPE_SA102S,    //!< SA102S, can be discovered, but is currently not supported
	DEVICE_TYPE_SA10HS,    //!< SA10HS, can be discovered, but is currently not supported
	DEVICE_TYPE_SHA204,    //!< SHA204 device
	DEVICE_TYPE_AES132,    //!< AES132 device
	DEVICE_TYPE_ECC108,    //!< ECC108 device
	DEVICE_TYPE_ECC108A,   //!< ECC108A device
	DEVICE_TYPE_ECC508A,   //!< ECC508A device
	DEVICE_TYPE_ECC608A    //!< ECC608A device
} device_type_t;

//! enumeration for interface types
typedef enum {
	DEVKIT_IF_UNKNOWN,
	DEVKIT_IF_SPI,
	DEVKIT_IF_I2C,
	DEVKIT_IF_SWI,
	DEVKIT_IF_UART
} interface_id_t;

//! information about a discovered device
typedef struct {
	//! interface type (SWI, I2C, SPI)
	interface_id_t bus_type;
	//! device type
	device_type_t device_type;
	//! I2C address or selector byte
	uint8_t address;
	//! array index into GPIO structure for SWI and SPI (Microbase does not support this for SPI.)
	uint8_t device_index;
	//! revision bytes (four bytes for SHA204 and ECC108, first two bytes for AES132)
	uint8_t dev_rev[4];
} device_info_t;

uint8_t *atca_kit_get_rx_buffer(void);
uint8_t *atca_kit_get_tx_buffer(void);
uint8_t atca_kit_convert_nibble_to_ascii(uint8_t nibble);
uint8_t atca_kit_convert_ascii_to_nibble(uint8_t ascii);
uint16_t atca_kit_convert_ascii_to_binary(uint16_t length, uint8_t *buffer);
device_info_t* atca_kit_get_device_info(uint8_t index);
device_type_t atca_kit_get_device_type(uint8_t index);
ATCA_STATUS atca_kit_detect_I2c_devices(void);
ATCA_STATUS atca_kit_detect_swi_devices(void);
interface_id_t atca_kit_discover_devices(void);
uint8_t atca_kit_parse_board_commands(uint16_t commandLength, uint8_t* command, 	uint16_t* responseLength, uint8_t* response, uint8_t* responseIsAscii);
uint8_t atca_kit_get_commands_info(uint8_t *tx_buffer, uint8_t *cmd_index, uint16_t *rx_length);
uint8_t atca_kit_send_and_receive(uint8_t *tx_buffer, uint8_t *rx_buffer);
uint8_t atca_kit_send_command(uint8_t *tx_buffer);
uint8_t atca_kit_receive_response(uint8_t size, uint8_t *rx_buffer);
uint8_t atca_kit_parse_ecc_commands(uint16_t commandLength, uint8_t *command, uint16_t* responseLength, uint8_t* response);
uint8_t atca_kit_extract_data_load(const char* command, uint16_t* dataLength, uint8_t** data);
uint16_t atca_kit_create_usb_packet(uint16_t length, uint8_t *buffer);
uint16_t atca_kit_convert_data(uint16_t length, uint8_t *buffer);
uint8_t* atca_kit_process_usb_packet(uint16_t rx_length, uint16_t* txLength);



void atca_kit_main_handler(void);
bool atca_kit_lock(void);
void atca_kit_timer_update(void);

#endif /* ATCA_KIT_CLIENT_H */
