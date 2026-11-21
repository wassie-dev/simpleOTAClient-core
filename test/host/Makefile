# Host-side unit tests. No ESP-IDF or Arduino toolchain required.
# Run from the repo root:  make -C test/host test
#
# The tests build with -Werror as C99, and test_cxx builds the headers as
# C++11, so they stay usable from C++ code.

CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -std=c99 -Wall -Wextra -Werror
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Werror
CORE     := ../../core
# sota_hash.h with no dependencies, and the Ed25519 signing the tests use.
PORTABLE := ../../port/portable
INC      := -I$(CORE) -I$(PORTABLE)

OBJS := monocypher.o monocypher-ed25519.o sota_json.o sota_hash_portable.o sota_signing.o sota_manifest.o sota_protocol.o
FRAMEWORK_TESTS := test_framework_idf test_framework_idf_nobundle \
                   test_framework_arduino test_framework_arduino_nobundle \
                   test_framework_arduino_2_0_4 test_framework_arduino_2_0_3 \
                   test_framework_arduino_unversioned
TESTS := test_signing test_report test_trial test_wdt test_url test_state test_contract test_protocol test_engine test_unsigned test_cxx \
         $(FRAMEWORK_TESTS) test_init_trust test_init_trust_unsigned test_image_ota

test: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done

test_signing: test_signing.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ test_signing.c $(OBJS)

test_report: test_report.c $(CORE)/sota_report.h $(CORE)/sota_time.h $(CORE)/sota_net.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_report.c

test_trial: test_trial.c $(CORE)/sota_trial.h $(CORE)/sota_report.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_trial.c

test_wdt: test_wdt.c $(CORE)/sota_wdt_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_wdt.c

test_url: test_url.c $(CORE)/sota_url.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_url.c

test_state: test_state.c fake_kv.c fake_kv.h $(CORE)/sota_state.c $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) -o $@ test_state.c fake_kv.c $(CORE)/sota_state.c

test_contract: test_contract.c fake_kv.c fake_kv.h $(CORE)/sota_state.c $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) -o $@ test_contract.c fake_kv.c $(CORE)/sota_state.c

test_protocol: test_protocol.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ test_protocol.c $(OBJS)

ENGINE := $(CORE)/sota_engine.c $(CORE)/sota_apply.c $(CORE)/sota_state.c

# hash_count.c wraps the hash functions to count the ones left open.
WRAP_HASH := -Wl,--wrap=sota_sha256_init,--wrap=sota_sha256_final,--wrap=sota_sha512_init,--wrap=sota_sha512_final
test_engine: test_engine.c fake_hal.c fake_hal.h fake_kv.c fake_kv.h fake_malloc.h hash_count.c $(ENGINE) $(OBJS) $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) $(WRAP_HASH) -include fake_malloc.h -o $@ test_engine.c fake_hal.c fake_kv.c hash_count.c $(ENGINE) $(OBJS)

# A build without signed updates: no Monocypher on the link line.
UNSIGNED := $(ENGINE) $(CORE)/sota_json.c $(PORTABLE)/sota_hash_portable.c $(CORE)/sota_signing.c $(CORE)/sota_manifest.c $(CORE)/sota_protocol.c
test_unsigned: test_unsigned.c fake_hal.c fake_hal.h fake_kv.c fake_kv.h $(UNSIGNED) $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) -DSIMPLEOTA_DISABLE_SIGNED_UPDATES $(INC) -o $@ test_unsigned.c fake_hal.c fake_kv.c $(UNSIGNED)

test_cxx: test_cxx.cpp $(OBJS) $(wildcard $(CORE)/*.h)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ test_cxx.cpp $(OBJS)

PORT := ../../port/esp32
# The test hashes the bundle's roots with the portable sota_hash.h, whose
# SHA-512 is Monocypher's.
FRAMEWORK_SRC := test_framework.c $(PORT)/sota_framework.c $(PORTABLE)/sota_hash_portable.c \
                 $(PORTABLE)/monocypher-ed25519.c $(CORE)/monocypher.c
FRAMEWORK_DEPS := $(FRAMEWORK_SRC) $(PORT)/sota_framework.h $(wildcard esp_stub/*.h) $(wildcard $(CORE)/*.h) \
                  $(wildcard $(PORTABLE)/*.h)
FRAMEWORK_INC := $(INC) -I$(PORT) -Iesp_stub
# The trust each build must default to: 0 none, 1 the bundle, 2 the PEM.
FRAMEWORK_CC = $(CC) $(CFLAGS) $(FRAMEWORK_INC) -DTEST_NAME='"$@"' -o $@ $(FRAMEWORK_SRC)
BUNDLE := -DCONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1

test_framework_idf: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) $(BUNDLE) -DEXPECT_TRUST=1

test_framework_idf_nobundle: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DEXPECT_TRUST=0

test_framework_arduino: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DARDUINO $(BUNDLE) -DEXPECT_TRUST=1

test_framework_arduino_nobundle: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DARDUINO -DEXPECT_TRUST=0

test_framework_arduino_2_0_4: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DARDUINO -DSTUB_ARDUINO_VERSION_PATCH=4 $(BUNDLE) -DEXPECT_TRUST=1

test_framework_arduino_2_0_3: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DARDUINO -DSTUB_ARDUINO_VERSION_PATCH=3 $(BUNDLE) -DEXPECT_TRUST=2

test_framework_arduino_unversioned: $(FRAMEWORK_DEPS)
	$(FRAMEWORK_CC) -DARDUINO -DSTUB_NO_ARDUINO_VERSION $(BUNDLE) -DEXPECT_TRUST=2

INIT_SRC := test_init_trust.c $(PORT)/simpleota.c

test_init_trust: $(INIT_SRC) ../../include/simpleota.h $(FRAMEWORK_DEPS)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L $(FRAMEWORK_INC) -I../../include -o $@ $(INIT_SRC)

test_init_trust_unsigned: $(INIT_SRC) ../../include/simpleota.h $(FRAMEWORK_DEPS)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DSIMPLEOTA_DISABLE_SIGNED_UPDATES $(FRAMEWORK_INC) \
	    -I../../include -o $@ $(INIT_SRC)

# The ESP32 port's image check, built for an ESP32-C6.
test_image_ota: test_image_ota.c $(PORT)/sota_image_ota.c $(CORE)/sota_image.h esp_stub/esp_ota_ops.h
	$(CC) $(CFLAGS) $(INC) -Iesp_stub -DCONFIG_IDF_FIRMWARE_CHIP_ID=0x000D -o $@ \
	    test_image_ota.c $(PORT)/sota_image_ota.c

%.o: $(CORE)/%.c $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

%.o: $(PORTABLE)/%.c $(wildcard $(CORE)/*.h) $(wildcard $(PORTABLE)/*.h)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f $(TESTS) $(OBJS)

.PHONY: test clean
