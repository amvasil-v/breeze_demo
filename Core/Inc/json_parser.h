#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

extern const char *example_json;

int json_parser_get_number(const char *json, const char *field, int *out);
int json_parser_get_string(const char *json, const char *field, char *out, size_t size);

#endif