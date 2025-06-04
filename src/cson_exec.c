#include "../include/cson.h"

int32_t main(void) {
	json_object_t *obj = malloc(sizeof(json_object_t));
	
	debug_free(obj);
	printf("DONE\n");
	return 0;
}