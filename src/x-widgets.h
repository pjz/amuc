#include <X11/X.h>    // used in this file
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <cairo.h>

typedef unsigned int uint;
typedef unsigned short uchort;
typedef unsigned char uchar;

extern uint cWhite,cRose,cBlack,cGrey,cRed,cBlue,
            cForeground,cBackground,cBorder,
            key_pressed,
            nominal_font_size;
extern XftColor *xft_White,*xft_Black,*xft_Red,*xft_Blue;
extern cairo_pattern_t *cai_White, *cai_Black, *cai_Blue, *cai_Red, *cai_Border;

const uint FN=0, // after resize: follows not
           MR=1, //               window moves right
           FR=2, //               rightside follows
           MEDfont=0,
           BOLDfont=1,
           SMALLfont=2;
const int TDIST=14;  // distance text lines

struct Int2 {
  int x,y;
  Int2(int x,int y);
  Int2();
  void set(int x,int y);
  bool operator!=(Int2 b);
};

struct Pixmap2 {
  Pixmap pm, // pixmap
         cm; // clip map
  Pixmap2();
};

struct Point:XPoint {
  Point();
  Point(short x,short y);
  void set(short x,short y);
  bool operator==(Point b);
  bool operator!=(Point b);
};

struct Rect:XRectangle {
  Rect(short x,short y,ushort dx,ushort dy);
  void set(short x,short y,ushort dx,ushort dy);
};

struct Segment {  // like XSegment
  short x1,y1,x2,y2;
  Segment();
  Segment(short x1,short y1,short x2,short y2);
  void set(short x1,short y1,short x2,short y2);
};

struct Id {
  int id1,id2;
  Id(int,int);
  Id(int);
};

struct Label {
  const char *txt;
  short undersc; // underscore
  void (*draw)(uint win,XftDraw *xft_win,Id id,int par,int y_off);
  Label(const char* t);
  Label(const char* t,int undersc);
  Label(void (*dr)(uint win,XftDraw *xft_win,Id id,int par,int y_off));
  Label(const char *t,void (*dr)(uint win,XftDraw *xft_win,Id id,int par,int y_off));
};

struct Style {
  int st;
  uint bgcol;
  int param;
  Style(int st,uint col,int par);
  void set(int st,uint col,int par);
};

struct SliderStyle {
  int st;
  bool grid;
  SliderStyle(int,bool);
  void set(int st,bool gr);
};

struct WinBase {
  uint pwin,          // parent window
       win;
  WinBase **children;
  XftDraw *xft_win;
  int x, y, dx, dy,
      lst_child,end_child;
  Id id;
  uint rez_mode;
  Rect t_rect;
  const char *title;
  WinBase(uint pw,const char* t,int x,int y,int dx,int dy,uint r_m,Id id);
  void mv_resize(uint w,int ddx,int ddy);
  void hide();
  void map();
  bool is_hidden();
  bool re_parent(uint new_p);
  void add_child(WinBase* child);
  void draw_title(XftDraw *xft_pwin,Rect expose_rect);
  void add_to(WinBase*); // needed when widget may be re_parent'ed
};

struct SubWin:WinBase {  // sub window
  void(*display_cmd)(Rect exp_rect,Id);
  void (*del_cmd)(Id);
  SubWin(const char* title,Rect rect,bool do_map,uint bg,void(*disp_cmd)(Rect,Id),void(*del_cmd)(Id),Id id=0);
  ~SubWin();
};

// background window
struct BgrWin:WinBase {
  uint wincol;
  void (*display_cmd)(Id id);
  void (*down_cmd)(Id id,int x,int y,int but);
  void (*moved_cmd)(Id id,int x,int y,int but);
  void (*up_cmd)(Id id,int x,int y,int but);
  const bool is_graphw;
  cairo_surface_t *surface;
  BgrWin(uint pwin,
         Rect rect,
         uint rez_mode,
         void (*display_cmd)(Id id),
         uint wcol,
         int bwid=1,
         Id id=0);
  BgrWin(uint pwin,
         Rect rect,
         uint rez_mode,
         void (*display_cmd)(Id id),
         void (*down_cmd)(Id id,int,int,int),
         void (*moved_cmd)(Id id,int,int,int),
         void (*up_cmd)(Id id,int,int,int),
         uint wcol,
         Id id=0);
  ~BgrWin();
  void clear();
  void clear(Rect);
  void draw_point(Point);
  void draw_line(Point,Point);
  void draw_lines(Point *points,int len);
  void cai_draw_lines(Point *points,int len,int lwid,cairo_pattern_t *col);
  void draw_segments(Segment *segs,int len);
  void fill_polygon(Point *points,int len);
  void fill_rectangle(Rect rect);
  void fill_circle(int x,int y,int r);
  void fill_triangle(Point pt1,Point pt2,Point pt3);
  void move_contents_h(int delta);  // move contents horizontal
  void set_selected(bool sel,uint bgcol);
  void raise(uint light_col,uint dark_col);  // 3D effect
};

// non-editable text window
struct TextWin:WinBase {
  static const int SMAX=100;    // string length
  int linenr,
      lmax;
  XftColor **xft_textcol;
  char (*textbuf)[SMAX];
  TextWin(uint pw,Rect rect,uint r_m,int lmax,Id id=0);
  ~TextWin();
  void draw_text();
  void add_text(const char*,XftColor *col=xft_Black);
  void print_text(const char*,XftColor *col=xft_Black);
  void reset();
};

// radio buttons
struct RButton {
  int nr;
  Label label;
  XftColor *xft_text_col;
  void (*cmd)(Id id,int nr,int fire);
  RButton(int nr,Label lab);
};

// choice-menu button, dialog-menu button
struct ChMButton {
  int nr;
  XftColor *xft_col;
  const char *label;
  ChMButton();
};

// cmd-menu buttons
struct CmdMButton {
  Id id;
  const char *label;
  void (*cmd)(Id);
  CmdMButton();
};

struct RButWinData {
  int butnr,
      rb_max;
  RButton **but;
  RButWinData();
  int next();
};

struct RButWin:WinBase {
  int y_off;
  bool maybe_z;  // unselect with second click?
  void (*rb_cmd)(Id id,int nr,int fire);
  RButton *act_button,
          *prev_abut;
  RButWinData def_buttons, // default buttons
              *buttons;
  RButWin(WinBase *parent,Rect,uint r_m,const char *t,bool mbz,void(*cmd)(Id id,int nr,int fire),uint bg,Id id=0);
  ~RButWin();
  void mv_resize(uint w,int ddx,int ddy);
  void draw_rbutton(RButton *rb);
  void set_rbut(RButton *rb,int fire); // if fire, then cmd is called
  void set_rbutnr(int nr,int fire,bool do_draw=true);
  void draw_actb();
  int act_rbutnr();
  void del_rbut(RButton *rb);
  void re_label(RButton*,const char* lab);
  RButton *nr2rb(int nr);
  void empty();   // reset and clear
  void no_actb(); // set to no active button
  void set_y_off(int yoff); // set vertical offset, redraw 
  RButton *add_rbut(Label lab,XftColor *col=xft_Black);
  RButton *is_in_rbutton(XButtonEvent *ev);
};

struct ChMenuData {
  int butnr,
      mb_max;
  ChMButton *but;
  ChMenuData();
  void next();
};

struct ChMenu:WinBase {  // choice menu
  int mw_dx,     // width of mbut_win
      mw_x,mw_y; // x and y of mbut_win
  Style style;   // 0: menu window below, 3: at right
  uint mbut_win; // the menu buttens
  XftDraw *xft_mbut_win;
  void (*fill_menu)(Id id);
  void (*cmd)(Id id,ChMButton*);
  ChMenuData mdata,
           *mbuttons;
  ChMButton value;
  ChMenu(int pw,Rect,uint r_m,void (*fill)(Id),void (*cmd)(Id id,ChMButton*),
         const char *title,Style,uint bgcol,Id id=0);
  void cp_value(int nr,const char* label);
  void add_mbut(const char* label,XftColor *col=xft_Black);
  void draw();
  void draw_mbutton(ChMButton *mb);
  void init_mwin();
  void reset();
};

struct CmdMenu:WinBase {  // command-menu
  int mw_dx,     // width of mbut_win
      mw_x,mw_y, // x and y of mbut_win
      butnr,
      mb_max;
  uint mbut_win; // the menu buttons
  XftDraw *xft_mbut_win;
  CmdMButton *mbuttons,
             *act_mbut;
  void (*fill_menu)();
  CmdMenu(int pw,Rect,uint r_m,void (*fill)(),const char *title,uint bgcol);
  void add_mbut(const char* label,void (*cmd)(Id),Id id=0);
  void draw(bool hilight=false);
  void draw_mbutton(CmdMButton *mb);
  void init_mwin();
  void reset();
};

// extern radio button
struct RExtButton:WinBase {
  Style style;
  bool is_act;
  Label label;
  void (*cmd)(Id id,bool is_act);
  RExtButton(int pw,Rect,uint r_m,Label,Id id=0);
  void draw_rxbut();
};

struct ExtRButCtrl {
  int butnr,
      rxb_max;
  RExtButton **but,
              *act_lbut;
  void (*reb_cmd)(Id id,bool is_act);
  ExtRButCtrl(void (*cmd)(Id id,bool is_act));
  ~ExtRButCtrl();
  int next();
  void reset();
  void set_rbut(RExtButton*,bool fire);
  RExtButton *add_extrbut(uint pwin,Rect,uint r_m,Label lab,Id id=0);
};

struct SliderData {
  int value,
      old_val;
  SliderData();
  SliderData(int);
};

// horizontal slider
struct HSlider:WinBase {
  int sdx,
      minv,maxv;
  SliderData def_data,  // default buffer for data
             *d;
  const char
       *lab_left,
       *lab_right;
  char *text;
  SliderStyle style;
  void (*cmd)(Id id,int val,int fire,char*&,bool rel);
  HSlider(WinBase *parent,Rect rect,uint r_m,int minval,int maxval,
          const char* t,const char *ll,const char *lr,void (*cmd)(Id id,int val,int fire,char*&,bool rel),uint bgcol,Id id=0);
  ~HSlider();
  int &value();
  void draw_sliderbar(int x);
  void calc_hslval(int x);
  void draw();
  void set_hsval(int,int fire,bool do_draw=true);
};

// vertical slider
struct VSlider:WinBase {
  int sdy,
      minv,maxv,
      value,old_val;
  const char
       *lab_top,
       *lab_bottom;
  char *text;
  SliderStyle style;
  void (*cmd)(Id id,int val,int fire,char*&,bool rel);
  VSlider(WinBase *parent,Rect rect,uint r_m,int minval,int maxval,
          const char* t,const char *lb,const char *lt,void (*cmd)(Id id,int val,int fire,char*&,bool rel),uint bgcol,Id id=0);
  ~VSlider();
  void draw_sliderbar(int y);
  void calc_vslval(int y);
  void draw();
  void set_vsval(int,bool fire,bool do_draw);
};

// 2-dimensional slider
struct HVSlider:WinBase {
  int x_inset,  // distance active area -> dy
      sdx, sdy; // active area
  Int2 minv,maxv,
       value,old_val;
  const char
       *lab_left,
       *lab_right,
       *lab_top,
       *lab_bottom;
  char *text_x,*text_y;
  SliderStyle style;
  void (*cmd)(Id id,Int2 val,char *&text_x,char *&text_y,bool rel);
  HVSlider(WinBase *parent,Rect rect,int x_ins,uint r_m,
               Int2 minval,Int2 maxval,
               const char* t,const char *ll,const char *lr,const char *lb,const char *lt,
               void (*cmd)(Id id,Int2 val,char*&,char*&,bool rel),uint bgcol,Id id=0);
  ~HVSlider();
  void draw_2dim_slider(Int2 i2);
  void calc_hvslval(Int2 i2);
  void draw();
  void set_hvsval(Int2,bool fire,bool do_draw=true);
};

// push button
struct Button:WinBase {
  bool is_down;
  XftColor *xft_text_col;
  Style style;
  Label label;
  void (*cmd)(Id);
  //virtual void command(Id);
  Button(int pw,Rect,uint r_m,Label lab,void (*cmd)(Id),Id id=0);
  ~Button();
  void draw_button();
};

// check box
struct CheckBox:WinBase {
  bool value;
  const char *label;
  Style style;
  void (*cmd)(Id id,bool val);
  CheckBox(int pw,Rect,uint r_m,uint bgcol,const char* lab,void (*cmd)(Id,bool),Id id=0);
  ~CheckBox();
  void draw();
  void set_cbval(bool,int fire,bool do_draw=true);
};

// horizontal scrollbar
struct HScrollbar:WinBase {
  int range, p0, xpos, wid,
      value;
  Style style;
  const int ssdim; // soft scroll area
  void (*cmd)(Id id,int val,int range,bool repeat_on);
  HScrollbar(int pw,Rect,Style st,uint r_m,int r,void (*cmd)(Id,int,int,bool),Id id=0);
  ~HScrollbar();
  void calc_params(int r);
  void draw_hscrollbar();
  void mv_resize(uint w,int ddx,int ddy);
  void set_range(int range);
  void calc_xpos(int);
  void set_xpos(int);
  void inc_value(bool incr);
  bool in_ss_area(XButtonEvent *ev,bool *dir);
};

// vertical scrollbar
struct VScrollbar:WinBase {
  int range, p0, ypos, height,
      value;
  void (*cmd)(Id id,int val,int range,bool);
  VScrollbar(int pw,Rect rect,uint r_m,int r,void (*cmd)(Id,int,int,bool),Id id=0);
  ~VScrollbar();
  void calc_params(int r);
  void draw_vscrollbar();
  void set_range(int);
  void set_ypos(int);
  void calc_ypos(int);
};

// editable text window
struct EditWin:WinBase {
  int linenr,
      lmax,
      y_off;
  bool editable;
  struct Line **lines;
  void(*cmd)(Id,int,int);
  struct {
    int x,y;
  } cursor;
  EditWin(int pw,Rect,uint r_m,bool edble,void(*cmd)(Id,int ctrl_key,int key),Id id=0);
  ~EditWin();
  void handle_key(KeySym ks);
  void print_line(int vpos);
  void set_y_off(int yoff);
  void set_cursor(int x,int y);
  void unset_cursor();
  void read_file(FILE* in);
  void set_line(char *s,int n);
  void insert_line(const char *s,int n);
  void write_file(FILE *out);
  char *get_text(char *buf,int maxlen);
  void reset();
  void get_info(int* nr_of_lines,int* cursor_ypos,int *nr_chars);
  void insert_string(uchar *s);
};

struct DialogWin:WinBase {
  bool show_menu, // menu window?
       alloc_lab; // allocate menu labels?
  int cursor,
      mw_dx,     // width of mbut_win
      mw_x,mw_y, // x and y of mbut_win
      butnr,     // menu buttons
      mb_max,
      cmd_id;
  ChMButton *but;
  const char *label;
  uint bgcol,
       mwin_mv_mode,
       mbut_win;
  XftDraw *xft_mbut_win;
  struct Line *lin;
  void (*fill_menu)(int id);
  void (*cmd)(const char*);
  DialogWin(uint pw,Rect,const uint r_m,uint bgcol,Id _id=0);
  DialogWin(uint pw,Rect,const uint r_m,bool al_lab,uint bgcol,Id id=0);
  bool handle_key(KeySym ks);
  void print_line();
  void set_cursor(int x);
  void unset_cursor();
  void draw_dialabel();
  void dlabel(const char *s,uint col);
  void ddefault(const char *s,void(*cmd1)(const char*));
  void ddefault(const char *s,void(*cmd1)(const char*),bool show_m,void (*fill)(int mode),int cmdid);
  void copy(const char*);
  void dok();
  void insert_string(uchar *s);
  bool set_menu(int x1);
  void init_mwin();
  void add_mbut(const char* lab); // lab is malloc'd and copied
  void next();
  void reset_mwin();
};

struct Lamp {
  uint pwin;
  Rect rect;
  Pixmap2 pm;
  uint col;
  Lamp(uint pwin,Rect,uint bgcol);
  void draw();
  void lamp_color(uint col);
};

struct XftText {
  uint win,
       fontnr;
  XftDraw *xft_win;
  const char *text;
  Point start;
  Rect rect;
  XftText(uint w,XftDraw *xft_w,const char *txt,Point pt,uint fnt_nr=MEDfont);
  void draw();
};

extern Style button_style,
             ext_rbut_style,
             checkbox_style;

extern SliderStyle slider_style;

void send_uev(int cmd,int param1=0,int param2=0);
void do_atexit();
void x_widgets_log();
void handle_uev(int,int param,int param2);
void set_text(char *&txt,const char *val);
void set_text(char *&txt,const char *fs,int val);
void set_text(char *&txt,const char *fs,int val1,int val2);
void set_text(char *&txt,const char *fs,float val);
void say(const char *form,...);
void err(const char *form,...);
void alert(const char *form,...);
uint calc_color(const char*);
XftColor *xft_calc_color(uchar r,uchar g,uchar b);
void init_xwindows();
WinBase *create_top_window(const char* title,Rect rect,bool resiz,void (*draw)(XftDraw *win,Rect exp_rect),uint bgcol);
void map_top_window();
void run_xwindows();
uint bit2pix(const char *bits,int dx,int dy);
void set_icon(uint win,uint pm,int dx,int dy);  // pm = pixmap
void set_icon(uint win,const char *bits,int dx,int dy,uint bgcol); // bits = bitmap
void flush_X();
void delete_window(uint &win);
void hide_window(uint win);
void map_window(uint win);
void xft_draw_string(XftDraw *xft_win,XftColor *col,Point pt,const char* s,uint font=MEDfont);
void draw_form_string(uint win,uint col,Point pt,const char *s);
void draw_line(uint win,int width,uint col,Point pt1,Point pt2);
void draw_circle(uint win,int width,uint col,Point mid,int radius);
void clear(uint win);
void clear(uint win,Rect);
void fill_polygon(uint win,uint col,Point *points,int len);
void fill_rectangle(uint win,uint col,Rect rect);
void fill_triangle(uint win,uint col,Point pt1,Point pt2,Point pt3);
void fill_circle(uint win,int width,uint col,Point mid,int radius);
void set_cursor(uint win,uint cursor);
void reset_cursor(uint win);
uint create_cursor(const uchar *bm,const uchar *clip_bm,int dx,int dy,Point hot,bool invert);
void set_custom_cursor(uint win,uint cursor);
bool textcursor_active();   // somewhere a cursor set?
void set_width(int wid);    // for def_gc
void set_color(uint col);
void set_width_color(int wid,uint col);
int xft_text_width(const char *s);
Pixmap2 create_pixmap(const char* pm[]);
void draw_pixmap(uint win,Point pt,Pixmap pm,int dx,int dy);  // without clipping
void draw_pixmap(uint win,Point pt,Pixmap2 pm,int dx,int dy); // with clipping
cairo_surface_t *cai_get_surface(BgrWin *bgr);
bool a_in_b(Rect a,Rect b); // a and b overlap?
