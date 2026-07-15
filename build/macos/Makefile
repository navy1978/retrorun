CXX ?= clang++
ROOT := ../..
OBJDIR := obj/$(config)
TARGET := $(ROOT)/retrorun

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
PNG_CFLAGS := $(shell pkg-config --cflags libpng)
PNG_LIBS := $(shell pkg-config --libs libpng)

COMMON_FLAGS := -std=c++20 -Wall -Wextra -DRR_PLATFORM_SDL \
                -I$(ROOT)/src -I$(ROOT)/src/menu $(SDL_CFLAGS) $(PNG_CFLAGS)
ifeq ($(config),release)
  CXXFLAGS := $(COMMON_FLAGS) -O3 -DNDEBUG -flto
  LDFLAGS_EXTRA := -flto
else
  CXXFLAGS := $(COMMON_FLAGS) -g -DDEBUG
endif
LDFLAGS := $(SDL_LIBS) $(PNG_LIBS) -framework OpenGL $(LDFLAGS_EXTRA)

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
  src/config.cpp \
  src/core_loader.cpp \
  src/savestate.cpp \
  src/menu_setup.cpp \
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
