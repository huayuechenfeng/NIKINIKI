/*
 * NIKINIKI mongoose-compatible JSON reader -- implementation.
 *
 * Clean-room implementation written from the documented external behavior
 * contract.  It does not copy, translate or derive from mongoose source.
 *
 * Design: a small, read-only, allocation-free JSON scanner.  Paths are
 * resolved by walking the document once; no DOM is built and no memory is
 * allocated (the only allocation in this layer is mg_json_get_str, which is
 * not part of Phase 1).
 */

#include "mg_json.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void skip_ws(const char *s, size_t len, size_t *pos)
{
    while (*pos < len && is_ws(s[*pos]))
        (*pos)++;
}

/*
 * Scan a JSON string whose opening quote is at s[start].
 * On success sets *end to one past the closing quote and returns 1.
 */
static int scan_string(const char *s, size_t len, size_t start, size_t *end)
{
    size_t i = start + 1;
    if (start >= len || s[start] != '"')
        return 0;
    while (i < len) {
        if (s[i] == '\\') {
            i += 2;
            continue;
        }
        if (s[i] == '"') {
            *end = i + 1;
            return 1;
        }
        i++;
    }
    return 0;
}

/*
 * Validate a JSON number and set *end to one past the last digit.
 * Grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
 */
static int scan_number(const char *s, size_t len, size_t start, size_t *end)
{
    size_t i = start;
    if (i >= len)
        return 0;
    if (s[i] == '-')
        i++;
    if (i >= len)
        return 0;
    if (s[i] == '0') {
        i++;
    } else if (s[i] >= '1' && s[i] <= '9') {
        i++;
        while (i < len && s[i] >= '0' && s[i] <= '9')
            i++;
    } else {
        return 0;
    }
    if (i < len && s[i] == '.') {
        i++;
        if (i >= len || s[i] < '0' || s[i] > '9')
            return 0;
        while (i < len && s[i] >= '0' && s[i] <= '9')
            i++;
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < len && (s[i] == '+' || s[i] == '-'))
            i++;
        if (i >= len || s[i] < '0' || s[i] > '9')
            return 0;
        while (i < len && s[i] >= '0' && s[i] <= '9')
            i++;
    }
    *end = i;
    return 1;
}

/* A scalar is terminated by whitespace, a comma or a closing bracket. */
static int at_scalar_end(const char *s, size_t len, size_t pos)
{
    return pos >= len || is_ws(s[pos]) || s[pos] == ',' ||
           s[pos] == ']' || s[pos] == '}';
}

/*
 * Scan the JSON value whose first non-whitespace byte is at s[start].
 * On success sets *end to one past the last byte of the value.
 */
static int scan_value(const char *s, size_t len, size_t start, size_t *end)
{
    char c;
    size_t i;
    if (start >= len)
        return 0;
    c = s[start];
    if (c == '"')
        return scan_string(s, len, start, end);
    if (c == '{' || c == '[') {
        size_t depth = 1;
        i = start + 1;
        while (i < len && depth > 0) {
            char d = s[i];
            if (d == '"') {
                size_t strEnd = 0;
                if (!scan_string(s, len, i, &strEnd))
                    return 0;
                i = strEnd;
                continue;
            }
            if (d == '{' || d == '[')
                depth++;
            else if (d == '}' || d == ']')
                depth--;
            i++;
        }
        if (depth != 0)
            return 0;
        *end = i;
        return 1;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        if (!scan_number(s, len, start, &i))
            return 0;
        if (!at_scalar_end(s, len, i))
            return 0;
        *end = i;
        return 1;
    }
    if (c == 't') {
        if (len - start < 4 || memcmp(s + start, "true", 4) != 0)
            return 0;
        i = start + 4;
        if (!at_scalar_end(s, len, i))
            return 0;
        *end = i;
        return 1;
    }
    if (c == 'f') {
        if (len - start < 5 || memcmp(s + start, "false", 5) != 0)
            return 0;
        i = start + 5;
        if (!at_scalar_end(s, len, i))
            return 0;
        *end = i;
        return 1;
    }
    if (c == 'n') {
        if (len - start < 4 || memcmp(s + start, "null", 4) != 0)
            return 0;
        i = start + 4;
        if (!at_scalar_end(s, len, i))
            return 0;
        *end = i;
        return 1;
    }
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/*
 * Decode a \uXXXX escape at s[*pos] (the backslash) into a UTF-8 byte
 * sequence, combining a following low surrogate when present.  *pos is
 * advanced past the escape.  Returns 1 on success.
 */
static int decode_unicode_escape(const char *s,
                                 size_t len,
                                 size_t *pos,
                                 unsigned char out[4],
                                 size_t *out_len)
{
    size_t i = *pos;
    unsigned int cp;
    int d0, d1, d2, d3;
    if (len - i < 6 || s[i] != '\\' || s[i + 1] != 'u')
        return 0;
    d0 = hex_value(s[i + 2]);
    d1 = hex_value(s[i + 3]);
    d2 = hex_value(s[i + 4]);
    d3 = hex_value(s[i + 5]);
    if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0)
        return 0;
    cp = (unsigned int)((d0 << 12) | (d1 << 8) | (d2 << 4) | d3);
    i += 6;
    if (cp >= 0xD800 && cp <= 0xDBFF && len - i >= 6 &&
        s[i] == '\\' && s[i + 1] == 'u') {
        int e0, e1, e2, e3;
        e0 = hex_value(s[i + 2]);
        e1 = hex_value(s[i + 3]);
        e2 = hex_value(s[i + 4]);
        e3 = hex_value(s[i + 5]);
        if (e0 >= 0 && e1 >= 0 && e2 >= 0 && e3 >= 0) {
            unsigned int lo = (unsigned int)((e0 << 12) | (e1 << 8) |
                                             (e2 << 4) | e3);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i += 6;
            }
        }
    }
    /* Lone surrogates are encoded as-is (WTF-8 style); never out of bounds. */
    if (cp < 0x80) {
        out[0] = (unsigned char)cp;
        *out_len = 1;
    } else if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        *out_len = 2;
    } else if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        *out_len = 3;
    } else {
        out[0] = (unsigned char)(0xF0 | (cp >> 18));
        out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (unsigned char)(0x80 | (cp & 0x3F));
        *out_len = 4;
    }
    *pos = i;
    return 1;
}

/*
 * Compare the raw content of a JSON key (between quotes) with the target
 * path segment, decoding JSON escapes in the key on the fly.
 */
static int key_bytes_match(const char *s,
                           size_t start,
                           size_t end,
                           const char *key,
                           size_t klen)
{
    size_t i = start;
    size_t j = 0;
    while (i < end) {
        unsigned char c = (unsigned char)s[i];
        if (c != '\\') {
            if (j >= klen || (unsigned char)key[j] != c)
                return 0;
            j++;
            i++;
            continue;
        }
        /* escape sequence */
        i++;
        if (i >= end)
            return 0;
        switch (s[i]) {
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case '/':
            c = '/';
            break;
        case '"':
            c = '"';
            break;
        case '\\':
            c = '\\';
            break;
        case 'u': {
            size_t escapePos = i - 1; /* backslash */
            unsigned char seq[4];
            size_t seqLen = 0;
            if (!decode_unicode_escape(s, end, &escapePos, seq, &seqLen))
                return 0;
            i = escapePos;
            if (j + seqLen > klen || memcmp(key + j, seq, seqLen) != 0)
                return 0;
            j += seqLen;
            continue;
        }
        default:
            return 0;
        }
        i++; /* move past the escape letter */
        if (j >= klen || (unsigned char)key[j] != c)
            return 0;
        j++;
    }
    return j == klen;
}

/*
 * Find the value of object member `key` inside the object starting at
 * s[pos] ('{').  On success sets *out to the value start and returns 1.
 */
static int object_member(const char *s,
                         size_t len,
                         size_t pos,
                         const char *key,
                         size_t klen,
                         size_t *out)
{
    size_t i;
    if (pos >= len || s[pos] != '{')
        return 0;
    i = pos + 1;
    for (;;) {
        size_t keyStart, keyEnd;
        skip_ws(s, len, &i);
        if (i >= len)
            return 0;
        if (s[i] == '}')
            return 0;
        if (s[i] != '"')
            return 0;
        keyStart = i;
        if (!scan_string(s, len, i, &keyEnd))
            return 0;
        if (key_bytes_match(s, keyStart + 1, keyEnd - 1, key, klen)) {
            i = keyEnd;
            skip_ws(s, len, &i);
            if (i >= len || s[i] != ':')
                return 0;
            i++;
            skip_ws(s, len, &i);
            if (i >= len)
                return 0;
            *out = i;
            return 1;
        }
        i = keyEnd;
        skip_ws(s, len, &i);
        if (i >= len || s[i] != ':')
            return 0;
        i++;
        skip_ws(s, len, &i);
        if (!scan_value(s, len, i, &i))
            return 0;
        skip_ws(s, len, &i);
        if (i >= len)
            return 0;
        if (s[i] == ',') {
            i++;
            continue;
        }
        return 0;
    }
}

/*
 * Find the element at array index `index` inside the array starting at
 * s[pos] ('[').  On success sets *out to the value start and returns 1.
 */
static int array_element(const char *s,
                         size_t len,
                         size_t pos,
                         size_t index,
                         size_t *out)
{
    size_t i;
    size_t current = 0;
    if (pos >= len || s[pos] != '[')
        return 0;
    i = pos + 1;
    for (;;) {
        size_t valueStart;
        skip_ws(s, len, &i);
        if (i >= len)
            return 0;
        if (s[i] == ']')
            return 0;
        valueStart = i;
        if (!scan_value(s, len, i, &i))
            return 0;
        if (current == index) {
            *out = valueStart;
            return 1;
        }
        current++;
        skip_ws(s, len, &i);
        if (i >= len)
            return 0;
        if (s[i] == ',') {
            i++;
            continue;
        }
        if (s[i] == ']')
            return 0;
        return 0;
    }
}

struct mg_str mg_str_n(const char *s, size_t n)
{
    struct mg_str str;
    str.buf = (char *)s;
    str.len = n;
    return str;
}

struct mg_str mg_json_get_tok(struct mg_str json, const char *path)
{
    struct mg_str none = mg_str_n(NULL, 0);
    const char *s;
    size_t len;
    size_t pos = 0;
    size_t p = 1; /* path index, after '$' */
    if (!json.buf || !path || path[0] != '$')
        return none;
    s = json.buf;
    len = json.len;
    skip_ws(s, len, &pos);
    if (pos >= len)
        return none;
    for (;;) {
        if (path[p] == '\0') {
            size_t end = 0;
            if (!scan_value(s, len, pos, &end))
                return none;
            return mg_str_n(s + pos, end - pos);
        }
        if (path[p] == '.') {
            const char *key;
            size_t klen = 0;
            p++;
            key = path + p;
            while (path[p + klen] != '\0' &&
                   path[p + klen] != '.' &&
                   path[p + klen] != '[')
                klen++;
            if (!object_member(s, len, pos, key, klen, &pos))
                return none;
            p += klen;
        } else if (path[p] == '[') {
            size_t index = 0;
            size_t digits = 0;
            p++;
            while (path[p] >= '0' && path[p] <= '9') {
                index = index * 10 + (size_t)(path[p] - '0');
                p++;
                digits++;
            }
            if (digits == 0 || path[p] != ']')
                return none;
            p++;
            if (!array_element(s, len, pos, index, &pos))
                return none;
        } else {
            return none;
        }
    }
}

size_t mg_json_next(struct mg_str obj,
                    size_t ofs,
                    struct mg_str *key,
                    struct mg_str *val)
{
    const char *s;
    size_t len;
    size_t pos;
    int is_array;
    if (!obj.buf || obj.len < 2 ||
        (obj.buf[0] != '{' && obj.buf[0] != '['))
        return 0;
    if (ofs >= obj.len)
        return 0;
    s = obj.buf;
    len = obj.len;
    is_array = s[0] == '[';
    pos = ofs == 0 ? 1 : ofs;
    skip_ws(s, len, &pos);
    if (pos >= len || s[pos] == ']' || s[pos] == '}')
        return 0;
    if (!is_array) {
        size_t keyStart = pos;
        size_t keyEnd = 0;
        if (s[pos] != '"')
            return 0;
        if (!scan_string(s, len, pos, &keyEnd))
            return 0;
        if (key)
            *key = mg_str_n(s + keyStart, keyEnd - keyStart);
        pos = keyEnd;
        skip_ws(s, len, &pos);
        if (pos >= len || s[pos] != ':')
            return 0;
        pos++;
        skip_ws(s, len, &pos);
    } else if (key) {
        *key = mg_str_n(NULL, 0);
    }
    if (pos >= len)
        return 0;
    {
        size_t valueStart = pos;
        size_t valueEnd = 0;
        if (!scan_value(s, len, pos, &valueEnd))
            return 0;
        if (val)
            *val = mg_str_n(s + valueStart, valueEnd - valueStart);
        pos = valueEnd;
    }
    skip_ws(s, len, &pos);
    if (pos < len && s[pos] == ',')
        pos++;
    return pos;
}

/*
 * Parse and validate a JSON number occupying exactly [start, len).
 * Returns 1 and stores the value when the whole span is a valid number.
 */
static int parse_number_value(const char *s, size_t len, double *out)
{
    size_t i = 0;
    double value = 0.0;
    int sign = 1;
    if (i < len && s[i] == '-') {
        sign = -1;
        i++;
    }
    if (i >= len)
        return 0;
    if (s[i] == '0') {
        i++;
    } else if (s[i] >= '1' && s[i] <= '9') {
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            value = value * 10.0 + (double)(s[i] - '0');
            i++;
        }
    } else {
        return 0;
    }
    if (i < len && s[i] == '.') {
        double scale = 0.1;
        i++;
        if (i >= len || s[i] < '0' || s[i] > '9')
            return 0;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            value += (double)(s[i] - '0') * scale;
            scale *= 0.1;
            i++;
        }
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        long exp = 0;
        int expSign = 1;
        long e;
        i++;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            if (s[i] == '-')
                expSign = -1;
            i++;
        }
        if (i >= len || s[i] < '0' || s[i] > '9')
            return 0;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            exp = exp * 10 + (s[i] - '0');
            if (exp > 400)
                exp = 400;
            i++;
        }
        e = exp * expSign;
        while (e > 0) {
            value *= 10.0;
            e--;
        }
        while (e < 0) {
            value /= 10.0;
            e++;
        }
    }
    if (i != len)
        return 0;
    if (out)
        *out = sign * value;
    return 1;
}

bool mg_json_get_num(struct mg_str json, const char *path, double *v)
{
    struct mg_str t = mg_json_get_tok(json, path);
    if (!t.buf || t.len == 0)
        return false;
    return parse_number_value(t.buf, t.len, v);
}

bool mg_json_get_bool(struct mg_str json, const char *path, bool *v)
{
    struct mg_str t = mg_json_get_tok(json, path);
    if (!t.buf)
        return false;
    if (t.len == 4 && memcmp(t.buf, "true", 4) == 0) {
        if (v)
            *v = true;
        return true;
    }
    if (t.len == 5 && memcmp(t.buf, "false", 5) == 0) {
        if (v)
            *v = false;
        return true;
    }
    return false;
}

/*
 * Unescape the content of a JSON string (without the surrounding quotes).
 * Handles standard JSON escapes and \uXXXX (including surrogate pairs,
 * emitted as UTF-8).  Lone surrogates are emitted as-is; the function never
 * writes out of bounds.
 */
static int json_unescape(const char *s,
                         size_t len,
                         char *out,
                         size_t cap,
                         size_t *out_len)
{
    size_t i = 0;
    size_t j = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        if (c != '\\') {
            if (j + 1 >= cap)
                return 0;
            out[j++] = (char)c;
            i++;
            continue;
        }
        /* escape sequence */
        i++;
        if (i >= len)
            return 0;
        switch (s[i]) {
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case '/':
            c = '/';
            break;
        case '"':
            c = '"';
            break;
        case '\\':
            c = '\\';
            break;
        case 'u': {
            size_t escPos = i - 1; /* backslash */
            unsigned char seq[4];
            size_t seqLen = 0;
            if (!decode_unicode_escape(s, len, &escPos, seq, &seqLen))
                return 0;
            i = escPos;
            if (j + seqLen >= cap)
                return 0;
            memcpy(out + j, seq, seqLen);
            j += seqLen;
            continue;
        }
        default:
            return 0;
        }
        i++; /* move past the escape letter */
        if (j + 1 >= cap)
            return 0;
        out[j++] = (char)c;
    }
    *out_len = j;
    return 1;
}

char *mg_json_get_str(struct mg_str json, const char *path)
{
    struct mg_str t = mg_json_get_tok(json, path);
    char *out;
    size_t out_len = 0;
    if (!t.buf || t.len < 2 || t.buf[0] != '"' ||
        t.buf[t.len - 1] != '"')
        return NULL;
    out = (char *)calloc(1, t.len);
    if (!out)
        return NULL;
    if (!json_unescape(t.buf + 1, t.len - 2, out, t.len, &out_len)) {
        free(out);
        return NULL;
    }
    out[out_len] = '\0';
    return out;
}
