CC = g++
OBJECTS=abcparse.o buffer.o deco.o draw.o format.o music.o parse.o subs.o syms.o
# Using environment variable DBG, which might be: "-g -Wuninitialized -Wall -Wno-non-virtual-dtor"

all: abc2ps.a abcm2ps

abc2ps.a: abcm2ps.o $(OBJECTS)
	ar rs abc2ps.a abcm2ps.o $(OBJECTS)

abcm2ps: main.o $(OBJECTS)
	$(CC) -o abcm2ps main.o $(OBJECTS)

.SUFFIXES=

%.o: %.cpp
	$(CC) -c -O $(DBG) -Wno-char-subscripts -Wno-sign-compare $<

main.o: abcm2ps.cpp
	$(CC) -c -DREAD_FILE -o main.o $<

$(OBJECTS) main.o abcm2ps.o: abcparse.h abc2ps.h
