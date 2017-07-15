/* Generated tags: 0x80,
                   0xb0 ... 0xb5, 0,
                   0x40,
                   0xc0 ... 0xc5, 0 ... 17,
                   0xc9, 0,
                   0x7f,
                   0xff,
                   0x2f, 0
   For the Timidity midi-player the events must be in a certain order - found out by trial-and-error.
*/
#include <stdio.h>
#include <string.h>
#include "colors.h"
#include "midi-out.h"

static const char* gm_instr[128]= {
  "Acoustic Grand Piano",
  "Bright Acoustic Piano",
  "Electric Grand Piano",
  "Honky-tonk Piano",
  "Electric Piano 1",
  "Electric Piano 2",
  "Harpsichord",
  "Clavi",
  "Celesta",
  "Glockenspiel" ,
  "Music Box" ,
  "Vibraphone" ,
  "Marimba" ,
  "Xylophone" ,
  "Tubular Bells" ,
  "Dulcimer" ,
  "Drawbar Organ" ,
  "Percussive Organ" ,
  "Rock Organ" ,
  "Church Organ" ,
  "Reed Organ" ,
  "Accordion" ,
  "Harmonica" ,
  "Tango Accordion" ,
  "Acoustic Guitar (nylon)" ,
  "Acoustic Guitar (steel)" ,
  "Electric Guitar (jazz)" ,
  "Electric Guitar (clean)" ,
  "Electric Guitar (muted)" ,
  "Overdriven Guitar" ,
  "Distortion Guitar" ,
  "Guitar harmonics" ,
  "Acoustic Bass" ,
  "Electric Bass (finger)" ,
  "Electric Bass (pick)" ,
  "Fretless Bass" ,
  "Slap Bass 1" ,
  "Slap Bass 2" ,
  "Synth Bass 1" ,
  "Synth Bass 2" ,
  "Violin" ,
  "Viola" ,
  "Cello" ,
  "Contrabass" ,
  "Tremolo Strings" ,
  "Pizzicato Strings" ,
  "Orchestral Harp" ,
  "Timpani" ,
  "String Ensemble 1" ,
  "String Ensemble 2" ,
  "SynthStrings 1" ,
  "SynthStrings 2" ,
  "Choir Aahs" ,
  "Voice Oohs" ,
  "Synth Voice" ,
  "Orchestra Hit" ,
  "Trumpet" ,
  "Trombone" ,
  "Tuba" ,
  "Muted Trumpet" ,
  "French Horn" ,
  "Brass Section" ,
  "SynthBrass 1" ,
  "SynthBrass 2" ,
  "Soprano Sax" ,
  "Alto Sax" ,
  "Tenor Sax" ,
  "Baritone Sax" ,
  "Oboe" ,
  "English Horn" ,
  "Bassoon" ,
  "Clarinet" ,
  "Piccolo" ,
  "Flute" ,
  "Recorder" ,
  "Pan Flute" ,
  "Blown Bottle" ,
  "Shakuhachi" ,
  "Whistle" ,
  "Ocarina" ,
  "Lead 1 (square)" ,
  "Lead 2 (sawtooth)" ,
  "Lead 3 (calliope)" ,
  "Lead 4 (chiff)" ,
  "Lead 5 (charang)" ,
  "Lead 6 (voice)" ,
  "Lead 7 (fifths)" ,
  "Lead 8 (bass + lead)" ,
  "Pad 1 (new age)" ,
  "Pad 2 (warm)" ,
  "Pad 3 (polysynth)" ,
  "Pad 4 (choir)" ,
  "Pad 5 (bowed)" ,
  "Pad 6 (metallic)" ,
  "Pad 7 (halo)" ,
  "Pad 8 (sweep)" ,
  "FX 1 (rain)" ,
  "FX 2 (soundtrack)" ,
  "FX 3 (crystal)" ,
  "FX 4 (atmosphere)",
  "FX 5 (brightness)",
  "FX 7 (echoes)",
  "FX 8 (sci-fi)",
  "Sitar",
  "Banjo",
  "Shamisen",
  "Koto",
  "Kalimba",
  "Bag pipe",
  "Fiddle",
  "Shanai",
  "Tinkle Bell",
  "Agogo",
  "Steel Drums",
  "Woodblock",
  "Taiko Drum",
  "Melodic Tom",
  "Synth Drum",
  "Reverse Cymbal",
  "Guitar Fret Noise",
  "Breath Noise",
  "Seashore",
  "Bird Tweet",
  "Telephone Ring",
  "Helicopter",
  "Applause",
  "Gunshot" };

MidiOut midi_out;

static uchar *reord16(uint n) {
  static uchar buf[2];
  buf[0]=(n>>8) & 0xff;
  buf[1]=n & 0xff;
  return buf;
}

static uchar *reord8(uchar n) {
  static uchar buf[1];
  buf[0]=n;
  return buf;
}

static uchar *reord24(uint n) {
  static uchar buf[3];
  buf[0]=(n>>16) & 0xff;
  buf[1]=(n>>8) & 0xff;
  buf[2]=n & 0xff;
  return buf;
}

static uchar *reord32(uint n) {
  static uchar buf[4];
  buf[0]=(n>>24) & 0xff;
  buf[1]=(n>>16) & 0xff;
  buf[2]=(n>>8) & 0xff;
  buf[3]=n & 0xff;
  return buf;
}

void check(bool ok) { if (!ok) puts("fwrite problem"); }

static void wr(uchar *n,int nr) {
  midi_out.data_size+=nr;
  check(1==fwrite(n, nr,1,midi_out.midi_f));
}

static void del_0() {
  uchar i8=0;
  wr(&i8, 1);
}

static void var_int(uint val) {
  uchar bytes[5];
  uchar i8;
  bytes[0] = (val >> 28) & 0x7f;    // most significant 5 bits
  bytes[1] = (val >> 21) & 0x7f;    // next largest 7 bits
  bytes[2] = (val >> 14) & 0x7f;
  bytes[3] = (val >> 7)  & 0x7f;
  bytes[4] = (val)       & 0x7f;    // least significant 7 bits

  int start = 0;
  while (start<5 && bytes[start] == 0)  start++;

  for (int i=start; i<4; i++) {
    i8=bytes[i] | 0x80;
    wr(&i8, 1);
  }
  i8=bytes[4];
  wr(&i8, 1);
}

MidiOut::MidiOut() {
}

bool MidiOut::init(const char *fn,int subd,int nupq,int met,int us_pb) {  // sets init_ok if succesful
  init_ok=false;
  sub_div=subd;
  nu_pq=nupq;
  meter=met;
  us_per_beat=us_pb;
  if ((midi_f=fopen(fn,"w"))==0) {
    alert("%s not opened",fn);
    return false;
  }
  for (int grn=0;grn<groupnr_max;++grn) {
    col2midi_instr[grn][eBlack] = 6-1;   // electric piano 2
    col2midi_instr[grn][eRed]   = 25-1;  // guitar
    col2midi_instr[grn][eBlue]  = 0;     // piano
    col2midi_instr[grn][eGreen] = 22-1;  // reed organ
    col2midi_instr[grn][ePurple]= 17-1;  // drawbar organ
    col2midi_instr[grn][eBrown] = 8-1;   // clavichord
    for (int n=0;n<colors_max;++n)
      instr_mapping[grn][n]=-1; // force program change
    col2smp_instr[grn][eBlack] = 36;  // bass drum
    col2smp_instr[grn][eRed]   = 47;  // low-mid tom
    col2smp_instr[grn][eBlue]  = 46;  // open hi-hat
    col2smp_instr[grn][eGreen] = 38;  // snare
    col2smp_instr[grn][ePurple]= 62;  // mute high conga
    col2smp_instr[grn][eBrown] = 42;  // closed hi-hat
  }

  col2pan[eBlack]=30;
  col2pan[eRed]=100;
  col2pan[eBlue]=30;
  col2pan[eGreen]=100;
  col2pan[ePurple]=30;
  col2pan[eBrown]=100;
  new_chan=-1;
  for (int i=0;i<colors_max*groupnr_max;++i) col_gr2chan[i]= -1;
  init_ok=true;
  chan_warn=false;
  check(1==fwrite("MThd", 4,1,midi_f)); // header
  wr(reord32(6), 4);          // header size
  wr(reord16(1), 2);          // file type
  return true;
}

void MidiOut::init2(int nr_tracks) {
  wr(reord16(nr_tracks), 2);  // # tracks
  wr(reord16(4*sub_div), 2);  // time format
}

void MidiOut::write_track1() {
  check(1==fwrite("MTrk", 4,1,midi_f)); // track data
  seek_setsize=ftell(midi_f);
  wr(reord32(0), 4);          // track size (temporary)
  data_size=0; cur_time=0;

  del_0();
  wr(reord8(0xff),1);        // meta event
  wr(reord8(0x58),1);        // time signature
  var_int(4);
  wr(reord8(meter/nu_pq),1); // numerator of time signature
  wr(reord8(2),1);           // 2log of denominator of time signature
  wr(reord8(24),1);          // midi clocks (1 clock = 1/24 quarter note) per quarter note
  wr(reord8(8),1);           // 1/32 notes per quarter note

  del_0();
  wr(reord8(0xff),1);        // meta event
  wr(reord8(0x51),1);        // tempo meta event (must be after previous timing events)
  var_int(3);
  wr(reord24(us_per_beat*nu_pq/4),3); // micro sec per quarter note, typical 500000
}

void MidiOut::init_track(int tracknr,int gr,int col) {
  cur_track=tracknr;
  if (debug) puts("-- new track --");
  check(1==fwrite("MTrk", 4,1,midi_f)); // track data
  seek_setsize=ftell(midi_f);
  wr(reord32(0), 4);          // track size (temporary)
  data_size=0; cur_time=0;
}

void MidiOut::init_perc_track(int tracknr) {
  cur_track=tracknr;
  check(1==fwrite("MTrk", 4,1,midi_f)); // track data
  seek_setsize=ftell(midi_f);
  wr(reord32(0), 4);          // track size (temporary)
  data_size=0; cur_time=0;

  del_0();
  wr(reord8(0xff),1);      // meta event
  wr(reord8(0x03),1);      // track title
  char title[50];
  int len=snprintf(title,50,"track %d - Percussion",cur_track);
  var_int(len);
  wr((uchar*)title,len);

  del_0();
  wr(reord8(0xc9),1);   // select instrument channel 10
  wr(reord8(0),1);      // instrument
  del_0();
  wr(reord8(0xb9),1);   // change mode channel 10
  wr(reord8(0x7),1);    // volume
  wr(reord8(127),1);
  del_0();
  wr(reord8(0x0a),1);   // pan
  wr(reord8(60),1);
}

void MidiOut::close_track() {
  del_0();
  wr(reord8(0xff),1);
  wr(reord8(0x2f),1);
  wr(reord8(0),1);
  int trackend=ftell(midi_f);
  fseek(midi_f, seek_setsize, SEEK_SET);
  wr(reord32(data_size), 4);
  fseek(midi_f, trackend, SEEK_SET);
}

static int note2note(int n) {         // midi: middle C = 60, Amuc: middle C = 20
  if (n<0 || n>=100) { alert("midi: bad note"); return 60; }
  return n;
}

int MidiOut::get_chan(bool sampled,int ind) { // ind = index in col_gr2chan[]
  if (sampled) return 9;
  if (col_gr2chan[ind]>=0) return col_gr2chan[ind];
  if (new_chan==16-1) {
    if (!chan_warn) { chan_warn=true; alert("midi: channels > 16"); }
    return 0;
  }
  ++new_chan;
  if (new_chan==9) ++new_chan;  // skip percussion channel
  return (col_gr2chan[ind]=new_chan);
}

void MidiOut::note_onoff(bool start,int col,int grn,int midi_instr,bool sampled,int tim,int nr,int ampl) {
  if (!init_ok) return;
  if (debug) printf("start=%d col=%d grn=%d sampled=%d tim=%d instr=%d midi-nr=%d ampl=%d\n",
                    start,col,grn,sampled,tim,midi_instr,nr,ampl);
  int chan=get_chan(sampled,col+grn*colors_max);
  if (!sampled && instr_mapping[grn][col]!=midi_instr) {
    if (debug) printf("instr from %d to %d, chan=%d\n",instr_mapping[grn][col],midi_instr,chan);
    instr_mapping[grn][col]=midi_instr;

    del_0();
    wr(reord8(0xff),1);      // meta event
    wr(reord8(0x03),1);      // track title
    char title[100];
    int len=snprintf(title,100,"track %d - %s",cur_track,gm_instr[midi_instr]);
    var_int(len);
    wr((uchar*)title,len);
    del_0();
    wr(reord8(0xb0 + chan),1);   // control change mode

    wr(reord8(0x40),1);          // damper on/off
    wr(reord8(0),1);             // damped
    del_0();
    wr(reord8(0x7),1);           // volume
    wr(reord8(80),1);    // good balance with percussion amplitude

    // following events will be timed!
    if (tim==cur_time) del_0();
    else {
      var_int((tim-cur_time)*sub_div*4/nu_pq);
      cur_time=tim;
    }
    wr(reord8(0x0a),1);          // pan
    wr(reord8(col2pan[col]),1);
    del_0();
    wr(reord8(0xc0 + chan),1);   // program change
    wr(reord8(midi_instr),1);    // instrument
    del_0();
  }
  else {
    if (tim==cur_time) del_0();
    else {
      var_int((tim-cur_time)*sub_div*4/nu_pq);
      cur_time=tim;
    }
  }

  wr(reord8((start ? 0x90 : 0x80)+chan),1);        // note on, channel #
  if (sampled)
    wr(reord8(midi_instr),1);
  else
    wr(reord8(note2note(nr)),1); // note nr
  wr(reord8(start ? ampl : 0),1); // velocity
}

void MidiOut::close() {
  if (!init_ok) return;
  fclose(midi_f);
  init_ok=false;
}
/*
int main() {
  midi_out.init("out.mid",3);
  midi_out.note_on(0,0,20,4);
  midi_out.note_off(0,200,20,0);
  midi_out.close();
  if (init_ok) exit(0); exit(1);
}
*/
