/**
 * fake_hal.c - see fake_hal.h.
 */

#include "fake_hal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_kv.h"
#include "sota_boot.h"
#include "sota_engine.h"
#include "sota_image.h"
#include "sota_json.h"
#include "sota_net.h"
#include "sota_sha256.h"
#include "sota_sys.h"
#include "sota_trial.h"

fake_world_t fw;
fake_server_t fs;
jmp_buf fake_reset_target;
sota_event_id_t fake_events[64];
int fake_event_count;
int fake_wakes;

static const uint32_t k_addr[2] = {FAKE_SLOT_A, FAKE_SLOT_B};

static int slot_of(uint32_t addr)
{
    return addr == FAKE_SLOT_A ? 0 : addr == FAKE_SLOT_B ? 1 : -1;
}

static void power_cut(void)
{
    longjmp(fake_reset_target, FAKE_JUMP_POWER_CUT);
}

void fake_world_reset(bool bl_rollback)
{
    memset(&fw, 0, sizeof(fw));
    fw.bl_rollback = bl_rollback;
    fw.build[0] = 1;
    fw.state[0] = IMG_VALID;
    fw.writing = -1;
    fw.reset_reason = SOTA_RST_POWERON;
    memset(&fs, 0, sizeof(fs));
    fs.up = true;
    fs.dl_status = 200;
    fs.dl_fail_at = -1;
    fs.dl_fail_with = SOTA_NET_AGAIN;
    fake_event_count = 0;
    fake_wakes = 0;
    fake_kv_set_cut_hook(power_cut);
}

void fake_server_offer(uint32_t build, const char *dep)
{
    fs.offer_build = build;
    snprintf(fs.deployment, sizeof(fs.deployment), "%s", dep);
    fs.image_len = sizeof(fs.image);
    uint32_t x = build * 2654435761u;
    for (size_t i = 0; i < fs.image_len; ++i) {
        x = x * 1103515245u + 12345u;
        fs.image[i] = (uint8_t)(x >> 16);
    }
    memcpy(fs.image, &build, sizeof(build));
    uint8_t digest[32];
    sota_sha256_ctx ctx;
    sota_sha256_init(&ctx);
    sota_sha256_update(&ctx, fs.image, fs.image_len);
    sota_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) sprintf(fs.checksum + 2 * i, "%02x", digest[i]);
}

/* ESP-IDF's: a new image becomes pending on its first boot, and a pending
 * one that was not confirmed before the next reset is given up. */
void fake_bootloader(int reset_reason)
{
    fw.reset_reason = reset_reason;
    fw.writing = -1;
    fw.timer_armed = false;
    /* A wake from deep sleep resumes the image that slept. */
    if (reset_reason == SOTA_RST_DEEPSLEEP) return;
    if (fw.bl_rollback) {
        if (fw.state[fw.boot] == IMG_NEW) {
            fw.state[fw.boot] = IMG_PENDING;
        } else if (fw.state[fw.boot] == IMG_PENDING) {
            fw.state[fw.boot] = IMG_ABORTED;
            fw.boot ^= 1;
        }
    }
    fw.running = fw.boot;
}

int fake_server_count(const char *event, const char *reason)
{
    char want[48];
    snprintf(want, sizeof(want), "%s %s", event, reason ? reason : "");
    int n = 0;
    for (int i = 0; i < fs.log_count; ++i) {
        if (reason ? strcmp(fs.log[i], want) == 0
                   : strncmp(fs.log[i], want, strlen(event) + 1) == 0) {
            ++n;
        }
    }
    return n;
}

int fake_event_seen(sota_event_id_t id)
{
    int n = 0;
    for (int i = 0; i < fake_event_count; ++i) n += fake_events[i] == id;
    return n;
}

/* ---- sota_image.h ------------------------------------------------------ */

bool sota_image_begin(size_t expected, uint32_t *slot, uint32_t *running)
{
    (void)expected;
    fw.writing = fw.running ^ 1;
    fw.written_len = 0;
    *slot = k_addr[fw.writing];
    *running = k_addr[fw.running];
    return true;
}

bool sota_image_write(const uint8_t *buf, size_t len)
{
    if (fw.writing < 0 || fw.written_len + len > sizeof(fw.written)) return false;
    memcpy(fw.written + fw.written_len, buf, len);
    fw.written_len += len;
    return true;
}

void sota_image_abort(void)
{
    fw.writing = -1;
}

bool sota_image_finish(void)
{
    int slot = fw.writing;
    fw.writing = -1;
    if (slot < 0 || fw.fail_finish || fw.written_len < 4) return false;
    memcpy(&fw.build[slot], fw.written, 4);
    fw.state[slot] = IMG_UNDEFINED;
    return true;
}

/* ---- sota_boot.h -------------------------------------------------------- */

void sota_boot_info(sota_boot_info_t *info)
{
    info->running_part = k_addr[fw.running];
    info->boot_part = k_addr[fw.boot];
    info->reset_reason = fw.reset_reason;
    info->pending = fw.bl_rollback && fw.state[fw.running] == IMG_PENDING;
}

bool sota_boot_slot_invalid(uint32_t part)
{
    int slot = slot_of(part);
    return slot >= 0 && fw.state[slot] == IMG_INVALID;
}

bool sota_boot_set(uint32_t part)
{
    int slot = slot_of(part);
    if (slot < 0 || fw.fail_boot_set || !fake_kv_step()) return false;
    fw.boot = slot;
    if (fw.bl_rollback) fw.state[slot] = IMG_NEW;
    return true;
}

bool sota_boot_mark_valid(void)
{
    if (fw.fail_mark_valid) return false;
    if (fw.state[fw.running] == IMG_PENDING) fw.state[fw.running] = IMG_VALID;
    return true;
}

/* Either way the slot change is a flash write that a power cut can stop. */
void sota_boot_rollback(uint32_t prev_part)
{
    fake_kv_step();
    if (fw.bl_rollback && fw.state[fw.running] == IMG_PENDING) {
        fw.state[fw.running] = IMG_INVALID;
        fw.boot = fw.running ^ 1;
    } else if (slot_of(prev_part) >= 0) {
        fw.boot = slot_of(prev_part);
    }
    sota_sys_restart();
}

/* ---- sota_sys.h --------------------------------------------------------- */

void sota_sys_sleep_ms(uint32_t ms) { fw.sleep_ms += ms; }
uint32_t sota_sys_now_ms(void) { return fw.sleep_ms; }
void sota_sys_lock(void) {}
void sota_sys_unlock(void) {}

bool sota_sys_timer_start(void **timer, uint32_t seconds, struct sota_engine *e)
{
    if (fw.fail_timer) return false;
    *timer = e;
    fw.timer_armed = true;
    fw.timer_s = seconds;
    return true;
}

void sota_sys_timer_stop(void **timer)
{
    *timer = NULL;
    fw.timer_armed = false;
}

bool sota_sys_wdt_arm(uint32_t confirm_s, uint32_t floor_s)
{
    (void)confirm_s;
    (void)floor_s;
    fw.wdt_armed = true;
    return false;
}

void sota_sys_wdt_disarm(void) { fw.wdt_armed = false; }
void sota_sys_wdt_quiesce(void) { fw.wdt_armed = false; }

void sota_sys_restart(void)
{
    fw.reset_reason = SOTA_RST_SW;
    longjmp(fake_reset_target, FAKE_JUMP_RESTART);
}

/* ---- sota_app.h --------------------------------------------------------- */

void sota_app_event(void *user, const sota_event_t *event)
{
    (void)user;
    if (fake_event_count < 64) fake_events[fake_event_count++] = event->id;
}

void sota_app_wake(void *user)
{
    (void)user;
    ++fake_wakes;
}

void sota_app_log(int level, const char *fmt, ...)
{
    if (!getenv("SOTA_VERBOSE")) return;
    va_list ap;
    va_start(ap, fmt);
    printf("  [%d] ", level);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

/* ---- sota_net.h: the server --------------------------------------------- */

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static int answer(char *resp, size_t cap, const char *text)
{
    if (resp && cap) snprintf(resp, cap, "%s", text);
    return 200;
}

static int check_in(const char *body, char *resp, size_t cap)
{
    uint32_t current = 0;
    sota_json_get_u32(body, "current_build_number", &current);
    ++fs.checks;
    if (!fs.offer_build || current >= fs.offer_build) {
        return answer(resp, cap, "{\"update_available\":false,\"status\":\"up_to_date\"}");
    }
    char offer[1024];
    snprintf(offer, sizeof(offer),
             "{\"update_available\":true,\"url\":\"https://dl.example/fw.bin\","
             "\"checksum\":\"%s\",\"build_number\":%u,\"deployment_id\":\"%s\","
             "\"version\":\"%u.0.0\",\"size\":%u%s%s}",
             fs.checksum, (unsigned)fs.offer_build, fs.deployment, (unsigned)fs.offer_build,
             (unsigned)fs.image_len, fs.extra ? "," : "", fs.extra ? fs.extra : "");
    return answer(resp, cap, offer);
}

/* A rolled_back report ends the deployment for this device. */
static int status(const char *body, char *resp, size_t cap)
{
    char event[24] = "", reason[24] = "";
    sota_json_get_string(body, "event", event, sizeof(event));
    sota_json_get_string(body, "reason", reason, sizeof(reason));
    if (fs.log_count < 64) snprintf(fs.log[fs.log_count++], 48, "%s %s", event, reason);
    if (strcmp(event, "rolled_back") == 0) fs.offer_build = 0;
    return answer(resp, cap, "{\"accepted\":true}");
}

int sota_net_post(void *user, const char *url, const char *token, const char *body,
                  char *resp, size_t cap)
{
    (void)user;
    if (!fs.up || strcmp(token, "tok") != 0) return -1;
    if (ends_with(url, SOTA_PATH_CHECK)) return check_in(body, resp, cap);
    if (ends_with(url, SOTA_PATH_STATUS)) return status(body, resp, cap);
    return 404;
}

int sota_net_dl_open(void *user, const char *url, int64_t *len, char *location,
                     size_t location_cap)
{
    (void)user;
    if (!fs.up) return 0;
    snprintf(fs.opened, sizeof(fs.opened), "%s", url);
    const char *to = fs.redirects_done < 5 ? fs.redirect[fs.redirects_done] : NULL;
    if (to) {
        fs.redirects_done++;
        snprintf(location, location_cap, "%s", to);
        return 302;
    }
    fs.dl_pos = 0;
    *len = fs.chunked ? -1 : (int64_t)fs.image_len;
    return fs.dl_status;
}

int sota_net_dl_read(void *user, uint8_t *buf, size_t cap)
{
    (void)user;
    size_t left = fs.image_len - fs.dl_pos;
    if (fs.dl_fail_at >= 0 && fs.dl_pos >= (size_t)fs.dl_fail_at) return fs.dl_fail_with;
    size_t n = left < cap ? left : cap;
    memcpy(buf, fs.image + fs.dl_pos, n);
    if (fs.corrupt && fs.dl_pos == 0 && n > 100) buf[100] ^= 0xffu;
    fs.dl_pos += n;
    return (int)n;
}

void sota_net_dl_close(void *user)
{
    (void)user;
}
