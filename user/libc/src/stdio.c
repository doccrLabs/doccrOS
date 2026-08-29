/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: stdio.c
 *
 */

#include "syscall.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static FILE stdin_object = {0, 0, 0};
static FILE stdout_object = {1, 0, 0};
static FILE stderr_object = {2, 0, 0};

FILE *stdin = &stdin_object;
FILE *stdout = &stdout_object;
FILE *stderr = &stderr_object;

FILE *fopen(const char *path, const char *mode) {
    int flags = (mode && mode[0] == 'r') ? 0 : 1;
    int fd = (int)open(path, flags);
    FILE *stream;

    if (fd < 0) {
        return NULL;
    }

    stream = malloc(sizeof(*stream));
    if (!stream) {
        close(fd);
        return NULL;
    }

    stream->fd = fd;
    stream->error = 0;
    stream->eof = 0;
    return stream;
}

int fclose(FILE *stream) {
    int result;

    if (!stream) {
        return EOF;
    }

    result = (int)close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return result;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream) {
    long bytes_read;

    if (!size || !count) {
        return 0;
    }

    bytes_read = read(stream->fd, buffer, size * count);
    if (bytes_read < 0) {
        stream->error = 1;
        return 0;
    }
    if ((size_t)bytes_read < size * count) {
        stream->eof = 1;
    }
    return (size_t)bytes_read / size;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream) {
    long bytes_written;

    if (!size || !count) {
        return 0;
    }

    bytes_written = write(stream->fd, buffer, size * count);
    if (bytes_written < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)bytes_written / size;
}

int fseek(FILE *stream, long offset, int origin) {
    long result = lseek(stream->fd, offset, origin);

    if (result < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream) {
    return lseek(stream->fd, 0, SEEK_CUR);
}

int feof(FILE *stream) {
    return stream->eof;
}

int ferror(FILE *stream) {
    return stream->error;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int putchar(int character) {
    char byte = character;

    return write(1, &byte, 1) == 1 ? (unsigned char)character : EOF;
}

typedef struct output_buffer {
    char *data;
    size_t capacity;
    size_t position;
} output_buffer_t;

static void emit(output_buffer_t *output, char character) {
    if (output->capacity && output->position + 1 < output->capacity) {
        output->data[output->position] = character;
    }
    output->position++;
}

static void emit_number(output_buffer_t *output, unsigned long long value,
                        unsigned base, int uppercase, int negative,
                        int width, int precision, char padding) {
    char reversed[32];
    int digits = 0;
    int zeroes;
    int total;

    if (!value) {
        reversed[digits++] = '0';
    }
    while (value) {
        unsigned digit = value % base;

        reversed[digits++] = digit < 10
                                 ? '0' + digit
                                 : (uppercase ? 'A' : 'a') + digit - 10;
        value /= base;
    }

    zeroes = precision > digits ? precision - digits : 0;
    total = digits + zeroes + negative;

    if (padding == '0' && precision < 0) {
        if (negative) {
            emit(output, '-');
            negative = 0;
        }
        while (total < width) {
            emit(output, '0');
            total++;
        }
    } else {
        while (total < width) {
            emit(output, ' ');
            total++;
        }
    }

    if (negative) {
        emit(output, '-');
    }
    while (zeroes-- > 0) {
        emit(output, '0');
    }
    while (digits > 0) {
        emit(output, reversed[--digits]);
    }
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list arguments) {
    output_buffer_t output = {buffer, size, 0};

    while (*format) {
        char padding = ' ';
        int width = 0;
        int precision = -1;
        int long_modifier = 0;

        if (*format != '%') {
            emit(&output, *format++);
            continue;
        }
        format++;

        if (*format == '0') {
            padding = '0';
            format++;
        }
        while (isdigit(*format)) {
            width = width * 10 + *format++ - '0';
        }
        if (*format == '.') {
            format++;
            precision = 0;
            while (isdigit(*format)) {
                precision = precision * 10 + *format++ - '0';
            }
        }
        while (*format == 'l') {
            long_modifier++;
            format++;
        }

        switch (*format++) {
        case 's': {
            const char *text = va_arg(arguments, const char *);
            int emitted = 0;

            if (!text) {
                text = "(null)";
            }
            while (*text && (precision < 0 || emitted++ < precision)) {
                emit(&output, *text++);
            }
            break;
        }
        case 'c':
            emit(&output, va_arg(arguments, int));
            break;
        case 'd':
        case 'i': {
            long long value = long_modifier ? va_arg(arguments, long)
                                            : va_arg(arguments, int);
            unsigned long long magnitude = value < 0
                                                ? -(unsigned long long)value
                                                : (unsigned long long)value;

            emit_number(&output, magnitude, 10, 0, value < 0,
                        width, precision, padding);
            break;
        }
        case 'u': {
            unsigned long value = long_modifier
                                      ? va_arg(arguments, unsigned long)
                                      : va_arg(arguments, unsigned);

            emit_number(&output, value, 10, 0, 0, width, precision, padding);
            break;
        }
        case 'x':
        case 'X': {
            int uppercase = format[-1] == 'X';
            unsigned long value = long_modifier
                                      ? va_arg(arguments, unsigned long)
                                      : va_arg(arguments, unsigned);

            emit_number(&output, value, 16, uppercase, 0,
                        width, precision, padding);
            break;
        }
        case 'p':
            emit(&output, '0');
            emit(&output, 'x');
            emit_number(&output, (unsigned long)va_arg(arguments, void *),
                        16, 0, 0, 0, -1, ' ');
            break;
        case '%':
            emit(&output, '%');
            break;
        default:
            emit(&output, format[-1]);
            break;
        }
    }

    if (size) {
        buffer[output.position < size ? output.position : size - 1] = '\0';
    }
    return (int)output.position;
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}

int sprintf(char *buffer, const char *format, ...) {
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = vsnprintf(buffer, (size_t)-1, format, arguments);
    va_end(arguments);
    return result;
}

int vfprintf(FILE *stream, const char *format, va_list arguments) {
    char buffer[1024];
    int result = vsnprintf(buffer, sizeof(buffer), format, arguments);
    size_t bytes;

    if (result < 0) {
        return result;
    }

    bytes = (size_t)result < sizeof(buffer)
                ? (size_t)result
                : sizeof(buffer) - 1;
    return write(stream->fd, buffer, bytes) < 0 ? -1 : result;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int sscanf(const char *input, const char *format, ...) {
    va_list arguments;
    int assignments = 0;

    va_start(arguments, format);
    while (*format && *input) {
        if (isspace(*format)) {
            while (isspace(*format)) {
                format++;
            }
            while (isspace(*input)) {
                input++;
            }
            continue;
        }

        if (*format != '%') {
            if (*input++ != *format++) {
                break;
            }
            continue;
        }
        format++;

        if (*format == 'd' || *format == 'i') {
            int *destination = va_arg(arguments, int *);
            int value = 0;
            int sign = 1;

            if (*input == '-') {
                sign = -1;
                input++;
            }
            if (!isdigit(*input)) {
                break;
            }
            while (isdigit(*input)) {
                value = value * 10 + *input++ - '0';
            }
            *destination = value * sign;
            assignments++;
            format++;
        } else if (*format == 's') {
            char *destination = va_arg(arguments, char *);

            while (*input && !isspace(*input)) {
                *destination++ = *input++;
            }
            *destination = '\0';
            assignments++;
            format++;
        } else {
            break;
        }
    }
    va_end(arguments);
    return assignments;
}

int remove(const char *path) {
    return (int)unlink(path);
}

int rename(const char *old_path, const char *new_path)
{
    return syscall3(
        SYS_RENAME,
        (long)old_path,
        (long)new_path,
        0
    );
}
