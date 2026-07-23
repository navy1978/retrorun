CXX ?= g++
ROOT := ../..
OBJDIR := obj/$(config)
TARGET := $(ROOT)/retrorun-sdl2

SDL_CONFIG ?= sdl2-config
PKG_CONFIG ?= pkg-config
SDL_CFLAGS ?= $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS ?= $(shell $(SDL_CONFIG) --libs)
PNG_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libpng)
PNG_LIBS ?= $(shell $(PKG_CONFIG) --libs libpng)
GLES_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags egl glesv2 2>/dev/null)
GLES_LIBS ?= $(shell $(PKG_CONFIG) --libs egl glesv2 2>/dev/null)
ifeq ($(strip $(GLES_LIBS)),)
  GLES_LIBS := -lEGL -lGLESv2
endif

# GCC 9 (the default compiler on Ubuntu 20.04) exposes C++20 as c++2a.
# Newer GCC and Clang versions continue to accept this spelling.
COMMON_FLAGS := -std=c++2a -Wall -Wextra -DRR_PLATFORM_SDL -DRR_SDL_GLES -DRC_CLIENT_SUPPORTS_HASH \
                -I$(ROOT)/src -I$(ROOT)/src/menu \
                -I$(ROOT)/deps/rcheevos/include -I$(ROOT)/deps/rcheevos/src \
                $(SDL_CFLAGS) $(PNG_CFLAGS) $(GLES_CFLAGS)
ifeq ($(config),release)
  CXXFLAGS := $(COMMON_FLAGS) -O3 -DNDEBUG -flto
  LDFLAGS_EXTRA := -s -flto
else
  CXXFLAGS := $(COMMON_FLAGS) -g -DDEBUG
endif
LDFLAGS := $(SDL_LIBS) $(PNG_LIBS) $(GLES_LIBS) -lcurl -lz -ldl -pthread $(LDFLAGS_EXTRA)

SOURCES := \
  src/logger.cpp \
  src/platform_sdl.cpp \
  src/system-info.cpp \
  src/ui-renderer.cpp \
  src/menu/menu_item.cpp \
  src/menu/menu.cpp \
  src/menu/menu_manager.cpp \
  src/fonts.cpp \
  src/input.cpp \
  src/benchmark.cpp \
  src/audio.cpp \
  src/video-helper.cpp \
  src/decoration.cpp \
  src/decoration_catalog.cpp \
  src/video.cpp \
  src/globals.cpp \
  src/rumble.cpp \
  src/events_sdl.cpp \
  src/config.cpp \
  src/core_loader.cpp \
  src/savestate.cpp \
  src/menu_setup.cpp \
  src/keyboard.cpp \
  src/disk_control.cpp \
  src/file_browser.cpp \
  src/achievements.cpp \
  src/network_status.cpp \
  src/main.cpp

RCHEEVOS_SOURCES := deps/rcheevos/src/rc_client.c deps/rcheevos/src/rc_compat.c \
  deps/rcheevos/src/rc_libretro.c deps/rcheevos/src/rc_util.c deps/rcheevos/src/rc_version.c \
  $(patsubst $(ROOT)/%,%,$(wildcard $(ROOT)/deps/rcheevos/src/rcheevos/*.c)) \
  $(patsubst $(ROOT)/%,%,$(wildcard $(ROOT)/deps/rcheevos/src/rhash/*.c)) \
  $(patsubst $(ROOT)/%,%,$(wildcard $(ROOT)/deps/rcheevos/src/rapi/*.c))
OBJECTS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SOURCES)) \
           $(patsubst deps/rcheevos/src/%.c,$(OBJDIR)/rcheevos/%.o,$(RCHEEVOS_SOURCES))

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/rcheevos/%.o: $(ROOT)/deps/rcheevos/src/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -include strings.h \
		-Werror=implicit-function-declaration \
		-DRC_CLIENT_SUPPORTS_HASH -I$(ROOT)/deps/rcheevos/include \
	      -I$(ROOT)/deps/rcheevos/src -I$(ROOT)/src -MMD -MP -c $< -o $@

clean:
	rm -rf obj $(TARGET)

-include $(OBJECTS:.o=.d)
