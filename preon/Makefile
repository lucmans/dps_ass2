CXX=g++
CXXFLAGS=-Wall -Wextra -g -c
LDFLAGS=-lpthread

DEPS=$(wildcard *.h)
OBJS=$(patsubst %.cc, %.o, $(wildcard *.cc))
EXEC=preon

CORES = 20

.PHONY: all clean
all:
	make -j $(CORES) $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) -o $(EXEC) $(OBJS) $(LDFLAGS)

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $<

clean:
	rm -rf *.o
	rm -rf $(EXEC)
