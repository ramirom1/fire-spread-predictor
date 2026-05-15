CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2
SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2)

TARGET = creator

all: $(TARGET)

$(TARGET): creator.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(SDL_FLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
