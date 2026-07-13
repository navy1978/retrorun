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

COMMON_FLAGS := -std=c++20 -Wall -Wextra -DRR_PLATFORM_SDL -DRR_SDL_GLES \
                -I$(ROOT)/src -I$(ROOT)/src/menu \
                $(SDL_CFLAGS) $(PNG_CFLAGS) $(GLES_CFLAGS)
ifeq ($(config),release)
  CXXFLAGS := $(COMMON_FLAGS) -O3 -DNDEBUG
  LDFLAGS_EXTRA := -s
else
  CXXFLAGS := $(COMMON_FLAGS) -g -DDEBUG
endif
LDFLAGS := $(SDL_LIBS) $(PNG_LIBS) $(GLES_LIBS) -ldl -pthread $(LDFLAGS_EXTRA)

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
  src/audio.cpp \
  src/video-helper.cpp \
  src/video.cpp \
  src/globals.cpp \
  src/rumble.cpp \
  src/events_sdl.cpp \
  src/main.cpp

OBJECTS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf obj $(TARGET)

-include $(OBJECTS:.o=.d)
