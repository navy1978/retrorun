# Minimal pinned MiniZip reader; zlib is already a frontend dependency.
RR_ROOT ?= ../..
MINIZIP_DIR := $(RR_ROOT)/deps/minizip
MINIZIP_OBJECTS := $(OBJDIR)/minizip/unzip.o $(OBJDIR)/minizip/ioapi.o
OBJECTS += $(MINIZIP_OBJECTS)

$(OBJDIR)/minizip/%.o: $(MINIZIP_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -D_POSIX_C_SOURCE=200809L -DNOUNCRYPT \
	  -O2 -MMD -MP -c $< -o $@
