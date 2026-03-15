# FataMorgana Unit Tests Makefile

CXX ?= g++

# Paths
SRC_PATH = .
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin
GTEST_PATH = ../../../libs/libDaisy/tests/googletest

# Executables
WAV_TEST = wav_parser_test
DSP_TEST = dsp_test

# Sources
WAV_SOURCES = wav_parser_test.cpp
DSP_SOURCES = dsp_test.cpp

# Objects
WAV_OBJECTS = $(WAV_SOURCES:%.cpp=$(BUILD_PATH)/%.o)
DSP_OBJECTS = $(DSP_SOURCES:%.cpp=$(BUILD_PATH)/%.o)

# Flags
CXXFLAGS = -std=c++14 -Wall -Wextra -g -pthread -DUNIT_TEST=1
INCLUDES = -I $(GTEST_PATH)/googletest/include \
           -I $(GTEST_PATH)/googletest \
           -I ..

LIBS = -pthread

# Google Test sources (compile gtest-all.cc)
GTEST_SRCS = $(GTEST_PATH)/googletest/src/gtest-all.cc
GTEST_OBJS = $(BUILD_PATH)/gtest-all.o

.PHONY: all clean test test-wav test-dsp dirs

all: dirs $(BIN_PATH)/$(WAV_TEST) $(BIN_PATH)/$(DSP_TEST)

dirs:
	@mkdir -p $(BUILD_PATH)
	@mkdir -p $(BIN_PATH)

clean:
	rm -rf $(BUILD_PATH)

test: all
	@echo "Running WAV parser tests..."
	./$(BIN_PATH)/$(WAV_TEST)
	@echo ""
	@echo "Running DSP tests..."
	./$(BIN_PATH)/$(DSP_TEST)

test-wav: dirs $(BIN_PATH)/$(WAV_TEST)
	./$(BIN_PATH)/$(WAV_TEST)

test-dsp: dirs $(BIN_PATH)/$(DSP_TEST)
	./$(BIN_PATH)/$(DSP_TEST)

# Link WAV test
$(BIN_PATH)/$(WAV_TEST): $(WAV_OBJECTS) $(GTEST_OBJS)
	@echo "Linking: $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

# Link DSP test
$(BIN_PATH)/$(DSP_TEST): $(DSP_OBJECTS) $(GTEST_OBJS)
	@echo "Linking: $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

# Compile test sources
$(BUILD_PATH)/%.o: $(SRC_PATH)/%.cpp
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile gtest
$(BUILD_PATH)/gtest-all.o: $(GTEST_SRCS)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) -I $(GTEST_PATH)/googletest/include -I $(GTEST_PATH)/googletest -c $< -o $@
