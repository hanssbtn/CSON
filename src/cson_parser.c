#include "../include/cson_parser.h"

int32_t json_parser_push_state(json_parser_t *parser, json_parser_state_t state) {
	if (!parser->states) {
		parser->states = debug_calloc(8, sizeof(json_parser_state_t));
		if (!parser->states) {
			fprintf(stderr, LOG_STRING"Failed to allocate memory for states\n", __FILE__, __LINE__);
			return CSON_ERR_ALLOC;
		}
		parser->state_size = 8;
		parser->state_count = 0;
	} 
	if (parser->state_count >= parser->state_size) {
		ssize_t nsz = parser->state_size * 2;
		if (nsz <= 0) return CSON_ERR_MAX_SIZE_REACHED;
		json_parser_state_t *tmp = debug_realloc(parser->states, nsz * sizeof(json_parser_state_t));
		if (!tmp) {
			fprintf(stderr, LOG_STRING"Failed to reallocate memory for state pointer with size %lld bytes\n", __FILE__, __LINE__, nsz * sizeof(json_parser_state_t));
			return CSON_ERR_ALLOC;
		}
		parser->states = tmp;
		parser->state_size;
	}
	parser->states[parser->state_count++] = state;
	return 0;
}

int32_t json_parser_push_temporary(json_parser_t *parser, json_value_t *val) {
	int res = json_array_append_value(&parser->temporaries, val);
	if (res) {
		return res;
	}
	return 0;
}

int32_t json_parser_pop_state(json_parser_t *parser, json_parser_state_t *state) {
	if (!parser->states || parser->state_count <= 0) {
		fprintf(stderr, LOG_STRING"Tried to pop state when empty\n", __FILE__, __LINE__);
		return CSON_ERR_ILLEGAL_OPERATION;
	}
	if (state) *state = parser->states[parser->state_count - 1];
	parser->state_count--; 
	return 0;
}

int32_t json_parser_pop_temporary(json_parser_t *parser, json_value_t *val) {
	int res = json_array_pop(&parser->temporaries, val);
	if (res) {
		return res;
	}
	return 0;
}

int32_t json_parser_handle_digit(json_parser_t *parser, const char ch) {
	if (parser->state_count <= 0) return JSON_PARSER_STATE_INVALID_CHARACTER;
	switch (parser->states[parser->state_count - 1]) {
		case JSON_PARSER_STATE_OBJECT:
		case JSON_PARSER_STATE_ARRAY: {
			json_value_t new_val = {
				.value_type = JSON_OBJECT_TYPE_NUMBER,
				.number.num_type = JSON_NUMBER_TYPE_U64,
				.number.u64 = ch - '0'
			};
			int res = json_parser_push_temporary(parser, &new_val);
			if (res) return res;
			res = json_parser_push_state(parser, JSON_PARSER_STATE_U64);
			if (res) return res;
		} break;
		case JSON_PARSER_STATE_F64: {
			double f = parser->temporaries.objects[parser->temporaries.length - 1].number.f64;
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_PERIOD) {
				double mantissa = ch - '0';
				parser->exponent++;
				for (ssize_t p = 0; p < parser->exponent; ++p) {
					mantissa /= 10;
				}
				if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) f -= mantissa;
				else f += mantissa;
			} else {
				f *= 10;
				if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) f -= ch - '0';
				else f += ch - '0';
			}
			parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
		} break;
		case JSON_PARSER_STATE_U64: {
			uint64_t num = parser->temporaries.objects[parser->temporaries.length - 1].number.u64;
			if (num >= UINT64_MAX / 10 - (ch - '0')) {
				parser->states[parser->state_count - 1] = JSON_PARSER_STATE_F64;
				double f = num;
				f = f * 10 + ch - '0';
				parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
				parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
				break;
			}
			num *= 10;
			num += ch - '0';
			parser->temporaries.objects[parser->temporaries.length - 1].number.u64 = num;
		} break;
		case JSON_PARSER_STATE_I64: {
			int64_t num = parser->temporaries.objects[parser->temporaries.length - 1].number.i64;
			if (num >= INT64_MAX / 10 - (ch - '0') || num <= INT64_MIN / 10 + (ch - '0')) {
				parser->states[parser->state_count - 1] = JSON_PARSER_STATE_F64;
				double f = num;
				f *= 10;
				if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) f -= ch - '0';
				else f += ch - '0';
				parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
				parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
				break;
			}
			num *= 10;
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) num -= ch - '0';
			else num += ch - '0';
			parser->temporaries.objects[parser->temporaries.length - 1].number.i64 = num;
		} break;
		case JSON_PARSER_STATE_STRING: {
			json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		case JSON_PARSER_STATE_KEY: {
			json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		default: {
			return JSON_PARSER_STATE_INVALID_CHARACTER;
		} 
	}
	return 0;
}

int32_t json_parser_handle_char(json_parser_t *parser, json_parser_state_t *current_state, const char ch) {
	if (parser->state_count <= 0) return JSON_PARSER_STATE_INVALID_CHARACTER;
	switch (*current_state) {
		case JSON_OBJECT_TYPE_OBJECT:
		case JSON_OBJECT_TYPE_ARRAY: {
			if (!(parser->parser_flag & (CSON_PARSER_FLAG_FOUND_NULL_N | CSON_PARSER_FLAG_FOUND_NULL_U | CSON_PARSER_FLAG_FOUND_NULL_L1))) {
				if (ch != 'n') {
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return JSON_PARSER_STATE_INVALID_CHARACTER;
				}
				json_parser_push_state(parser, *current_state);
				*current_state = JSON_PARSER_STATE_NULL; 
				parser->parser_flag |= CSON_PARSER_FLAG_FOUND_NULL_N;
				break;
			}
		} break;
		case JSON_PARSER_STATE_KEY: {
			json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		case JSON_PARSER_STATE_STRING: {
			json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		case JSON_PARSER_STATE_NULL: {
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_N)) {
				if (ch != 'u'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return JSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_NULL_N) | CSON_PARSER_FLAG_FOUND_NULL_U;
				break;
			}
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_U)) {
				if (ch != 'l'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return JSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_NULL_U) | CSON_PARSER_FLAG_FOUND_NULL_L1;
				break;
			}
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_L1)) {
				if (ch != 'l'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return JSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag &= ~CSON_PARSER_FLAG_FOUND_NULL_L1;
				printf(LOG_STRING"Found null object.\n", __FILE__, __LINE__);
				json_value_t val = {
					.value_type = JSON_OBJECT_TYPE_NULL,
					.null = (json_null_t){}
				};
				int res = json_parser_push_temporary(parser, &val);
				if (res) {
					fprintf(stderr, "Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
					return res;
				}
				res = json_parser_pop_state(parser, current_state);
				if (res) {
					fprintf(stderr, LOG_STRING"Failed to pop state due to error %d\n", __FILE__, __LINE__, parser->pointer);
					return res;
				}
			}
		} break;
		default: {
			return JSON_PARSER_STATE_INVALID_CHARACTER;
		} 
	}
}

int32_t json_parser_init(json_parser_t *const parser) {
	if (!parser) return CSON_ERR_NULL_PTR;
	parser->states = debug_malloc(8 * sizeof(json_parser_state_t));
	if (!parser->states) return CSON_ERR_ALLOC;
	parser->temporary_keys = debug_calloc(8, sizeof(json_string_t));
	if (!parser->temporary_keys) {
		debug_free(parser->states);
		return CSON_ERR_ALLOC;
	}
	int res = json_array_init(&parser->temporaries, 8); 
	if (res) {
		debug_free(parser->states);
		debug_free(parser->temporary_keys);
		return res;
	}
	parser->state_size = 8;
	parser->state_count = 0;
	parser->key_size = 8;
	parser->key_count = 0;
	parser->exponent = 0;
	memset(parser->buf, 0, BUFFER_SIZE);
	parser->parser_flag = 0;
	parser->value = (json_value_t){};
	return 0;
}

int32_t json_parser_digest(json_parser_t *const parser, ssize_t n) {
	if (!parser || !parser->states || !parser->temporaries.objects || !parser->temporary_keys) return CSON_ERR_NULL_PTR;
	if (n < 0) return CSON_ERR_INVALID_ARGUMENT; 
	json_parser_state_t current_state; 
	if (parser->state_count > 0) {
		json_parser_pop_state(parser, &current_state);	
	} 
	int res = 0;
	for (parser->pointer = 0; parser->pointer < n; ++parser->pointer) {
		char ch = parser->buf[parser->pointer];
		if (isalpha(ch)) json_parser_handle_char(parser, &current_state, ch);
		else if (isdigit(ch)) json_parser_handle_digit(parser, ch);
		else {
			switch (ch) {
				case '\"': {
					switch (current_state) {
						case JSON_PARSER_STATE_ESCAPE: {
							res = json_parser_pop_state(parser, &current_state);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to pop state due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							switch (current_state) {
								case JSON_PARSER_STATE_STRING: {
									json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
									res = json_string_append_char(str, ch);
									if (res) {
										return res;
									}
								} break;
								case JSON_PARSER_STATE_KEY: {
									json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
									res = json_string_append_char(str, ch);
									if (res) {
										return res;
									}
								} break;
								default: {
									fprintf(stderr, "Unreachable state reached.\n");
									exit(-1);
									return JSON_PARSER_STATE_INVALID_CHARACTER;
								} break;
							}
						} break;
						case JSON_OBJECT_TYPE_OBJECT: {
							if (!(parser->parser_flag & (CSON_PARSER_FLAG_FOUND_KEY_START | CSON_PARSER_FLAG_FOUND_KEY_END | CSON_PARSER_FLAG_FOUND_VALUE_START))) {
								res = json_parser_push_key(parser);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push key into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								parser->parser_flag |= CSON_PARSER_FLAG_FOUND_KEY_START;
								current_state = JSON_PARSER_STATE_KEY;
								continue;
							}
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_KEY_END) {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing object values\n", __FILE__, __LINE__, parser->pointer);
								return JSON_PARSER_STATE_INVALID_CHARACTER; 
							}
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START) {
								printf("Pushing string starting at index %lld\n", parser->pointer);
								json_value_t val = {
									.value_type = JSON_OBJECT_TYPE_STRING,
									.string = {
										.buf = NULL
									}
								};
								res = json_parser_push_temporary(parser, &val);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push string into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								res = json_parser_push_state(parser, current_state);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_VALUE_START) | CSON_PARSER_FLAG_FOUND_STRING_START;
								current_state = JSON_PARSER_STATE_STRING;
							}
							res = json_parser_push_state(parser, current_state);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							current_state = JSON_PARSER_STATE_KEY;
						} break;
						case JSON_OBJECT_TYPE_ARRAY: {
							if (!(parser->parser_flag & CSON_PARSER_FLAG_FOUND_STRING_START)) {
								json_value_t val = {
									.value_type = JSON_OBJECT_TYPE_STRING,
									.string = {
										.buf = NULL
									}
								};
								res = json_parser_push_temporary(parser, &val);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push string into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								res = json_parser_push_state(parser, current_state);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								current_state = JSON_PARSER_STATE_STRING;
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing array values\n", __FILE__, __LINE__, parser->pointer);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case JSON_PARSER_STATE_KEY: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_KEY_START) {
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_KEY_START) | CSON_PARSER_FLAG_FOUND_KEY_END;
								res = json_parser_pop_state(parser, &current_state);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to pop state due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								continue;
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing key\n", __FILE__, __LINE__, parser->pointer);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case JSON_PARSER_STATE_STRING: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_STRING_START) {
								json_value_t val;
								res = json_parser_pop_temporary(parser, &val);
								if (res) {
									return res;
								}
								res = json_parser_pop_state(parser, &current_state);
								if (res) {
									return res;
								}
								if (current_state == JSON_PARSER_STATE_ARRAY) {
									if (&parser->temporaries.objects[parser->temporaries.length - 1].value_type != JSON_OBJECT_TYPE_ARRAY) {
										fprintf(stderr, LOG_STRING"Tried to insert value into non array\n", __FILE__, __LINE__);
										return CSON_ERR_ILLEGAL_OPERATION;
									}
									res = json_array_append_value(&parser->temporaries.objects[parser->temporaries.length - 1].array, &val);
									if (res) {
										fprintf(stderr, LOG_STRING"Failed to push string into array due to error %d\n", __FILE__, __LINE__, res);
										return res;
									}
								} else if (current_state == JSON_PARSER_STATE_OBJECT) {
									if (&parser->temporaries.objects[parser->temporaries.length - 1].value_type != JSON_OBJECT_TYPE_OBJECT) {
										fprintf(stderr, LOG_STRING"Tried to insert value into non object\n", __FILE__, __LINE__);
										return CSON_ERR_ILLEGAL_OPERATION;
									}
									json_string_t key;
									res = json_parser_pop_key(parser, &key);
									if (res) {
										fprintf(stderr, LOG_STRING"Failed to pop key due to error %d\n", __FILE__, __LINE__, res);
										return res;
									}
									res = json_object_append_value(&parser->temporaries.objects[parser->temporaries.length - 1].object, &key, &val);
									if (res) {
										fprintf(stderr, LOG_STRING"Failed to push string into object due to error %d\n", __FILE__, __LINE__, res);
										return res;
									}
									json_value_free(&key);
								}
								json_value_free(&val);
								parser->parser_flag &= ~CSON_PARSER_FLAG_FOUND_STRING_START;
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing string\n", __FILE__, __LINE__, parser->pointer);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing (state: %d)\n", __FILE__, __LINE__, parser->pointer, current_state);
							return JSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '-': {
					switch (current_state) {
						case JSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_ARRAY: 
						case JSON_PARSER_STATE_OBJECT: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) {
								fprintf(stderr, LOG_STRING"Found duplicate \'-\' characters at index %lld when parsing number.\n", parser->pointer, __FILE__, __LINE__);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
							printf(LOG_STRING"Found sign at index %lld, converting number to negative.\n", __FILE__, __LINE__, parser->pointer);
							int res = json_parser_push_state(parser, JSON_PARSER_STATE_I64);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							json_value_t val = {
								.value_type = JSON_OBJECT_TYPE_NUMBER,
								.number = {
									.num_type = JSON_NUMBER_TYPE_I64,
									.i64 = 0
								}
							};
							res = json_parser_push_temporary(parser, &val);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_SIGN;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'-\' at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return JSON_PARSER_STATE_INVALID_CHARACTER;
						}	
					}
				} break;
				case '.': {
					switch (current_state) {
						case JSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_I64: 
						case JSON_PARSER_STATE_U64: {
							printf(LOG_STRING"Found period at index %lld, converting number to double.\n", __FILE__, __LINE__, parser->pointer);
							parser->states[parser->state_count - 1] = JSON_PARSER_STATE_F64;
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_PERIOD;
							double f = parser->temporaries.objects[parser->temporaries.length - 1].number.u64;
							parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
							parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
						} break;
						case JSON_PARSER_STATE_F64: {
							if (parser->parser_flag | CSON_PARSER_FLAG_FOUND_PERIOD) {
								fprintf(stderr, LOG_STRING"Found duplicate \'.\' characters at index %lld when parsing double.\n", parser->pointer, __FILE__, __LINE__);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
							printf(LOG_STRING"Found period at index %lld, setting period flag.\n", __FILE__, __LINE__, parser->pointer);
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_PERIOD;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'.\' at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return JSON_PARSER_STATE_INVALID_CHARACTER;
						}	
					}
				} break;
				case ':': {
					
				} break;
				case '{': {
					switch (current_state) {
						case JSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case JSON_PARSER_STATE_OBJECT: {
							if (parser->temporary_keys[parser->key_count - 1].length > 0) {
								json_value_t val = {};
								res = json_parser_push_temporary(parser, &val);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
							} else {
								fprintf(stderr, LOG_STRING"Found \'{\' at index %lld without key\n", __FILE__, __LINE__, parser->pointer);
								return JSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case JSON_PARSER_STATE_ARRAY: {
							
						} break;
						default: {
							return JSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case '}': {
				
				} break;
				case '[': {
				} break;
				case ']': {
					
				} break;
				case ',': {
				
				} break;
				case '\\': {
					switch (current_state) {
						case JSON_PARSER_STATE_STRING: 
						case JSON_PARSER_STATE_KEY: {
							printf("Found \'\\\' at index %lld, entering escape mode\n", parser->pointer);
							res = json_parser_push_state(parser, JSON_PARSER_STATE_ESCAPE);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid escape character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return JSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case ' ': {
					
				} break;
				case '\r': {
					
				} break;
				case '\t': {
					
				} break;
				case '\0': {
					
				} break;
				case '\n': {
					
				} break;
				default: {
				
				} break;
			}
		} 
	}
	return 0;
}

int32_t json_parse(json_parser_t *const parser, json_value_t *value, const char *const filename) {
	if (!filename || !parser) return CSON_ERR_NULL_PTR;
	FILE *file = fopen64(filename, "r");
	if (!file) return errno;
	int res = 0;
	ssize_t n = fread(parser->buf, sizeof(char), BUFFER_SIZE, file); 
	while (n) {
		res = json_parser_digest(parser, n);
		if (res != CSON_EXPECT) goto cleanup;
		fread(parser->buf, sizeof(char), BUFFER_SIZE, file); 
	}
	res = json_parser_finalize(value);
	cleanup:
		fclose(file);
	return res;
}