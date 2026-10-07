# Host tests for the hardware-free APRS code (run on the build machine, not the radio).
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined
CORE = $(addprefix ../../App/app/, aprs_ax25.c aprs_beacon.c aprs_digi.c aprs_lastheard.c aprs_msg.c aprs_parse.c aprs_text.c aprs_settings.c)
SRC = ../../App/app/aprs_settings.c

test: test_aprs_settings test_aprs_bands test_aprs_menu_text test_aprs_core test_aprs_build
	./test_aprs_settings
	./test_aprs_bands
	./test_aprs_menu_text
	./test_aprs_core
	./test_aprs_build

test_aprs_settings: test_aprs_settings.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_bands: test_aprs_bands.c ../../App/app/aprs_bands.c
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_menu_text: test_aprs_menu_text.c ../../App/app/aprs_menu_text.c ../../App/app/aprs_settings.c
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_core: test_aprs_core.c $(CORE)
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_build: test_aprs_build.c $(CORE)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf *.dSYM
	rm -f test_aprs_settings test_aprs_bands test_aprs_menu_text test_aprs_core test_aprs_build
.PHONY: test clean

# Line coverage of the hardware-free APRS files: make -C tools/aprs coverage
# (clang source-based coverage; needs xcrun llvm-profdata / llvm-cov from the Xcode tools)
ALLSRC = $(addprefix ../../App/app/, aprs_ax25.c aprs_beacon.c aprs_digi.c aprs_lastheard.c aprs_msg.c aprs_parse.c aprs_text.c aprs_settings.c aprs_bands.c aprs_menu_text.c)
TESTS = settings bands menu_text core build
coverage:
	rm -rf cov && mkdir cov
	for t in $(TESTS); do \
	  $(CC) -std=c11 -I../../App -fprofile-instr-generate -fcoverage-mapping -O0 -o cov/t_$$t test_aprs_$$t.c $(ALLSRC) || exit 1; \
	  LLVM_PROFILE_FILE=cov/$$t.profraw ./cov/t_$$t >/dev/null || exit 1; \
	done
	xcrun llvm-profdata merge -o cov/all.profdata cov/*.profraw
	xcrun llvm-cov report ./cov/t_core $(addprefix -object ./cov/t_, settings bands menu_text build) \
	  -instr-profile=cov/all.profdata $(ALLSRC)
.PHONY: coverage
