#include "model.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Ошибки                                                                    */
/* ------------------------------------------------------------------------- */

void db_error(char *err, size_t errlen, const char *format, ...)
{
    if (err == NULL || errlen == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(err, errlen, format, args);
    va_end(args);
}

/* ------------------------------------------------------------------------- */
/* Хеш-индекс id -> позиция                                                  */
/* ------------------------------------------------------------------------- */

static uint64_t id_hash(uint32_t id)
{
    uint64_t x = (uint64_t)id + 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static void index_reset(IdIndex *index)
{
    free(index->entries);
    memset(index, 0, sizeof *index);
}

static int index_rehash(IdIndex *index, size_t new_capacity,
                        char *err, size_t errlen)
{
    IdIndexEntry *old_entries = index->entries;
    size_t old_capacity = index->capacity;

    if (new_capacity < 16) {
        new_capacity = 16;
    }

    IdIndexEntry *entries = calloc(new_capacity, sizeof *entries);
    if (entries == NULL) {
        db_error(err, errlen, "out of memory while growing id index");
        return -1;
    }

    index->entries = entries;
    index->capacity = new_capacity;
    index->size = 0;
    index->tombstones = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_entries[i].state != 1) {
            continue;
        }
        uint64_t h = id_hash(old_entries[i].id);
        size_t mask = new_capacity - 1;
        size_t slot = (size_t)h & mask;
        while (entries[slot].state == 1) {
            slot = (slot + 1) & mask;
        }
        entries[slot].id = old_entries[i].id;
        entries[slot].position = old_entries[i].position;
        entries[slot].state = 1;
        ++index->size;
    }

    free(old_entries);
    return 0;
}

static int index_ensure_capacity(IdIndex *index, char *err, size_t errlen)
{
    if (index->capacity == 0) {
        return index_rehash(index, 16, err, errlen);
    }

    if ((index->size + index->tombstones + 1) * 10 >= index->capacity * 7) {
        if (index->capacity > SIZE_MAX / 2) {
            db_error(err, errlen, "id index is too large");
            return -1;
        }
        return index_rehash(index, index->capacity * 2, err, errlen);
    }

    return 0;
}

static int index_put(IdIndex *index, uint32_t id, size_t position,
                     char *err, size_t errlen)
{
    if (index_ensure_capacity(index, err, errlen) != 0) {
        return -1;
    }

    size_t mask = index->capacity - 1;
    size_t slot = (size_t)id_hash(id) & mask;
    size_t first_tombstone = SIZE_MAX;

    for (size_t probe = 0; probe < index->capacity; ++probe) {
        IdIndexEntry *entry = &index->entries[slot];

        if (entry->state == 1 && entry->id == id) {
            entry->position = position;
            return 0;
        }
        if (entry->state == 2 && first_tombstone == SIZE_MAX) {
            first_tombstone = slot;
        }
        if (entry->state == 0) {
            if (first_tombstone != SIZE_MAX) {
                slot = first_tombstone;
                --index->tombstones;
            }
            index->entries[slot].id = id;
            index->entries[slot].position = position;
            index->entries[slot].state = 1;
            ++index->size;
            return 0;
        }

        slot = (slot + 1) & mask;
    }

    db_error(err, errlen, "id index is full");
    return -1;
}

static size_t index_get(const IdIndex *index, uint32_t id)
{
    if (index->capacity == 0) {
        return SIZE_MAX;
    }

    size_t mask = index->capacity - 1;
    size_t slot = (size_t)id_hash(id) & mask;

    for (size_t probe = 0; probe < index->capacity; ++probe) {
        const IdIndexEntry *entry = &index->entries[slot];
        if (entry->state == 0) {
            return SIZE_MAX;
        }
        if (entry->state == 1 && entry->id == id) {
            return entry->position;
        }
        slot = (slot + 1) & mask;
    }

    return SIZE_MAX;
}

static int index_remove(IdIndex *index, uint32_t id)
{
    if (index->capacity == 0) {
        return -1;
    }

    size_t mask = index->capacity - 1;
    size_t slot = (size_t)id_hash(id) & mask;

    for (size_t probe = 0; probe < index->capacity; ++probe) {
        IdIndexEntry *entry = &index->entries[slot];
        if (entry->state == 0) {
            return -1;
        }
        if (entry->state == 1 && entry->id == id) {
            entry->state = 2;
            --index->size;
            ++index->tombstones;
            return 0;
        }
        slot = (slot + 1) & mask;
    }

    return -1;
}

/* ------------------------------------------------------------------------- */
/* Динамические массивы                                                      */
/* ------------------------------------------------------------------------- */

static int grow_buffer(void **data, size_t *capacity, size_t element_size,
                       size_t needed, char *err, size_t errlen)
{
    if (*capacity >= needed) {
        return 0;
    }

    size_t new_capacity = (*capacity == 0) ? 4 : *capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            db_error(err, errlen, "table is too large");
            return -1;
        }
        new_capacity *= 2;
    }

    if (new_capacity > SIZE_MAX / element_size) {
        db_error(err, errlen, "table is too large");
        return -1;
    }

    void *temporary = realloc(*data, new_capacity * element_size);
    if (temporary == NULL) {
        db_error(err, errlen, "out of memory while growing table");
        return -1;
    }

    *data = temporary;
    *capacity = new_capacity;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Инициализация и освобождение                                              */
/* ------------------------------------------------------------------------- */

void db_init(Database *db)
{
    memset(db, 0, sizeof *db);
    db->next_faculty_id = 1;
    db->next_group_id = 1;
    db->next_student_id = 1;
}

void db_free(Database *db)
{
    free(db->faculties);
    free(db->groups);
    free(db->students);
    index_reset(&db->faculty_index);
    index_reset(&db->group_index);
    index_reset(&db->student_index);
    memset(db, 0, sizeof *db);
}

/* ------------------------------------------------------------------------- */
/* Поиск                                                                     */
/* ------------------------------------------------------------------------- */

size_t db_faculty_index_of(const Database *db, uint32_t id)
{
    return index_get(&db->faculty_index, id);
}

size_t db_group_index_of(const Database *db, uint32_t id)
{
    return index_get(&db->group_index, id);
}

size_t db_student_index_of(const Database *db, uint32_t id)
{
    return index_get(&db->student_index, id);
}

const Faculty *db_faculty_find(const Database *db, uint32_t id)
{
    size_t position = db_faculty_index_of(db, id);
    return (position == SIZE_MAX) ? NULL : &db->faculties[position];
}

const Group *db_group_find(const Database *db, uint32_t id)
{
    size_t position = db_group_index_of(db, id);
    return (position == SIZE_MAX) ? NULL : &db->groups[position];
}

const Student *db_student_find(const Database *db, uint32_t id)
{
    size_t position = db_student_index_of(db, id);
    return (position == SIZE_MAX) ? NULL : &db->students[position];
}

/* ------------------------------------------------------------------------- */
/* Проверка имён                                                             */
/* ------------------------------------------------------------------------- */

static int validate_input_name(const char *name, size_t capacity,
                               const char *entity, char *err, size_t errlen)
{
    if (name == NULL || name[0] == '\0') {
        db_error(err, errlen, "%s name must not be empty", entity);
        return -1;
    }

    size_t length = strlen(name);
    if (length >= capacity) {
        db_error(err, errlen, "%s name is too long: %zu bytes (max %zu)",
                 entity, length, capacity - 1);
        return -1;
    }

    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20 || c == 0x7f) {
            db_error(err, errlen,
                     "%s name contains a control character at byte %zu",
                     entity, i);
            return -1;
        }
    }

    return 0;
}

static int validate_stored_name(const char name[], size_t capacity,
                                const char *entity, size_t record_index,
                                char *err, size_t errlen)
{
    const char *terminator = memchr(name, '\0', capacity);
    if (terminator == NULL) {
        db_error(err, errlen, "%s[%zu].name is not NUL-terminated",
                 entity, record_index);
        return -1;
    }
    if (terminator == name) {
        db_error(err, errlen, "%s[%zu].name is empty", entity, record_index);
        return -1;
    }

    for (const unsigned char *p = (const unsigned char *)name; *p != '\0'; ++p) {
        if (*p < 0x20 || *p == 0x7f) {
            db_error(err, errlen,
                     "%s[%zu].name contains a control character",
                     entity, record_index);
            return -1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Create                                                                    */
/* ------------------------------------------------------------------------- */

static int allocate_id(uint32_t requested, uint32_t *next,
                       const char *entity, uint32_t *result,
                       char *err, size_t errlen)
{
    if (requested != 0) {
        *result = requested;
        return 0;
    }

    if (*next == 0) {
        db_error(err, errlen, "%s id space is exhausted", entity);
        return -1;
    }

    *result = *next;
    return 0;
}

static void advance_sequence(uint32_t used_id, uint32_t *next)
{
    if (used_id >= *next) {
        *next = (used_id == UINT32_MAX) ? 0 : used_id + 1;
    }
}

int db_faculty_insert(Database *db, uint32_t id, const char *name,
                      uint32_t *assigned_id, char *err, size_t errlen)
{
    if (validate_input_name(name, VPD_FACULTY_NAME_CAP,
                            "faculty", err, errlen) != 0) {
        return -1;
    }

    uint32_t use_id = 0;
    if (allocate_id(id, &db->next_faculty_id, "faculty",
                    &use_id, err, errlen) != 0) {
        return -1;
    }

    if (db_faculty_find(db, use_id) != NULL) {
        db_error(err, errlen, "faculty with id %u already exists", use_id);
        return -1;
    }

    if (grow_buffer((void **)&db->faculties, &db->faculty_capacity,
                    sizeof *db->faculties, db->faculty_count + 1,
                    err, errlen) != 0) {
        return -1;
    }

    Faculty *record = &db->faculties[db->faculty_count];
    memset(record, 0, sizeof *record);
    record->id = use_id;
    memcpy(record->name, name, strlen(name) + 1);

    if (index_put(&db->faculty_index, use_id, db->faculty_count,
                  err, errlen) != 0) {
        return -1;
    }

    ++db->faculty_count;
    advance_sequence(use_id, &db->next_faculty_id);
    if (assigned_id != NULL) {
        *assigned_id = use_id;
    }
    return 0;
}

int db_group_insert(Database *db, uint32_t id, const char *name,
                    uint32_t faculty_id, uint32_t *assigned_id,
                    char *err, size_t errlen)
{
    if (validate_input_name(name, VPD_GROUP_NAME_CAP,
                            "group", err, errlen) != 0) {
        return -1;
    }

    if (db_faculty_find(db, faculty_id) == NULL) {
        db_error(err, errlen,
                 "foreign key violation: faculty with id %u does not exist",
                 faculty_id);
        return -1;
    }

    uint32_t use_id = 0;
    if (allocate_id(id, &db->next_group_id, "group",
                    &use_id, err, errlen) != 0) {
        return -1;
    }

    if (db_group_find(db, use_id) != NULL) {
        db_error(err, errlen, "group with id %u already exists", use_id);
        return -1;
    }

    if (grow_buffer((void **)&db->groups, &db->group_capacity,
                    sizeof *db->groups, db->group_count + 1,
                    err, errlen) != 0) {
        return -1;
    }

    Group *record = &db->groups[db->group_count];
    memset(record, 0, sizeof *record);
    record->id = use_id;
    memcpy(record->name, name, strlen(name) + 1);
    record->faculty_id = faculty_id;

    if (index_put(&db->group_index, use_id, db->group_count,
                  err, errlen) != 0) {
        return -1;
    }

    ++db->group_count;
    advance_sequence(use_id, &db->next_group_id);
    if (assigned_id != NULL) {
        *assigned_id = use_id;
    }
    return 0;
}

int db_student_insert(Database *db, uint32_t id, const char *name,
                      uint32_t group_id, uint32_t *assigned_id,
                      char *err, size_t errlen)
{
    if (validate_input_name(name, VPD_STUDENT_NAME_CAP,
                            "student", err, errlen) != 0) {
        return -1;
    }

    if (db_group_find(db, group_id) == NULL) {
        db_error(err, errlen,
                 "foreign key violation: group with id %u does not exist",
                 group_id);
        return -1;
    }

    uint32_t use_id = 0;
    if (allocate_id(id, &db->next_student_id, "student",
                    &use_id, err, errlen) != 0) {
        return -1;
    }

    if (db_student_find(db, use_id) != NULL) {
        db_error(err, errlen, "student with id %u already exists", use_id);
        return -1;
    }

    if (grow_buffer((void **)&db->students, &db->student_capacity,
                    sizeof *db->students, db->student_count + 1,
                    err, errlen) != 0) {
        return -1;
    }

    Student *record = &db->students[db->student_count];
    memset(record, 0, sizeof *record);
    record->id = use_id;
    memcpy(record->name, name, strlen(name) + 1);
    record->group_id = group_id;

    if (index_put(&db->student_index, use_id, db->student_count,
                  err, errlen) != 0) {
        return -1;
    }

    ++db->student_count;
    advance_sequence(use_id, &db->next_student_id);
    if (assigned_id != NULL) {
        *assigned_id = use_id;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

int db_faculty_update(Database *db, uint32_t id,
                      int has_name, const char *name,
                      char *err, size_t errlen)
{
    if (!has_name) {
        db_error(err, errlen, "nothing to update for faculty");
        return -1;
    }

    size_t position = db_faculty_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "faculty with id %u not found", id);
        return -1;
    }

    if (validate_input_name(name, VPD_FACULTY_NAME_CAP,
                            "faculty", err, errlen) != 0) {
        return -1;
    }

    Faculty *record = &db->faculties[position];
    memset(record->name, 0, sizeof record->name);
    memcpy(record->name, name, strlen(name) + 1);
    return 0;
}

int db_group_update(Database *db, uint32_t id,
                    int has_name, const char *name,
                    int has_faculty, uint32_t faculty_id,
                    char *err, size_t errlen)
{
    if (!has_name && !has_faculty) {
        db_error(err, errlen, "nothing to update for group");
        return -1;
    }

    size_t position = db_group_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "group with id %u not found", id);
        return -1;
    }

    if (has_name && validate_input_name(name, VPD_GROUP_NAME_CAP,
                                        "group", err, errlen) != 0) {
        return -1;
    }

    if (has_faculty && db_faculty_find(db, faculty_id) == NULL) {
        db_error(err, errlen,
                 "foreign key violation: faculty with id %u does not exist",
                 faculty_id);
        return -1;
    }

    Group *record = &db->groups[position];
    if (has_name) {
        memset(record->name, 0, sizeof record->name);
        memcpy(record->name, name, strlen(name) + 1);
    }
    if (has_faculty) {
        record->faculty_id = faculty_id;
    }
    return 0;
}

int db_student_update(Database *db, uint32_t id,
                      int has_name, const char *name,
                      int has_group, uint32_t group_id,
                      char *err, size_t errlen)
{
    if (!has_name && !has_group) {
        db_error(err, errlen, "nothing to update for student");
        return -1;
    }

    size_t position = db_student_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "student with id %u not found", id);
        return -1;
    }

    if (has_name && validate_input_name(name, VPD_STUDENT_NAME_CAP,
                                        "student", err, errlen) != 0) {
        return -1;
    }

    if (has_group && db_group_find(db, group_id) == NULL) {
        db_error(err, errlen,
                 "foreign key violation: group with id %u does not exist",
                 group_id);
        return -1;
    }

    Student *record = &db->students[position];
    if (has_name) {
        memset(record->name, 0, sizeof record->name);
        memcpy(record->name, name, strlen(name) + 1);
    }
    if (has_group) {
        record->group_id = group_id;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Delete                                                                    */
/* ------------------------------------------------------------------------- */

static int update_position_after_swap(IdIndex *index, uint32_t id,
                                      size_t new_position,
                                      char *err, size_t errlen)
{
    size_t position = index_get(index, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "internal error: index entry for id %u is lost", id);
        return -1;
    }

    return index_put(index, id, new_position, err, errlen);
}

int db_faculty_delete(Database *db, uint32_t id, char *err, size_t errlen)
{
    size_t position = db_faculty_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "faculty with id %u not found", id);
        return -1;
    }

    for (size_t i = 0; i < db->group_count; ++i) {
        if (db->groups[i].faculty_id == id) {
            db_error(err, errlen,
                     "cannot delete faculty %u: group %u references it",
                     id, db->groups[i].id);
            return -1;
        }
    }

    if (index_remove(&db->faculty_index, id) != 0) {
        db_error(err, errlen, "internal error: faculty index is inconsistent");
        return -1;
    }

    size_t last = db->faculty_count - 1;
    if (position != last) {
        db->faculties[position] = db->faculties[last];
        if (update_position_after_swap(&db->faculty_index,
                                       db->faculties[position].id,
                                       position, err, errlen) != 0) {
            return -1;
        }
    }

    --db->faculty_count;
    return 0;
}

int db_group_delete(Database *db, uint32_t id, char *err, size_t errlen)
{
    size_t position = db_group_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "group with id %u not found", id);
        return -1;
    }

    for (size_t i = 0; i < db->student_count; ++i) {
        if (db->students[i].group_id == id) {
            db_error(err, errlen,
                     "cannot delete group %u: student %u references it",
                     id, db->students[i].id);
            return -1;
        }
    }

    if (index_remove(&db->group_index, id) != 0) {
        db_error(err, errlen, "internal error: group index is inconsistent");
        return -1;
    }

    size_t last = db->group_count - 1;
    if (position != last) {
        db->groups[position] = db->groups[last];
        if (update_position_after_swap(&db->group_index,
                                       db->groups[position].id,
                                       position, err, errlen) != 0) {
            return -1;
        }
    }

    --db->group_count;
    return 0;
}

int db_student_delete(Database *db, uint32_t id, char *err, size_t errlen)
{
    size_t position = db_student_index_of(db, id);
    if (position == SIZE_MAX) {
        db_error(err, errlen, "student with id %u not found", id);
        return -1;
    }

    if (index_remove(&db->student_index, id) != 0) {
        db_error(err, errlen, "internal error: student index is inconsistent");
        return -1;
    }

    size_t last = db->student_count - 1;
    if (position != last) {
        db->students[position] = db->students[last];
        if (update_position_after_swap(&db->student_index,
                                       db->students[position].id,
                                       position, err, errlen) != 0) {
            return -1;
        }
    }

    --db->student_count;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Полная проверка целостности                                               */
/* ------------------------------------------------------------------------- */

static int validate_index_entries(const IdIndex *index, const char *entity,
                                  size_t count, char *err, size_t errlen)
{
    if (index->size != count) {
        db_error(err, errlen,
                 "%s index contains %zu entries, table contains %zu records",
                 entity, index->size, count);
        return -1;
    }

    for (size_t i = 0; i < index->capacity; ++i) {
        if (index->entries[i].state != 1) {
            continue;
        }
        if (index->entries[i].position >= count) {
            db_error(err, errlen,
                     "%s index entry id %u points outside the table",
                     entity, index->entries[i].id);
            return -1;
        }
    }

    return 0;
}

static int validate_next_sequence(const char *entity, uint32_t next,
                                  uint32_t maximum, char *err, size_t errlen)
{
    if (next == 0) {
        if (maximum == UINT32_MAX) {
            return 0;
        }
        db_error(err, errlen, "%s id sequence is exhausted", entity);
        return -1;
    }
    if (next <= maximum) {
        db_error(err, errlen,
                 "%s id sequence %u is not greater than maximum id %u",
                 entity, next, maximum);
        return -1;
    }
    return 0;
}

int db_validate(const Database *db, char *err, size_t errlen)
{
    if (validate_index_entries(&db->faculty_index, "faculty",
                               db->faculty_count, err, errlen) != 0 ||
        validate_index_entries(&db->group_index, "group",
                               db->group_count, err, errlen) != 0 ||
        validate_index_entries(&db->student_index, "student",
                               db->student_count, err, errlen) != 0) {
        return -1;
    }

    uint32_t max_faculty = 0;
    for (size_t i = 0; i < db->faculty_count; ++i) {
        const Faculty *record = &db->faculties[i];
        if (record->id == 0) {
            db_error(err, errlen, "faculty[%zu] has primary key 0", i);
            return -1;
        }
        if (db_faculty_index_of(db, record->id) != i) {
            db_error(err, errlen,
                     "faculty[%zu] id %u is not at its indexed position",
                     i, record->id);
            return -1;
        }
        if (validate_stored_name(record->name, sizeof record->name,
                                 "faculty", i, err, errlen) != 0) {
            return -1;
        }
        if (record->id > max_faculty) {
            max_faculty = record->id;
        }
    }

    uint32_t max_group = 0;
    for (size_t i = 0; i < db->group_count; ++i) {
        const Group *record = &db->groups[i];
        if (record->id == 0) {
            db_error(err, errlen, "group[%zu] has primary key 0", i);
            return -1;
        }
        if (db_group_index_of(db, record->id) != i) {
            db_error(err, errlen,
                     "group[%zu] id %u is not at its indexed position",
                     i, record->id);
            return -1;
        }
        if (validate_stored_name(record->name, sizeof record->name,
                                 "group", i, err, errlen) != 0) {
            return -1;
        }
        if (db_faculty_find(db, record->faculty_id) == NULL) {
            db_error(err, errlen,
                     "group[%zu] references missing faculty %u",
                     i, record->faculty_id);
            return -1;
        }
        if (record->id > max_group) {
            max_group = record->id;
        }
    }

    uint32_t max_student = 0;
    for (size_t i = 0; i < db->student_count; ++i) {
        const Student *record = &db->students[i];
        if (record->id == 0) {
            db_error(err, errlen, "student[%zu] has primary key 0", i);
            return -1;
        }
        if (db_student_index_of(db, record->id) != i) {
            db_error(err, errlen,
                     "student[%zu] id %u is not at its indexed position",
                     i, record->id);
            return -1;
        }
        if (validate_stored_name(record->name, sizeof record->name,
                                 "student", i, err, errlen) != 0) {
            return -1;
        }
        if (db_group_find(db, record->group_id) == NULL) {
            db_error(err, errlen,
                     "student[%zu] references missing group %u",
                     i, record->group_id);
            return -1;
        }
        if (record->id > max_student) {
            max_student = record->id;
        }
    }

    if (validate_next_sequence("faculty", db->next_faculty_id,
                               max_faculty, err, errlen) != 0 ||
        validate_next_sequence("group", db->next_group_id,
                               max_group, err, errlen) != 0 ||
        validate_next_sequence("student", db->next_student_id,
                               max_student, err, errlen) != 0) {
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Перестройка индексов                                                      */
/* ------------------------------------------------------------------------- */

int db_rebuild_indexes(Database *db, char *err, size_t errlen)
{
    IdIndex faculty_index = {0};
    IdIndex group_index = {0};
    IdIndex student_index = {0};

    for (size_t i = 0; i < db->faculty_count; ++i) {
        if (index_put(&faculty_index, db->faculties[i].id, i,
                      err, errlen) != 0) {
            goto fail;
        }
    }
    for (size_t i = 0; i < db->group_count; ++i) {
        if (index_put(&group_index, db->groups[i].id, i,
                      err, errlen) != 0) {
            goto fail;
        }
    }
    for (size_t i = 0; i < db->student_count; ++i) {
        if (index_put(&student_index, db->students[i].id, i,
                      err, errlen) != 0) {
            goto fail;
        }
    }

    index_reset(&db->faculty_index);
    index_reset(&db->group_index);
    index_reset(&db->student_index);
    db->faculty_index = faculty_index;
    db->group_index = group_index;
    db->student_index = student_index;
    return 0;

fail:
    index_reset(&faculty_index);
    index_reset(&group_index);
    index_reset(&student_index);
    return -1;
}

void db_recompute_sequences(Database *db)
{
    uint32_t max_faculty = 0;
    uint32_t max_group = 0;
    uint32_t max_student = 0;

    for (size_t i = 0; i < db->faculty_count; ++i) {
        if (db->faculties[i].id > max_faculty) {
            max_faculty = db->faculties[i].id;
        }
    }
    for (size_t i = 0; i < db->group_count; ++i) {
        if (db->groups[i].id > max_group) {
            max_group = db->groups[i].id;
        }
    }
    for (size_t i = 0; i < db->student_count; ++i) {
        if (db->students[i].id > max_student) {
            max_student = db->students[i].id;
        }
    }

    db->next_faculty_id = (max_faculty == UINT32_MAX) ? 0 : max_faculty + 1;
    db->next_group_id = (max_group == UINT32_MAX) ? 0 : max_group + 1;
    db->next_student_id = (max_student == UINT32_MAX) ? 0 : max_student + 1;
}
