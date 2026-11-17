#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "database.h"
#include "database_json.h"
#include "query.h"
#include "storage.h"

enum {
    EXIT_OK = 0,
    EXIT_USAGE = 64,
    EXIT_QUERY = 65,
    EXIT_INPUT = 66,
    EXIT_OUTPUT = 73,
    EXIT_INTEGRITY = 74,
    EXIT_CORRUPT = 75
};

#define MAX_QUERY_FILE_BYTES (1024u * 1024u)

typedef struct {
    char database[1024];
    char export_file[1024];
    char config_file[1024];
    const char *query;
    const char *query_file;
    const char *import_file;

    int init;
    int seed;
    int show_stats;
    int check;
    int show_args;
    int help;

    int database_given;
    int export_given;
    int export_requested;
    int show_stats_given;
    int config_given;
} Options;

static void usage(FILE *stream)
{
    fprintf(stream,
            "Usage: vpddb --database <file> [options]\n"
            "\n"
            "Database options:\n"
            "  --database <file>   binary database file (default: students.dat)\n"
            "  --init              create an empty database (overwrites the file)\n"
            "  --seed              load the demonstration Faculty/Group/Student data\n"
            "  --import <file>     replace database contents from JSON\n"
            "  --export [file]     export database to JSON "
            "(default: export.json or config value)\n"
            "  --check             validate keys, references and indexes\n"
            "\n"
            "Query options:\n"
            "  --query <text>      execute one query; use '-' to read stdin\n"
            "  --file <file>       execute queries line by line from a text file\n"
            "\n"
            "Other options:\n"
            "  --config <file>     read simple YAML configuration\n"
            "  --show-stats        print entity counts and file size\n"
            "  --show-args         print argc and every argv element\n"
            "  --help              show this help\n"
            "\n"
            "Examples:\n"
            "  vpddb --database students.dat --init --seed\n"
            "  vpddb --database students.dat --query 'SELECT students'\n"
            "  echo 'SELECT students WHERE group_id = 11' | "
            "vpddb --database students.dat --query -\n");
}

static int copy_option(char *destination, size_t capacity,
                       const char *value, const char *option)
{
    size_t length = strlen(value);
    if (length >= capacity) {
        fprintf(stderr, "Error: value for %s is too long\n", option);
        return -1;
    }
    memcpy(destination, value, length + 1);
    return 0;
}

static int parse_options(int argc, char *argv[], Options *options)
{
    memset(options, 0, sizeof *options);
    strcpy(options->database, "students.dat");
    strcpy(options->export_file, "export.json");

    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];

        if (strcmp(argument, "--database") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --database requires a file name\n");
                return -1;
            }
            if (copy_option(options->database, sizeof options->database,
                            argv[++i], "--database") != 0) {
                return -1;
            }
            options->database_given = 1;
        } else if (strcmp(argument, "--export") == 0) {
            options->export_requested = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                if (copy_option(options->export_file,
                                sizeof options->export_file,
                                argv[++i], "--export") != 0) {
                    return -1;
                }
                options->export_given = 1;
            }
        } else if (strcmp(argument, "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --config requires a file name\n");
                return -1;
            }
            if (copy_option(options->config_file, sizeof options->config_file,
                            argv[++i], "--config") != 0) {
                return -1;
            }
            options->config_given = 1;
        } else if (strcmp(argument, "--query") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --query requires a query text\n");
                return -1;
            }
            options->query = argv[++i];
        } else if (strcmp(argument, "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --file requires a file name\n");
                return -1;
            }
            options->query_file = argv[++i];
        } else if (strcmp(argument, "--import") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --import requires a JSON file\n");
                return -1;
            }
            options->import_file = argv[++i];
        } else if (strcmp(argument, "--init") == 0) {
            options->init = 1;
        } else if (strcmp(argument, "--seed") == 0) {
            options->seed = 1;
        } else if (strcmp(argument, "--show-stats") == 0) {
            options->show_stats = 1;
            options->show_stats_given = 1;
        } else if (strcmp(argument, "--check") == 0) {
            options->check = 1;
        } else if (strcmp(argument, "--show-args") == 0) {
            options->show_args = 1;
        } else if (strcmp(argument, "--help") == 0) {
            options->help = 1;
        } else {
            fprintf(stderr, "Error: unknown option: %s\n", argument);
            return -1;
        }
    }

    return 0;
}

static void print_args(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }
}

static int apply_config(Options *options)
{
    if (!options->config_given) {
        return 0;
    }

    Config config;
    config_init(&config);
    char err[256];
    if (config_load(options->config_file, &config, err, sizeof err) != 0) {
        fprintf(stderr, "Config error: %s\n", err);
        return EXIT_QUERY;
    }

    if (!options->database_given && config.has_database) {
        if (copy_option(options->database, sizeof options->database,
                        config.database, "config database") != 0) {
            return EXIT_USAGE;
        }
    }
    if (!options->export_given && config.has_export_file) {
        if (copy_option(options->export_file, sizeof options->export_file,
                        config.export_file, "config export_file") != 0) {
            return EXIT_USAGE;
        }
    }
    if (!options->show_stats_given && config.has_show_stats) {
        options->show_stats = config.show_stats;
    }

    return 0;
}

static char *read_all(FILE *file, size_t max_bytes, size_t *length,
                      char *err, size_t errlen)
{
    size_t size = 0;
    size_t capacity = 1024;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        snprintf(err, errlen, "out of memory while reading input");
        return NULL;
    }

    unsigned char chunk[1024];
    for (;;) {
        size_t count = fread(chunk, 1, sizeof chunk, file);
        if (count != 0) {
            if (size + count + 1 > max_bytes) {
                snprintf(err, errlen, "input is too large (max %zu bytes)",
                         max_bytes);
                free(buffer);
                return NULL;
            }
            while (capacity < size + count + 1) {
                size_t new_capacity = capacity * 2;
                char *temporary = realloc(buffer, new_capacity);
                if (temporary == NULL) {
                    snprintf(err, errlen, "out of memory while reading input");
                    free(buffer);
                    return NULL;
                }
                buffer = temporary;
                capacity = new_capacity;
            }
            memcpy(buffer + size, chunk, count);
            size += count;
        }

        if (count < sizeof chunk) {
            if (ferror(file)) {
                snprintf(err, errlen, "cannot read input: %s", strerror(errno));
                free(buffer);
                return NULL;
            }
            break;
        }
    }

    buffer[size] = '\0';
    *length = size;
    return buffer;
}

static int save_database(Database *db, const char *path)
{
    char err[256];
    if (database_save(db, path, err, sizeof err) != 0) {
        fprintf(stderr, "Save error: %s\n", err);
        return EXIT_OUTPUT;
    }
    return EXIT_OK;
}

static int execute_query_text(Database *db, const char *text,
                              const char *database_path)
{
    char err[512];
    Query *query = query_parse(text, err, sizeof err);
    if (query == NULL) {
        fprintf(stderr, "Query error: %s\n", err);
        return EXIT_QUERY;
    }

    if (query->command != QUERY_SELECT) {
        char journal_error[256];
        if (storage_journal(database_path,
                            query_command_name(query->command), text,
                            journal_error, sizeof journal_error) != 0) {
            fprintf(stderr, "Warning: %s\n", journal_error);
        }
    }

    int changed = 0;
    int executed = query_execute(db, query, stdout, &changed, err, sizeof err);
    query_free(query);

    if (executed != QUERY_EXEC_OK) {
        fprintf(stderr, "Execution error: %s\n", err);
        return EXIT_INTEGRITY;
    }

    if (changed) {
        int saved = save_database(db, database_path);
        if (saved != EXIT_OK) {
            return saved;
        }
    }

    return EXIT_OK;
}

static char *trim_line(char *line)
{
    while (*line != '\0' &&
           (*line == ' ' || *line == '\t' ||
            *line == '\r' || *line == '\n')) {
        ++line;
    }

    char *end = line + strlen(line);
    while (end > line &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return line;
}

static int execute_query_file(Database *db, const char *path,
                              const char *database_path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open query file %s: %s\n",
                path, strerror(errno));
        return EXIT_INPUT;
    }

    char err[256];
    size_t length = 0;
    char *content = read_all(file, MAX_QUERY_FILE_BYTES, &length,
                             err, sizeof err);
    fclose(file);
    if (content == NULL) {
        fprintf(stderr, "Error: %s\n", err);
        return EXIT_INPUT;
    }

    int result = EXIT_OK;
    char *cursor = content;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        line = trim_line(line);
        if (*line == '\0' || *line == '#') {
            continue;
        }

        result = execute_query_text(db, line, database_path);
        if (result != EXIT_OK) {
            break;
        }
    }

    free(content);
    return result;
}

static int execute_query_argument(Database *db, const char *text,
                                  const char *database_path)
{
    if (strcmp(text, "-") != 0) {
        return execute_query_text(db, text, database_path);
    }

    char err[256];
    size_t length = 0;
    char *content = read_all(stdin, MAX_QUERY_FILE_BYTES, &length,
                             err, sizeof err);
    if (content == NULL) {
        fprintf(stderr, "Error: %s\n", err);
        return EXIT_INPUT;
    }

    char *trimmed = trim_line(content);
    if (*trimmed == '\0') {
        fprintf(stderr, "Query error: empty query on stdin\n");
        free(content);
        return EXIT_QUERY;
    }

    int result = execute_query_text(db, trimmed, database_path);
    free(content);
    return result;
}

static int show_stats(const Database *db, const char *path)
{
    DatabaseStats stats;
    database_stats(db, &stats);

    printf("database: %s\n", path);
    printf("faculties: %zu\n", stats.faculties);
    printf("groups: %zu\n", stats.groups);
    printf("students: %zu\n", stats.students);
    printf("index: open addressing, id -> position\n");

    struct stat info;
    if (stat(path, &info) == 0) {
        printf("file_bytes: %lld\n", (long long)info.st_size);
    } else {
        printf("file_bytes: unavailable (%s)\n", strerror(errno));
    }

    return EXIT_OK;
}

int main(int argc, char *argv[])
{
    Options options;
    if (parse_options(argc, argv, &options) != 0) {
        usage(stderr);
        return EXIT_USAGE;
    }

    if (options.help) {
        usage(stdout);
        return EXIT_OK;
    }

    if (options.show_args) {
        print_args(argc, argv);
    }

    int config_result = apply_config(&options);
    if (config_result != 0) {
        return config_result;
    }

    int has_action = options.init || options.seed || options.query != NULL ||
                     options.query_file != NULL || options.import_file != NULL ||
                     options.export_requested || options.show_stats ||
                     options.check;
    if (!has_action) {
        if (options.show_args) {
            return EXIT_OK;
        }
        usage(stderr);
        return EXIT_USAGE;
    }

    Database db;
    db_init(&db);
    char err[512];

    if (options.init) {
        if (options.seed) {
            if (database_seed(&db, err, sizeof err) != 0) {
                fprintf(stderr, "Seed error: %s\n", err);
                db_free(&db);
                return EXIT_INTEGRITY;
            }
        }
        int saved = save_database(&db, options.database);
        if (saved != EXIT_OK) {
            db_free(&db);
            return saved;
        }
        printf("initialized database: %s\n", options.database);
    } else {
        if (access(options.database, R_OK) != 0) {
            fprintf(stderr,
                    "Error: cannot read database %s: %s "
                    "(use --init to create it)\n",
                    options.database, strerror(errno));
            db_free(&db);
            return EXIT_INPUT;
        }
        if (storage_load(&db, options.database, err, sizeof err) != 0) {
            fprintf(stderr, "Database error: %s\n", err);
            db_free(&db);
            return EXIT_CORRUPT;
        }
    }

    if (options.seed && !options.init) {
        if (database_seed(&db, err, sizeof err) != 0) {
            fprintf(stderr, "Seed error: %s\n", err);
            db_free(&db);
            return EXIT_INTEGRITY;
        }
        int saved = save_database(&db, options.database);
        if (saved != EXIT_OK) {
            db_free(&db);
            return saved;
        }
        printf("seeded database: %s\n", options.database);
    }

    if (options.import_file != NULL) {
        int imported = json_import_database(&db, options.import_file,
                                            err, sizeof err);
        if (imported != JSON_DB_OK) {
            fprintf(stderr, "Import error: %s\n", err);
            db_free(&db);
            if (imported == JSON_DB_ERROR_FILE) {
                return EXIT_INPUT;
            }
            if (imported == JSON_DB_ERROR_PARSE) {
                return EXIT_QUERY;
            }
            return EXIT_INTEGRITY;
        }

        int saved = save_database(&db, options.database);
        if (saved != EXIT_OK) {
            db_free(&db);
            return saved;
        }
        DatabaseStats stats;
        database_stats(&db, &stats);
        printf("imported JSON: %s (faculties=%zu, groups=%zu, students=%zu)\n",
               options.import_file, stats.faculties, stats.groups,
               stats.students);
    }

    int result = EXIT_OK;

    if (options.query != NULL) {
        result = execute_query_argument(&db, options.query,
                                        options.database);
    }

    if (result == EXIT_OK && options.query_file != NULL) {
        result = execute_query_file(&db, options.query_file,
                                    options.database);
    }

    if (result == EXIT_OK && options.export_requested) {
        int exported = json_export_database(&db, options.export_file,
                                            err, sizeof err);
        if (exported != JSON_DB_OK) {
            fprintf(stderr, "Export error: %s\n", err);
            result = (exported == JSON_DB_ERROR_FILE)
                         ? EXIT_OUTPUT
                         : EXIT_INTEGRITY;
        } else {
            printf("exported database to: %s\n", options.export_file);
        }
    }

    if (result == EXIT_OK && options.show_stats) {
        result = show_stats(&db, options.database);
    }

    if (result == EXIT_OK && options.check) {
        if (db_validate(&db, err, sizeof err) != 0) {
            fprintf(stderr, "Integrity error: %s\n", err);
            result = EXIT_INTEGRITY;
        } else {
            printf("integrity: ok\n");
        }
    }

    db_free(&db);
    return result;
}
