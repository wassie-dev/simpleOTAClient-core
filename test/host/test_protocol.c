/**
 * Host checks for the request bodies and the check response.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "sota_json.h"
#include "sota_protocol.h"

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

#define SIG "RnV5622C7mDIKzs73+bTbKKAaBNDgFrpFg9EcfB/nAbT+QzLRzznHavLSjgjeEMc" \
            "0NmhNtKk9qUOESyG4FqmDg=="
#define HASH "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"

static sota_device_info_t device(void)
{
    sota_device_info_t d;
    memset(&d, 0, sizeof(d));
    d.device_id = "aa:bb:cc:dd:ee:ff";
    d.framework = "esp_idf";
    d.chip_family = "esp32c6";
    d.nvs_schema_version = 1;
    return d;
}

static void test_check_body(void)
{
    char out[512];
    sota_device_info_t d = device();
    sota_identity_t live;
    memset(&live, 0, sizeof(live));

    check(sota_protocol_check_body(&d, &live, NULL, out, sizeof(out)) &&
              strcmp(out, "{\"device_id\":\"aa:bb:cc:dd:ee:ff\",\"framework\":\"esp_idf\","
                          "\"chip_family\":\"esp32c6\",\"current_build_number\":0,"
                          "\"nvs_schema_version\":1}") == 0,
          "a fresh device sends only the required members");

    d.board_id = "devkit";
    d.hardware_revision = "rev2";
    d.partition_profile = "two_ota_large";
    d.security_mode = "signed";
    d.channel = "beta";
    d.labels_json = "{\"room\":\"lab\"}";
    live.build = 4294967295u;
    strcpy(live.hash, HASH);
    check(sota_protocol_check_body(&d, &live, "1.2 \"rc\"", out, sizeof(out)) &&
              strcmp(out, "{\"device_id\":\"aa:bb:cc:dd:ee:ff\",\"framework\":\"esp_idf\","
                          "\"chip_family\":\"esp32c6\",\"current_build_number\":4294967295,"
                          "\"nvs_schema_version\":1,\"board_id\":\"devkit\","
                          "\"hardware_revision\":\"rev2\",\"partition_profile\":\"two_ota_large\","
                          "\"security_mode\":\"signed\",\"channel\":\"beta\","
                          "\"version_label\":\"1.2 \\\"rc\\\"\",\"current_hash\":\"" HASH "\","
                          "\"labels\":{\"room\":\"lab\"}}") == 0,
          "every member, string values escaped, labels verbatim");

    bool b = false;
    uint32_t u = 0;
    char s[80];
    check(sota_json_get_u32(out, "current_build_number", &u) && u == 4294967295u &&
              sota_json_get_string(out, "version_label", s, sizeof(s)) &&
              strcmp(s, "1.2 \"rc\"") == 0 && !sota_json_get_bool(out, "labels", &b),
          "the body reads back with the scanner");
    check(!sota_json_get_u32("{\"meta\":{\"n\":7},\"list\":[{\"n\":8}]}", "n", &u),
          "a key inside a nested object or array is not a top-level key");
    check(sota_json_get_u32("{\"meta\":{\"n\":7,\"s\":\"}{\"},\"n\":9}", "n", &u) && u == 9,
          "the top-level key is found after nested members, braces in strings ignored");
    check(sota_json_get_string("{\"a\":\"url\",\"url\":\"x\"}", "url", s, sizeof(s)) &&
              strcmp(s, "x") == 0,
          "a value equal to the key's name is not taken for the key");
    check(!sota_json_get_u32("{\"n\":4294967296}", "n", &u),
          "a number past 32 bits is refused rather than wrapped");

    d.board_id = "";
    check(sota_protocol_check_body(&d, &live, "", out, sizeof(out)) &&
              !strstr(out, "board_id") && !strstr(out, "version_label"),
          "empty optional members are left out");

    size_t full = strlen(out);
    check(sota_protocol_check_body(&d, &live, NULL, out, full + 1) && strlen(out) == full,
          "a buffer of exactly the body's size is enough");
    bool all_short_fail = true;
    for (size_t cap = 0; cap <= full; ++cap) {
        all_short_fail = all_short_fail && !sota_protocol_check_body(&d, &live, NULL, out, cap);
    }
    check(all_short_fail, "every shorter buffer fails instead of truncating");
}

static void test_status_body(void)
{
    char out[256];
    check(sota_protocol_status_body("dev", "dep-1", "failed", 42, "checksum_mismatch", out,
                                    sizeof(out)) &&
              strcmp(out, "{\"device_id\":\"dev\",\"deployment_id\":\"dep-1\","
                          "\"event\":\"failed\",\"build_number\":42,"
                          "\"reason\":\"checksum_mismatch\"}") == 0,
          "status body with a reason");
    check(sota_protocol_status_body("dev", "dep-1", "flashed", 0, NULL, out, sizeof(out)) &&
              strcmp(out, "{\"device_id\":\"dev\",\"deployment_id\":\"dep-1\","
                          "\"event\":\"flashed\",\"build_number\":0}") == 0,
          "status body without a reason");
    check(!sota_protocol_status_body("dev", "dep-1", "flashed", 0, NULL, out, 20) &&
              !sota_protocol_status_body("dev", "dep-1", "flashed", 0, NULL, out, 0),
          "a status body that does not fit fails");
}

static sota_check_result_t parse(const char *resp, sota_offer_t *offer, char *url, size_t cap)
{
    memset(offer, 0x5A, sizeof(*offer));
    offer->url = url;
    offer->url_cap = cap;
    return sota_protocol_parse_check(resp, offer);
}

static void test_parse(void)
{
    char url[128];
    sota_offer_t o;
    char status[32];

    const char *none = "{\"update_available\": false, \"status\": \"up_to_date\"}";
    check(parse(none, &o, url, sizeof(url)) == SOTA_CHECK_NO_UPDATE && url[0] == '\0' &&
              o.build == 0 && !o.signed_offer,
          "no update; the offer is cleared");
    check(sota_protocol_check_status(none, status, sizeof(status)) &&
              strcmp(status, "up_to_date") == 0,
          "the server's status when nothing is offered");
    check(parse("{}", &o, url, sizeof(url)) == SOTA_CHECK_NO_UPDATE &&
              !sota_protocol_check_status("{}", status, sizeof(status)),
          "an empty response offers nothing");

    const char *full =
        "{\"update_available\":true,\"url\":\"https://cdn.example/fw.bin?sig=a%2Fb\","
        "\"checksum\":\"ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef0123456789\","
        "\"build_number\":42,\"size\":1048576,\"deployment_id\":\"dep-42\","
        "\"version\":\"1.4.2\",\"security_mode\":\"signed\",\"signature\":\"" SIG "\","
        "\"signature_algorithm\":\"ed25519\",\"signing_key_id\":\"prod-2026\"}";
    check(parse(full, &o, url, sizeof(url)) == SOTA_CHECK_OFFER &&
              strcmp(url, "https://cdn.example/fw.bin?sig=a%2Fb") == 0 &&
              strcmp(o.checksum, HASH) == 0 && o.build == 42 && o.size == 1048576 &&
              strcmp(o.deployment, "dep-42") == 0 && strcmp(o.version, "1.4.2") == 0 &&
              strcmp(o.key_id, "prod-2026") == 0 && o.signed_offer && o.has_signature &&
              o.signature[0] == 0x46 && o.url == url && o.url_cap == sizeof(url),
          "a full signed offer; the checksum is lowercased");

    const char *minimal =
        "{\"update_available\":true,\"url\":\"https://x/fw\",\"checksum\":\"" HASH "\","
        "\"build_number\":7}";
    check(parse(minimal, &o, url, sizeof(url)) == SOTA_CHECK_OFFER && o.build == 7 &&
              o.size == 0 && o.deployment[0] == '\0' && o.version[0] == '\0' &&
              o.key_id[0] == '\0' && !o.signed_offer && !o.has_signature,
          "a minimal offer; optional members empty");

    check(parse("{\"update_available\":true,\"checksum\":\"" HASH "\",\"build_number\":7}",
                &o, url, sizeof(url)) == SOTA_CHECK_MALFORMED &&
              parse("{\"update_available\":true,\"url\":\"https://x\",\"build_number\":7}",
                    &o, url, sizeof(url)) == SOTA_CHECK_MALFORMED &&
              parse("{\"update_available\":true,\"url\":\"https://x\",\"checksum\":\"" HASH "\"}",
                    &o, url, sizeof(url)) == SOTA_CHECK_MALFORMED &&
              url[0] == '\0',
          "an offer without url, checksum or build is malformed");
    check(parse(minimal, &o, url, 8) == SOTA_CHECK_MALFORMED && url[0] == '\0',
          "a url longer than the buffer is malformed, never truncated");
    check(parse(minimal, &o, NULL, 0) == SOTA_CHECK_MALFORMED, "no url buffer");
    check(parse("{\"update_available\":true,\"url\":\"https://x\",\"checksum\":\"" HASH
                "0\",\"build_number\":7}",
                &o, url, sizeof(url)) == SOTA_CHECK_MALFORMED,
          "a checksum longer than sha256 hex is malformed");

    char long_version[300];
    snprintf(long_version, sizeof(long_version),
             "{\"update_available\":true,\"url\":\"https://x\",\"checksum\":\"" HASH "\","
             "\"build_number\":7,\"version\":\"%0100d\"}", 0);
    check(parse(long_version, &o, url, sizeof(url)) == SOTA_CHECK_OFFER && o.version[0] == '\0',
          "a version too long to store is left empty");
}

static void test_signatures(void)
{
    char url[64];
    sota_offer_t o;
    char resp[512];
    struct {
        const char *extra;
        bool signed_offer;
        bool has_signature;
        const char *name;
    } cases[] = {
        {",\"security_mode\":\"signed\",\"signature\":\"" SIG "\"", true, true,
         "no algorithm named: ed25519"},
        {",\"security_mode\":\"signed\",\"signature\":\"" SIG "\",\"signature_algorithm\":\"\"",
         true, true, "an empty algorithm: ed25519"},
        {",\"security_mode\":\"signed\",\"signature\":\"" SIG "\",\"signature_algorithm\":\"rsa\"",
         true, false, "another algorithm: unusable"},
        {",\"security_mode\":\"signed\",\"signature\":\"" SIG
         "\",\"signature_algorithm\":\"ed25519-with-a-very-long-suffix\"",
         true, false, "an algorithm name too long to be ed25519: unusable"},
        {",\"security_mode\":\"signed\",\"signature\":\"" SIG "\",\"signature_algorithm\":null",
         true, false, "an algorithm that is not a string: unusable"},
        {",\"security_mode\":\"signed\",\"signature\":\"QUJD\"", true, false,
         "a signature that is not 64 bytes: unusable"},
        {",\"security_mode\":\"signed\"", true, false, "signed without a signature"},
        {",\"security_mode\":\"basic\",\"signature\":\"" SIG "\"", false, false,
         "a signature on a basic offer is ignored"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        snprintf(resp, sizeof(resp),
                 "{\"update_available\":true,\"url\":\"https://x\",\"checksum\":\"" HASH
                 "\",\"build_number\":7%s}",
                 cases[i].extra);
        bool ok = parse(resp, &o, url, sizeof(url)) == SOTA_CHECK_OFFER &&
                  o.signed_offer == cases[i].signed_offer &&
                  o.has_signature == cases[i].has_signature;
        check(ok, cases[i].name);
    }
}

int main(void)
{
    test_check_body();
    test_status_body();
    test_parse();
    test_signatures();

    if (g_failures) {
        printf("%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("protocol host checks passed\n");
    return 0;
}
