#include "stringbuilder.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#define UINT64_MAX_STRLEN 20
#define LONG_FMT "%" PRId64
#define ULONG_FMT "%" PRIu64

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define ENSURE_MEMORY_SIZE(sb, requiredSize) do {                                 \
    if (((requiredSize) + 1) > (sb)->size) {                                      \
        int32_t newSize = (sb)->size * 2;                                         \
        if (((requiredSize) + 1) > newSize) {                                     \
            newSize = ((requiredSize) + 1);                                       \
        }                                                                         \
        if (stringbuilder_set_size((sb), newSize) != STRING_BUILDER_ERROR_NONE) { \
            return (sb)->error;                                                   \
        }                                                                         \
    }                                                                             \
} while (0)

static void *(*mem_alloc)(size_t) = malloc;
static void *(*mem_realloc)(void *, size_t) = realloc;
static void (*mem_free)(void *) = free;

void stringbuilder_set_memory_alloc(void *(*malloc_fn)(size_t))
{
    mem_alloc = malloc_fn;
}

void stringbuilder_set_memory_realloc(void *(*realloc_fn)(void *, size_t))
{
    mem_realloc = realloc_fn;
}

void stringbuilder_set_memory_free(void (*free_fn)(void *))
{
    mem_free = free_fn;
}

StringBuilderError stringbuilder_init(StringBuilder *sb)
{
    return stringbuilder_init_size(sb, STRING_BUILDER_DEFAULT_INITIAL_SIZE);
}

StringBuilderError stringbuilder_init_size(StringBuilder *sb, int32_t size)
{
    sb->error = STRING_BUILDER_ERROR_MEM_ALLOC_FAILURE;
    if (size > 0) {
        sb->value = mem_alloc(sizeof(char) * size);
        if (sb->value != NULL) {
            sb->length = 0;
            sb->size = size;
            sb->error = STRING_BUILDER_ERROR_NONE;
        }
    }

    return sb->error;
}

void stringbuilder_finalize(StringBuilder *sb)
{
    if (sb != NULL) {
        if (sb->value != NULL) {
            mem_free(sb->value);
        }

        sb->value = NULL;
        sb->length = 0;
        sb->size = 0;
        sb->error = STRING_BUILDER_ERROR_NONE;
    }
}

StringBuilderError stringbuilder_copy(StringBuilder *src, StringBuilder *dest)
{
    src->error = stringbuilder_init_size(dest, src->length + 1);
    if (src->error == STRING_BUILDER_ERROR_NONE) {
        memcpy(dest->value, src->value, src->length);
        dest->length = src->length;
        dest->value[dest->length] = '\0';
    }

    return src->error;
}

const char *stringbuilder_get_str(const StringBuilder *sb)
{
    return sb->value;
}

StringBuilderError stringbuilder_get_last_error(const StringBuilder *sb)
{
    return sb->error;
}

const char *stringbuilder_get_error_msg(StringBuilderError code)
{
#define CASE_RETURN_ENUM_AS_STRING(val) case val: return #val
    switch (code) {
        CASE_RETURN_ENUM_AS_STRING(STRING_BUILDER_ERROR_NONE);
        CASE_RETURN_ENUM_AS_STRING(STRING_BUILDER_ERROR_MEM_ALLOC_FAILURE);
        CASE_RETURN_ENUM_AS_STRING(STRING_BUILDER_ERROR_OUT_OF_RANGE);
        default:
            return "Unknown error code";
    }
}

int32_t stringbuilder_get_length(const StringBuilder *sb)
{
    return sb->length;
}

StringBuilderError stringbuilder_set_length(StringBuilder *sb, int32_t length)
{
    ENSURE_MEMORY_SIZE(sb, length);
    if (length > sb->length) {
        char *dst = sb->value + sb->length;
        memset(dst, 0, length - sb->length);
    }

    sb->length = length;
    sb->value[length] = '\0';
    return sb->error;
}

int32_t stringbuilder_get_size(const StringBuilder *sb)
{
    return sb->size;
}

StringBuilderError stringbuilder_set_size(StringBuilder *sb, int32_t newSize)
{
    sb->error = STRING_BUILDER_ERROR_MEM_ALLOC_FAILURE;
    char *tmp = mem_realloc(sb->value, sizeof(char) * newSize);
    if (tmp != NULL) {
        sb->value = tmp;
        sb->size = newSize;
        sb->error = STRING_BUILDER_ERROR_NONE;
        if (sb->length >= sb->size) {
            sb->length = sb->size - 1;
            sb->value[sb->length] = '\0';
        }
    }

    return sb->error;
}

int stringbuilder_compare(const StringBuilder *a, const StringBuilder *b)
{
    if (a == b) {
        return 0;
    }

    int result = memcmp(a->value, b->value, min(a->length, b->length));
    if (!result) {
        result = (int) (a->length - b->length);
    }

    return result;
}

bool stringbuilder_equals(const StringBuilder *a, const StringBuilder *b)
{
    return a == b || a->length == b->length && memcmp(a->value, b->value, a->length) == 0;
}

static char *internal_string_pos(char *str, int32_t str_len, const char *needle, int32_t needle_len)
{
    // All strings contain an empty string ""
    if (needle_len == 0) {
        return str;
    }

    if (str_len > 0) {
        // Lookup for a single character in the string
        if (needle_len == 1) {
            return memchr(str, *needle, str_len);
        }

        // Lookup for a word in the string
        if (needle_len > 1 && needle_len <= str_len) {
            char *curr = str;
            const char *end = str + str_len;

            do {
                curr = memchr(curr, *needle, end - curr);
                if (curr == NULL) {
                    break;
                }


                if (memcmp(curr + 1, needle + 1, needle_len - 1) == 0) {
                    return curr;
                }

                curr += needle_len;
            } while (curr + needle_len < end);
        }
    }

    return NULL;
}

int32_t stringbuilder_index_of(const StringBuilder *sb, const char *needle, int32_t needle_len)
{
    const char *pos = internal_string_pos(sb->value, sb->length, needle, needle_len);
    return pos == NULL ? -1 : (int32_t) (pos - sb->value);
}

bool stringbuilder_contains(const StringBuilder *sb, const char *needle, int32_t needle_len)
{
    return internal_string_pos(sb->value, sb->length, needle, needle_len) != NULL;
}

bool stringbuilder_starts_with(const StringBuilder *sb, const char *prefix, int32_t prefix_len)
{
    if (sb->length < prefix_len) {
        return false;
    }

    return memcmp(sb->value, prefix, prefix_len) == 0;
}

bool stringbuilder_ends_with(const StringBuilder *sb, const char *suffix, int32_t suffix_len)
{
    if (sb->length < suffix_len) {
        return false;
    }

    const char *ptr = sb->value + sb->length - suffix_len;
    return memcmp(ptr, suffix, suffix_len) == 0;
}

StringBuilderError stringbuilder_concat(StringBuilder *sb, const StringBuilder *other)
{
    return stringbuilder_append_string(sb, other->value, other->length);
}

StringBuilderError stringbuilder_append_char(StringBuilder *sb, char c)
{
    ENSURE_MEMORY_SIZE(sb, sb->length + 1);
    sb->value[sb->length++] = c;
    sb->value[sb->length] = '\0';
    return sb->error;
}

StringBuilderError stringbuilder_append_string(StringBuilder *sb, const char *string, int32_t len)
{
    int32_t newLen = sb->length + len;
    ENSURE_MEMORY_SIZE(sb, newLen);
    char *dst = sb->value + sb->length;
    memcpy(dst, string, len);
    sb->length = newLen;
    sb->value[newLen] = '\0';
    return sb->error;
}

StringBuilderError stringbuilder_append_format(StringBuilder *sb, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int chars = vsnprintf(NULL, 0, fmt, args);
    ENSURE_MEMORY_SIZE(sb, sb->length + chars);

    char *ptr = sb->value + sb->length;
    vsnprintf(ptr, sb->size - sb->length, fmt, args);
    va_end(args);
    sb->length += chars;
    sb->value[sb->length] = '\0';
    return sb->error;
}

static StringBuilderError internal_append_int(StringBuilder *sb, const char *fmt, uint64_t value)
{
    if (value <= (uint64_t) 9) {
        char c = (char) (value + '0');
        return stringbuilder_append_char(sb, c);
    }

    ENSURE_MEMORY_SIZE(sb, sb->length + UINT64_MAX_STRLEN);
    char *ptr = sb->value + sb->length;
    int chars = snprintf(ptr, sb->size - sb->length, fmt, value);
    sb->length += chars;
    sb->value[sb->length] = '\0';
    return sb->error;
}

StringBuilderError stringbuilder_append_int(StringBuilder *sb, int64_t value)
{
    return internal_append_int(sb, LONG_FMT, (uint64_t) value);
}

StringBuilderError stringbuilder_append_uint(StringBuilder *sb, uint64_t value)
{
    return internal_append_int(sb, ULONG_FMT, value);
}

StringBuilderError stringbuilder_append_float(StringBuilder *sb, double value, int32_t decimals)
{
    int chars = snprintf(NULL, 0, "%.*f", decimals, value);
    ENSURE_MEMORY_SIZE(sb, sb->length + chars);

    char *ptr = sb->value + sb->length;
    snprintf(ptr, sb->size - sb->length, "%.*f", decimals, value);
    sb->length += chars;
    sb->value[sb->length] = '\0';
    return sb->error;
}

static void internal_case_convert(StringBuilder *sb, int (*convert)(int))
{
    if (sb->length > 0) {
        char *c = sb->value;
        const char *e = c + sb->length;

        while (c < e) {
            *c = (char) convert(*c);
            c++;
        }
    }
}

void stringbuilder_to_uppercase(StringBuilder *sb)
{
    internal_case_convert(sb, toupper);
}

void stringbuilder_to_lowercase(StringBuilder *sb)
{
    internal_case_convert(sb, tolower);
}

int stringbuilder_replace_char(StringBuilder *sb, char search, char replace)
{
    int n = 0;
    if (sb->length > 0) {
        char *c = sb->value;
        const char *e = c + sb->length;
        while ((c = memchr(c, search, e - c)) != NULL) {
            *c = replace;
            c++;
            n++;
        }
    }

    return n;
}

StringBuilderError stringbuilder_repeat(StringBuilder *sb, int times)
{
    if (times < 0) {
        return sb->error = STRING_BUILDER_ERROR_OUT_OF_RANGE;
    }

    sb->error = STRING_BUILDER_ERROR_NONE;
    if (sb->length > 0) {
        if (times == 0) {
            sb->length = 0;
            sb->value[0] = '\0';
        } else {
            int32_t newLen = sb->length + (sb->length * (times - 1));
            ENSURE_MEMORY_SIZE(sb, newLen);

            char *dst = sb->value + sb->length;
            while (--times > 0) {
                memmove(dst, sb->value, sb->length);
                dst += sb->length;
            }

            sb->length = newLen;
            sb->value[newLen] = '\0';
        }
    }

    return sb->error;
}

void stringbuilder_trim(StringBuilder *sb)
{
    if (sb->length > 0) {
        const char *c = sb->value;
        const char *e = c + sb->length;

        // Trim the beginning of the string
        while (c < e && isspace(*c)) {
            c++;
        }

        if (c == e) {
            // The whole string is made of whitespace.
            // Set the length to 0 and return earlier
            sb->length = 0;
            sb->value[0] = '\0';
            return;
        }

        // Trim the end of the string
        e--; // Position into the last character (not the null terminator)
        while (e >= c && isspace(*e)) {
            e--;
        }

        int32_t newLen = ((int32_t) (e - c)) + 1;
        if (c > sb->value) {
            memmove(sb->value, c, newLen);
        }

        sb->length = newLen;
        sb->value[newLen] = '\0';
    }
}

int stringbuilder_split(const StringBuilder *sb, StringBuilder *pieces, int32_t max_pieces, const char *separator, int32_t separator_len)
{
    int n = 0;
    if (sb->length > 0 && max_pieces > 0) {
        char *start = sb->value;
        char *end = start + sb->length;
        char *current = internal_string_pos(start, sb->length, separator, separator_len);

        while (current != NULL && n + 1 < max_pieces) {
            StringBuilder *p = &pieces[n];
            int32_t len = (int32_t) (current - start);
            stringbuilder_init_size(p, len + 1);
            stringbuilder_append_string(p, start, len);

            int32_t remaining = (int32_t) (end - current);
            start = current + separator_len;
            current = internal_string_pos(start, remaining, separator, separator_len);
            n++;
        }

        // Split the remaining part of the string
        if (start <= end && n < max_pieces) {
            StringBuilder *p = &pieces[n++];
            int32_t len = (int32_t) (end - start);
            stringbuilder_init_size(p, len + 1);
            stringbuilder_append_string(p, start, len);
        }
    }

    return n;
}
