#include "json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_DEMO_MAX_BYTES (16u * 1024u * 1024u)

static int read_file(const char *path, char **text)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Cannot seek in %s\n", path);
        fclose(file);
        return -1;
    }
    long end = ftell(file);
    if (end < 0 || (unsigned long long)end > JSON_DEMO_MAX_BYTES) {
        fprintf(stderr, "File %s is too large or unreadable\n", path);
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Cannot rewind %s\n", path);
        fclose(file);
        return -1;
    }

    size_t length = (size_t)end;
    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory while reading %s\n", path);
        fclose(file);
        return -1;
    }
    if (length != 0 && fread(buffer, 1, length, file) != length) {
        fprintf(stderr, "Cannot read %s\n", path);
        free(buffer);
        fclose(file);
        return -1;
    }
    buffer[length] = '\0';

    if (fclose(file) != 0) {
        fprintf(stderr, "Cannot close %s\n", path);
        free(buffer);
        return -1;
    }

    *text = buffer;
    return 0;
}

static void count_types(const JsonValue *value, size_t counts[6])
{
    ++counts[value->type];

    if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->as.array.count; ++i) {
            count_types(value->as.array.items[i], counts);
        }
    } else if (value->type == JSON_OBJECT) {
        for (size_t i = 0; i < value->as.object.count; ++i) {
            count_types(value->as.object.values[i], counts);
        }
    }
}

static void print_string_field(const JsonValue *object, const char *key)
{
    const JsonValue *value = json_object_get(object, key);
    if (value != NULL && value->type == JSON_STRING) {
        printf("%s: %s\n", key, value->as.string);
    }
}

static void print_statham_summary(const JsonValue *root)
{
    print_string_field(root, "author");
    print_string_field(root, "subject");

    const JsonValue *number = json_object_get(root, "quote_count");
    if (number != NULL && number->type == JSON_NUMBER) {
        printf("quote_count field: %.0f\n", number->as.number);
    }

    const JsonValue *flag = json_object_get(root, "memes_are_verified");
    if (flag != NULL && flag->type == JSON_BOOL) {
        printf("memes_are_verified: %s\n", flag->as.boolean ? "true" : "false");
    }

    const JsonValue *tags = json_object_get(root, "tags");
    if (tags != NULL && tags->type == JSON_ARRAY) {
        printf("tags (%zu):", tags->as.array.count);
        for (size_t i = 0; i < tags->as.array.count; ++i) {
            const JsonValue *item = tags->as.array.items[i];
            if (item->type == JSON_STRING) {
                printf(" %s", item->as.string);
            }
        }
        printf("\n");
    }

    const JsonValue *source = json_object_get(root, "primary_source");
    if (source != NULL) {
        printf("primary_source type: %s", json_type_name(source->type));
        if (source->type == JSON_NULL) {
            printf(" (value: null)");
        }
        printf("\n");
    }

    const JsonValue *quotes = json_object_get(root, "quotes");
    if (quotes != NULL && quotes->type == JSON_ARRAY) {
        printf("quotes: %zu\n", quotes->as.array.count);
        for (size_t i = 0; i < quotes->as.array.count; ++i) {
            const JsonValue *item = quotes->as.array.items[i];
            const JsonValue *text = json_object_get(item, "text");
            const JsonValue *category = json_object_get(item, "category");
            const JsonValue *verified = json_object_get(item, "verified");
            const JsonValue *times = json_object_get(item, "times_quoted");

            printf("quote[%zu]: verified=%s", i,
                   verified != NULL && verified->type == JSON_BOOL &&
                           verified->as.boolean ? "true" : "false");
            if (category != NULL && category->type == JSON_STRING) {
                printf(" category=%s", category->as.string);
            }
            if (times != NULL && times->type == JSON_NUMBER) {
                printf(" times_quoted=%.0f", times->as.number);
            }
            printf(" text=");
            if (text != NULL && text->type == JSON_STRING) {
                printf("\"%s\"", text->as.string);
            } else {
                printf("<missing>");
            }
            printf("\n");
        }
    }
}

static int demo_file(const char *path)
{
    char *text = NULL;
    if (read_file(path, &text) != 0) {
        return -1;
    }

    char error[256];
    JsonValue *root = json_parse(text, error, sizeof error);
    free(text);
    if (root == NULL) {
        fprintf(stderr, "%s: %s\n", path, error);
        return -1;
    }

    size_t counts[6] = {0};
    count_types(root, counts);

    printf("file: %s\n", path);
    printf("root: %s\n", json_type_name(root->type));
    printf("types: object=%zu array=%zu string=%zu number=%zu "
           "boolean=%zu null=%zu\n",
           counts[JSON_OBJECT], counts[JSON_ARRAY], counts[JSON_STRING],
           counts[JSON_NUMBER], counts[JSON_BOOL], counts[JSON_NULL]);

    if (root->type == JSON_OBJECT) {
        print_statham_summary(root);
    }

    json_free(root);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *paths[8];
    size_t path_count = 0;

    if (argc <= 1) {
        paths[path_count++] = "json/statham.json";
    } else {
        for (int i = 1; i < argc; ++i) {
            if (path_count >= sizeof paths / sizeof paths[0]) {
                fprintf(stderr, "Too many files\n");
                return 1;
            }
            paths[path_count++] = argv[i];
        }
    }

    for (size_t i = 0; i < path_count; ++i) {
        if (i != 0) {
            printf("\n");
        }
        if (demo_file(paths[i]) != 0) {
            return 1;
        }
    }

    return 0;
}
