CXX = g++
CXXFLAGS = -std=c++11 -Wall
TARGET = referee
OBJ = referee_main.o game_logic.o
HEADER = game_logic.h

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean