#include "json.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_DEPTH 64

typedef struct {
    const char *text;
    size_t position;
    int depth;
    char *err;
    size_t errlen;
    int failed;
} JsonParser;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static int json_error(JsonParser *parser, const char *message)
{
    if (!parser->failed) {
        snprintf(parser->err, parser->errlen,
                 "JSON error at byte %zu: %s", parser->position, message);
        parser->failed = 1;
    }
    return -1;
}

static void skip_whitespace(JsonParser *parser)
{
    while (parser->text[parser->position] == ' ' ||
           parser->text[parser->position] == '\t' ||
           parser->text[parser->position] == '\n' ||
           parser->text[parser->position] == '\r') {
        ++parser->position;
    }
}

static JsonValue *parse_value(JsonParser *parser);

/* ------------------------------------------------------------------------- */
/* Строки                                                                    */
/* ------------------------------------------------------------------------- */

static int sb_reserve(StringBuilder *builder, size_t extra,
                      JsonParser *parser)
{
    if (extra > SIZE_MAX - builder->length) {
        return json_error(parser, "string is too long");
    }

    size_t needed = builder->length + extra;
    if (needed <= builder->capacity) {
        return 0;
    }

    size_t new_capacity = (builder->capacity == 0) ? 32 : builder->capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            return json_error(parser, "string is too long");
        }
        new_capacity *= 2;
    }

    char *temporary = realloc(builder->data, new_capacity);
    if (temporary == NULL) {
        return json_error(parser, "out of memory while parsing string");
    }

    builder->data = temporary;
    builder->capacity = new_capacity;
    return 0;
}

static int sb_append_byte(StringBuilder *builder, unsigned char byte,
                          JsonParser *parser)
{
    if (sb_reserve(builder, 1, parser) != 0) {
        return -1;
    }
    builder->data[builder->length++] = (char)byte;
    return 0;
}

static int sb_append_codepoint(StringBuilder *builder, unsigned codepoint,
                               JsonParser *parser)
{
    unsigned char encoded[4];
    size_t count = 0;

    if (codepoint == 0) {
        return json_error(parser, "\\u0000 is not supported");
    } else if (codepoint <= 0x7f) {
        encoded[count++] = (unsigned char)codepoint;
    } else if (codepoint <= 0x7ff) {
        encoded[count++] = (unsigned char)(0xc0 | (codepoint >> 6));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        encoded[count++] = (unsigned char)(0xe0 | (codepoint >> 12));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0x10ffff) {
        encoded[count++] = (unsigned char)(0xf0 | (codepoint >> 18));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    } else {
        return json_error(parser, "invalid Unicode code point");
    }

    if (sb_reserve(builder, count, parser) != 0) {
        return -1;
    }
    memcpy(builder->data + builder->length, encoded, count);
    builder->length += count;
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_hex4(JsonParser *parser, unsigned *value)
{
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
        int digit = hex_value(parser->text[parser->position + (size_t)i]);
        if (digit < 0) {
            return json_error(parser, "invalid \\u escape");
        }
        result = (result << 4) | (unsigned)digit;
    }
    parser->position += 4;
    *value = result;
    return 0;
}

static char *parse_string_literal(JsonParser *parser)
{
    if (parser->text[parser->position] != '"') {
        json_error(parser, "expected string");
        return NULL;
    }
    ++parser->position;

    StringBuilder builder = {0};
    for (;;) {
        char c = parser->text[parser->position];

        if (c == '\0') {
            json_error(parser, "unterminated string");
            free(builder.data);
            return NULL;
        }
        ++parser->position;

        if (c == '"') {
            break;
        }

        if ((unsigned char)c < 0x20) {
            json_error(parser, "control character inside string");
            free(builder.data);
            return NULL;
        }

        if (c != '\\') {
            if (sb_append_byte(&builder, (unsigned char)c, parser) != 0) {
                free(builder.data);
                return NULL;
            }
            continue;
        }

        char escaped = parser->text[parser->position];
        if (escaped == '\0') {
            json_error(parser, "incomplete escape sequence");
            free(builder.data);
            return NULL;
        }
        ++parser->position;

        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            if (sb_append_byte(&builder, (unsigned char)escaped, parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 'b':
            if (sb_append_byte(&builder, '\b', parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 'f':
            if (sb_append_byte(&builder, '\f', parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 'n':
            if (sb_append_byte(&builder, '\n', parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 'r':
            if (sb_append_byte(&builder, '\r', parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 't':
            if (sb_append_byte(&builder, '\t', parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        case 'u': {
            unsigned codepoint = 0;
            if (parse_hex4(parser, &codepoint) != 0) {
                free(builder.data);
                return NULL;
            }

            if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                if (parser->text[parser->position] != '\\' ||
                    parser->text[parser->position + 1] != 'u') {
                    json_error(parser, "high surrogate without low surrogate");
                    free(builder.data);
                    return NULL;
                }
                parser->position += 2;
                unsigned low = 0;
                if (parse_hex4(parser, &low) != 0) {
                    free(builder.data);
                    return NULL;
                }
                if (low < 0xdc00 || low > 0xdfff) {
                    json_error(parser, "invalid low surrogate");
                    free(builder.data);
                    return NULL;
                }
                codepoint = 0x10000u +
                            ((codepoint - 0xd800u) << 10) +
                            (low - 0xdc00u);
            } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                json_error(parser, "unexpected low surrogate");
                free(builder.data);
                return NULL;
            }

            if (sb_append_codepoint(&builder, codepoint, parser) != 0) {
                free(builder.data);
                return NULL;
            }
            break;
        }
        default:
            json_error(parser, "unknown escape sequence");
            free(builder.data);
            return NULL;
        }
    }

    if (sb_reserve(&builder, 1, parser) != 0) {
        free(builder.data);
        return NULL;
    }
    builder.data[builder.length] = '\0';
    return builder.data;
}

/* ------------------------------------------------------------------------- */
/* Числа                                                                     */
/* ------------------------------------------------------------------------- */

static int parse_number(JsonParser *parser, double *result)
{
    size_t start = parser->position;
    const char *text = parser->text;

    if (text[parser->position] == '-') {
        ++parser->position;
    }

    if (text[parser->position] == '0') {
        ++parser->position;
        if (text[parser->position] >= '0' && text[parser->position] <= '9') {
            return json_error(parser, "leading zero in number");
        }
    } else if (text[parser->position] >= '1' &&
               text[parser->position] <= '9') {
        while (text[parser->position] >= '0' &&
               text[parser->position] <= '9') {
            ++parser->position;
        }
    } else {
        return json_error(parser, "invalid number");
    }

    if (text[parser->position] == '.') {
        ++parser->position;
        if (text[parser->position] < '0' || text[parser->position] > '9') {
            return json_error(parser, "digit expected after decimal point");
        }
        while (text[parser->position] >= '0' &&
               text[parser->position] <= '9') {
            ++parser->position;
        }
    }

    if (text[parser->position] == 'e' || text[parser->position] == 'E') {
        ++parser->position;
        if (text[parser->position] == '+' || text[parser->position] == '-') {
            ++parser->position;
        }
        if (text[parser->position] < '0' || text[parser->position] > '9') {
            return json_error(parser, "digit expected in exponent");
        }
        while (text[parser->position] >= '0' &&
               text[parser->position] <= '9') {
            ++parser->position;
        }
    }

    char *end = NULL;
    double value = strtod(text + start, &end);
    if (end != text + parser->position) {
        return json_error(parser, "invalid number");
    }
    if (!isfinite(value)) {
        return json_error(parser, "number is out of range");
    }

    *result = value;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Массивы и объекты                                                         */
/* ------------------------------------------------------------------------- */

static JsonValue *parse_array(JsonParser *parser)
{
    ++parser->position; /* '[' */

    JsonValue **items = NULL;
    size_t count = 0;
    size_t capacity = 0;

    skip_whitespace(parser);
    if (parser->text[parser->position] == ']') {
        ++parser->position;
    } else {
        for (;;) {
            if (count == capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                if (new_capacity > SIZE_MAX / sizeof *items) {
                    json_error(parser, "array is too large");
                    goto fail;
                }
                JsonValue **temporary =
                    realloc(items, new_capacity * sizeof *temporary);
                if (temporary == NULL) {
                    json_error(parser, "out of memory while parsing array");
                    goto fail;
                }
                items = temporary;
                capacity = new_capacity;
            }

            JsonValue *item = parse_value(parser);
            if (item == NULL) {
                goto fail;
            }
            items[count++] = item;

            skip_whitespace(parser);
            if (parser->text[parser->position] == ',') {
                ++parser->position;
                skip_whitespace(parser);
                if (parser->text[parser->position] == ']') {
                    json_error(parser, "trailing comma in array");
                    goto fail;
                }
                continue;
            }
            if (parser->text[parser->position] == ']') {
                ++parser->position;
                break;
            }
            json_error(parser, "expected ',' or ']' in array");
            goto fail;
        }
    }

    JsonValue *value = calloc(1, sizeof *value);
    if (value == NULL) {
        json_error(parser, "out of memory for array");
        goto fail;
    }
    value->type = JSON_ARRAY;
    value->as.array.items = items;
    value->as.array.count = count;
    return value;

fail:
    for (size_t i = 0; i < count; ++i) {
        json_free(items[i]);
    }
    free(items);
    return NULL;
}

static JsonValue *parse_object(JsonParser *parser)
{
    ++parser->position; /* '{' */

    char **keys = NULL;
    JsonValue **values = NULL;
    size_t count = 0;
    size_t capacity = 0;

    skip_whitespace(parser);
    if (parser->text[parser->position] == '}') {
        ++parser->position;
    } else {
        for (;;) {
            if (parser->text[parser->position] != '"') {
                json_error(parser, "expected string key");
                goto fail;
            }
            char *key = parse_string_literal(parser);
            if (key == NULL) {
                goto fail;
            }

            for (size_t i = 0; i < count; ++i) {
                if (strcmp(keys[i], key) == 0) {
                    free(key);
                    json_error(parser, "duplicate object key");
                    goto fail;
                }
            }

            skip_whitespace(parser);
            if (parser->text[parser->position] != ':') {
                free(key);
                json_error(parser, "expected ':' after key");
                goto fail;
            }
            ++parser->position;

            JsonValue *value = parse_value(parser);
            if (value == NULL) {
                free(key);
                goto fail;
            }

            if (count == capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                if (new_capacity > SIZE_MAX / sizeof *keys) {
                    free(key);
                    json_free(value);
                    json_error(parser, "object is too large");
                    goto fail;
                }
                char **new_keys =
                    realloc(keys, new_capacity * sizeof *new_keys);
                if (new_keys == NULL) {
                    free(key);
                    json_free(value);
                    json_error(parser, "out of memory while parsing object");
                    goto fail;
                }
                keys = new_keys;

                JsonValue **new_values =
                    realloc(values, new_capacity * sizeof *new_values);
                if (new_values == NULL) {
                    free(key);
                    json_free(value);
                    json_error(parser, "out of memory while parsing object");
                    goto fail;
                }
                values = new_values;
                capacity = new_capacity;
            }

            keys[count] = key;
            values[count] = value;
            ++count;

            skip_whitespace(parser);
            if (parser->text[parser->position] == ',') {
                ++parser->position;
                skip_whitespace(parser);
                if (parser->text[parser->position] == '}') {
                    json_error(parser, "trailing comma in object");
                    goto fail;
                }
                continue;
            }
            if (parser->text[parser->position] == '}') {
                ++parser->position;
                break;
            }
            json_error(parser, "expected ',' or '}' in object");
            goto fail;
        }
    }

    JsonValue *value = calloc(1, sizeof *value);
    if (value == NULL) {
        json_error(parser, "out of memory for object");
        goto fail;
    }
    value->type = JSON_OBJECT;
    value->as.object.keys = keys;
    value->as.object.values = values;
    value->as.object.count = count;
    return value;

fail:
    for (size_t i = 0; i < count; ++i) {
        free(keys[i]);
        json_free(values[i]);
    }
    free(keys);
    free(values);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Значения                                                                  */
/* ------------------------------------------------------------------------- */

static JsonValue *parse_value(JsonParser *parser)
{
    skip_whitespace(parser);
    char c = parser->text[parser->position];

    if (c == '\0') {
        json_error(parser, "unexpected end of input");
        return NULL;
    }

    if (c == '{' || c == '[') {
        if (parser->depth >= JSON_MAX_DEPTH) {
            json_error(parser, "maximum nesting depth exceeded");
            return NULL;
        }
        ++parser->depth;
        JsonValue *value = (c == '{') ? parse_object(parser)
                                      : parse_array(parser);
        --parser->depth;
        return value;
    }

    JsonValue *value = calloc(1, sizeof *value);
    if (value == NULL) {
        json_error(parser, "out of memory for JSON value");
        return NULL;
    }

    if (c == '"') {
        char *string = parse_string_literal(parser);
        if (string == NULL) {
            free(value);
            return NULL;
        }
        value->type = JSON_STRING;
        value->as.string = string;
        return value;
    }

    if (c == 't' && strncmp(parser->text + parser->position, "true", 4) == 0) {
        parser->position += 4;
        value->type = JSON_BOOL;
        value->as.boolean = 1;
        return value;
    }
    if (c == 'f' && strncmp(parser->text + parser->position, "false", 5) == 0) {
        parser->position += 5;
        value->type = JSON_BOOL;
        value->as.boolean = 0;
        return value;
    }
    if (c == 'n' && strncmp(parser->text + parser->position, "null", 4) == 0) {
        parser->position += 4;
        value->type = JSON_NULL;
        return value;
    }

    if (c == '-' || (c >= '0' && c <= '9')) {
        double number = 0.0;
        if (parse_number(parser, &number) != 0) {
            free(value);
            return NULL;
        }
        value->type = JSON_NUMBER;
        value->as.number = number;
        return value;
    }

    json_error(parser, "unexpected character");
    free(value);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Публичный интерфейс                                                       */
/* ------------------------------------------------------------------------- */

JsonValue *json_parse(const char *text, char *err, size_t errlen)
{
    JsonParser parser;
    memset(&parser, 0, sizeof parser);
    parser.text = text;
    parser.err = err;
    parser.errlen = errlen;
    if (errlen != 0) {
        err[0] = '\0';
    }

    JsonValue *value = parse_value(&parser);
    if (value == NULL) {
        return NULL;
    }

    skip_whitespace(&parser);
    if (parser.text[parser.position] != '\0') {
        json_error(&parser, "trailing data after JSON value");
        json_free(value);
        return NULL;
    }

    return value;
}

void json_free(JsonValue *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
    case JSON_NULL:
    case JSON_BOOL:
    case JSON_NUMBER:
        break;
    case JSON_STRING:
        free(value->as.string);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < value->as.array.count; ++i) {
            json_free(value->as.array.items[i]);
        }
        free(value->as.array.items);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < value->as.object.count; ++i) {
            free(value->as.object.keys[i]);
            json_free(value->as.object.values[i]);
        }
        free(value->as.object.keys);
        free(value->as.object.values);
        break;
    }

    free(value);
}

const JsonValue *json_object_get(const JsonValue *object, const char *key)
{
    if (object == NULL || object->type != JSON_OBJECT) {
        return NULL;
    }

    for (size_t i = 0; i < object->as.object.count; ++i) {
        if (strcmp(object->as.object.keys[i], key) == 0) {
            return object->as.object.values[i];
        }
    }
    return NULL;
}

const char *json_type_name(JsonType type)
{
    switch (type) {
    case JSON_NULL:
        return "null";
    case JSON_BOOL:
        return "boolean";
    case JSON_NUMBER:
        return "number";
    case JSON_STRING:
        return "string";
    case JSON_ARRAY:
        return "array";
    case JSON_OBJECT:
        return "object";
    }
    return "unknown";
}
