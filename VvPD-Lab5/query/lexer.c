#include "query.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lexer_init(Lexer *lexer, const char *input)
{
    lexer->input = input;
    lexer->position = 0;
}

static int lexer_error(char *err, size_t errlen, size_t position,
                       const char *message)
{
    snprintf(err, errlen, "lexer error at position %zu: %s", position, message);
    return -1;
}

static void skip_whitespace(Lexer *lexer)
{
    while (lexer->input[lexer->position] != '\0' &&
           isspace((unsigned char)lexer->input[lexer->position])) {
        ++lexer->position;
    }
}

static int copy_token_text(Token *token, const char *start, size_t length,
                           char *err, size_t errlen, size_t position)
{
    if (length >= sizeof token->text) {
        return lexer_error(err, errlen, position, "token is too long");
    }
    memcpy(token->text, start, length);
    token->text[length] = '\0';
    return 0;
}

static int read_word(Lexer *lexer, Token *token, char *err, size_t errlen)
{
    size_t start = lexer->position;
    while (isalnum((unsigned char)lexer->input[lexer->position]) ||
           lexer->input[lexer->position] == '_') {
        ++lexer->position;
    }

    token->type = TOKEN_WORD;
    token->position = start;
    return copy_token_text(token, lexer->input + start,
                           lexer->position - start, err, errlen, start);
}

static int read_number(Lexer *lexer, Token *token, char *err, size_t errlen)
{
    size_t start = lexer->position;
    while (isdigit((unsigned char)lexer->input[lexer->position])) {
        ++lexer->position;
    }

    size_t length = lexer->position - start;
    if (length >= sizeof token->text) {
        return lexer_error(err, errlen, start, "number is too long");
    }

    char buffer[VPD_QUERY_TEXT_CAP];
    memcpy(buffer, lexer->input + start, length);
    buffer[length] = '\0';

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(buffer, &end, 10);
    if (errno == ERANGE || end == buffer || *end != '\0' ||
        value > UINT32_MAX) {
        return lexer_error(err, errlen, start,
                           "number does not fit into uint32_t");
    }

    token->type = TOKEN_NUMBER;
    token->position = start;
    token->number = (uint32_t)value;
    memcpy(token->text, buffer, length + 1);
    return 0;
}

static int append_utf8(char *buffer, size_t *length, size_t capacity,
                       unsigned codepoint, char *err, size_t errlen,
                       size_t position)
{
    unsigned char encoded[4];
    size_t count = 0;

    if (codepoint == 0) {
        return lexer_error(err, errlen, position,
                           "\\u0000 is not supported");
    }
    if (codepoint <= 0x7f) {
        encoded[count++] = (unsigned char)codepoint;
    } else if (codepoint <= 0x7ff) {
        encoded[count++] = (unsigned char)(0xc0 | (codepoint >> 6));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        encoded[count++] = (unsigned char)(0xe0 | (codepoint >> 12));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    } else {
        encoded[count++] = (unsigned char)(0xf0 | (codepoint >> 18));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[count++] = (unsigned char)(0x80 | (codepoint & 0x3f));
    }

    if (*length + count >= capacity) {
        return lexer_error(err, errlen, position, "string literal is too long");
    }
    memcpy(buffer + *length, encoded, count);
    *length += count;
    return 0;
}

static int parse_hex4(const char *text, unsigned *value)
{
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
        unsigned char c = (unsigned char)text[i];
        unsigned digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10u;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10u;
        } else {
            return -1;
        }
        result = (result << 4) | digit;
    }
    *value = result;
    return 0;
}

static int read_string(Lexer *lexer, Token *token, char *err, size_t errlen)
{
    size_t start = lexer->position;
    ++lexer->position; /* opening quote */

    char buffer[VPD_QUERY_TEXT_CAP];
    size_t length = 0;

    while (lexer->input[lexer->position] != '\0') {
        char c = lexer->input[lexer->position++];

        if (c == '"') {
            buffer[length] = '\0';
            token->type = TOKEN_STRING;
            token->position = start;
            memcpy(token->text, buffer, length + 1);
            return 0;
        }

        if (c == '\\') {
            char escaped = lexer->input[lexer->position];
            if (escaped == '\0') {
                break;
            }
            ++lexer->position;

            if (escaped == '"' || escaped == '\\' || escaped == '/') {
                c = escaped;
            } else if (escaped == 'n') {
                c = '\n';
            } else if (escaped == 'r') {
                c = '\r';
            } else if (escaped == 't') {
                c = '\t';
            } else if (escaped == 'u') {
                if (strlen(lexer->input + lexer->position) < 4) {
                    return lexer_error(err, errlen, lexer->position,
                                       "incomplete \\u escape");
                }
                unsigned codepoint = 0;
                if (parse_hex4(lexer->input + lexer->position, &codepoint) != 0) {
                    return lexer_error(err, errlen, lexer->position,
                                       "invalid \\u escape");
                }
                lexer->position += 4;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (lexer->input[lexer->position] != '\\' ||
                        lexer->input[lexer->position + 1] != 'u') {
                        return lexer_error(err, errlen, lexer->position,
                                           "high surrogate without low surrogate");
                    }
                    lexer->position += 2;
                    if (strlen(lexer->input + lexer->position) < 4) {
                        return lexer_error(err, errlen, lexer->position,
                                           "incomplete low surrogate");
                    }
                    unsigned low = 0;
                    if (parse_hex4(lexer->input + lexer->position, &low) != 0 ||
                        low < 0xdc00 || low > 0xdfff) {
                        return lexer_error(err, errlen, lexer->position,
                                           "invalid low surrogate");
                    }
                    lexer->position += 4;
                    codepoint = 0x10000u +
                                ((codepoint - 0xd800u) << 10) +
                                (low - 0xdc00u);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return lexer_error(err, errlen, lexer->position,
                                       "unexpected low surrogate");
                }

                if (append_utf8(buffer, &length, sizeof buffer, codepoint,
                                err, errlen, start) != 0) {
                    return -1;
                }
                continue;
            } else {
                return lexer_error(err, errlen, lexer->position - 1,
                                   "unknown escape sequence");
            }
        }

        if (length + 1 >= sizeof buffer) {
            return lexer_error(err, errlen, start, "string literal is too long");
        }
        buffer[length++] = c;
    }

    return lexer_error(err, errlen, start, "unterminated string literal");
}

int lexer_next(Lexer *lexer, Token *token, char *err, size_t errlen)
{
    skip_whitespace(lexer);

    size_t position = lexer->position;
    char c = lexer->input[lexer->position];

    memset(token, 0, sizeof *token);
    token->position = position;

    if (c == '\0') {
        token->type = TOKEN_EOF;
        return 0;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        return read_word(lexer, token, err, errlen);
    }
    if (isdigit((unsigned char)c)) {
        return read_number(lexer, token, err, errlen);
    }
    if (c == '"') {
        return read_string(lexer, token, err, errlen);
    }

    ++lexer->position;
    token->text[0] = c;
    token->text[1] = '\0';

    switch (c) {
    case '=':
        if (lexer->input[lexer->position] == '=') {
            ++lexer->position;
            token->text[1] = '=';
            token->text[2] = '\0';
        }
        token->type = TOKEN_EQ;
        return 0;
    case '!':
        if (lexer->input[lexer->position] != '=') {
            return lexer_error(err, errlen, position,
                               "expected '=' after '!'");
        }
        ++lexer->position;
        token->text[1] = '=';
        token->text[2] = '\0';
        token->type = TOKEN_NE;
        return 0;
    case '<':
        if (lexer->input[lexer->position] == '=') {
            ++lexer->position;
            token->text[1] = '=';
            token->text[2] = '\0';
            token->type = TOKEN_LE;
        } else {
            token->type = TOKEN_LT;
        }
        return 0;
    case '>':
        if (lexer->input[lexer->position] == '=') {
            ++lexer->position;
            token->text[1] = '=';
            token->text[2] = '\0';
            token->type = TOKEN_GE;
        } else {
            token->type = TOKEN_GT;
        }
        return 0;
    case ',':
        token->type = TOKEN_COMMA;
        return 0;
    case '*':
        token->type = TOKEN_STAR;
        return 0;
    case ';':
        token->type = TOKEN_SEMICOLON;
        return 0;
    default:
        break;
    }

    char message[64];
    if (isprint((unsigned char)c)) {
        snprintf(message, sizeof message, "unexpected character '%c'", c);
    } else {
        snprintf(message, sizeof message, "unexpected byte 0x%02x",
                 (unsigned char)c);
    }
    return lexer_error(err, errlen, position, message);
}
