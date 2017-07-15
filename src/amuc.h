typedef unsigned short ushort;
typedef unsigned int uint;
typedef SafeBuffer<short> ShortBuffer;

const uint
  ePlay=0,
  ePlay_x=1,
  eSilent=2,
  eHi=1, // sharp
  eLo=2; // flat
const int
  sclin_max=45,      // max lines per score
  harm_max=5,        // harmonics purple instr
  mn_bits=14,        // multiple notes
  tn_bits=7,         // tune names
  times_max=50,      // max parameters of 'time:' command
  scope_dim=128;     // width of scope window
extern const char
  *maj_min_keys[keys_max*4];

struct ScoreViewBase;
struct Score;
struct MusicView;
struct ScLine;

struct ScSection {
  uint s_col       :4,  // eBlack, eRed, ...
       cat         :2,  // ePlay, ePlay_x, eSilent
       sign        :2,  // sharp: eHi, flat: eLo, or 0
       stacc       :1,  // staccato?
       sampled     :1,  // sampled or physical model note?
       sel         :1,  // selected?
       sel_color   :1,  // selected on color? (only valid if sel == true)
       port_dsnr   :5,  // portando: delta snr  
       del_start   :3,  // delay at section start
       del_end     :3,  // delay at section end
       s_group     :2;  // group nr
  int  port_dlnr   :6;  // portando: delta lnr
                 // sum: 31
  uint nxt_note    :mn_bits, // = 14, index in mn_buf.buf[]
       src_tune    :tn_bits; // = 7, index in tnames.buf[]
  ScSection();
  void drawSect(ScoreViewBase*,int snr,int lnr);
  void drawSect(ScoreViewBase*,ScoreViewBase*,int snr,int lnr);
  void drawPlaySect(ScoreViewBase* theV,Point start,Point end,uchar n_sign);
  void drawPortaLine(ScoreViewBase* theV,int snr,int lnr,bool erase);
  void reset();
  ScSection *nxt();
  ScSection* get_the_section(ScSection *from,int typ);
  void set_to_silent(struct ScoreView* sv,int snr,int lnr);
  int copy_to_new_sect(int typ,uint *lst_cp);
  void rm_duplicates(bool *warn=0);
  bool prepend(ScSection *from,int typ,uint *lst_cp);
};  

struct ScLine {
  uchar n_sign;   // sharp or flat
  ScSection *sect;
};

struct Score {
  int name;       // score-name = tname[name] 
  int len,        // number of sections
      lst_sect,   // last written section
      end_sect,   // place of end line
      sc_key_nr,  // key
      signs_mode, // flats or sharps?
      sc_meter,   // local meter
      ngroup;     // note group
  const int sc_type;   // 0 or eMusic
  struct ScInfo *scInfo;
  ScSection *block;
  Array<ScLine,sclin_max>lin;
  RButton *rbut;  // located in tunesView
  Score(const char *nam,int length,uint sctype);
  ~Score();
  void copy(Score*);
  void add_copy(Score*,int,int,int,int,int,ScoreViewBase*,int atcolor);
  void reset(bool reset_len);
  bool check_len(int);
  ScSection *get_section(int lnr,int snr);
  int get_meas(int snr);
  void tone2signs(int);
  void copy_keyb_tune();
  void insert_midi_note(int time,int time_end,int lnr,int col,int sign);
  void insert_midi_perc(int time,int lnr,int col);
};

struct App {
  Score *act_tune,
        *scoreBuf,
        *cur_score;
  Str script_file,
      input_file,
      command,
      midi_in_file;
  int act_score,     // set by score choice buttons
      act_color,     // set by color choice buttons
      act_meter,     // set by meter view
      act_action,    // set by mouse action choice buttons
      act_tune_ind,  // set by tunes view
      act_scope_scale,
      act_tempo,     // set by tempo slider
      mv_key_nr,     // score key of musicView
      nupq,          // note units per quarter note
      task,
      play_1_col;
  bool stop_requested,
       repeat;
  ExtRButCtrl *active_scoreCtrl;
  struct CtrlBase *act_instr_ctrl;
  struct FMCtrl *black_control,
                *brown_control;
  struct RedCtrl *red_control;
  struct GreenCtrl *green_control;
  struct BlueCtrl *blue_control;
  struct PurpleCtrl *purple_control;
  struct PhmCtrl *black_phm_control,
                 *red_phm_control,
                 *green_phm_control,
                 *blue_phm_control,
                 *purple_phm_control,
                 *brown_phm_control;
  struct ChordsWindow *chordsWin;
  CheckBox *no_set,
           *sampl,
           *conn_mk;
  struct ScopeView *scopeView;
  struct PtrAdjView *ptr_adjust;
  struct DialogWin *dia_wd;
  App(char *inf);
  ~App();
  void playScore(int start,int stop);
  bool exec_info_cmd(struct ScInfo&,bool);
  bool save(const char*);
  bool restore(const char *file,bool add_tunes);
  bool read_script(const char *script);
  bool save_script(const char *script);
  void new_tune(const char *tname);
  void copy_tune(const char *tname);
  bool run_script(const char *text);
  void set_ctrl_sliders();
  void check_reset_as();
  void modify_script(struct EditScript*,int start,int stop);
  bool read_tunes(const char*,bool add_tunes);
  void unfocus_textviews();
  bool find_score(Score*,struct ScoreView*&);
  void scope_insert(int val);
  void scope_draw();
  void report_meas(int);
  void svplay(struct ScoreView *sv1);
  void mplay();
  char *title();
};

struct ScopeView {
  Point pt_buf[scope_dim*64];
  int buf[scope_dim*64];
  int nr_samples,
      ptr,lst_ptr,
      time_scale,
      maxi; // max filled index in scope buffer
  const int mid;
  bool round;
  BgrWin *scopeview;
  BgrWin *bgwin;
  VSlider *scale;
  ScopeView(Rect);
  static void draw_cmd(Id);
  void reset();
  void draw();
  void redraw();
  void insert(int val);
  void set_scale();
};

int lnr_to_midinr(int lnr,uint sign);
bool midinr_to_lnr(uchar mnr,uchar& lnr,uchar& sign,int signs_mode);
Score *new_score(const char*);
bool key_name_to_keynr(char *kn,int& key_nr);
const char* ind2tname(int ind);
uint col2color(int col);

int min(int a,int b);
int max(int a,int b);
int minmax(int a, int x, int b);

extern const int nop,
             signs[][7],
             signsMode[keys_max];
extern bool debug,
            midi_mes,
            wb_extensions,
            i_am_playing,
            wf_playing;
extern const int colors[];
extern Style def_but_st, // default button style, set after init_xwindows()
       def_erb_st; // default extern radio button style
extern SliderStyle def_sl_st;

extern const char
  *pcm_dev_name,
  *amuc_data,
  *wave_samples,
  *cur_dir;

extern ShortBuffer wl_buf;
extern App *app;
