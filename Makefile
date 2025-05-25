TARGET = bank
CXX = g++
CXXFLAGS = -pthread -std=c++11 -g -Wall -Werror -pedantic-errors -DNDEBUG

SRCS = main.cpp account.cpp bank.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)