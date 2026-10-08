# Host tests for the repeater build's hardware-free code:  make -C tools/repeater test
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined

test: test_rpt_info
	./test_rpt_info

test_rpt_info: test_rpt_info.c ../../App/app/rpt_info.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf test_rpt_info *.dSYM
.PHONY: test clean
