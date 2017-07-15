struct MidiIn {
  MidiIn();
  bool read_mapf;
  int key_nr,
      acc,    // accidentals: eHi, eLo
      shift;  // increase midi nr
  bool read_mf(const char *midi_fn,const char* i_map_fn);
  bool chanmessage(int status,int c1,int c2);
  bool readtrack();
};

extern MidiIn midi_in;
