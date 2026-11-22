CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
SRC = ../../src/c

test: test_mandel
	./test_mandel

test_mandel: test_mandel.c $(SRC)/mandel.c $(SRC)/dive.c $(SRC)/grid.c $(SRC)/*.h $(SRC)/generated/targets.h
	$(CC) $(CFLAGS) -o $@ test_mandel.c $(SRC)/mandel.c $(SRC)/dive.c $(SRC)/grid.c -lm

clean:
	rm -rf test_mandel test_mandel.dSYM librender.so

.PHONY: test clean
