#include "p8/format.h"

#include <stdbool.h>

typedef struct {
    char *buffer;
    size_t size;
    size_t length;
} output_t;

static void put(output_t *out, char c) {
    if (out->size > 0 && out->length < out->size - 1) {
        out->buffer[out->length] = c;
    }
    out->length++;
}

static void put_padding(output_t *out, char c, int count) {
    for (; count > 0; count--) {
        put(out, c);
    }
}

typedef struct {
    bool left;
    bool zero;
    int width;
    int precision;
} spec_t;

static void put_field(output_t *out, const spec_t *spec, const char *sign, const char *text, int text_length) {
    int sign_length = sign[0] != '\0' ? 1 : 0;
    int padding = spec->width - sign_length - text_length;
    if (!spec->left && !spec->zero) put_padding(out, ' ', padding);
    if (sign_length) put(out, sign[0]);
    if (!spec->left && spec->zero) put_padding(out, '0', padding);
    for (int i = 0; i < text_length; i++) {
        put(out, text[i]);
    }
    if (spec->left) put_padding(out, ' ', padding);
}

static void put_number(output_t *out, const spec_t *spec, unsigned long value, unsigned base, bool upper,
                       const char *sign) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[3 * sizeof value];
    int count = 0;
    do {
        reversed[count++] = digits[value % base];
        value /= base;
    } while (value != 0);
    char text[3 * sizeof value];
    for (int i = 0; i < count; i++) {
        text[i] = reversed[count - 1 - i];
    }
    put_field(out, spec, sign, text, count);
}

static int parse_count(const char **format, va_list *args) {
    if (**format == '*') {
        (*format)++;
        return va_arg(*args, int);
    }
    int count = 0;
    for (; **format >= '0' && **format <= '9'; (*format)++) {
        count = count * 10 + (**format - '0');
    }
    return count;
}

static void put_conversion(output_t *out, const char **format, va_list *args) {
    const char *start = *format;
    spec_t spec = { false, false, 0, -1 };
    for (;; (*format)++) {
        if (**format == '-') spec.left = true;
        else if (**format == '0') spec.zero = true;
        else break;
    }
    spec.width = parse_count(format, args);
    if (spec.width < 0) {
        spec.left = true;
        spec.width = -spec.width;
    }
    if (**format == '.') {
        (*format)++;
        spec.precision = parse_count(format, args);
    }
    bool is_long = false;
    if (**format == 'l') {
        is_long = true;
        (*format)++;
    }
    char conversion = **format;
    if (conversion == '\0') {
        put(out, '%');
        *format = start;
        return;
    }
    (*format)++;
    switch (conversion) {
    case 'd':
    case 'i': {
        long value = is_long ? va_arg(*args, long) : va_arg(*args, int);
        unsigned long magnitude = value < 0 ? 0UL - (unsigned long)value : (unsigned long)value;
        put_number(out, &spec, magnitude, 10, false, value < 0 ? "-" : "");
        break;
    }
    case 'u':
    case 'x':
    case 'X': {
        unsigned long value = is_long ? va_arg(*args, unsigned long) : va_arg(*args, unsigned);
        put_number(out, &spec, value, conversion == 'u' ? 10 : 16, conversion == 'X', "");
        break;
    }
    case 'c': {
        char c = (char)va_arg(*args, int);
        spec.zero = false;
        put_field(out, &spec, "", &c, 1);
        break;
    }
    case 's': {
        const char *text = va_arg(*args, const char *);
        int length = 0;
        while (text[length] != '\0' && (spec.precision < 0 || length < spec.precision)) {
            length++;
        }
        spec.zero = false;
        put_field(out, &spec, "", text, length);
        break;
    }
    case '%':
        put(out, '%');
        break;
    default:
        put(out, '%');
        for (const char *c = start; c < *format; c++) {
            put(out, *c);
        }
        break;
    }
}

int p8_vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    output_t out = { buffer, size, 0 };
    va_list copy;
    va_copy(copy, args);
    while (*format != '\0') {
        if (*format != '%') {
            put(&out, *format++);
            continue;
        }
        format++;
        put_conversion(&out, &format, &copy);
    }
    va_end(copy);
    if (size > 0) {
        buffer[out.length < size ? out.length : size - 1] = '\0';
    }
    return (int)out.length;
}

int p8_snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = p8_vsnprintf(buffer, size, format, args);
    va_end(args);
    return length;
}
