#include "../include/cson.h"

#define __CSON_DEBUG

#ifdef __CSON_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
void debug_free(void *ptr) { \
	printf("Freeing address 0x%p\n", ptr); \
	free(ptr); \
}
void *debug_malloc(size_t size) {
	printf("Allocating pointer with size %llu byte(s)\n", size);
	void *ptr = malloc(size);
	printf("Resulting ptr: 0x%p\n", ptr);
	return ptr;
}
void *debug_calloc(const size_t num_elements, const size_t element_size) {
	printf("Allocating pointer with length %lld and size %llu byte(s)\n", num_elements, num_elements * element_size);
	void *ptr = calloc(num_elements, element_size);
	printf("Resulting ptr: 0x%p\n", ptr);
	return ptr;
}
void *debug_realloc(void *ptr, size_t size) {
	printf("Rellocating pointer at address %p with size %llu byte(s)\n", ptr, size);
	void *new_ptr = realloc(ptr, size);
	printf("Resulting ptr: 0x%p\n", new_ptr);
	return new_ptr;
}
#else
#define debug_printf(...) {}
#define debug_free(ptr) free(ptr)
#define debug_malloc(size) malloc(size)
#define debug_calloc(num, size) calloc(num, size)
#define debug_realloc(ptr, size) realloc(ptr, size)
#endif // __CSON_DEBUG

#define LOG_STRING "[%s]:%d "
#define FNV_PRIME 1099511628211LL
#define FNV_OFFSET_BASIS 14695981039346656037ULL

ssize_t json_key_hash(ssize_t bucket_size, const json_string_t *const string) {
	if (!string || !string->buf) return CSON_ERR_NULL_PTR;
	size_t hash = FNV_OFFSET_BASIS;
    for (ssize_t i = 0; i < string->length; ++i) {
		hash ^= string->buf[i];
		hash *= FNV_PRIME;
	}
    return hash % bucket_size;
}

int32_t json_object_init(json_object_t *obj, size_t size) {
	if (!obj) return CSON_ERR_NULL_PTR;
	if (size <= 0) return CSON_ERR_INVALID_ARGUMENT;
	obj->buckets = debug_calloc(size, sizeof(json_bucket_t));
	if (!obj->buckets) {
		fprintf(stderr, LOG_STRING"Failed to allocate memory for buckets\n", __FILE__, __LINE__);
		return CSON_ERR_ALLOC;
	}
	obj->keys = debug_malloc(size * sizeof(json_string_t));
	if (!obj->keys) {
		fprintf(stderr, LOG_STRING"Failed to allocate memory for keys\n", __FILE__, __LINE__);
		free(obj->buckets);
		return CSON_ERR_ALLOC;
	}
	obj->count = 0;
	obj->size = size;
	obj->keys->length = 0;
	obj->keys->size = size;
	return 0;
}

int32_t json_array_init(json_array_t *array, size_t size) {
	if (!array) return CSON_ERR_NULL_PTR;
	if (size <= 0) return CSON_ERR_INVALID_ARGUMENT;
	array->objects = debug_calloc(size, sizeof(json_value_t));
	if (!array->objects) {
		fprintf(stderr, LOG_STRING"Failed to allocate memory for array\n", __FILE__, __LINE__);
		return CSON_ERR_ALLOC;
	}
	array->length = 0;
	array->size = size;
	return 0;
}

int32_t json_string_copy(json_string_t *restrict string, const json_string_t *const restrict original) {
	if (!string || !original) return CSON_ERR_NULL_PTR;
	printf("Copying string with length %lld and size %lld\n", original->size, original->length);
	if (original->buf) printf("original: \"%s\"\n", original->buf);
	string->size = original->size > 0 ? original->size : 8;
	if (original->size == original->length) string->size++;
	string->buf = debug_malloc(string->size * sizeof(char));
	if (!string->buf) {
		fprintf(stderr, LOG_STRING"Failed to copy \"%s\" with length %lld and size %lld byte(s)\n", __FILE__, __LINE__, original->buf, original->length, original->size * sizeof(char));
		string->size = 0;
		return CSON_ERR_ALLOC;
	}
	ssize_t length = original->length > 0 ? original->length : 0;
	memcpy(string->buf, original->buf, length);
	string->buf[length] = '\0';
	printf("Result: %s\n", string->buf);
	string->length = length;
	return 0;
}

int32_t json_value_copy(json_value_t *restrict copy, const json_value_t *const restrict original) {
	if (!copy || !original) return CSON_ERR_NULL_PTR;
	printf("Copying value\n");
	json_value_printf(original, 0, true);
	printf("\n");
	int res = 0;
	copy->value_type = original->value_type;
	switch (original->value_type) {
		case JSON_OBJECT_TYPE_OBJECT: {
			json_object_t *obj = debug_malloc(sizeof(json_object_t));
			*obj = (json_object_t){};
			json_object_copy(obj, original->object);
			copy->object = obj;
		} break;
		case JSON_OBJECT_TYPE_ARRAY: {
			json_array_t *array = debug_malloc(sizeof(json_array_t));
			*array = (json_array_t){};
			json_array_copy(array, original->array);
			copy->array = array;
		} break;
		case JSON_OBJECT_TYPE_STRING: {
			res = json_string_copy(&(copy->string), &(original->string));
			if (res) {
				fprintf(stderr, LOG_STRING"Freeing copied string \'%s\" due to error %d\n", __FILE__, __LINE__, original->string.buf, res);
				json_string_free(&(copy->string));
				copy->value_type = __JSON_OBJECT_TYPE_MAX;
			}
		} break;
		case JSON_OBJECT_TYPE_BOOL: {
			copy->boolean = original->boolean;
		} break;
		case JSON_OBJECT_TYPE_NUMBER: {
			copy->number = original->number;
		} break;
		case JSON_OBJECT_TYPE_NULL: {
			copy->value_type = JSON_OBJECT_TYPE_NULL;
			copy->null = (json_null_t){};
		} break;
		default: {
			res = CSON_ERR_INVALID_ARGUMENT;
		}  
	}
	return res;
}

int32_t json_object_append_key(json_object_t *const obj, const json_string_t *const key, bool copy) {
	if (!obj || !obj->buckets || !obj->keys || !key) return CSON_ERR_NULL_PTR;
	if (obj->count >= obj->size) {
		ssize_t nsz = obj->size * 2;
		if (nsz < 0 || (nsz * ((ssize_t)sizeof(json_string_t)) < 0)) return CSON_ERR_MAX_SIZE_REACHED;
		json_string_t *tmp = debug_realloc(obj->keys, nsz * sizeof(json_string_t));
		if (!tmp) {
			fprintf(stderr, LOG_STRING"Failed to append key \"%s\" due to error %d\n", __FILE__, __LINE__, key->buf, CSON_ERR_ALLOC);
			return CSON_ERR_ALLOC;
		}
		obj->keys = tmp;
		obj->size = nsz;
	}
	if (copy) {
		int res = json_string_copy(&obj->keys[obj->count], key);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to append key \"%s\" due to error %d\n", __FILE__, __LINE__, key->buf, res);
			return res;
		}
	} else {
		obj->keys[obj->count] = *key;
	}
	obj->count++;
	return 0;
}

int32_t json_object_find_value(json_object_t *const obj, const json_string_t *const key, json_value_t *value) {
	if (!obj || !obj->buckets || !obj->keys || !key || !key->buf || !value) return CSON_ERR_NULL_PTR;
	int res = 0;
	size_t j = json_key_hash(obj->size, key);
	printf("j: %lld\n", j);
	json_bucket_t *curr = &obj->buckets[j], *next = obj->buckets[j].next;
	if (curr->key.buf && strcmp(key->buf, curr->key.buf) == 0) {
		printf("Found object with key \"%s\"\n", key->buf);
		*value = curr->value;
		return 0;
	}
	while (next) {
		if (next->key.buf && strcmp(key->buf, next->key.buf) == 0) {
			printf("Found object with key \"%s\"\n", key->buf);
			*value = curr->value;
			return 0;
		}
		printf("curr: 0x%p, next: 0x%p\n", (void*)curr, (void*)next);
		curr = next;
		next = next->next;
	}
	return CSON_ERR_NOT_FOUND;
}

int32_t json_object_delete_key(json_object_t *const obj, const json_string_t *const key, json_value_t *value) {
	if (!obj || !obj->buckets || !obj->keys || !key || !key->buf) return CSON_ERR_NULL_PTR;
	if (obj->count < 0) return CSON_ERR_NOT_FOUND; 
	int res = 0;
	size_t j = json_key_hash(obj->size, key);
	printf("j: %lld\n", j);
	json_bucket_t *curr = &obj->buckets[j], *next = obj->buckets[j].next;
	if (curr->key.buf && strcmp(key->buf, curr->key.buf) == 0) {
		printf("Found object with key \"%s\"\n", key->buf);
		for (ssize_t i = 0; i < obj->count; ++i) {
			if (strcmp(key->buf, obj->keys[i].buf) == 0) {
				json_string_free(&obj->keys[i]);
				obj->keys[i] = obj->keys[obj->count - 1];
				break;
			}
		}
		if (value) {
			*value = curr->value;
			curr->value = (json_value_t){};
		}
		else {
			json_value_free(&curr->value);
		}
		json_string_free(&curr->key);
		curr = curr->next;
		obj->count--;
		return 0;
	}
	while (next) {
		if (next->key.buf && strcmp(key->buf, next->key.buf) == 0) {
			printf("Found object with key \"%s\"\n", key->buf);
			for (ssize_t i = 0; i < obj->count; ++i) {
				if (strcmp(key->buf, obj->keys[i].buf) == 0) {
					json_string_free(&obj->keys[i]);
					obj->keys[i] = obj->keys[obj->count - 1];
					break;
				}
			}
			json_string_free(&next->key);
			if (value) {
				*value = curr->value;
				curr->value = (json_value_t){};
			}
			else {
				json_value_free(&curr->value);
			}
			next->value = (json_value_t){};
			curr->next = next->next;
			obj->count--;
			return 0;
		}
		printf("curr: 0x%p, next: 0x%p\n", (void*)curr, (void*)next);
		curr = next;
		next = next->next;
	}
	return CSON_ERR_NOT_FOUND;
}

int32_t json_object_append_value(json_object_t *const obj, const json_string_t *const key, const json_value_t *const value) {
	if (!obj || !obj->buckets || !obj->keys || !key || !key->buf || !value) return CSON_ERR_NULL_PTR;
	int res = 0;
	size_t j = json_key_hash(obj->size, key);
	json_bucket_t *curr = &obj->buckets[j], *next = obj->buckets[j].next;
	if (curr->key.buf && strcmp(key->buf, curr->key.buf) == 0) {
		printf("Found duplicate key \"%s\"\n", key->buf);
		return CSON_ERR_ILLEGAL_OPERATION;
	}
	while (next) {
		if (next->key.buf && strcmp(key->buf, next->key.buf) == 0) {
			printf("Found duplicate key \"%s\"\n", key->buf);
			return CSON_ERR_ILLEGAL_OPERATION;
		}
		printf("curr: 0x%p, next: 0x%p\n", (void*)curr, (void*)next);
		curr = next;
		next = next->next;
	}
	if (obj->count >= obj->size) {
		res = json_object_rehash(obj, obj->size * 2);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to rehash object due to error %d\n", __FILE__, __LINE__, res);
			return res;
		}
	}
	json_string_t key_copy = {};
	res = json_string_copy(&key_copy, key);
	if (res) {
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	json_value_t value_copy = {
		.value_type = __JSON_OBJECT_TYPE_MAX
	};
	res = json_value_copy(&value_copy, value);
	if (res) {
		json_string_free(&key_copy);
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	curr->value = value_copy;
	curr->next = NULL;
	res = json_string_copy(&curr->key, &key_copy);
	if (res) {
		json_string_free(&key_copy);
		json_value_free(&value_copy);
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	res = json_object_append_key(obj, &key_copy, false);
	if (res) {
		json_string_free(&key_copy);
		json_value_free(&value_copy);
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
	}
	return res;
}

int32_t json_array_append_value(json_array_t *arr, const json_value_t *const val) {
	if (!arr || !val) return CSON_ERR_NULL_PTR;
	if (arr->length >= arr->size) {
		int res = json_array_resize(arr, arr->size * 2);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to append value to array with length %lld and size %lld byte(s) due to error %d\n", __FILE__, __LINE__, arr->length, arr->size * sizeof(json_value_t), res);
			return res;
		}
	}
	json_value_t val_copy = {
		.value_type = __JSON_OBJECT_TYPE_MAX
	};
	int res = json_value_copy(&val_copy, val);
	if (res) {
		fprintf(stderr, LOG_STRING"Failed to append value to array with length %lld and size %lld byte(s) due to error %d\n", __FILE__, __LINE__, arr->length, arr->size * sizeof(json_value_t), res);
		return res;
	}
	arr->objects[arr->length++] = val_copy;
	return 0;
}

int32_t json_array_delete_index(json_array_t *arr, const ssize_t index) {
	if (!arr || !arr->objects) return CSON_ERR_NULL_PTR;
	if (index >= arr->length) return CSON_ERR_NOT_FOUND;
	json_value_free(&arr->objects[index]);
	for (size_t i = index + 1; i < arr->length; ++i) {
		arr->objects[i - 1] = arr->objects[i];
	}
	arr->length--;
	return 0;
}

int32_t json_array_pop(json_array_t *array, json_value_t *val) {
	if (array->length <= 0) return CSON_ERR_ILLEGAL_OPERATION;
	if (val) {
		*val = array->objects[array->length - 1];
	} else {
		json_value_free(&array->objects[array->length - 1]);
	} 
	array->length--;
	return 0;
}

int32_t json_array_delete_value(json_array_t *arr, const json_value_t *const val) {
	if (!arr || !arr->objects) return CSON_ERR_NULL_PTR;
	size_t i;
	for (i = 0; i < arr->length; ++i) {
		if (arr->objects[i].value_type == val->value_type) {
			switch (val->value_type) {
				case JSON_OBJECT_TYPE_OBJECT: {
					// if ()
				} break;
				
				default:
					json_value_free(&arr->objects[i]);
					break;
			}
		}
	}
	if (i >= arr->length) return CSON_ERR_NOT_FOUND;
	arr->length--;
	return 0;
}

int32_t json_array_copy(json_array_t *const restrict array, const json_array_t *const restrict original_array) {
	if (!array || !original_array || (!original_array->objects && original_array->length)) return CSON_ERR_NULL_PTR;
	int res = 0;
	if (!array->objects) {
		res = json_array_init(array, original_array->size > 0 ? original_array->size : 8);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to copy array with length %lld and size %lld byte(s) due to error %d\n", __FILE__, __LINE__, original_array->length, original_array->size * sizeof(json_value_t), res);
			return res;
		}
	}
	for (ssize_t i = 0; i < original_array->length; ++i) {
		json_value_t val_copy = {
			.value_type = __JSON_OBJECT_TYPE_MAX
		};
		int res = json_value_copy(&val_copy, &original_array->objects[i]);
		if (res) {
			json_array_free(array);
			fprintf(stderr, LOG_STRING"Failed to append value to array with length %lld and size %lld byte(s) due to error %d\n", __FILE__, __LINE__, array->length, array->size * sizeof(json_value_t), res);
			return res;
		}
		array->objects[i] = val_copy;
	}
	array->length = original_array->length;
	array->size = original_array->size;
	return 0;
}

int32_t json_object_copy(json_object_t *const copy, const json_object_t *const obj) {
	if (!copy || !obj) return CSON_ERR_NULL_PTR;
	ssize_t i;
	if (!copy->buckets) {
		int res = json_object_init(copy, obj->size > 0 ? obj->size : 8);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to copy due to error %d\n", __FILE__, __LINE__, res);
			return res;
		}
	}
	printf("Copying object at address %p", (void*)obj);
	for (i = 0; i < obj->count; ++i) {
		json_string_t *key = &obj->keys[i];
		ssize_t j = json_key_hash(copy->size, key);
		assert(j >= 0);
		json_value_t *curr = &obj->buckets[j].value;
		printf("Copying value with key: \"%s\"\n", key->buf);
		json_value_printf(curr, 0, true);
		printf("\n");
		int res = json_object_append_value(copy, key, curr);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to copy value at index %lld\n", __FILE__, __LINE__, j);
			json_object_free(copy);
		}
	}
	return 0;
}

int32_t json_string_append_char(json_string_t *str, const char ch) {
	if (!str) return CSON_ERR_NULL_PTR;
	if (!str->buf) {
		str->buf = debug_malloc(8 * sizeof(char));
		if (!str->buf) {
			return CSON_ERR_ALLOC;
		}
		str->length = 0;
		str->size = 8;
	}
	if (str->length >= str->size) {
		ssize_t nsz = str->size * 2;
		char *tmp = debug_realloc(str->buf, nsz * sizeof(char));
		if (nsz <= 0) return CSON_ERR_MAX_SIZE_REACHED;
		if (!tmp) return CSON_ERR_ALLOC;
		str->buf = tmp;
		str->size = nsz;
	}
	str->buf[str->length++] = ch;
	return 0;
}

int32_t json_string_free(json_string_t *string) {
	if (!string) return CSON_ERR_NULL_PTR;
	if (string->buf) debug_free(string->buf);
	*string = (json_string_t){};
	return 0;
}

int32_t json_value_free(json_value_t *val) {
	if (!val) return CSON_ERR_NULL_PTR;
	switch (val->value_type) {
		case JSON_OBJECT_TYPE_OBJECT: {
			json_object_free(val->object);
			printf("Freeing allocated object pointer\n");
			debug_free(val->object);
		} break;
		case JSON_OBJECT_TYPE_ARRAY: {
			json_array_free(val->array);
			printf("Freeing allocated array pointer\n");
			debug_free(val->array);
		} break;
		case JSON_OBJECT_TYPE_STRING: {
			json_string_free(&val->string);
		} break;
		case JSON_OBJECT_TYPE_BOOL:
		case JSON_OBJECT_TYPE_NUMBER:
		case JSON_OBJECT_TYPE_NULL:
		default: {} break;
	}
	*val = (json_value_t){};
	return 0;
}

int32_t json_object_free(json_object_t *obj) {
	if (!obj) return CSON_ERR_NULL_PTR;
	if (obj->buckets) {
		for (ssize_t i = 0, j = 0; i < obj->count && j < obj->size; ++j) {
			json_bucket_t *curr = &obj->buckets[j];
			if (!curr->key.buf) continue;
			printf(LOG_STRING"Freeing object with key %s\n", __FILE__, __LINE__, curr->key.buf);
			printf("Value:\n");
			json_value_printf(&curr->value, 0, true);
			printf("\n");
			json_value_free(&curr->value);
			curr = curr->next;
			i++;
			while (curr) {
				json_bucket_t *prev = curr;
				printf(LOG_STRING"Freeing object with key %s\n", __FILE__, __LINE__, curr->key.buf);
				printf("Value:\n");
				json_value_printf(&curr->value, 0, true);
				printf("\n");
				json_value_free(&curr->value);
				curr = curr->next;
				debug_free(prev);
				i++;
			}
		}
		printf(LOG_STRING"Freeing buckets with count %lld and size %lld byte(s)\n", __FILE__, __LINE__, obj->count, obj->size * sizeof(json_bucket_t));
		debug_free(obj->buckets);
	}
	if (obj->keys) {
		for (ssize_t j = 0; j < obj->count; ++j) {
			printf(LOG_STRING"Freeing key %s at index %lld with length %lld and size %lld byte(s)\n",  __FILE__, __LINE__, obj->keys[j].buf, j, obj->keys[j].length, obj->keys[j].size * sizeof(char));
			debug_free(obj->keys[j].buf);
		}
		printf(LOG_STRING"Freeing key array with count %lld and size %lld byte(s)\n", __FILE__, __LINE__, obj->count, obj->size * sizeof(json_string_t));
		debug_free(obj->keys);
	}
	*obj = (json_object_t){};
	return 0;
}

int32_t json_array_free(json_array_t *array) {
	if (!array) return CSON_ERR_NULL_PTR;
	for (ssize_t i = 0; i < array->length; ++i) {
		json_value_t *val = &array->objects[i];
		debug_printf(LOG_STRING"Freeing value at index %lld\n", __FILE__, __LINE__,  i);
		json_value_printf(val, 0, true);
		json_value_free(val);
	}
	if (array->objects) {
		printf(LOG_STRING"Freeing object array with length %lld and size %lld byte(s)\n", __FILE__, __LINE__, array->length, array->size * sizeof(json_value_t));
		debug_free(array->objects);
	}
	*array = (json_array_t){};
	return 0;
}

int32_t json_array_resize(json_array_t *array, ssize_t new_size) {
	if (!array) return CSON_ERR_NULL_PTR;
	if (new_size <= array->size) return CSON_ERR_INVALID_ARGUMENT;
	if (new_size * ((ssize_t)sizeof(json_value_t)) < 0) return CSON_ERR_MAX_SIZE_REACHED;
	json_value_t *tmp = debug_realloc(array->objects, new_size * sizeof(json_value_t));
	if (!tmp) {
		fprintf(stderr, LOG_STRING"Failed to resize array with length %lld and size %lld byte(s) to %lld byte(s)\n", __FILE__, __LINE__, array->length, array->size * sizeof(json_value_t), new_size * sizeof(json_value_t));
		return CSON_ERR_ALLOC;
	}
	array->objects = tmp;
	array->size = new_size;
	return 0;
}

int32_t json_object_rehash(json_object_t *obj, ssize_t new_size) {
	if (!obj) return CSON_ERR_NULL_PTR;
	if (obj->count >= new_size) return CSON_ERR_INVALID_ARGUMENT;
	json_object_t new_obj;
	int res = json_object_init(&new_obj, new_size);
	if (res) return res;
	for (ssize_t i = 0; i < obj->count; ++i) {
		json_string_t *key = &obj->keys[i]; 
		ssize_t j = json_key_hash(obj->size, key);
		if (j < 0) {
			fprintf(stderr, LOG_STRING"Failed to hash key at index %lld with key %s\n", __FILE__, __LINE__, i, key->buf);
			json_object_free(&new_obj);
			return res;
		}
		json_value_t *curr = &obj->buckets[j].value;
		res = json_object_append_value(&new_obj, key, curr);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to append key at index %lld with key %s\n", __FILE__, __LINE__, i, key->buf);
			json_object_free(&new_obj);
			return res;
		}
	}
	*obj = new_obj;
	return 0;
}

int32_t json_array_printf(const json_array_t *const arr, uint64_t indent) {
	if (!arr || (!arr->objects && arr->length)) return CSON_ERR_NULL_PTR;
	printf("[");
	if (arr->length > 0) {
		printf("\n");
		ssize_t i;
		for (i = 0; i < arr->length - 1; ++i) {
			for (uint64_t l = 0; l < indent + 1; ++l) printf("\t");
			json_value_t *val = &arr->objects[i];
			json_value_printf(val, indent + 1, false);
			printf(",\n");
		}
		for (uint64_t l = 0; l < indent + 1; ++l) printf("\t");
		json_value_t *val = &arr->objects[i];
		json_value_printf(val, indent + 1, false);
		printf("\n");
		for (uint64_t l = 0; l < indent; ++l) printf("\t");
	}
	printf("]");
	return 0;
}

int32_t json_string_printf(const json_string_t *const str) {
	if (!str) return CSON_ERR_NULL_PTR;
	printf("\"%s\"", str->buf ? str->buf : "<null>");
	return 0;
}

int32_t json_value_printf(const json_value_t *const val, uint64_t indent, bool start) {
	if (!val) return CSON_ERR_NULL_PTR;
	switch (val->value_type) {
		case JSON_OBJECT_TYPE_OBJECT: {
			json_object_printf(val->object, indent, start);
		} break;
		case JSON_OBJECT_TYPE_ARRAY: {
			json_array_printf(val->array, indent);
		} break;
		case JSON_OBJECT_TYPE_STRING: {
			json_string_printf(&val->string);
		} break;
		case JSON_OBJECT_TYPE_BOOL: {
			printf("%s", val->boolean ? "true" : "false");
		} break;
		case JSON_OBJECT_TYPE_NUMBER: {
			switch (val->number.num_type) {
				case JSON_NUMBER_TYPE_I64: {
					printf("%lld", val->number.i64);
				} break;
				case JSON_NUMBER_TYPE_U64: {
					printf("%llu", val->number.u64);
				} break;
				case JSON_NUMBER_TYPE_F64: {
					printf("%f", val->number.f64);
				} break;
				default: {
					return CSON_ERR_INVALID_ARGUMENT;
				} break;
			}
		} break;
		case JSON_OBJECT_TYPE_NULL: {
			printf("null");
		} break;
		default: {
			return CSON_ERR_INVALID_ARGUMENT;
		} break;
	}
	return 0;
}

int32_t json_object_printf(const json_object_t *const obj, uint64_t indent, bool start) {
	if (!obj || ((!obj->buckets || !obj->keys) && obj->count)) return CSON_ERR_NULL_PTR;
	printf("{");
	if (obj->count > 0) {
		printf("\n");
		ssize_t i, j = 0;
		for (i = 0; i < obj->count - 1; ++i) {
			j = json_key_hash(obj->size, &obj->keys[i]);
			if (j < 0) {
				fprintf(stderr, "Cannot hash key \"%s\"\n", obj->keys[i].buf);
				continue;
			}
			json_bucket_t *curr = &obj->buckets[j];
			while (curr && strcmp(curr->key.buf, obj->keys[i].buf)) {
				curr = curr->next;
			} 
			if (!curr) {
				fprintf(stderr, "Cannot find key \"%s\"\n", obj->keys[i].buf);
				continue;
			}
			for (uint64_t l = 0; l < indent + 1; ++l) printf("\t");
			json_string_printf(&curr->key);
			printf(": ");
			json_value_t *val = &curr->value;
			json_value_printf(val, indent + 1, false);
			printf(",\n");
		}
		for (uint64_t l = 0; l < indent + 1; ++l) printf("\t");
		j = json_key_hash(obj->size, &obj->keys[i]);
		if (j < 0) {
			fprintf(stderr, "Cannot hash key \"%s\"\n", obj->keys[i].buf);
		} else {
			json_bucket_t *curr = &obj->buckets[j];
			while (curr && strcmp(curr->key.buf, obj->keys[i].buf)) {
				curr = curr->next;
			} 
			if (!curr) {
				fprintf(stderr, "Cannot find key \"%s\"\n", obj->keys[i].buf);
			} else {
				json_string_printf(&curr->key);
				printf(": ");
				json_value_t *val = &curr->value;
				json_value_printf(val, indent + 1, false);
				printf("\n");
			}
		}
		for (uint64_t l = 0; l < indent; ++l) printf("\t");
	}
	printf("}");
	if (start) printf("\n");
	return 0;
}

int32_t expect(const char ch, const char *expected, const ssize_t expected_len) {
	if (!expected) return CSON_ERR_NULL_PTR; 
	for (ssize_t i = 0; i < expected_len; ++i) if (ch == expected[i]) return 0;
	return CSON_ERR_NOT_FOUND;
}

#define BUFFER_SIZE 8192

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

int32_t json_parser_init(json_parser_t *parser) {
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

int32_t main(void) {
	json_value_t value = {};
	json_parse(&value, "tests/test.json");
	printf("DONE\n");
	return 0;
}
