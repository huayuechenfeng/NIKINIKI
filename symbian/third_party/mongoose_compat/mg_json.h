#ifndef WILIWILI_MG_JSON_COMPAT_H
#define WILIWILI_MG_JSON_COMPAT_H

/*
 * NIKINIKI mongoose-compatible JSON reader.
 *
 * This header intentionally exposes only the mg_* API surface used by the
 * Symbian Bilibili parsers.  It is a clean-room replacement for the
 * GPL-2.0-only mongoose json.c/str.c helpers and is implemented from
 * scratch against the documented behavior contract; it does not contain
 * mongoose code.
 *
 * struct mg_str is a non-owning view.  No function in this layer allocates
 * memory except mg_json_get_str(), whose result must be released with
 * free() (the parsers use std::free).
 */

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#else
/* C99 bool when available; otherwise a compatible int typedef. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdbool.h>
#else
typedef int bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#endif
#endif

struct mg_str {
    char *buf;   /* Pointer into the JSON document, never owned. */
    size_t len;  /* Length in bytes of the view. */
};

/* Build a non-owning view over [s, s+n). */
struct mg_str mg_str_n(const char *s, size_t n);

/*
 * Resolve a JSON path and return the raw token span, including quotes for
 * strings and the surrounding brackets/braces for containers.  On failure
 * buf is NULL and len is 0.
 *
 * Supported path grammar (the subset used by the port):
 *   $           whole document
 *   $.key       object member
 *   $.a.b.c     nested object members
 *   $.array[0]  array element
 *   $.a.b[3].c  mixed
 */
struct mg_str mg_json_get_tok(struct mg_str json, const char *path);

/*
 * Iterate object or array elements.
 *
 * Call protocol:
 *   size_t ofs = 0;
 *   while ((ofs = mg_json_next(obj, ofs, NULL, &val)) != 0) { ... }
 *
 * Returns the next scan offset, or 0 when iteration is finished or the
 * input is not an array/object.  For arrays, key is left untouched (callers
 * pass NULL); for objects, key receives the raw quoted key token.
 */
size_t mg_json_next(struct mg_str obj,
                    size_t ofs,
                    struct mg_str *key,
                    struct mg_str *val);

/*
 * Read a JSON number token.  Returns 1 (true) only when the token at `path`
 * is a valid JSON number (integer, negative, decimal or exponent) and stores
 * the value in *v when v is non-NULL.
 */
bool mg_json_get_num(struct mg_str json, const char *path, double *v);

/*
 * Read a JSON boolean literal.  Returns 1 (true) only for the exact tokens
 * `true` / `false` and stores the value in *v when v is non-NULL.
 */
bool mg_json_get_bool(struct mg_str json, const char *path, bool *v);

/*
 * Read a JSON string token and return a NUL-terminated, unescaped copy
 * allocated with calloc().  The caller releases it with free() (the parsers
 * use std::free).  Returns NULL when the token is missing or not a string.
 */
char *mg_json_get_str(struct mg_str json, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WILIWILI_MG_JSON_COMPAT_H */
