CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
SRC = ../../src/c

test: test_mandel
	./test_mandel

test_mandel: test_mandel.c $(SRC)/mandel.c $(SRC)/dive.c $(SRC)/grid.c $(SRC)/face.c $(SRC)/*.h $(SRC)/generated/*.h
	$(CC) $(CFLAGS) -o $@ test_mandel.c $(SRC)/mandel.c $(SRC)/dive.c $(SRC)/grid.c $(SRC)/face.c -lm

clean:
	rm -rf test_mandel test_mandel.dSYM librender.so

.PHONY: test clean
