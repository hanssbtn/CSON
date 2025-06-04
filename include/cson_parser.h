#ifndef CSON_PARSER_H__
#define CSON_PARSER_H__

#define BUFFER_SIZE 8192

#include "cson_common.h"

#define CSON_PARSER_FLAG_FOUND_SIGN 1
#define CSON_PARSER_FLAG_FOUND_PERIOD 2
#define CSON_PARSER_FLAG_FOUND_EXPONENT 4
#define CSON_PARSER_FLAG_FOUND_NULL_N 8
#define CSON_PARSER_FLAG_FOUND_NULL_U 16
#define CSON_PARSER_FLAG_FOUND_NULL_L1 32
#define CSON_PARSER_FLAG_FOUND_KEY_START 64
#define CSON_PARSER_FLAG_FOUND_KEY_END 128
#define CSON_PARSER_FLAG_FOUND_VALUE_START 256
#define CSON_PARSER_FLAG_FOUND_STRING_START 512

typedef enum {
	JSON_PARSER_STATE_IDLE,
	JSON_PARSER_STATE_OBJECT,
	JSON_PARSER_STATE_KEY,
	JSON_PARSER_STATE_ARRAY,
	JSON_PARSER_STATE_STRING,
	JSON_PARSER_STATE_I64,
	JSON_PARSER_STATE_U64,
	JSON_PARSER_STATE_F64,
	JSON_PARSER_STATE_BOOLEAN,
	JSON_PARSER_STATE_NULL,
	JSON_PARSER_STATE_INVALID_CHARACTER,
	JSON_PARSER_STATE_ESCAPE,
	__JSON_PARSER_STATE_MAX = 255
} __attribute__((packed)) json_parser_state_t; 

typedef struct {
	ssize_t state_count, state_size;
	ssize_t key_count, key_size;
	// For parsing doubles
	uint16_t exponent;
	json_parser_state_t *states;
	json_value_t value;
	uint16_t parser_flag;
	ssize_t pointer;
	json_string_t *temporary_keys;
	json_array_t temporaries;
	char buf[BUFFER_SIZE];
} json_parser_t;

int32_t json_parser_init(json_parser_t *const parser);

#endif // CSON_PARSER_H__