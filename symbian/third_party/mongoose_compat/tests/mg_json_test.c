/*
 * Standalone tests for the NIKINIKI mongoose-compatible JSON reader.
 *
 * Phase 1: mg_str_n / mg_json_get_tok / mg_json_next.
 * Phase 2: mg_json_get_num / mg_json_get_bool / mg_json_get_str.
 *
 * Build and run with the host C compiler (no Qt, no Symbian dependency):
 *   cl mg_json_test.c ..\mg_json.c
 *   gcc mg_json_test.c ../mg_json.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../mg_json.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        ++g_checks;                                                    \
        if (!(cond)) {                                                 \
            ++g_failures;                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                              \
    } while (0)

#define CHECK_STR(actualBuf, actualLen, expected)                      \
    do {                                                               \
        size_t expLen_ = strlen(expected);                             \
        ++g_checks;                                                    \
        if (!(actualBuf) || (actualLen) != expLen_ ||                  \
            memcmp((actualBuf), (expected), expLen_) != 0) {           \
            ++g_failures;                                              \
            printf("FAIL %s:%d: expected \"%s\" got \"%.*s\" (%u)\n",  \
                   __FILE__, __LINE__, (expected),                     \
                   (int)(actualLen),                                   \
                   (actualBuf) ? (actualBuf) : "(null)",               \
                   (unsigned int)(actualLen));                         \
        }                                                              \
    } while (0)

static struct mg_str from_lit(const char *s)
{
    return mg_str_n(s, strlen(s));
}

static void test_get_tok_basic(void)
{
    struct mg_str doc = from_lit(
        "{\"data\":{\"b\":123},\"name\":\"abc\",\"n\":42,"
        "\"flag\":true,\"z\":null,\"e\":[],\"s\":\"a\\\"b\","
        "\"array\":[10,{\"x\":\"deep\"},[1,2]]}");
    struct mg_str t;

    t = mg_json_get_tok(doc, "$.data.b");
    CHECK_STR(t.buf, t.len, "123");

    t = mg_json_get_tok(doc, "$.name");
    CHECK_STR(t.buf, t.len, "\"abc\""); /* string token keeps quotes */

    t = mg_json_get_tok(doc, "$.n");
    CHECK_STR(t.buf, t.len, "42");

    t = mg_json_get_tok(doc, "$.flag");
    CHECK_STR(t.buf, t.len, "true");

    t = mg_json_get_tok(doc, "$.z");
    CHECK_STR(t.buf, t.len, "null");

    t = mg_json_get_tok(doc, "$.e");
    CHECK_STR(t.buf, t.len, "[]");

    t = mg_json_get_tok(doc, "$.s");
    CHECK_STR(t.buf, t.len, "\"a\\\"b\""); /* escaped quote preserved raw */

    t = mg_json_get_tok(doc, "$.array[0]");
    CHECK_STR(t.buf, t.len, "10");

    t = mg_json_get_tok(doc, "$.array[1].x");
    CHECK_STR(t.buf, t.len, "\"deep\"");

    t = mg_json_get_tok(doc, "$.array[2][1]");
    CHECK_STR(t.buf, t.len, "2");

    /* Whole document with $ */
    t = mg_json_get_tok(doc, "$");
    CHECK(t.buf && t.len == doc.len && memcmp(t.buf, doc.buf, doc.len) == 0);
}

static void test_get_tok_missing(void)
{
    struct mg_str doc = from_lit("{\"a\":{\"b\":1},\"arr\":[5]}");
    struct mg_str t;

    t = mg_json_get_tok(doc, "$.missing");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(doc, "$.a.missing");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(doc, "$.arr[3]");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(doc, "$.arr[-1]");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(doc, "$bad");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(doc, "a.b");
    CHECK(!t.buf && t.len == 0);
    t = mg_json_get_tok(from_lit(""), "$");
    CHECK(!t.buf && t.len == 0);
}

static void test_next_array(void)
{
    struct mg_str doc = from_lit("[1,2,3]");
    struct mg_str list = mg_json_get_tok(doc, "$");
    struct mg_str val;
    size_t ofs = 0;
    int count = 0;
    const char *expected[3] = { "1", "2", "3" };

    while ((ofs = mg_json_next(list, ofs, NULL, &val)) != 0) {
        CHECK(count < 3);
        if (count < 3)
            CHECK_STR(val.buf, val.len, expected[count]);
        ++count;
    }
    CHECK(count == 3);
}

static void test_next_empty(void)
{
    struct mg_str val;
    size_t ofs = 0;
    int count = 0;
    struct mg_str emptyArray = from_lit("[]");
    struct mg_str emptyObject = from_lit("{}");

    while ((ofs = mg_json_next(emptyArray, ofs, NULL, &val)) != 0)
        ++count;
    CHECK(count == 0);

    ofs = 0;
    count = 0;
    while ((ofs = mg_json_next(emptyObject, ofs, NULL, &val)) != 0)
        ++count;
    CHECK(count == 0);
}

static void test_next_object(void)
{
    struct mg_str doc = from_lit("{\"k1\":1,\"k2\":\"v\"}");
    struct mg_str key;
    struct mg_str val;
    size_t ofs = 0;
    int count = 0;
    const char *expectedKeys[2] = { "\"k1\"", "\"k2\"" };

    while ((ofs = mg_json_next(doc, ofs, &key, &val)) != 0) {
        CHECK(count < 2);
        if (count < 2)
            CHECK_STR(key.buf, key.len, expectedKeys[count]);
        ++count;
    }
    CHECK(count == 2);
}

static void test_next_whitespace(void)
{
    struct mg_str doc = from_lit("  [ 1 , 2 ]  ");
    struct mg_str list = mg_json_get_tok(doc, "$");
    struct mg_str val;
    size_t ofs = 0;
    int count = 0;
    while ((ofs = mg_json_next(list, ofs, NULL, &val)) != 0)
        ++count;
    CHECK(count == 2);
}

static void test_next_nested(void)
{
    struct mg_str doc = from_lit(
        "[{\"a\":1,\"b\":[true,null]},{\"c\":\"x\"}]");
    struct mg_str item;
    struct mg_str t;
    size_t ofs = 0;
    int count = 0;
    while ((ofs = mg_json_next(doc, ofs, NULL, &item)) != 0) {
        ++count;
        if (count == 1) {
            t = mg_json_get_tok(item, "$.b[1]");
            CHECK_STR(t.buf, t.len, "null");
        } else if (count == 2) {
            t = mg_json_get_tok(item, "$.c");
            CHECK_STR(t.buf, t.len, "\"x\"");
        }
    }
    CHECK(count == 2);
}

static void test_get_num(void)
{
    struct mg_str doc = from_lit(
        "{\"i\":-7,\"d\":3.5,\"e\":1.5e3,\"neg\":-0.25,\"big\":12345678901234}");
    double v = 0.0;

    CHECK(mg_json_get_num(doc, "$.i", &v) && v == -7.0);
    CHECK(mg_json_get_num(doc, "$.d", &v) && v == 3.5);
    CHECK(mg_json_get_num(doc, "$.e", &v) && v == 1500.0);
    CHECK(mg_json_get_num(doc, "$.neg", &v) && v == -0.25);
    CHECK(mg_json_get_num(doc, "$.big", &v) && v == 12345678901234.0);
    CHECK(mg_json_get_num(doc, "$.missing", &v) == false);
    CHECK(mg_json_get_num(doc, "$.d", NULL) == true);
}

static void test_get_num_invalid(void)
{
    double v = 0.0;

    /* A string token is not a number. */
    CHECK(mg_json_get_num(from_lit("{\"a\":\"12\"}"), "$.a", &v) == false);
    /* Leading zero is invalid JSON; lookup fails as well. */
    CHECK(mg_json_get_num(from_lit("01"), "$", &v) == false);
    /* Trailing junk after a valid prefix is not a valid number. */
    CHECK(mg_json_get_num(from_lit("1.2.3"), "$", &v) == false);
    /* Valid document with a valid array number. */
    CHECK(mg_json_get_num(from_lit("{\"e\":[1]}"), "$.e[0]", &v) &&
          v == 1.0);
}

static void test_get_bool(void)
{
    struct mg_str doc = from_lit("{\"t\":true,\"f\":false,\"n\":null,\"s\":\"true\"}");
    bool v = false;

    CHECK(mg_json_get_bool(doc, "$.t", &v) && v == true);
    CHECK(mg_json_get_bool(doc, "$.f", &v) && v == false);
    CHECK(mg_json_get_bool(doc, "$.n", &v) == false);
    CHECK(mg_json_get_bool(doc, "$.s", &v) == false);
    CHECK(mg_json_get_bool(doc, "$.missing", &v) == false);
}

static void test_get_str(void)
{
    struct mg_str doc = from_lit(
        "{\"a\":\"plain\",\"b\":\"line\\nfeed\\ttab\\r\\\"quote\\\\slash\","
        "\"c\":\"\\u4e2d\\u6587\",\"d\":\"\\uD83D\\uDE00\","
        "\"e\":123,\"f\":\"x\"}");
    char *s;

    s = mg_json_get_str(doc, "$.a");
    CHECK(s && strcmp(s, "plain") == 0);
    free(s);

    s = mg_json_get_str(doc, "$.b");
    CHECK(s && strcmp(s, "line\nfeed\ttab\r\"quote\\slash") == 0);
    free(s);

    s = mg_json_get_str(doc, "$.c"); /* \u4e2d\u6587 -> UTF-8 */
    CHECK(s && strcmp(s, "\xe4\xb8\xad\xe6\x96\x87") == 0);
    free(s);

    s = mg_json_get_str(doc, "$.d"); /* surrogate pair -> F0 9F 98 80 */
    CHECK(s && strcmp(s, "\xf0\x9f\x98\x80") == 0);
    free(s);

    CHECK(mg_json_get_str(doc, "$.e") == NULL); /* number is not a string */
    CHECK(mg_json_get_str(doc, "$.missing") == NULL);
}

int main(void)
{
    test_get_tok_basic();
    test_get_tok_missing();
    test_next_array();
    test_next_empty();
    test_next_object();
    test_next_whitespace();
    test_next_nested();
    test_get_num();
    test_get_num_invalid();
    test_get_bool();
    test_get_str();

    if (g_failures == 0) {
        printf("PASS: all %d checks\n", g_checks);
        return 0;
    }
    printf("FAIL: %d of %d checks failed\n", g_failures, g_checks);
    return 1;
}
