#include "storage.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Формат файла                                                              */
/* ------------------------------------------------------------------------- */

#define VPD_STORAGE_MAX_BYTES (64u * 1024u * 1024u)

static const char STORAGE_MAGIC[8] = {'V', 'P', 'D', 'D', 'B', '0', '5', '\0'};

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t header_size;
    uint32_t payload_size;
    uint32_t payload_checksum;
    uint32_t faculty_count;
    uint32_t group_count;
    uint32_t student_count;
    uint32_t next_faculty_id;
    uint32_t next_group_id;
    uint32_t next_student_id;
} StorageHeader;

typedef struct {
    uint32_t id;
    char name[VPD_FACULTY_NAME_CAP];
} FileFaculty;

typedef struct {
    uint32_t id;
    char name[VPD_GROUP_NAME_CAP];
    uint32_t faculty_id;
} FileGroup;

typedef struct {
    uint32_t id;
    char name[VPD_STUDENT_NAME_CAP];
    uint32_t group_id;
} FileStudent;

_Static_assert(sizeof(StorageHeader) == 48, "unexpected StorageHeader size");
_Static_assert(sizeof(FileFaculty) == 68, "unexpected FileFaculty size");
_Static_assert(sizeof(FileGroup) == 40, "unexpected FileGroup size");
_Static_assert(sizeof(FileStudent) == 72, "unexpected FileStudent size");

static uint32_t fnv1a(const unsigned char *data, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

/* ------------------------------------------------------------------------- */
/* Буфер полезной нагрузки                                                   */
/* ------------------------------------------------------------------------- */

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} Buffer;

static int buffer_reserve(Buffer *buffer, size_t extra,
                          char *err, size_t errlen)
{
    if (extra > SIZE_MAX - buffer->size) {
        db_error(err, errlen, "storage payload is too large");
        return -1;
    }

    size_t needed = buffer->size + extra;
    if (needed <= buffer->capacity) {
        return 0;
    }

    size_t new_capacity = (buffer->capacity == 0) ? 256 : buffer->capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            db_error(err, errlen, "storage payload is too large");
            return -1;
        }
        new_capacity *= 2;
    }

    unsigned char *temporary = realloc(buffer->data, new_capacity);
    if (temporary == NULL) {
        db_error(err, errlen, "out of memory while building storage payload");
        return -1;
    }

    buffer->data = temporary;
    buffer->capacity = new_capacity;
    return 0;
}

static int buffer_append(Buffer *buffer, const void *data, size_t size,
                         char *err, size_t errlen)
{
    if (buffer_reserve(buffer, size, err, errlen) != 0) {
        return -1;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Чтение файла                                                              */
/* ------------------------------------------------------------------------- */

static int read_entire_file(const char *path, unsigned char **data,
                            size_t *size, char *err, size_t errlen)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        db_error(err, errlen, "cannot open %s: %s", path, strerror(errno));
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        db_error(err, errlen, "cannot seek in %s: %s", path, strerror(errno));
        fclose(file);
        return -1;
    }

    long end = ftell(file);
    if (end < 0) {
        db_error(err, errlen, "cannot determine size of %s: %s",
                 path, strerror(errno));
        fclose(file);
        return -1;
    }
    if ((unsigned long long)end > VPD_STORAGE_MAX_BYTES) {
        db_error(err, errlen, "file %s is too large: %ld bytes", path, end);
        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        db_error(err, errlen, "cannot rewind %s: %s", path, strerror(errno));
        fclose(file);
        return -1;
    }

    size_t length = (size_t)end;
    unsigned char *buffer = malloc(length == 0 ? 1 : length);
    if (buffer == NULL) {
        db_error(err, errlen, "out of memory while reading %s", path);
        fclose(file);
        return -1;
    }

    if (length != 0 && fread(buffer, 1, length, file) != length) {
        db_error(err, errlen, "cannot read %s: %s", path, strerror(errno));
        free(buffer);
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) {
        db_error(err, errlen, "cannot close %s: %s", path, strerror(errno));
        free(buffer);
        return -1;
    }

    *data = buffer;
    *size = length;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Сохранение                                                                */
/* ------------------------------------------------------------------------- */

static int allocate_payload(const Database *db, Buffer *payload,
                            char *err, size_t errlen)
{
    uint64_t expected =
        (uint64_t)db->faculty_count * sizeof(FileFaculty) +
        (uint64_t)db->group_count * sizeof(FileGroup) +
        (uint64_t)db->student_count * sizeof(FileStudent);

    if (expected > VPD_STORAGE_MAX_BYTES) {
        db_error(err, errlen, "database is too large to store: %llu bytes",
                 (unsigned long long)expected);
        return -1;
    }

    if (buffer_reserve(payload, (size_t)expected, err, errlen) != 0) {
        return -1;
    }

    for (size_t i = 0; i < db->faculty_count; ++i) {
        FileFaculty record;
        memset(&record, 0, sizeof record);
        record.id = db->faculties[i].id;
        memcpy(record.name, db->faculties[i].name, sizeof record.name);
        if (buffer_append(payload, &record, sizeof record, err, errlen) != 0) {
            return -1;
        }
    }

    for (size_t i = 0; i < db->group_count; ++i) {
        FileGroup record;
        memset(&record, 0, sizeof record);
        record.id = db->groups[i].id;
        memcpy(record.name, db->groups[i].name, sizeof record.name);
        record.faculty_id = db->groups[i].faculty_id;
        if (buffer_append(payload, &record, sizeof record, err, errlen) != 0) {
            return -1;
        }
    }

    for (size_t i = 0; i < db->student_count; ++i) {
        FileStudent record;
        memset(&record, 0, sizeof record);
        record.id = db->students[i].id;
        memcpy(record.name, db->students[i].name, sizeof record.name);
        record.group_id = db->students[i].group_id;
        if (buffer_append(payload, &record, sizeof record, err, errlen) != 0) {
            return -1;
        }
    }

    return 0;
}

int storage_save(const Database *db, const char *path,
                 char *err, size_t errlen)
{
    if (db_validate(db, err, errlen) != 0) {
        return -1;
    }

    Buffer payload = {0};
    if (allocate_payload(db, &payload, err, errlen) != 0) {
        free(payload.data);
        return -1;
    }

    StorageHeader header;
    memset(&header, 0, sizeof header);
    memcpy(header.magic, STORAGE_MAGIC, sizeof header.magic);
    header.version = VPD_STORAGE_VERSION;
    header.header_size = (uint32_t)sizeof header;
    header.payload_size = (uint32_t)payload.size;
    header.payload_checksum = fnv1a(payload.data, payload.size);
    header.faculty_count = (uint32_t)db->faculty_count;
    header.group_count = (uint32_t)db->group_count;
    header.student_count = (uint32_t)db->student_count;
    header.next_faculty_id = db->next_faculty_id;
    header.next_group_id = db->next_group_id;
    header.next_student_id = db->next_student_id;

    size_t tmp_length = strlen(path) + sizeof ".tmp";
    char *tmp_path = malloc(tmp_length);
    if (tmp_path == NULL) {
        db_error(err, errlen, "out of memory while saving database");
        free(payload.data);
        return -1;
    }
    snprintf(tmp_path, tmp_length, "%s.tmp", path);

    FILE *file = fopen(tmp_path, "wb");
    if (file == NULL) {
        db_error(err, errlen, "cannot open %s for writing: %s",
                 tmp_path, strerror(errno));
        free(tmp_path);
        free(payload.data);
        return -1;
    }

    int failed = 0;
    if (fwrite(&header, sizeof header, 1, file) != 1) {
        db_error(err, errlen, "cannot write header to %s: %s",
                 tmp_path, strerror(errno));
        failed = 1;
    }
    if (!failed && payload.size != 0 &&
        fwrite(payload.data, 1, payload.size, file) != payload.size) {
        db_error(err, errlen, "cannot write payload to %s: %s",
                 tmp_path, strerror(errno));
        failed = 1;
    }
    if (!failed && fflush(file) != 0) {
        db_error(err, errlen, "cannot flush %s: %s",
                 tmp_path, strerror(errno));
        failed = 1;
    }
    if (!failed && fsync(fileno(file)) != 0) {
        db_error(err, errlen, "cannot sync %s: %s",
                 tmp_path, strerror(errno));
        failed = 1;
    }
    if (fclose(file) != 0 && !failed) {
        db_error(err, errlen, "cannot close %s: %s",
                 tmp_path, strerror(errno));
        failed = 1;
    }

    if (!failed && rename(tmp_path, path) != 0) {
        db_error(err, errlen, "cannot replace %s: %s", path, strerror(errno));
        failed = 1;
    }

    if (failed) {
        remove(tmp_path);
    }

    free(tmp_path);
    free(payload.data);
    return failed ? -1 : 0;
}

/* ------------------------------------------------------------------------- */
/* Загрузка                                                                  */
/* ------------------------------------------------------------------------- */

static int allocate_records(Database *db, uint32_t faculties,
                            uint32_t groups, uint32_t students,
                            char *err, size_t errlen)
{
    if (faculties != 0) {
        uint64_t bytes = (uint64_t)faculties * sizeof *db->faculties;
        if (bytes > (uint64_t)SIZE_MAX) {
            db_error(err, errlen, "faculty table is too large");
            return -1;
        }
        db->faculties = malloc((size_t)bytes);
        if (db->faculties == NULL) {
            db_error(err, errlen, "out of memory for faculty table");
            return -1;
        }
        db->faculty_capacity = faculties;
    }

    if (groups != 0) {
        uint64_t bytes = (uint64_t)groups * sizeof *db->groups;
        if (bytes > (uint64_t)SIZE_MAX) {
            db_error(err, errlen, "group table is too large");
            return -1;
        }
        db->groups = malloc((size_t)bytes);
        if (db->groups == NULL) {
            db_error(err, errlen, "out of memory for group table");
            return -1;
        }
        db->group_capacity = groups;
    }

    if (students != 0) {
        uint64_t bytes = (uint64_t)students * sizeof *db->students;
        if (bytes > (uint64_t)SIZE_MAX) {
            db_error(err, errlen, "student table is too large");
            return -1;
        }
        db->students = malloc((size_t)bytes);
        if (db->students == NULL) {
            db_error(err, errlen, "out of memory for student table");
            return -1;
        }
        db->student_capacity = students;
    }

    return 0;
}

int storage_load(Database *db, const char *path,
                 char *err, size_t errlen)
{
    unsigned char *data = NULL;
    size_t size = 0;
    if (read_entire_file(path, &data, &size, err, errlen) != 0) {
        return -1;
    }

    if (size < sizeof(StorageHeader)) {
        db_error(err, errlen, "corrupted database %s: file is shorter than header",
                 path);
        free(data);
        return -1;
    }

    StorageHeader header;
    memcpy(&header, data, sizeof header);

    if (memcmp(header.magic, STORAGE_MAGIC, sizeof header.magic) != 0) {
        db_error(err, errlen, "corrupted database %s: bad magic", path);
        free(data);
        return -1;
    }
    if (header.version != VPD_STORAGE_VERSION) {
        db_error(err, errlen, "unsupported database version %u in %s",
                 header.version, path);
        free(data);
        return -1;
    }
    if (header.header_size != sizeof header) {
        db_error(err, errlen, "corrupted database %s: bad header size %u",
                 path, header.header_size);
        free(data);
        return -1;
    }

    size_t payload_size = size - sizeof header;
    if (header.payload_size != payload_size) {
        db_error(err, errlen,
                 "corrupted database %s: payload size %u, file has %zu",
                 path, header.payload_size, payload_size);
        free(data);
        return -1;
    }

    uint64_t expected =
        (uint64_t)header.faculty_count * sizeof(FileFaculty) +
        (uint64_t)header.group_count * sizeof(FileGroup) +
        (uint64_t)header.student_count * sizeof(FileStudent);
    if (expected != payload_size) {
        db_error(err, errlen,
                 "corrupted database %s: record counts do not match payload",
                 path);
        free(data);
        return -1;
    }

    uint32_t actual_checksum =
        fnv1a(data + sizeof header, payload_size);
    if (actual_checksum != header.payload_checksum) {
        db_error(err, errlen,
                 "corrupted database %s: checksum mismatch "
                 "(stored 0x%08x, actual 0x%08x)",
                 path, header.payload_checksum, actual_checksum);
        free(data);
        return -1;
    }

    Database temporary;
    db_init(&temporary);

    if (allocate_records(&temporary, header.faculty_count,
                         header.group_count, header.student_count,
                         err, errlen) != 0) {
        db_free(&temporary);
        free(data);
        return -1;
    }

    size_t offset = sizeof header;
    for (uint32_t i = 0; i < header.faculty_count; ++i) {
        FileFaculty record;
        memcpy(&record, data + offset, sizeof record);
        offset += sizeof record;
        temporary.faculties[i].id = record.id;
        memcpy(temporary.faculties[i].name, record.name,
               sizeof temporary.faculties[i].name);
    }
    temporary.faculty_count = header.faculty_count;

    for (uint32_t i = 0; i < header.group_count; ++i) {
        FileGroup record;
        memcpy(&record, data + offset, sizeof record);
        offset += sizeof record;
        temporary.groups[i].id = record.id;
        memcpy(temporary.groups[i].name, record.name,
               sizeof temporary.groups[i].name);
        temporary.groups[i].faculty_id = record.faculty_id;
    }
    temporary.group_count = header.group_count;

    for (uint32_t i = 0; i < header.student_count; ++i) {
        FileStudent record;
        memcpy(&record, data + offset, sizeof record);
        offset += sizeof record;
        temporary.students[i].id = record.id;
        memcpy(temporary.students[i].name, record.name,
               sizeof temporary.students[i].name);
        temporary.students[i].group_id = record.group_id;
    }
    temporary.student_count = header.student_count;

    temporary.next_faculty_id = header.next_faculty_id;
    temporary.next_group_id = header.next_group_id;
    temporary.next_student_id = header.next_student_id;
    db_recompute_sequences(&temporary);

    if (db_rebuild_indexes(&temporary, err, errlen) != 0 ||
        db_validate(&temporary, err, errlen) != 0) {
        db_free(&temporary);
        free(data);
        return -1;
    }

    db_free(db);
    *db = temporary;
    free(data);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Журнал изменений                                                          */
/* ------------------------------------------------------------------------- */

int storage_journal(const char *database_path, const char *command,
                    const char *detail, char *err, size_t errlen)
{
    size_t path_length = strlen(database_path) + sizeof ".journal";
    char *journal_path = malloc(path_length);
    if (journal_path == NULL) {
        db_error(err, errlen, "out of memory while writing journal");
        return -1;
    }
    snprintf(journal_path, path_length, "%s.journal", database_path);

    FILE *file = fopen(journal_path, "a");
    if (file == NULL) {
        db_error(err, errlen, "cannot open journal %s: %s",
                 journal_path, strerror(errno));
        free(journal_path);
        return -1;
    }

    int failed = 0;
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) == 0) {
        if (fprintf(file, "# VPD Lab 05 journal\n") < 0) {
            failed = 1;
        }
    }

    time_t now = time(NULL);
    struct tm local;
    char timestamp[32] = "unknown-time";
    if (localtime_r(&now, &local) != NULL) {
        strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%S", &local);
    }

    char *clean_detail = malloc(strlen(detail) + 1);
    if (clean_detail == NULL) {
        db_error(err, errlen, "out of memory while writing journal");
        fclose(file);
        free(journal_path);
        return -1;
    }
    size_t i = 0;
    for (; detail[i] != '\0'; ++i) {
        char c = detail[i];
        clean_detail[i] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    clean_detail[i] = '\0';

    if (!failed &&
        fprintf(file, "[%s] %s: %s\n", timestamp, command, clean_detail) < 0) {
        failed = 1;
    }

    if (!failed && fflush(file) != 0) {
        failed = 1;
    }
    if (fclose(file) != 0) {
        failed = 1;
    }

    if (failed) {
        db_error(err, errlen, "cannot write journal %s: %s",
                 journal_path, strerror(errno));
    }

    free(clean_detail);
    free(journal_path);
    return failed ? -1 : 0;
}
