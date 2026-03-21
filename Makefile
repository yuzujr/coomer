# coomer — build file for Nix derivations (and plain make on any distro).
# For development, xmake is the primary build system.
#
# Requires:
#   clang / g++ with C17+C++17, pkg-config, wayland-scanner

CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -O2
CXXFLAGS ?= -std=c++17 -O2
PREFIX   ?= /usr
VERSION  := $(strip $(file < VERSION))
BASH_COMPLETIONDIR ?= $(PREFIX)/share/bash-completion/completions
ZSH_COMPLETIONDIR  ?= $(PREFIX)/share/zsh/site-functions
FISH_COMPLETIONDIR ?= $(PREFIX)/share/fish/vendor_completions.d

# All features on by default; pass e.g. WAYLAND=0 to disable
X11     ?= 1
WAYLAND ?= 1
PORTAL  ?= 1

# pkg-config dependencies
PKG_DEPS :=
ifeq ($(X11),1)
  PKG_DEPS += x11 xrandr
endif
ifeq ($(WAYLAND),1)
  PKG_DEPS += wayland-client wayland-egl xkbcommon
endif
PKG_DEPS += gl egl
ifeq ($(PORTAL),1)
  PKG_DEPS += dbus-1
endif

PKG_CFLAGS := $(shell pkg-config --cflags $(PKG_DEPS))
PKG_LIBS   := $(shell pkg-config --libs   $(PKG_DEPS)) -ldl -lpthread -lm

# Defines
DEFINES :=
DEFINES += -DCOOMER_VERSION=\"$(VERSION)\"
ifeq ($(X11),1)
  DEFINES += -DCOOMER_HAS_X11
endif
ifeq ($(WAYLAND),1)
  DEFINES += -DCOOMER_HAS_WAYLAND
endif
ifeq ($(PORTAL),1)
  DEFINES += -DCOOMER_HAS_PORTAL
endif

COMMON_FLAGS := -Isrc -Igenerated -Ithird_party $(PKG_CFLAGS) $(DEFINES)
ALL_CFLAGS   := $(CFLAGS)   $(COMMON_FLAGS)
ALL_CXXFLAGS := $(CXXFLAGS) $(COMMON_FLAGS)
BUILD_DIR    := build/make

# Sources
GLAD_OBJ := $(BUILD_DIR)/glad_gl.o

CXX_SRCS := src/app/main.cpp \
             src/app/cli.cpp \
             src/render/RendererGL.cpp \
             src/capture/BackendAuto.cpp

ifeq ($(X11),1)
  CXX_SRCS += src/capture/BackendX11.cpp \
               src/window/X11WindowGlx.cpp
endif
ifeq ($(WAYLAND),1)
  CXX_SRCS += src/capture/BackendWlrScreencopy.cpp \
               src/window/WaylandWindowXdgEgl.cpp \
               src/window/WaylandWindowLayerShellEgl.cpp
endif
ifeq ($(PORTAL),1)
  CXX_SRCS += src/capture/BackendPortalScreenshot.cpp
endif

CXX_OBJS := $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(CXX_SRCS))

# Wayland protocol generated files
PROTO_SRCS := $(patsubst protocols/%.xml, generated/%-protocol.c, $(wildcard protocols/*.xml))
PROTO_OBJS := $(patsubst generated/%.c, $(BUILD_DIR)/proto_%.o, $(PROTO_SRCS))
PROTO_HDRS := $(patsubst protocols/%.xml, generated/%-client-protocol.h, $(wildcard protocols/*.xml))

TARGET := $(BUILD_DIR)/coomer

.PHONY: all install clean
.SECONDARY: $(PROTO_SRCS) $(PROTO_HDRS)

all: $(TARGET)

# ── Wayland protocol code generation ─────────────────────────────────────────
# Generate both .c and .h from same .xml — the .c rule also emits the .h so
# that compiling the .c file can always find its companion header.
generated/%-protocol.c: protocols/%.xml | generated
	wayland-scanner private-code  $< $@
	wayland-scanner client-header $< $(@:%-protocol.c=%-client-protocol.h)

generated/%-client-protocol.h: protocols/%.xml | generated
	wayland-scanner client-header $< $@

generated:
	mkdir -p generated

# ── Link ──────────────────────────────────────────────────────────────────────
$(TARGET): $(GLAD_OBJ) $(PROTO_OBJS) $(CXX_OBJS)
	$(CXX) $(ALL_CXXFLAGS) -o $@ $^ $(PKG_LIBS)

# ── Compile glad (C) ──────────────────────────────────────────────────────────
$(GLAD_OBJ): third_party/glad/gl.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

# ── Compile Wayland protocol stubs (C) ───────────────────────────────────────
# The generated/%-protocol.c rule also emits the companion .h, so depending on
# the .c alone is sufficient.
$(BUILD_DIR)/proto_%.o: generated/%.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

# ── Compile C++ sources ───────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: src/%.cpp $(PROTO_HDRS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: all
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/coomer
	install -Dm644 completions/bash/coomer $(DESTDIR)$(BASH_COMPLETIONDIR)/coomer
	install -Dm644 completions/zsh/_coomer $(DESTDIR)$(ZSH_COMPLETIONDIR)/_coomer
	install -Dm644 completions/fish/coomer.fish $(DESTDIR)$(FISH_COMPLETIONDIR)/coomer.fish

clean:
	rm -rf $(BUILD_DIR) generated
