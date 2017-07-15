enum {
  eScDep_no,  // scale is constant
  eScDep_lk,  // scale depends on local key
  eScDep_sk   // scale depends on score key
};

struct ScaleData {
  static const int nmax=30;
  char *name;
  bool valid[24],
       gt12;
  int depmode;  // dependancy on local or score key  
};

struct ChordsWindow {
  SubWin *subw;
  BgrWin *circle;
  RButWin *keys,
          *scales;
  EditWin *tone_nrs;
  Button *but1,*but2,
         *get_scales;
  CheckBox *dep_mode;
  VScrollbar *scroll;
  Array<ScaleData,100>scale_data;
  int the_key_nr,
      dm;
  bool shift_12;
  uint chwin;
  XftDraw *xft_chwin;
  static void del_chwin(Id);
  static void quit_chwin(Id);
  static void hide_chwin(Id);
  static void set_key(Id);
  static void read_scales(Id);
  static void force_key(Id);
  static void base_cmd(Id,int nr,int fire);
  static void scales_cmd(Id,int nr,int fire);
  static void scr_cmd(Id,int val,int,bool);
  static void depm_cmd(Id,bool);
  static void draw_circ(Id);
  ChordsWindow(Point top);
  void draw_tone_numbers();
  bool read_scalef(const char *fname);
  void chord_or_scale_name(char *buf,int sc_key_nr);
  void upd_chordname(struct ScoreView *sv);
  void upd_scoreview(int scales_nr);
};

extern int keynr2ind(int key_nr);
