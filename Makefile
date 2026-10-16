CC ?= cc
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_XOPEN_SOURCE_EXTENDED
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += -lncursesw
PREFIX ?= /usr/local

.PHONY: all install test clean
all: gui
gui: src/gui.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
install: gui
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 gui $(DESTDIR)$(PREFIX)/bin/gui
tests/core-test: tests/core.c src/gui.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -o $@ $< $(LDFLAGS) $(LDLIBS)
test: tests/core-test
	./tests/core-test
clean:
	rm -f gui tests/core-test

