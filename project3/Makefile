CCPP=g++
LIB=-lpthread -lrt

all: addem life

addem:
	$(CCPP) -o addem addem.cpp $(LIB)

life:
	$(CCPP) life.cpp -o life -lpthread -lrt

clean:
	rm -f *.o *.txt addem life
