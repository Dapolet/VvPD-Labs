#ifndef VPD_MODEL_H
#define VPD_MODEL_H

#include <stddef.h>
#include <stdint.h>

/*
 * VPD Lab 05 — учебная схема «Faculty -> Group -> Student».
 *
 * 1НФ: каждое поле содержит одно скалярное значение.
 * 2НФ: у каждой таблицы простой первичный ключ id, поэтому частичных
 *      зависимостей от составного ключа нет.
 * 3НФ: название факультета хранится только в Faculty, название группы —
 *      только в Group; в Student остаются только внешние ключи.
 */

#define VPD_FACULTY_NAME_CAP 64
#define VPD_GROUP_NAME_CAP   32
#define VPD_STUDENT_NAME_CAP 64

typedef struct {
    uint32_t id;
    char name[VPD_FACULTY_NAME_CAP];
} Faculty;

typedef struct {
    uint32_t id;
    char name[VPD_GROUP_NAME_CAP];
    uint32_t faculty_id;
} Group;

typedef struct {
    uint32_t id;
    char name[VPD_STUDENT_NAME_CAP];
    uint32_t group_id;
} Student;

/*
 * Индекс «id -> позиция в массиве» на основе открытой адресации.
 * Это расширение повышенного уровня: поиск по первичному ключу не
 * требует последовательного просмотра всей таблицы.
 */
typedef struct {
    uint32_t id;
    size_t position;
    unsigned char state; /* 0 — пусто, 1 — занято, 2 — надгробие */
} IdIndexEntry;

typedef struct {
    IdIndexEntry *entries;
    size_t capacity;
    size_t size;
    size_t tombstones;
} IdIndex;

typedef struct {
    Faculty *faculties;
    size_t faculty_count;
    size_t faculty_capacity;

    Group *groups;
    size_t group_count;
    size_t group_capacity;

    Student *students;
    size_t student_count;
    size_t student_capacity;

    IdIndex faculty_index;
    IdIndex group_index;
    IdIndex student_index;

    /* Следующий свободный идентификатор для команды INSERT ... 0 "..." */
    uint32_t next_faculty_id;
    uint32_t next_group_id;
    uint32_t next_student_id;
} Database;

void db_init(Database *db);
void db_free(Database *db);

/* Единый формат сообщений об ошибках для всех слоёв. */
void db_error(char *err, size_t errlen, const char *format, ...);

/* Индексы и проверки целостности. */
int db_rebuild_indexes(Database *db, char *err, size_t errlen);
void db_recompute_sequences(Database *db);
int db_validate(const Database *db, char *err, size_t errlen);

/* Поиск по первичному ключу: O(1) в среднем благодаря индексу. */
const Faculty *db_faculty_find(const Database *db, uint32_t id);
const Group *db_group_find(const Database *db, uint32_t id);
const Student *db_student_find(const Database *db, uint32_t id);

size_t db_faculty_index_of(const Database *db, uint32_t id);
size_t db_group_index_of(const Database *db, uint32_t id);
size_t db_student_index_of(const Database *db, uint32_t id);

/*
 * Create. id == 0 означает «выдать следующий свободный идентификатор».
 * Если assigned_id не NULL, фактический идентификатор возвращается туда.
 * Возврат: 0 — успех, -1 — ошибка (текст в err).
 */
int db_faculty_insert(Database *db, uint32_t id, const char *name,
                      uint32_t *assigned_id, char *err, size_t errlen);
int db_group_insert(Database *db, uint32_t id, const char *name,
                    uint32_t faculty_id, uint32_t *assigned_id,
                    char *err, size_t errlen);
int db_student_insert(Database *db, uint32_t id, const char *name,
                      uint32_t group_id, uint32_t *assigned_id,
                      char *err, size_t errlen);

/* Update. Изменять разрешено только неключевые поля. */
int db_faculty_update(Database *db, uint32_t id,
                      int has_name, const char *name,
                      char *err, size_t errlen);
int db_group_update(Database *db, uint32_t id,
                    int has_name, const char *name,
                    int has_faculty, uint32_t faculty_id,
                    char *err, size_t errlen);
int db_student_update(Database *db, uint32_t id,
                      int has_name, const char *name,
                      int has_group, uint32_t group_id,
                      char *err, size_t errlen);

/* Delete с проверкой ссылочной целостности. */
int db_faculty_delete(Database *db, uint32_t id, char *err, size_t errlen);
int db_group_delete(Database *db, uint32_t id, char *err, size_t errlen);
int db_student_delete(Database *db, uint32_t id, char *err, size_t errlen);

#endif /* VPD_MODEL_H */
