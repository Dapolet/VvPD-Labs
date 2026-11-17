#ifndef VPD_JSON_H
#define VPD_JSON_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType type;
    union {
        int boolean;
        double number;
        char *string;
        struct {
            JsonValue **items;
            size_t count;
        } array;
        struct {
            char **keys;
            JsonValue **values;
            size_t count;
        } object;
    } as;
};

/*
 * Строгий рекурсивный JSON-парсер. Поддерживает объекты, массивы, строки
 * с escape-последовательностями (включая \uXXXX и суррогатные пары),
 * числа, true, false и null. Отвергает trailing comma и лишний мусор.
 */
JsonValue *json_parse(const char *text, char *err, size_t errlen);
void json_free(JsonValue *value);

const JsonValue *json_object_get(const JsonValue *object, const char *key);
const char *json_type_name(JsonType type);

#endif /* VPD_JSON_H */
