# CC=gcc
CFLAGS=-g -Wall -O2 -pipe -DNDEBUG
# CFLAGS=-g -Wall -O2 -pipe -march=pentium3 -mmmx -msse -m32 -DNDEBUG
DESTDIR ?= /
PREFIX  ?= /usr/

objs := main.o setup.o buffer.o options.o input_file.o input_tcp.o            \
	input_tcp_connection.o input_udp.o input_unix.o input_tools.o         \
	input_buffer.o output.o output_file.o output_tcp.o output_udp.o       \
	output_unix.o output_tools.o net_tools.o reader.o logger.o log.o
srcs := $(patsubst %.o,%.c,$(objs))
deps := $(patsubst %.c,%.d,$(srcs))

ifndef debug
	echo := @
endif

### For production builds
# CFLAGS=-g -Wall -DNDEBUG

all: genbuf

%.d: %.c
	$(echo) echo "Depends: $<"; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.c
	$(echo) echo "Compiling: $<"; \
	$(CC) $(CFLAGS) -c $< -o $@;

genbuf: $(objs)
	$(echo) echo "Linking: $<"; \
	$(CC) $(CFLAGS) -lpthread -o $@ $(objs);

clean:
	$(echo) echo "Cleaning up..."; \
	rm -f genbuf *.o *~;

realclean: clean
	$(echo) echo "Making really clean..."; \
	rm -f *.d core*

distclean: realclean

install: genbuf
	$(echo) echo "Installing genbuf..."; \
	install genbuf -m 0755 $(DESTDIR)/$(PREFIX)/bin

-include $(deps)
