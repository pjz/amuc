const int
  NSLIDER=31,
  NRBUT=5,
  NCHECKB=3,
  NCONTROLS=NSLIDER+NRBUT+NCHECKB;

struct MSynthData {
  int col;
  uchar hsl_buf[NSLIDER],
      rb_buf[NRBUT],
      cb_buf[NCHECKB];
  bool fill_msynth_data(int col,const char *str);
};

struct Synth {
  struct MSynth *m_synth;
  uint topwin;     // top window
  XftDraw *xft_topwin;
  int *ms_ampl;   // pointer to amplitude value
  const int col;
  int isa_col;
  float mult_eg;  // used for eg display
  SubWin *subw;
  HSlider *hsliders[NSLIDER];
  RButWin *rbutwins[NRBUT];
  CheckBox *checkboxs[NCHECKB];
  Button *eq;
  struct Patches* set_patch;
  BgrWin *vcf_display,
         *eg1_display,
         *eg2_display,
         *vco1_display,
         *vco2_display,
         *vco3_display,
         *lfo_display;
  Synth(Point top,int col,bool do_map);
  void init(bool soft);
  void set_values(int,float freq);
  bool fill_buffer(float *buf,const int buf_size,const float amp_mul);
  void note_off();
  void update_values(MSynthData*,bool setpatch);
  void dump_settings(char *buf,int bufmax,const char *col);
  void button_cmd(Id);
  void set_isa(Synth *other);
  void map_topwin(bool do_map);
  void draw_isa_col(bool);
  void draw_eg_display(int eg);
  void draw_wave_display(int eg,bool test_if_needed=false);
};

extern bool debug;
extern const int nop;
void monosynth_uev(int cmd,int id);
