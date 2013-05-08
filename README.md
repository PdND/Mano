Mano
====

A tiny mano machine compiler and simulator

Compiling
-------------

```
cd mano
make
```
or

```
g++ mano.cpp -o mano -lboost_regex -lcurses
```

Usage
-------------

Running

```
$ mano source_file.asm
```

executes the source file and outputs the memory map before and after the execution

```
0000: 2006
0001: 7200
0002: 7020
0003: 1007
0004: 3008
0005: 7001
0006: 0002
0007: 0006
0008: 0000

Emulation done
0000: 2006
0001: 7200
0002: 7020
0003: 1007
0004: 3008
0005: 7001
0006: 0002
0007: 0006
0008: 0004
```

Dependencies
-------------

+ boost (http://www.boost.org)
+ (n)curses
