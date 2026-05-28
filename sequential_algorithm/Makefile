CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2
SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2)

TARGET = sequential_fire
SOURCES = main.cpp sequential_fire.cpp animated_view.cpp ../matrix_creator/environment.cpp
HEADERS = sequential_fire.h animated_view.h ../matrix_creator/environment.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(SDL_FLAGS)

run: $(TARGET)
ifdef WIND
ifdef SEED
	./$(TARGET) --wind $(WIND) --seed $(SEED)
else
	./$(TARGET) --wind $(WIND)
endif
else
ifdef SEED
	./$(TARGET) --seed $(SEED)
else
	./$(TARGET)
endif
endif

clean:
	rm -f $(TARGET)

.PHONY: all run clean
