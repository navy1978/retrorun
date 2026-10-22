CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -O1 -g
SANITIZER ?=
ROOT := ..
INCLUDES := -I$(ROOT)/src -I$(ROOT)/src/menu -I$(ROOT)/src/audio \
	-I$(ROOT)/src/config -I$(ROOT)/src/core -I$(ROOT)/src/input \
	-I$(ROOT)/src/video -I$(ROOT)/src/services \
	-I$(ROOT)/src/diagnostics -I$(ROOT)/src/platform
COMMON_FLAGS := $(CXXFLAGS) $(SANITIZER) $(INCLUDES) -pthread

.PHONY: all run clean

all: state_preview_test archive_content_test benchmark_test audio_worker_test transition_test config_file_test flycast_game_catalog_test core_option_resolution_test video_multithread_mode_test

state_preview_test: state_preview_test.cpp $(ROOT)/src/core/state_file.h $(ROOT)/src/video/screenshot_capture.h
	$(CXX) $(COMMON_FLAGS) state_preview_test.cpp -o $@

benchmark_test: benchmark_test.cpp $(ROOT)/src/diagnostics/benchmark.cpp $(ROOT)/src/diagnostics/benchmark.h
	$(CXX) $(COMMON_FLAGS) benchmark_test.cpp $(ROOT)/src/diagnostics/benchmark.cpp -o $@

audio_worker_test: audio_worker_test.cpp $(ROOT)/src/audio/audio.cpp $(ROOT)/src/diagnostics/benchmark.cpp
	$(CXX) $(COMMON_FLAGS) audio_worker_test.cpp $(ROOT)/src/audio/audio.cpp \
		$(ROOT)/src/diagnostics/benchmark.cpp $(ROOT)/src/diagnostics/logger.cpp -o $@

transition_test: transition_test.cpp $(ROOT)/src/core/savestate.cpp $(ROOT)/src/core/disk_control.cpp
	$(CXX) $(COMMON_FLAGS) transition_test.cpp $(ROOT)/src/core/savestate.cpp \
		$(ROOT)/src/core/disk_control.cpp $(ROOT)/src/diagnostics/logger.cpp -o $@

config_file_test: config_file_test.cpp $(ROOT)/src/config/config_file.cpp $(ROOT)/src/config/config_file.h
	$(CXX) $(COMMON_FLAGS) config_file_test.cpp $(ROOT)/src/config/config_file.cpp -o $@

flycast_game_catalog_test: flycast_game_catalog_test.cpp \
		$(ROOT)/src/config/flycast_game_catalog.cpp \
		$(ROOT)/src/config/flycast_game_catalog.h \
		$(ROOT)/src/config/config_file.cpp $(ROOT)/src/config/config_file.h
	$(CXX) $(COMMON_FLAGS) flycast_game_catalog_test.cpp \
		$(ROOT)/src/config/flycast_game_catalog.cpp \
		$(ROOT)/src/config/config_file.cpp -o $@

core_option_resolution_test: core_option_resolution_test.cpp \
		$(ROOT)/src/core/core_option_resolution.h
	$(CXX) $(COMMON_FLAGS) core_option_resolution_test.cpp -o $@

video_multithread_mode_test: video_multithread_mode_test.cpp \
		$(ROOT)/src/video/video_multithread_mode.h
	$(CXX) $(COMMON_FLAGS) video_multithread_mode_test.cpp -o $@

run: all
	./state_preview_test
	python3 archive_content_test.py ./archive_content_test
	./benchmark_test
	./audio_worker_test
	./transition_test
	./config_file_test
	./flycast_game_catalog_test
	./core_option_resolution_test
	./video_multithread_mode_test

clean:
	rm -f state_preview_test archive_content_test archive_unzip.o archive_ioapi.o benchmark_test audio_worker_test transition_test config_file_test flycast_game_catalog_test core_option_resolution_test video_multithread_mode_test benchmark-result-1.json benchmark-result-2.json transition-test.state

archive_unzip.o: $(ROOT)/deps/minizip/unzip.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER) -std=c99 -D_POSIX_C_SOURCE=200809L -DNOUNCRYPT -c $< -o $@

archive_ioapi.o: $(ROOT)/deps/minizip/ioapi.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER) -std=c99 -D_POSIX_C_SOURCE=200809L -c $< -o $@

archive_content_test: archive_content_test.cpp $(ROOT)/src/core/archive_content.cpp $(ROOT)/src/core/archive_content.h archive_unzip.o archive_ioapi.o
	$(CXX) $(COMMON_FLAGS) archive_content_test.cpp $(ROOT)/src/core/archive_content.cpp archive_unzip.o archive_ioapi.o -lz -o $@
