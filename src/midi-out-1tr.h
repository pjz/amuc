typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

struct MidiOut {
  int sub_div; // from sound.h
  int col2midi_instr[groupnr_max][colors_max]; // instruments
  int col2smp_instr[groupnr_max][colors_max];  // percussion, channel 10
  int col2pan[colors_max];
  int instr_mapping[groupnr_max][colors_max];  // actual mapping
  MidiOut();
  bool init(const char*,int subd,int us_per_beat,int meter,int nu_per_quarter);
  void close();
  void note_onoff(uchar tag,int col,int grn,int midi_chan,bool sampled,int time,int nr,int ampl);
};

extern MidiOut midi_out;
extern bool debug;

void alert(char *form,...);
