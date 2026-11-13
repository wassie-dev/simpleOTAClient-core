/**
 * Host checks for the default TLS trust, built for each framework, core
 * version and bundle setting (see the Makefile), which sets EXPECT_TRUST.
 *
 *   make -C test/host test
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sota_framework.h"
#include "sota_sha256.h"

#include "esp_crt_bundle.h"
#include "esp_log.h"

#define TRUST_NONE 0
#define TRUST_BUNDLE 1
#define TRUST_PEM 2

esp_err_t esp_crt_bundle_attach(void *conf)
{
    (void)conf;
    return ESP_OK;
}

esp_log_level_t esp_log_level_get(const char *tag)
{
    (void)tag;
    return ESP_LOG_ERROR;
}

void esp_log_level_set(const char *tag, esp_log_level_t level)
{
    (void)tag;
    (void)level;
}

static esp_err_t other_attach(void *conf)
{
    (void)conf;
    return ESP_OK;
}

#if EXPECT_TRUST == TRUST_PEM
/* SHA-256 of ISRG Root X1, ISRG Root X2 and GTS Root R4. */
static const char *const k_roots[] = {
    "96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6",
    "69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470",
    "349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d",
};
#define NUM_ROOTS (sizeof(k_roots) / sizeof(k_roots[0]))

static int b64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Decodes one certificate's base64 body, from p up to its END line, into
 * der, and returns its length (0 on a malformed body). */
static size_t decode(const char *p, const char *end, unsigned char *der, size_t cap)
{
    size_t n = 0;
    unsigned acc = 0;
    int bits = 0;
    for (; p < end; ++p) {
        if (*p == '\n' || *p == '=') continue;
        const int v = b64(*p);
        if (v < 0) return 0;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n == cap) return 0;
            der[n++] = (unsigned char)(acc >> bits);
        }
    }
    return n;
}

/* True when pem holds exactly the certificates in k_roots, in order. */
static bool pem_is_roots(const char *pem)
{
    static const char begin[] = "-----BEGIN CERTIFICATE-----\n";
    static const char finish[] = "-----END CERTIFICATE-----\n";
    size_t found = 0;
    while (*pem) {
        if (strncmp(pem, begin, sizeof(begin) - 1) != 0 || found == NUM_ROOTS) return false;
        const char *body = pem + sizeof(begin) - 1;
        const char *end = strstr(body, finish);
        if (!end) return false;
        unsigned char der[2048], digest[32];
        const size_t len = decode(body, end, der, sizeof(der));
        if (!len) return false;
        sota_sha256_ctx ctx;
        sota_sha256_init(&ctx);
        sota_sha256_update(&ctx, der, len);
        sota_sha256_final(&ctx, digest);
        char hex[65];
        for (int i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
        if (strcmp(hex, k_roots[found++]) != 0) return false;
        pem = end + sizeof(finish) - 1;
    }
    return found == NUM_ROOTS;
}
#endif

int main(void)
{
    const char *pem = "unset";
    esp_err_t (*attach)(void *conf) = other_attach;
    const bool ok = sota_framework_default_trust(&pem, &attach);
#if EXPECT_TRUST == TRUST_BUNDLE
    const bool pass = ok && !pem && attach == esp_crt_bundle_attach;
    const char *what = "the default is ESP-IDF's certificate bundle";
#elif EXPECT_TRUST == TRUST_PEM
    const bool pass = ok && !attach && pem && pem_is_roots(pem);
    const char *what = "the default is ISRG Root X1 and X2 and GTS Root R4";
#else
    const bool pass = !ok && !pem && !attach;
    const char *what = "there is no default trust";
#endif
    printf("%s  %s: %s\n", pass ? "  ok" : "FAIL", TEST_NAME, what);
    return pass ? 0 : 1;
}
