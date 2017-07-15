/*
  Several functions copied from midifile.c, which is
  from project 'abcmidi' by Seymour Shlien and others.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "amuc-headers.h"

const int perc_lo=27,
          perc_hi=87;

struct GM_instr {
  int gm_nr;
  const char *name;
  int col;
};

static GM_instr gm_instr[128]= {
{ 1, "Acoustic Grand Piano", 0 },
{ 2, "Bright Acoustic Piano", 0 },
{ 3, "Electric Grand Piano", 0 },
{ 4, "Honky-tonk Piano", 0 },
{ 5, "Electric Piano 1", 0 },
{ 6, "Electric Piano 2", 0 },
{ 7, "Harpsichord", 0 },
{ 8, "Clavi", 0 },
{ 9, "Celesta", 0 },
{ 10, "Glockenspiel", 0 },
{ 11, "Music Box", 0 },
{ 12, "Vibraphone", 0 },
{ 13, "Marimba", 0 },
{ 14, "Xylophone", 0 },
{ 15, "Tubular Bells", 0 },
{ 16, "Dulcimer", 0 },
{ 17, "Drawbar Organ", 0 },
{ 18, "Percussive Organ", 0 },
{ 19, "Rock Organ", 0 },
{ 20, "Church Organ", 0 },
{ 21, "Reed Organ", 0 },
{ 22, "Accordion", 0 },
{ 23, "Harmonica", 0 },
{ 24, "Tango Accordion", 0 },
{ 25, "Acoustic Guitar (nylon)", 0 },
{ 26, "Acoustic Guitar (steel)", 0 },
{ 27, "Electric Guitar (jazz)", 0 },
{ 28, "Electric Guitar (clean)", 0 },
{ 29, "Electric Guitar (muted)", 0 },
{ 30, "Overdriven Guitar", 0 },
{ 31, "Distortion Guitar", 0 },
{ 32, "Guitar harmonics", 0 },
{ 33, "Acoustic Bass", 0 },
{ 34, "Electric Bass (finger)", 0 },
{ 35, "Electric Bass (pick)", 0 },
{ 36, "Fretless Bass", 0 },
{ 37, "Slap Bass 1", 0 },
{ 38, "Slap Bass 2", 0 },
{ 39, "Synth Bass 1", 0 },
{ 40, "Synth Bass 2", 0 },
{ 41, "Violin", 0 },
{ 42, "Viola", 0 },
{ 43, "Cello", 0 },
{ 44, "Contrabass", 0 },
{ 45, "Tremolo Strings", 0 },
{ 46, "Pizzicato Strings", 0 },
{ 47, "Orchestral Harp", 0 },
{ 48, "Timpani", 0 },
{ 49, "String Ensemble 1", 0 },
{ 50, "String Ensemble 2", 0 },
{ 51, "SynthStrings 1", 0 },
{ 52, "SynthStrings 2", 0 },
{ 53, "Choir Aahs", 0 },
{ 54, "Voice Oohs", 0 },
{ 55, "Synth Voice", 0 },
{ 56, "Orchestra Hit", 0 },
{ 57, "Trumpet", 0 },
{ 58, "Trombone", 0 },
{ 59, "Tuba", 0 },
{ 60, "Muted Trumpet", 0 },
{ 61, "French Horn", 0 },
{ 62, "Brass Section", 0 },
{ 63, "SynthBrass 1", 0 },
{ 64, "SynthBrass 2", 0 },
{ 65, "Soprano Sax", 0 },
{ 66, "Alto Sax", 0 },
{ 67, "Tenor Sax", 0 },
{ 68, "Baritone Sax", 0 },
{ 69, "Oboe", 0 },
{ 70, "English Horn", 0 },
{ 71, "Bassoon", 0 },
{ 72, "Clarinet", 0 },
{ 73, "Piccolo", 0 },
{ 74, "Flute", 0 },
{ 75, "Recorder", 0 },
{ 76, "Pan Flute", 0 },
{ 77, "Blown Bottle", 0 },
{ 78, "Shakuhachi", 0 },
{ 79, "Whistle", 0 },
{ 80, "Ocarina", 0 },
{ 81, "Lead 1 (square)", 0 },
{ 82, "Lead 2 (sawtooth)", 0 },
{ 83, "Lead 3 (calliope)", 0 },
{ 84, "Lead 4 (chiff)", 0 },
{ 85, "Lead 5 (charang)", 0 },
{ 86, "Lead 6 (voice)", 0 },
{ 87, "Lead 7 (fifths)", 0 },
{ 88, "Lead 8 (bass + lead)", 0 },
{ 89, "Pad 1 (new age)", 0 },
{ 90, "Pad 2 (warm)", 0 },
{ 91, "Pad 3 (polysynth)", 0 },
{ 92, "Pad 4 (choir)", 0 },
{ 93, "Pad 5 (bowed)", 0 },
{ 94, "Pad 6 (metallic)", 0 },
{ 95, "Pad 7 (halo)", 0 },
{ 96, "Pad 8 (sweep)", 0 },
{ 97, "FX 1 (rain)", 0 },
{ 98, "FX 2 (soundtrack)", 0 },
{ 99, "FX 3 (crystal)", 0 },
{ 100, "FX 4 (atmosphere)", 0 },
{ 101, "FX 5 (brightness)", 0 },
{ 102, "FX 6 (goblins)", 0 },
{ 103, "FX 7 (echoes)", 0 },
{ 104, "FX 8 (sci-fi)", 0 },
{ 105, "Sitar", 0 },
{ 106, "Banjo", 0 },
{ 107, "Shamisen", 0 },
{ 108, "Koto", 0 },
{ 109, "Kalimba", 0 },
{ 110, "Bag pipe", 0 },
{ 111, "Fiddle", 0 },
{ 112, "Shanai", 0 },
{ 113, "Tinkle Bell", 0 },
{ 114, "Agogo", 0 },
{ 115, "Steel Drums", 0 },
{ 116, "Woodblock", 0 },
{ 117, "Taiko Drum", 0 },
{ 118, "Melodic Tom", 0 },
{ 119, "Synth Drum", 0 },
{ 120, "Reverse Cymbal", 0 },
{ 121, "Guitar Fret Noise", 0 },
{ 122, "Breath Noise", 0 },
{ 123, "Seashore", 0 },
{ 124, "Bird Tweet", 0 },
{ 125, "Telephone Ring", 0 },
{ 126, "Helicopter", 0 },
{ 127, "Applause", 0 },
{ 128, "Gunshot", 0 } };

struct GM_perc {
  int midi_nr;
  const char *name;
  int col;
};

GM_perc gm_perc[perc_hi-perc_lo+1]= {
    // upto 34 and from 82: from timidity patches, /etc/timidity.cfg
{ perc_lo /*27*/, "Highq", 2 },
{ 28, "Slap", 2 },
{ 29, "Scratch1", 3 },
{ 30, "Scratch2", 3 },
{ 31, "Sticks", 3 },
{ 32, "Sqrclick", 5 },
{ 33, "Metclick", 5 },
{ 34, "Metbell", 5 },
{ 35, "Acoustic Bass Drum", 0 },
{ 36, "Bass Drum 1", 0 },
{ 37, "Side Stick", 1 },
{ 38, "Acoustic Snare", 1 },
{ 39, "Hand Clap", 4 },
{ 40, "Electric Snare", 2 },
{ 41, "Low Floor Tom", 0 },
{ 42, "Closed Hi Hat", 3 },
{ 43, "High Floor Tom", 4 },
{ 44, "Pedal Hi-Hat", 5 },
{ 45, "Low Tom", 0 },
{ 46, "Open Hi-Hat", 3 },
{ 47, "Low-Mid Tom", 1 },
{ 48, "Hi-Mid Tom", 2 },
{ 49, "Crash Cymbal 1", 3 },
{ 50, "High Tom", 3 },
{ 51, "Ride Cymbal 1", 3 },
{ 52, "Chinese Cymbal", 2 },
{ 53, "Ride Bell", 5 },
{ 54, "Tambourine", 4 },
{ 55, "Splash Cymbal", 3 },
{ 56, "Cowbell", 3 },
{ 57, "Crash Cymbal 2", 3 },
{ 58, "Vibraslap", 3 },
{ 59, "Ride Cymbal 2", 2 },
{ 60, "Hi Bongo", 4 },
{ 61, "Low Bongo", 1 },
{ 62, "Mute Hi Conga", 4 },
{ 63, "Open Hi Conga", 4 },
{ 64, "Low Conga", 1 },
{ 65, "High Timbale", 5 },
{ 66, "Low Timbale", 1 },
{ 67, "High Agogo", 5 },
{ 68, "Low Agogo", 1 },
{ 69, "Cabasa", 2 },
{ 70, "Maracas", 2 },
{ 71, "Short Whistle", 5 },
{ 72, "Long Whistle", 4 },
{ 73, "Short Guiro", 4 },
{ 74, "Long Guiro", 3 },
{ 75, "Claves", 5 },
{ 76, "Hi Wood Block", 5 },
{ 77, "Low Wood Block", 1 },
{ 78, "Mute Cuica", 2 },
{ 79, "Open Cuica", 2 },
{ 80, "Mute Triangle", 4 },
{ 81, "Open Triangle", 4 },
{ 82, "Shaker",3 },
{ 83, "Jingles",4 },
{ 84, "Bell tree",2 },
{ 85, "Castinet",3 },
{ 86, "Surdo1",1 },
{ perc_hi /*87*/, "Surdo2",1 } };

struct ProgChange {
  int time,
      gm_nr,
      group_nr,
      transp,
      col_nr;
  char *tune;
  ProgChange(int t,int gmnr):time(t),gm_nr(gmnr),group_nr(0),transp(0),col_nr(0),tune(0) { }
  bool operator==(ProgChange &pc) { return time==pc.time; }
  bool operator<(ProgChange &pc) { return time<pc.time; }
};

struct MidiChannel {
  SLinkedList<ProgChange> pc_list;
  SLList_elem<ProgChange> *cur_pc;
  void reset() { pc_list.reset(); cur_pc=0; }
} channels[16];

static FILE *midi_f;

static bool tinc_is_one=true;  // t_incr = 1.0?
const int msgbuf_max=1000;
static int Mf_toberead,
           Mf_bytesread,
           Mf_currtime,
           Msgindex,
           ntrks,
           division=1,
           nu_pq=4;         // amuc note units per quarter
bool alert_mes;
static char Msgbuff[msgbuf_max];
static float t_incr=1.;

struct Midi_Note {
  uchar lnr,sign,
        occ;  // occupied?
  bool valid; // not out of range?
  int start_time;
  ProgChange *pch_data;
} note_arr[128][16];

struct MidiPercNote {
  uchar occ;   // occupied?
} perc_note_arr[perc_hi+1];

MidiIn midi_in;

static int divide(int a,int b) { return (2 * a + b)/b/2; }

int get_perc_instr(int midi_nr) {
  if (midi_nr<perc_lo || midi_nr>perc_hi) {
    alert_mes=true;
    return 5;
  }
  return gm_perc[midi_nr-perc_lo].col;
}

int egetc() {
  int c = getc(midi_f);

  if (c == EOF) { alert("unexpected end-of-file"); return c; }
  Mf_toberead--;
  Mf_bytesread++;
  return c;
}

int readvarinum()
{
  int value;
  int c;

  c = egetc();
  value = c;
  if ( c & 0x80 ) {
    value &= 0x7f;
    do {
      c = egetc();
      value = (value << 7) + (c & 0x7f);
    } while (c & 0x80);
  }
  return value;
}

int to32bit(int c1,int c2,int c3,int c4) {
  int value = 0;

  value = (c1 & 0xff);
  value = (value<<8) + (c2 & 0xff);
  value = (value<<8) + (c3 & 0xff);
  value = (value<<8) + (c4 & 0xff);
  return value;
}

int to16bit(int c1, int c2) {
  return ((c1 & 0xff ) << 8) + (c2 & 0xff);
}

int read32bit() {
  int c1, c2, c3, c4;

  c1 = egetc();
  c2 = egetc();
  c3 = egetc();
  c4 = egetc();
  return to32bit(c1,c2,c3,c4);
}

int read16bit() {
  int c1, c2;
  c1 = egetc();
  c2 = egetc();
  return to16bit(c1,c2);
}

MidiIn::MidiIn() {
}

struct MidiScores {
  static const int sc_max=40;
  Score *buf[sc_max];
  void reset() {
    for (int i=0;i<sc_max;++i) buf[i]=0;
  }
  Score *get_score(char *name) {
    for (int i=0;i<sc_max;++i) {
      if (!buf[i]) {
        buf[i]=new_score(name);
        buf[i]->signs_mode=midi_in.acc;
        buf[i]->tone2signs(midi_in.key_nr % keys_max);
        buf[i]->ngroup=-1;
        return buf[i];
      }
      if (!strcmp(ind2tname(buf[i]->name),name)) return buf[i];
    }
    alert ("get_score: more then %d scores",sc_max);
    return 0;
  }
} midi_scores;

void midi_note_on(int chan,int time,int midi_nr,ProgChange& pch) {
  uchar lnr,sign;
  Midi_Note *nb=note_arr[midi_nr]+chan;
  ++nb->occ;
  if (nb->occ==1) {  // else no opdating
    nb->start_time=time;
    if (midinr_to_lnr(midi_nr+midi_in.shift+pch.transp,lnr,sign,midi_in.acc)) {
      nb->valid=true;
      nb->lnr=lnr;
      nb->sign=sign;
      nb->pch_data=&pch;
    }
    else nb->valid=false;
  }
}

void midi_note_off(int chan,int time,int midi_nr) {
  Midi_Note *nb=note_arr[midi_nr]+chan;
  if (nb->occ) {
    --nb->occ;
    //if (nb->occ) alert("unexpected note %d, channel %d, time %d",midi_nr,chan+1,time*4/division);
    if (nb->valid && !nb->occ) { // else do nothing
      Score *sc=midi_scores.get_score(nb->pch_data->tune);
      if (sc) {
        if (sc->ngroup>=0 && sc->ngroup!=nb->pch_data->group_nr)
          alert("group nr mismatch in .gm-map file (tune: '%s')",ind2tname(sc->name));
        else
          sc->ngroup=nb->pch_data->group_nr;
        sc->insert_midi_note(nb->start_time,time,nb->lnr,nb->pch_data->col_nr,nb->sign);
      }
    }
  }
  else
    alert("note %d lost, channel %d, time %d",midi_nr,chan+1,time*4/division);
}

void midi_perc_on(int chan,int time,int midi_nr,ProgChange& pch) { // midi_nr: 35 - 81
  MidiPercNote *nb=perc_note_arr+midi_nr;
  ++nb->occ;
  int instr=get_perc_instr(midi_nr);
  Score *sc=midi_scores.get_score(pch.tune);
  if (sc) {
    sc->ngroup=pch.group_nr;
    sc->insert_midi_perc(time,sclin_max-1-instr,instr);
  }
}

void midi_perc_off(int time,int midi_nr) {
  if (midi_nr<perc_lo || midi_nr>perc_hi)
    return;
  MidiPercNote *nb=perc_note_arr+midi_nr;
  if (nb->occ)
    --nb->occ;
  else {
    alert("percussion note %d lost, time %d",midi_nr,time*4/division);
  }
}

void midi_noteOn(int chan,int time,int midi_nr,ProgChange& pch) {
  //if (debug) printf("noteOn: chan=%d time=%d instr=%d nr=%d transp=%d\n",chan,time,instr,midi_nr,transp);
  if (chan==9)  // percussion
    midi_perc_on(chan,time,midi_nr,pch);
  else
    midi_note_on(chan,time,midi_nr,pch);
}

void midi_noteOff(int chan,int time,int midi_nr,ProgChange& pch) {
  //if (debug) printf("noteOff: chan=%d time=%d instr=%d nr=%d\n",chan,time,instr,midi_nr);
  if (chan==9)  // percussion
    midi_perc_off(time,midi_nr);
  else
    midi_note_off(chan,time,midi_nr);
}

int cur_time() { // rounded to nearest integer
  if (tinc_is_one)
    return divide(subdiv*Mf_currtime*nu_pq,division);
  return lrint(subdiv*Mf_currtime*nu_pq*t_incr/division);
}

bool fill_chan_array(FILE *gmmap) {
  int chan_nr,
      time;
  Str str;
  for (;;) {
    str.rword(gmmap,"=");  // "channel="
    if (str.ch==EOF) break;
    if (str!="channel") { alert(".gm-map file: missing 'channel' item"); return false; }
    str.rword(gmmap," ");
    chan_nr=atoi(str.s)-1;
    if (chan_nr<0 || chan_nr>=16) { alert(".gm-map file: channel nr out of range"); return false; }
    str.rword(gmmap,"=");  // "time="
    str.rword(gmmap," \"");
    time=atoi(str.s);
    if (debug) printf("fill_chan_arr: time=%d cnr=%d\n",time,chan_nr);
    SLList_elem<ProgChange> *pc=channels[chan_nr].pc_list.insert(ProgChange(time,chan_nr),true);
    if (str.ch=='"')
      str.rword(gmmap,"\"");
    else
      str.rword(gmmap," ");
    if (chan_nr!=9) {
      str.rword(gmmap," "); // patch nr
      str.rword(gmmap," "); // color
      pc->d.col_nr=color_nr(str.s,0);
    }
    str.rword(gmmap," ");  // tune name
    pc->d.tune=strdup(str.s);
    str.rword(gmmap," \n");
    pc->d.group_nr=atoi(str.s);
    if (chan_nr==9) {  // percussion?
      if (str.ch!='\n') {
        alert("wrong nr items in .gm-map file (expected 5)");
        return false;
      }
    }
    else {
      if (str.ch==' ') {
        str.rword(gmmap," \n");
        pc->d.transp=atoi(str.s);
      }
      if (str.ch!='\n') {
        alert("wrong nr items in .gm-map file (expected 7 or 8)");
        return false;
      }
    }
  }
  return true;
}

void print_mapf(FILE *gmmap) {
  int chan;
  SLList_elem<ProgChange> *pc;
  for (chan=0;chan<16;++chan) {
    if (chan==9) {
      fprintf(gmmap,"channel=10 time=0   %-30s       midi 0\n","PERCUSSION");
    }
    else {
      char name[40];
      for (pc=channels[chan].pc_list.lis;pc;pc=pc->nxt) {
        snprintf(name,40,"\"%s\" (%d)",gm_instr[pc->d.gm_nr].name,gm_instr[pc->d.gm_nr].gm_nr);
        fprintf(gmmap,"channel=%-2d time=%-3d %-30s black midi 0\n",chan+1,pc->d.time,name);
      }
    }
  }
}

bool MidiIn::chanmessage(int status,int c1,int c2) {
  int chan = status & 0xf;
  status = status & 0xf0;
  switch (status) {
    case 0x80:
    case 0x90: 
      if (read_mapf) {
        MidiChannel *mc=channels+chan;
        if (status==0x90) {
          if (!mc->cur_pc) { alert(".gm-map file: unexpected channel nr %d",chan+1); return false; }
          while (mc->cur_pc->nxt && mc->cur_pc->nxt->d.time<Mf_currtime) mc->cur_pc=mc->cur_pc->nxt;
        }
        SLList_elem<ProgChange> *pc=mc->cur_pc;
        if (debug) printf("chanmessage: stat=0x%x pc=%p\n",status,pc);
        if (status==0x90 && c2>0)
          midi_noteOn(chan,cur_time(),c1,pc->d);
        else
          midi_noteOff(chan,cur_time(),c1,pc->d);
      }
      break;
    case 0xa0:
      if (debug) puts("Mf_pressure");
      break;
    case 0xb0:
      if (debug) printf("Mf_parameter: %d %d %d\n",chan,c1,c2);
      break;
    case 0xe0:
      if (debug) puts("Mf_pitchbend");
      break;
    case 0xc0:
      if (debug) printf("Mf_program: chan=%d c1=%d\n",chan,c1);
      if (!read_mapf)
        channels[chan].pc_list.insert(ProgChange(Mf_currtime,c1),true);
      break;
    case 0xd0:
      if (debug) puts("Mf_chanpressure");
      break;
    default:
      printf("chanmessage: %x\n",status);
  }
  return true;
}

void msgadd(int c) {
  if (Msgindex >= msgbuf_max-1) {
    alert("msgadd: buf overflow");
  }
  else {
    Msgbuff[Msgindex++] = c;
    Msgbuff[Msgindex] = 0;
  }
}

void metaevent(int type) {
  int leng = Msgindex;
  char *m = Msgbuff;

  switch  ( type ) {
    case 0x00:
      if (debug) printf("Mf_seqnum: %d\n",to16bit(m[0],m[1]));
      break;
    case 0x01:  /* Text event */
    case 0x02:  /* Copyright notice */
    case 0x03:  /* Sequence/Track name */
    case 0x04:  /* Instrument name */
    case 0x05:  /* Lyric */
    case 0x06:  /* Marker */
    case 0x07:  /* Cue point */
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f:
      /* These are all text events */
      if (debug) printf("Mf_text: %d %d '%s'\n",type,leng,m);
      break;
    case 0x2f:  /* End of Track */
      if (debug) puts("Mf_eot");
      break;
    case 0x51:  /* Set tempo */
      if (debug) printf("Mf_tempo: %d %d %d\n",m[0],m[1],m[2]);
      break;
    case 0x54:
      if (debug) printf("Mf_smpte: %d %d %d %d %d\n",m[0],m[1],m[2],m[3],m[4]);
      break;
    case 0x58:
      nu_pq=m[3]/2;
      if (debug) printf("Mf_timesig: %d %d %d %d\n",m[0],m[1],m[2],m[3]);
      break;
    case 0x59:
      if (debug) printf("Mf_keysig: %d %d\n",m[0],m[1]);
      break;
    case 0x7f:
      if (debug) printf("Mf_seqspecific %d '%s'\n",leng,m);
      break;
    default:
      if (debug) printf("Mf_metamisc: %d %d '%s'\n",type,leng,m);
  }
}

void sysex()
{
  if (debug) printf("Mf_sysex: %d %s\n",Msgindex,Msgbuff);
}

bool MidiIn::readtrack() {    /* read a track chunk */
  /* This array is indexed by the high half of a status byte.  It's */
  /* value is either the number of bytes needed (1 or 2) for a channel */
  /* message, or 0 (meaning it's not  a channel message). */
  static int chantype[] = {
    0, 0, 0, 0, 0, 0, 0, 0,   /* 0x00 through 0x70 */
    2, 2, 2, 2, 1, 1, 2, 0    /* 0x80 through 0xf0 */
  };
  char buf[10];
  int lookfor,
      c, c1, type,
      status = 0,    /* status value (e.g. 0x90==note-on) */
      needed,
      varinum;
  bool sysexcontinue = false,  /* 1 if last message was an unfinished sysex */
       running = false;        /* 1 when running status used */

  if (fread(buf,4,1,midi_f)!=1) { alert("readtrack: fread problem"); return false; }
  if (strncmp(buf,"MTrk",4)) {
    alert("track start 'MTrk' not found");
    return false;
  }

  Mf_toberead = read32bit();
  Mf_currtime = 0;
  Mf_bytesread =0;
  for (int i=0;i<16;++i) channels[i].cur_pc=channels[i].pc_list.lis;

  if (debug) printf("Mf_trackstart, %d bytes\n",Mf_toberead);

  while ( Mf_toberead > 0 ) {

    Mf_currtime += readvarinum();  /* delta time */

    c = egetc();

    if ( sysexcontinue && c != 0xf7 ) {
      alert("didn't find expected continuation of a sysex");
      return false;
    }
    if ( (c & 0x80) == 0 ) {   /* running status? */
      if ( status == 0 ) {
        alert("unexpected running status");
        return false;
      }
      running = true;
    }
    else {
      status = c;
      running = false;
    }

    needed = chantype[ (status>>4) & 0xf ];

    if ( needed ) {    /* ie. is it a channel message? */
      if ( running )
        c1 = c;
      else
        c1 = egetc();
      if (!chanmessage( status, c1, (needed>1) ? egetc() : 0 )) {
        return false;
      }
      continue;
    }

    switch ( c ) {
      case 0xff:      /* meta event */
        type = egetc();
        if (debug) printf("Meta event, type=0x%x\n",type);
        varinum = readvarinum();
        lookfor = Mf_toberead - varinum;
        Msgindex = 0;

        while ( Mf_toberead > lookfor )
          msgadd(egetc());

        metaevent(type);
        break;

      case 0xf0:    /* start of system exclusive */
        if (debug) printf("Start sysex\n");
        varinum = readvarinum();
        lookfor = Mf_toberead - varinum;
        Msgindex = 0;
        msgadd(0xf0);
  
        while ( Mf_toberead > lookfor )
          msgadd(c=egetc());
  
        if ( c==0xf7 )
          sysex();
        else
          sysexcontinue = true;  /* merge into next msg */
        break;
  
      case 0xf7:  /* sysex continuation or arbitrary stuff */
        if (debug) printf("Sysex continuation\n");

        varinum = readvarinum();
        lookfor = Mf_toberead - varinum;
  
        if ( ! sysexcontinue )
          Msgindex = 0;
  
        while ( Mf_toberead > lookfor )
          msgadd(c=egetc());
  
        if (!sysexcontinue) {
          if (debug) printf("Mf_arbitrary: %d %s\n",Msgindex,Msgbuff);
        }
        else if ( c == 0xf7 ) {
          sysex();
          sysexcontinue = 0;
        }
        break;
      default:
        alert("track: unexpected byte %x",c);
        if (debug) printf("UNEXPECTED BYTE %x\n",c);
        break;
    }
  }
  if (debug) puts("end of track");
  return true;
}

bool MidiIn::read_mf(const char* midi_fn,const char* i_map_fn) {
  static char buf[10];
  int i,i2,
      format,
      track;
  Str str;
  shift=0; format=0; key_nr=0; acc=eLo;
  alert_mes=false;
  for (i=0;i<16;++i) channels[i].reset();
  if (read_mapf) {
    FILE *gm_map=fopen(i_map_fn,"r");
    if (!gm_map) {
      alert("%s not opened",i_map_fn);
      return false;
    }
    str.rword(gm_map," \n");
    if (str=="set") {
      for (;;) {
        str.rword(gm_map,":\n");
        if (str=="format") {
          str.rword(gm_map," \n");
          format=atoi(str.s);
          if (format!=2) {
            alert(".gm-map file: wrong format (expected: 2). Delete or rename it.");
            return false;
          }
        }
        else if (str=="key") {
          str.rword(gm_map," \n");
          if (!key_name_to_keynr(str.s,key_nr)) return false;
        }
        else if (str=="acc") {
          str.rword(gm_map," \n");
          if (str=="sharp") acc=eHi;
          else if (str=="flat") acc=eLo;
          else {
            alert("unknown acc '%s' (should be 'sharp' or 'flat') in %s",str.s,i_map_fn);
            return false;
          }
        }
        else if (str=="shift") {
          str.rword(gm_map," \n");
          shift=atoi(str.s);
        }
        else if (str=="tinc") {
          str.rword(gm_map," \n");
          t_incr=atof(str.s);
          if (fabs(t_incr-1.0)>0.01) {
            tinc_is_one=false;
            app->act_meter=lrint(8*t_incr);
          }
        }
        else {
          alert("unknown parameter '%s' in %s",str.s,i_map_fn);
          return false;
        }
        if (str.ch=='\n')
          break;
      }
    }
    else {
      alert("no first 'set ...' line in %s",i_map_fn);
      return false;
    }
    if (!fill_chan_array(gm_map)) return false;
    fclose(gm_map);
  }
  if ((midi_f=fopen(midi_fn,"r"))==0) {
    alert("midi file '%s' not found",midi_fn);
    return false;
  }
  for (i=0;i<128;++i) {
    for (i2=0;i2<16;++i2) note_arr[i][i2].occ=0;
  }
  for (i=0;i<=perc_hi;++i)
    perc_note_arr[i].occ=0;
  midi_scores.reset();
  if (fread(buf,4,1,midi_f)!=1) { alert("fread problem, file %s",midi_fn); return false; }
  if (strncmp(buf,"MThd",4)) {
    alert("unexpected start '%s' in %s (should be 'MThd')",buf,midi_fn);
    fclose(midi_f);
    return false;
  }
  Mf_toberead = read32bit();
  Mf_bytesread = 0;
  format = read16bit();
  ntrks = read16bit();
  division = read16bit();
  if (debug) printf("format=%d ntrks=%d division=%d\n",format,ntrks,division);

  while (Mf_toberead > 0) // flush any extra stuff
    egetc();
  track=1;
  bool init_ok=true;
  while ((init_ok=readtrack())==true) { 
    if (++track>ntrks) break;
  }
  fclose(midi_f);
  if (debug) printf("read_mf: read_mapf=%d\n",read_mapf);
  if (!read_mapf) {
    FILE *gm_map=fopen(i_map_fn,"w");
    if (!gm_map) {
      alert("%s not opened",i_map_fn);
      return false;
    }
    fputs("set format:2 key:C acc:flat\n",gm_map);
    print_mapf(gm_map);
    fclose(gm_map);
  }
  if (!init_ok) return false;
  if (alert_mes)
    alert("warning: out-of-range percussion instruments");
  return true;
}
