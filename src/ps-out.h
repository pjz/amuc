typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

const int annot_max='Z'-'A'+1;

struct PostscriptOut {
  struct AnnotChar {
    int mnr;
    char ch;
  };
  int meter,
      nupq,   // note units per quarter note
      nu_subd, // subdivision of note units, used for note- and rest-splitting
      lst_ind,
      key_nr,
      start_meas,
      transp[groupnr_max][colors_max];
  AnnotChar annot[annot_max];
  bool s_voice;  // single-voice scores?
  const char *title,
       *abc_or_ps_file,
       *header_file;
  void init_ps();
  void write_ps(bool abc_only);
  void reset_ps();
  void insert(int col,int grn,int time,int lnr,int note_sign,int dur);
  void insert_perc(int col,int grn,int time);
  void add_first_rests(struct Voice*,int subv_nr,int meas_nr,const char* rest);
  void add_last_rest(struct Voice*,int subv_nr,int meas_nr,const char* rest,int annot_ch);
  bool find_ai(int mnr,int& ai);
  void add_Z_rest(struct Voice *voice,int subv_nr,int meas_nr);
  bool fill_subvoice(int meas_nr,int subv_nr,int annot_ch,bool& done);
  bool fill_perc_subv(int meas_nr,int subv_nr,int annot_ch,bool& done);
  void create_postscript(struct Score*); // in sound.cpp
  void set(int m,int nu,int knr,char *t);
  void print_subvoice(int voice_nr,int group_nr,struct Voice *voice,int subv);
  void print_perc_subvoice(int group_nr,struct Voice *voice,int subv);
};

int gen_ps(char *prog,const char *outf);

extern PostscriptOut ps_out;
