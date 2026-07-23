CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -O1 -g
SANITIZER ?=
ROOT := ..
INCLUDES := -I$(ROOT)/src -I$(ROOT)/src/menu
COMMON_FLAGS := $(CXXFLAGS) $(SANITIZER) $(INCLUDES) -pthread

.PHONY: all run clean

all: benchmark_test audio_worker_test transition_test

benchmark_test: benchmark_test.cpp $(ROOT)/src/benchmark.cpp $(ROOT)/src/benchmark.h
	$(CXX) $(COMMON_FLAGS) benchmark_test.cpp $(ROOT)/src/benchmark.cpp -o $@

audio_worker_test: audio_worker_test.cpp $(ROOT)/src/audio.cpp $(ROOT)/src/benchmark.cpp
	$(CXX) $(COMMON_FLAGS) audio_worker_test.cpp $(ROOT)/src/audio.cpp \
		$(ROOT)/src/benchmark.cpp $(ROOT)/src/logger.cpp -o $@

transition_test: transition_test.cpp $(ROOT)/src/savestate.cpp $(ROOT)/src/disk_control.cpp
	$(CXX) $(COMMON_FLAGS) transition_test.cpp $(ROOT)/src/savestate.cpp \
		$(ROOT)/src/disk_control.cpp $(ROOT)/src/logger.cpp -o $@

run: all
	./benchmark_test
	./audio_worker_test
	./transition_test

clean:
	rm -f benchmark_test audio_worker_test transition_test benchmark-result-1.json benchmark-result-2.json transition-test.state
