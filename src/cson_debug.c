
#include "../include/cson_common.h"

static int64_t ptr_count;
#ifdef __CSON_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
void debug_free(void *ptr) { \
	ptr_count--;
	assert(ptr_count >= 0);
	printf("Pointer count: %lld\n", ptr_count);
	printf("Freeing address 0x%p\n", ptr); \
	free(ptr); \
}
void *debug_malloc(size_t size) {
	printf("Allocating pointer with size %llu byte(s)\n", size);
	void *ptr = malloc(size);
	ptr_count++;
	printf("Resulting ptr: 0x%p\n", ptr);
	printf("Pointer count: %lld\n", ptr_count);
	return ptr;
}
void *debug_calloc(const size_t num_elements, const size_t element_size) {
	printf("Allocating pointer with length %lld and size %llu byte(s)\n", num_elements, num_elements * element_size);
	void *ptr = calloc(num_elements, element_size);
	ptr_count++;
	printf("Pointer count: %lld\n", ptr_count);
	printf("Resulting ptr: 0x%p\n", ptr);
	return ptr;
}
void *debug_realloc(void *ptr, size_t size) {
	printf("Reallocating pointer at address %p with size %llu byte(s)\n", ptr, size);
	void *new_ptr = realloc(ptr, size);
	printf("Resulting ptr: 0x%p\n", new_ptr);
	return new_ptr;
}
#endif // __CSON_DEBUG