#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

void config_init(Config *config)
{
    memset(config, 0, sizeof *config);
}

static char *trim(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static int copy_value(char *destination, size_t capacity,
                      const char *value, const char *key,
                      char *err, size_t errlen)
{
    size_t length = strlen(value);
    if (length >= capacity) {
        snprintf(err, errlen, "config value for %s is too long", key);
        return -1;
    }
    memcpy(destination, value, length + 1);
    return 0;
}

int config_load(const char *path, Config *config,
                char *err, size_t errlen)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        snprintf(err, errlen, "cannot open config %s: %s",
                 path, strerror(errno));
        return -1;
    }

    char line[2048];
    size_t line_number = 0;
    int result = 0;

    while (fgets(line, sizeof line, file) != NULL) {
        ++line_number;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            snprintf(err, errlen, "config %s:%zu: line is too long",
                     path, line_number);
            result = -1;
            break;
        }

        if (line[0] == '\t') {
            snprintf(err, errlen,
                     "config %s:%zu: tabs are not allowed for indentation",
                     path, line_number);
            result = -1;
            break;
        }

        char *content = trim(line);
        if (*content == '\0' || *content == '#') {
            continue;
        }

        char *colon = strchr(content, ':');
        if (colon == NULL) {
            snprintf(err, errlen, "config %s:%zu: expected key: value",
                     path, line_number);
            result = -1;
            break;
        }

        *colon = '\0';
        char *key = trim(content);
        char *value = trim(colon + 1);

        if (*key == '\0') {
            snprintf(err, errlen, "config %s:%zu: empty key",
                     path, line_number);
            result = -1;
            break;
        }

        size_t value_length = strlen(value);
        if (value_length >= 2 && value[0] == '"' && value[value_length - 1] == '"') {
            value[value_length - 1] = '\0';
            ++value;
            value_length -= 2;
        } else if (value_length >= 2 && value[0] == '\'' &&
                   value[value_length - 1] == '\'') {
            value[value_length - 1] = '\0';
            ++value;
            value_length -= 2;
        }
        (void)value_length;

        if (strcmp(key, "database") == 0) {
            if (copy_value(config->database, sizeof config->database,
                           value, key, err, errlen) != 0) {
                result = -1;
                break;
            }
            config->has_database = 1;
        } else if (strcmp(key, "export_file") == 0) {
            if (copy_value(config->export_file, sizeof config->export_file,
                           value, key, err, errlen) != 0) {
                result = -1;
                break;
            }
            config->has_export_file = 1;
        } else if (strcmp(key, "show_stats") == 0) {
            if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
                config->show_stats = 1;
            } else if (strcmp(value, "false") == 0 ||
                       strcmp(value, "0") == 0) {
                config->show_stats = 0;
            } else {
                snprintf(err, errlen,
                         "config %s:%zu: show_stats must be true or false",
                         path, line_number);
                result = -1;
                break;
            }
            config->has_show_stats = 1;
        } else {
            snprintf(err, errlen, "config %s:%zu: unknown key '%s'",
                     path, line_number, key);
            result = -1;
            break;
        }
    }

    if (ferror(file)) {
        snprintf(err, errlen, "cannot read config %s: %s",
                 path, strerror(errno));
        result = -1;
    }

    if (fclose(file) != 0 && result == 0) {
        snprintf(err, errlen, "cannot close config %s: %s",
                 path, strerror(errno));
        result = -1;
    }

    return result;
}
