# rzh Makefile
# Scott Bronson
# This file is MIT licensed (public domain, but removes author liability).

VERSION=0.5

CSRC=bgio.c echo.c fifo.c io/io_select.c log.c rzh.c
# -lrt

# ------------------------ #

# OBJ=$(CSRC:.c=.o)

COPTS+=-DVERSION=$(VERSION)
COPTS+=-Wall -Werror
COPTS+=-g

all: rzh doc

rzh: $(CSRC)
	$(CC) $(COPTS) $(CSRC) -lutil -o rzh

#%.o: %.c
#	$(CC) $(COPTS) -c $<

%.c: %.re
	re2c $(REOPTS) $< > $@
	perl -pi -e 's/^\#line.*$$//' $@

# autogenerate deps
# See http://www.gnu.org/software/make/manual/html_chapter/make_4.html#SEC51
#%.dep: %.c
#	@set -e; rm -f $@; \
#		$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
#		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#		rm -f $@.$$$$

-include $(CSRC:.c=.dep)

doc: rzh.1

%.1: %.pod
	pod2man -c "" -r "" -s 1 $< > $@

clean:
	rm -f rzh rzh.1
#	rm -f $(CSRC:.c=.o)
#	rm -f $(CSRC:.c=.dep)
#	rm -f $(CSRC:.c=.dep.*)
	@(cd test; $(MAKE) clean)

dist-clean: clean

test: rzh
	@(cd test; $(MAKE) test)

.PHONY: test
