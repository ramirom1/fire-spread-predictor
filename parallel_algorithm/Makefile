CXX       = mpic++
CXXFLAGS  = -std=c++17 -Wall -O2
SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2)

TARGET  = mpi_fire
SOURCES = main.cpp mpi_fire.cpp ../matrix_creator/environment.cpp
HEADERS = mpi_fire.h ../matrix_creator/environment.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(SDL_FLAGS)

run: $(TARGET)
	mpirun -np 10 ./$(TARGET)

run-headless: $(TARGET)
	mpirun -np 10 ./$(TARGET) --no-window

cluster:
	$(CXX) $(CXXFLAGS) -DNO_SDL -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all run run-headless clean cluster
