# Host tests for the hardware-free APRS code (run on the build machine, not the radio).
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g
SRC = ../../App/app/aprs_settings.c

test: test_aprs_settings
	./test_aprs_settings

test_aprs_settings: test_aprs_settings.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_aprs_settings
.PHONY: test clean
