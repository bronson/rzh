# rzh Makefile
# Scott Bronson
# This file is MIT licensed (public domain, but removes author liability).

VERSION=0.2

# pdpzm files
CSRC=zmcore.c zmfrunix.c zmextm.c error.c pdcomm.c fifo.c bgio.c scan.c zio.c rzh.c


# ------------------------ #

OBJ=$(CSRC:.c=.o)

COPTS+=-isystem .
COPTS+=-Wall -Werror
COPTS+=-g
COPTS+=-DVERSION=$(VERSION)

all: rzh doc
	@(cd test; $(MAKE))

rzh: $(OBJ)
	$(CC) $(OBJ) -lrt -lutil -o rzh

%.o: %.c
	$(CC) $(COPTS) -c $<

# autogenerate deps
# See http://www.gnu.org/software/make/manual/html_chapter/make_4.html#SEC51
%.dep: %.c
	@set -e; rm -f $@; \
		$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(CSRC:.c=.dep)

doc: rzh.1

%.1: %.pod
	pod2man -c "" -r "" -s 1 $< > $@

clean:
	rm -f rzh rzh.1
	rm -f $(CSRC:.c=.o)
	rm -f $(CSRC:.c=.dep)
	rm -f $(CSRC:.c=.dep.*)
	@(cd test; $(MAKE) clean)

dist-clean: clean

test: rzh
	@(cd test; $(MAKE) test)

.PHONY: test
