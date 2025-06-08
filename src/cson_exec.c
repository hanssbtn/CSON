#include "../include/cson.h"

int32_t main(void) {
	json_value_t value = {};
	json_parser_t parser = {};
	json_parser_init(&parser);
	int32_t res = json_parse(&parser, NULL, "tests/test_.json");
	if (res) {
		printf("res: %d\n", res);
	} else {
		json_value_printf(&value, 0, true);
		printf("\n");
	}
	printf("DONE\n");
	return 0;
}