# Pinned libchdr, built into the frontend: no new runtime package is required.
# Included by GO2, Linux SDL and macOS builds.
RR_ROOT ?= ../..
CHDR_DIR := $(RR_ROOT)/deps/libchdr
CHDR_SOURCES := $(wildcard $(CHDR_DIR)/src/*.c) \
  $(CHDR_DIR)/deps/lzma-25.01/src/LzmaDec.c \
  $(CHDR_DIR)/deps/zstd-1.5.7/zstddeclib.c
CHDR_OBJECTS := $(patsubst $(CHDR_DIR)/%.c,$(OBJDIR)/libchdr/%.o,$(CHDR_SOURCES))
OBJECTS += $(CHDR_OBJECTS)
CXXFLAGS += -I$(CHDR_DIR)/include

$(OBJDIR)/libchdr/%.o: $(CHDR_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -std=c99 -D_POSIX_C_SOURCE=200809L -DCHDR_SYSTEM_ZLIB \
	  -fvisibility=hidden -O2 -I$(CHDR_DIR)/include -MMD -MP -c $< -o $@
