# rzh Makefile
# Scott Bronson
# This file is BSD licensed.

# pdpzm files
CSRC=zmcore.c zmfr.c error.c pdcomm.c


# ------------------------ #

OBJ=$(CSRC:.c=.o)

COPTS+=-isystem .
COPTS+=-Wall -Werror
COPTS+=-g

rzh: $(OBJ) rzh.o
	$(CC) $(OBJ) rzh.o -o rzh

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

clean:
	rm -f rzh rzh.o
	rm -f $(CSRC:.c=.o)
	rm -f $(CSRC:.c=.dep)
	rm -f $(CSRC:.c=.dep.*)
