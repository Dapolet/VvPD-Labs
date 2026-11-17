#ifndef VPD_CONFIG_H
#define VPD_CONFIG_H

#include <stddef.h>

typedef struct {
    char database[1024];
    char export_file[1024];
    int show_stats;

    int has_database;
    int has_export_file;
    int has_show_stats;
} Config;

void config_init(Config *config);

/*
 * Минимальный YAML-подобный разбор: ключ: значение, комментарии с #,
 * логические true/false. Вложенность и табуляция не поддерживаются —
 * в лабораторной YAML нужен только для сравнения с JSON.
 */
int config_load(const char *path, Config *config,
                char *err, size_t errlen);

#endif /* VPD_CONFIG_H */
