#include "database_json.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define JSON_DB_MAX_BYTES (16u * 1024u * 1024u)

/* ------------------------------------------------------------------------- */
/* Экспорт                                                                   */
/* ------------------------------------------------------------------------- */

static int write_json_string(FILE *file, const char *text)
{
    if (fputc('"', file) == EOF) {
        return -1;
    }

    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", file) == EOF) {
                return -1;
            }
            break;
        case '\\':
            if (fputs("\\\\", file) == EOF) {
                return -1;
            }
            break;
        case '\b':
            if (fputs("\\b", file) == EOF) {
                return -1;
            }
            break;
        case '\f':
            if (fputs("\\f", file) == EOF) {
                return -1;
            }
            break;
        case '\n':
            if (fputs("\\n", file) == EOF) {
                return -1;
            }
            break;
        case '\r':
            if (fputs("\\r", file) == EOF) {
                return -1;
            }
            break;
        case '\t':
            if (fputs("\\t", file) == EOF) {
                return -1;
            }
            break;
        default:
            if (*p < 0x20) {
                if (fprintf(file, "\\u%04x", *p) < 0) {
                    return -1;
                }
            } else if (fputc(*p, file) == EOF) {
                return -1;
            }
            break;
        }
    }

    return fputc('"', file) == EOF ? -1 : 0;
}

int json_export_database(const Database *db, const char *path,
                         char *err, size_t errlen)
{
    if (db_validate(db, err, errlen) != 0) {
        return JSON_DB_ERROR_DATA;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        db_error(err, errlen, "cannot open %s for writing: %s",
                 path, strerror(errno));
        return JSON_DB_ERROR_FILE;
    }

    int failed = 0;

    if (fprintf(file,
                "{\n"
                "  \"format\": \"vpd-lab05\",\n"
                "  \"version\": 1,\n") < 0) {
        failed = 1;
    }

    if (!failed && fputs("  \"faculties\": [", file) == EOF) {
        failed = 1;
    }
    for (size_t i = 0; !failed && i < db->faculty_count; ++i) {
        if (fprintf(file, "%s\n    {\n      \"id\": %" PRIu32
                          ",\n      \"name\": ",
                    i == 0 ? "" : ",", db->faculties[i].id) < 0 ||
            write_json_string(file, db->faculties[i].name) != 0 ||
            fputs("\n    }", file) == EOF) {
            failed = 1;
        }
    }
    if (!failed && fputs(db->faculty_count == 0 ? "],\n" : "\n  ],\n", file) == EOF) {
        failed = 1;
    }

    if (!failed && fputs("  \"groups\": [", file) == EOF) {
        failed = 1;
    }
    for (size_t i = 0; !failed && i < db->group_count; ++i) {
        if (fprintf(file, "%s\n    {\n      \"id\": %" PRIu32
                          ",\n      \"name\": ",
                    i == 0 ? "" : ",", db->groups[i].id) < 0 ||
            write_json_string(file, db->groups[i].name) != 0 ||
            fprintf(file, ",\n      \"faculty_id\": %" PRIu32 "\n    }",
                    db->groups[i].faculty_id) < 0) {
            failed = 1;
        }
    }
    if (!failed && fputs(db->group_count == 0 ? "],\n" : "\n  ],\n", file) == EOF) {
        failed = 1;
    }

    if (!failed && fputs("  \"students\": [", file) == EOF) {
        failed = 1;
    }
    for (size_t i = 0; !failed && i < db->student_count; ++i) {
        if (fprintf(file, "%s\n    {\n      \"id\": %" PRIu32
                          ",\n      \"name\": ",
                    i == 0 ? "" : ",", db->students[i].id) < 0 ||
            write_json_string(file, db->students[i].name) != 0 ||
            fprintf(file, ",\n      \"group_id\": %" PRIu32 "\n    }",
                    db->students[i].group_id) < 0) {
            failed = 1;
        }
    }
    if (!failed && fputs(db->student_count == 0 ? "]\n" : "\n  ]\n", file) == EOF) {
        failed = 1;
    }

    if (!failed && fputs("}\n", file) == EOF) {
        failed = 1;
    }
    if (!failed && fflush(file) != 0) {
        failed = 1;
    }
    if (fclose(file) != 0) {
        failed = 1;
    }

    if (failed) {
        db_error(err, errlen, "cannot write JSON export to %s: %s",
                 path, strerror(errno));
        remove(path);
        return JSON_DB_ERROR_FILE;
    }

    return JSON_DB_OK;
}

/* ------------------------------------------------------------------------- */
/* Импорт: чтение файла                                                      */
/* ------------------------------------------------------------------------- */

static int read_json_file(const char *path, char **text,
                          char *err, size_t errlen)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        db_error(err, errlen, "cannot open JSON file %s: %s",
                 path, strerror(errno));
        return JSON_DB_ERROR_FILE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        db_error(err, errlen, "cannot seek in %s: %s", path, strerror(errno));
        fclose(file);
        return JSON_DB_ERROR_FILE;
    }
    long end = ftell(file);
    if (end < 0 || (unsigned long long)end > JSON_DB_MAX_BYTES) {
        db_error(err, errlen, "JSON file %s is too large or unreadable", path);
        fclose(file);
        return JSON_DB_ERROR_FILE;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        db_error(err, errlen, "cannot rewind %s: %s", path, strerror(errno));
        fclose(file);
        return JSON_DB_ERROR_FILE;
    }

    size_t length = (size_t)end;
    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        db_error(err, errlen, "out of memory while reading %s", path);
        fclose(file);
        return JSON_DB_ERROR_FILE;
    }

    if (length != 0 && fread(buffer, 1, length, file) != length) {
        db_error(err, errlen, "cannot read JSON file %s: %s",
                 path, strerror(errno));
        free(buffer);
        fclose(file);
        return JSON_DB_ERROR_FILE;
    }
    buffer[length] = '\0';

    if (fclose(file) != 0) {
        db_error(err, errlen, "cannot close %s: %s", path, strerror(errno));
        free(buffer);
        return JSON_DB_ERROR_FILE;
    }

    *text = buffer;
    return JSON_DB_OK;
}

/* ------------------------------------------------------------------------- */
/* Импорт: поля                                                              */
/* ------------------------------------------------------------------------- */

static int require_object(const JsonValue *value, const char *what,
                          char *err, size_t errlen)
{
    if (value == NULL || value->type != JSON_OBJECT) {
        db_error(err, errlen, "%s must be a JSON object", what);
        return -1;
    }
    return 0;
}

static int get_uint32_field(const JsonValue *object, const char *field,
                            int require_positive, uint32_t *result,
                            char *err, size_t errlen)
{
    const JsonValue *value = json_object_get(object, field);
    if (value == NULL) {
        db_error(err, errlen, "missing required field '%s'", field);
        return -1;
    }
    if (value->type != JSON_NUMBER) {
        db_error(err, errlen, "field '%s' must be a number", field);
        return -1;
    }

    double number = value->as.number;
    if (number < 0.0 || number > 4294967295.0 ||
        number != (double)(uint32_t)number) {
        db_error(err, errlen,
                 "field '%s' must be an integer in uint32_t range", field);
        return -1;
    }

    uint32_t converted = (uint32_t)number;
    if (require_positive && converted == 0) {
        db_error(err, errlen, "field '%s' must be positive", field);
        return -1;
    }

    *result = converted;
    return 0;
}

static int get_string_field(const JsonValue *object, const char *field,
                            const char **result, char *err, size_t errlen)
{
    const JsonValue *value = json_object_get(object, field);
    if (value == NULL) {
        db_error(err, errlen, "missing required field '%s'", field);
        return -1;
    }
    if (value->type != JSON_STRING) {
        db_error(err, errlen, "field '%s' must be a string", field);
        return -1;
    }
    *result = value->as.string;
    return 0;
}

static int import_faculties(Database *db, const JsonValue *array,
                            char *err, size_t errlen)
{
    if (array == NULL) {
        return 0;
    }
    if (array->type != JSON_ARRAY) {
        db_error(err, errlen, "'faculties' must be an array");
        return -1;
    }

    for (size_t i = 0; i < array->as.array.count; ++i) {
        const JsonValue *item = array->as.array.items[i];
        char what[64];
        snprintf(what, sizeof what, "faculties[%zu]", i);
        if (require_object(item, what, err, errlen) != 0) {
            return -1;
        }

        uint32_t id = 0;
        const char *name = NULL;
        if (get_uint32_field(item, "id", 1, &id, err, errlen) != 0 ||
            get_string_field(item, "name", &name, err, errlen) != 0 ||
            db_faculty_insert(db, id, name, NULL, err, errlen) != 0) {
            return -1;
        }
    }
    return 0;
}

static int import_groups(Database *db, const JsonValue *array,
                         char *err, size_t errlen)
{
    if (array == NULL) {
        return 0;
    }
    if (array->type != JSON_ARRAY) {
        db_error(err, errlen, "'groups' must be an array");
        return -1;
    }

    for (size_t i = 0; i < array->as.array.count; ++i) {
        const JsonValue *item = array->as.array.items[i];
        char what[64];
        snprintf(what, sizeof what, "groups[%zu]", i);
        if (require_object(item, what, err, errlen) != 0) {
            return -1;
        }

        uint32_t id = 0;
        uint32_t faculty_id = 0;
        const char *name = NULL;
        if (get_uint32_field(item, "id", 1, &id, err, errlen) != 0 ||
            get_string_field(item, "name", &name, err, errlen) != 0 ||
            get_uint32_field(item, "faculty_id", 1, &faculty_id,
                             err, errlen) != 0 ||
            db_group_insert(db, id, name, faculty_id,
                            NULL, err, errlen) != 0) {
            return -1;
        }
    }
    return 0;
}

static int import_students(Database *db, const JsonValue *array,
                           char *err, size_t errlen)
{
    if (array == NULL) {
        return 0;
    }
    if (array->type != JSON_ARRAY) {
        db_error(err, errlen, "'students' must be an array");
        return -1;
    }

    for (size_t i = 0; i < array->as.array.count; ++i) {
        const JsonValue *item = array->as.array.items[i];
        char what[64];
        snprintf(what, sizeof what, "students[%zu]", i);
        if (require_object(item, what, err, errlen) != 0) {
            return -1;
        }

        uint32_t id = 0;
        uint32_t group_id = 0;
        const char *name = NULL;
        if (get_uint32_field(item, "id", 1, &id, err, errlen) != 0 ||
            get_string_field(item, "name", &name, err, errlen) != 0 ||
            get_uint32_field(item, "group_id", 1, &group_id,
                             err, errlen) != 0 ||
            db_student_insert(db, id, name, group_id,
                              NULL, err, errlen) != 0) {
            return -1;
        }
    }
    return 0;
}

int json_import_database(Database *db, const char *path,
                         char *err, size_t errlen)
{
    char *text = NULL;
    int result = read_json_file(path, &text, err, errlen);
    if (result != JSON_DB_OK) {
        return result;
    }

    char parse_error[256];
    JsonValue *root = json_parse(text, parse_error, sizeof parse_error);
    free(text);
    if (root == NULL) {
        db_error(err, errlen, "%s", parse_error);
        return JSON_DB_ERROR_PARSE;
    }

    if (root->type != JSON_OBJECT) {
        db_error(err, errlen, "JSON root must be an object");
        json_free(root);
        return JSON_DB_ERROR_DATA;
    }

    const JsonValue *faculties = json_object_get(root, "faculties");
    const JsonValue *groups = json_object_get(root, "groups");
    const JsonValue *students = json_object_get(root, "students");

    if (faculties == NULL && groups == NULL && students == NULL) {
        db_error(err, errlen,
                 "JSON must contain at least one of "
                 "'faculties', 'groups', 'students'");
        json_free(root);
        return JSON_DB_ERROR_DATA;
    }

    Database temporary;
    db_init(&temporary);

    if (import_faculties(&temporary, faculties, err, errlen) != 0 ||
        import_groups(&temporary, groups, err, errlen) != 0 ||
        import_students(&temporary, students, err, errlen) != 0 ||
        db_validate(&temporary, err, errlen) != 0) {
        db_free(&temporary);
        json_free(root);
        return JSON_DB_ERROR_DATA;
    }

    db_free(db);
    *db = temporary;
    json_free(root);
    return JSON_DB_OK;
}
