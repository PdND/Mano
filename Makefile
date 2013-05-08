CC=g++
LIBS=-lboost_regex -lcurses

all:	main

main:	
	$(CC) mano.cpp -o mano $(LIBS)

clean:
	rm -rf *.o *.a mano
