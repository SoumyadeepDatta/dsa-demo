# Linux / macOS / MinGW build.
#   make        -> builds ./sharding_demo
#   make run    -> builds and starts it on port 8080

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
SOURCES = $(wildcard src/*.cpp)

ifeq ($(OS),Windows_NT)
    TARGET = sharding_demo.exe
    LIBS = -lws2_32
else
    TARGET = sharding_demo
    LIBS =
endif

$(TARGET): $(SOURCES) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f sharding_demo sharding_demo.exe
