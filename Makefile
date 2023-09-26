CPL=g++
FLAGS=-Wall -g 
BINARY=icsh

all: icsh

icsh: icsh.cpp
	$(CPL) -o $(BINARY) $(FLAGS) $<

.PHONY: clean

clean:
	rm -f $(BINARY)
