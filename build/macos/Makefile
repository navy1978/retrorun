CXX ?= clang++
ROOT := ../..
OBJDIR := obj
TARGET := $(ROOT)/retrorun

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
PNG_CFLAGS := $(shell pkg-config --cflags libpng)
PNG_LIBS := $(shell pkg-config --libs libpng)

CXXFLAGS := -std=c++20 -Wall -Wextra -g -DRR_PLATFORM_SDL \
            -I$(ROOT)/src -I$(ROOT)/src/menu $(SDL_CFLAGS) $(PNG_CFLAGS)
LDFLAGS := $(SDL_LIBS) $(PNG_LIBS) -framework OpenGL

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
	rm -rf $(OBJDIR) $(TARGET)

-include $(OBJECTS:.o=.d)
