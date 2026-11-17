#ifndef VPD_QUERY_H
#define VPD_QUERY_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "model.h"

#define VPD_QUERY_TEXT_CAP      128
#define VPD_QUERY_MAX_CONDITIONS 4
#define VPD_QUERY_MAX_ASSIGNMENTS 4
#define VPD_QUERY_MAX_VALUES     3

/* ------------------------------------------------------------------------- */
/* Лексический анализатор                                                    */
/* ------------------------------------------------------------------------- */

typedef enum {
    TOKEN_EOF,
    TOKEN_WORD,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_COMMA,
    TOKEN_STAR,
    TOKEN_SEMICOLON
} TokenType;

typedef struct {
    TokenType type;
    char text[VPD_QUERY_TEXT_CAP];
    uint32_t number;
    size_t position;
} Token;

typedef struct {
    const char *input;
    size_t position;
} Lexer;

void lexer_init(Lexer *lexer, const char *input);
int lexer_next(Lexer *lexer, Token *token, char *err, size_t errlen);

/* ------------------------------------------------------------------------- */
/* Внутреннее представление запроса                                          */
/* ------------------------------------------------------------------------- */

typedef enum {
    QUERY_SELECT,
    QUERY_INSERT,
    QUERY_UPDATE,
    QUERY_DELETE
} QueryCommand;

typedef enum {
    TABLE_FACULTY,
    TABLE_GROUP,
    TABLE_STUDENT
} QueryTable;

typedef enum {
    FIELD_ID,
    FIELD_NAME,
    FIELD_GROUP_ID,
    FIELD_FACULTY_ID
} QueryField;

typedef enum {
    COMPARE_EQ,
    COMPARE_NE,
    COMPARE_LT,
    COMPARE_LE,
    COMPARE_GT,
    COMPARE_GE
} QueryCompare;

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING
} QueryValueType;

typedef struct {
    QueryValueType type;
    uint32_t number;
    char string[VPD_QUERY_TEXT_CAP];
} QueryValue;

typedef struct {
    QueryField field;
    QueryCompare compare;
    QueryValue value;
} QueryCondition;

typedef struct {
    QueryField field;
    QueryValue value;
} QueryAssignment;

typedef struct {
    QueryCommand command;
    QueryTable table;

    /* INSERT: значения в порядке колонок таблицы. */
    QueryValue insert_values[VPD_QUERY_MAX_VALUES];
    size_t insert_value_count;

    /* UPDATE: список присваиваний SET. */
    QueryAssignment assignments[VPD_QUERY_MAX_ASSIGNMENTS];
    size_t assignment_count;

    /* SELECT / UPDATE / DELETE: условия WHERE, соединённые AND. */
    QueryCondition conditions[VPD_QUERY_MAX_CONDITIONS];
    size_t condition_count;

    /* SELECT: необязательный ORDER BY. */
    int has_order;
    QueryField order_field;
    int order_desc;
} Query;

Query *query_parse(const char *text, char *err, size_t errlen);
void query_free(Query *query);

const char *query_command_name(QueryCommand command);
const char *query_table_name(QueryTable table);
const char *query_field_name(QueryField field);

/* ------------------------------------------------------------------------- */
/* Исполнитель                                                               */
/* ------------------------------------------------------------------------- */

#define QUERY_EXEC_OK         0
#define QUERY_EXEC_DATA_ERROR 2

int query_execute(Database *db, const Query *query, FILE *out,
                  int *changed, char *err, size_t errlen);

#endif /* VPD_QUERY_H */
