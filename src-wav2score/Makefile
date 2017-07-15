include ../Makefile.inc
CC=g++
ADIR=../src
OBJS=$(ADIR)/x-widgets.o $(ADIR)/snd-interface.o $(ADIR)/str.o wav2score.o fft.o
OPT=-O -Wuninitialized -Wno-multichar

.SUFFIXES=

wav2score: $(OBJS)
	$(CC) $(OBJS) -o wav2score $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -I../src $(OPT) $(CFLAGS) $<

$(ADIR)/x-widgets.o: $(ADIR)/x-widgets.cpp $(ADIR)/x-widgets.h
	@cd $(ADIR); make x-widgets.o

$(ADIR)/alsa-interface.o: $(ADIR)/alsa-interface.cpp $(ADIR)/snd-interface.h
	@cd $(ADIR); make alsa-interface.o

wav2score.o: fft.h $(ADIR)/x-widgets.h $(ADIR)/snd-interface.h
fft.o: fft.h
