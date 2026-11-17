#include "query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    Lexer lexer;
    Token token;
    char *err;
    size_t errlen;
    int has_error;
} Parser;

static int parser_error(Parser *parser, const char *message)
{
    if (!parser->has_error) {
        snprintf(parser->err, parser->errlen,
                 "parser error at position %zu: %s",
                 parser->token.position, message);
        parser->has_error = 1;
    }
    return -1;
}

static int parser_error_word(Parser *parser, const char *expected)
{
    char message[160];
    if (parser->token.type == TOKEN_EOF) {
        snprintf(message, sizeof message,
                 "expected %s, found end of query", expected);
    } else {
        snprintf(message, sizeof message,
                 "expected %s, found '%s'", expected, parser->token.text);
    }
    return parser_error(parser, message);
}

static int advance(Parser *parser)
{
    if (lexer_next(&parser->lexer, &parser->token,
                   parser->err, parser->errlen) != 0) {
        parser->has_error = 1;
        return -1;
    }
    return 0;
}

static int token_word_is(const Parser *parser, const char *word)
{
    return parser->token.type == TOKEN_WORD &&
           strcasecmp(parser->token.text, word) == 0;
}

static int expect_word(Parser *parser, const char *word)
{
    if (!token_word_is(parser, word)) {
        return parser_error_word(parser, word);
    }
    return advance(parser);
}

static int expect_type(Parser *parser, TokenType type, const char *name)
{
    if (parser->token.type != type) {
        return parser_error_word(parser, name);
    }
    return advance(parser);
}

static int parse_table(Parser *parser, QueryTable *table)
{
    if (parser->token.type != TOKEN_WORD) {
        return parser_error_word(parser, "table name");
    }

    if (strcasecmp(parser->token.text, "faculty") == 0 ||
        strcasecmp(parser->token.text, "faculties") == 0) {
        *table = TABLE_FACULTY;
    } else if (strcasecmp(parser->token.text, "group") == 0 ||
               strcasecmp(parser->token.text, "groups") == 0) {
        *table = TABLE_GROUP;
    } else if (strcasecmp(parser->token.text, "student") == 0 ||
               strcasecmp(parser->token.text, "students") == 0) {
        *table = TABLE_STUDENT;
    } else {
        char message[160];
        snprintf(message, sizeof message, "unknown table '%s'",
                 parser->token.text);
        return parser_error(parser, message);
    }

    return advance(parser);
}

static int field_allowed(QueryTable table, QueryField field)
{
    switch (table) {
    case TABLE_FACULTY:
        return field == FIELD_ID || field == FIELD_NAME;
    case TABLE_GROUP:
        return field == FIELD_ID || field == FIELD_NAME ||
               field == FIELD_FACULTY_ID;
    case TABLE_STUDENT:
        return field == FIELD_ID || field == FIELD_NAME ||
               field == FIELD_GROUP_ID;
    }
    return 0;
}

static int parse_field(Parser *parser, QueryTable table, QueryField *field)
{
    if (parser->token.type != TOKEN_WORD) {
        return parser_error_word(parser, "field name");
    }

    QueryField parsed;
    if (strcasecmp(parser->token.text, "id") == 0) {
        parsed = FIELD_ID;
    } else if (strcasecmp(parser->token.text, "name") == 0) {
        parsed = FIELD_NAME;
    } else if (strcasecmp(parser->token.text, "group_id") == 0) {
        parsed = FIELD_GROUP_ID;
    } else if (strcasecmp(parser->token.text, "faculty_id") == 0) {
        parsed = FIELD_FACULTY_ID;
    } else {
        char message[160];
        snprintf(message, sizeof message, "unknown field '%s'",
                 parser->token.text);
        return parser_error(parser, message);
    }

    if (!field_allowed(table, parsed)) {
        char message[200];
        snprintf(message, sizeof message,
                 "field '%s' does not belong to table '%s'",
                 query_field_name(parsed), query_table_name(table));
        return parser_error(parser, message);
    }

    *field = parsed;
    return advance(parser);
}

static int parse_value(Parser *parser, QueryValue *value)
{
    memset(value, 0, sizeof *value);

    if (parser->token.type == TOKEN_NUMBER) {
        value->type = VALUE_NUMBER;
        value->number = parser->token.number;
        return advance(parser);
    }

    if (parser->token.type == TOKEN_STRING ||
        parser->token.type == TOKEN_WORD) {
        value->type = VALUE_STRING;
        if (strlen(parser->token.text) >= sizeof value->string) {
            return parser_error(parser, "string value is too long");
        }
        memcpy(value->string, parser->token.text,
               strlen(parser->token.text) + 1);
        return advance(parser);
    }

    return parser_error_word(parser, "value");
}

static int parse_compare(Parser *parser, QueryCompare *compare)
{
    switch (parser->token.type) {
    case TOKEN_EQ:
        *compare = COMPARE_EQ;
        break;
    case TOKEN_NE:
        *compare = COMPARE_NE;
        break;
    case TOKEN_LT:
        *compare = COMPARE_LT;
        break;
    case TOKEN_LE:
        *compare = COMPARE_LE;
        break;
    case TOKEN_GT:
        *compare = COMPARE_GT;
        break;
    case TOKEN_GE:
        *compare = COMPARE_GE;
        break;
    default:
        return parser_error_word(parser, "comparison operator");
    }
    return advance(parser);
}

static int validate_condition(Parser *parser, const QueryCondition *condition)
{
    if (condition->field == FIELD_NAME) {
        if (condition->value.type != VALUE_STRING) {
            return parser_error(parser, "name must be compared with a string");
        }
        if (condition->compare != COMPARE_EQ &&
            condition->compare != COMPARE_NE) {
            return parser_error(parser,
                                "only = and != are supported for name");
        }
    } else if (condition->value.type != VALUE_NUMBER) {
        return parser_error(parser,
                            "numeric field must be compared with a number");
    }
    return 0;
}

static int parse_condition(Parser *parser, QueryTable table,
                           QueryCondition *condition)
{
    memset(condition, 0, sizeof *condition);

    if (parse_field(parser, table, &condition->field) != 0 ||
        parse_compare(parser, &condition->compare) != 0 ||
        parse_value(parser, &condition->value) != 0) {
        return -1;
    }

    return validate_condition(parser, condition);
}

static int parse_conditions(Parser *parser, QueryTable table, Query *query)
{
    for (;;) {
        if (query->condition_count >= VPD_QUERY_MAX_CONDITIONS) {
            return parser_error(parser, "too many WHERE conditions");
        }

        QueryCondition *condition =
            &query->conditions[query->condition_count];
        if (parse_condition(parser, table, condition) != 0) {
            return -1;
        }
        ++query->condition_count;

        if (!token_word_is(parser, "AND")) {
            return 0;
        }
        if (advance(parser) != 0) {
            return -1;
        }
    }
}

static int parse_assignment(Parser *parser, QueryTable table,
                            QueryAssignment *assignment)
{
    memset(assignment, 0, sizeof *assignment);

    if (parse_field(parser, table, &assignment->field) != 0) {
        return -1;
    }

    if (assignment->field == FIELD_ID) {
        return parser_error(parser, "primary key id cannot be updated");
    }

    if (expect_type(parser, TOKEN_EQ, "=") != 0 ||
        parse_value(parser, &assignment->value) != 0) {
        return -1;
    }

    if (assignment->field == FIELD_NAME) {
        if (assignment->value.type != VALUE_STRING) {
            return parser_error(parser, "name must be assigned a string");
        }
    } else if (assignment->value.type != VALUE_NUMBER) {
        return parser_error(parser,
                            "numeric field must be assigned a number");
    }

    return 0;
}

static int parse_end(Parser *parser)
{
    if (parser->token.type == TOKEN_SEMICOLON) {
        if (advance(parser) != 0) {
            return -1;
        }
    }
    if (parser->token.type != TOKEN_EOF) {
        return parser_error_word(parser, "end of query");
    }
    return 0;
}

static int parse_select_tail(Parser *parser, QueryTable table, Query *query)
{
    if (token_word_is(parser, "WHERE")) {
        if (advance(parser) != 0 ||
            parse_conditions(parser, table, query) != 0) {
            return -1;
        }
    }

    if (token_word_is(parser, "ORDER")) {
        if (advance(parser) != 0 ||
            expect_word(parser, "BY") != 0 ||
            parse_field(parser, table, &query->order_field) != 0) {
            return -1;
        }
        query->has_order = 1;
        if (token_word_is(parser, "ASC")) {
            query->order_desc = 0;
            if (advance(parser) != 0) {
                return -1;
            }
        } else if (token_word_is(parser, "DESC")) {
            query->order_desc = 1;
            if (advance(parser) != 0) {
                return -1;
            }
        }
    }

    return parse_end(parser);
}

static int parse_insert_statement(Parser *parser, Query *query)
{
    if (token_word_is(parser, "INTO")) {
        if (advance(parser) != 0) {
            return -1;
        }
    }

    if (parse_table(parser, &query->table) != 0) {
        return -1;
    }

    if (token_word_is(parser, "VALUES")) {
        if (advance(parser) != 0) {
            return -1;
        }
    }

    size_t expected = 0;
    switch (query->table) {
    case TABLE_FACULTY:
        expected = 2;
        break;
    case TABLE_GROUP:
    case TABLE_STUDENT:
        expected = 3;
        break;
    }

    for (size_t i = 0; i < expected; ++i) {
        if (parse_value(parser, &query->insert_values[i]) != 0) {
            return -1;
        }
        ++query->insert_value_count;
    }

    if (query->insert_values[0].type != VALUE_NUMBER) {
        return parser_error(parser, "id must be a number");
    }
    if (query->insert_values[1].type != VALUE_STRING) {
        return parser_error(parser, "name must be a string");
    }
    if (expected == 3 && query->insert_values[2].type != VALUE_NUMBER) {
        return parser_error(parser, "foreign key must be a number");
    }

    return parse_end(parser);
}

static int parse_update_statement(Parser *parser, Query *query)
{
    if (parse_table(parser, &query->table) != 0 ||
        expect_word(parser, "SET") != 0) {
        return -1;
    }

    for (;;) {
        if (query->assignment_count >= VPD_QUERY_MAX_ASSIGNMENTS) {
            return parser_error(parser, "too many SET assignments");
        }

        QueryAssignment *assignment =
            &query->assignments[query->assignment_count];
        if (parse_assignment(parser, query->table, assignment) != 0) {
            return -1;
        }
        ++query->assignment_count;

        if (parser->token.type != TOKEN_COMMA) {
            break;
        }
        if (advance(parser) != 0) {
            return -1;
        }
    }

    if (!token_word_is(parser, "WHERE")) {
        return parser_error(parser, "UPDATE requires a WHERE clause");
    }
    if (advance(parser) != 0 ||
        parse_conditions(parser, query->table, query) != 0) {
        return -1;
    }

    return parse_end(parser);
}

static int parse_delete_statement(Parser *parser, Query *query)
{
    if (parse_table(parser, &query->table) != 0) {
        return -1;
    }

    if (!token_word_is(parser, "WHERE")) {
        return parser_error(parser, "DELETE requires a WHERE clause");
    }
    if (advance(parser) != 0 ||
        parse_conditions(parser, query->table, query) != 0) {
        return -1;
    }

    return parse_end(parser);
}

Query *query_parse(const char *text, char *err, size_t errlen)
{
    Parser parser;
    memset(&parser, 0, sizeof parser);
    parser.err = err;
    parser.errlen = errlen;
    if (errlen != 0) {
        err[0] = '\0';
    }

    lexer_init(&parser.lexer, text);
    if (advance(&parser) != 0) {
        return NULL;
    }

    if (parser.token.type != TOKEN_WORD) {
        parser_error(&parser, "query must start with a command");
        return NULL;
    }

    Query *query = calloc(1, sizeof *query);
    if (query == NULL) {
        snprintf(err, errlen, "out of memory while parsing query");
        return NULL;
    }

    int result = -1;

    if (strcasecmp(parser.token.text, "SELECT") == 0) {
        query->command = QUERY_SELECT;
        if (advance(&parser) != 0 ||
            parse_table(&parser, &query->table) != 0 ||
            parse_select_tail(&parser, query->table, query) != 0) {
            goto done;
        }
        result = 0;
    } else if (strcasecmp(parser.token.text, "INSERT") == 0) {
        query->command = QUERY_INSERT;
        if (advance(&parser) != 0 ||
            parse_insert_statement(&parser, query) != 0) {
            goto done;
        }
        result = 0;
    } else if (strcasecmp(parser.token.text, "UPDATE") == 0) {
        query->command = QUERY_UPDATE;
        if (advance(&parser) != 0 ||
            parse_update_statement(&parser, query) != 0) {
            goto done;
        }
        result = 0;
    } else if (strcasecmp(parser.token.text, "DELETE") == 0) {
        query->command = QUERY_DELETE;
        if (advance(&parser) != 0 ||
            parse_delete_statement(&parser, query) != 0) {
            goto done;
        }
        result = 0;
    } else {
        char message[160];
        snprintf(message, sizeof message, "unknown command '%s'",
                 parser.token.text);
        parser_error(&parser, message);
    }

done:
    if (result != 0) {
        free(query);
        return NULL;
    }
    return query;
}

void query_free(Query *query)
{
    free(query);
}

const char *query_command_name(QueryCommand command)
{
    switch (command) {
    case QUERY_SELECT:
        return "SELECT";
    case QUERY_INSERT:
        return "INSERT";
    case QUERY_UPDATE:
        return "UPDATE";
    case QUERY_DELETE:
        return "DELETE";
    }
    return "UNKNOWN";
}

const char *query_table_name(QueryTable table)
{
    switch (table) {
    case TABLE_FACULTY:
        return "faculty";
    case TABLE_GROUP:
        return "group";
    case TABLE_STUDENT:
        return "student";
    }
    return "unknown";
}

const char *query_field_name(QueryField field)
{
    switch (field) {
    case FIELD_ID:
        return "id";
    case FIELD_NAME:
        return "name";
    case FIELD_GROUP_ID:
        return "group_id";
    case FIELD_FACULTY_ID:
        return "faculty_id";
    }
    return "unknown";
}
