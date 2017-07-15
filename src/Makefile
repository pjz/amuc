# Makefile for Amuc - the A'dam Music Composer
# Using environment variable DBG, which might be: "-g -Wuninitialized -Wall -Wno-non-virtual-dtor"
include ../Makefile.inc

CC=g++
A2PS_DIR=../src-abcm2ps
W2S_DIR=../src-wav2score

OBJS=amuc.o x-widgets.o sound.o str.o midi-out.o ps-out.o physical-mod.o bitmaps.o \
     read-waves.o dump-wav.o mono-synth.o snd-interface.o midi-in.o chords.o \
     midi-keyb.o midi-keyb-jack.o

.SUFFIXES=

amuc: $(OBJS)
	$(CC) $(OBJS) $(A2PS_DIR)/abc2ps.a -o amuc $(LDFLAGS)

%.o: %.cpp
	$(CC) -c -O $(DBG) $(CFLAGS) -Wno-multichar $<

amuc-headers.h: str.h x-widgets.h dump-wav.h colors.h midi-out.h templates.h ps-out.h \
                amuc.h read-waves.h physical-mod.h midi-keyb.h mono-synth.h sound.h \
                midi-in.h chords.h
	touch amuc-headers.h

amuc.o sound.o mono-synth.o read-waves.o physical-mod.o ps-out.o midi-in.o chords.o: amuc-headers.h
amuc.o: bitmaps.h snd-interface.h
sound.o: snd-interface.h
x-widgets.o: x-widgets.h
midi-out.o: colors.h midi-out.h
midi-in.o: colors.h
snd-interface.o: snd-interface.h
dump-wav.o: dump-wav.h
midi-keyb.o midi-keyb-jack.o: midi-keyb.h
str.o: str.h
bitmaps.o: bitmaps.h x-widgets.h
