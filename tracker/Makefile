CXX=g++
CXXFLAGS=-Wall -Wextra -O2 -c
LDFLAGS=

DEPS=$(wildcard *.h)
OBJS=$(patsubst %.cc, %.o, $(wildcard *.cc))
EXEC=tracker

.PHONY: all
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) -o $(EXEC) $(OBJS) $(LDFLAGS)

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $<

clean:
	rm -rf *.o
	rm -rf $(EXEC)
