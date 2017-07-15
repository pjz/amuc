#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "midi-keyb.h"

typedef unsigned char uchar;

static const int BUFFER_SIZE=1024;

enum parserState {
  stIgnore,     dtaIgnore,
  stNoteOff,    dtaNoteOff1,    dtaNoteOff2,
  stNoteOn,     dtaNoteOn1,     dtaNoteOn2,
  stControlChg, dtaControlChg1, dtaControlChg2,
  stProgramChg, dtaProgramChg
};

struct statusTableRow {
  int state;
} statusTable[16];

bool midi_mes=false;
uchar inBuffer[BUFFER_SIZE];
int midi_fd;
const char *midi_input_dev="/dev/midi1";

static void loadStatusTable () {
  for (int i=0; i < 16; i++)
    statusTable[i].state = stIgnore;
  statusTable[0x80/0x10].state = stNoteOff;
  statusTable[0x90/0x10].state = stNoteOn;
  statusTable[0xb0/0x10].state = stControlChg;
  statusTable[0xc0/0x10].state = stProgramChg;
}

void run_keyboard() {
  int inBytes,
      state = stIgnore;
  uchar byte1=0,byte2=0,
        *mbp,
        *mbpEnd;
  while (mk_connected) {
    if ((inBytes = read(midi_fd,(void *)inBuffer,BUFFER_SIZE)) < 0) {
      alert("read midi_fd failed");
      break;
    }
    if (inBytes == 0) continue;
    mbp    = inBuffer;
    mbpEnd = mbp + inBytes;

    while (mbp < mbpEnd && mk_connected) {
      uchar B = * mbp++; /* Get next MIDI byte from buffer */
      if (0xF8 & B == 0xF8)
	continue;		/* Ignore */
      if (B & 0x80) {
	byte1 = B;
	state = statusTable[B/0x10].state;
      }
      if (midi_mes) printf("\tB=0x%x state=%d\n",B,state);
      switch (state) {
        case stNoteOff:
          state = dtaNoteOff1;
          break;
        case dtaNoteOff1:
          byte2 = B;		/* Note number */
          state = dtaNoteOff2;
          break;
        case dtaNoteOff2:
	  if (midi_mes) printf("\t  off: %d\n",byte2); /* B is release velocity */
          if (mk_connected) keyb_noteOff(byte2);
          state = dtaNoteOff1;
          break;
        case stNoteOn:
          state = dtaNoteOn1;
          break;
        case dtaNoteOn1:
          byte2 = B;		/* Note number */
          state = dtaNoteOn2;
          break;
        case dtaNoteOn2:
	  if (B>0) {		/* B is note on velocity */
	    if (midi_mes) printf("\ton: %d %d\n",byte2,B);
            if (mk_connected) keyb_noteOn(byte2);
	  } else {
	    if (midi_mes) printf("\toff: %d %d\n",byte2,B); /* Zero velocity on = off */
            if (mk_connected) keyb_noteOff(byte2);
	  }
          state = dtaNoteOn1;
          break;
      case stControlChg:
          state = dtaControlChg1;
          break;
      case dtaControlChg1:
          byte2 = B;		/* Controller number */
          state = dtaControlChg2;
          break;
      case dtaControlChg2:
          if (midi_mes) printf("\tctrl data=%d\n",B);	/* B is controller data */
          state = dtaControlChg1;
          break;
      case stProgramChg:
          state = dtaProgramChg;
          break;
      case dtaProgramChg:
          if (midi_mes) printf("\nprog change, prog=%d\n",B); /* B is program number */
          state = dtaProgramChg;
          break;
      case stIgnore:
          state = dtaIgnore;
          break;
      case dtaIgnore:
          break;
      default:
          printf("midi-keyb: unexpected state=0x%x\n",state);
      }
    }
  }
}

void *conn_mk_alsa(void*) {
  mk_connected=false;
  if ((midi_fd = open (midi_input_dev, O_RDONLY, 0)) == -1) {
    alert("open %s failed",midi_input_dev);
    alert("If midi keyboard connected correctly,");
    alert("   then check 'midi_input' in .amucrc file");
  }
  else {
    loadStatusTable();
    mk_connected=true;
    run_keyboard();
    close(midi_fd);
  }
  mk_connected=false;
  stop_conn_mk();
  return 0;
}
