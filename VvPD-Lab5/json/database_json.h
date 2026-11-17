#ifndef VPD_DATABASE_JSON_H
#define VPD_DATABASE_JSON_H

#include <stddef.h>

#include "model.h"

#define JSON_DB_OK           0
#define JSON_DB_ERROR_FILE   1
#define JSON_DB_ERROR_PARSE  2
#define JSON_DB_ERROR_DATA   3

/*
 * Сериализация всей базы в JSON: faculties, groups, students.
 * JSON здесь — формат обмена/экспорта, а не внутренний формат хранения.
 */
int json_export_database(const Database *db, const char *path,
                         char *err, size_t errlen);

/*
 * Десериализация JSON. Если файл повреждён или нарушены ключи, исходная
 * база не изменяется: импорт строится во временной Database.
 */
int json_import_database(Database *db, const char *path,
                         char *err, size_t errlen);

#endif /* VPD_DATABASE_JSON_H */
