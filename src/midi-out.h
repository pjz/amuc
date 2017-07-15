typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

struct MidiOut {
  FILE *midi_f;
  int data_size,
      cur_time,
      seek_setsize,
      cur_track, // current track nr
      new_chan,  // channel nr, start at 0, skip 9
      sub_div,   // from sound.h
      meter,
      nu_pq,     // note units per quarter
      us_per_beat;
  bool init_ok,
       chan_warn; // channel > 16?
  int col2midi_instr[groupnr_max][colors_max], // instruments
      col2smp_instr[groupnr_max][colors_max],  // percussion, channel 10
      col2pan[colors_max],
      col_gr2chan[colors_max*groupnr_max],
      instr_mapping[groupnr_max][colors_max];  // actual mapping
  MidiOut();
  void make_midi();  // in sound.cpp
  bool init(const char*,int subd,int nupq,int meter,int us_pb);
  void init2(int nr_tracks);
  void write_track1();
  void init_track(int tracknr,int group,int col);
  void init_perc_track(int tracknr);
  void close_track();
  void close();
  void note_onoff(bool start,int col,int grn,int midi_instr,bool sampled,int tim,int nr,int ampl);
  int get_chan(bool sampled,int ind);
};

extern MidiOut midi_out;
extern bool debug;
extern const float ampl_mult[ampl_max+1]; // from sound.cpp

void alert(const char *form,...);
