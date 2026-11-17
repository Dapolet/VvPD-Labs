#ifndef VPD_DATABASE_H
#define VPD_DATABASE_H

#include <stddef.h>

#include "model.h"

typedef struct {
    size_t faculties;
    size_t groups;
    size_t students;
} DatabaseStats;

void database_stats(const Database *db, DatabaseStats *stats);

/*
 * Наполняет пустую базу демонстрационными данными.
 * Требует пустых таблиц, чтобы повторный запуск не создавал дубликаты.
 */
int database_seed(Database *db, char *err, size_t errlen);

/* Тонкая обёртка над storage_save: удобная точка для уровня приложения. */
int database_save(const Database *db, const char *path,
                  char *err, size_t errlen);

#endif /* VPD_DATABASE_H */
