CC=g++
CPPFLAGS=-lboost_regex-mt -lcurses

all:	main

main:	
	$(CC) $(CPPFLAGS) mano.cpp -o mano

clean:
	rm -rf *.o *.a mano
