#pragma once
#ifndef CSON_COMMON_H__
#define CSON_COMMON_H__

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define CSON_ERR_NULL_PTR -1
#define CSON_ERR_ALLOC -2
#define CSON_ERR_INVALID_ARGUMENT -4
#define CSON_ERR_ILLEGAL_OPERATION -5
#define CSON_ERR_MAX_SIZE_REACHED -6
#define CSON_ERR_NOT_FOUND -7

#define __CSON_DEBUG

#ifdef __CSON_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
void debug_free(void *ptr);
void *debug_malloc(size_t size);
void *debug_calloc(const size_t num_elements, const size_t element_size);
void *debug_realloc(void *ptr, size_t size);
#else
#define debug_printf(...) {}
#define debug_free(ptr) free(ptr)
#define debug_malloc(size) malloc(size)
#define debug_calloc(num, size) calloc(num, size)
#define debug_realloc(ptr, size) realloc(ptr, size)
#endif // __CSON_DEBUG

#define LOG_STRING "[%s]:%d "

#define CSON_EXPECT 10

struct __json_object;
struct __json_array;
struct __json_bucket;

typedef struct __json_object json_object_t;
typedef struct __json_array json_array_t;
typedef struct __json_bucket json_bucket_t;

typedef enum {
	JSON_OBJECT_TYPE_OBJECT,
	JSON_OBJECT_TYPE_ARRAY,
	JSON_OBJECT_TYPE_STRING,
	JSON_OBJECT_TYPE_BOOL,
	JSON_OBJECT_TYPE_NUMBER,
	JSON_OBJECT_TYPE_NULL,
	__JSON_OBJECT_TYPE_MAX = 255
} __attribute__((packed)) json_object_type_t;

typedef enum {
	JSON_NUMBER_TYPE_I64,
	JSON_NUMBER_TYPE_U64,
	JSON_NUMBER_TYPE_F64,
	__JSON_NUMBER_TYPE_MAX = 255
} __attribute__((packed)) json_number_type_t;

typedef struct {
	json_number_type_t num_type;
	union {
		int64_t i64;
		uint64_t u64;
		double f64;
	};
} json_number_t;

typedef struct {
	ssize_t length, size;
	char *buf;
} json_string_t;

typedef struct {} json_null_t;

typedef struct {
	json_object_type_t value_type;
	union {
		json_number_t number;
		json_object_t *object;
		json_array_t *array;
		bool boolean;
		json_null_t null;
		json_string_t string;
	};
} json_value_t;

struct __json_array {
	json_value_t *objects;
	ssize_t length, size;
};

struct __json_bucket {
	json_string_t key;
	json_value_t value;
	struct __json_bucket *next;
};

typedef struct {
	json_string_t *keys;
	ssize_t length, size; 
} json_key_array_t;

struct __json_object {
	json_bucket_t *buckets;
	json_string_t *keys;
	ssize_t count, size;
};

int32_t json_array_init(json_array_t *array, size_t size);
int32_t json_object_init(json_object_t *obj, size_t size);
int32_t json_string_free(json_string_t *string);

int32_t json_string_copy(json_string_t *restrict string, const json_string_t *const restrict original);
int32_t json_array_copy(json_array_t *const restrict copy, const json_array_t *const restrict obj);
int32_t json_object_copy(json_object_t *const copy, const json_object_t *const obj);

int32_t json_string_append_char(json_string_t *str, const char ch);
int32_t json_array_append_value(json_array_t *arr, const json_value_t *const val);
int32_t json_array_move_value(json_array_t *arr, const json_value_t *const val);
int32_t json_object_append_value(json_object_t *const obj, const json_string_t *const key, const json_value_t *const value);
int32_t json_object_move_value(json_object_t *const obj, const json_string_t *const key, json_value_t *const value);

int32_t json_string_cmp(const json_string_t *const str1, const json_string_t *const str2, int *res);
int32_t json_string_cmp_str(const json_string_t *const str1, const char *const buf, int *res);
int32_t json_object_cmp(const json_object_t *const obj1, const json_object_t *const obj2, int *res);
int32_t json_array_cmp(const json_array_t *const arr1, const json_array_t *const arr2, int *res);
int32_t json_value_cmp(const json_value_t *const val1, const json_value_t *const val2, int *res);

int32_t json_array_resize(json_array_t *array, ssize_t new_size);
int32_t json_object_rehash(json_object_t *obj, ssize_t new_size);

int32_t json_value_printf(const json_value_t *const val, uint64_t indent, bool start);
int32_t json_string_printf(const json_string_t *const str);
int32_t json_array_printf(const json_array_t *const arr, uint64_t indent);
int32_t json_object_printf(const json_object_t *const obj, uint64_t indent, bool start);

int32_t json_object_find_value(json_object_t *const obj, const json_string_t *const key, json_value_t *value);

int32_t json_array_delete_value(json_array_t *arr, const json_value_t *const val);
int32_t json_array_delete_index(json_array_t *arr, const ssize_t index, json_value_t *val);
int32_t json_array_pop(json_array_t *arr, json_value_t *val);
int32_t json_object_delete_key(json_object_t *const obj, const json_string_t *const key, json_value_t *value);

int32_t json_value_free(json_value_t *val);
int32_t json_array_free(json_array_t *array);
int32_t json_object_free(json_object_t *obj);

#endif // CSON_COMMON_H__