CC ?= cc
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_XOPEN_SOURCE_EXTENDED
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += -lncursesw
PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man

.PHONY: all install test clean
all: gui
gui: src/gui.c src/file_picker.h src/workspace.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
install: gui docs/gui.1
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 gui $(DESTDIR)$(PREFIX)/bin/gui
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 docs/gui.1 $(DESTDIR)$(MANDIR)/man1/gui.1
tests/core-test: tests/core.c src/gui.c src/file_picker.h src/workspace.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -o $@ $< $(LDFLAGS) $(LDLIBS)
tests/dialog-test: tests/dialogs.c src/gui.c src/file_picker.h src/workspace.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -o $@ $< $(LDFLAGS) $(LDLIBS)

test: tests/core-test tests/dialog-test
	./tests/core-test
	./tests/dialog-test
clean:
	rm -f gui tests/core-test tests/dialog-test
