# rzh Makefile
# Scott Bronson
# This file is MIT licensed (public domain, but removes author liability).

# "make PRODUCTION=1" to optimize and strip binary.


VERSION=0.8

CSRC=bgio.c cmd.c fifo.c idle.c log.c pipe.c task.c util.c zfin.c zrq.c
CSRC+=consoletask.c echotask.c rztask.c
CSRC+=io/io_socket.c
CHDR:=$(CSRC:.c=.h)

CSRC+=io/io_select.c 
CHDR+=io/io.h

CSRC+=rzh.c 


COPTS+=-DVERSION=$(VERSION)

ifeq ("$(PRODUCTION)","1")
COPTS+=-O2 -DNDEBUG
else
COPTS+=-Wall -Werror -g
endif

LIBS=-lutil
ifneq ($(shell uname), Darwin)
LIBS+=-lrt
endif



all: rzh doc

rzh: $(CSRC) $(CHDR)
	$(CC) $(COPTS) $(CSRC) $(LIBS) -o rzh
ifeq ("$(PRODUCTION)","1")
	strip rzh
endif

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
	mkdir -p /usr/local/bin
	cp rzh /usr/local/bin
	mkdir -p /usr/local/man/man1
	cp rzh.1 /usr/local/man/man1

uninstall:
	rm -f /usr/local/bin/rzh
	rm -f /usr/local/man/man1/rzh.1

.PHONY: test
