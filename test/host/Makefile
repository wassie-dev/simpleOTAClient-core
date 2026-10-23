# Host-side unit tests. No ESP-IDF or Arduino toolchain required.
# Run from the repo root:  make -C test/host test
#
# The tests build with -Werror as C99, and test_cxx builds the headers as
# C++11, the way the Arduino adapter includes them.

CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -std=c99 -Wall -Wextra -Werror
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Werror
CORE     := ../../core
INC      := -I$(CORE)

OBJS := monocypher.o monocypher-ed25519.o sota_json.o sota_sha256.o sota_signing.o
TESTS := test_signing test_report test_trial test_wdt test_url test_state test_cxx

test: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done

test_signing: test_signing.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ test_signing.c $(OBJS)

test_report: test_report.c $(CORE)/sota_report.h $(CORE)/sota_time.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_report.c

test_trial: test_trial.c $(CORE)/sota_trial.h $(CORE)/sota_report.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_trial.c

test_wdt: test_wdt.c $(CORE)/sota_wdt_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_wdt.c

test_url: test_url.c $(CORE)/sota_url.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_url.c

test_state: test_state.c fake_kv.c fake_kv.h $(CORE)/sota_state.c $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) -o $@ test_state.c fake_kv.c $(CORE)/sota_state.c

test_cxx: test_cxx.cpp $(OBJS) $(wildcard $(CORE)/*.h)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ test_cxx.cpp $(OBJS)

%.o: $(CORE)/%.c $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f $(TESTS) $(OBJS)

.PHONY: test clean
