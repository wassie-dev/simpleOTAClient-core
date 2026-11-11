/**
 * Host checks for tick conversion and the deferred-report policy.
 *
 *   make -C test/host test
 */

#include <stdio.h>

#include "sota_report.h"
#include "sota_time.h"

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

int main(void)
{
    check(sota_ms_to_ticks(3600000u, 1000) == 3600000u, "1 h at 1000 Hz");
    check(sota_ms_to_ticks(21600000u, 1000) == 21600000u,
          "6 h at 1000 Hz does not wrap");
    check(sota_ms_to_ticks(86400000u, 100) == 8640000u, "24 h at 100 Hz");
    check(sota_ms_to_ticks(0xFFFFFFFFull * 1000u, 1000) == SOTA_MAX_WAIT_TICKS,
          "a duration past the tick range saturates");
    check(SOTA_MAX_WAIT_TICKS != UINT32_MAX,
          "a saturated wait is never portMAX_DELAY, which waits forever");
    check(sota_ms_to_ticks(4294967294ull, 1000) == 4294967294u &&
              sota_ms_to_ticks(4294967295ull, 1000) == SOTA_MAX_WAIT_TICKS,
          "saturation boundary at 1000 Hz");
    check(sota_ms_to_ticks(42949672940ull, 100) == 4294967294u &&
              sota_ms_to_ticks(42949672960ull, 100) == SOTA_MAX_WAIT_TICKS,
          "saturation boundary at 100 Hz");
    check(sota_ms_to_ticks(15, 100) == 1u, "partial ticks round down");
    check(sota_ms_to_ticks(0, 1000) == 0u, "zero stays zero");
    check(sota_s_to_ticks(3600, 1000) == 3600000u, "1 h in seconds at 1000 Hz");
    check(sota_s_to_ticks(86400, 1000) == 86400000u, "24 h in seconds at 1000 Hz");
    check(sota_s_to_ticks(86400, 100) == 8640000u, "24 h in seconds at 100 Hz");
    check(sota_s_to_ticks(0xFFFFFFFFu, 1000) == SOTA_MAX_WAIT_TICKS,
          "a duration in seconds past the tick range saturates, never to portMAX_DELAY");

    check(sota_report_delivered(200), "a 2xx delivers the report");
    check(sota_report_delivered(404), "a 4xx delivers the report");
    check(sota_report_delivered(503), "a 5xx delivers the report");
    check(!sota_report_delivered(-1), "no response keeps the report");
    check(!sota_report_delivered(0), "status 0 keeps the report");

    check(sota_poll_interval_s(true, true, false, 0, 10, 3600) == 10,
          "a trial polls fast");
    check(sota_poll_interval_s(true, false, true, 0, 10, 3600) == 10,
          "a new pending report polls fast");
    check(sota_poll_interval_s(true, false, true, SOTA_REPORT_QUICK_RETRIES - 1, 10, 3600) == 10,
          "the last quick retry is still fast");
    check(sota_poll_interval_s(true, false, true, SOTA_REPORT_QUICK_RETRIES, 10, 3600) == 3600,
          "after the quick retries, a pending report waits the check interval");
    check(sota_poll_interval_s(true, false, false, 0, 10, 3600) == 3600,
          "steady state waits the check interval");
    check(sota_http_transient(-1) && sota_http_transient(0) && sota_http_transient(500) &&
              sota_http_transient(503) && sota_http_transient(429) && sota_http_transient(408),
          "no answer, 5xx, 408 and 429 are worth an early retry");
    check(!sota_http_transient(200) && !sota_http_transient(302) && !sota_http_transient(401) &&
              !sota_http_transient(403) && !sota_http_transient(404),
          "an answer that will not change is not");
    check(sota_retry_interval_s(0, 3600) == 3600 && sota_retry_interval_s(1, 3600) == 30 &&
              sota_retry_interval_s(2, 3600) == 120 && sota_retry_interval_s(3, 3600) == 600 &&
              sota_retry_interval_s(4, 3600) == 3600 && sota_retry_interval_s(255, 3600) == 3600,
          "retries after 30 s, 2 min and 10 min, then the interval");
    check(sota_retry_interval_s(1, 20) == 20 && sota_retry_interval_s(3, 300) == 300,
          "a retry never waits longer than the check interval");
    check(sota_jitter_s(3600, 0x80000000u) == 3600 && sota_jitter_s(3600, 0) == 3240 &&
              sota_jitter_s(3600, UINT32_MAX) == 3960,
          "an hour is spread from 54 to 66 minutes");
    check(sota_jitter_s(59, 0) == 59 && sota_jitter_s(10, UINT32_MAX) == 10 &&
              sota_jitter_s(60, 0) == 54,
          "waits under a minute stay exact");
    check(sota_jitter_s(UINT32_MAX, UINT32_MAX) == UINT32_MAX &&
              sota_jitter_s(UINT32_MAX, 0) == UINT32_MAX - UINT32_MAX / 5u / 2u,
          "the longest wait does not wrap");
    check(sota_poll_interval_s(false, false, false, 0, 10, 21600) == SOTA_CONNECT_POLL_S,
          "no network: poll again shortly, not a whole interval later");
    check(sota_poll_interval_s(false, true, false, 0, 10, 3600) == SOTA_CONNECT_POLL_S &&
              sota_poll_interval_s(false, false, true, 5, 10, 3600) == SOTA_CONNECT_POLL_S,
          "no network: shortly, also during a trial or with a report pending");

    if (g_failures) {
        printf("%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("report and tick host checks passed\n");
    return 0;
}
