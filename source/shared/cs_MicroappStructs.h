/*
 * Microapp structs.
 *
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Dec 10, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

const uint8_t MAX_PAYLOAD = 32;

enum CommandMicroappPin {
	CS_MICROAPP_COMMAND_PIN_SWITCH        = 0x00,
	CS_MICROAPP_COMMAND_PIN_DIMMER        = 0x00, // same one
	CS_MICROAPP_COMMAND_PIN_GPIO1         = 0x01, // GPIO pin on connector
	CS_MICROAPP_COMMAND_PIN_GPIO2         = 0x02, // GPIO pin on connector
	CS_MICROAPP_COMMAND_PIN_GPIO3         = 0x03, // GPIO pin on connector
};

enum ErrorCodesMicroapp {
	ERR_NO_PAYLOAD                        = 0x01,    // need at least an opcode in the payload
	ERR_TOO_LARGE                         = 0x02,
};

enum CommandMicroapp {
	CS_MICROAPP_COMMAND_LOG               = 0x01,
	CS_MICROAPP_COMMAND_DELAY             = 0x02,
	CS_MICROAPP_COMMAND_PIN               = 0x03,
	CS_MICROAPP_COMMAND_SERVICE_DATA      = 0x04,
};

enum CommandMicroappLog {
	CS_MICROAPP_COMMAND_LOG_CHAR          = 0x00,
	CS_MICROAPP_COMMAND_LOG_INT           = 0x01,
	CS_MICROAPP_COMMAND_LOG_STR           = 0x02,
};

enum CommandMicroappPinOpcode {
	CS_MICROAPP_COMMAND_PIN_READ          = 0x01,
	CS_MICROAPP_COMMAND_PIN_WRITE         = 0x02,
	CS_MICROAPP_COMMAND_PIN_TOGGLE        = 0x03,
	CS_MICROAPP_COMMAND_PIN_I2C_READ      = 0x04,
	CS_MICROAPP_COMMAND_PIN_I2C_WRITE     = 0x05,
};

enum CommandMicroappPinValue {
	CS_MICROAPP_COMMAND_VALUE_OFF         = 0x00,
	CS_MICROAPP_COMMAND_VALUE_ON          = 0x01,
};

/*
 * The structure used for communication between microapp and bluenet.
 */
typedef struct {
	uint8_t payload[MAX_PAYLOAD];
	int length;
} message_t;

/*
 * Struct to set and read pins. This can be used for analog and digital writes and reads. For digital writes it is
 * just zeros or ones. For analog writes it is an uint8_t. For reads the value field is the one that is being returned.
 */
typedef struct {
	uint8_t cmd;
	uint8_t pin;
	uint8_t opcode;
	uint8_t value;
	uint8_t ack;
} pin_cmd_t;

/*
 * Struct with data to implement sleep command through coroutines.
 */
typedef struct {
	uint8_t cmd;
	uint16_t period;
	uintptr_t coargs;
} sleep_cmd_t;

