# coomer — build file for Nix derivations (and plain make on any distro).
# For development, xmake is the primary build system.
#
# Requires:
#   clang / g++ with C17+C++17, pkg-config, wayland-scanner
#   stb headers available in CPATH (set by Nix derivation or manually)

CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -O2
CXXFLAGS ?= -std=c++17 -O2
PREFIX   ?= /usr

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

# Sources
GLAD_OBJ := _build/glad_gl.o

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

CXX_OBJS := $(patsubst src/%.cpp, _build/%.o, $(CXX_SRCS))

# Wayland protocol generated files
PROTO_SRCS := $(patsubst protocols/%.xml, generated/%-protocol.c, $(wildcard protocols/*.xml))
PROTO_OBJS := $(patsubst generated/%.c, _build/proto_%.o, $(PROTO_SRCS))

TARGET := _build/coomer

.PHONY: all install clean

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
$(GLAD_OBJ): third_party/glad/gl.c | _build
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

# ── Compile Wayland protocol stubs (C) ───────────────────────────────────────
# The generated/%-protocol.c rule also emits the companion .h, so depending on
# the .c alone is sufficient.
_build/proto_%.o: generated/%.c | _build
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

# ── Compile C++ sources ───────────────────────────────────────────────────────
_build/%.o: src/%.cpp | _build
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_CXXFLAGS) -c -o $@ $<

_build:
	mkdir -p _build

install: all
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/coomer

clean:
	rm -rf _build generated
