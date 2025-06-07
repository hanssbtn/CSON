#include "../include/cson.h"

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
	printf("Copying string with length %lld and size %lld\n", original->length, original->size);
	json_string_printf(original);
	printf("\n");
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
	printf("Result: \"%s\"\n", string->buf);
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
	printf("appended key:\n");
	json_string_printf(&obj->keys[obj->count]);
	printf("\n");
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
	json_value_t value_copy = {
		.value_type = __JSON_OBJECT_TYPE_MAX
	};
	res = json_value_copy(&value_copy, value);
	if (res) {
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	curr->value = value_copy;
	curr->next = NULL;
	res = json_string_copy(&curr->key, key);
	if (res) {
		json_value_free(&value_copy);
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	res = json_object_append_key(obj, key, true);
	if (res) {
		json_value_free(&value_copy);
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
	}
	return res;
}

int32_t json_object_move_value(json_object_t *const obj, const json_string_t *const key, json_value_t *const value) {
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
	curr->value = *value;
	printf("curr->value:\n");
	json_value_printf(&curr->value, 0,true);
	printf("\n");
	curr->next = NULL;
	res = json_string_copy(&curr->key, key);
	if (res) {
		fprintf(stderr, LOG_STRING"Failed to append object due to error %d\n", __FILE__, __LINE__, res);
		return res;
	}
	res = json_object_append_key(obj, key, false);
	if (res) {
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

int32_t json_array_move_value(json_array_t *arr, const json_value_t *const val) {
	if (!arr || !val) return CSON_ERR_NULL_PTR;
	if (arr->length >= arr->size) {
		int res = json_array_resize(arr, arr->size * 2);
		if (res) {
			fprintf(stderr, LOG_STRING"Failed to append value to array with length %lld and size %lld byte(s) due to error %d\n", __FILE__, __LINE__, arr->length, arr->size * sizeof(json_value_t), res);
			return res;
		}
	}
	arr->objects[arr->length++] = *val;
	return 0;
}

int32_t json_string_cmp(const json_string_t *const str1, const json_string_t *const str2, int *res) {
	if (!str1 || !str2 || !res) return CSON_ERR_NULL_PTR;
	if (str1 == str2 || str1->buf == str2->buf) {
		*res = 0;
		return 0;
	}
	if (str1->buf && !str2->buf) {
		*res = 1;
		return 0;
	}
	if (!str1->buf && str2->buf) {
		*res = -1;
		return 0;
	}
	int cmp = strncmp(str1->buf, str2->buf, str1->length);
	if (cmp) {
		*res = cmp;
		return 0;
	}
	*res = str1->length - str2->length;
	return 0;
}

int32_t json_string_cmp_str(const json_string_t *const str, const char *const buf, int *res) {
	if (!str || !res) return CSON_ERR_NULL_PTR;
	if (str->buf == buf) {
		*res = 0;
		return 0;
	}
	if (str->buf && !buf) {
		*res = 1;
		return 0;
	}
	if (!str->buf && buf) {
		*res = -1;
		return 0;
	}
	*res = strcmp(str->buf, buf);
	return 0;
}

int32_t json_object_cmp(const json_object_t *const obj1, const json_object_t *const obj2, int *res) {
	if (!obj1 || !obj2) return CSON_ERR_NULL_PTR;
	*res = 0;
	if (obj1->count != obj2->count) {
		*res = 1;
		return 0;
	}
	if (obj1->count == 0) {
		*res = 0;
		return 0;
	}
	if (obj1->count < 0) return CSON_ERR_INVALID_ARGUMENT;
	if (obj1->buckets && !obj2->buckets) { 
		*res = 1;
		return 0;
	}
	if (!obj1->buckets && obj2->buckets) {
		*res = 1;
		return 0;
	} 
	if (obj1->keys && !obj2->keys) {
		*res = 1;
		return 0;
	}
	if (!obj1->buckets && obj2->buckets) {
		*res = 1;
		return 0;
	}
	json_object_t key_map = {};
	int result = json_object_init(&key_map, obj1->count);
	if (result) {
		return result;
	}
	json_value_t dummy_value = {
		.value_type = JSON_OBJECT_TYPE_NULL,
		.null = (json_null_t){}
	};
	for (ssize_t i = 0; i < obj1->count; ++i) {
		ssize_t h1 = json_key_hash(obj1->size, &obj1->keys[i]);
		ssize_t h2 = json_key_hash(obj2->size, &obj2->keys[i]);
		// try to append the key to the same object. The number of keys in the key 
		// map will be equal to the count of the objects if all the keys are equal.
		json_object_append_value(&key_map, &obj1->keys[i], &dummy_value);
		json_object_append_value(&key_map, &obj2->keys[i], &dummy_value);
		json_value_t *val1 = &obj1->buckets[h1].value, *val2 = &obj2->buckets[h2].value;
		result = json_value_cmp(val1, val2, res);
		if (result) {
			json_object_free(&key_map);
			return result;
		}
		if (*res) {
			json_object_free(&key_map);
			return 0;
		}
	}
	*res = key_map.count != obj1->count;
	json_object_free(&key_map);
	return 0;
}

int32_t json_array_cmp(const json_array_t *const arr1, const json_array_t *const arr2, int *res) {
	if (!arr1 || !arr2 || !res) return CSON_ERR_NULL_PTR;
	*res = 0;
	if (arr1->length != arr2->length) {
		*res = 1;
		return 0;
	}
	for (ssize_t i = 0; i < arr1->length; ++i) {
		int result = json_value_cmp(&arr1->objects[i], &arr2->objects[i], res);
		if (result) {
			return result;
		}
		if (*res) {
			return 0;
		}
	}
	return 0;
}

int32_t json_value_cmp(const json_value_t *const val1, const json_value_t *const val2, int *res) {
	if (!val1 || !val2 || !res) return CSON_ERR_NULL_PTR;
	if (val1->value_type != val2->value_type) {
		*res = 1;
		return 0;
	}
	int result = 0;
	switch (val1->value_type) {
		case JSON_OBJECT_TYPE_OBJECT: {
			result = json_object_cmp(val1->object, val2->object, res);
			if (result) return result;
		} break;
		case JSON_OBJECT_TYPE_ARRAY: {
			result = json_array_cmp(val1->array, val2->array, res);
			if (result) return result;
		} break;
		case JSON_OBJECT_TYPE_STRING: {
			result = json_string_cmp(&val1->string, &val2->string, res);
			if (result) return result;
		} break;
		case JSON_OBJECT_TYPE_BOOL: {
			*res = val1->boolean != val2->boolean;
		} break;
		case JSON_OBJECT_TYPE_NUMBER: {
			*res = val1->number.num_type != val2->number.num_type;
			if (*res) return 0;
			switch (val1->number.num_type) {
				case JSON_NUMBER_TYPE_F64: {
					*res = (val1->number.f64 > val2->number.f64) - (val1->number.f64 < val2->number.f64);
				} break;
				case JSON_NUMBER_TYPE_I64: {
					*res = (val1->number.i64 > val2->number.i64) - (val1->number.i64 < val2->number.i64);
				} break;
				case JSON_NUMBER_TYPE_U64: {
					*res = (val1->number.u64 > val2->number.u64) - (val1->number.u64 < val2->number.u64);
				} break;
				default:
					break;
			}
		} break;
		case JSON_OBJECT_TYPE_NULL: {
			*res = 0;
		} break;
		default: {
			return CSON_ERR_INVALID_ARGUMENT;
		} break;
	}
	return 0;
}

int32_t json_array_delete_index(json_array_t *arr, const ssize_t index, json_value_t *val) {
	if (!arr || !arr->objects) return CSON_ERR_NULL_PTR;
	if (index >= arr->length) return CSON_ERR_NOT_FOUND;
	if (val) val = &arr->objects[index];
	else json_value_free(&arr->objects[index]);
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
		str->buf[str->size - 1] = '\0';
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
	for (ssize_t i = array->length; i < new_size; ++i) {
		array->objects[i] = (json_value_t){};
	}
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
	if (str->buf) {
		printf("\"");
		for (ssize_t i = 0; i < str->length; ++i) {
			char c = str->buf[i];
			switch (c) {
				case '\'': {
					printf("\\\'");
				} break;
				case '\"': {
					printf("\\\"");
				} break;
				case '\n': {
					printf("\\n");
				} break;
				case '\t': {
					printf("\\t");
				} break;
				case '\r': {
					printf("\\r");
				} break;
				case '\\': {
					printf("\\\\");
				} break;
				default: {
					printf("%c", c);
				}
			}
		}
		printf("\"");
	} else printf("\"\"");
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
			while (curr && strncmp(curr->key.buf, obj->keys[i].buf, curr->key.length)) {
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
			while (curr && strncmp(curr->key.buf, obj->keys[i].buf, curr->key.length)) {
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

