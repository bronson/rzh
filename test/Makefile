# randfile Makefile
# Scott Bronson
# 4 Nov 2004

randfile: randfile.c mt19937ar.c mt19937ar.h Makefile
	$(CC) -g -Wall -Werror randfile.c mt19937ar.c -o randfile

clean:
	rm -f randfile

test: randfile
	./tmtest

.PHONY: test
