# rzh Makefile
# Scott Bronson
# This file is MIT licensed (public domain, but removes author liability).

VERSION=0.7

CSRC=bgio.c echo.c fifo.c log.c idle.c master.c pipe.c rzcmd.c \
	rztask.c task.c util.c zfin.c zrq.c
CHDR:=$(CSRC:.c=.h)
CSRC+=rzh.c io/io_select.c 

COPTS+=-DVERSION=$(VERSION)
COPTS+=-Wall -Werror
COPTS+=-g

all: rzh doc

rzh: $(CSRC) $(CHDR)
	$(CC) $(COPTS) $(CSRC) -lutil -lrt -o rzh

doc: rzh.1

%.1: %.pod
	pod2man -c "" -r "" -s 1 $< > $@

clean:
	rm -f rzh rzh.1
	@(cd test; $(MAKE) clean)
	rm -f tags

dist-clean: clean

test: rzh
	@(cd test; $(MAKE) test)

tags: $(CSRC) $(CHDR)
	ctags -R

install: all
	cp rzh /usr/local/bin
	mkdir -p /usr/local/man/man1
	cp rzh.1 /usr/local/man/man1

.PHONY: test
