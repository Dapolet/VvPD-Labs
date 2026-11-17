#include "database.h"

#include "storage.h"

void database_stats(const Database *db, DatabaseStats *stats)
{
    stats->faculties = db->faculty_count;
    stats->groups = db->group_count;
    stats->students = db->student_count;
}

int database_save(const Database *db, const char *path,
                  char *err, size_t errlen)
{
    return storage_save(db, path, err, errlen);
}

int database_seed(Database *db, char *err, size_t errlen)
{
    if (db->faculty_count != 0 || db->group_count != 0 ||
        db->student_count != 0) {
        db_error(err, errlen, "seed requires an empty database");
        return -1;
    }

    if (db_faculty_insert(db, 1, "Institute of Information Technology",
                          NULL, err, errlen) != 0 ||
        db_faculty_insert(db, 2, "Institute of Radio Engineering",
                          NULL, err, errlen) != 0) {
        return -1;
    }

    if (db_group_insert(db, 11, "M3O-119BV-26", 1, NULL, err, errlen) != 0 ||
        db_group_insert(db, 12, "M3O-118BV-26", 1, NULL, err, errlen) != 0 ||
        db_group_insert(db, 21, "RT-119BV-26", 2, NULL, err, errlen) != 0) {
        return -1;
    }

    if (db_student_insert(db, 301, "Ivan Ivanov", 11, NULL, err, errlen) != 0 ||
        db_student_insert(db, 302, "Petr Petrov", 11, NULL, err, errlen) != 0 ||
        db_student_insert(db, 303, "Anna Sidorova", 12, NULL, err, errlen) != 0 ||
        db_student_insert(db, 304, "Ivan Smirnov", 12, NULL, err, errlen) != 0 ||
        db_student_insert(db, 305, "Maria Volkova", 21, NULL, err, errlen) != 0) {
        return -1;
    }

    return 0;
}
