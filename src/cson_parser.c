#include "../include/cson_parser.h"

const char *get_state_name(json_parser_state_t state) {
	switch (state) {
		case CSON_PARSER_STATE_IDLE: {
			return "IDLE";
		}
		case CSON_PARSER_STATE_OBJECT: {
			return "OBJECT";
		}
		case CSON_PARSER_STATE_KEY: {
			return "KEY";
		}
		case CSON_PARSER_STATE_ARRAY: {
			return "ARRAY";
		}
		case CSON_PARSER_STATE_STRING: {
			return "STRING";
		}
		case CSON_PARSER_STATE_I64: {
			return "I64";
		}
		case CSON_PARSER_STATE_U64: {
			return "U64";
		}
		case CSON_PARSER_STATE_F64: {
			return "F64";
		}
		case CSON_PARSER_STATE_BOOLEAN: {
			return "BOOLEAN";
		}
		case CSON_PARSER_STATE_NULL: {
			return "NULL";
		}
		case CSON_PARSER_STATE_INVALID_CHARACTER: {
			return "INVALID CHARACTER";
		}
		case CSON_PARSER_STATE_ESCAPE: {
			return "ESCAPE";
		}
		case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {
			return "EXPECT";
		}
		default: break;
	}
	return "INVALID STATE";
}

void json_parser_print_state(const json_parser_t *const parser) {
	for (ssize_t i = 0; i < parser->state_count; ++i) {
		printf("%s\n", get_state_name(parser->states[i]));
	}
}

int32_t json_parser_push_key(json_parser_t *parser) {
	if (parser->key_count == parser->key_size) {
		ssize_t nsz = parser->key_size * 2;
		json_string_t *tmp = realloc(parser->temporary_keys, nsz * sizeof(json_string_t));
		if (!tmp) {
			fprintf(stderr, LOG_STRING"Failed to allocate memory for keys\n", __FILE__, __LINE__);
			return CSON_ERR_ALLOC;
		}
		parser->temporary_keys = tmp;
		parser->key_size =  nsz;
	}
	char *tmp = debug_malloc(8 * sizeof(char));
	if (!tmp) {
		fprintf(stderr, LOG_STRING"Failed to allocate memory for new key\n", __FILE__, __LINE__);
		return CSON_ERR_ALLOC;
	}
	// printf("parser->key_count: %lld, size: %lld\n", parser->key_count, parser->key_size);
	parser->temporary_keys[parser->key_count] = (json_string_t){
		.buf = tmp,
		.size = 8
	};
	parser->key_count++;
	return 0;
}

int32_t json_parser_pop_key(json_parser_t *parser, json_string_t *key) {
	if (parser->key_count <= 0) {
		fprintf(stderr, LOG_STRING"Tried to pop key when the array is empty\n", __FILE__, __LINE__);
		return CSON_ERR_ILLEGAL_OPERATION;
	}
	if (key) *key = parser->temporary_keys[parser->key_count - 1];
	else json_string_free(&parser->temporary_keys[parser->key_count - 1]);
	parser->key_count--;
	return 0;
}



int32_t json_parser_push_state(json_parser_t *parser, json_parser_state_t state) { 
	if (parser->state_count >= parser->state_size) {
		ssize_t nsz = parser->state_size * 2;
		json_parser_state_t *tmp = debug_realloc(parser->states, nsz * sizeof(json_parser_state_t));
		if (!tmp) {
			fprintf(stderr, LOG_STRING"Failed to reallocate memory for state pointer with size %lld bytes\n", __FILE__, __LINE__, nsz * sizeof(json_parser_state_t));
			return CSON_ERR_ALLOC;
		}
		parser->states = tmp;
		parser->state_size = nsz;
	}
	parser->states[parser->state_count++] = state;
	printf("current stack:\n");
	json_parser_print_state(parser);
	return 0;
}

int32_t json_parser_push_temporary(json_parser_t *parser, const json_value_t *const val, bool copy) {
	int res = 0;
	if (copy) json_array_append_value(&parser->temporaries, val);
	else json_array_move_value(&parser->temporaries, val);
	if (res) {
		return res;
	}
	return 0;
}

int32_t json_parser_pop_state(json_parser_t *parser,	json_parser_state_t *state) {
	if (!parser->states || parser->state_count <= 0) {
		fprintf(stderr, LOG_STRING"Tried to pop state when the array is empty\n", __FILE__, __LINE__);
		return CSON_ERR_ILLEGAL_OPERATION;
	}
	if (state) *state = parser->states[parser->state_count - 1];
	parser->state_count--; 
	printf("current stack:\n");
	json_parser_print_state(parser);
	return 0;
}

int32_t json_parser_pop_temporary(json_parser_t *parser, json_value_t *val) {
	int res = json_array_pop(&parser->temporaries, val);
	if (res) {
		return res;
	}
	return 0;
}

int32_t json_parser_handle_digit(json_parser_t *parser, json_parser_state_t *current_state, const char ch) {
	if (parser->state_count <= 0) return CSON_PARSER_STATE_INVALID_CHARACTER;
	switch (*current_state) {
		 case CSON_PARSER_STATE_OBJECT: {
			if (!(parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START)) {
				return CSON_PARSER_STATE_INVALID_CHARACTER;
			}
		} __attribute__((fallthrough));
		case CSON_PARSER_STATE_ARRAY: {
			json_value_t new_val = {
				.value_type = JSON_OBJECT_TYPE_NUMBER,
				.number.num_type = JSON_NUMBER_TYPE_U64,
				.number.u64 = ch - '0'
			};
			int res = json_parser_push_temporary(parser, &new_val, true);
			if (res) return res;
			printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
			res = json_parser_push_state(parser, *current_state);
			*current_state = CSON_PARSER_STATE_U64;
			parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_VALUE_START | CSON_PARSER_FLAG_FOUND_TRAILING_COMMA);
			if (res) return res;
		} break;
		case CSON_PARSER_STATE_F64: {
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_EXPONENT) {
				parser->exponent = parser->exponent * 10 + (ch - '0');
				parser->found_number_after_exponent = true;
				break;
			} 
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT) {
				parser->exponent = parser->exponent * 10 - (ch - '0');
				parser->found_number_after_exponent = true;
				break;
			}
			double f = parser->temporaries.objects[parser->temporaries.length - 1].number.f64;
			// printf("f: %e, digit: %c\n", f, ch);
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_PERIOD) {
				double mantissa = ch - '0';
				parser->exponent++;
				// printf("exponent: %hd\n", parser->exponent);
				for (ssize_t p = 0; p < parser->exponent; ++p) {
					mantissa /= 10;
				}
				if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) {
					f -= mantissa;
					parser->found_number_after_sign = true;
				}
				else f += mantissa;
				parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
				parser->found_number_after_period = true;
				break;
			} 
			f *= 10;
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) f -= ch - '0';
			else f += ch - '0';
			parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
		} break;
		case CSON_PARSER_STATE_U64: {
			uint64_t num = parser->temporaries.objects[parser->temporaries.length - 1].number.u64;
			if (num >= UINT64_MAX / 10 - (ch - '0')) {
				*current_state = CSON_PARSER_STATE_F64;
				double f = num;
				f = f * 10 + ch - '0';
				parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
				parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
				break;
			}
			num *= 10;
			num += ch - '0';
			parser->temporaries.objects[parser->temporaries.length - 1].number.u64 = num;
			parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_TRAILING_COMMA);
		} break;
		case CSON_PARSER_STATE_I64: {
			int64_t num = parser->temporaries.objects[parser->temporaries.length - 1].number.i64;
			// printf("num: %lld, digit: %c\n", num, ch);
			if (num >= INT64_MAX / 10 - (ch - '0') || num <= INT64_MIN / 10 + (ch - '0')) {
				*current_state = CSON_PARSER_STATE_F64;
				double f = num;
				f *= 10;
				if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) {
					f -= ch - '0';
					parser->found_number_after_sign = true;
				} else f += ch - '0';
				parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
				parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
				break;
			}
			num *= 10;
			if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) {
				num -= ch - '0';
				parser->found_number_after_sign = true;
			}
			else num += ch - '0';
			parser->temporaries.objects[parser->temporaries.length - 1].number.i64 = num;
			parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_TRAILING_COMMA);
		} break;
		case CSON_PARSER_STATE_STRING: {
			json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		case CSON_PARSER_STATE_KEY: {
			json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
			int res = json_string_append_char(str, ch);
			if (res) return res;
		} break;
		default: {
			return CSON_PARSER_STATE_INVALID_CHARACTER;
		} 
	}
	return 0;
}

int32_t json_parser_handle_char(json_parser_t *parser, json_parser_state_t *current_state, const char ch) {
	if (parser->state_count <= 0) return CSON_PARSER_STATE_INVALID_CHARACTER;
	switch (*current_state) {
		case CSON_PARSER_STATE_I64: {
			if ((ch != 'e' && ch != 'E') || parser->temporaries.objects[parser->temporaries.length - 1].number.i64 == 0) {
				fprintf(stderr, LOG_STRING"Invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
				exit(-1);
			}
			double f = parser->temporaries.objects[parser->temporaries.length - 1].number.i64;
			parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
			parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
			printf("Found exponent at index %lld\n", parser->pointer);
			parser->parser_flag |= CSON_PARSER_FLAG_FOUND_EXPONENT;
			parser->exponent = 0;
			*current_state = CSON_PARSER_STATE_F64;
		} break;
		case CSON_PARSER_STATE_U64: {
			if ((ch != 'e' && ch != 'E') || parser->temporaries.objects[parser->temporaries.length - 1].number.u64 == 0) {
				fprintf(stderr, LOG_STRING"Invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
				exit(-1);
			}
			double f = parser->temporaries.objects[parser->temporaries.length - 1].number.u64;
			parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
			parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
			printf("Found exponent at index %lld\n", parser->pointer);
			parser->parser_flag |= CSON_PARSER_FLAG_FOUND_EXPONENT;
			parser->exponent = 0;
			*current_state = CSON_PARSER_STATE_F64;
		} break;
		case CSON_PARSER_STATE_F64: {
			if (ch != 'e' && ch != 'E') {
				fprintf(stderr, LOG_STRING"Invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
				exit(-1);
			}
			if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT)) {
				fprintf(stderr, LOG_STRING"Found duplicate exponent %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
				exit(-1);
			}
			printf("Found exponent at index %lld\n", parser->pointer);
			parser->exponent = 0;
			parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_PERIOD) | CSON_PARSER_FLAG_FOUND_EXPONENT;
		} break;
		case CSON_PARSER_STATE_OBJECT:
		case CSON_PARSER_STATE_ARRAY: {
			if (!(parser->parser_flag & (CSON_PARSER_FLAG_FOUND_NULL_N | CSON_PARSER_FLAG_FOUND_NULL_U | CSON_PARSER_FLAG_FOUND_NULL_L1))) {
				if (ch != 'n') {
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				}
				printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
				json_parser_push_state(parser, *current_state);
				*current_state = CSON_PARSER_STATE_NULL; 
				parser->parser_flag |= CSON_PARSER_FLAG_FOUND_NULL_N;
				break;
			}
		} break;
		case CSON_PARSER_STATE_ESCAPE: {
			switch (ch) {
				case 'r': {
					switch (parser->states[parser->state_count - 1]) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, '\r');
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, '\r');
							if (res) return res;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"invalid state.\n", __FILE__, __LINE__);
							exit(-1);
						}
					}
				} break;
				case 'n': {
					switch (parser->states[parser->state_count - 1]) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, '\n');
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, '\n');
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"invalid state.\n", __FILE__, __LINE__);
							exit(-1);
						}
					}
				} break;
				case 't': {
					switch (parser->states[parser->state_count - 1]) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, '\t');
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, '\t');
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"invalid state.\n", __FILE__, __LINE__);
							exit(-1);
						}
					}
				} break;
				default: {
					fprintf(stderr, "Invalid escape character \\%c\n", ch);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				} break;
			}
			printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
			json_parser_pop_state(parser, current_state);
		} break;
		case CSON_PARSER_STATE_KEY: {
			json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
			int res = json_string_append_char(str, ch);
			if (res) {
				fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
				return res;
			}
		} break;
		case CSON_PARSER_STATE_STRING: {
			json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
			int res = json_string_append_char(str, ch);
			if (res) {
				fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
				return res;
			}
		} break;
		case CSON_PARSER_STATE_NULL: {
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_N)) {
				if (ch != 'u'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_NULL_N) | CSON_PARSER_FLAG_FOUND_NULL_U;
				break;
			}
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_U)) {
				if (ch != 'l'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_NULL_U) | CSON_PARSER_FLAG_FOUND_NULL_L1;
				break;
			}
			if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_NULL_L1)) {
				if (ch != 'l'){
					fprintf(stderr, LOG_STRING"Found illegal character \'%c\' at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				}
				parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_VALUE_START |CSON_PARSER_FLAG_FOUND_NULL_L1 | CSON_PARSER_FLAG_FOUND_TRAILING_COMMA);
				// printf(LOG_STRING"Found null object.\n", __FILE__, __LINE__);
				json_value_t val = {
					.value_type = JSON_OBJECT_TYPE_NULL,
					.null = (json_null_t){}
				};
				int res = json_parser_push_temporary(parser, &val, false);
				if (res) {
					fprintf(stderr, LOG_STRING"Failed to push null object into parser temporary due to error %d\n", __FILE__, __LINE__, res);
					return res;
				}
				*current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
				printf("Expecting comma or end\n");
			}
		} break;
		default: {
			return CSON_PARSER_STATE_INVALID_CHARACTER;
		} 
	}
	return 0;
}

int32_t validate_number(json_parser_t *parser) {
	switch (parser->temporaries.objects[parser->temporaries.length - 1].number.num_type) {
		case JSON_NUMBER_TYPE_I64: {
			if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_SIGN) && !parser->found_number_after_sign) {
				fprintf(stderr, LOG_STRING"Found invalid sign symbol (no number after \'-\')\n", __FILE__, __LINE__);
				return CSON_PARSER_STATE_INVALID_CHARACTER;
			}
		} break;
		case JSON_NUMBER_TYPE_F64: {
			if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT) && !parser->found_number_after_exponent) {
				fprintf(stderr, LOG_STRING"Found invalid exponent symbol (no number after \'e\')\n", __FILE__, __LINE__);
				return CSON_PARSER_STATE_INVALID_CHARACTER;
			}
			if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_PERIOD) && !parser->found_number_after_period) {
				if (!parser->exponent) {
					fprintf(stderr, LOG_STRING"Found invalid period symbol (no number after \'.\')\n", __FILE__, __LINE__);
					return CSON_PARSER_STATE_INVALID_CHARACTER;
				}
			}
			if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_SIGN) && !parser->found_number_after_sign) {
				fprintf(stderr, LOG_STRING"Found invalid sign symbol (no number after \'-\')\n", __FILE__, __LINE__);
				return CSON_PARSER_STATE_INVALID_CHARACTER;
			}
			
			parser->found_number_after_period = false;
			double f = parser->temporaries.objects[parser->temporaries.length - 1].number.f64;
			// printf("exponent: %hd\n", parser->exponent);
			if (parser->found_number_after_exponent) {
				if (parser->exponent < 0) {
					while (parser->exponent + 100 < -100) {
						f /= 1e100;
						parser->exponent += 100;
					}
					while (parser->exponent + 10 < -10) {
						f /= 1e10;
						parser->exponent += 10;
					}
					while (parser->exponent < 0) {
						f /= 10;
						parser->exponent++;
					}
				} else if (parser->exponent > 0) {
					while (parser->exponent - 100 > 100) {
						f *= 1e100;
						parser->exponent -= 100;
					}
					while (parser->exponent - 10 > 10) {
						f *= 1e10;
						parser->exponent -= 10;
					}
					while (parser->exponent > 0) {
						f *= 10;
						parser->exponent--;
					}
				}
			}
			parser->found_number_after_sign = false;
			parser->found_number_after_exponent = false;
			parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
			parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_SIGN | CSON_PARSER_FLAG_FOUND_PERIOD | CSON_PARSER_FLAG_FOUND_VALUE_START | CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT);
		} break;
		default: {} break;
	}
	return 0;
}

int32_t json_parser_push_depth(json_parser_t *const parser) {
	if (parser->depth_count == parser->depth_size) {
		ssize_t nsz = parser->depth_size * 2;
		ssize_t *tmp = realloc(parser->depth, nsz * sizeof(ssize_t));
		if (!tmp) {
			fprintf(stderr, LOG_STRING"Failed to allocate memory for depth\n", __FILE__, __LINE__);
			return CSON_ERR_ALLOC;
		}
		parser->depth = tmp;
		parser->depth_size =  nsz;
	}
	parser->depth[parser->depth_count] = parser->temporaries.length;
	parser->depth_count++;
	return 0;
}

int32_t json_parser_pop_depth(json_parser_t *const parser, ssize_t *ptr) {
	if (parser->depth_count <= 0) return CSON_ERR_ILLEGAL_OPERATION;
	if (ptr) *ptr = parser->depth[parser->depth_count - 1];
	parser->depth_count--;
	return 0;
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
	parser->depth = debug_malloc(sizeof(ssize_t) * 8);
	if (!parser->depth) {
		debug_free(parser->states);
		debug_free(parser->temporary_keys);
		debug_free(parser->temporaries.objects);
		return res;
	}
	parser->depth_count = 0;
	parser->depth_size = 8;
	parser->state_size = 8;
	parser->state_count = 0;
	parser->key_size = 8;
	parser->key_count = 0;
	parser->found_number_after_period = false;
	parser->found_number_after_exponent = false;
	parser->exponent = 0;
	memset(parser->buf, 0, BUFFER_SIZE);
	parser->parser_flag = 0;
	parser->value = (json_value_t){};
	return 0;
}

void json_parser_flags_printf(uint16_t flags) {
	printf("--------------------------\n");
	if (flags & CSON_PARSER_FLAG_FOUND_SIGN) {
		printf("FOUND_SIGN\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_PERIOD) {
		printf("FOUND_PERIOD\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_EXPONENT) {
		printf("FOUND_EXPONENT\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT) {
		printf("FOUND_NEGATIVE_EXPONENT\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_NULL_N) {
		printf("FOUND_NULL_N\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_NULL_U) {
		printf("FOUND_NULL_U\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_NULL_L1) {
		printf("FOUND_NULL_L1\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_KEY_START) {
		printf("FOUND_KEY_START\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_KEY_END) {
		printf("FOUND_KEY_END\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_VALUE_START) {
		printf("FOUND_VALUE_START\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_STRING_START) {
		printf("FOUND_STRING_START\n");
	}
	if (flags & CSON_PARSER_FLAG_FOUND_TRAILING_COMMA) {
		printf("FOUND_TRAILING_COMMA\n");
	}
	printf("--------------------------\n");
}

int32_t json_parser_digest(json_parser_t *const parser, ssize_t n) {
	if (!parser || !parser->states || !parser->temporaries.objects || !parser->temporary_keys) return CSON_ERR_NULL_PTR;
	if (n < 0) return CSON_ERR_INVALID_ARGUMENT; 
	json_parser_state_t current_state = CSON_PARSER_STATE_IDLE; 
	if (parser->state_count > 0) {
		printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
		json_parser_pop_state(parser, &current_state);	
	} 
	int res = 0;
	for (parser->pointer = 0; parser->pointer < n; ++parser->pointer) {
		printf("state stack:\n");
		json_parser_print_state(parser);
		printf("current state: %s\n", get_state_name(current_state));
		char ch = parser->buf[parser->pointer];
		printf("current char \"%c\" at index %lld\n", ch, parser->pointer);
		printf("flags:\n");
		json_parser_flags_printf(parser->parser_flag);
		if (isalpha(ch)) {
			res = json_parser_handle_char(parser, &current_state, ch);
			if (res) {
				printf(LOG_STRING"got error %d\n", __FILE__, __LINE__, res);
				return res;
			}
		}
		else if (isdigit(ch)) {
			res = json_parser_handle_digit(parser, &current_state, ch);
			if (res) {
				printf(LOG_STRING"got error %d\n", __FILE__, __LINE__, res);
				return res;
			}
		}
		else {
			switch (ch) {
				case '\"': {
					switch (current_state) {
						case CSON_PARSER_STATE_ESCAPE: {
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							res = json_parser_pop_state(parser, &current_state);
							printf("Resulting state: %s\n", get_state_name(current_state));
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to pop state due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							switch (current_state) {
								case CSON_PARSER_STATE_STRING: {
									json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
									res = json_string_append_char(str, ch);
									if (res) {
										return res;
									}
								} break;
								case CSON_PARSER_STATE_KEY: {
									json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
									res = json_string_append_char(str, ch);
									if (res) {
										return res;
									}
								} break;
								default: {
									fprintf(stderr, "Unreachable state reached.\n");
									exit(-1);
									return CSON_PARSER_STATE_INVALID_CHARACTER;
								} break;
							}
						} break;
						case CSON_PARSER_STATE_OBJECT: {
							if (!(parser->parser_flag & (CSON_PARSER_FLAG_FOUND_KEY_START | CSON_PARSER_FLAG_FOUND_KEY_END | CSON_PARSER_FLAG_FOUND_VALUE_START))) {
								printf("Pushing key starting at index %lld\n", parser->pointer);
								res = json_parser_push_key(parser);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push key into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_TRAILING_COMMA) | CSON_PARSER_FLAG_FOUND_KEY_START;
								printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
								res = json_parser_push_state(parser, current_state);
								if (res) {
									return res;
								}

								current_state = CSON_PARSER_STATE_KEY;
								continue;
							}
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_KEY_END) {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing object values\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER; 
							}
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START) {
								printf("Pushing string starting at index %lld\n", parser->pointer);
								json_value_t val = {
									.value_type = JSON_OBJECT_TYPE_STRING,
									.string = {
										.length = 0,
										.size = 0,
										.buf = NULL
									}
								};
								res = json_parser_push_temporary(parser, &val, false);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push string into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
								res = json_parser_push_state(parser, current_state);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_VALUE_START) | CSON_PARSER_FLAG_FOUND_STRING_START;
								current_state = CSON_PARSER_STATE_STRING;
								continue;
							}
						} break;
						case CSON_PARSER_STATE_ARRAY: {
							parser->parser_flag &= ~CSON_PARSER_FLAG_FOUND_TRAILING_COMMA;
							if (!(parser->parser_flag & CSON_PARSER_FLAG_FOUND_STRING_START)) {
								json_value_t val = {
									.value_type = JSON_OBJECT_TYPE_STRING,
									.string = {
										.buf = NULL
									}
								};
								res = json_parser_push_temporary(parser, &val, false);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push string into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
								res = json_parser_push_state(parser, current_state);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								current_state = CSON_PARSER_STATE_STRING;
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_VALUE_START) | CSON_PARSER_FLAG_FOUND_STRING_START;
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing array values\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case CSON_PARSER_STATE_KEY: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_KEY_START) {
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_KEY_START) | CSON_PARSER_FLAG_FOUND_KEY_END;
								printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
								res = json_parser_pop_state(parser, &current_state);
								if (res) {
									return res;
								}
								continue;
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing key\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case CSON_PARSER_STATE_STRING: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_STRING_START) {
								current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
								printf("Expecting comma or end\n");
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_STRING_START);
							} else {
								fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing string\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'\"\' at index %lld when parsing (state: %d)\n", __FILE__, __LINE__, parser->pointer, current_state);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '-': {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_OBJECT: {
							if (!(parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START)) {
								fprintf(stderr, "Found illegal \'-\' at index %lld\n", parser->pointer);
								exit(-1);
							}
							json_value_t val = {
								.value_type = JSON_OBJECT_TYPE_NUMBER,
								.number = {
									.num_type = JSON_NUMBER_TYPE_I64,
									.i64 = 0
								}
							};
							parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_VALUE_START) | CSON_PARSER_FLAG_FOUND_SIGN;
							json_parser_push_temporary(parser, &val, false);
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							json_parser_push_state(parser, current_state);
							current_state = CSON_PARSER_STATE_I64;
						} break;
						case CSON_PARSER_STATE_ARRAY: {
							json_value_t val = {
								.value_type = JSON_OBJECT_TYPE_NUMBER,
								.number = {
									.num_type = JSON_NUMBER_TYPE_I64,
									.i64 = 0
								}
							};
							parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_VALUE_START) | CSON_PARSER_FLAG_FOUND_SIGN;
							json_parser_push_temporary(parser, &val, false);
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							json_parser_push_state(parser, current_state);
							current_state = CSON_PARSER_STATE_I64;
						} break;
						case CSON_PARSER_STATE_F64: {
							json_parser_flags_printf(parser->parser_flag);
							// Parse '-' after 'e'
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_SIGN) {
								if ((parser->parser_flag & (CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT)) == CSON_PARSER_FLAG_FOUND_EXPONENT && !parser->found_number_after_exponent) {
									printf("Found negative sign after exponent.\n");
									parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_EXPONENT) | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT;
									break;
								} else {
									fprintf(stderr, LOG_STRING"Found duplicate \'-\' characters at index %lld when parsing number.\n", __FILE__, __LINE__, parser->pointer);
									return CSON_PARSER_STATE_INVALID_CHARACTER;
								}
							} else {
								if ((parser->parser_flag & CSON_PARSER_FLAG_FOUND_EXPONENT) && !parser->found_number_after_exponent) {
									printf("Found negative sign after exponent.\n");
									parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_EXPONENT) | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT;
									break;
								} else {
									fprintf(stderr, LOG_STRING"Found invalid \'-\' characters at index %lld when parsing number.\n", __FILE__, __LINE__, parser->pointer);
									return CSON_PARSER_STATE_INVALID_CHARACTER;
								}
							}
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'-\' at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}	
					}
				} break;
				case '.': {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							int res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_I64: {
							printf(LOG_STRING"Found period at index %lld, converting number to double.\n", __FILE__, __LINE__, parser->pointer);
							current_state = CSON_PARSER_STATE_F64;
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_PERIOD;
							parser->exponent = 0;
							double f = parser->temporaries.objects[parser->temporaries.length - 1].number.i64;
							parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
							parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
						} break;
						case CSON_PARSER_STATE_U64: {
							printf(LOG_STRING"Found period at index %lld, converting number to double.\n", __FILE__, __LINE__, parser->pointer);
							current_state = CSON_PARSER_STATE_F64;
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_PERIOD;
							parser->exponent = 0;
							double f = parser->temporaries.objects[parser->temporaries.length - 1].number.u64;
							parser->temporaries.objects[parser->temporaries.length - 1].number.f64 = f;
							parser->temporaries.objects[parser->temporaries.length - 1].number.num_type = JSON_NUMBER_TYPE_F64;
						} break;
						case CSON_PARSER_STATE_F64: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_PERIOD) {
								fprintf(stderr, LOG_STRING"Found duplicate \'.\' characters at index %lld when parsing double.\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
							if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT)) {
								fprintf(stderr, LOG_STRING"Found period after exponent symbol at index %lld when parsing double\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
							printf(LOG_STRING"Found period at index %lld, setting period flag.\n", __FILE__, __LINE__, parser->pointer);
							parser->exponent = 0;
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_PERIOD;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character \'.\' at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}	
					}
				} break;
				case ':': {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_OBJECT: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_KEY_END) {
								parser->parser_flag = (parser->parser_flag & ~CSON_PARSER_FLAG_FOUND_KEY_END) | CSON_PARSER_FLAG_FOUND_VALUE_START;
								break;
							} else {
								fprintf(stderr, LOG_STRING"Found unaccounted \':\' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}

						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \':\' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							fprintf(stderr, "current state: %s\n", get_state_name(current_state));
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '{': {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_IDLE: {
							json_object_t *object = debug_malloc(sizeof(json_object_t));
							if (!object) {
								fprintf(stderr, LOG_STRING"Failed to initialize first object\n", __FILE__, __LINE__);
								return CSON_ERR_ALLOC;
							}
							res = json_object_init(object, 8);
							if (res) {
								return res;
							}
							json_value_t val = {
								.object = object
							};
							parser->value = val;
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							res = json_parser_push_state(parser, current_state);
							if (res) {
								return res;
							}
							current_state = CSON_PARSER_STATE_OBJECT;
						} break;
						case CSON_PARSER_STATE_OBJECT: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START) {
								json_object_t *object = debug_malloc(sizeof(json_object_t));
								if (!object) {
									fprintf(stderr, LOG_STRING"Failed to initialize first object\n", __FILE__, __LINE__);
									return CSON_ERR_ALLOC;
								}
								res = json_object_init(object, 8);
								if (res) {
									return res;
								}
								res = json_parser_push_depth(parser);
								if (res) {
									return res;
								}
								// printf("depth:\n");
								// for (ssize_t j = 0; j < parser->depth_count; ++j) {
								// 	printf("%lld ", parser->depth[j]);
								// }
								// printf("\n");
								json_value_t val = {
									.object = object
								};
								res = json_parser_push_temporary(parser, &val, false);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								parser->parser_flag &= ~CSON_PARSER_FLAG_FOUND_VALUE_START;
								printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
								res = json_parser_push_state(parser, current_state);
								if (res) {
									return res;
								}
								current_state = CSON_PARSER_STATE_OBJECT;
								break;
							} else {
								fprintf(stderr, LOG_STRING"Found \'{\' at index %lld without key\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case CSON_PARSER_STATE_ARRAY: {
							res = json_parser_push_depth(parser);
							if (res) {
								return res;
							}
							json_object_t *object = debug_malloc(sizeof(json_object_t));
							if (!object) {
								fprintf(stderr, LOG_STRING"Failed to initialize first object\n", __FILE__, __LINE__);
								return CSON_ERR_ALLOC;
							}
							res = json_object_init(object, 8);
							if (res) {
								return res;
							}
							json_value_t val = {
								.object = object
							};
							res = json_parser_push_temporary(parser, &val, false);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							parser->parser_flag &= ~CSON_PARSER_FLAG_FOUND_VALUE_START;
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							res = json_parser_push_state(parser, current_state);
							if (res) {
								return res;
							}
							current_state = CSON_PARSER_STATE_OBJECT;
						} break;

						default: {
							fprintf(stderr, LOG_STRING"Found invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case '}': {
					if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_TRAILING_COMMA) {
						fprintf(stderr, LOG_STRING"Found trailing comma\n", __FILE__, __LINE__);
						return CSON_PARSER_STATE_INVALID_CHARACTER;
					}
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							json_parser_pop_state(parser, NULL);
						} __attribute__((fallthrough));
						case CSON_PARSER_STATE_OBJECT: 
						{
							ssize_t index = -1;
							printf("keys:\n");
							for (ssize_t i = 0; i < parser->key_count; ++i) {
								printf("i: %lld, ", i);
								json_string_printf(&parser->temporary_keys[i]);
								printf("\n");
							}
							printf("temporaries:\n");
							for (ssize_t i = 0; i < parser->temporaries.length; ++i) {
								printf("i: %lld, ", i);
								json_value_printf(&parser->temporaries.objects[i], 0, true);
								printf("\n");
							}
							printf("Depth:\n");
							for (ssize_t j = 0; j < parser->depth_count; ++j) {
								printf("%lld ", parser->depth[j]);
							}
							printf("\n");
							if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_VALUE_START | CSON_PARSER_FLAG_FOUND_STRING_START | CSON_PARSER_FLAG_FOUND_KEY_END | CSON_PARSER_FLAG_FOUND_KEY_START)) {
								json_parser_flags_printf(parser->parser_flag);
								printf(LOG_STRING"Found unterminated key\n", __FILE__, __LINE__);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
							
							json_parser_pop_depth(parser, &index);
							if (index >= 0) {
								printf("index: %lld, type: %d\n", index, parser->temporaries.objects[index].value_type);
								if (parser->temporaries.objects[index].value_type != JSON_OBJECT_TYPE_OBJECT) {
									fprintf(stderr, LOG_STRING"Found illegal \'}\' on index %lld\n", __FILE__, __LINE__, index);
									return CSON_PARSER_STATE_INVALID_CHARACTER;
								}
								if (parser->temporaries.length > 0 
									&& parser->temporaries.objects[parser->temporaries.length - 1].value_type == JSON_OBJECT_TYPE_NUMBER
								) {
									res = validate_number(parser);
									if (res) {
										printf(LOG_STRING"number invalidated\n", __FILE__, __LINE__);
										return res;
									}
								} 
								json_object_t *obj = parser->temporaries.objects[index].object;
								ssize_t j, k;
								ssize_t new_key_length = parser->key_count - (parser->temporaries.length - index) + 1;
								printf("new_key_length: %lld\n", new_key_length);
								assert(new_key_length >= 0);
								for (j = index + 1, k = new_key_length; k < parser->key_count && j < parser->temporaries.length; ++j, ++k) {
									// printf("moving key to object\n");
									// json_string_printf(&parser->temporary_keys[k]);
									// printf("\nmoving value to object\n");
									// json_value_printf(&parser->temporaries.objects[j], 0, true);
									// printf("\n");
									json_object_move_value(obj, &parser->temporary_keys[k], &parser->temporaries.objects[j]);
									parser->temporary_keys[k] = (json_string_t){};
									parser->temporaries.objects[j] = (json_value_t){};
								}
								printf("new length: %lld\n", index);
								parser->temporaries.length = index + 1;
								parser->key_count = new_key_length;
							} else {
								if (parser->value.value_type != JSON_OBJECT_TYPE_OBJECT) {
									fprintf(stderr, LOG_STRING"Failed to find object\n", __FILE__, __LINE__);
									exit(-1);
								} else {
									if (parser->temporaries.length > 0 
										&& parser->temporaries.objects[parser->temporaries.length - 1].value_type == JSON_OBJECT_TYPE_NUMBER
									) {
										res = validate_number(parser);
										if (res) {
											printf(LOG_STRING"number invalidated\n", __FILE__, __LINE__);
											return res;
										}
									} 
									index++;
									for (ssize_t j = 0, k = 0; j < parser->temporaries.length && k < parser->key_count; ++j, ++k) {
										printf("moving key to object\n");
										json_string_printf(&parser->temporary_keys[k]);
										printf("\n");
										printf("moving value to object\n");
										json_value_printf(&parser->temporaries.objects[j], 0, true);
										printf("\n");
										json_object_move_value(parser->value.object, &parser->temporary_keys[k], &parser->temporaries.objects[j]);
										parser->temporary_keys[k] = (json_string_t){};
										parser->temporaries.objects[j] = (json_value_t){};
									}
									printf("new length: %lld\n", index);
									parser->temporaries.length = 0;
									parser->key_count = 0;
									printf("object:\n");
									json_object_printf(parser->value.object, 0, true);
									printf("\n");
								}
							}
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_SIGN | CSON_PARSER_FLAG_FOUND_PERIOD | CSON_PARSER_FLAG_FOUND_VALUE_START | CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT);
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case '[': {
					switch (current_state) {
						case CSON_PARSER_STATE_IDLE: {
							json_array_t *array = debug_malloc(sizeof(json_array_t));
							if (!array) {
								fprintf(stderr, LOG_STRING"Failed to initialize first array\n", __FILE__, __LINE__);
								return CSON_ERR_ALLOC;
							}
							res = json_array_init(array, 8);
							if (res) {
								return res;
							}
							json_value_t val = {
								.value_type = JSON_OBJECT_TYPE_ARRAY,
								.array = array
							};
							parser->value = val;
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							res = json_parser_push_state(parser, current_state);
							if (res) {
								return res;
							}
							current_state = CSON_PARSER_STATE_ARRAY;
						} break;
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_OBJECT: {
							if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_VALUE_START) {
								res = json_parser_push_depth(parser);
								if (res) {
									return res;
								}
								json_array_t *array = debug_malloc(sizeof(json_array_t));
								res = json_array_init(array, 8);
								if (res) return res;
								json_value_t val = {
									.value_type = JSON_OBJECT_TYPE_ARRAY,
									.array = array
								};
								res = json_parser_push_temporary(parser, &val, false);
								if (res) {
									fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
									return res;
								}
								printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
								res = json_parser_push_state(parser, current_state);
								if (res) {
									return res;
								}
								current_state = CSON_PARSER_STATE_ARRAY;
								break;
							} else {
								fprintf(stderr, LOG_STRING"Found \'[\' at index %lld without key\n", __FILE__, __LINE__, parser->pointer);
								return CSON_PARSER_STATE_INVALID_CHARACTER;
							}
						} break;
						case CSON_PARSER_STATE_ARRAY: {
							res = json_parser_push_depth(parser);
							if (res) {
								return res;
							}
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							res = json_parser_push_state(parser, current_state);
							if (res) {
								return res;
							}
							current_state = CSON_PARSER_STATE_ARRAY;
							json_array_t *array = debug_malloc(sizeof(json_array_t));
							res = json_array_init(array, 8);
							if (res) return res;
							json_value_t val = {
								.value_type = JSON_OBJECT_TYPE_ARRAY,
								.array = array
							};
							res = json_parser_push_temporary(parser, &val, false);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push temporary into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							break;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case ']': {
					if (parser->parser_flag & CSON_PARSER_FLAG_FOUND_TRAILING_COMMA) {
						fprintf(stderr, LOG_STRING"Found trailing comma\n", __FILE__, __LINE__);
						return CSON_PARSER_STATE_INVALID_CHARACTER;
					}
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							json_parser_pop_state(parser, NULL);
						} __attribute__((fallthrough));
						case CSON_PARSER_STATE_ARRAY: {
							ssize_t index = -1;
							printf("temporaries:\n");
							for (ssize_t i = 0; i < parser->temporaries.length; ++i) {
								printf("i: %lld\n", i);
								json_value_printf(&parser->temporaries.objects[i], 0, true);
								printf("\n");
							}
							printf("Depth:\n");
							for (ssize_t j = 0; j < parser->depth_count; ++j) {
								printf("%lld ", parser->depth[j]);
							}
							printf("\n");
							json_parser_pop_depth(parser, &index);
							if (index >= 0) {
								printf("index: %lld, type: %d\n", index, parser->temporaries.objects[index].value_type);
								if (parser->temporaries.objects[index].value_type != JSON_OBJECT_TYPE_ARRAY) {
									fprintf(stderr, LOG_STRING"Found illegal \']\' on index %lld\n", __FILE__, __LINE__, index);
									return CSON_PARSER_STATE_INVALID_CHARACTER;
								}
								if (parser->temporaries.length > 0 
									&& parser->temporaries.objects[parser->temporaries.length - 1].value_type == JSON_OBJECT_TYPE_NUMBER
								) {
									res = validate_number(parser);
									if (res) {
										printf(LOG_STRING"number invalidated\n", __FILE__, __LINE__);
										return res;
									}
								} 
								json_array_t *arr = parser->temporaries.objects[index].array;
								for (ssize_t j = index + 1; j < parser->temporaries.length; ++j) {
									printf("moving value to array\n");
									json_value_printf(&parser->temporaries.objects[j], 0, true);
									json_array_move_value(arr, &parser->temporaries.objects[j]);
									parser->temporaries.objects[j] = (json_value_t){};
								}
								printf("new length: %lld\n", index);
								parser->temporaries.length = index + 1;
								printf("array:\n");
								json_array_printf(arr, 0);
								printf("\n");
								break;
							} else {
								if (parser->value.value_type != JSON_OBJECT_TYPE_ARRAY) {
									fprintf(stderr, LOG_STRING"Failed to find array\n", __FILE__, __LINE__);
									exit(-1);
								} else {
									if (parser->temporaries.length > 0 
										&& parser->temporaries.objects[parser->temporaries.length - 1].value_type == JSON_OBJECT_TYPE_NUMBER
									) {
										res = validate_number(parser);
										if (res) {
											printf(LOG_STRING"number invalidated\n", __FILE__, __LINE__);
											return res;
										}
									} 
									index++;
									for (ssize_t j = index; j < parser->temporaries.length; ++j) {
										printf("moving value to array\n");
										json_value_printf(&parser->temporaries.objects[j], 0, true);
										printf("\n");
										json_array_move_value(parser->value.array, &parser->temporaries.objects[j]);
										parser->temporaries.objects[j] = (json_value_t){};
									}
									printf("new length: %lld\n", index);
									parser->temporaries.length = index;
									printf("array:\n");
									json_array_printf(parser->value.array, 0);
									printf("\n");
								}
							}
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_SIGN | CSON_PARSER_FLAG_FOUND_PERIOD | CSON_PARSER_FLAG_FOUND_VALUE_START | CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT);
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case ',': {
					switch (current_state) {
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							res = json_parser_pop_state(parser, &current_state);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_F64: {
						}
						case CSON_PARSER_STATE_I64:
						case CSON_PARSER_STATE_U64: {
							res = validate_number(parser);
							if (res) {
								return res;
							}
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							res = json_parser_pop_state(parser, &current_state);
							if (res) return res;
							parser->parser_flag &= ~(CSON_PARSER_FLAG_FOUND_PERIOD | CSON_PARSER_FLAG_FOUND_SIGN | CSON_PARSER_FLAG_FOUND_EXPONENT | CSON_PARSER_FLAG_FOUND_NEGATIVE_EXPONENT);
							parser->exponent = 0;
						} __attribute__((fallthrough));
						case CSON_PARSER_STATE_NULL: {
							json_parser_flags_printf(parser->parser_flag);
							if (parser->parser_flag & (CSON_PARSER_FLAG_FOUND_TRAILING_COMMA | CSON_PARSER_FLAG_FOUND_KEY_START | CSON_PARSER_FLAG_FOUND_KEY_END | CSON_PARSER_FLAG_FOUND_VALUE_START)) {
								fprintf(stderr, LOG_STRING"INVALID STATE\n", __FILE__, __LINE__);
								exit(-1);
							}
							parser->parser_flag |= CSON_PARSER_FLAG_FOUND_TRAILING_COMMA;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid character %c at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						}
					}
				} break;
				case '\\': {
					switch (current_state) {
						case CSON_PARSER_STATE_ESCAPE: {
							printf(LOG_STRING"Popping state\n", __FILE__, __LINE__);
							json_parser_pop_state(parser, &current_state);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to pop state due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							switch (current_state) {
								case CSON_PARSER_STATE_STRING: {
									json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
									res = json_string_append_char(str, ch);
									if (res) return res;
								} break;
								case CSON_PARSER_STATE_KEY: {
									json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
									res = json_string_append_char(str, ch);
									if (res) return res;
								} break;
								default: {
									fprintf(stderr, LOG_STRING"Unreachable state reached.\n", __FILE__, __LINE__);
									exit(-1);
								} break;
							}
						} break;
						case CSON_PARSER_STATE_STRING:  
						case CSON_PARSER_STATE_KEY: {
							printf("Entering escape mode\n");
							printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
							res = json_parser_push_state(parser, current_state);
							if (res) {
								fprintf(stderr, LOG_STRING"Failed to push state into parser due to error %d\n", __FILE__, __LINE__, res);
								return res;
							}
							current_state = CSON_PARSER_STATE_ESCAPE;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found invalid escape character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case ' ': {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_I64: 
						case CSON_PARSER_STATE_F64: 
						case CSON_PARSER_STATE_U64: {
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							printf("Expecting comma or end\n");
						}
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {}
						case CSON_PARSER_STATE_IDLE: {
							
						
						} break;
						case CSON_PARSER_STATE_OBJECT: {} break;
						case CSON_PARSER_STATE_ARRAY: {} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \' \' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '\r': {
					printf("Found \"\\r\" at index %lld\n", parser->pointer);
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_I64: 
						case CSON_PARSER_STATE_F64: 
						case CSON_PARSER_STATE_U64: {
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							printf("Expecting comma or end\n");
						} break;
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {} break;
						case CSON_PARSER_STATE_IDLE: {} break;
						case CSON_PARSER_STATE_ESCAPE: {} break;
						case CSON_PARSER_STATE_OBJECT: {} break;
						case CSON_PARSER_STATE_ARRAY: {} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \' \' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '\t': {
					printf("Found \"\\t\" at index %lld\n", parser->pointer);
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_I64: 
						case CSON_PARSER_STATE_F64: 
						case CSON_PARSER_STATE_U64: {
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							printf("Expecting comma or end\n");
						} break;
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {} break;
						case CSON_PARSER_STATE_IDLE: {} break;
						case CSON_PARSER_STATE_OBJECT: {} break;
						case CSON_PARSER_STATE_ARRAY: {} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \' \' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				case '\0': {
					
				} break;
				case '\n': {
					printf("Found \"\\n\" at index %lld\n", parser->pointer);
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_I64: 
						case CSON_PARSER_STATE_F64: 
						case CSON_PARSER_STATE_U64: {
							current_state = CSON_PARSER_STATE_EXPECT_END_OR_COMMA;
							printf("Expecting comma or end\n");
						} break;
						case CSON_PARSER_STATE_EXPECT_END_OR_COMMA: {} break;
						case CSON_PARSER_STATE_IDLE: {} break;
						case CSON_PARSER_STATE_OBJECT: {} break;
						case CSON_PARSER_STATE_ARRAY: {} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \' \' character at index %lld\n", __FILE__, __LINE__, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
				default: {
					switch (current_state) {
						case CSON_PARSER_STATE_KEY: {
							json_string_t *str = &parser->temporary_keys[parser->key_count - 1];
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						case CSON_PARSER_STATE_STRING: {
							json_string_t *str = &parser->temporaries.objects[parser->temporaries.length - 1].string;
							res = json_string_append_char(str, ch);
							if (res) return res;
						} break;
						default: {
							fprintf(stderr, LOG_STRING"Found illegal \'%c\' character at index %lld\n", __FILE__, __LINE__, ch, parser->pointer);
							return CSON_PARSER_STATE_INVALID_CHARACTER;
						} break;
					}
				} break;
			}
		} 
	}
	printf(LOG_STRING"Pushing state\n", __FILE__, __LINE__);
	json_parser_push_state(parser, current_state);

	return 0;
}

int32_t json_parser_finalize(json_parser_t *const parser, json_value_t *val) {
	printf("Finalizing\n");
	if (parser->parser_flag || 
		parser->temporaries.length 
		|| parser->key_count 
		|| parser->state_count > 2
		|| parser->states[0] != CSON_PARSER_STATE_IDLE
		|| parser->states[1] != CSON_PARSER_STATE_EXPECT_END_OR_COMMA
	) {
		fprintf(stderr, LOG_STRING"Failed to properly parse file.\n", __FILE__, __LINE__);
		json_parser_flags_printf(parser->parser_flag);
		json_parser_print_state(parser);
		return CSON_PARSER_STATE_INVALID_CHARACTER;
	}
	printf("-----------------------------------------FINAL-----------------------------------------\n");
	json_value_printf(&parser->value, 0, true);
	printf("\n");
	printf("-----------------------------------------FINAL-----------------------------------------\n");
	if (val) *val = parser->value;
	else json_value_free(&parser->value);
	printf("\n");
	return 0;
}

int32_t json_parse(json_parser_t *const parser, json_value_t *value, const char *const filename) {
	if (!filename || !parser) return CSON_ERR_NULL_PTR;
	FILE *file = fopen64(filename, "r");
	if (!file) {
		printf("Failed to open file %s\n", filename);
		return errno;
	}
	int res = 0;
	ssize_t n = fread(parser->buf, sizeof(char), BUFFER_SIZE, file); 
	while (n) {
		printf("read %lld bytes\n", n);
		res = json_parser_digest(parser, n);
		if (res) goto cleanup;
		n = fread(parser->buf, sizeof(char), BUFFER_SIZE, file); 
	}
	res = json_parser_finalize(parser, value);
	cleanup:
	if (res) json_value_free(&parser->value);
		json_parser_free(parser);
		fclose(file);
	return res;
}

int32_t json_parser_free(json_parser_t *parser) {
	if (!parser) return CSON_ERR_NULL_PTR;
	if (parser->depth) {
		printf("Freeing depth stack\n");
		debug_free(parser->depth);
	}
	if (parser->temporary_keys) {
		printf("Freeing temporary keys\n");
		for (ssize_t i = 0; i < parser->key_count; ++i) {
			json_string_free(&parser->temporary_keys[i]);
		}
		debug_free(parser->temporary_keys);
	}
	if (parser->states) {
		printf("Freeing states\n");
		debug_free(parser->states);
	}
	if (parser->temporaries.objects) {
		printf("Freeing temporaries\n");
		json_array_free(&parser->temporaries);
	}
	*parser = (json_parser_t){};
	return 0;
}