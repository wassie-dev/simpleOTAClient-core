// Stub adapter functions. They live outside the .ino, whose prototype
// generation does not handle an extern "C" block and gives setup() and
// loop() C linkage.

#include <sota_app.h>
#include <sota_net.h>

extern "C" {
int sota_net_post(void*, const char*, const char*, const char*, char*, size_t) { return -1; }
int sota_net_dl_open(void*, const char*, int64_t*, char*, size_t) { return 0; }
int sota_net_dl_read(void*, uint8_t*, size_t) { return SOTA_NET_ERROR; }
void sota_net_dl_close(void*) {}
void sota_app_event(void*, const sota_event_t*) {}
void sota_app_wake(void*) {}
void sota_app_log(int, const char*, ...) {}
}
