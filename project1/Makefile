all: doit

doit: doit_processes.h
	g++ -g -c doit_processes.cpp
	g++ -g -c doit.cpp
	g++ -g doit.o doit_processes.o -o doit

clean:
	rm -f *.o doit
