/**
 * fake_kv.c - see fake_kv.h. Types are kept, as NVS keeps them: a read of
 * another type fails.
 */

#include "fake_kv.h"

#include <string.h>

#include "sota_kv.h"

enum { T_U8, T_U32, T_STR, T_BLOB };

typedef struct {
    char key[16];
    int type;
    size_t len;
    unsigned char data[256];
} entry_t;

static entry_t s_entries[32];
static size_t s_count;
static int s_budget = -1;
static int s_writes;
static int s_fail_at = -1;
static void (*s_cut_hook)(void);

void fake_kv_reset(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    s_count = 0;
    s_budget = -1;
    s_writes = 0;
    s_fail_at = -1;
}

void fake_kv_cut_after(int n) { s_budget = n; }
void fake_kv_fail_nth(int n) { s_fail_at = n; }
void fake_kv_set_cut_hook(void (*hook)(void)) { s_cut_hook = hook; }
void fake_kv_power_on(void) { s_budget = -1; }
int fake_kv_writes(void) { return s_writes; }
size_t fake_kv_count(void) { return s_count; }

static entry_t *find(const char *key)
{
    for (size_t i = 0; i < s_count; ++i) {
        if (strcmp(s_entries[i].key, key) == 0) return &s_entries[i];
    }
    return NULL;
}

bool fake_kv_step(void)
{
    if (s_budget == 0) {
        if (s_cut_hook) s_cut_hook();
        return false;
    }
    if (s_budget > 0) --s_budget;
    return true;
}

/* One write is allowed through the budget. NVS keys are at most 15 chars. */
static bool may_write(const char *key)
{
    if (strlen(key) > 15) return false;
    if (s_fail_at >= 0 && s_fail_at-- == 0) return false;
    if (!fake_kv_step()) return false;
    ++s_writes;
    return true;
}

static bool put(const char *key, int type, const void *data, size_t len)
{
    if (len > sizeof(s_entries[0].data) || !may_write(key)) return false;
    entry_t *e = find(key);
    if (!e) {
        if (s_count == sizeof(s_entries) / sizeof(s_entries[0])) return false;
        e = &s_entries[s_count++];
        strcpy(e->key, key);
    }
    e->type = type;
    e->len = len;
    memcpy(e->data, data, len);
    return true;
}

static const entry_t *get(const char *key, int type)
{
    const entry_t *e = find(key);
    return e && e->type == type ? e : NULL;
}

bool sota_kv_get_u8(const char *key, uint8_t *out)
{
    const entry_t *e = get(key, T_U8);
    if (!e) return false;
    memcpy(out, e->data, 1);
    return true;
}

bool sota_kv_get_u32(const char *key, uint32_t *out)
{
    const entry_t *e = get(key, T_U32);
    if (!e) return false;
    memcpy(out, e->data, 4);
    return true;
}

bool sota_kv_get_str(const char *key, char *out, size_t cap)
{
    const entry_t *e = get(key, T_STR);
    if (!e || e->len > cap) return false;
    memcpy(out, e->data, e->len);
    return true;
}

bool sota_kv_get_blob(const char *key, void *out, size_t len)
{
    const entry_t *e = get(key, T_BLOB);
    if (!e || e->len != len) return false;
    memcpy(out, e->data, len);
    return true;
}

bool sota_kv_set_u8(const char *key, uint8_t value) { return put(key, T_U8, &value, 1); }
bool sota_kv_set_u32(const char *key, uint32_t value) { return put(key, T_U32, &value, 4); }
bool sota_kv_set_str(const char *key, const char *value)
{
    return put(key, T_STR, value, strlen(value) + 1);
}
bool sota_kv_set_blob(const char *key, const void *value, size_t len)
{
    return put(key, T_BLOB, value, len);
}

bool sota_kv_erase(const char *key)
{
    entry_t *e = find(key);
    if (!may_write(key)) return false;
    if (e) {
        *e = s_entries[--s_count];
        memset(&s_entries[s_count], 0, sizeof(entry_t));
    }
    return true;
}
