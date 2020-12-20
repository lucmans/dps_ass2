
CXX=g++
CXXFLAGS=-std=c++11 -s  #-fsanitize=address
WARNINGS=-Wall -Wextra -Wfloat-equal
OPTIMIZATION=-O3

BIN=fractal
CORES=20


.PHONY: all clean


all:
	make -j $(CORES) $(BIN)


$(BIN): main.o lodepng.o
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(OPTIMIZATION) -o $@ $^

lodepng.o: lodepng.cpp lodepng.h
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(OPTIMIZATION) -o $@ -c $<

main.o: main.cpp lodepng.h lodepng.cpp
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(OPTIMIZATION) -o $@ -c $<


clean:
	-rm *.o
	-rm $(BIN)
