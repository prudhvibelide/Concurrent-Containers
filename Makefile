# Author: Prudhvi Raj Belide
# Makefile for concurrent containers project

CXX = g++
CXXFLAGS = -std=c++17 -pthread -O2 -Wall
TARGET = test_containers

# Source files
SOURCES = sgl_stack.cpp sgl_queue.cpp treiber_stack.cpp msqueue.cpp \
          elimination_stack.cpp fc_stack.cpp fc_queue.cpp \
          condvar.cpp main.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.cpp containers.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests
test: $(TARGET)
	./$(TARGET)

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets
.PHONY: all test clean
