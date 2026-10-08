# Host tests for the repeater build's hardware-free code:  make -C tools/repeater test
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined

test: test_rpt_info rpt_vectors
	./test_rpt_info
	python3 -m pytest -q test_rpt_codec.py

test_rpt_info: test_rpt_info.c ../../App/app/rpt_info.c
	$(CC) $(CFLAGS) -o $@ $^

rpt_vectors: test_rpt_codec_vectors.c ../../App/app/rpt_info.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf test_rpt_info rpt_vectors *.dSYM
.PHONY: test clean
