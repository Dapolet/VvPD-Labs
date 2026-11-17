#include "query.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Сравнение значений                                                        */
/* ------------------------------------------------------------------------- */

static int compare_uint32(uint32_t left, uint32_t right,
                          QueryCompare compare)
{
    switch (compare) {
    case COMPARE_EQ:
        return left == right;
    case COMPARE_NE:
        return left != right;
    case COMPARE_LT:
        return left < right;
    case COMPARE_LE:
        return left <= right;
    case COMPARE_GT:
        return left > right;
    case COMPARE_GE:
        return left >= right;
    }
    return 0;
}

static int compare_string(const char *left, const char *right,
                          QueryCompare compare)
{
    int result = strcmp(left, right);
    switch (compare) {
    case COMPARE_EQ:
        return result == 0;
    case COMPARE_NE:
        return result != 0;
    case COMPARE_LT:
        return result < 0;
    case COMPARE_LE:
        return result <= 0;
    case COMPARE_GT:
        return result > 0;
    case COMPARE_GE:
        return result >= 0;
    }
    return 0;
}

static int match_faculty(const Faculty *record, const Query *query)
{
    for (size_t i = 0; i < query->condition_count; ++i) {
        const QueryCondition *condition = &query->conditions[i];
        int matched = 0;

        switch (condition->field) {
        case FIELD_ID:
            matched = compare_uint32(record->id, condition->value.number,
                                     condition->compare);
            break;
        case FIELD_NAME:
            matched = compare_string(record->name, condition->value.string,
                                     condition->compare);
            break;
        case FIELD_GROUP_ID:
        case FIELD_FACULTY_ID:
            return 0;
        }

        if (!matched) {
            return 0;
        }
    }
    return 1;
}

static int match_group(const Group *record, const Query *query)
{
    for (size_t i = 0; i < query->condition_count; ++i) {
        const QueryCondition *condition = &query->conditions[i];
        int matched = 0;

        switch (condition->field) {
        case FIELD_ID:
            matched = compare_uint32(record->id, condition->value.number,
                                     condition->compare);
            break;
        case FIELD_NAME:
            matched = compare_string(record->name, condition->value.string,
                                     condition->compare);
            break;
        case FIELD_FACULTY_ID:
            matched = compare_uint32(record->faculty_id,
                                     condition->value.number,
                                     condition->compare);
            break;
        case FIELD_GROUP_ID:
            return 0;
        }

        if (!matched) {
            return 0;
        }
    }
    return 1;
}

static int match_student(const Student *record, const Query *query)
{
    for (size_t i = 0; i < query->condition_count; ++i) {
        const QueryCondition *condition = &query->conditions[i];
        int matched = 0;

        switch (condition->field) {
        case FIELD_ID:
            matched = compare_uint32(record->id, condition->value.number,
                                     condition->compare);
            break;
        case FIELD_NAME:
            matched = compare_string(record->name, condition->value.string,
                                     condition->compare);
            break;
        case FIELD_GROUP_ID:
            matched = compare_uint32(record->group_id,
                                     condition->value.number,
                                     condition->compare);
            break;
        case FIELD_FACULTY_ID:
            return 0;
        }

        if (!matched) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* SELECT                                                                    */
/* ------------------------------------------------------------------------- */

static int compare_faculty_rows(const Faculty *left, const Faculty *right,
                                QueryField field)
{
    if (field == FIELD_ID) {
        return (left->id > right->id) - (left->id < right->id);
    }
    return strcmp(left->name, right->name);
}

static int compare_group_rows(const Group *left, const Group *right,
                              QueryField field)
{
    if (field == FIELD_ID) {
        return (left->id > right->id) - (left->id < right->id);
    }
    if (field == FIELD_FACULTY_ID) {
        return (left->faculty_id > right->faculty_id) -
               (left->faculty_id < right->faculty_id);
    }
    return strcmp(left->name, right->name);
}

static int compare_student_rows(const Student *left, const Student *right,
                                QueryField field)
{
    if (field == FIELD_ID) {
        return (left->id > right->id) - (left->id < right->id);
    }
    if (field == FIELD_GROUP_ID) {
        return (left->group_id > right->group_id) -
               (left->group_id < right->group_id);
    }
    return strcmp(left->name, right->name);
}

static int should_swap(int comparison, int descending)
{
    return descending ? comparison < 0 : comparison > 0;
}

static void sort_faculty_rows(const Faculty **rows, size_t count,
                              QueryField field, int descending)
{
    for (size_t i = 1; i < count; ++i) {
        const Faculty *key = rows[i];
        size_t j = i;
        while (j > 0 &&
               should_swap(compare_faculty_rows(rows[j - 1], key, field),
                           descending)) {
            rows[j] = rows[j - 1];
            --j;
        }
        rows[j] = key;
    }
}

static void sort_group_rows(const Group **rows, size_t count,
                            QueryField field, int descending)
{
    for (size_t i = 1; i < count; ++i) {
        const Group *key = rows[i];
        size_t j = i;
        while (j > 0 &&
               should_swap(compare_group_rows(rows[j - 1], key, field),
                           descending)) {
            rows[j] = rows[j - 1];
            --j;
        }
        rows[j] = key;
    }
}

static void sort_student_rows(const Student **rows, size_t count,
                              QueryField field, int descending)
{
    for (size_t i = 1; i < count; ++i) {
        const Student *key = rows[i];
        size_t j = i;
        while (j > 0 &&
               should_swap(compare_student_rows(rows[j - 1], key, field),
                           descending)) {
            rows[j] = rows[j - 1];
            --j;
        }
        rows[j] = key;
    }
}

static int execute_select(const Database *db, const Query *query, FILE *out,
                          char *err, size_t errlen)
{
    switch (query->table) {
    case TABLE_FACULTY: {
        const Faculty **rows = NULL;
        if (db->faculty_count != 0) {
            rows = malloc(db->faculty_count * sizeof *rows);
            if (rows == NULL) {
                db_error(err, errlen, "out of memory for SELECT result");
                return QUERY_EXEC_DATA_ERROR;
            }
        }

        size_t count = 0;
        for (size_t i = 0; i < db->faculty_count; ++i) {
            if (match_faculty(&db->faculties[i], query)) {
                rows[count++] = &db->faculties[i];
            }
        }
        if (query->has_order) {
            sort_faculty_rows(rows, count, query->order_field,
                              query->order_desc);
        }

        fprintf(out, "id\tname\n");
        for (size_t i = 0; i < count; ++i) {
            fprintf(out, "%" PRIu32 "\t%s\n", rows[i]->id, rows[i]->name);
        }
        free(rows);
        break;
    }

    case TABLE_GROUP: {
        const Group **rows = NULL;
        if (db->group_count != 0) {
            rows = malloc(db->group_count * sizeof *rows);
            if (rows == NULL) {
                db_error(err, errlen, "out of memory for SELECT result");
                return QUERY_EXEC_DATA_ERROR;
            }
        }

        size_t count = 0;
        for (size_t i = 0; i < db->group_count; ++i) {
            if (match_group(&db->groups[i], query)) {
                rows[count++] = &db->groups[i];
            }
        }
        if (query->has_order) {
            sort_group_rows(rows, count, query->order_field,
                            query->order_desc);
        }

        fprintf(out, "id\tname\tfaculty_id\n");
        for (size_t i = 0; i < count; ++i) {
            fprintf(out, "%" PRIu32 "\t%s\t%" PRIu32 "\n",
                    rows[i]->id, rows[i]->name, rows[i]->faculty_id);
        }
        free(rows);
        break;
    }

    case TABLE_STUDENT: {
        const Student **rows = NULL;
        if (db->student_count != 0) {
            rows = malloc(db->student_count * sizeof *rows);
            if (rows == NULL) {
                db_error(err, errlen, "out of memory for SELECT result");
                return QUERY_EXEC_DATA_ERROR;
            }
        }

        size_t count = 0;
        for (size_t i = 0; i < db->student_count; ++i) {
            if (match_student(&db->students[i], query)) {
                rows[count++] = &db->students[i];
            }
        }
        if (query->has_order) {
            sort_student_rows(rows, count, query->order_field,
                              query->order_desc);
        }

        fprintf(out, "id\tname\tgroup_id\n");
        for (size_t i = 0; i < count; ++i) {
            fprintf(out, "%" PRIu32 "\t%s\t%" PRIu32 "\n",
                    rows[i]->id, rows[i]->name, rows[i]->group_id);
        }
        free(rows);
        break;
    }
    }

    if (ferror(out)) {
        db_error(err, errlen, "cannot write SELECT result");
        return QUERY_EXEC_DATA_ERROR;
    }
    return QUERY_EXEC_OK;
}

/* ------------------------------------------------------------------------- */
/* Сбор идентификаторов, подходящих под WHERE                                */
/* ------------------------------------------------------------------------- */

static int collect_faculty_ids(const Database *db, const Query *query,
                               uint32_t **ids, size_t *count,
                               char *err, size_t errlen)
{
    *ids = NULL;
    *count = 0;
    if (db->faculty_count == 0) {
        return 0;
    }

    uint32_t *result = malloc(db->faculty_count * sizeof *result);
    if (result == NULL) {
        db_error(err, errlen, "out of memory while selecting rows");
        return -1;
    }

    size_t used = 0;
    for (size_t i = 0; i < db->faculty_count; ++i) {
        if (match_faculty(&db->faculties[i], query)) {
            result[used++] = db->faculties[i].id;
        }
    }

    *ids = result;
    *count = used;
    return 0;
}

static int collect_group_ids(const Database *db, const Query *query,
                             uint32_t **ids, size_t *count,
                             char *err, size_t errlen)
{
    *ids = NULL;
    *count = 0;
    if (db->group_count == 0) {
        return 0;
    }

    uint32_t *result = malloc(db->group_count * sizeof *result);
    if (result == NULL) {
        db_error(err, errlen, "out of memory while selecting rows");
        return -1;
    }

    size_t used = 0;
    for (size_t i = 0; i < db->group_count; ++i) {
        if (match_group(&db->groups[i], query)) {
            result[used++] = db->groups[i].id;
        }
    }

    *ids = result;
    *count = used;
    return 0;
}

static int collect_student_ids(const Database *db, const Query *query,
                               uint32_t **ids, size_t *count,
                               char *err, size_t errlen)
{
    *ids = NULL;
    *count = 0;
    if (db->student_count == 0) {
        return 0;
    }

    uint32_t *result = malloc(db->student_count * sizeof *result);
    if (result == NULL) {
        db_error(err, errlen, "out of memory while selecting rows");
        return -1;
    }

    size_t used = 0;
    for (size_t i = 0; i < db->student_count; ++i) {
        if (match_student(&db->students[i], query)) {
            result[used++] = db->students[i].id;
        }
    }

    *ids = result;
    *count = used;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* INSERT                                                                    */
/* ------------------------------------------------------------------------- */

static int execute_insert(Database *db, const Query *query, FILE *out,
                          char *err, size_t errlen)
{
    uint32_t assigned = 0;
    const QueryValue *values = query->insert_values;

    switch (query->table) {
    case TABLE_FACULTY:
        if (db_faculty_insert(db, values[0].number, values[1].string,
                              &assigned, err, errlen) != 0) {
            return QUERY_EXEC_DATA_ERROR;
        }
        break;
    case TABLE_GROUP:
        if (db_group_insert(db, values[0].number, values[1].string,
                            values[2].number, &assigned,
                            err, errlen) != 0) {
            return QUERY_EXEC_DATA_ERROR;
        }
        break;
    case TABLE_STUDENT:
        if (db_student_insert(db, values[0].number, values[1].string,
                              values[2].number, &assigned,
                              err, errlen) != 0) {
            return QUERY_EXEC_DATA_ERROR;
        }
        break;
    }

    fprintf(out, "inserted %s id=%" PRIu32 "\n",
            query_table_name(query->table), assigned);
    return QUERY_EXEC_OK;
}

/* ------------------------------------------------------------------------- */
/* UPDATE                                                                    */
/* ------------------------------------------------------------------------- */

static int execute_update(Database *db, const Query *query, FILE *out,
                          int *changed, char *err, size_t errlen)
{
    uint32_t *ids = NULL;
    size_t count = 0;

    int collected = 0;
    switch (query->table) {
    case TABLE_FACULTY:
        collected = collect_faculty_ids(db, query, &ids, &count, err, errlen);
        break;
    case TABLE_GROUP:
        collected = collect_group_ids(db, query, &ids, &count, err, errlen);
        break;
    case TABLE_STUDENT:
        collected = collect_student_ids(db, query, &ids, &count, err, errlen);
        break;
    }
    if (collected != 0) {
        return QUERY_EXEC_DATA_ERROR;
    }

    for (size_t i = 0; i < count; ++i) {
        int has_name = 0;
        int has_reference = 0;
        const char *name = NULL;
        uint32_t reference = 0;

        for (size_t j = 0; j < query->assignment_count; ++j) {
            const QueryAssignment *assignment = &query->assignments[j];
            if (assignment->field == FIELD_NAME) {
                has_name = 1;
                name = assignment->value.string;
            } else if (assignment->field == FIELD_GROUP_ID ||
                       assignment->field == FIELD_FACULTY_ID) {
                has_reference = 1;
                reference = assignment->value.number;
            }
        }

        int result = 0;
        switch (query->table) {
        case TABLE_FACULTY:
            result = db_faculty_update(db, ids[i], has_name, name,
                                       err, errlen);
            break;
        case TABLE_GROUP:
            result = db_group_update(db, ids[i], has_name, name,
                                     has_reference, reference,
                                     err, errlen);
            break;
        case TABLE_STUDENT:
            result = db_student_update(db, ids[i], has_name, name,
                                       has_reference, reference,
                                       err, errlen);
            break;
        }

        if (result != 0) {
            free(ids);
            return QUERY_EXEC_DATA_ERROR;
        }
    }

    if (count != 0) {
        *changed = 1;
    }
    fprintf(out, "updated %zu %s record(s)\n", count,
            query_table_name(query->table));
    free(ids);
    return QUERY_EXEC_OK;
}

/* ------------------------------------------------------------------------- */
/* DELETE                                                                    */
/* ------------------------------------------------------------------------- */

static int execute_delete(Database *db, const Query *query, FILE *out,
                          int *changed, char *err, size_t errlen)
{
    uint32_t *ids = NULL;
    size_t count = 0;

    int collected = 0;
    switch (query->table) {
    case TABLE_FACULTY:
        collected = collect_faculty_ids(db, query, &ids, &count, err, errlen);
        break;
    case TABLE_GROUP:
        collected = collect_group_ids(db, query, &ids, &count, err, errlen);
        break;
    case TABLE_STUDENT:
        collected = collect_student_ids(db, query, &ids, &count, err, errlen);
        break;
    }
    if (collected != 0) {
        return QUERY_EXEC_DATA_ERROR;
    }

    for (size_t i = 0; i < count; ++i) {
        int result = 0;
        switch (query->table) {
        case TABLE_FACULTY:
            result = db_faculty_delete(db, ids[i], err, errlen);
            break;
        case TABLE_GROUP:
            result = db_group_delete(db, ids[i], err, errlen);
            break;
        case TABLE_STUDENT:
            result = db_student_delete(db, ids[i], err, errlen);
            break;
        }

        if (result != 0) {
            free(ids);
            return QUERY_EXEC_DATA_ERROR;
        }
    }

    if (count != 0) {
        *changed = 1;
    }
    fprintf(out, "deleted %zu %s record(s)\n", count,
            query_table_name(query->table));
    free(ids);
    return QUERY_EXEC_OK;
}

/* ------------------------------------------------------------------------- */
/* Точка входа исполнителя                                                   */
/* ------------------------------------------------------------------------- */

int query_execute(Database *db, const Query *query, FILE *out,
                  int *changed, char *err, size_t errlen)
{
    *changed = 0;

    switch (query->command) {
    case QUERY_SELECT:
        return execute_select(db, query, out, err, errlen);
    case QUERY_INSERT: {
        int result = execute_insert(db, query, out, err, errlen);
        if (result == QUERY_EXEC_OK) {
            *changed = 1;
        }
        return result;
    }
    case QUERY_UPDATE:
        return execute_update(db, query, out, changed, err, errlen);
    case QUERY_DELETE:
        return execute_delete(db, query, out, changed, err, errlen);
    }

    db_error(err, errlen, "unknown query command");
    return QUERY_EXEC_DATA_ERROR;
}
