#ifndef CSON_PARSER_H__
#define CSON_PARSER_H__

#define BUFFER_SIZE 8192

#include "cson_common.h"

#define CSON_PARSER_FLAG_FOUND_SIGN 1
#define CSON_PARSER_FLAG_FOUND_PERIOD 2
#define CSON_PARSER_FLAG_FOUND_EXPONENT 4
#define CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT 8
#define CSON_PARSER_FLAG_FOUND_NULL_N 16
#define CSON_PARSER_FLAG_FOUND_NULL_U 32
#define CSON_PARSER_FLAG_FOUND_NULL_L1 64
#define CSON_PARSER_FLAG_FOUND_KEY_START 128
#define CSON_PARSER_FLAG_FOUND_KEY_END 256
#define CSON_PARSER_FLAG_FOUND_VALUE_START 512
#define CSON_PARSER_FLAG_FOUND_STRING_START 1024
#define CSON_PARSER_FLAG_FOUND_TRAILING_COMMA 2048

typedef enum {
	CSON_PARSER_STATE_IDLE,
	CSON_PARSER_STATE_OBJECT,
	CSON_PARSER_STATE_KEY,
	CSON_PARSER_STATE_ARRAY,
	CSON_PARSER_STATE_STRING,
	CSON_PARSER_STATE_I64,
	CSON_PARSER_STATE_U64,
	CSON_PARSER_STATE_F64,
	CSON_PARSER_STATE_BOOLEAN,
	CSON_PARSER_STATE_NULL,
	CSON_PARSER_STATE_ESCAPE,
	CSON_PARSER_STATE_EXPECT_END_OR_COMMA,
	CSON_PARSER_STATE_INVALID_CHARACTER,
	__CSON_PARSER_STATE_MAX = 255
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
int32_t json_parse(json_parser_t *const parser, json_value_t *value, const char *const filename);

#endif // CSON_PARSER_H__