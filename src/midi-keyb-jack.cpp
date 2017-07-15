#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "midi-keyb.h"

typedef unsigned char uchar;

snd_midi_event_t *alsa_decoder;

void midi_action(snd_seq_t *seq_handle) {
  snd_seq_event_t *ev;
  static uchar buffer[16];
  int count,
      nbytes=0;
  do {
    snd_seq_event_input(seq_handle, &ev);
    count = snd_midi_event_decode(alsa_decoder, buffer, 16, ev);
    if (count > 0 && count < 16) {
      nbytes = count;
      count++;
    }
    snd_seq_free_event(ev);
  } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
  uchar code=buffer[0] & 0xf0;
  if (midi_mes) printf("nbytes=%d buffer=[%x,%x,%x], code=%x\n",nbytes,buffer[0],buffer[1],buffer[2],code);
  switch (code) {
    case 0x80:
      if (mk_connected)
        keyb_noteOff(buffer[1]);
      break;
    case 0x90:
      if (mk_connected) {
        if (buffer[2]>0) keyb_noteOn(buffer[1]);
        else keyb_noteOff(buffer[1]);
      }
      break;
    case 0xc0: {  // change instrument
        int instr=buffer[1]+1;
        if (midi_mes) printf("  instr: %d\n",instr);
      }
      break;
    case 0xb0:   // set volume or modulation
      if (midi_mes) {
        if (buffer[1]==0x07) printf("  volume: %d\n",buffer[2]);
        else printf("  modulation: %d\n",buffer[2]);
      }
      break;
    default:
      if (midi_mes) putchar('\n');
  }
}

void *conn_mk_jack(void*) {
  mk_connected=false;
  int portid;
  int npfd;
  struct pollfd *pfd;
  snd_seq_t *seq_handle;

  if (snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_INPUT, 0) < 0) {
      alert("Error opening ALSA sequencer");
      return 0;
  }
  snd_seq_set_client_name(seq_handle, "Amuc midi keyboard");
  if ((portid = snd_seq_create_simple_port(seq_handle, "midi_in",
          SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
          SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
    alert("Error creating sequencer port");
    return 0;
  }
  npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
  pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);

  if (snd_midi_event_new(16, &alsa_decoder)) {
     alert("Error initializing ALSA MIDI decoder");
     return 0;
  }
  snd_midi_event_reset_decode(alsa_decoder);
  snd_midi_event_no_status(alsa_decoder, 1);
  if (midi_mes) puts("start polling");
  mk_connected=true;
  while (mk_connected) {
    if (poll(pfd, npfd, 1000) > 0)
        midi_action(seq_handle);
  }
  snd_seq_close(seq_handle);
  stop_conn_mk();
  return 0;
}
