CC       ?= cc
CFLAGS   ?= -O2 -Wall -Wextra
CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?= -lm

BUILD_DIR    ?= ../build
LIB_BUILD_DIR = $(BUILD_DIR)/lib
LIB_SRCS      = bigNumLibThingy.c smartFFTThingy.c
LIB_OBJS      = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(LIB_SRCS))
LIB_VERSION   = 0.1.0
LIB_SONAME    = libbignums.so.0
LIB_REAL      = libbignums.so.$(LIB_VERSION)
SHARED_LIB    = $(LIB_BUILD_DIR)/$(LIB_REAL)

# Optional legacy driver: make MAIN=demo.c TARGET=demo
MAIN   ?=
TARGET ?= test_big

PREFIX        ?= /usr/local
LIBDIR        ?= $(PREFIX)/lib
INCLUDEDIR    ?= $(PREFIX)/include
MANDIR        ?= $(PREFIX)/share/man
DESTDIR       ?=
GROFF         ?= groff
INSTALL       ?= install
MAN_SRC_DIR   := ../man
MAN_BUILD_DIR ?= $(BUILD_DIR)/man
MAN_SOURCES   := $(wildcard $(MAN_SRC_DIR)/man3/*.3 $(MAN_SRC_DIR)/man7/*.7)
MAN_TEXTS     := $(patsubst $(MAN_SRC_DIR)/%,$(MAN_BUILD_DIR)/%.txt,$(MAN_SOURCES))

.PHONY: all shared demo test check clean man install install-library install-man link-help

all: shared
ifneq ($(strip $(MAIN)),)
all: $(TARGET)

$(TARGET): $(MAIN) $(SHARED_LIB) bignums.h complexFFT.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o "$@" "$(MAIN)" \
		-L"$(LIB_BUILD_DIR)" -Wl,-rpath,"$(abspath $(LIB_BUILD_DIR))" -lbignums $(LDLIBS)
endif

shared: $(SHARED_LIB)

$(SHARED_LIB): $(LIB_OBJS)
	mkdir -p "$(LIB_BUILD_DIR)"
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB_SONAME) -o "$@" $^ $(LDLIBS)
	ln -sfn "$(LIB_REAL)" "$(LIB_BUILD_DIR)/$(LIB_SONAME)"
	ln -sfn "$(LIB_SONAME)" "$(LIB_BUILD_DIR)/libbignums.so"

$(BUILD_DIR)/obj/%.o: %.c bignums.h complexFFT.h
	mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c "$<" -o "$@"

$(BUILD_DIR)/demo: demo.c $(SHARED_LIB) bignums.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o "$@" "$<" \
		-L"$(LIB_BUILD_DIR)" -Wl,-rpath,'$$ORIGIN/lib' -lbignums $(LDLIBS)

$(BUILD_DIR)/test_big: test.c $(SHARED_LIB) bignums.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o "$@" "$<" \
		-L"$(LIB_BUILD_DIR)" -Wl,-rpath,'$$ORIGIN/lib' -lbignums $(LDLIBS)

demo: $(BUILD_DIR)/demo

test check: $(BUILD_DIR)/test_big
	"$(BUILD_DIR)/test_big"

install: install-library install-man

install-library: shared
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(INCLUDEDIR)"
	$(INSTALL) -m 755 "$(SHARED_LIB)" "$(DESTDIR)$(LIBDIR)/$(LIB_REAL)"
	ln -sfn "$(LIB_REAL)" "$(DESTDIR)$(LIBDIR)/$(LIB_SONAME)"
	ln -sfn "$(LIB_SONAME)" "$(DESTDIR)$(LIBDIR)/libbignums.so"
	$(INSTALL) -m 644 bignums.h complexFFT.h "$(DESTDIR)$(INCLUDEDIR)/"

man: $(MAN_TEXTS)

# Source aliases are relative symlinks; installed pages are self-contained.
$(MAN_BUILD_DIR)/%.txt: $(MAN_SRC_DIR)/% $(MAN_SOURCES)
	mkdir -p "$(@D)"
	cd "$(MAN_SRC_DIR)" && $(GROFF) -ww -Tutf8 -man "$*" > "$(abspath $@)"

install-man:
	$(INSTALL) -d "$(DESTDIR)$(MANDIR)/man3" "$(DESTDIR)$(MANDIR)/man7"
	$(INSTALL) -m 644 $(MAN_SRC_DIR)/man3/*.3 "$(DESTDIR)$(MANDIR)/man3/"
	$(INSTALL) -m 644 $(MAN_SRC_DIR)/man7/*.7 "$(DESTDIR)$(MANDIR)/man7/"

link-help:
	@printf '\nLink an application (paths refer to the final prefix, not DESTDIR):\n'
	@printf '%s\n' 'cc app.c -I"$(INCLUDEDIR)" -L"$(LIBDIR)" -Wl,-rpath,"$(LIBDIR)" -lbignums -o app'
	@printf '%s\n' 'man -M "$(MANDIR)" 7 bignums'

clean:
	rm -f $(LIB_OBJS) "$(SHARED_LIB)" "$(LIB_BUILD_DIR)/$(LIB_SONAME)" \
		"$(LIB_BUILD_DIR)/libbignums.so" "$(BUILD_DIR)/demo" "$(BUILD_DIR)/test_big" $(MAN_TEXTS)
