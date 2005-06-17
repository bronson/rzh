# rzh Makefile
# Scott Bronson
# This file is MIT licensed (public domain, but removes author liability).

VERSION=0.5

CSRC=bgio.c echo.c fifo.c log.c master.c pipe.c rztask.c task.c util.c zscan.c
CHDR:=$(CSRC:.c=.h)
CSRC+=rzh.c io/io_select.c 

COPTS+=-DVERSION=$(VERSION)
COPTS+=-Wall -Werror
COPTS+=-g

all: rzh doc

rzh: $(CSRC) $(CHDR)
	$(CC) $(COPTS) $(CSRC) -lutil -o rzh

doc: rzh.1

%.1: %.pod
	pod2man -c "" -r "" -s 1 $< > $@

clean:
	rm -f rzh rzh.1
	@(cd test; $(MAKE) clean)

dist-clean: clean

test: rzh
	@(cd test; $(MAKE) test)

.PHONY: test
