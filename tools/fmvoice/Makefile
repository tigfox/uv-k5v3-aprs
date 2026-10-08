# Host tests for the FM Voice build's hardware-free code:  make -C tools/fmvoice test
CFLAGS = -std=c11 -Wall -Wextra -Werror -I../../App -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined

test: test_fmv_info test_fmv_bank test_fmv_tone fmv_vectors
	./test_fmv_info
	./test_fmv_bank
	./test_fmv_tone
	python3 -m pytest -q test_fmv_codec.py

test_fmv_info: test_fmv_info.c ../../App/app/fmv_info.c
	$(CC) $(CFLAGS) -o $@ $^

fmv_vectors: test_fmv_codec_vectors.c ../../App/app/fmv_info.c
	$(CC) $(CFLAGS) -o $@ $^

test_fmv_bank: test_fmv_bank.c ../../App/app/fmv_bank.c
	$(CC) $(CFLAGS) -o $@ $^

test_fmv_tone: test_fmv_tone.c ../../App/app/fmv_tone.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf test_fmv_tone test_fmv_bank test_fmv_info fmv_vectors *.dSYM
.PHONY: test clean
