#ifndef VPD_STORAGE_H
#define VPD_STORAGE_H

#include <stddef.h>

#include "model.h"

#define VPD_STORAGE_VERSION 1u

/*
 * Сохраняет базу в двоичный файл версии 1.
 * Запись идёт во временный файл <path>.tmp, затем выполняется атомарный
 * rename(). Поэтому при аварии посреди записи старый файл остаётся целым.
 */
int storage_save(const Database *db, const char *path,
                 char *err, size_t errlen);

/*
 * Загружает базу, проверяя magic, версию, размеры, контрольную сумму,
 * имена, первичные и внешние ключи. При ошибке исходная база не меняется.
 */
int storage_load(Database *db, const char *path,
                 char *err, size_t errlen);

/*
 * Дописывает строку в журнал изменений <path>.journal перед мутирующим
 * запросом. Это учебный audit log, а не полноценный WAL с replay.
 */
int storage_journal(const char *database_path, const char *command,
                    const char *detail, char *err, size_t errlen);

#endif /* VPD_STORAGE_H */
