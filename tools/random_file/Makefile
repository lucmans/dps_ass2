
CXX=g++
CXXFLAGS=-std=c++11 -s  #-fsanitize=address
WARNINGS=-Wall -Wextra -Wfloat-equal
OPTIMIZATION=-O3

BIN=random_file
CORES=20


.PHONY: all clean


all:
	make -j $(CORES) $(BIN)


$(BIN): main.o
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(OPTIMIZATION) -o $@ $^

main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(OPTIMIZATION) -o $@ -c $<


clean:
	-rm *.o
	-rm $(BIN)
