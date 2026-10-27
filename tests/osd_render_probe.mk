# Use after a normal SDL build, from build/macos or build/linux-sdl:
# make -f Makefile -f ../../tests/osd_render_probe.mk config=debug osd-probe
# OSD_PROBE_DIR points to an existing temporary output directory.
OSD_PROBE_DIR ?= /tmp/retrorun-osd-probe
OSD_PROBE_FLAGS = $(CPPFLAGS) $(COMMON_FLAGS) $(CXXFLAGS)
OSD_PROBE_LINK = $(LDFLAGS) $(RR_LIBS) $(LDLIBS)
.PHONY: osd-probe
osd-probe: $(OBJECTS)
	@mkdir -p $(OSD_PROBE_DIR)
	$(CXX) $(OSD_PROBE_FLAGS) -Dmain=retrorun_application_main -c $(ROOT)/src/main.cpp -o $(OSD_PROBE_DIR)/main.o
	$(CXX) $(OSD_PROBE_FLAGS) -DRR_OSD_SDL_PROBE -c $(ROOT)/tests/osd_render_probe.cpp -o $(OSD_PROBE_DIR)/probe.o
	$(CXX) $(OSD_PROBE_DIR)/probe.o $(OSD_PROBE_DIR)/main.o $(filter-out $(OBJDIR)/main.o $(OBJDIR)/platform/platform_sdl.o,$(OBJECTS)) $(OSD_PROBE_LINK) -o $(OSD_PROBE_DIR)/probe
