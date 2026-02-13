CXX = g++
CXXFLAGS = -std=c++11 -Wall -Isrc
SRC_DIR = src
TEST_DIR = tests
TARGET = referee
RANDOM_ENGINE = random_engine
MCTS_01 = mcts_engine_01
MCTS_03 = mcts_engine_03
MCTS_04 = mcts_engine_04
REPLAY = replay
TEST_TARGET = test_referee

OBJ = referee_main.o game_logic.o
RANDOM_OBJ = random_engine.o game_logic.o
MCTS_01_OBJ = mcts_engine_01.o game_logic.o
MCTS_03_OBJ = mcts_engine_03.o game_logic.o
MCTS_04_OBJ = mcts_engine_04.o game_logic.o
REPLAY_OBJ = replay_main.o game_logic.o
HEADER = game_logic.h

all: $(TARGET) $(RANDOM_ENGINE) $(MCTS_01) $(MCTS_03) $(MCTS_04) $(REPLAY)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

$(REPLAY): $(REPLAY_OBJ)
	$(CXX) $(CXXFLAGS) -o $(REPLAY) $(REPLAY_OBJ)

$(RANDOM_ENGINE): $(RANDOM_OBJ)
	$(CXX) $(CXXFLAGS) -o $(RANDOM_ENGINE) $(RANDOM_OBJ)

$(MCTS_01): $(MCTS_01_OBJ)
	$(CXX) $(CXXFLAGS) -o $(MCTS_01) $(MCTS_01_OBJ)

$(MCTS_03): $(MCTS_03_OBJ)
	$(CXX) $(CXXFLAGS) -o $(MCTS_03) $(MCTS_03_OBJ)

$(MCTS_04): $(MCTS_04_OBJ)
	$(CXX) $(CXXFLAGS) -o $(MCTS_04) $(MCTS_04_OBJ)

# Compilation rule for object files in src/
%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/game_logic.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile and run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_DIR)/tests.cpp game_logic.o
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(TEST_DIR)/tests.cpp game_logic.o

clean:
	rm -f $(TARGET) $(RANDOM_ENGINE) $(MCTS_01) $(MCTS_03) $(MCTS_04) $(REPLAY) $(TEST_TARGET) *.o

.PHONY: all clean test