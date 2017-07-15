const int
  subdiv=3;        // time units pro score segment

enum {
  eMid, eLLeft, eLeft, eRight, eRRight,   // stereo pan
  eModFreq, eModIndex,     // FM modulation
  eSynthMode, eNativeMode, // instr mode
  ePhysmodMode,eWaveMode,  // sample mode
  eDumpwav,eMidiout,ePsout,eAbcout,  // values of task
  eJack,eAlsa              // output port
};

struct CtrlBase {
  BgrWin *cview;
  int bgwin,
      ctrl_col,
      instrm_val,  // eSynthMode, eNativeMode
      sample_mode; // ePhysmodMode, eWaveMode
  Rect gr_rect;    // for RButWin *group
  SliderData ampl_val[3];  // values for amplitude groups
  HSlider *ampl;
  RButWin *mode;  // instr, msynth
  struct Instrument *inst;
  CtrlBase *isa[3];   // sounds as
  Point synth_top;  // location of synth window
  Synth *synth;
  Button *eq;      // equal to
  RButWin *group; // 0,1,2
  CheckBox *ctrl_pitch;
  CtrlBase(Rect,int col);
  static void draw(Id id);
  void draw_isa_col(); // indication of set eq:
  void set_isa(int col); // equal to other instr
  void set_isa(int col,int gr);
  void set_ms_isa(int col); // synth controlled by other synth
  void set_loc(int locat);
  void set_instr_mode(int n,bool upd);
  void set_ampl_txt(int val);
  int *get_ampl_ptr(bool sampled,int group_nr);
  virtual void dump_settings(char *buf,int bmax) = 0;
  void map_win(bool map);
  void init_ampl();
  void set_revb(int val,int phasing);
};

struct RedCtrl:CtrlBase {
  RedCtrl(Rect,int col);
  HVSlider *start_timbre,
           *timbre;
  HSlider *startup,
          *decay,
          *dur_limit;
  VSlider *start_amp;
  RButWin *tone;
  void set_startup();
  void set_decay();
  void set_start_amp();
  void set_start_timbre();
  void set_timbre();
  void set_durlim();
  void set_tone();
  void dump_settings(char *buf,int bmax);
  static void draw(Id);
};

struct FMCtrl: CtrlBase {
  FMCtrl(Rect,int col);
  HVSlider *fm_ctrl,
           *mod_mod;
  HSlider *detune,*attack,*decay;
  CheckBox *base_freq;
  void set_attack();
  void set_decay();
  void setfm(int);
  void set_detune();
  void set_mmod();
  void dump_settings(char *buf,int bmax);
};

struct GreenCtrl: CtrlBase {
  GreenCtrl(Rect,int col);
  HSlider *attack, *decay,
          *freq_mult,
          *chorus;
  HVSlider *timbre1,
           *timbre2;
  void set_attack(),set_decay(),
       set_freq_mult(),
       set_timbre1(),set_timbre2();
  static void draw(Id);
  void dump_settings(char *buf,int bmax);
};

struct PurpleCtrl: CtrlBase {
  PurpleCtrl(Rect,int col);
  VSlider *harm[harm_max];    // harmonics
  VSlider *st_harm[harm_max]; // startup harmonics
  HSlider *start_dur,         // attack duration
          *decay;
  RButWin *sound;             // tone mode
  void set_hs_ampl(int*);
  void set_st_hs_ampl(int*);
  void set_start_dur();
  void set_tone();
  void set_decay();
  static void draw(Id);
  void dump_settings(char *buf,int bmax);
};

struct BlueCtrl: CtrlBase {
  BlueCtrl(Rect,int col);
  HSlider *attack,*decay,*dur_limit,*lowpass;
  CheckBox *chorus,*rich;
  void set_attack();
  void set_decay();
  void set_durlim();
  void set_lowpass();
  void dump_settings(char *buf,int bmax);
};

struct PhmCtrl:CtrlBase {  // used for physical and for sampled waveforms
  int subwin1,
      subwin2;
  BgrWin *subview1,
         *subview2;
  ChMenu *file_select; // = app->app_local->file_sel
  PhmCtrl(Rect,int col);
  HVSlider *speed_tension;
  HSlider *wave_ampl,
          *decay;
  CheckBox *add_noise,
           *just_listen;
  void dump_settings(char *buf,int bmax);
  bool reparent();
  void reset_wf_sliders(bool do_draw);
  void set_mode(int tak,ScInfo&);
  void set_ampl_txt(int);
  void set_sampled_loc(int loc);
  static void show_menu(Id);
};

struct KeybNote {
  int lnr,snr,dur;
  uint col:4,
       sign:2;
};

struct KeybTune {
  static const int keyb_notes_max=10000;
  KeybNote buf[keyb_notes_max];
  int cur_ind,
      turn_start,
      tune_wid;
  void reset();
  void nxt_turn();
  KeybTune();
  void add(int lnr,int sign,int snr1,int snr2,int col);
};

bool exec_info(int,Score*,bool);
void init_soundcard();
CtrlBase *col2ctrl(int col);
PhmCtrl *col2phm_ctrl(int col);
struct Instrument *col2instr(int note_col);
void *wave_listen(void *arg);
bool play_wfile(float *buf_left,float *buf_right);
int same_color(ScSection *fst,ScSection *sec,ScSection *end,ScSection*& lst);
void restore_marked_sects(Score* sc,int play_start);
bool init_phmbuf();
bool play(float *buf_left,float *buf_right);
void set_time_scale();
void set_buffer_size();
void reset_mk_nbuf();

extern KeybTune kb_tune;
extern bool mk_connected,
            sample_rate_changed;
extern const char *midi_out_file,
                  *ps_out_file,
                  *wave_out_file;
extern const float ampl_mult[];
extern int output_port;
extern struct JackInterf *jack_interface;
