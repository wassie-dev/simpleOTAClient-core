/**
 * Host checks for the server base URL.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "sota_url.h"

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

static const struct {
    const char *in;
    sota_url_status_t want;
} k_url[] = {
    {"HTTPS://user:pass@ota.example:8443/a/../b", SOTA_URL_OK},
    {"HTTP://127.0.0.1", SOTA_URL_NOT_HTTPS},
    {"https://", SOTA_URL_MALFORMED},
    {"https:///", SOTA_URL_MALFORMED},
    {"ftp://simpleota.com", SOTA_URL_MALFORMED},
    {"https://a b", SOTA_URL_MALFORMED},
    {"https://ota\texample.com", SOTA_URL_MALFORMED},
    {"https://h\x7f", SOTA_URL_MALFORMED},
    {"", SOTA_URL_MALFORMED},
};

int main(void)
{
    for (size_t i = 0; i < sizeof(k_url) / sizeof(k_url[0]); ++i)
        check(sota_validate_server_url(k_url[i].in) == k_url[i].want, k_url[i].in);

    check(sota_validate_server_url(NULL) == SOTA_URL_MALFORMED, "null");
    check(strcmp(SOTA_DEFAULT_SERVER_URL, "https://simpleota.com") == 0, "default");
    check(sota_validate_server_url(SOTA_DEFAULT_SERVER_URL) == SOTA_URL_OK, "default ok");
    check(strcmp(sota_url_status_name(SOTA_URL_OK), "ok") == 0, "ok");
    check(strcmp(sota_url_status_name(SOTA_URL_NOT_HTTPS), "not_https") == 0, "not_https");
    check(strcmp(sota_url_status_name(SOTA_URL_MALFORMED), "malformed") == 0, "malformed");

    {
        char url[300];
        memcpy(url, "https://", 8);
        memset(url + 8, 'a', 248);
        url[256] = '\0';
        check(strlen(url) == SOTA_SERVER_URL_MAX_LEN &&
                  sota_validate_server_url(url) == SOTA_URL_OK,
              "256 characters is accepted");
        url[256] = 'b';
        url[257] = '\0';
        check(sota_validate_server_url(url) == SOTA_URL_MALFORMED, "257 characters is not");
    }

    sota_url_status_t st = SOTA_URL_OK;
    const char *cur = SOTA_DEFAULT_SERVER_URL;
    check(sota_select_server_url(cur, "http://h", &st) == cur && st == SOTA_URL_NOT_HTTPS,
          "a rejected http url keeps the previous url");
    const char *next = "https://staging.example";
    check(sota_select_server_url(cur, next, &st) == next && st == SOTA_URL_OK,
          "an accepted url replaces the previous one");
    check(sota_select_server_url(cur, next, NULL) == next, "the status is optional");

    char buf[16];
    memset(buf, 'X', sizeof(buf));
    check(sota_build_api_url("HTTPS://ab/", "/c", buf, 13) && strcmp(buf, "HTTPS://ab/c") == 0,
          "join drops the trailing slash");
    memset(buf, 'X', sizeof(buf));
    check(!sota_build_api_url("HTTPS://ab/", "/c", buf, 12) && buf[0] == 'X',
          "a join that does not fit leaves the buffer alone");
    check(sota_build_api_url("https://", "/c", buf, sizeof(buf)) &&
              strcmp(buf, "https:///c") == 0,
          "the scheme's own slashes are kept");

    /* Redirect targets. */
    char out[64];
    check(sota_redirect_target("https://a.example/x", "https://b.example/y?s=1", out,
                               sizeof(out)) == SOTA_URL_OK &&
              strcmp(out, "https://b.example/y?s=1") == 0,
          "an absolute https redirect is taken as is");
    check(sota_redirect_target("https://a.example/x?q", "/y", out, sizeof(out)) == SOTA_URL_OK &&
              strcmp(out, "https://a.example/y") == 0,
          "a path redirect stays on the same host");
    check(sota_redirect_target("https://a.example", "/y", out, sizeof(out)) == SOTA_URL_OK &&
              strcmp(out, "https://a.example/y") == 0,
          "a path redirect from a URL without a path");
    memset(out, 'X', sizeof(out));
    check(sota_redirect_target("https://a/x", "http://b/y", out, sizeof(out)) ==
                  SOTA_URL_NOT_HTTPS && out[0] == 'X',
          "a redirect to plain HTTP is refused, out untouched");
    check(sota_redirect_target("https://a/x", "HTTP://b/y", out, sizeof(out)) == SOTA_URL_NOT_HTTPS,
          "in any case");
    check(sota_redirect_target("http://a/x", "/y", out, sizeof(out)) == SOTA_URL_NOT_HTTPS,
          "a path redirect from plain HTTP is refused");
    check(sota_redirect_target("https://a/x", "//b/y", out, sizeof(out)) == SOTA_URL_MALFORMED &&
              sota_redirect_target("https://a/x", "", out, sizeof(out)) == SOTA_URL_MALFORMED &&
              sota_redirect_target("https://a/x", "https:///y", out, sizeof(out)) ==
                  SOTA_URL_MALFORMED &&
              sota_redirect_target("https://a/x", "https://b/ y", out, sizeof(out)) ==
                  SOTA_URL_MALFORMED &&
              sota_redirect_target("https://a/x", "ftp://b/y", out, sizeof(out)) ==
                  SOTA_URL_MALFORMED,
          "scheme-relative, empty, hostless, spaced or other-scheme targets are malformed");
    check(sota_redirect_target("https://a/x", "https://b/0123456789", out, 20) ==
              SOTA_URL_MALFORMED,
          "a target that does not fit is refused");
    check(sota_validate_server_url("http://simpleota.com") == SOTA_URL_NOT_HTTPS,
          "a plain HTTP server address is refused");

    if (g_failures) {
        printf("%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("url host checks passed\n");
    return 0;
}
