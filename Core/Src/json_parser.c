#include "json_parser.h"

#include <stdio.h>
#include <string.h>

#include "cJSON/cJSON.h"

const char *example_json = "{\"month\": \"2\", \"num\": 2430, \"link\": \"\", \"year\": \"2021\", \"news\": \"\", \"safe_title\": \"Post-Pandemic Hat\", \"transcript\": \"\", \"alt\": \"Plus a shirt that says \\\"it feels like you're making eye contact.\\\"\", \"img\": \"https://imgs.xkcd.com/comics/post_pandemic_hat.png\", \"title\": \"Post-Pandemic Hat\", \"day\": \"26\"}";

int json_parser_get_number(const char *json, const char *field, int *out)
{
    cJSON *json_obj = cJSON_Parse(json);
    if (json_obj == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf("Error before: %s\n", error_ptr);
        }
        cJSON_Delete(json_obj);
        return -1;
    }

    cJSON *value = cJSON_GetObjectItemCaseSensitive(json_obj, field);
    if (!cJSON_IsNumber(value))
    {
        cJSON_Delete(json_obj);
        return -1;
    }

    *out = (int)value->valueint;
    cJSON_Delete(json_obj);
    return 0;
}

int json_parser_get_string(const char *json, const char *field, char *out, size_t size)
{
    cJSON *json_obj = cJSON_Parse(json);
    if (json_obj == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf("Error before: %s\n", error_ptr);
        }
        cJSON_Delete(json_obj);
        return -1;
    }

    cJSON *value = cJSON_GetObjectItem(json_obj, field);
    if (!cJSON_IsString(value))
    {
        cJSON_Delete(json_obj);
        return -1;
    }

    int len = strlen(value->valuestring);
    if (len >= size - 1) {
        printf("json value too long: %d\n", len);
        cJSON_Delete(json_obj);
        return -1;
    }
    strncpy(out, value->valuestring, size);
    cJSON_Delete(json_obj);
    return 0;
}
