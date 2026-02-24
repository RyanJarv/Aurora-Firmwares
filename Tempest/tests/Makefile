# Distortion Bank Unit Tests Makefile

CXX ?= g++

# Paths
SRC_PATH = .
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin
GTEST_PATH = ../../../libs/libDaisy/tests/googletest

# Executable
BIN_NAME = distortion_test

# Sources
SOURCES = distortion_bank_test.cpp

# Objects
OBJECTS = $(SOURCES:%.cpp=$(BUILD_PATH)/%.o)

# Flags
CXXFLAGS = -std=c++14 -Wall -Wextra -g -pthread -DUNIT_TEST=1
INCLUDES = -I $(GTEST_PATH)/googletest/include \
           -I $(GTEST_PATH)/googletest \
           -I .. \
           -I ../../../libs/DaisySP/Source

LIBS = -pthread

# Google Test sources (compile gtest-all.cc)
GTEST_SRCS = $(GTEST_PATH)/googletest/src/gtest-all.cc
GTEST_OBJS = $(BUILD_PATH)/gtest-all.o

.PHONY: all clean test dirs

all: dirs $(BIN_PATH)/$(BIN_NAME)

dirs:
	@mkdir -p $(BUILD_PATH)
	@mkdir -p $(BIN_PATH)

clean:
	rm -rf $(BUILD_PATH)
	rm -f $(BIN_NAME)

test: all
	./$(BIN_PATH)/$(BIN_NAME)

# Link
$(BIN_PATH)/$(BIN_NAME): $(OBJECTS) $(GTEST_OBJS)
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
