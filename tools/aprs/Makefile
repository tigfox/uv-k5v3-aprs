# Host tests for the hardware-free APRS code (run on the build machine, not the radio).
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g
SRC = ../../App/app/aprs_settings.c

test: test_aprs_settings test_aprs_bands test_aprs_menu_text
	./test_aprs_settings
	./test_aprs_bands
	./test_aprs_menu_text

test_aprs_settings: test_aprs_settings.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_bands: test_aprs_bands.c ../../App/app/aprs_bands.c
	$(CC) $(CFLAGS) -o $@ $^

test_aprs_menu_text: test_aprs_menu_text.c ../../App/app/aprs_menu_text.c ../../App/app/aprs_settings.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf *.dSYM
	rm -f test_aprs_settings test_aprs_bands test_aprs_menu_text
.PHONY: test clean
