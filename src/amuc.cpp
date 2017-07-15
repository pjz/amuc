#include <X11/keysym.h> // for keyboard symbols
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>

#include "bitmaps.h"
#include "amuc-headers.h"
#include "snd-interface.h"

const int
  sect_scv=40,       // initial scoreview sections in ScLine
  sect_dmax=150,     // max drawable sections
  wavef_max=100,     // max wave sample files
  sect_mus_max=66,   // max sections in music ScLine
  sclin_dist=4,      // line distance
  p_sclin_dist=3,    // line distance, draw_mode ePiano
  y_off=2,           // y offset of score lines
  p_y_off=-48,       // idem if draw_mode = ePiano
  s_wid=14,          // width of signs window
  sced_wid=56,       // score edit window 2
  sect_length=6,
  nop=-99,
  eo_arr=-98,        // end of array
  scrollbar_wid=8,   // scrollbars
  view_vmax=660,     // total hight
  view_hmax=650,     // total width
  mnr_height=12,     // measure nr view 
  scview_vmax=3+y_off+mnr_height+sclin_max*sclin_dist+scrollbar_wid, // height of score view
  tnview_height=50,  // musicview tune names
  rbutview_left=247, // distance left side of radio-button views and right side
  but_dist=18,       // button distance
  slider_height=10+TDIST, // horizontal slider height
  ictrl_height=126,       // instrument control view
  score_info_len=81,      // at right side of scores
  checkbox_height=15,     // checkbox height
  scview_wid=s_wid+sect_scv*sect_length, // score part in score view
  mn_max=1<<mn_bits,      // multiple note buffer
  tn_max=1<<tn_bits,      // tune names
  mn_end_start=mn_max/2,  // start value of mn_end (mn_tunes_end starts at 0)
  ptr_dy=1,               // pointer y offset
  eSco=0,eScr=1,eMnr=2,eClip=2,eCdir=3; // for InfoView

                          // colors.h: eBlack, eRed, eBlue, eGreen, ePurple, eBrown
const char *const color_name[colors_max]= { "black","red","blue","green","purple","brown" };

const char
  *version="1.7 (dec 2, 2008)",
  *help_browser="epiphany";

Style def_but_st(0,0,0), // default button style, set after init_xwindows()
      def_erb_st(1,0,0); // default extern radio button style
SliderStyle def_sl_st(1,true);  // default slider style

enum {
  ePlayNote=2,
  eScore_start1,
  eGlobal,
  eScore_start2, // <- used after dec 2005
  eScore_start3, // <- used after jan 2007
  eScore_start4, // <- used after april 2007

  eAdd, eTake, eTake_nc,  // read script

  eText,     // ScInfo tags
  eTempo,
  eMidiInstr, eMidiPerc,
  ePurple_a_harm, ePurple_s_harm, ePurple_attack, ePurple_tone, ePurple_decay,
  eRed_att_timbre, eRed_timbre, eRed_attack, eRed_decay, eRed_stamp, eRed_durlim, eRed_tone,
  eGreen_timbre1, eGreen_timbre2, eGreen_attack, eGreen_decay, eGreen_fmul, eGreen_chorus,
  eBlack_fm, eBlack_attack, eBlack_decay, eBlack_detune, eBlack_mmod,
  eBlue_attack, eBlue_decay, eBlue_durlim, eBlue_chorus, eBlue_rich, eBlue_lowp,
  eBrown_fm, eBrown_attack, eBrown_decay, eBrown_detune, eBrown_mmod,
  eRevb, 
  eLoc, eSLoc,
  eIsa, eIsa_gr, eMsIsa,
  eWaveF,ePhysM,
  eAmpl,eWaveAmpl,eAmpl_gr,
  eCtrPitch,
  eSynth,
  eMode,
  eBaseFreq,

  eMusic, eScorebuf,           // score type

  eColorValue, eTimingValue, eAccValue, ePiano, // section drawing mode

  eIdle, eTracking, eErasing,  // mouse states
  eMoving, eMoving_hor, eMoving_vert,
  eCopying, eCopying_hor, eCopying_vert, 
  eCollectSel,eCollectColSel,
  ePortaStart,
  eToBeReset,

  eMidiDev, eMidiJack       // midi keyboard input mode
};
enum { eSet, eAdd_Take };      // modify script

static uint
  cPurple, cGreen, cLightGreen, cBrown, cLightGrey, cDarkGrey,
  cBgrGrey, cLightBlue, cScopeBgr;
static XftColor *xft_Purple,*xft_Green,*xft_Brown,*xft_Grey;
bool debug,
     mk_connected,
     no_def_tunes,
     extended,
     i_am_playing,
     wf_playing,
     Xlog;
WinBase *top_win;
int output_port=eAlsa;
static int midi_input_mode=eMidiDev;
pthread_t thread2,  // usb midi keyboard
          thread3;  // listen to wave file
const char *amuc_data="/usr/share/amuc",
           *wave_samples="/usr/share/amuc/samples",
           *cur_dir=".";
ShortBuffer wl_buf;  // filled by read_wav()
JackInterf *wfile_play; // to play a wavefile
App *app;
struct AppLocal *app_local;

const bool make_higher[]={ true,false,false,false,true,false,false }, // increase lnr?
           make_lower[]={ false,false,false,true,false,false,true };  // decrease lnr?

const int signs[keys_max][7]= {
//  B   A   G   F   E   D   C
  { 0  ,0  ,0  ,0  ,0  ,0  ,0 },    // C
  { eLo,eLo,eLo,0  ,eLo,eLo,0 },    // Des
  { 0  ,0  ,0  ,eHi,0  ,0  ,eHi },  // D
  { eLo,eLo,0  ,0  ,eLo,0  ,0 },    // Es
  { 0  ,0  ,eHi,eHi,0  ,eHi,eHi },  // E
  { eLo,0  ,0  ,0  ,0  ,0  ,0 },    // F
  { 0  ,eHi,eHi,eHi,eHi,eHi,eHi },  // Fis
  { eLo,eLo,eLo,eLo,eLo,eLo,0 },    // Ges (= Fis)
  { 0  ,0  ,0  ,eHi,0  ,0  ,0 },    // G
  { eLo,eLo,0  ,0  ,eLo,eLo,0 },    // As
  { 0  ,0  ,eHi,eHi,0  ,0  ,eHi },  // A
  { eLo,0  ,0  ,0  ,eLo,0  ,0 },    // Bes
  { 0  ,eHi,eHi,eHi,0  ,eHi,eHi }   // B
};        
                               // C Des D   Es  E   F   Fis Ges G   As  A   Bes B
const int signsMode[keys_max] = { 0,eLo,eHi,eLo,eHi,eLo,eHi,eLo,eHi,eLo,eHi,eLo,eHi };

// ======= forward declarations =======

void menu_cmd(Id);
void button_cmd(Id);
void slider_cmd(Id id,int val,int fire,char *&text,bool rel);
void hvslider_cmd(Id id,Int2 val,char *&text_x,char *&text_y,bool rel);
void exp_rbutton_cmd(Id,bool);
ScoreView* the_sv(int ind);
MusicView* the_mv();

// ====================================

int min(int a,int b) { return a<=b ? a : b; }
int max(int a,int b) { return a>=b ? a : b; }
int minmax(int a, int x, int b) { return x>=b ? b : x<=a ? a : x; }

int color_nr(const char *coln,bool *ok) {
  if (ok) *ok=true;
  for (int i=0;i<colors_max;++i)
    if (!strcmp(color_name[i],coln)) return i;
  if (ok) *ok=false;
  else alert("unknown color %s",coln);
  return eBlack;
}

// ======= declarations ========

struct MultiNoteBuf {
  ScSection buf[mn_max];
  int mn_end,        // index of mn_buf[] (for complete score)
      mn_tunes_end;  // index of mn_buf[] (for tunes)
  MultiNoteBuf():
      mn_end(mn_end_start),
      mn_tunes_end(0) {
  }
  int new_section(int sctype) {
    int& end= sctype==eMusic ? mn_end : mn_tunes_end;
    const int max= sctype==eMusic ? mn_max : mn_end_start;
    if (end==max-1) { alert("> %d multi notes",max); return 0; }
    return ++end;
  }
  void reset(int typ) {
    if (typ==eMusic) {
      for (int n=mn_end_start;n<=mn_end;++n) buf[n].reset();
      mn_end=mn_end_start;
    }
    else {
      for (int n=0;n<=mn_tunes_end;++n) buf[n].reset();
      mn_tunes_end=0;
    }
  }
} mn_buf;

struct TuneNames {
  int dim,
      lst_ind;
  const char **buf;
  TuneNames():
    dim(50),
    lst_ind(-1),
    buf(new const char*[dim]) {
  }
  void reset() { lst_ind=-1; }
  int add(const char* item) {
    if (lst_ind==dim-1) buf=re_alloc<const char*>(buf,dim);
    buf[++lst_ind]=strndup(item,50);
    return lst_ind;
  }
  const char *txt(int ind) {
    return ind<0 || ind>lst_ind ? "?" : buf[ind];
  }
  void replace(int ind,const char* n_name) {
    if (ind<0 || ind>lst_ind)
      alert("TN::replace: ind=%d?",ind);
    else
      buf[ind]=strndup(n_name,50);
  }
} tnames;

const char* ind2tname(int ind) { return tnames.txt(ind); }

struct SectData {
  int snr;
  uchar lnr,sign;
  SectData(int _lnr,int _snr,int _sign):
    snr(_snr),lnr(_lnr),sign(_sign) { }
  bool operator==(SectData &sd) { return lnr==sd.lnr && snr==sd.snr; }
  bool operator<(SectData &sd) {  // sign is irrelevant
    return snr==sd.snr ? lnr<sd.lnr : snr<sd.snr;
  }
};

struct MidiData {
  Array<short,colors_max> n6;
  int gnr;  // group nr
};

struct ScInfo {
  struct IData {
    char col;
    short d0,d1,d2,d3;
    bool b;
  };
  uint tag;
  union {
    const char *text;
    bool b;
    IData idata;
    short n5[5];
    int n;
    MSynthData ms_data;
    MidiData midi_data;
  };
  ScInfo *next;
  ShortBuffer wavedata;
  ScInfo():tag(0),next(0) { }
  ScInfo(uint t):tag(t),next(0) { }
  ~ScInfo() { delete next; }
  void add(ScInfo& info) {
    ScInfo *sci;
    if (!tag) *this=info;
    else {
      for (sci=this;sci->next;sci=sci->next);
      sci->next=new ScInfo(info);
    }
  }
  void reset() {
    delete next;
    tag=0; next=0; wavedata.reset();
  }
};

struct ExSample {
  ScInfo *info;
  int fsel_butnr; // button nr PhmCtrl::file_select
  ExSample():info(0) { }
};

struct Scores {
  int dim,
      lst_score;
  Score **buf;
  Scores():
      dim(50),
      lst_score(-1),
      buf(new Score*[dim]) {
  }
  Score *new_score(const char* nam) {
    if (lst_score==dim-1)
      buf=re_alloc<Score*>(buf,dim);
    return (buf[++lst_score]=new Score(nam,sect_scv,0));
  }
  Score *exist_name(const char *nam);
  void swap(int ind1,int ind2);
  void remove(Score*);
  void reset() {
    for (int n=0;n<=lst_score;++n) {
      delete buf[n]; buf[n]=0;
    }
    lst_score=-1;
  }
  int get_index(Score *sc) {
    for (int i=0;i<dim;++i)
      if (buf[i]==sc) return i;
    alert("Scores::get_ind: sc unknown");
    return -1;
  }
  Score* operator[](int ind) {
    if (ind<0 || ind>lst_score) {
      alert("Scores::[] ind=%d?",ind);
      return buf[0];
    }
    return buf[ind];
  }
} scores;

int f_or_s_to_keynr(int flatn,int sharpn) { // for backwards compatibility
  int f,s;
  for (int knr=0;knr<keys_max;++knr) {
    f=s=0;
    for (int i=0;;++i) {
      uint sign=signs[knr][i];
      if (sign==eHi) s |= 1;
      else if (sign==eLo) f |= 1;
      if (i==6) break;
      f<<=1; s<<=1;
    }
    if (f==flatn && s==sharpn) return knr;
  }
  return 0;
}

struct Encode {
  Str save_buf;
  char textbuf[100];
  int age; // backwards compatibility
  char* set_meter(int m) {
    sprintf(save_buf.s,"%um%d",eGlobal,m);
    return save_buf.s;
  }
  char* score_start4(int score_nr,
                     int meter,int endsect,int score_gr,int sc_keynr) {
    sprintf(save_buf.s,"%um%de%dg%dk%d",
      eScore_start4,meter,endsect,score_gr,sc_keynr);
    return save_buf.s;
  }
  char* play_note(int lnr,int snr,int dur,int sign,int stacc_sampled,
                  int col,int gnr,int dlnr,int dsnr,int del_s,int del_e) {
    sprintf(save_buf.s,"%uL%dN%dd%di%ds%dc%dg%d ",ePlayNote,lnr,snr,dur,sign,stacc_sampled,col,gnr);
    char *sb_end=save_buf.s+strlen(save_buf.s);
    if (del_s || del_e) sprintf(sb_end-1,"p%d,%dD%d,%d ",dlnr,dsnr,del_s,del_e);
    else if (dlnr) sprintf(sb_end-1,"p%d,%d ",dlnr,dsnr);
    return save_buf.s;
  }
  bool decode(FILE *in,Score*& scp,bool set_meter_val) {
    int res;
    uint opc=0;
    opc=atoi(save_buf.s);
    switch (opc) {
      case eGlobal: {
          int mtr;
          if (sscanf(save_buf.s,"%*um%d",&mtr)!=1) {
            alert("bad code: %s",save_buf.s); return false;
          }
          if (set_meter_val) app->act_meter=mtr;
        }
        break;
      case eScore_start2: {
          age=1;
          int flatn,sharpn,
              dummy;
          scp=scores.new_score(0); if (!scp) return false;
          res=sscanf(save_buf.s,"%*uc%da%*df%ds%dm%de%dg%d",
                     &dummy,&flatn,&sharpn,&scp->sc_meter,
                     &scp->end_sect,&scp->ngroup);
          if (res==5 || res==6) {
            if (res==6) age=2;
            if (scp->end_sect)
              scp->check_len(scp->end_sect+1);
            scp->sc_key_nr=f_or_s_to_keynr(flatn,sharpn);
            scp->tone2signs(scp->sc_key_nr);
          }
          else {
            alert("bad code: %s",save_buf.s);
            return false;
          };
          if (fscanf(in,"%50s",textbuf)==1)
            scp->name=tnames.add(textbuf);
        }
        break;
      case eScore_start3: {
          age=3;
          scp=scores.new_score(0); if (!scp) return false;
          res=sscanf(save_buf.s,"%*ua%*dm%de%dg%dk%d",
                     &scp->sc_meter,
                     &scp->end_sect,&scp->ngroup,&scp->sc_key_nr);
          if (res==4) {
            if (scp->end_sect)
              scp->check_len(scp->end_sect+1);
            scp->tone2signs(scp->sc_key_nr);
          }
          else {
            alert("bad code: %s",save_buf.s);
            return false;
          };
          if (fscanf(in," \"%50[^\"]s",textbuf)==1)
            scp->name=tnames.add(textbuf);
          getc(in); // skip '"'
        }
        break;
      case eScore_start4: {
          age=4;
          scp=scores.new_score(0); if (!scp) return false;
          res=sscanf(save_buf.s,"%*um%de%dg%dk%d",
                     &scp->sc_meter,
                     &scp->end_sect,&scp->ngroup,&scp->sc_key_nr);
          if (res==4) {
            if (scp->end_sect)
              scp->check_len(scp->end_sect+1);
            scp->tone2signs(scp->sc_key_nr);
          }
          else {
            alert("bad code: %s",save_buf.s);
            return false;
          };
          if (fscanf(in," \"%50[^\"]s",textbuf)==1)
            scp->name=tnames.add(textbuf);
          getc(in); // skip '"'
        }
        break;
      case ePlayNote: {
          int ind,snr,dur,sign,stacc_sampl,col,gnr=0,dlnr=0,dsnr=0,del_s=0,del_e=0;
          if (age==1) {
            res=sscanf(save_buf.s,"%*uL%dN%dd%di%ds%dc%da%*dp%d,%dD%d,%d",
                       &ind,&snr,&dur,&sign,&stacc_sampl,&col,&dlnr,&dsnr,&del_s,&del_e);
          }
          else if (age==3 || age==2) {
            res=sscanf(save_buf.s,"%*uL%dN%dd%di%ds%dc%da%*dg%dp%d,%dD%d,%d",
                       &ind,&snr,&dur,&sign,&stacc_sampl,&col,&gnr,&dlnr,&dsnr,&del_s,&del_e);
          }
          else if (age==4) {
            res=sscanf(save_buf.s,"%*uL%dN%dd%di%ds%dc%dg%dp%d,%dD%d,%d",
                       &ind,&snr,&dur,&sign,&stacc_sampl,&col,&gnr,&dlnr,&dsnr,&del_s,&del_e);
          }
          else {
            alert("age=%d",age);
            return false;
          }
          if (res<6 || res>11) {
            alert("bad code: %s",save_buf.s); return false;
          }
          scp->check_len(snr+dur); // 1 extra
          ScSection *start=scp->get_section(ind,snr),
                    *sec;
          for (int i=0;;) {
            sec=start;
            if (sec->cat==ePlay)   // multi note?
              sec=start->get_the_section(0,0);
            if (i==0) sec->del_start=del_s;
            sec->cat=ePlay;
            sec->s_col=col;
            sec->sign=sign;
            sec->stacc=0;
            sec->sampled=0;
            sec->s_group=gnr;
            if (++i==dur) break;
            ++start;
          }
          sec->stacc= stacc_sampl & 1;
          sec->sampled= (stacc_sampl>>1) & 1;
          sec->port_dlnr=dlnr;
          sec->port_dsnr=dsnr;
          sec->del_end=del_e;
        }
        break;
      default:
        if (opc==eScore_start1) alert("old score file?");
        else alert("unknown code %d",opc);
        return false;
    }
    return true;
  }
};

struct ScoreViewBase {
  int draw_mode,
      play_start,
      play_stop,
      leftside,   // left side of window, set by scrollbar
      piano_y_off,// y offset if draw_mode = ePiano
      prev_drawmode, // previous draw_mode
      sect_len,
      scale_dep,     // ePiano mode: shift lines depending on chordswin key or score key
      key;           // pressed key
  bool zoomed,
       keep_m_action; // shift key pressed?
  const int sv_type;  // 0 or eMusic
  uint p_scline_col[24];  // ePiano mode: line colors
  Score *score;
  BgrWin *scview,   // score
         *tnview,   // tune names
         *mnrview;  // measure numbers
  HScrollbar *scroll;
  ScoreViewBase(Rect rect,int typ):
      draw_mode(eColorValue),
      play_start(0),play_stop(-1),
      leftside(0),
      piano_y_off(y_off),
      prev_drawmode(0),
      sect_len(sect_length),
      scale_dep(eScDep_no),
      key(0),
      zoomed(false),
      keep_m_action(false),
      sv_type(typ),
      score(0),
      tnview(0) {
    static bool ds[12]={ 1,0,1,0,1,1,0,1,0,1,0,1 };
    p_set_scline_col(ds,false,false);
  }
  void mouseDown_mnr(int x,int y,int button);
  int sectnr(int x) {
    return (x+leftside*sect_len)/sect_len;
  }
  void p_set_scline_col(bool *b,bool gt_12,bool shift_12) {  // gt_12: chord>7 ?
    if (gt_12)
      for (int i=0;i<24;++i) {
        if (shift_12)
          p_scline_col[23-i]= b[i] ? i==0 ? cBlue : i>12 ? cGreen : cGrey : 0;
        else
          p_scline_col[23-i]= b[(i+12)%24] ? i==12 ? cBlue : i>12 ? cGrey : cGreen : 0;
      }
    else
      for (int i=0;i<12;++i)
        p_scline_col[11-i]=p_scline_col[23-i]= b[i] ? (i==0||i==12 ? cBlue : cGrey) : 0;
  }
  void draw_sc2(bool clear,int delta=0);
  void draw_start_stop(Score *score);
  void enter_start_stop(Score *score,int snr,int mouse_but);
  virtual void draw_endline(bool draw_it);
  void draw_meter_nrs(Score*);
  void draw_scview(int start_snr,int stop_snr,int meter);
  void p_draw_scview(int start_snr,int stop_snr,int meter);
  uint p_lnr2col(int lnr,int sign) {
    static int nr0=lnr_to_midinr(0,0);
    int diff= scale_dep==eScDep_lk ? keynr2ind(app->chordsWin->the_key_nr) :
              scale_dep==eScDep_sk ? keynr2ind(score->sc_key_nr) :
              0;
    return p_scline_col[(nr0 - lnr_to_midinr(lnr,sign) + diff)%24];
  }
  void draw_info(ScSection *sec,bool is_mus) {
    const char *const acc_name[3]= { "","sign=sharp","sign=flat" };
    char buffer[150],
         *p;
    alert("note info:");
    for(;sec;sec=sec->nxt()) {
      p=buffer;
      p+=sprintf(p,"   color=%s #90group=%d ",color_name[sec->s_col],sec->s_group);
      if (sec->sampled) {
        p+=sprintf(p,"#140(sampled)#200%s ",acc_name[sec->sign]);
      }
      else if (sec->stacc)
        p+=sprintf(p,"#140%s #210stacc ",acc_name[sec->sign]);
      else
        p+=sprintf(p,"#140%s ",acc_name[sec->sign]);
      if (is_mus)
        sprintf(p,"#250src=%s",sec->src_tune < tn_max-1 ? tnames.txt(sec->src_tune) : "?");
      alert(buffer);
    }
  }
};

int ypos(int lnr) {   // linenr -> y
  return y_off + sclin_dist * lnr;
}

int ypos(int lnr,int sign,int y_offset) {   // supposed: draw_mode = ePiano
  static int diff=lnr_to_midinr(0,0);
  return (diff-lnr_to_midinr(lnr,sign)) * p_sclin_dist + y_offset;
}

struct TunesView {
  VScrollbar *scroll;
  RButWin *rbutwin;
  TunesView(Rect rect) {
    rect.width-=scrollbar_wid;
    rbutwin=new RButWin(top_win,rect,MR,"tunes",false,tune_cmd,cBackground);
    Score* sc;
    for (int n=0;n<=scores.lst_score;++n) {
      sc=scores[n];
      sc->rbut=rbutwin->add_rbut(tnames.txt(sc->name));
    }
    if (scores.lst_score>=0)
      app->act_tune=scores[0];
    scroll=new VScrollbar(top_win->win,Rect(rect.x+rect.width,rect.y,0,rect.height),
                          MR,(scores.lst_score+2)*TDIST,scr_cmd);
  }
  static void tune_cmd(Id,int nr,int fire);
  static void scr_cmd(Id id,int val,int,bool);
  void set_scroll_range() { scroll->set_range((scores.lst_score+2)*TDIST); }
};

struct MeterView {
  HSlider *meter;
  static const int meter_dim=6;
  Array<int,meter_dim> m;
  MeterView(Rect rect) {
    m=(int[meter_dim]){ 6,8,12,16,24,32 };
    meter=new HSlider(top_win,rect,MR,0,meter_dim-1,"meter",0,0,meter_cmd,cBackground);
    draw(app->act_meter);
  }
  void draw(int val) {
    int i=m.get_index(val);
    if (i>=0) {
      set_text(meter->text,"%d",val);
      meter->set_hsval(i,0);
      return;
    }
    alert("warning: non-standard meter value %d",val);
    set_text(meter->text,"%d",val);
    meter->set_hsval(-1,0);
  }
  static void meter_cmd(Id,int val,int,char*&,bool);
};

struct TempoView {
  HSlider *tempo;
  TempoView(Rect rect) {
    tempo=new HSlider(top_win,rect,MR,6,16,"tempo",0,0,tempo_cmd,cBackground);
    tempo->value()=11;
    set_text(tempo->text,"110");
  }
  static void tempo_cmd(Id,int val,int,char *&txt,bool) {
    app->act_tempo=val;
    set_text(txt,"%d",10*val);
  }
}; 

uint col2color(int col) {
  switch (col) {
    case eBlack:  return cBlack;
    case ePurple: return cPurple;
    case eRed:    return cRed;
    case eBlue:   return cBlue;
    case eGreen:  return cGreen;
    case eBrown:  return cBrown;
    case eGrey:   return cGrey;
    default:
      alert("col2color: %d?",col);
      return cBlack;
  }
}

XftColor *xft_col2color(int col) {
  switch (col) {
    case eBlack:  return xft_Black;
    case ePurple: return xft_Purple;
    case eRed:    return xft_Red;
    case eBlue:   return xft_Blue;
    case eGreen:  return xft_Green;
    case eBrown:  return xft_Brown;
    case eGrey:   return xft_Grey;
    default:
      alert("xft_col2color: %d?",col);
      return xft_Black;
  }
}

PhmCtrl *col2phm_ctrl(int col) {
  switch (col) {
    case eBlack: return app->black_phm_control;
    case eRed: return app->red_phm_control;
    case eGreen: return app->green_phm_control;
    case eBlue: return app->blue_phm_control;
    case eBrown: return app->brown_phm_control;
    case ePurple: return app->purple_phm_control;
    default: alert("col2phm_ctrl: col=%d",col); return app->black_phm_control;
  }
}

CtrlBase *col2ctrl(int col) {
  switch (col) {
    case eBlack: return app->black_control;
    case eRed: return app->red_control;
    case eGreen: return app->green_control;
    case eBlue: return app->blue_control;
    case eBrown: return app->brown_control;
    case ePurple: return app->purple_control;
    default: alert("col2ctrl: col=%d",col); return app->black_control;
  }
}

FMCtrl *col2FMctrl(int col) {
  switch (col) {
    case eBlack: return app->black_control;
    case eBrown: return app->brown_control;
    default: alert("col2FMctrl: col=%d",col); return app->black_control;
  }
}

struct ColorView {
  RButWin *rbutwin;
  ColorView(Rect rect) {
    rbutwin=new RButWin(top_win,rect,MR,"colors",false,color_cmd,cBackground);
    for (int n=0;n<colors_max;n++)
      rbutwin->add_rbut(color_name[n],xft_col2color(n));
  }
  static void color_cmd(Id id,int nr,int fire);
};

struct Question {
  BgrWin *bgwin;
  Question(Rect rect) {
    bgwin=new BgrWin(top_win->win,rect,MR,0,cForeground);
    new Button(bgwin->win,Rect(2,TDIST,20,0),FN,"ok",button_cmd,Id('ok'));
    app->dia_wd=new DialogWin(bgwin->win,Rect(24,TDIST,rect.width-28,0),FN,true,cForeground);
    set_custom_cursor(app->dia_wd->win,text_cursor);
    app->dia_wd->show_menu=false;
  }
};

void arrow_up(uint win,XftDraw*,Id id,int,int) {
  const int x=8,y=10;
  static Point pts_u[3]={ Point(x,y-7),Point(x-4,y),Point(x+4,y) };
  fill_polygon(win,cBlack,pts_u,3);
}

void arrow_down(uint win,XftDraw*,Id id,int,int) {
  const int x=8,y=10;
  static Point pts_d[3]={ Point(x,y),Point(x-4,y-6),Point(x+4,y-6) };
  fill_polygon(win,cBlack,pts_d,3);
}

void cross_sign(uint win,XftDraw*,Id id,int,int) {
  const int x=5,y=4;
  draw_line(win,2,cBlack,Point(x,y),Point(x+7,y+6));
  draw_line(win,2,cBlack,Point(x,y+6),Point(x+7,y));
}

struct MouseAction {
  ExtRButCtrl *ctrl;
  RExtButton *but_all,
             *but_all_col,
             *but_move,
             *but_copy,
             *but_select,
             *but_multi,
             *but_ninf,
             *but_porta,
             *but_flat,*but_sharp,*but_normal;
  VSlider *semi_tones;
  Button *unselect;
  MouseAction() {
    ctrl=new ExtRButCtrl(exp_rbutton_cmd);
    Rect rect2(2,4,42,0);
    but_select=ctrl->add_extrbut(top_win->win,rect2,FN,Label("select",0),Id('sel'));
    rect2.y+=but_dist; rect2.width=55;
    ctrl->add_extrbut(top_win->win,rect2,FN,"sel color",Id('scol'));
    rect2.y+=but_dist;
    but_all=ctrl->add_extrbut(top_win->win,rect2,FN,Label("sel all \273",4),Id('all'));
    rect2.y+=but_dist;
    but_all_col=ctrl->add_extrbut(top_win->win,rect2,FN,"sel col \273",Id('allc'));
    rect2.y+=but_dist; rect2.width=42;
    but_move=ctrl->add_extrbut(top_win->win,rect2,FN,Label("move",0),Id('move'));
    rect2.y+=but_dist;
    but_copy=ctrl->add_extrbut(top_win->win,rect2,FN,Label("copy",0),Id('copy'));
    rect2.y+=but_dist; rect2.width=55;
    but_porta=ctrl->add_extrbut(top_win->win,rect2,FN,Label("portando",0),Id('prta'));
    rect2.y+=but_dist;
    but_multi=ctrl->add_extrbut(top_win->win,rect2,FN,Label("multinote",5),Id('muln'));
    rect2.y+=but_dist; rect2.width=42;
    but_sharp=ctrl->add_extrbut(top_win->win,rect2,FN,"sharp",Id('up'));
    rect2.y+=but_dist;
    but_flat=ctrl->add_extrbut(top_win->win,rect2,FN,"flat",Id('do'));
    rect2.y+=but_dist; rect2.width=45;
    but_normal=ctrl->add_extrbut(top_win->win,rect2,FN,"normal",Id('left'));
    rect2.y+=but_dist; rect2.width=55;
    but_ninf=ctrl->add_extrbut(top_win->win,rect2,FN,Label("note info",5),Id('ninf'));
    rect2.y+=but_dist;
    unselect=new Button(top_win->win,rect2,FN,"unselect",button_cmd,Id('uns'));
    rect2.y+=but_dist;
    new Button(top_win->win,rect2,FN,"re-color",button_cmd,Id('rcol'));
    rect2.y+=but_dist; rect2.width=42;
    new Button(top_win->win,rect2,FN,"delete",button_cmd,Id('del '));
    rect2.y+=but_dist+20; rect2.width=55;
    semi_tones=new VSlider(top_win,Rect(2,rect2.y,55,74),FN,1,12,"shift/cp"," 1","12",slider_cmd,cBackground,Id('semt'));
            // 74 = 11 * 6 + 8
    semi_tones->value=6;
    set_text(semi_tones->text," 6 semi t.");
    int y1=rect2.y+78;
    button_style.set(1,cBackground,0);
    new Button(top_win->win,Rect(1,y1,14,0),FN,arrow_up,button_cmd,Id('shmu'));                  // shift move up
    new Button(top_win->win,Rect(15,y1,40,0),FN,Label("shift",arrow_down),button_cmd,Id('shmd'));// shift move down
    y1+=but_dist;
    new Button(top_win->win,Rect(1,y1,14,0),FN,arrow_up,button_cmd,Id('shcu'));                  // copy move up
    new Button(top_win->win,Rect(15,y1,40,0),FN,Label("copy",arrow_down),button_cmd,Id('shcd')); // copy move down
    button_style=def_but_st;
  }
  void reset() {
    ctrl->reset();
    app->act_action=0;
  }
};

struct EditScript {
  EditWin *textview,
          *meas_info;
  VScrollbar *scroll;
  EditScript(Rect rect) {
    meas_info=new EditWin(
      top_win->win,Rect(rect.x,rect.y,26,rect.height),FN,false,0);
    scroll=new VScrollbar(
      top_win->win,Rect(rect.x+27,rect.y,scrollbar_wid,rect.height),FN,rect.height,scr_cmd);
    textview=new EditWin(
      top_win->win,Rect(rect.x+scrollbar_wid+28,rect.y,rect.width-scrollbar_wid-28,rect.height),FR,true,key_cmd);
    set_custom_cursor(textview->win,text_cursor);
  }
  static void key_cmd(Id,int ctrl_key,int key);
  static void scr_cmd(Id,int yoff,int range,bool);
  void write_scriptf(FILE*);
  void save_scriptf(FILE*);
  void print_meas(int pos,int snr) {
    Str str;
    meas_info->set_line(str.tos(snr/app->act_meter),pos);
  }
  void reset_sbar() {
    int nr_lines;
    textview->get_info(&nr_lines,0,0);
    scroll->value=0;
    scroll->set_range((nr_lines+4)*TDIST);
    meas_info->set_y_off(0);
  }
};

const char
  *maj_min_keys[keys_max*4] = {
   "C","Db","D","Eb","E","F","F#","Gb","G","Ab","A","Bb","B",     // preferred
   "a","bb","b","c","c#","d","d#","eb","e","f","f#","g","g#",
   "C","Des","D","Es","E","F","Fis","Ges","G","As","A","Bes","B", // classic
   "a","bes","b","c","cis","d","dis","es","e","f","fis","g","gis"
  };


struct ReverbView {
  uint sel_col;
  BgrWin *bgwin;
  HVSlider *reverb;
  ReverbView(Rect rect):sel_col(eGreen) {
    bgwin=new BgrWin(top_win->win,rect,MR,draw,cForeground);
    
    reverb=new HVSlider(bgwin,Rect(4,TDIST,66,38),12,FN,Int2(0,0),Int2(4,2),
                        "reverb/ph","0","4","0","2",hvslider_cmd,cForeground,Id('rval'));
    reverb->value.set(0,0);

    button_style.set(0,cForeground,1);
    new Button(bgwin->win,Rect(5,51,38,0),FN,"instr?",button_cmd,Id('revc'));
    button_style=def_but_st;
  }
  static void draw(Id);
};

struct Menus {
  BgrWin *bgwin;
  CmdMenu *file,
          *imp_export,
          *help;
  Menus(Rect rect) {
    bgwin=new BgrWin(top_win->win,rect,MR,0,cForeground);
    file=new CmdMenu(bgwin->win,Rect(6,3,0,0),FN,fill_file_menu,"File",cForeground);
    imp_export=new CmdMenu(bgwin->win,Rect(68,3,0,0),FN,fill_imp_exp_menu,"Import/Export",cForeground);
    help=new CmdMenu(bgwin->win,Rect(172,3,0,0),FN,fill_help_menu,"Help",cForeground);
  }
  static void fill_file_menu(),
              fill_imp_exp_menu(),
              fill_help_menu();
};

struct AppLocal {
  Array<ScoreView*,2>scViews;
  Menus *menus;
  MusicView *musicView;
  EditScript *editScript;
  TunesView *tunesView;
  MeterView *meterView;
  TempoView *tempoView;
  ColorView *colorView;
  Question *textView;
  MouseAction *mouseAction;
  struct InfoView *info_view;
  ReverbView *revb_view;
  HVSlider *adj;
  RButWin *mv_display;
  static void mv_display_cmd(Id id,int nr,int fire);
  AppLocal();
  static void remove_cmd(Id);
  static void clear_cmd(Id);
  static void modt_cmd(Id);
  static void run_script_cmd(Id);
  static void mplay_cmd(Id) {
    if (i_am_playing) return;
    app->mplay();
  }
  static void stop_cmd(Id) {
    app->stop_requested=true;
    i_am_playing=false; // to terminate lockup-state
    if (mk_connected && kb_tune.cur_ind>=0) {
      Score *sc=new_score("keyboard");
      sc->copy_keyb_tune();
      kb_tune.reset();
    }
  }
  static void mvup_cmd(Id) {
    int ind=app->act_tune_ind;
    if (!app->act_tune || ind==0) return;
    app_local->tunesView->rbutwin->set_rbut(scores.buf[ind-1]->rbut,false);
    scores.swap(ind,ind-1);
    --app->act_tune_ind;
  }
  static void mvdo_cmd(Id) {
    int ind=app->act_tune_ind;
    if (!app->act_tune || ind==scores.lst_score) return;
    app_local->tunesView->rbutwin->set_rbut(scores.buf[ind+1]->rbut,false);
    scores.swap(ind,ind+1);
    ++app->act_tune_ind;
  }
  static void chords_cmd(Id) {
    if (app->chordsWin) map_window(app->chordsWin->chwin);
    else app->chordsWin=new ChordsWindow(Point(700,20));
  } 
  static void raw_cmd(Id id,bool val) {
    if (val && app->act_instr_ctrl->synth) hide_window(app->act_instr_ctrl->synth->topwin);
    app->act_instr_ctrl->cview->hide();
    app->act_instr_ctrl=val ? col2phm_ctrl(app->act_color) : col2ctrl(app->act_color);
    app->act_instr_ctrl->cview->map();
  }
  static void con_mk_cmd(Id id,bool val) {
    if (val) {
      switch (midi_input_mode) {
        case eMidiDev:
          pthread_create(&thread2,0,conn_mk_alsa,0); break;
        case eMidiJack:
          pthread_create(&thread2,0,conn_mk_jack,0); break;
        default:
          alert("midi input mode?");
          mk_connected=false;
      }
      kb_tune.reset();
    }
    else
      mk_connected=false; // this will stop start_conn_mk()
  }
};

struct Selected {
  struct ScoreView *sv;           // the selected scoreview
  SLinkedList<SectData> sd_list; 
  bool inv;                       // list inverted?
  Selected():sv(0),inv(false) { }
  void insert(int lnr,int snr,int sign) {
    if (!sd_list.lis) {
      static Button *us=app_local->mouseAction->unselect;
      us->xft_text_col=xft_Red;
      us->draw_button();
    }
    sd_list.insert(SectData(lnr,snr,sign),!inv);
  }
  void remove(int lnr,int snr) {
    sd_list.remove(SectData(lnr,snr,0));
    if (!sd_list.lis) {
      static Button *us=app_local->mouseAction->unselect;
      us->xft_text_col=xft_Black;
      us->draw_button();
    }
  }
  void check_direction(int delta_lnr,int delta_snr) {
    if ((!inv && (delta_lnr>0 || delta_snr>0)) ||
        (inv && (delta_lnr<0 || delta_snr<0))) {
      sd_list.invert();
      inv=!inv;
    }
  }
  void reset() {
    sd_list.reset();
    inv=false;
    if (app_local) { // is 0 at startup
      Button *us=app_local->mouseAction->unselect;
      us->xft_text_col=xft_Black;
      us->draw_button();
    }
  }
  void restore_sel();
  int min_snr() {
    SLList_elem<SectData> *sd;
    int snr=sd_list.lis->d.snr;
    for (sd=sd_list.lis;sd;sd=sd->nxt)
      if (snr>sd->d.snr) snr=sd->d.snr;
    return snr;
  }
} selected;

struct InfoView {
  BgrWin *bgwin;
  TextWin *txt[4];
  Lamp *lamps[3];
  static const int dist=17;

  InfoView(Rect rect) {
    bgwin=new BgrWin(top_win->win,rect,FN,draw_cmd,cBackground,0);
    txt[eSco]=new TextWin(bgwin->win,Rect(62,dist+1,110,0),FN,1);    // .sco file
    txt[eScr]=new TextWin(bgwin->win,Rect(62,dist*2+1,110,0),FN,1);  // .scr file
    txt[eMnr]=new TextWin(bgwin->win,Rect(62,dist*3+1,35,0),FN,1);   // measure nr
    txt[eCdir]=new TextWin(bgwin->win,Rect(62,1,rect.width-64,0),FN,1); // current dir
    draw_cwd(false);
    lamps[eSco]=new Lamp(bgwin->win,Rect(177,dist+3,0,0),cForeground);
    lamps[eScr]=new Lamp(bgwin->win,Rect(177,dist*2+3,0,0),cForeground);
    lamps[eClip]=new Lamp(bgwin->win,Rect(165,dist*3+3,0,0),cForeground);
    for (int i=0;i<3;++i) lamps[i]->col=cLightGreen;
  }
  void set_modif(int nr,bool on) {
    uint col=on ? cRed : cLightGreen;
    lamps[nr]->lamp_color(col);
  }
  void draw() {
    bgwin->clear();
    xft_draw_string(bgwin->xft_win,xft_Black,Point(2,12),"current dir:");
    xft_draw_string(bgwin->xft_win,xft_Black,Point(2,12+dist),"score file:");
    xft_draw_string(bgwin->xft_win,xft_Black,Point(2,12+dist*2),"script file:");
    xft_draw_string(bgwin->xft_win,xft_Black,Point(2,12+dist*3),"measure:");
    xft_draw_string(bgwin->xft_win,xft_Black,Point(110,12+dist*3),"clipping?");
    for (int i=0;i<3;++i) lamps[i]->draw();
  }
  static void draw_cmd(Id) { app_local->info_view->draw(); }
  void draw_cwd(bool do_draw) {
    char buf[TextWin::SMAX];
    if (!getcwd(buf,TextWin::SMAX)) { alert("getcwd problem"); return; }
    char *start=buf+max(0,strlen(buf)-26); // long strings: skip begin
    if (do_draw) txt[eCdir]->print_text(start);
    else txt[eCdir]->add_text(start);
  }
};

char *cconst2s(int cc)  {  // char constant -> string
  static char buf[5];
  char ch;
  int i;
  for (i=0;i<4;++i) {
    ch=(cc/(1 << 8*(3-i))) & 0xff;
    buf[i]=ch;
  }
  for (i=0;i<4 && !buf[i];++i);
  return buf+i;
}
    
void handle_uev(int cmd,int param,int param2) {
  switch (cmd) {
    case 'scop':
      if (app->act_scope_scale!=app->scopeView->scale->value)
        app->scopeView->set_scale(); // sets app->act_scope_scale
      app->scopeView->draw();
      break;
    case 'repm':
      app->report_meas(param);
      break;
    case 'clip':
      app_local->info_view->set_modif(eClip,true);
      break;
    case 'wfd':  // wfile_play() finished
      wf_playing=false;
      break;      
    case 'done': // play() finished
      i_am_playing=false;
      if (app->cur_score->sc_type==eMusic) {
        if (app->task==eDumpwav) {
          if (!close_dump_wav())
            alert("wave file closing problem");
          else 
            app->dia_wd->dlabel("wave file created",cForeground);
          app->task=0;
        }
      }
      else if (app->repeat && !app->stop_requested) {
        kb_tune.nxt_turn();
        app->svplay(0);
      }
      break;
    case 'vcfd':
    case 'clvd':
      monosynth_uev(cmd,param);  // monosynth display command
      break;
    case 'sisa':
      col2ctrl(param)->draw_isa_col();
      break;
    case 'msis':
      col2ctrl(param)->synth->draw_isa_col(true);
      break;
    case 'dr0': {
      static HSlider *hsl=app_local->tempoView->tempo;
      set_text(hsl->text,"%d",10*app->act_tempo);
      hsl->set_hsval(app->act_tempo,0);
      break;
    }
    case 'dr1': app->red_control->start_timbre->draw(); break;
    case 'dr2': app->red_control->timbre->draw(); break;
    case 'dr3': app->red_control->startup->draw(); break;
    case 'dr4': app->red_control->start_amp->draw(); break;
    case 'dr5': app->red_control->decay->draw(); break;
    case 'dr6': app->purple_control->st_harm[param]->draw(); break;
    case 'dr7': app->purple_control->harm[param]->draw(); break;
    case 'dr8': app->purple_control->start_dur->draw(); break;
    case 'dr9': app->blue_control->attack->draw(); break;
    case 'dr10': app->blue_control->decay->draw(); break;
    case 'dr11': app->blue_control->rich->draw(); break;
    case 'dr12': app->blue_control->chorus->draw(); break;
    case 'dr13': app->black_control->fm_ctrl->draw(); break;
    case 'dr14': app->black_control->attack->draw(); break;
    case 'dr15': app->black_control->decay->draw(); break;
    case 'dr16': app->black_control->detune->draw(); break;
    case 'dr17': app->brown_control->fm_ctrl->draw(); break;
    case 'dr18': app->brown_control->detune->draw(); break;
    case 'dr19': app->brown_control->attack->draw(); break;
    case 'dr20': app->brown_control->decay->draw(); break;
    case 'dr21': app->blue_control->dur_limit->draw(); break;
    case 'dr22': app->red_control->dur_limit->draw(); break;
    case 'dr23': app->green_control->freq_mult->draw(); break;
    case 'dr24': app->green_control->attack->draw(); break;
    case 'dr25': app->green_control->decay->draw(); break;
    case 'dr26': app->green_control->chorus->draw(); break;
    case 'dr27': app->blue_control->lowpass->draw(); break;
    case 'dr33': col2ctrl(param)->mode->set_rbutnr(1,false); break;
    case 'dr36': col2ctrl(param)->mode->set_rbutnr(param2,false); break;
    case 'dr40':
      app_local->revb_view->reverb->draw();
      ReverbView::draw(0);
      break;
    case 'dr41': app->purple_control->sound->draw_actb(); break;
    case 'dr43': app->purple_control->decay->draw(); break;
    case 'dr44': app->black_control->mod_mod->draw(); break;
    case 'dr45': app->brown_control->mod_mod->draw(); break;
    case 'dr46': {
        PhmCtrl *pct=col2phm_ctrl(param);
        if (!pct->reparent()) {
          pct->wave_ampl->draw();
          pct->file_select->draw();
        }
      }
      break;
    case 'dr49': col2phm_ctrl(param)->wave_ampl->draw(); break;
    case 'dr50': col2phm_ctrl(param)->ctrl_pitch->draw(); break;
    case 'dr51': col2FMctrl(param)->base_freq->draw(); break;
    case 'dr52': col2ctrl(param)->ampl->draw(); break;
    case 'dr55': app->red_control->tone->draw_actb(); break;
    case 'dr58': {
        PhmCtrl *pmctr=col2phm_ctrl(param);
        if (!pmctr->reparent()) {
          pmctr->speed_tension->draw();
          pmctr->decay->draw();
          pmctr->wave_ampl->draw();
          pmctr->add_noise->draw();
        }
      }
      break;
    case 'dr64': app->green_control->timbre1->draw(); break;
    case 'dr65': app->green_control->timbre2->draw(); break;
    case 'dr66': col2ctrl(param)->synth->hsliders[param2]->draw(); break;
    case 'dr67': col2ctrl(param)->synth->rbutwins[param2]->draw_actb(); break;
    case 'dr68': col2ctrl(param)->synth->checkboxs[param2]->draw(); break;
    case 'dr69':
      col2ctrl(param)->synth->draw_eg_display(1);
      col2ctrl(param)->synth->draw_eg_display(2);
      break;
    case 'dr70': col2ctrl(param)->synth->draw_wave_display(1); break;
    case 'dr71': col2ctrl(param)->synth->draw_wave_display(2); break;
    case 'dr72': col2ctrl(param)->synth->draw_wave_display(3); break;
    case 'dr73': col2ctrl(param)->synth->draw_wave_display(4); break;
    default:
      alert("unk client message '%s'",cconst2s(cmd));
  }
}

void stop_conn_mk() { app->conn_mk->set_cbval(false,0); }

struct ScoreView:ScoreViewBase {
  int index,
      state,             // set when mouse down
      cur_lnr, cur_snr,  // set when mouse down
      prev_snr,          // previous visited section nr
      delta_lnr, delta_snr,
      left_snr;          // new snr of leftmost selected section
  char the_chord_name[50];
  ScoreView *other;
  BgrWin *text_win,
         *signsview,
         *chord_name;
  TextWin *sc_name;
  CheckBox *zoom,
           *set_repeat,
           *play_1col;
  RExtButton *active;
  RButWin *display_mode,
          *group_nr;
  Point cur_point,       // set when mouse down
        prev_point;      // set when mouse moved

  ScoreView(Rect rect,int ident);
  static void draw_cmd(Id id);
  static void drawsigns_cmd(Id id);
  void mouseDown(int x,int y,int button);
  void mouseMoved(int x,int y,int button);
  void mouseUp(int x,int y,int button);
  void select_all(int start);
  void select_all_1col(int start);
  void modify_sel(int mes);
  void sel_column(int snr);
  void sel_column_1col(int snr,uint col,bool sampled);
  void assign_score(Score*,bool draw_it);
  void draw_endline(bool);
  void drawSigns();
  void draw_chordn(bool store_name);
  static void draw_chn(Id id) {
    the_sv(id.id2)->draw_chordn(false);
  }
  int linenr(int y,int snr,uchar& sign) {
    if (draw_mode==ePiano) {
      uchar lnr;
      static int diff=lnr_to_midinr(0,0);
      uchar midinr=diff - (y + ptr_dy - piano_y_off) / p_sclin_dist;
      if (!midinr_to_lnr(midinr,lnr,sign,score->signs_mode)) return -1;
      ScSection *sec=score->get_section(lnr,snr);
      if (sec->cat==eSilent) {  // search for ePlay section with same midinr
        if (lnr>0 && (sec=score->get_section(lnr-1,snr))->cat==ePlay && 
            lnr_to_midinr(lnr-1,sec->sign)==midinr) {
           --lnr;
           sign=sec->sign;
        }
        else if (lnr<sclin_max-1 && (sec=score->get_section(lnr+1,snr))->cat==ePlay &&
                 lnr_to_midinr(lnr+1,sec->sign)==midinr) {
            ++lnr;
            sign=sec->sign;
          }
      }
      return lnr;
    }
    return (y-y_off+ptr_dy)/sclin_dist;
  }
  static void svplay_cmd(Id id) {
    if (i_am_playing) return;
    app->svplay(the_sv(id.id2));
  }
  static void zoom_cmd(Id id,bool val) {
    ScoreView *sv=the_sv(id.id2);
    if (sv->zoomed!=val) {
      if (val) {
        sv->sect_len=sect_length*subdiv;
        int maxl=sect_scv/subdiv;
        if (sv->leftside > sv->score->len-maxl) sv->leftside=sv->score->len-maxl;
      }
      else {
        sv->sect_len=sect_length;
        if (sv->score->len<=sect_scv) sv->leftside=0;
        else sv->leftside/=subdiv;
      }
      sv->zoomed=val;
      sv->scroll->value=sv->leftside*sv->sect_len;
      sv->set_scroll_range();
      sv->draw_sc2(true);
    }
  }
  static void active_cmd(Id id,bool val) {
    if (val) {
      app->act_score=id.id2;
      ScoreView *sv=the_sv(app->act_score);
      if (sv->score) {
        int m=sv->score->sc_meter;
        if (!m) m=app->act_meter;
        app_local->meterView->draw(m);
      }
    }
    else {
      app->act_score=nop;
      app_local->meterView->draw(app->act_meter);
    }
  }
  static void display_cmd(Id id,int nr,int fire) {
    ScoreView *sv=the_sv(id.id2);
    switch (nr) {
      case 0: sv->draw_mode=eColorValue; break;
      case 1: sv->draw_mode=eTimingValue; break;
      case 2: sv->draw_mode=eAccValue; break;
      case 3:
        if (sv->prev_drawmode==ePiano) {
          if (sv->piano_y_off==y_off) sv->piano_y_off=p_y_off;
          else sv->piano_y_off=y_off;
        }
        sv->draw_mode=ePiano;
        break;
    }
    sv->prev_drawmode=sv->draw_mode;
    sv->draw_sc2(true);
  }
  static void scroll_cmd(Id id,int xoff,int range,bool repeat_on) {
    ScoreView *sv=the_sv(id.id2);
    sv->scroll->style.param=sv->sect_len; // step size if repeat_on
    int ls=xoff/sv->sect_len;
    if (repeat_on || ls!=sv->leftside) {
      int delta=sv->leftside-ls;
      sv->scview->move_contents_h(delta*sv->sect_len);
      sv->leftside=ls;
      sv->draw_sc2(false,delta);
    }
  }
  void set_scroll_range() { if (score) scroll->set_range(score->len*sect_len+10); }
  void reset() {
    if (score && score==other->score) {
      score=0;
      other->reset();
    }
    else score=0;
    scview->clear();
    sc_name->print_text("");
    chord_name->hide();
  }
};

struct MusicView:ScoreViewBase {
  MusicView(Rect rect);
  static void draw_cmd(Id);
  void draw_tune_name(int snr);
  void draw_tune_names();
  void exec_info_t0();
  void reset(bool reset_start,bool reset_len,bool reset_scroll) {
    score->reset(reset_len);
    if (reset_start) { play_start=0; play_stop=-1; }
    if (reset_scroll) { leftside=0; scroll->value=0; }
    mn_buf.reset(eMusic);
    scview->clear();
    tnview->clear();
    mnrview->clear();
    phm_buf.reset();
    wave_buf.reset_buffers();
  }
  void upd_info(int tim,ScInfo& sci) {
    if (tim) score->check_len(tim+1);
    score->scInfo[tim].add(sci);
  }
  static void scroll_cmd(Id,int xoff,int range,bool repeat_on) {
    MusicView *mv=the_mv();
    int ls=xoff/mv->sect_len;
    if (repeat_on || ls!=mv->leftside) {
      int delta=mv->leftside-ls;
      mv->scview->move_contents_h(delta*mv->sect_len);
      mv->leftside=ls;
      mv->draw_sc2(false,delta);
    }
  }
  void mouseDown(int x,int y,int button);
  void mouseUp(int x,int y,int button);
  void set_scroll_range() { scroll->set_range(score->end_sect*sect_len+10); }
};

struct LineColor {
  uint col[sclin_max];
  void init() {
    int i;
    for (i=0;i<sclin_max;i+=2) col[i]=cWhite;
    for (i=1;i<16;i+=2) col[i]=cLightGrey;
    for (;i<26;i+=2) col[i]=cBlack;
    for (;i<28;i+=2) col[i]=cLightGrey;
    for (;i<38;i+=2) col[i]=cLightGreen;
    for (;i<sclin_max;i+=2) col[i]=cLightGrey;
  }
} linecol;

static void modify_cwd(Str& path) {
  char *dir=path.get_dir();
  if (dir) {
    if (chdir(dir)) { alert("chdir problem"); return; }
    path.cpy(path.strip_dir());
    if (app_local)
      app_local->info_view->draw_cwd(true);
  }
}

static int cmp_labels(const void* a,const void* b) {
  return strcmp(reinterpret_cast<const ChMButton*>(a)->label,
                reinterpret_cast<const ChMButton*>(b)->label);
}
  
void fill_menu(int cmd_id) { // fill dialog menu
  switch (cmd_id) {
    case 'sco':
    case 'scr': {
      DIR *cdir=opendir(".");
      dirent *dir;
      const char *ext1= cmd_id=='sco' ? ".sco" : ".scr",
                 *ext=0;
      if (!cdir) { alert("files: current dir not opeded"); return; }
      while ((dir=readdir(cdir))!=0) {
        if (!dir->d_ino) { alert("d_ino?"); continue; }
        ext=strrchr(dir->d_name,'.');
        if (!ext || strcmp(ext,ext1)) continue;
        app->dia_wd->add_mbut(dir->d_name);
      }
      closedir(cdir);
    }
    break;
    case 'dir': {
      DIR *cdir=opendir(".");
      if (!cdir) { alert("dirs: current dir not opeded"); return; }
      dirent *dir;
      while ((dir=readdir(cdir))!=0) {
        if (!dir->d_ino) { alert("d_ino?"); continue; }
        if (dir->d_type & DT_DIR && (dir->d_name[0]!='.' || !strcmp(dir->d_name,"..")))  // no '.', no hidden files
          app->dia_wd->add_mbut(dir->d_name);
      }
      closedir(cdir);
    }
    break;
    default: alert("fill_menu?"); return;
  }
  qsort(app->dia_wd->but,app->dia_wd->butnr+1,sizeof(ChMButton),cmp_labels);
}

struct DialogCommands {
  static void save(const char *fname) {
    if (app->save(fname)) {
      app->dia_wd->dlabel("scorefile saved",cForeground);
      app->input_file.cpy(fname);
      modify_cwd(app->input_file);
      app_local->info_view->txt[eSco]->print_text(app->input_file.s);
      app_local->info_view->set_modif(eSco,false);
    }
  }
  static void read_tunes(const char *fname) {
    app->input_file.cpy(fname);
    if (app->read_tunes(fname,false)) {
      app->dia_wd->dlabel("scorefile loaded",cForeground);
      modify_cwd(app->input_file);
      InfoView *iv=app_local->info_view;
      iv->txt[eSco]->print_text(app->input_file.s);
      iv->set_modif(eSco,false);
    }
  }
  static void add_tunes(const char *fname) {
    if (app->read_tunes(fname,true)) {
      app->dia_wd->dlabel("scorefile added",cForeground);
      app_local->info_view->set_modif(eSco,true);
    }
  }
  static void new_tune(const char *tname) {
    app->new_tune(tname);
    app->dia_wd->dlabel("tune added",cForeground);
  }
  static void copy_tune(const char *tname) {
    app->copy_tune(tname);
    app->dia_wd->dlabel("copied",cForeground);
  }
  static void rename_tune(const char *tname) {
    ScoreView *sv;
    Score *sc=app->act_tune;
    if (!sc) return;
    tnames.replace(sc->name,tname);
    app_local->tunesView->rbutwin->re_label(sc->rbut,tnames.txt(sc->name));
    if (app->find_score(sc,sv))
      sv->sc_name->print_text(tnames.txt(sc->name));
    app->dia_wd->dlabel("tune renamed",cForeground);
    app_local->info_view->set_modif(eSco,true);
  }
  static void read_script_cmd(const char* sname) {
    static MusicView *mv=the_mv();
    if (mv->score->lst_sect>0) {
      if (app->script_file==sname) mv->reset(false,false,false);
      else mv->reset(true,false,false);
      mv->draw_sc2(false);
    }
    app->script_file.cpy(sname);
    InfoView *iv=app_local->info_view;
    if (!app->input_file.s[0]) {
      app->input_file.cpy(sname);
      app->input_file.new_ext(".sco");
      if (!app->read_tunes(app->input_file.s,false)) return;
      iv->txt[eSco]->print_text(app->input_file.s);
    }
    if (app->read_script(sname)) {
      app->dia_wd->dlabel("read",cForeground);
      modify_cwd(app->script_file);
      iv->txt[eScr]->print_text(app->script_file.s);
      iv->set_modif(eScr,false);
    }
    else
      app_local->info_view->txt[eScr]->print_text("");
    flush_X();
  }
  static void save_script(const char* sname) {
    app->script_file.cpy(sname);
    if (app->save_script(sname)) {
      app->dia_wd->dlabel("script saved",cForeground);
      modify_cwd(app->script_file);
      app_local->info_view->txt[eScr]->print_text(app->script_file.s);
      app_local->info_view->set_modif(eScr,false);
    }
  }
  static void save_cmd(Id) {
    app->dia_wd->dlabel("save as:",cRose);
    if (!app->input_file.s[0]) app->input_file.cpy("my-tune.sco");
    app->dia_wd->ddefault(app->input_file.s,save);
  }
  static void load_cmd(Id) {
    app->dia_wd->dlabel("load scorefile:",cRose);
    if (!app->input_file.s[0]) app->input_file.cpy("my-tune.sco");
    app->dia_wd->ddefault(app->input_file.s,read_tunes,true,fill_menu,'sco');
  }
  static void add_cmd(Id) {
    app->dia_wd->dlabel("add scorefile:",cRose);
    app->dia_wd->ddefault(app->input_file.s,add_tunes,true,fill_menu,'sco');
  }
  static void new_cmd(Id) {
    app->dia_wd->dlabel("new tune:",cRose);
    app->dia_wd->ddefault("no-name",new_tune);
  }
  static void copy_cmd(Id) {
    if (!app->act_tune) {
      alert("copy: no tune selected");
      return;
    }
    app->dia_wd->dlabel("copy to tune:",cRose);
    app->dia_wd->ddefault("no-name",copy_tune);
  } 
  static void rename_cmd(Id) {
    if (!app->act_tune) {
      alert("rename: no tune selected");
      return;
    }
    app->dia_wd->dlabel("rename to:",cRose);
    app->dia_wd->ddefault(tnames.txt(app->act_tune->name),rename_tune);
  }
  static void script_cmd(Id) {
    if (!app->script_file.s[0]) {
      if (app->input_file.s[0]) {
        app->script_file.cpy(app->input_file.s);
        app->script_file.new_ext(".scr");
      }
      else app->script_file.cpy("my-tune.scr");
    }
    app->dia_wd->dlabel("script:",cRose);
    app->dia_wd->ddefault(app->script_file.s,read_script_cmd,true,fill_menu,'scr');
  }
  static void save_script_cmd(Id) {
    if (!app->script_file.s[0]) app->script_file.cpy("my-tune.scr");
    app->dia_wd->dlabel("save script as:",cRose);
    app->dia_wd->ddefault(app->script_file.s,save_script);
  }
  static void set_wd(const char *s) {
    if (chdir(s))
      alert("directory '%s' not accessable",s);
    else {
      app->dia_wd->dlabel("current dir",cForeground);
      app_local->info_view->draw_cwd(true);
    }
  }
  static void current_dir_cmd(Id) {
    app->dia_wd->dlabel("set current directory:",cRose);
    char buf[100];
    if (!getcwd(buf,100)) { alert("getcwd problem"); return; }
    app->dia_wd->ddefault(buf,set_wd,true,fill_menu,'dir');
  }
  static void cmd(const char* s) {
    if (app->run_script(s)) {
      app->command.cpy(s);
      app->dia_wd->dlabel("cmd done",cForeground);
      MusicView *mv=the_mv();
      mv->set_scroll_range();
      mv->draw_sc2(true);
      mv->exec_info_t0();
    }
  }
  static void cmd_cmd(Id) {
    app->dia_wd->dlabel("command:",cRose);
    app->dia_wd->ddefault(app->command.s,cmd);
  }
  static void create_wav(const char *wave_file) {
    if (init_dump_wav(wave_file)) {
      app->task=eDumpwav;
      if (SAMPLE_RATE!=44100) {
        int old_srate=SAMPLE_RATE;
        SAMPLE_RATE=44100;
        set_time_scale();
        app->mplay();
        SAMPLE_RATE=old_srate;
        set_time_scale();
      }
      else
        app->mplay();
    }
    else alert("%s not opened",wave_file);
  }
  static void create_midi(const char *midi_file) {
  // act_tempo = 12 -> 120 beats/minute -> midi tempo event value = 500000
    if (midi_out.init(midi_file,subdiv,app->nupq,app->act_meter,12*500000/app->act_tempo)) {
      app->task=eMidiout;
      app->mplay();
    }
  }
  static void create_ps(const char *ps_file) {
    ps_out.abc_or_ps_file=ps_file;
    app->task=ePsout;
    if (the_mv()->play_start) {
      alert("warning: start has been set to 0");
      the_mv()->play_start=0; // else numbering won't be correct
    }
    if (!strcmp(ps_file+strlen(ps_file)-4,".abc"))  // abc instead of postcript output?
      app->task=eAbcout;
    app->mplay();
  }
  static void import_midi(const char *midi_f) {
    Str instr_map(midi_f);
    instr_map.new_ext(".gm-map");
    app->input_file.cpy(midi_f); app->input_file.new_ext(".sco");
    app->script_file.cpy(midi_f); app->script_file.new_ext(".scr");

    FILE *gm_map=fopen(instr_map.s,"r");
    char *gm_mapf=0;
    if (gm_map) {
      midi_in.read_mapf=true;
      gm_mapf=instr_map.s;
      app->midi_in_file.cpy(midi_f);
      fclose(gm_map);
    }
    else {
      midi_in.read_mapf=false;
      gm_mapf=instr_map.s;
    }
    if (midi_in.read_mf(midi_f,gm_mapf)) {
      if (midi_in.read_mapf) {
        app->dia_wd->dlabel("midi file read",cForeground);
        app_local->meterView->draw(app->act_meter);
      }
      else {
        static char mes[50];
        snprintf(mes,50,"edit %s",instr_map.s);
        app->dia_wd->dlabel(mes,cLightBlue);
      }
    }
  }
} dial_cmds;

bool exec_info(int snr,Score *sc,bool upd_sliders) {
  if (app->no_set->value) return true;
  ScInfo *sci=sc->scInfo;
  if (!sci) return true;
  for (sci=sci+snr;sci;sci=sci->next) {
    if (sci->tag && !app->exec_info_cmd(*sci,upd_sliders)) return false;
  }
  return true;
}

struct RewrScript {
  const int
    start,
    stop,
    meter;
  bool insert_gap;
  char in_buf[max200],
       out_buf[max200];
  RewrScript(int sta,int sto,int m):
      start(sta),stop(sto),meter(m),insert_gap(sto>sta) { }
  int read_rwtime(Str &str,int &pos);
  void rewr_line(bool &ok);
  void rewr_params(Str &str,int &pos,int mode,char *&ob,bool &ok);
  void char_cpy(char ch,char *&ob) {
    switch (ch) {
      case ';': str_cpy(ob,"; "); break;
      case ' ': str_cpy(ob," "); break;
      //case '#': str_cpy(ob," #"); break;
      case '\n': str_cpy(ob,"\n"); break;
      default: ob[0]=ch; ob[1]=0;
               alert("unexpected char (%c)",ch);
    }
  }
  void str_cpy(char *&ob,const char *s) {
    const char *ib;
    for (ib=s;*ib;) (ob++)[0]=(ib++)[0];
  }
};

int lnr_to_midinr(int lnr,uint sign) {  // ScLine -> midi note number
  int ind=lnr%7;
  //                       b a g f e d c
  static const int ar[7]={ 0,2,4,6,7,9,11 };
  int nr = ar[ind] + (sign==eHi ? -1 : sign==eLo ? 1 : 0) + (lnr-ind)/7*12;
  // middle C: amuc: lnr=27, ind=6, nr=11+21/7*12=47
  //           midi: 60
  return 107-nr; // 60=107-47
  // lnr=0, sign=0 -> 107
  // lnr=sclin_max=45 -> 29
}

bool midinr_to_lnr(uchar mnr,uchar& lnr,uchar& sign,int signs_mode) {
  static const int
                     // c  cis  d  es   e   f  fis  g  gis  a  bes  b 
             ar_0[12]={ 0 , 0 , 1 , 2 , 2 , 3 , 3 , 4 , 4 , 5 , 6 , 6 },
             s_0[12]= { 0 ,eHi, 0 ,eLo, 0 , 0 ,eHi, 0 ,eHi, 0 ,eLo, 0 },

                     // c  des  d  es   e   f  ges  g  as   a  bes  b 
             ar_f[12]={ 0 , 1 , 1 , 2 , 2 , 3 , 4 , 4 , 5 , 5 , 6 , 6 },
             s_f[12]= { 0 ,eLo, 0 ,eLo, 0 , 0 ,eLo, 0 ,eLo, 0 ,eLo, 0 },

                     // c  cis  d  dis  e   f  fis  g  gis a  ais  b 
             ar_s[12]={ 0 , 0 , 1 , 1 , 2 , 3 , 3 , 4 , 4 , 5 , 5 , 6 },
             s_s[12]= { 0 ,eHi, 0 ,eHi, 0 , 0 ,eHi, 0 ,eHi, 0 ,eHi, 0 };
  int ind=mnr%12,
      lnr2;
  const int *ar, *s;

  switch (signs_mode) {
    case eLo: ar=ar_f; s=s_f; break;
    case eHi: ar=ar_s; s=s_s; break;
    case 0: ar=ar_0; s=s_0; break;
    default:
      ar=ar_0; s=s_0;
      alert("midinr_to_lnr: signs_mode = %d",signs_mode);
  }

  // middle C: amuc: lnr=27
  //           midi: 60, 60/12*7 = 35, 27=62-35
  
  lnr2=62 - mnr/12*7 - ar[ind];
  if (lnr2<0 || lnr2>=sclin_max) return false;
  lnr=lnr2;
  sign=s[ind];
  return true;
}

const int piano_lines_max= lnr_to_midinr(0,0) - lnr_to_midinr(sclin_max-1,eLo);

// ========== specifications ===========

void Menus::fill_file_menu() {
  CmdMenu *file=app_local->menus->file;
  file->add_mbut("load scorefile...",dial_cmds.load_cmd);
  file->add_mbut("add scorefile...",dial_cmds.add_cmd);
  file->add_mbut("save scorefile...",dial_cmds.save_cmd);
  file->add_mbut("read script...",dial_cmds.script_cmd);
  file->add_mbut("save script...",dial_cmds.save_script_cmd);
  file->add_mbut("set current dir...",dial_cmds.current_dir_cmd);
}

void Menus::fill_imp_exp_menu() {
  CmdMenu *imp_exp=app_local->menus->imp_export;
  imp_exp->add_mbut("save as WAVE...",menu_cmd,'mkwa');
  imp_exp->add_mbut("save as MIDI...",menu_cmd,'mkmi');
  imp_exp->add_mbut("save as postscript...",menu_cmd,'mkps');
  imp_exp->add_mbut("read MIDI file...",menu_cmd,'imid');
}

void Menus::fill_help_menu() {
  CmdMenu *help=app_local->menus->help;
  help->add_mbut("manual",menu_cmd,'man');
  help->add_mbut("about",menu_cmd,'abou');
}

void ReverbView::draw(Id) {
  ReverbView *rv=app_local->revb_view;
  set_width_color(2,col2color(rv->sel_col));
  rv->bgwin->draw_line(Point(1,0),Point(1,rv->bgwin->dy));
}

void Score::tone2signs(int key_nr) {   // C = 0, B = 12
  for (int lnr=0;lnr<sclin_max;++lnr)
    lin[lnr].n_sign=signs[key_nr][lnr%7];
  signs_mode=signsMode[key_nr];
  sc_key_nr=key_nr;
}

int Score::get_meas(int snr) {
  if (sc_meter) return snr/sc_meter;
  return snr/app->act_meter;
}

ScoreView *the_sv(int i) { return app_local->scViews[i]; }
MusicView *the_mv() { return app_local->musicView; }

void drawS_ghost(ScoreViewBase* theV,int snr,int lnr,int sign,bool erase) {
  int x=(snr-theV->leftside)*theV->sect_len,
      y= theV->draw_mode==ePiano? ypos(lnr,sign,theV->piano_y_off) : ypos(lnr);
  Point start(x,y),
        end(x+theV->sect_len,y);
  if (erase)
    set_width_color(3,cWhite);
  else
    set_width_color(3,cDarkGrey);
  theV->scview->draw_line(start,end);
  if (erase)
    theV->score->get_section(lnr,snr)->drawSect(theV,snr,lnr);
}

void PhmCtrl::set_ampl_txt(int val) {
  float fval=ampl_mult[val];
  set_text(wave_ampl->text,"%.2f",fval);
}

void PhmCtrl::reset_wf_sliders(bool do_draw) {
  wave_ampl->value()=3;
  set_ampl_txt(3);
  if (do_draw) wave_ampl->draw();
}

void PhmCtrl::dump_settings(char *buf,int bufmax) {
  switch (sample_mode) {
    case ePhysmodMode:
      snprintf(buf,bufmax,"set %s phm:%d,%d,%d,%d wa:%d cpit:%s",
        color_name[ctrl_col],speed_tension->value.x,speed_tension->value.y,
        decay->value(),add_noise->value,wave_ampl->value(),ctrl_pitch->value?"on":"off");
      break;
    case eWaveMode:
      if (wave_buf.fname_nr<0) {
        alert("dump settings: wave files not yet read");
        buf[0]=0;
        break;
      }
      if (file_select->value.nr<0) {
        alert("dump settings: no wave file for %s instrument",color_name[ctrl_col]);
        buf[0]=0;
        break;
      }
      snprintf(buf,bufmax,"set %s wf:%d wa:%d cpit:%s",
        color_name[ctrl_col],
        wave_buf.filenames[file_select->value.nr].nr,
        wave_ampl->value(),ctrl_pitch->value?"on":"off");
      break;
  }
}

bool PhmCtrl::reparent() {
  switch (sample_mode) {
    case ePhysmodMode:
      if (mode->re_parent(subwin1)) {
        wave_ampl->re_parent(subwin1);
        ctrl_pitch->re_parent(subwin1);
        file_select->reset();
        hide_window(subwin2);
        map_window(subwin1);
        return true;
      }
      return false;
    case eWaveMode:
      if (mode->re_parent(subwin2)) {
        wave_ampl->re_parent(subwin2);
        ctrl_pitch->re_parent(subwin2);
        hide_window(subwin1);
        map_window(subwin2);
        return true;
      }
      return false;
  }
  return false;
}

void dump_ampl(char *buf,SliderData *a_val) {
  if (extended)
    snprintf(buf,30," a-0:%d a-1:%d a-2:%d",a_val[0].value,a_val[1].value,a_val[2].value);
  else
    snprintf(buf,30," a:%d",a_val[0].value);
}

void FMCtrl::dump_settings(char *buf,int bmax) {
  const char *col= color_name[ctrl_col];
  if (instrm_val==eSynthMode) 
    synth->dump_settings(buf,bmax,col);
  else {
    int n=snprintf(buf,bmax-30,"set %s m:nat fm:%d,%d dt:%d at:%d dc:%d mm:%d,%d bf:%s",col,
      fm_ctrl->value.x,fm_ctrl->value.y,detune->value(),attack->value(),decay->value(),
      mod_mod->value.x,mod_mod->value.y,base_freq->value?"on":"off");
    dump_ampl(buf+n,ampl_val);
  }
}

void GreenCtrl::dump_settings(char *buf,int bufmax) {
  if (instrm_val==eSynthMode) 
    synth->dump_settings(buf,bufmax,color_name[ctrl_col]);
  else {
    int n=snprintf(buf,bufmax,"set green m:nat w1:%d,%d w2:%d,%d fr:%d ch:%d at:%d dc:%d",
      timbre1->value.x,timbre1->value.y,timbre2->value.x,timbre2->value.y,
      freq_mult->value(),chorus->value(),
      attack->value(),decay->value());
    dump_ampl(buf+n,ampl_val);
  }
}

void PurpleCtrl::dump_settings(char *buf,int bufmax) {
  if (instrm_val==eSynthMode) 
    synth->dump_settings(buf,bufmax,color_name[ctrl_col]);
  else {
    int n=snprintf(buf,bufmax,"set purple m:nat sth:%d,%d,%d,%d,%d suh:%d,%d,%d,%d,%d su:%d snd:%d dc:%d",
      st_harm[0]->value,st_harm[1]->value,st_harm[2]->value,st_harm[3]->value,st_harm[4]->value,
      harm[0]->value,harm[1]->value,harm[2]->value,harm[3]->value,harm[4]->value,
      start_dur->value(),sound->act_rbutnr(),decay->value());
    dump_ampl(buf+n,ampl_val);
  }
}

void RedCtrl::dump_settings(char *buf,int bufmax) {
  if (instrm_val==eSynthMode) 
    synth->dump_settings(buf,bufmax,color_name[ctrl_col]);
  else {
    int n=snprintf(buf,bufmax,"set red m:nat stw:%d,%d suw:%d,%d sa:%d su:%d dc:%d dl:%d tone:%d",
      start_timbre->value.x,start_timbre->value.y,
      timbre->value.x,timbre->value.y,
      start_amp->value,startup->value(),decay->value(),dur_limit->value(),tone->act_rbutnr());
    dump_ampl(buf+n,ampl_val);
  }
}

void BlueCtrl::dump_settings(char *buf,int bufmax) {
  if (instrm_val==eSynthMode) 
    synth->dump_settings(buf,bufmax,color_name[ctrl_col]);
  else {
    int n=snprintf(buf,bufmax,"set blue m:nat rich:%s ch:%s at:%d dc:%d dl:%d lp:%d",
      rich->value?"on":"off",chorus->value?"on":"off",
      attack->value(),decay->value(),dur_limit->value(),lowpass->value());
    dump_ampl(buf+n,ampl_val);
  }
}

void EditScript::key_cmd(Id,int ctrl_key,int key) {
  app_local->info_view->set_modif(eScr,true);
  static EditScript *es=app_local->editScript;
  int nr_lines;
  if ((ctrl_key==XK_Control_L || ctrl_key==XK_Control_R)) {
    if (key=='s') {
      int ypos,nrch;
      char buf[max200];
      es->textview->get_info(&nr_lines,&ypos,&nrch);
      app->act_instr_ctrl->dump_settings(buf,max200);
      if (nrch>0) {
        es->textview->insert_line(buf,ypos+1);
        es->scroll->set_range((nr_lines+4)*TDIST);
      }
      else es->textview->set_line(buf,ypos);
    }
  }
  else if (key==XK_Return) {
    es->textview->get_info(&nr_lines,0,0);
    es->scroll->set_range((nr_lines+4)*TDIST);
  }
}

void EditScript::scr_cmd(Id,int yoff,int range,bool) {
  EditScript *eds=app_local->editScript;
  eds->textview->set_y_off(yoff);
  eds->meas_info->set_y_off(yoff);
}

void App::report_meas(int snr) {
  char s[10];
  sprintf(s,"%d",snr/act_meter);
  app_local->info_view->txt[eMnr]->print_text(s);
}

void App::svplay(ScoreView *sv1) {
  static ScoreView *sv;
  if (sv1) {   // sv1 = 0 during repeat
    sv=sv1;
    scopeView->reset();
  }
  app_local->info_view->set_modif(eClip,false);
  if (!(cur_score=sv->score)) {
    i_am_playing=false;
    return;
  }
  stop_requested=false;
  play_1_col= sv->play_1col->value ? act_color : nop;
  repeat=sv->set_repeat->value;
  if (mk_connected && !repeat) reset_mk_nbuf();
  playScore(sv->play_start,sv->play_stop);
}

void App::mplay() {
  static MusicView *mv=the_mv();
  stop_requested=false;
  app_local->info_view->set_modif(eClip,false);
  cur_score=mv->score;
  scopeView->reset();
  repeat=false;
  play_1_col=nop;
  playScore(mv->play_start,mv->play_stop);
}

void AppLocal::run_script_cmd(Id) {
  static MusicView *mv=the_mv();
  mv->reset(false,false,false);
  static char script_text_buf[2000];
  app_local->editScript->textview->get_text(script_text_buf,2000);
  if (app->run_script(script_text_buf)) {
    mv->set_scroll_range();
    mv->draw_sc2(false);
    mv->exec_info_t0();
  }
}

ScopeView::ScopeView(Rect rect):
    nr_samples(IBsize/8),
    ptr(-1),
    lst_ptr(0),
    time_scale(1),
    maxi(scope_dim),
    mid(rect.height/2),
    round(false) {
  for (int i=0;i<scope_dim*64;++i) { buf[i]=0; pt_buf[i].set(i/time_scale,mid); }
  scopeview=new BgrWin(top_win->win,rect,MR,draw_cmd,0,0,0,cScopeBgr);
  bgwin=new BgrWin(top_win->win,Rect(rect.x+rect.width+1,rect.y,38,rect.height),MR,0,cForeground);
  scale=new VSlider(bgwin,Rect(2,TDIST,32,52),FN,0,6,"scale","1","64",0,cForeground);
  scale->value=1;
  set_scale();
}

void ScopeView::set_scale() {
  int val=app->act_scope_scale=scale->value;
  time_scale=1<<val; // 1 - 64
  maxi=time_scale*scope_dim;
  for (int i=0;i<maxi;++i) pt_buf[i].x=i/time_scale;
}
void ScopeView::reset() {
  ptr=-1; lst_ptr=0; round=false;
  for (int i=0;i<maxi;++i) { buf[i]=0; pt_buf[i].set(i/time_scale,mid); }
  scopeview->clear();
}
void ScopeView::draw_cmd(Id) { app->scopeView->redraw(); }

void ScopeView::redraw() {
  int n,i;
  set_width_color(1,cGreen);
  if (ptr>=0)
    for (n=0,i=ptr;n<maxi;++n,++i,i%=maxi)
      pt_buf[n].y=mid-buf[i];
  scopeview->draw_lines(pt_buf,maxi);
}
void ScopeView::draw() { // only to be called once, from handle_uev()
  int i,n;
  set_width_color(1,cGreen);
  if (round) {
    scopeview->move_contents_h(-nr_samples/time_scale); // 1024/8=128 new samples per 'scop' message
    for (n=maxi-scope_dim,i=(ptr+maxi-scope_dim)%maxi;n<maxi;++n,++i,i%=maxi)
      pt_buf[n].y=mid-buf[i];
    scopeview->draw_lines(pt_buf+maxi-scope_dim,scope_dim);
  }
  else if (ptr>0) {
    for (i=lst_ptr;i<ptr;++i)
      pt_buf[i].y=mid-buf[i];
    scopeview->draw_lines(pt_buf+lst_ptr,ptr-lst_ptr); 
    lst_ptr=ptr;
  }
}
void ScopeView::insert(int val) {
  if (++ptr>=maxi) { round=true; ptr=0; }
  buf[ptr]=val;
}
void AppLocal::mv_display_cmd(Id id,int nr,int fire) {
  MusicView *mv=the_mv();
  switch (nr) {
    case 0: mv->draw_mode=eColorValue; break;
    case 1: mv->draw_mode=eTimingValue; break;
    case 2:
      if (mv->prev_drawmode==ePiano) {
        if (mv->piano_y_off==y_off) mv->piano_y_off=p_y_off;
        else mv->piano_y_off=y_off;
      }
      mv->draw_mode=ePiano;
      break;
  }
  mv->prev_drawmode=mv->draw_mode;
  mv->draw_sc2(true);
}

static void set_phm(ScInfo &info,bool upd,int col,int message) {
  ShortBuffer *pmd=&info.wavedata;
  if (!upd) {
    phm_buf.set_phys_model(col,pmd);
  }
  else {
    phm_buf.var_data[col]=pmd;
    send_uev(message,col);
  }
}

bool read_wavef(bool upd,ScInfo& info,int message) {
  int col=info.idata.col,
      filenr=info.idata.d1;
  ShortBuffer *wd=wave_buf.w_buf+col;
  PhmCtrl *pct=col2phm_ctrl(col);
  int butnr=0;

  if (wave_buf.fname_nr<0) {  // PhmCtrl::collect_files() not yet run?
    if (!wave_buf.sample_dirs->coll_wavefiles()) return false;
  }
  for (int i=0;;++i) {
    if (i>wave_buf.fname_nr) {
      if (!upd) alert("read_wavef: wave file nr %d unknown",filenr);
      return false;
    }
    FileName *fn=wave_buf.filenames+i;
    if (filenr==fn->nr) {
      pct->file_select->cp_value(fn->nr,fn->name); // should be set when upd = true or false
      butnr=i;
      break;
    }
  }
  pct->reset_wf_sliders(false);
  if (upd) {
    wd->buf=info.wavedata.buf;
    wd->size=info.wavedata.size;
    wd->alloced=false;
    send_uev(message,col);
  }
  else {
    FileName *fn=wave_buf.filenames+butnr;
    if (!read_wav(fn->dir,fn->name,&info.wavedata)) return false;
    wd->size=info.wavedata.size; // fill_note_bufs() needs this
    wd->buf=0;
  }
  return true;
}

void CtrlBase::set_instr_mode(int n,bool upd) {
  switch (n) {
    case 'i': // obsolete 
    case 'n': 
      instrm_val=eNativeMode;
      if (upd) send_uev('dr36',ctrl_col,0);
      break;
    case 'm':
      instrm_val=eSynthMode;
      if (upd) send_uev('dr36',ctrl_col,1);
      break;
  }
}

void CtrlBase::set_ampl_txt(int val) {
  set_text(ampl->text,"%.2f",ampl_mult[val]);
}

void PhmCtrl::set_mode(int tag,ScInfo& info) {
  switch (tag) {
    case eWaveMode:
      sample_mode=tag;
      mode->set_rbutnr(1,false,false);
      break;
    case ePhysmodMode:
      sample_mode=tag;
      mode->set_rbutnr(0,false,false);
      speed_tension->value.set(info.idata.d0,info.idata.d1);
      decay->value()=info.idata.d2;
      add_noise->value=info.idata.d3;
      break;
  }
}

int *CtrlBase::get_ampl_ptr(bool sampled,int group_nr) {
  int *ptr=0;
  if (sampled) {
    ptr=&static_cast<PhmCtrl*>(this)->wave_ampl->value();
  }
  else {
    switch (instrm_val) {
      case eNativeMode:
        ptr= extended ? &ampl_val[group_nr].value : &ampl_val[0].value;
        break;
      case eSynthMode:
        ptr=synth->ms_ampl;
        break;
    }
  }
  if (!ptr) { alert("get_ampl?"); static int n=0; return &n; }
  return ptr;
}

bool App::exec_info_cmd(ScInfo& info,bool upd) {
  int n;
  switch (info.tag) {
    case eText: // handled by draw_tune_name()
      break;
    case eTempo:
      act_tempo=info.n;
      if (upd) send_uev('dr0'); // app_local->tempoView->tempo->draw();
      break;
    case eRed_att_timbre:
      red_control->start_timbre->value.set(info.idata.d0,info.idata.d1);
      if (upd) send_uev('dr1'); // red_control->start_timbre->draw();
      red_control->set_start_timbre();
      break;
    case eRed_timbre:
      red_control->timbre->value.set(info.idata.d0,info.idata.d1);
      if (upd) send_uev('dr2'); // red_control->timbre->draw();
      red_control->set_timbre();
      break;
    case eRed_attack:
      red_control->startup->value()=info.n;
      if (upd) send_uev('dr3'); // red_control->startup->draw();
      red_control->set_startup();
      break;
    case eRed_stamp:
      red_control->start_amp->value=info.n;
      if (upd) send_uev('dr4'); // red_control->start_amp->draw();
      red_control->set_start_amp();
      break;
    case eRed_decay:
      red_control->decay->value()=info.n;
      if (upd) send_uev('dr5'); // red_control->decay->draw();
      red_control->set_decay();
      break;
    case ePurple_a_harm: {
        static int st_harmonics[harm_max];
        for (n=0;n<harm_max;++n)
          purple_control->st_harm[n]->value=st_harmonics[n]=info.n5[n];
        if (upd) {
          for (n=0;n<harm_max;++n)
            send_uev('dr6',n); // purple_control->st_harm[n]->draw();
          purple_control->set_st_hs_ampl(st_harmonics);
        }
      }
      break;
    case ePurple_s_harm: {
        static int harmonics[harm_max];
        for (n=0;n<harm_max;++n)
          purple_control->harm[n]->value=harmonics[n]=info.n5[n];
        if (upd) {
          for (n=0;n<harm_max;++n)
            send_uev('dr7',n); // purple_control->harm[n]->draw();
          purple_control->set_hs_ampl(harmonics);
        }
      }
      break;
    case ePurple_attack:
      purple_control->start_dur->value()=info.n;
      if (upd) send_uev('dr8'); // purple_control->start_dur->draw();
      purple_control->set_start_dur();
      break;
    case eBlue_attack:
      blue_control->attack->value()=info.n;
      if (upd) send_uev('dr9'); // blue_control->attack->draw();
      blue_control->set_attack();
      break;
    case eBlue_decay:
      blue_control->decay->value()=info.n;
      if (upd) send_uev('dr10'); // blue_control->decay->draw();
      blue_control->set_decay();
      break;
    case eBlue_durlim:
      blue_control->dur_limit->value()=info.n;
      if (upd) send_uev('dr21'); // blue_control->dur_limit->draw();
      blue_control->set_durlim();
      break;
    case eBlue_lowp:
      blue_control->lowpass->value()=info.n;
      if (upd) send_uev('dr27'); // blue_control->lowpass->draw();
      blue_control->set_lowpass();
      break;
    case eRed_durlim:
      red_control->dur_limit->value()=info.n;
      if (upd) send_uev('dr22'); // red_control->dur_limit->draw();
      red_control->set_durlim();
      break;
    case eBlue_rich:
      blue_control->rich->value=info.b;
      if (upd) send_uev('dr11'); // blue_control->rich->draw();
      break;
    case eBlue_chorus:
      blue_control->chorus->value=info.b;
      if (upd) send_uev('dr12'); // blue_control->chorus->draw();
      break;
    case eBlack_fm:
      black_control->fm_ctrl->value.set(info.idata.d0,info.idata.d1);
      if (upd) {
        black_control->setfm(eModFreq);
        black_control->setfm(eModIndex);
        send_uev('dr13'); // black_control->fm_ctrl->draw();
      }
      break;
    case eBlack_attack:
      black_control->attack->value()=info.n;
      if (upd) send_uev('dr14'); // black_control->attack->draw();
      black_control->set_attack();
      break;
    case eBlack_decay:
      black_control->decay->value()=info.n;
      if (upd) send_uev('dr15'); // black_control->decay->draw();
      black_control->set_decay();
      break;
    case eBlack_detune:
      black_control->detune->value()=info.n;
      if (upd) send_uev('dr16'); // black_control->detune->draw();
      black_control->set_detune();
      break;
    case eBrown_fm:
      brown_control->fm_ctrl->value.set(info.idata.d0,info.idata.d1);
      if (upd) { 
        brown_control->setfm(eModFreq);
        brown_control->setfm(eModIndex);
        send_uev('dr17'); // brown_control->fm_ctrl->draw();
      }
      break;
    case eBrown_detune:
      brown_control->detune->value()=info.n;
      if (upd) send_uev('dr18'); // brown_control->detune->draw();
      brown_control->set_detune();
      break;
    case eBrown_attack:
      brown_control->attack->value()=info.n;
      if (upd) send_uev('dr19'); // brown_control->attack->draw();
      brown_control->set_attack();
      break;
    case eBrown_decay:
      brown_control->decay->value()=info.n;
      if (upd) send_uev('dr20'); // brown_control->decay->draw();
      brown_control->set_decay();
      break;
    case eGreen_timbre1:
      green_control->timbre1->value.set(info.idata.d0,info.idata.d1);
      green_control->set_timbre1();
      if (upd) send_uev('dr64'); // green_control->timbre1->draw();
      break;
    case eGreen_timbre2:
      green_control->timbre2->value.set(info.idata.d0,info.idata.d1);
      green_control->set_timbre2();
      if (upd) send_uev('dr65'); // green_control->timbre2->draw();
      break;
    case eGreen_attack:
      green_control->attack->value()=info.n;
      if (upd) send_uev('dr24'); // green_control->attack->draw();
      green_control->set_attack();
      break;
    case eGreen_decay:
      green_control->decay->value()=info.n;
      if (upd) send_uev('dr25'); // green_control->decay->draw();
      green_control->set_decay();
      break;
    case eGreen_fmul:
      green_control->freq_mult->value()=info.n;
      green_control->set_freq_mult();
      if (upd) send_uev('dr23'); // green_control->freq_mult->draw();
      break;
    case eGreen_chorus:
      green_control->chorus->value()=info.n;
      green_control->set_freq_mult();
      if (upd) send_uev('dr26'); // green_control->chorus->draw();
      break;
    case eSynth: {
        int col=info.ms_data.col;
        CtrlBase *ctr=col2ctrl(col);
        ctr->instrm_val=eSynthMode;
        if (upd) {
          ctr->synth->update_values(&info.ms_data,false);
          send_uev('dr33',col);
        }
      }
      break;
    case eMode:
      col2ctrl(info.idata.col)->set_instr_mode(info.idata.d1,upd);
      break;
    case eRevb:
      if (upd) {
        app_local->revb_view->reverb->set_hvsval(Int2(info.idata.d1,info.idata.d2),1,false); // fire, not draw
        app_local->revb_view->sel_col=info.idata.col;
        col2ctrl(info.idata.col)->set_revb(info.idata.d1,info.idata.d2);
        send_uev('dr40'); // revb_view->reverb->draw(), revb_view->draw()
      }
      break;
    case ePurple_tone:
      purple_control->sound->set_rbutnr(info.n,true);
      if (upd) send_uev('dr41'); // purple_control->sound->draw()
      break;
    case ePhysM:
      col2phm_ctrl(info.idata.col)->set_mode(ePhysmodMode,info);
      set_phm(info,upd,info.idata.col,'dr58');
      break;
    case eRed_tone:
      red_control->tone->set_rbutnr(info.n,true);
      if (upd) send_uev('dr55'); // red_control->tone->draw()
      break;
    case ePurple_decay:
      purple_control->decay->value()=info.n;
      if (upd) send_uev('dr43'); // purple_control->decay->draw();
      purple_control->set_decay();
      break;
    case eBlack_mmod:
      black_control->mod_mod->value.set(info.idata.d0,info.idata.d1);
      if (upd) {
        black_control->set_mmod();
        send_uev('dr44'); // black_control->mod_mod->draw();
      }
      break;
    case eBrown_mmod:
      brown_control->mod_mod->value.set(info.idata.d0,info.idata.d1);
      if (upd) {
        brown_control->set_mmod();
        send_uev('dr45'); // brown_control->mod_mod->draw();
      }
      break;
    case eWaveF:
      col2phm_ctrl(info.idata.col)->set_mode(eWaveMode,info);
      if (!read_wavef(upd,info,'dr46')) return false;
      break;
    case eWaveAmpl:
      col2phm_ctrl(info.idata.col)->wave_ampl->value()=info.idata.d1;
      col2phm_ctrl(info.idata.col)->set_ampl_txt(info.idata.d1);
      if (upd) send_uev('dr49',info.idata.col);
      break;
    case eCtrPitch:
      col2phm_ctrl(info.idata.col)->ctrl_pitch->value=info.idata.b;
      if (upd) send_uev('dr50',info.idata.col);
      break;
    case eBaseFreq:
      col2FMctrl(info.idata.col)->base_freq->value=info.idata.b;
      if (upd) {
        col2FMctrl(info.idata.col)->setfm(eModFreq);
        send_uev('dr51',info.idata.col);
      }
      break;
    case eAmpl: {
        CtrlBase *ctr=col2ctrl(info.idata.col);
        for (int i=0;i<3;++i) ctr->ampl_val[i]=info.idata.d1;
        ctr->set_ampl_txt(info.idata.d1);
      }
      if (upd) send_uev('dr52',info.idata.col);
      break;
    case eAmpl_gr: {
        int val=info.idata.d1,
            gr=info.idata.d2;
        CtrlBase *ctr=col2ctrl(info.idata.col);
        ctr->ampl_val[gr].value=val;
        if (ctr->group->act_rbutnr()==gr) {
          ctr->set_ampl_txt(val);
          if (upd) send_uev('dr52',info.idata.col);
        }
      }
      break;
    case eLoc: col2ctrl(info.idata.col)->set_loc(info.idata.d1); break;
    case eSLoc: col2phm_ctrl(info.idata.col)->set_sampled_loc(info.idata.d1); break;
    case eIsa: col2ctrl(info.idata.col)->set_isa(info.idata.d1); break;
    case eMsIsa: col2ctrl(info.idata.col)->set_ms_isa(info.idata.d1); break;
    case eIsa_gr: col2ctrl(info.idata.col)->set_isa(info.idata.d1,info.idata.d2); break;
    case eMidiInstr:  // then: upd = false
      for (n=0;n<colors_max;++n)
        midi_out.col2midi_instr[info.midi_data.gnr][n]=info.midi_data.n6[n]-1; // n6: 1 - 128
      break;
    case eMidiPerc:
      for (n=0;n<colors_max;++n)
        midi_out.col2smp_instr[info.midi_data.gnr][n]=info.midi_data.n6[n]; // n6: 35 - 81
      break;
    default:
      alert("exec_info_cmd: unknown tag %u",info.tag);
      return false;
  }
  return true;
}

void set_msynth(CtrlBase *ctrl,bool val,int col) {
  if (val) {
    ctrl->synth->map_topwin(true);
    ctrl->map_win(true);
  }
  else {
    ctrl->synth->map_topwin(false);
  }
}

void checkbox_cmd(Id cmd,bool val) {
  switch (cmd.id1) {
    case 'nois':  // from phm control, add noise
      phm_buf.set_phys_model(cmd.id2,0);
      break;
    default:
      alert("checkbox_cmd: unk cmd %d",cmd.id1);
  }
}

void *call_browser(void*) {
   char buf[100];
   snprintf(buf,100,"%s /usr/share/doc/amuc/amuc-man.html",help_browser);
   if (system(buf)) alert("Browser for help-page not available (hint: modify .amucrc)");
   return 0;
}

void menu_cmd(Id id) {
  switch (id.id1) {
    case 'mkwa':
      if (!i_am_playing) {
        app->dia_wd->dlabel("wave file:",cRose);
        app->dia_wd->ddefault("out.wav",dial_cmds.create_wav);
      }
      break;
    case 'mkmi':
      if (!i_am_playing) {
        app->dia_wd->dlabel("midi file:",cRose);
        app->dia_wd->ddefault("out.mid",dial_cmds.create_midi);
      }
      break;
    case 'mkps':
      if (!i_am_playing) {
        app->dia_wd->dlabel("postscript file:",cRose);
        app->dia_wd->ddefault("out.ps",dial_cmds.create_ps);
      }
      break;
    case 'imid':
      if (!i_am_playing) {
        if (!app->midi_in_file.s[0]) app->midi_in_file.cpy("in.mid");
        app->dia_wd->dlabel("midi file:",cRose);
        app->dia_wd->ddefault(app->midi_in_file.s,dial_cmds.import_midi);
      }
      break;
    case 'man': {
        pthread_t sys_thread;
        pthread_create(&sys_thread,0,call_browser,0);
      }
      break;
    case 'abou':
      alert("      Amuc - the Amsterdam Music Composer");
      alert("");
      alert("version#66: %s",version);
      alert("author#66: W.Boeke");
      alert("email#66: w.boeke@chello.nl");
      alert("homepage#66: members.chello.nl/w.boeke/amuc/index.html");
      break;
    default: alert("menu_cmd: unknown cmd %d",id.id1);
  }
}

void button_cmd(Id id) {
  switch (id.id1) {
    case 'uns':      // unselect
      app->act_action='u'; // no break;
    case 'rcol':     // re-color selected
    case 'del ':     // delete selection
    case 'shmu': case 'shmd':  // shift move up, down
    case 'shcu': case 'shcd':  // shift copy up, down
      if (selected.sv) selected.sv->modify_sel(id.id1);
      app_local->mouseAction->reset();
      app_local->info_view->set_modif(eSco,true);
      break;
    case 'ok':
      app->dia_wd->dok();
      break;
    case 'eq': {
        CtrlBase *ctr=col2ctrl(id.id2);
        ctr->set_isa((ctr->isa[ctr->group ? ctr->group->act_rbutnr() : 0]->ctrl_col+1)%colors_max);
      }
      break;
    case 'revc': {
        ReverbView *rv=app_local->revb_view;
        rv->sel_col=(rv->sel_col+1)%colors_max;
        col2ctrl(rv->sel_col)->set_revb(rv->reverb->value.x,rv->reverb->value.y);
        ReverbView::draw(0);
      }
      break;
    case 'cwf': {   // re-collect wave files
        PhmCtrl *pc=col2phm_ctrl(id.id2);
        pc->file_select->reset();
        if (wave_buf.sample_dirs->coll_wavefiles()) {
          for (int i=0;i<=wave_buf.fname_nr;++i) {
            FileName *fn=wave_buf.filenames+i;
            pc->file_select->add_mbut(fn->name,fn->col);
          }
          pc->file_select->init_mwin();
        }
      }
      break;
    default: alert("button_cmd: unk cmd %d",id.id1);
  }
}

void rbutton_cmd(Id id,int nr,int fire) {
  const int m1[2]={ eNativeMode,eSynthMode };
  const int m2[2]={ ePhysmodMode,eWaveMode };
  PhmCtrl *pct=col2phm_ctrl(id.id2);
  switch (id.id1) {
    case 'smod':  // from phys mod ctrl, sample mode
      pct->sample_mode=m2[nr];
      if (pct->sample_mode==ePhysmodMode) {
        pct->mode->re_parent(pct->subwin1);
        pct->wave_ampl->re_parent(pct->subwin1);
        pct->ctrl_pitch->re_parent(pct->subwin1);
        pct->file_select->reset();
        hide_window(pct->subwin2);
        map_window(pct->subwin1);
      }
      else {
        pct->mode->re_parent(pct->subwin2);
        pct->wave_ampl->re_parent(pct->subwin2);
        pct->ctrl_pitch->re_parent(pct->subwin2);
        hide_window(pct->subwin1);
        map_window(pct->subwin2);
      }
      break;
    case 'moms':  //  mode = mono synthesizer
      { CtrlBase *ct=col2ctrl(id.id2);
        ct->instrm_val=m1[nr];
        set_msynth(ct,nr==1,id.id2);
      }
      break;
    default: alert("rbutton_cmd: unk cmd %d",id.id1);
  }
}

void exp_rbutton_cmd(Id cmd,bool is_act) {
  if (!is_act) {
    app->act_action=0;
    return;
  }
  switch (cmd.id1) {
    case 'up': app->act_action=XK_Up; break;
    case 'do': app->act_action=XK_Down; break;
    case 'left': app->act_action=XK_Left; break;
    case 'ninf': app->act_action='i'; break;
    case 'sel': app->act_action='s'; break;
    case 'scol': app->act_action='scol'; break;
    case 'move': app->act_action='m'; break;
    case 'copy': app->act_action='c'; break;
    case 'prta': app->act_action='p'; break;
    case 'all': app->act_action='a'; break;
    case 'allc': app->act_action='allc'; break;
    case 'muln': app->act_action='n'; break;
    default: alert("exp_rbutton_cmd: unk cmd %d",cmd.id1);
  }
}

void hvslider_cmd(Id id,Int2 val,char *&text_x,char *&text_y,bool rel) {
  switch (id.id1) {
    case 'rval': {   // reverb value
        ReverbView *rv=app_local->revb_view;
        col2ctrl(rv->sel_col)->set_revb(val.x,val.y);
        set_text(rv->reverb->text_x,
          val.x==0 ? 0 :
          val.x==1 ? "0.5 short" :
          val.x==2 ? "0.5 long"  :
          val.x==3 ? "1.0 short" :
          val.x==4 ? "1.0 long"  : "?");
      }
      break;
    case 'fm  ': {  // from black_ or brown_control, fm freq, fm index
        FMCtrl *fmc=col2FMctrl(id.id2);
        fmc->setfm(eModFreq);
        fmc->setfm(eModIndex);
      }
      break;
    case 'momo':  // from black_ or brown_control, mod mod
      if (rel) col2FMctrl(id.id2)->set_mmod();
      break;
    case 'radn':  // from red_control, attack diff/nrsin
      app->red_control->set_start_timbre();
      break;
    case 'rsdn':  // from red_control, sustain diff/nsin
      app->red_control->set_timbre();
      break;
    case 'grw1':  // from green_control, diff/nsin wave1
      app->green_control->set_timbre1();
      break;
    case 'grw2':  // from green_control, diff/nsin wave2
      app->green_control->set_timbre2();
      break;
    case 'spte':  // from phys mod ctrl, speed_tension
      if (rel) phm_buf.set_phys_model(id.id2,0);
      break;
    default: alert("hvslider_cmd: unk cmd %d",id.id1);
  }
}

void slider_cmd(Id id,int val,int fire,char *&text,bool rel) {
  PhmCtrl *pct;
  FMCtrl *fmctr;
  switch (id.id1) {
    case 'deca':  // from phys mod ctrl, decay
      if (rel) phm_buf.set_phys_model(id.id2,0);
      break;
    case 'grfm':  // freq ratio green instrument
      app->green_control->set_freq_mult();
      break;
    case 'iamp': {
        CtrlBase *ctr=col2ctrl(id.id2);
        ctr->ampl_val[ctr->group ? ctr->group->act_rbutnr() : 0].value=val;
        set_text(ctr->ampl->text,"%.2f",ampl_mult[val]); // extern SliderData
      }
      break;
    case 'fmat':
      fmctr=col2FMctrl(id.id2);
      fmctr->set_attack();
      break;
    case 'fmde':
      fmctr=col2FMctrl(id.id2);
      fmctr->set_decay();
      break;
    case 'rest':  // startup red instrument
      app->red_control->set_startup();
      break;
    case 'samp':  // start-ampl red instrument
      app->red_control->set_start_amp();
      break;
    case 'rede':  // decay red instrument
      app->red_control->set_decay();
      break;
    case 'grat':  // attack green instrument
      app->green_control->set_attack();
      break;
    case 'grde':  // decay green instrument
      app->green_control->set_decay();
      break;
    case 'purd':  // decay purple instrument
      app->purple_control->set_decay();
      break;
    case 'blat':  // attack blue instrument
      app->blue_control->set_attack();
      break;
    case 'blde':  // decay blue instrument
      app->blue_control->set_decay();
      break;
    case 'durl':  // duration limit instrument
      switch (id.id2) {
        case eBlue: app->blue_control->set_durlim(); break;
        case eRed: app->red_control->set_durlim(); break;
      }
      break;
    case 'lpas':  // lowpass blue instr
      app->blue_control->set_lowpass();
      break;
    case 'fmdt':  // detune black or brown instrument
      fmctr=col2FMctrl(id.id2);
      fmctr->set_detune();
      break;
    case 'puah': { // update attack harmonics purple instr
        static int harmonics[harm_max];
        for(int n=0;n<harm_max;++n)
          harmonics[n]=app->purple_control->st_harm[n]->value;
        app->purple_control->set_st_hs_ampl(harmonics);
      }
      break;
    case 'purp': { // update purple instr
        static int harmonics[harm_max];
        for(int n=0;n<harm_max;++n)
          harmonics[n]=app->purple_control->harm[n]->value;
        app->purple_control->set_hs_ampl(harmonics); 
      }
      break;
    case 'pusu':  // startup duration purple instr
      app->purple_control->set_start_dur();
      break;
    case 'wamp': // wave ampl sampled instr
      pct=col2phm_ctrl(id.id2);
      pct->set_ampl_txt(val);
      break;
    case 'phwa': // wave ampl sampled instr
      pct=col2phm_ctrl(id.id2);
      pct->set_ampl_txt(val);
      break;
    case 'semt': // shift semitones
      set_text(text,"%d semi t.",app_local->mouseAction->semi_tones->value);
      break;
    default: alert("slider_cmd: unk cmd %s",cconst2s(id.id1));
  }
}

void rbutwin_cmd(Id id,int butnr,int fire) {
  switch (id.id1) {
    case 'fmgr': {  // amplitude group
      CtrlBase *ctr=col2ctrl(id.id2);
      ctr->ampl->d=ctr->ampl_val+butnr;
      ctr->ampl->draw();
      ctr->draw_isa_col();
    }
    case 'purt':  // sound-mode purple instrument
      app->purple_control->set_tone();
      break;
    case 'redt':  // tone-mode red instrument
      app->red_control->set_tone();
      break;
    case 'grnr': { // score group nr
      ScoreView *sv=the_sv(id.id2);
      if (sv->score) {
        ScSection *sec;
        sv->score->ngroup=butnr;
        for (int lnr=0;lnr<sclin_max;++lnr)
          for (int snr=0;snr<sv->score->len;++snr) {
            sec=sv->score->get_section(lnr,snr);
            if (sec->cat==ePlay)
              for (;sec;sec=sec->nxt()) sec->s_group=butnr;
          }
        }
        app_local->info_view->set_modif(eSco,true);
      }
      break;
    default: alert("rbutwin_cmd: unk cmd %d",id.id1);
  }
}

void menu_cmd(Id id,ChMButton *mb) {
  PhmCtrl *pct;
  switch (id.id1) {
    case 'fsel':   // read sample wave file
      pct=col2phm_ctrl(id.id2);
      if (pct->just_listen->value) {
        if (!i_am_playing && !wf_playing) {
          app->stop_requested=false;
          app->scopeView->reset();
          wl_buf.reset();
          FileName *fn=wave_buf.filenames+mb->nr;
          if (read_wav(fn->dir,fn->name,&wl_buf)) {
            if (output_port==eAlsa) {
              pthread_create(&thread3, 0, wave_listen, 0);
            }
            else wf_playing=true;
          }
        }
      }
      else {
        pct->reset_wf_sliders(true);
        ShortBuffer *wd=wave_buf.w_buf + pct->ctrl_col;
        wd->reset();
        pct->file_select->reset();
        FileName *fn=wave_buf.filenames+mb->nr;
        if (read_wav(fn->dir,fn->name,wd)) {
          pct->file_select->cp_value(mb->nr,mb->label);
          pct->file_select->draw();
        }
      }
      break;
    default: alert("menu_cmd: unk cmd %d",id.id1);
  }
}

ScSection::ScSection():
  s_col(eBlack),cat(eSilent),sign(0),
  stacc(false),sampled(false),sel(false),sel_color(false),
  port_dsnr(0),
  del_start(0),del_end(0),s_group(0),
  port_dlnr(0),
  nxt_note(0),
  src_tune(0) {
}

void ScSection::reset() {
  static const ScSection sec;
  *this=sec;
}

ScSection* ScSection::nxt() {
  if (nxt_note) return mn_buf.buf+nxt_note;
  return 0;
}

bool ScSection::prepend(ScSection *from,int sctype,uint *lst_cp) { // *lst_cp = index of last copied ScSection
  int ind,ind2;
  if (cat==ePlay) {
    if (!(ind2=mn_buf.new_section(sctype))) return false;
    mn_buf.buf[ind2]=*this;
    *this=*from;
    if (nxt_note) {
      if (!(ind=nxt()->copy_to_new_sect(sctype,lst_cp))) return false;
      nxt_note=ind;
    }
    else
      if (lst_cp) *lst_cp=0;
    for (ScSection *sec=this;;sec=sec->nxt())
      if (!sec->nxt_note) { sec->nxt_note=ind2; break; }
  }
  else { // cat==eSilent
    *this=*from;
    if (nxt_note) {
      if (!(ind=nxt()->copy_to_new_sect(sctype,lst_cp))) return false;
      nxt_note=ind;
    }
  }
  return true;
}
  
Score::Score(const char *nam,int length,uint sctype):
    name(nam ? tnames.add(nam) : -1),
    len(length),
    lst_sect(-1),
    end_sect(0),
    sc_key_nr(0),
    signs_mode(0),
    sc_meter(0),
    ngroup(0),
    sc_type(sctype),
    scInfo(sctype==eMusic ? new ScInfo[len] : 0),
    block(new ScSection[sclin_max*len]) { 
  for (int i=0;i<sclin_max;i++) {
    lin[i].sect=block+i*len;
    lin[i].n_sign=0;
  }
}

Score::~Score() { // not used for musicView (where scInfo is non-zero)
  delete[] block;
}

bool Score::check_len(int required) {  // may modify lin[].sect
  if (len>=required) return false;
  ScInfo *old_info;
  ScSection *old_block=block;
  int n,n2,
      old_len=len;
  if (debug) printf("len incr: len=%d req=%d\n",len,required);
  while (len<required) len+=old_len;
  block=new ScSection[sclin_max*len];  
  for (n=0;n<sclin_max;n++) {
    lin[n].sect=block+n*len;
    for (n2=0;n2<old_len;++n2)
      lin[n].sect[n2]=old_block[n2+old_len*n];
  }
  delete[] old_block;
  if (scInfo) {
    old_info=scInfo;
    scInfo=new ScInfo[len];
    for (n=0;n<old_len;++n) {
      scInfo[n]=old_info[n];
      old_info[n].next=0; // to prohibit deletion of *next sections
    }
    delete[] old_info;
  }
  return true;
}

void Score::reset(bool reset_len) {
  lst_sect=-1;
  end_sect=0;
  signs_mode=0;
  sc_meter=0;
  ngroup=0;
  int lnr,snr;
  int max= sc_type==eMusic ? sect_mus_max : sect_scv;
  if (sc_type==eMusic) {
    for (snr=0;snr<len;snr++) scInfo[snr].reset();
    phm_buf.reset();
    wave_buf.reset_buffers();
  }
  if (!reset_len || len<=max) {
    for (lnr=0;lnr<sclin_max;lnr++) {
      lin[lnr].n_sign=0;
      for (snr=0;snr<len;snr++) get_section(lnr,snr)->reset();
    }
  }
  else {
    len=max;
    ScSection *old_block=block;
    block=new ScSection[sclin_max*len];
    for (lnr=0;lnr<sclin_max;lnr++) {
      lin[lnr].sect=block+len*lnr;
      lin[lnr].n_sign=0;
    }
    delete[] old_block;
  }
}

ScSection* Score::get_section(int lnr,int snr) {
  return block + lnr*len + snr;
}

ScSection* ScSection::get_the_section(ScSection *from,int typ) {
// Get equal or new section
// Supposed: cat = ePlay
  ScSection *sec;
  for (sec=this;;sec=sec->nxt()) {
    if (from &&
       sec->s_col==from->s_col &&
       sec->s_group==from->s_group &&
       sec->sampled==from->sampled) // equal colors cannot be played together
      break;
    if (!sec->nxt_note) {
      sec->nxt_note=mn_buf.new_section(typ);
      if (sec->nxt_note)
        sec=sec->nxt();
      break;
    }
  }
  return sec;
}

void Score::copy_keyb_tune() {
  int n,n2;
  KeybNote *note;
  ScSection *to;
  note=kb_tune.buf + kb_tune.cur_ind;
  end_sect=note->snr+note->dur+1;
  check_len(end_sect);
  for (n=0;n<=kb_tune.cur_ind;++n) {
    note=kb_tune.buf+n;
    to=lin[note->lnr].sect + note->snr;
    for (n2=0;n2<note->dur;++n2,++to) {
      to->cat=ePlay;
      to->sign=note->sign;
      to->s_col=note->col;
    }
  }
}

void Score::insert_midi_note(int time,int time_end,int lnr,int col,int sign) {
  int n,
      snr_start=time/subdiv,
      snr_end=time_end/subdiv,
      del_start=time%subdiv,
      del_end=time_end%subdiv;
  if (snr_start==snr_end) { // zero-length note?
    if (debug) printf("ins_midi_note zero length: %d %d %d %d\n",time,time_end,lnr,col);
    ++snr_end;
    del_end=0;
  }
  ScSection *to,*to1,
            skip; // for preventing equal multiple notes

  if (debug) printf("ins_midi_note: %d %d %d %d %d\n",time,time_end,lnr,col,sign);
  skip.sampled=false;
  skip.s_group=0;
  skip.s_col=col;
  if (end_sect <= snr_end) {
    end_sect=snr_end+1;
    check_len(end_sect);
  }
  to=to1=lin[lnr].sect + snr_start;
  if (snr_start>0 && (to-1)->cat==ePlay && to->cat!=ePlay) (to-1)->stacc=true; // keep notes separate, unless multiple
  for (n=snr_start;;++n,++to) {
    to1=to;
    if (to->cat==ePlay) {   // multi note?
      to1=to->get_the_section(&skip,0);
    }
    to1->cat=ePlay;
    to1->sampled=false;
    to1->s_col=col;
    to1->sign=sign;
    to1->s_group=ngroup;
    if (n==snr_start) to1->del_start=del_start;
    if (n==snr_end-1) { to1->del_end=del_end; break; }
  }
}

void Score::insert_midi_perc(int time,int lnr,int col) {
  int snr_start=time/subdiv,
      del_start=time%subdiv;
  ScSection *to=0,
            skip; // for preventing equal multiple notes

  if (debug) printf("ins_midi_perc: %d %d %d\n",time,lnr,col);
  skip.sampled=true;
  skip.s_group=0;
  skip.s_col=col;
  if (end_sect <= snr_start) {
    end_sect=snr_start+1;
    check_len(end_sect);
  }
  for (int lnr1=lnr;lnr>=0;--lnr1) { // no multi notes!
    to=lin[lnr1].sect + snr_start;
    if (to->cat==eSilent) break;
  }
  to->cat=ePlay;
  to->sampled=true;
  to->s_col=col;
  to->del_start=del_start;
}

void mouse_down(Id id,int x,int y,int button) {
  switch (id.id1) {
    case 'scmn':
      the_sv(id.id2)->mouseDown_mnr(x,y,button);
      break;
    case 'scv':
      the_sv(id.id2)->mouseDown(x,y,button);
      break;
    case 'mmnr':
      the_mv()->mouseDown_mnr(x,y,button);
      break;
    case 'muv':
      the_mv()->mouseDown(x,y,button);
      break;
    default: alert("mouse_down: unk sender");
  }
}

void mouse_moved(Id id,int x,int y,int button) {
  switch (id.id1) {
    case 'scv':
      the_sv(id.id2)->mouseMoved(x,y,button);
      break;
    default: alert("mouse_moved: unk sender");
  }
}

void mouse_up(Id id,int x,int y,int button) {
  switch (id.id1) {
    case 'scv':
      the_sv(id.id2)->mouseUp(x,y,button);
      break;
    case 'muv':
      the_mv()->mouseUp(x,y,button);
      break;
    default: alert("mouse_up: unk sender");
  }
}

MusicView::MusicView(Rect rect):
    ScoreViewBase(rect,eMusic) {
  score=new Score(0,sect_mus_max,eMusic);
  mnrview=new BgrWin(top_win->win,Rect(rect.x,rect.y,rect.width,mnr_height),
                    FN,draw_cmd,mouse_down,0,0,cBgrGrey,Id('mmnr'));
  set_custom_cursor(mnrview->win,text_cursor);
  scview=new BgrWin(top_win->win,Rect(rect.x,rect.y+mnr_height,rect.width,rect.height-scrollbar_wid-tnview_height-mnr_height),
                    FN,draw_cmd,mouse_down,0,mouse_up,cWhite,Id('muv'));
  set_custom_cursor(scview->win,up_arrow_cursor);
  scroll=new HScrollbar(
    top_win->win,Rect(rect.x,rect.y+rect.height-scrollbar_wid-tnview_height+1,rect.width,0),
    Style(1,0,sect_length),FN,rect.width,scroll_cmd);
  tnview=new BgrWin(top_win->win,Rect(rect.x,rect.y+rect.height-tnview_height+2,rect.width,tnview_height),
                      FN,draw_cmd,cBgrGrey,1,Id('minf'));
}

void Scores::swap(int ind1,int ind2) {
  int label=buf[ind1]->name;
  RButton *rb=buf[ind1]->rbut;
  static RButWin *rbw=app_local->tunesView->rbutwin;
  rbw->re_label(rb,tnames.txt(buf[ind2]->name));
  buf[ind1]->rbut=buf[ind2]->rbut;
  rbw->re_label(buf[ind2]->rbut,tnames.txt(label));
  buf[ind2]->rbut=rb;
  Score *sc=buf[ind1];
  buf[ind1]=buf[ind2]; buf[ind2]=sc;
}

void Scores::remove(Score *sc) {
  int sc_ind,
      ind;
  if ((sc_ind=get_index(sc))<0) return;
  static RButWin *rbw=app_local->tunesView->rbutwin;
  for (ind=sc_ind;ind<lst_score;++ind) {
    buf[ind]=buf[ind+1];
  }
  if (app->act_tune==sc) app->act_tune=0;
  rbw->del_rbut(sc->rbut); // now: sc->rbut = act_button = 0
  delete sc;
  --lst_score;
}

void MusicView::exec_info_t0() {
  if (exec_info(0,score,false)) exec_info(0,score,true);
}

void MusicView::draw_tune_names() {
  tnview->clear();
  for (int n=leftside;n<score->len;n++)
    draw_tune_name(n);
}

void MusicView::draw_cmd(Id id) {
  switch (id.id1) {
    case 'muv': the_mv()->draw_sc2(false); break;
    case 'minf': the_mv()->draw_tune_names(); break;
  }
}

void ColorView::color_cmd(Id id,int nr,int fire) {
  app->act_color=nr;
  app->act_instr_ctrl->map_win(false);
  app->act_instr_ctrl=app->sampl->value ? col2phm_ctrl(app->act_color) : col2ctrl(app->act_color);
  app->act_instr_ctrl->map_win(true);
}

void CtrlBase::map_win(bool map) {
  if (map)    // synth not mapped
    cview->map();
  else {
    if (synth)  // 0 if PhmCtrl
      hide_window(synth->topwin);
    if (sample_mode==eWaveMode)
      static_cast<PhmCtrl*>(this)->file_select->reset();
    cview->hide();
  }
}

void CtrlBase::set_isa(int col) {
  for (int gr=0;gr<3;++gr) isa[gr]=col2ctrl(col);
  send_uev('sisa',ctrl_col);
}

void CtrlBase::set_ms_isa(int col) {
  synth->set_isa(col2ctrl(col)->synth);
  send_uev('msis',ctrl_col);
}

void CtrlBase::set_isa(int col,int gr) {
  isa[gr]=col2ctrl(col);
  if (group->act_rbutnr()==gr)
    send_uev('sisa',ctrl_col);
}

void MeterView::meter_cmd(Id,int val,int,char *&txt,bool rel) {
  MeterView *mev=app_local->meterView;
  if (app->act_score>nop) {
    ScoreView *sv=the_sv(app->act_score);
    if (!sv->score) return;
    int cur_m=sv->score->sc_meter;
    if (val<0) {  // out of range?
      set_text(txt,"%d",cur_m);
    }
    else {
      if (mev->m[val]!=cur_m) {
        set_text(txt,"%d",mev->m[val]);
        if (rel) {
          sv->score->sc_meter=mev->m[val];
          sv->draw_sc2(true);
        }
      }
    }
  }
  else {
    int cur_m=app->act_meter;
    if (val<0) {  // might be set by draw()
      set_text(txt,"%d",cur_m);
    }
    else {
      set_text(txt,"%d",mev->m[val]);
      if (rel) {
        app->act_meter=mev->m[val];
        the_sv(0)->draw_sc2(true);
        the_sv(1)->draw_sc2(true);
        the_mv()->draw_sc2(true);
      }
    }
  }
  if (rel) app->check_reset_as();
}

void App::check_reset_as() { // do not reset act_score if shift key pressed
  if (key_pressed!=XK_Shift_L) {
    active_scoreCtrl->reset();
    act_score=nop;
  }
}

void TunesView::tune_cmd(Id,int nr,int) {
  ScoreView *sv;
  app->act_tune_ind=nr;
  app->act_tune=scores[nr];
  if (app->act_score>nop) {
    if (!app->act_tune) return;
    sv=the_sv(app->act_score);
    if (selected.sv==sv) selected.restore_sel();
    sv->assign_score(app->act_tune,true);
    app->check_reset_as();
  }
}

void TunesView::scr_cmd(Id,int val,int,bool) {
  static RButWin *rb=app_local->tunesView->rbutwin;
  rb->set_y_off(val);
}

void AppLocal::modt_cmd(Id) {
  MusicView *mv=app_local->musicView;
  EditScript *es=app_local->editScript;
  char text_buf[2000];
  app->modify_script(es,mv->play_start,mv->play_stop);
  es->textview->get_text(text_buf,2000);
  mv->reset(true,false,false);
  if (app->run_script(text_buf)) {
    mv->set_scroll_range(); mv->draw_sc2(false); mv->exec_info_t0();
  }
  app_local->info_view->set_modif(eScr,true);
}

void AppLocal::clear_cmd(Id) {
  if (app->act_score>nop) {
    ScoreView *sv=the_sv(app->act_score);
    if (sv->score) {
      if (selected.sv==sv)
        selected.reset();
      sv->leftside=0; sv->scroll->value=0;
      sv->score->reset(true);
      sv->set_scroll_range();
      sv->draw_sc2(true);
      app_local->info_view->set_modif(eSco,true);
    }
    app->check_reset_as();
  }
  else {
    MusicView *mv=the_mv();
    if (mv->score->lst_sect>=0) {
      mv->reset(true,true,true);
      mv->draw_sc2(false);
    }
  }
}

void AppLocal::remove_cmd(Id) {
  if (!app->act_tune) return;
  ScoreView *sv;
  if (app->find_score(app->act_tune,sv)) {
    if (selected.sv==sv) selected.reset();
    sv->reset();
  }
  scores.remove(app->act_tune);
  app_local->info_view->set_modif(eSco,true);
}

void Selected::restore_sel() {
  SLList_elem<SectData> *sd;
  ScSection *sec;
  for (sd=sd_list.lis;sd;sd=sd->nxt) {
    sec=sv->score->get_section(sd->d.lnr,sd->d.snr);
    sec->sel=false;
    sec->drawSect(sv,sd->d.snr,sd->d.lnr);
    if (sv->score==sv->other->score)
      sec->drawSect(sv->other,sd->d.snr,sd->d.lnr);
  }
  reset();
}

void ScoreView::select_all(int start) {
  const int end= score->end_sect ? score->end_sect : score->len;
  for (int snr=start;snr<end;++snr) sel_column(snr);
}

void ScoreView::select_all_1col(int start) {  // select all of 1 color
  const int end= score->end_sect ? score->end_sect : score->len;
  for (int snr=start;snr<end;++snr)
    sel_column_1col(snr,app->act_color,app->sampl->value);
}

void ScoreView::sel_column(int snr) {
  ScSection *sect;
  for (int lnr=0;lnr<sclin_max;++lnr) {
    sect=score->get_section(lnr,snr);
    if (sect->cat==ePlay && !(draw_mode==ePiano && sect->sampled)) { // mode ePiano: sampled sections not visible
      selected.insert(lnr,snr,sect->sign);
      sect->sel=true; sect->sel_color=false;
      sect->drawSect(this,snr,lnr);
    }
  }
}

void ScoreView::sel_column_1col(int snr,uint col,bool sampled) {
  ScSection *sect,*sec;
  for (int lnr=0;lnr<sclin_max;++lnr) {
    sect=score->get_section(lnr,snr);
    if (sect->cat==ePlay) {
      for (sec=sect;sec;sec=sec->nxt()) {
        if (sec->s_col==col && sec->sampled==sampled && !(draw_mode==ePiano && sec->sampled)) {
          if (sec!=sect) {      // place *sec at frontside
            ScSection s1(*sect),s2(*sec);
            *sect=s2; sect->nxt_note=s1.nxt_note;
            *sec=s1; sec->nxt_note=s2.nxt_note;
          }
          selected.insert(lnr,snr,sect->sign);
          sect->sel=sect->sel_color=true;
          sect->drawSect(this,snr,lnr);
          break;
        }
      }
    }
  }
}

void ScoreViewBase::draw_meter_nrs(Score *sc) {
  int meter= score->sc_meter==0 ? app->act_meter : score->sc_meter;
  Str str;
  for (int snr=leftside;snr<sc->len;++snr) {
    if (snr%meter == 0) {
      int x=(snr-leftside)*sect_len;
      if (x<mnrview->dx)
        xft_draw_string(mnrview->xft_win,xft_Black,Point(x,mnr_height-2),str.tos(snr/meter));
      else break;
    }
  }
}

void ScoreViewBase::draw_start_stop(Score *sc) {
  int x,
      y=5;
  mnrview->clear();
  draw_meter_nrs(sc);
  set_color(cRed);
  if (play_start>0) {
    x=(play_start-leftside)*sect_len;
    Point pts[3]={ Point(x,y+5),Point(x+6,y),Point(x,y-5) };
    mnrview->fill_polygon(pts,3);
  }
  if (play_stop>0) {
    x=(play_stop-leftside+1)*sect_len;
    Point pts[3]={ Point(x,y+5),Point(x,y-5),Point(x-6,y) };
    mnrview->fill_polygon(pts,3);
  }
}

void ScoreViewBase::enter_start_stop(Score *sc,int snr,int mouse_but) {
  switch (mouse_but) {
    case Button1:
      if (play_start==snr)
        play_start=0;
      else {
        play_start=snr;
        if (play_stop>=0 && play_stop<play_start) alert("stop < start");
      }
      draw_start_stop(sc);
      break;
    case Button3:
      if (play_stop==snr)
        play_stop=-1;
      else {
        play_stop=snr;
        if (play_start && play_stop<play_start) alert("stop < start");
      }
      draw_start_stop(sc);
      break;
    case Button2:
      play_start=0; play_stop=-1;
      draw_start_stop(sc);
      break;
  }
}

void ScoreViewBase::mouseDown_mnr(int x,int y,int mouse_but) {
  if (!score) return;
  int snr=sectnr(x);
  enter_start_stop(score,snr,mouse_but);
}

void MusicView::mouseDown(int x,int y,int mouse_but) {
  int snr=sectnr(x);
  const int lnr=(y-y_off+ptr_dy)/sclin_dist;
  if (lnr<0 || lnr>=sclin_max || snr<0) return;
  key=key_pressed;
  if (key) {
    if (key==XK_Shift_L) {
      keep_m_action=true;
      key=app->act_action;
    }
    else app->act_action=key;
  }
  else
    key=app->act_action;
  ScSection *sec=score->get_section(lnr,snr);
  uint cat=sec->cat;
  static MouseAction *ma=app_local->mouseAction;
  switch (key) {
    case 'i':  // note info
      ma->ctrl->set_rbut(ma->but_ninf,false);
      if (draw_mode==ePiano) {
        alert("note info: not available in 'white keys' mode");
        break;
      } 
      if (cat==ePlay) {
        draw_info(sec,true);
      }
      break;
    case XK_Shift_L:
    case 0:
      break;
    default:
      alert("unexpected key pressed");
  }
}

void MusicView::mouseUp(int x,int y,int mouse_but) {
  static MouseAction *ma=app_local->mouseAction;
  if (keep_m_action) keep_m_action=false;
  else ma->reset();
}

void ScoreView::mouseDown(int x,int y,int mouse_but) {
  if (!score) return;
  if (key_pressed && textcursor_active()) alert("Warning: key pressed while cursor enabled!");
  state=eIdle;
  int snr=sectnr(x);
  uchar sign;
  const int lnr=linenr(y,snr,sign);
  if (lnr<0 || lnr>=sclin_max || snr<0) return;
  if (mouse_but==Button3 && !(zoomed && draw_mode==eTimingValue)) {  // right mouse button
    if (score->end_sect) {
      draw_endline(false);
      if (other->score==score) other->draw_endline(false);
    }
    score->end_sect=snr;
    if (score->check_len(snr+1)) {
      set_scroll_range(); draw_sc2(false);
      if (other->score==score) { other->set_scroll_range(); other->draw_sc2(false); }
    }
    else {
      draw_endline(true);
      if (other->score==score) other->draw_endline(true);
    }
    return;
  }
  if (snr>=score->len) return;

  app_local->info_view->set_modif(eSco,true);
  ScSection *sec=score->get_section(lnr,snr),
            proto;
  uint cat=sec->cat,
       col=sec->s_col;
  key=key_pressed;
  static MouseAction *ma=app_local->mouseAction;
  bool to_higher=false,
       to_lower=false,
       multiple_note=false;
  prev_snr=snr;
  cur_lnr=lnr;
  cur_snr=snr;
  if (key) {
    if (key==XK_Shift_L) {
      keep_m_action=true;
      key=app->act_action;
    }
    else app->act_action=key;
  }
  else
    key=app->act_action;
  switch (key) {
    case XK_Up:
    case XK_Down:
      to_higher=make_higher[lnr%7];
      to_lower=make_lower[lnr%7];
      // no break
    case XK_Left:
      state=eToBeReset;
      if (draw_mode==ePiano)
        return;
      for(sec=score->get_section(lnr,cur_snr);sec->cat==ePlay && sec->s_col==col;++snr,++sec) {
        ScSection *sec1;
        bool stop = snr>=score->len-1 || sec->stacc || sec->sampled;
        if (score==other->score && other->draw_mode==ePiano) {  // erase section in other view?
          sec->cat=eSilent; sec->drawSect(other,snr,lnr); sec->cat=ePlay;
        }
        switch (key) {
          case XK_Up:
            ma->ctrl->set_rbut(ma->but_sharp,false);
            if (to_higher) {
              for (sec1=sec;sec1;sec1=sec1->nxt()) sec1->sign=0;
              *score->get_section(lnr-1,snr)=*sec;
              if (sec->sel) {
                selected.remove(lnr,snr);
                selected.insert(lnr-1,snr,0);
              }
              if (lnr>0) // maybe out-of-range
                score->get_section(lnr-1,snr)->drawSect(this,other,snr,lnr-1);
              sec->reset();
            }
            else
              for (sec1=sec;sec1;sec1=sec1->nxt()) sec1->sign=eHi;
            break;
          case XK_Down:
            ma->ctrl->set_rbut(ma->but_flat,false);
            if (to_lower) {
              for (sec1=sec;sec1;sec1=sec1->nxt()) sec1->sign=0;
              *score->get_section(lnr+1,snr)=*sec;
              if (sec->sel) {
                selected.remove(lnr,snr);
                selected.insert(lnr+1,snr,0);
              }
              if (lnr<sclin_max)   // maybe out-of-range
                score->get_section(lnr+1,snr)->drawSect(this,other,snr,lnr+1);
              sec->reset();
            }
            else
              for (sec1=sec;sec1;sec1=sec1->nxt()) sec1->sign=eLo;
            break;
          case XK_Left:
            ma->ctrl->set_rbut(ma->but_normal,false);
            for (sec1=sec;sec1;sec1=sec1->nxt()) sec1->sign=0;
            break;
        }
        sec->drawSect(this,other,snr,lnr);
        if (stop) break;
      }
      return;
    case 'n':   // multiple note insert
      multiple_note=true;
      ma->ctrl->set_rbut(ma->but_multi,false);
      break;
    case 'a':   // select all
      ma->ctrl->set_rbut(ma->but_all,false);
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      state=eToBeReset;
      select_all(snr);
      return;
    case 'allc':   // select all of 1 color
      ma->ctrl->set_rbut(ma->but_all_col,false);
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      state=eToBeReset;
      select_all_1col(snr);
      return;
    case 'u':     // unselect
      if (selected.sv==this) {
        selected.restore_sel();
      }
      state=eToBeReset;
      return;
    case 's':     // select
      ma->ctrl->set_rbut(ma->but_select,false);
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      if (cat==ePlay) {
        ScSection *cur_sec=score->get_section(lnr,cur_snr);
        bool b=cur_sec->sel;
        for(sec=cur_sec;sec->cat==ePlay && sec->s_col==cur_sec->s_col && sec->sign==cur_sec->sign;++snr,++sec) {
          if (b) {
            if (sec->sel) selected.remove(lnr,snr);
          }
          else {
            selected.insert(lnr,snr,sec->sign);
          }
          sec->sel=!b; sec->sel_color=false;
          sec->drawSect(this,snr,lnr);
          if (snr>=score->len-1 || sec->stacc || sec->sampled) break;
        }
      }
      else {
        state=eCollectSel;
        sel_column(snr);
      }
      return;
    case 'i':  // note info
      ma->ctrl->set_rbut(ma->but_ninf,false);
      if (cat==ePlay) { draw_info(score->get_section(lnr,cur_snr),false); state=eToBeReset; }
      return;
    case 'scol':  // select all of 1 color
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      state=eCollectColSel;
      sel_column_1col(snr,app->act_color,app->sampl->value);
      return;
    case 'm':  // move
    case 'c':  // copy
      if (!selected.sd_list.lis) {
        state=eToBeReset; return;
      }
      if (selected.sv!=this && selected.sv->score==selected.sv->other->score) {
        alert("other score is the same");
        state=eToBeReset; return;
      }
      if (key=='c') {
        state=eCopying; ma->ctrl->set_rbut(ma->but_copy,false);
      }
      else {
        state=eMoving; ma->ctrl->set_rbut(ma->but_move,false);
      }
      cur_point=prev_point=Point(x,y);
      delta_lnr=delta_snr=left_snr=0;
      if (selected.sv!=this) {
        int lnr1,snr1,
            min_snr;
        SLList_elem<SectData> *sd;
        state= state==eMoving? eMoving_hor : eCopying_hor;
        min_snr=selected.min_snr(); // find smallest snr
        left_snr=cur_snr - min_snr;
        for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // draw ghost notes
          lnr1=sd->d.lnr;
          snr1=sd->d.snr + left_snr;
          if (lnr1>=0 && lnr1<sclin_max && snr1>=0) {
            if (score->check_len(snr1+1)) {
              set_scroll_range(); draw_sc2(false);
            }
            drawS_ghost(this,snr1,lnr1,sd->d.sign,false);
          }
        }
      }
      return;
    case 'p':  // portamento
      if (cat!=ePlay) {
        state=eToBeReset;
        return;
      }
      ma->ctrl->set_rbut(ma->but_porta,false);
      sec=score->get_section(lnr,cur_snr);
      if (sec->port_dlnr) {  
        sec->drawPortaLine(this,snr,lnr,true); // erase portaline
        sec->port_dlnr=sec->port_dsnr=0;
        state=eToBeReset;
      }
      else {
        set_custom_cursor(scview->win,dot_cursor);
        cur_point.set(x,y);
        state=ePortaStart;
      }
      return;
    case XK_Shift_L:
    case 0:
      break;
    default:
      alert("unexpected key pressed");
      state=eToBeReset;
      return;
  }
  if (!app->sampl->value) {
    if (cat==ePlay) state=eErasing;
    else if (cat==eSilent) state=eTracking;
  }
  proto.reset();
  proto.cat=ePlay;
  proto.s_col=app->act_color;
  proto.s_group=score->ngroup;
  proto.sign= draw_mode==ePiano ? sign : score->lin[lnr].n_sign;
  proto.sampled=app->sampl->value;
  proto.stacc= mouse_but==Button2;
  switch (cat) {
    case ePlay:
      if (draw_mode==eTimingValue) {
        if (!zoomed) { state=eIdle; return; }
        sec=score->get_section(lnr,cur_snr);
        switch(mouse_but) {
          case Button1:
            sec->del_start=(sec->del_start+1) % subdiv;
            sec->drawSect(this,other,snr,lnr);
            if (snr>0 && (sec-1)->cat==ePlay && (sec-1)->del_end) // if needed, redraw previous section
              (sec-1)->drawSect(this,other,snr-1,lnr);
            break;
          case Button3:
            sec->del_end=(sec->del_end+1) % subdiv;
            if (sec->del_end==0 && snr<score->len-1) // redraw next section
              (sec+1)->drawSect(this,other,snr+1,lnr);
            sec->drawSect(this,other,snr,lnr);
            if (snr>0 && (sec-1)->cat==ePlay && (sec-1)->del_end) // redraw previous section, if needed
              (sec-1)->drawSect(this,other,snr-1,lnr);
            break;
        }
        state=eIdle;
        return;
      }
      sec=score->get_section(lnr,cur_snr);
      if (multiple_note) {
        int fst_sign=sec->sign;
        sec=sec->get_the_section(&proto,0); // get equal or new section 
        int nxt_n=sec->nxt_note;
        *sec=proto;
        sec->nxt_note=nxt_n;  // nxt_note not copied
        sec->sign=fst_sign;   // sign of 1st sec copied;
        state=eToBeReset;
      }
      else if (mouse_but==Button2 && !sec->sampled) { // middle mouse button
        bool stacc=!sec->stacc;
        for (ScSection *sec1=sec;sec1;sec1=sec1->nxt()) sec1->stacc=stacc;
      }
      else {
        if (sec->sel) selected.remove(lnr,snr);
        if (sec->port_dlnr) {
          sec->drawPortaLine(this,snr,lnr,true);
          sec->port_dlnr=sec->port_dsnr=0;
        }
        sec->cat=eSilent;
      }
      break;
    case eSilent:
      if (draw_mode==eTimingValue && zoomed) { state=eIdle; return; }
      *score->get_section(lnr,cur_snr)=proto;
      break;
    default: alert("sect cat %d?",cat);
  }
  score->get_section(lnr,cur_snr)->drawSect(this,other,snr,lnr);
  if (sec->cat==eSilent) sec->reset();  // after drawSect()
}

void ScoreView::mouseMoved(int x,int y,int mouse_but) {
  if (!score) { state=eIdle; return; }
  switch (state) {
    case eTracking:
    case eErasing: {
        int snr,
            new_snr=sectnr(x);
        if (new_snr<=prev_snr) return; // only tracking to the right
        for (snr=prev_snr+1;;++snr) {
          if (snr<0 || snr>=score->len) { state=eIdle; return; }
          ScSection *const sect=score->get_section(cur_lnr,snr);
          if (state==eTracking) {
            if (sect->cat==eSilent) {
              *sect=*score->get_section(cur_lnr,cur_snr);
              sect->drawSect(this,other,snr,cur_lnr);
            }
            else if (sect->cat==ePlay) { state=eIdle; break; }
          }
          else {
            if (sect->cat==ePlay) {
              sect->cat=eSilent;
              sect->drawSect(this,other,snr,cur_lnr);
              if (sect->sel) selected.remove(cur_lnr,snr);
              if (sect->port_dlnr) {
                sect->drawPortaLine(this,snr,cur_lnr,true);
                sect->port_dlnr=sect->port_dsnr=0;
              }
              if (sect->stacc || sect->sampled) { state=eIdle; sect->reset(); break; }
              sect->reset();
            }
            else if (sect->cat==eSilent) { state=eIdle; break; }
          }
          if (snr==new_snr) break;
        }
        prev_snr=new_snr;
      }
      break;
    case eMoving:
    case eMoving_hor:
    case eMoving_vert:
    case eCopying:
    case eCopying_hor:
    case eCopying_vert: {
        SLList_elem<SectData> *sd;
        int lnr1, snr1,
            dx=(x-prev_point.x)/sect_len,
            dy=(y-prev_point.y)/sclin_dist;
        if (dy || dx) {
          int x2=abs(dx),
              y2=abs(dy);
          if (state==eMoving) {
            if (x2>y2) state=eMoving_hor;
            else if (x2<y2) state=eMoving_vert;
            else break;
          }
          else if (state==eCopying) {
            if (x2>y2) state=eCopying_hor;
            else if (x2<y2) state=eCopying_vert;
            else break;
          }
          prev_point.set(x,y);
          for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // erase old ghost notes
            lnr1=sd->d.lnr + delta_lnr;
            snr1=sd->d.snr + delta_snr + left_snr;
            if (lnr1>=0 && lnr1<sclin_max && snr1>=0 && snr1<score->len)
              drawS_ghost(this,snr1,lnr1,sd->d.sign,true);
          }
          int prev_delta_lnr=delta_lnr,
              prev_delta_snr=delta_snr;
          if (state==eMoving_vert || state==eCopying_vert)
            delta_lnr=(y-cur_point.y)/sclin_dist;
          else if (state==eMoving_hor || state==eCopying_hor)
            delta_snr=(x-cur_point.x)/sect_len;
          selected.check_direction(delta_lnr - prev_delta_lnr,delta_snr - prev_delta_snr);
          for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // draw new ghost notes
            lnr1=sd->d.lnr + delta_lnr;
            snr1=sd->d.snr + delta_snr + left_snr;
            if (lnr1>=0 && lnr1<sclin_max && snr1>=0) {
              if (score->check_len(snr1+1)) {
                set_scroll_range(); draw_sc2(false);
              }
              drawS_ghost(this,snr1,lnr1,sd->d.sign,false);
            }
          }
        }
      }
      break;
    case eCollectSel:
    case eCollectColSel: {
        int snr=sectnr(x);
        if (snr<0 || snr>=score->len) { state=eIdle; return; }
        if (snr>prev_snr)
          for (++prev_snr;;++prev_snr) {
            if (state==eCollectColSel) sel_column_1col(prev_snr,app->act_color,app->sampl->value);
            else sel_column(prev_snr);
            if (prev_snr==snr) break;
          }
        else if (snr<prev_snr)
          for (--prev_snr;;--prev_snr) {
            if (state==eCollectColSel) sel_column_1col(prev_snr,app->act_color,app->sampl->value);
            else sel_column(prev_snr);
            if (prev_snr==snr) break;
          }
      }
      break;
    case ePortaStart: {
        uchar sign;
        int snr=sectnr(x);
        const int lnr=linenr(y,snr,sign);
        ScSection *const sect=score->get_section(lnr,snr);
        if (sect->cat==ePlay)
          set_custom_cursor(scview->win,dot_cursor);
        else
          set_custom_cursor(scview->win,cross_cursor);
      }
      break;
    case eIdle:
    case eToBeReset:
      break;
    default:
      alert("mouse moving state %d?",state);
      state=eIdle;
  }
}

void ScSection::set_to_silent(ScoreView *sv,int snr,int lnr) {
  if (sel_color && nxt_note) {
    *this=*nxt();
    sel=false;
    drawSect(sv,sv->other,snr,lnr);
  }
  else {
    cat=eSilent;
    drawSect(sv,sv->other,snr,lnr);
    reset();
  }
}

int ScSection::copy_to_new_sect(int sctype,uint *lst_cp) { // supposed: this = *nxt()
  ScSection *sec,
            *to=0;
  int index=0,
      ind;
  for (sec=this;sec;sec=sec->nxt()) {
    if (!(ind=mn_buf.new_section(sctype))) return 0;
    if (to) to->nxt_note=ind;
    else index=ind; 
    to=mn_buf.buf+ind;
    *to=*sec;
    if (lst_cp) *lst_cp=ind;
  }
  return index;
}

void ScSection::rm_duplicates(bool *warn) {
  ScSection *sec,*sec2;
  if (warn) *warn=false;
  for (sec=this;sec;sec=sec->nxt()) {
    sec2=sec->nxt();
    if (!sec2) break;
    for (;sec2;sec2=sec2->nxt()) {
      if (sec->s_col==sec2->s_col &&
          sec->s_group==sec2->s_group &&
          sec->sampled==sec2->sampled) {
        if (warn) { *warn=true; return; }
        sec->nxt_note=sec2->nxt_note;
      }
    }
  }
}

void ScoreView::mouseUp(int x,int y,int mouse_but) {
  static MouseAction *ma=app_local->mouseAction;
  switch (state) {
    case eMoving_hor:
    case eMoving_vert:
    case eCopying_hor:
    case eCopying_vert: {
        SLList_elem<SectData> *sd;
        ScSection *sec,
                  *from,*to;
        int lnr1, snr1,
            to_lnr, to_snr;
        for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // erase ghost notes
          lnr1=sd->d.lnr + delta_lnr;
          snr1=sd->d.snr + delta_snr + left_snr;
          if (lnr1>=0 && lnr1<sclin_max && snr1>=0 && snr1<score->len)
            drawS_ghost(this,snr1,lnr1,sd->d.sign,true);
        }
        if (key_pressed!='k') {   // not keep last score
          if (delta_lnr || delta_snr || selected.sv!=this) {
            selected.check_direction(delta_lnr,delta_snr);
            for (sd=selected.sd_list.lis;sd;) {
              to_lnr=sd->d.lnr + delta_lnr;
              to_snr=sd->d.snr + delta_snr + left_snr;
              from=selected.sv->score->get_section(sd->d.lnr,sd->d.snr);
              if (to_lnr>=0 && to_lnr<sclin_max && to_snr>=0) {
                if (score->check_len(to_snr+1)) {
                  set_scroll_range(); draw_sc2(false);
                }
                to=score->get_section(to_lnr,to_snr);
                ScSection from1(*from);
                if (from->sel_color) from1.nxt_note=0;
                for (sec=&from1;sec;sec=sec->nxt()) {
                  sec->s_group=score->ngroup; // maybe copied from different score
                  if (state==eMoving_vert || state==eCopying_vert)  // sign is not copied
                    sec->sign=score->lin[to_lnr].n_sign;
                }
                if (!to->prepend(&from1,0,0)) return;
                to->rm_duplicates();

                if (state==eMoving_hor || state==eMoving_vert) {
                  if (from->port_dlnr) from->drawPortaLine(selected.sv,sd->d.snr,sd->d.lnr,true);
                  from->set_to_silent(selected.sv,sd->d.snr,sd->d.lnr);
                }
                else {
                  from->sel=false;
                  from->drawSect(selected.sv,selected.sv->other,sd->d.snr,sd->d.lnr);
                }
                sd->d.lnr=to_lnr;
                sd->d.snr=to_snr;
                sd->d.sign=to->sign;
                if (to_lnr>=0 && to_lnr<sclin_max) {
                  to->drawSect(this,other,to_snr,to_lnr);
                  if (to->port_dlnr) to->drawPortaLine(this,to_snr,to_lnr,false);
                }
                sd=sd->nxt;
              }
              else {
                if (state==eMoving_hor || state==eMoving_vert) {
                  from->set_to_silent(selected.sv,sd->d.snr,sd->d.lnr);
                }
                else {
                  from->sel=false;
                  from->drawSect(selected.sv,selected.sv->other,sd->d.snr,sd->d.lnr);
                }
                sd=selected.sd_list.remove(sd);
              }
            }
          }
          if (state==eCopying_hor || state==eMoving_hor) {  // update endline?
            int max_snr=0;
            for (sd=selected.sd_list.lis;sd;sd=sd->nxt) // find highest snr
              if (max_snr<sd->d.snr) max_snr=sd->d.snr;
            if (score->end_sect && max_snr>=score->end_sect) {
              draw_endline(false);
              if (other->score==score) other->draw_endline(false);
              score->end_sect=max_snr+1;
              draw_endline(true);
              if (other->score==score) other->draw_endline(true);
            }
          }
          selected.sv=this;
        }
      }
      ma->reset();
      break;
    case ePortaStart:
      ma->reset();
      set_custom_cursor(scview->win,up_arrow_cursor);
      { uchar sign;
        int snr=sectnr(x);
        const int lnr=linenr(y,snr,sign);
        if (lnr-cur_lnr==0 && snr-cur_snr==0) { alert("portando: same note"); break; }
        if (abs(lnr-cur_lnr)>= 1<<5) { alert("portando notes height difference >= %d",1<<5); break; }
        if (snr-cur_snr<1) { alert("portando notes distance < 0"); break; }
        if (snr-cur_snr>= 1<<5) { alert("portando notes distance >= %d",1<<5); break; }
        ScSection *const sect=score->get_section(lnr,cur_snr);
        ScSection *sec;
        for (sec=score->get_section(lnr,snr);sec;sec=sec->nxt()) {
          if (sec->cat==ePlay && !sec->sampled && sect->s_col==sec->s_col)
            break;
        }
        if (!sec) { alert("portando note mismatch"); break; }
        sect->port_dlnr= lnr-cur_lnr;
        sect->port_dsnr= snr-cur_snr-1;
        sect->drawPortaLine(this,cur_snr,cur_lnr,false);
      }
      break;
    case eCollectSel:
    case eCollectColSel:
    case eIdle:
    case eTracking:
    case eErasing:
      break;
    case eMoving:
    case eCopying:
    case eToBeReset:
      if (keep_m_action) keep_m_action=false;
      else ma->reset();
      break;
    default:
      alert("mouse-up state %d?",state);
  }
  state=eIdle;
}

ScoreView::ScoreView(Rect rect,int ind):
    ScoreViewBase(rect,0),
    index(ind) {
  mnrview=new BgrWin(top_win->win,Rect(rect.x+s_wid,rect.y,scview_wid-s_wid,mnr_height),
                    FR,draw_cmd,mouse_down,0,0,cBgrGrey,Id('scmn',index));
  set_custom_cursor(mnrview->win,text_cursor);
  scview=new BgrWin(top_win->win,Rect(rect.x+s_wid,rect.y+mnr_height,scview_wid-s_wid,rect.height-scrollbar_wid-mnr_height),
                    FR,draw_cmd,mouse_down,mouse_moved,mouse_up,cWhite,Id('scv',index));
  set_custom_cursor(scview->win,up_arrow_cursor);
  signsview=new BgrWin(top_win->win,Rect(rect.x,rect.y+mnr_height,s_wid-1,rect.height-mnr_height-scrollbar_wid),
                      FN,drawsigns_cmd,cBgrGrey,1,Id(0,index));

  chord_name=new BgrWin(scview->win,
                        Rect(20,rect.height-scrollbar_wid-mnr_height-TDIST-1,200,TDIST),
                        MR,draw_chn,cBackground,1,Id(0,index));
  chord_name->hide(); 

  scroll=new HScrollbar(top_win->win,Rect(rect.x+s_wid,rect.y+rect.height-scrollbar_wid+1,scview_wid-s_wid,0),
                        Style(1,0,sect_length),FR,sect_scv*sect_len+10,scroll_cmd,Id(0,index));

  text_win=new BgrWin(top_win->win,Rect(rect.x+scview_wid+1,rect.y,rect.width-scview_wid,rect.height+1),
                     MR,0,cForeground);

  sc_name=new TextWin(text_win->win,Rect(2,2,rect.width-scview_wid-6,0),FN,1);

  group_nr=new RButWin(text_win,Rect(58,32,18,3*TDIST),FN,"gr#",false,rbutwin_cmd,cForeground,Id('grnr',index));
  for (int i=0;i<3;++i)
    group_nr->add_rbut(i==0?"0":i==1?"1":"2");

  zoom=new CheckBox(text_win->win,Rect(2,42,0,15),FN,cForeground,"zoom",zoom_cmd,Id(0,index));

  set_repeat=new CheckBox(text_win->win,Rect(2,59,0,15),FN,cForeground,"repeat",0,Id(0,index));

  play_1col=new CheckBox(text_win->win,Rect(2,76,0,15),FN,cForeground,"play 1 color",0,Id(0,index));

  display_mode=new RButWin(text_win,Rect(2,110,rect.width-scview_wid-6,4*TDIST),FN,"display",false,display_cmd,cForeground,Id(0,index));
  display_mode->add_rbut("instruments");
  display_mode->add_rbut("timing");
  display_mode->add_rbut("accidentals");
  display_mode->add_rbut("white keys");

  new Button(text_win->win,Rect(2,187,33,0),FN,"play",svplay_cmd,Id(0,index));

  ext_rbut_style.bgcol=cBackground;
  active=app->active_scoreCtrl->add_extrbut(text_win->win,Rect(38,174,42,0),FN,"active",Id(0,index));
  ext_rbut_style=def_erb_st;

  the_chord_name[0]=0;
}

void ScSection::drawPlaySect(ScoreViewBase *theV,Point start,Point end,uchar n_sign) {
  uint s_color= nxt_note ? eGrey : s_col;
  int mid=end.x;
  if (theV->zoomed) {
    if (del_start)
      start.x += del_start*sect_length;
    if (del_end)
      end.x += del_end*sect_length;
  }
  switch (theV->draw_mode) {
    case eColorValue:
    case ePiano: {
        static uint sel_bgcolor[eGrey+1]= {  // selected note background color
          cGrey,cGrey,cGrey,cWhite,cGrey,cWhite,cWhite };
        if (sel) theV->scview->set_selected(true,sel_bgcolor[s_color]);
        set_color(col2color(s_color));
      }
      break;
    case eTimingValue:
      if (del_start) set_color(cRed);
      else if (del_end && !theV->zoomed) set_color(cBlue);
      else set_color(cDarkGrey);
      break;
    case eAccValue:
      if (!sampled && n_sign!=sign) {
        switch (n_sign) {
          case 0: set_color(sign==eHi ? cRed : cBlue); break;
          case eHi: set_color(sign==0 ? cBlack : cBlue); break;
          case eLo: set_color(sign==0 ? cBlack : cRed); break;
        }
      }
      else set_color(cGrey);
      break;
  }
  if (sampled) {
    if (theV->draw_mode!=ePiano) {
      set_width(1);
      theV->scview->draw_line(Point(start.x,start.y-2),Point(start.x+6,start.y+1));
      theV->scview->draw_line(Point(start.x,start.y-1),Point(start.x+6,start.y+2));
      theV->scview->draw_line(Point(start.x,start.y+1),Point(start.x+6,start.y-2));
      theV->scview->draw_line(Point(start.x,start.y+2),Point(start.x+6,start.y-1));
    }
    if (sign) {
      set_color(cWhite);
      if (sign==eHi) theV->scview->fill_rectangle(Rect(start.x+3,start.y-2,3,2));
      else if (sign==eLo) theV->scview->fill_rectangle(Rect(start.x+3,start.y+1,3,2));
    }
  }
  else {
    if (stacc) end.x-=2;
    set_width(3);
    if (theV->zoomed && theV->draw_mode==eTimingValue && del_end) {
      theV->scview->draw_line(start,Point(mid,start.y));
      set_color(cBlue);
      theV->scview->draw_line(Point(mid,start.y),end);
    }
    else
      theV->scview->draw_line(start,end);
    if (sign && theV->draw_mode!=ePiano) {
      set_width_color(1,cWhite);
      start.x+=2;
      if (sign==eHi) { --start.y; --end.y; }
      else if (sign==eLo) { ++start.y; ++end.y; }
      theV->scview->draw_line(start,end);
    }
  }
  if (sel) theV->scview->set_selected(false,0); // reset line style
}

void ScSection::drawSect(ScoreViewBase* theV,int snr,int lnr) { 
  const int x=(snr-theV->leftside)*theV->sect_len,
            y= theV->draw_mode==ePiano ? ypos(lnr,sign,theV->piano_y_off) : ypos(lnr);
  const uchar n_sign=theV->score->lin[lnr].n_sign;
  Point start(x,y),
        end(x+theV->sect_len,y);
  set_width_color(theV->draw_mode==ePiano ? 3 : 5,cWhite);
  theV->scview->draw_line(start,end);
  switch (cat) {
    case eSilent:
      --end.x;
      if (theV->draw_mode==ePiano) {
        uint col=theV->p_lnr2col(lnr,sign);
        if (col) {
          set_width_color(1,col);
          theV->scview->draw_line(start,end);
        }
      }
      else {
        set_width_color(1,linecol.col[lnr]);
        theV->scview->draw_line(start,end);
        if (lnr%2==0) {
          int meter=theV->score->sc_meter;
          if (!meter) meter=app->act_meter;
          if (snr%meter==0) {   // timing marks between lines
            set_color(cGrey);
            theV->scview->draw_line(Point(x,y-1),Point(x,y+1));
          }
        }
      }
      break;
    case ePlay:
      drawPlaySect(theV,start,end,n_sign);
      break;
    default: alert("section cat %d?",cat);
  }
}

void ScSection::drawSect(ScoreViewBase* theV,ScoreViewBase* otherV,int snr,int lnr) {
  drawSect(theV,snr,lnr);
  if (otherV->score==theV->score) drawSect(otherV,snr,lnr);
}

void ScSection::drawPortaLine(ScoreViewBase *theV,int snr,int lnr,bool erase) {
  int x1=(snr-theV->leftside+1)*theV->sect_len,
      y1=theV->draw_mode==ePiano ? ypos(lnr,sign,theV->piano_y_off) : ypos(lnr),
      x2=x1+port_dsnr*theV->sect_len,
      y2=theV->draw_mode==ePiano ? ypos(lnr+port_dlnr,0,theV->piano_y_off) : ypos(lnr+port_dlnr);
  // piano mode: y2 incorrect if sign != 0
  Point start(x1,y1),
        end(x2,y2);
  set_width(1);
  if (erase) set_color(cWhite);
  else set_color(col2color(s_col));
  theV->scview->draw_line(start,end);
}

void ScoreView::draw_endline(bool draw_it) {
  if (!score->end_sect) return;
  int x=(score->end_sect-leftside)*sect_len;
  if (draw_it) set_width_color(1,cBlack);
  else set_width_color(1,cWhite);
  scview->draw_line(Point(x,ypos(17)),Point(x,ypos(25)));
  if (!draw_it && draw_mode!=ePiano && score->end_sect<score->len) // restore sections at old endline
    for (int n=17;n<25;++n)
      score->get_section(n,score->end_sect)->drawSect(this,score->end_sect,n);
}

void ScoreViewBase::draw_endline(bool) {
  if (!score->end_sect) return;
  int x=(score->end_sect-leftside)*sect_len;
  set_width_color(1,cBlack);
  scview->draw_line(Point(x,ypos(17)),Point(x,ypos(25)));
}

void ScoreView::draw_cmd(Id id) { 
  ScoreView *sv=the_sv(id.id2);
  sv->set_scroll_range();  // needed after resizing
  sv->draw_sc2(false);
}

void ScoreView::drawsigns_cmd(Id id) {  the_sv(id.id2)->drawSigns(); }

void ScoreView::drawSigns() {
  if (!score) return;
  static const char 
    *sharp[]={
      "5 6 2 1",
      "# c #000000",
      ". c #ffffff",
      "..#.#",
      "#####",
      ".#.#.",
      ".#.#.",
      "#####",
      "#.#.."
    },
    *flat[]={
      "5 6 2 1",
      "# c #000000",
      ". c #ffffff",
      ".#...",
      ".#...",
      ".###.",
      ".#..#",
      ".#.#.",
      ".##.."
  };
  static Pixmap
    sharp_pm=create_pixmap(sharp).pm,
    flat_pm=create_pixmap(flat).pm;
  int lnr,
      y;
  uint sign;
  signsview->clear();
  for (lnr=0;lnr<sclin_max;++lnr) {
    y=ypos(lnr)-3;
    sign=score->lin[lnr].n_sign;
    if (sign)
      draw_pixmap(signsview->win,Point((lnr%7 & 1)==0 ? 2 : 7,y),sign==eHi ? sharp_pm : flat_pm,5,6);
  }
}

void ScoreViewBase::draw_scview(int start_snr,int stop_snr,int meter) { // draw_mode != ePiano
  int lnr, snr,
      x1, y1;
  Point start,
        end;
  uint line_color;
  for (lnr=0;lnr<sclin_max;++lnr) {
    y1=ypos(lnr);
    line_color=linecol.col[lnr];
    ScSection *sect=score->get_section(lnr,0),
              *sec;
    for (snr=start_snr;snr<score->len && snr<stop_snr;++snr) {
      x1=(snr-leftside)*sect_len;
      start.set(x1,y1);
      end.set(x1+sect_len-2,y1);
      if (line_color!=cWhite) {
        set_width_color(1,line_color);
        scview->draw_line(start,end);
      }
      if (snr%meter==0 && lnr%2==0) { // timing marks between lines
        set_color(cGrey);
        scview->draw_line(Point(x1,y1-1),Point(x1,y1+1));
      }
    }
    for (snr=leftside;snr<score->len && snr<stop_snr;++snr) {  // loop again
      sec=sect+snr;
      x1=(snr-leftside)*sect_len;
      if (sec->cat==ePlay && (snr>=start_snr || sec->port_dlnr)) {
        start.set(x1,y1);
        end.set(x1+sect_len,y1);
        sec->drawPlaySect(this,start,end,score->lin[lnr].n_sign);
        for (ScSection *sec1=sec;sec1;sec1=sec1->nxt()) {
          if (sec1->port_dlnr)
            sec1->drawPortaLine(this,snr,lnr,false);
        }
      }
    }
  }
}

void ScoreViewBase::p_draw_scview(int start_snr,int stop_snr,int meter) { // draw_mode = ePiano
  int lnr,snr,
      x1,y1,
      diff= scale_dep==eScDep_lk ? keynr2ind(app->chordsWin->the_key_nr) :
            scale_dep==eScDep_sk ? keynr2ind(score->sc_key_nr) :
            0;
  Point start,end;
  set_width_color(1,cGrey);
  for (snr=start_snr;snr<score->len && snr<stop_snr;++snr) { // timing lines
    if (snr%meter==0) {
      x1=(snr-leftside) * sect_len;
      scview->draw_line(Point(x1,y_off),Point(x1,(piano_lines_max-1) * p_sclin_dist + piano_y_off));
    }
  }
  for (int nr=0;nr<piano_lines_max;++nr) {  // lines
    uint col=p_scline_col[(nr + diff) % 24];
    if (col) {
      set_color(col);
      y1=nr * p_sclin_dist + piano_y_off;
      start.set(0,y1);
      end.set(0,y1);
      for (snr=start_snr;snr<score->len && snr<stop_snr;++snr) {
        x1=(snr-leftside) * sect_len;
        start.x=x1;
        end.x=x1+sect_len-1;
        scview->draw_line(start,end);
      }
    }
  }
  for (lnr=0;lnr<sclin_max;++lnr) {  // notes
    ScSection *sect=score->get_section(lnr,0),
              *sec;
    for (snr=leftside;snr<score->len && snr<stop_snr;++snr) {
      sec=sect+snr;
      if (sec->cat==ePlay  && (snr>=start_snr || sec->port_dlnr)) {
        x1=(snr-leftside) * sect_len;
        y1=ypos(lnr,sec->sign,piano_y_off);
        start.set(x1,y1);
        end.set(x1+sect_len,y1);
        sec->drawPlaySect(this,start,end,score->lin[lnr].n_sign);
        for (ScSection *sec1=sec;sec1;sec1=sec1->nxt()) {
          if (sec1->port_dlnr)
            sec1->drawPortaLine(this,snr,lnr,false);
        }
      }
    }
  }
}

void ScoreViewBase::draw_sc2(bool clear,int delta) {
  if (!score) return;
  int smax=scview->dx/sect_len,
      start_snr,stop_snr;
  if (delta<=-smax || delta>=smax) {
    delta=0; clear=true; // else wrong results
  }
  if (delta<0) {
    start_snr=leftside+smax+delta;
    stop_snr=leftside+smax;
  }
  else if (delta>0) {
    start_snr=leftside;
    stop_snr=leftside+delta;
  }
  else {
    if (clear) { scview->clear(); mnrview->clear(); }
    start_snr=leftside;
    stop_snr=leftside+smax;
  }
  int meter= score->sc_meter==0 ? app->act_meter : score->sc_meter;
  ScoreView *sv=static_cast<ScoreView*>(this);
  MusicView *mv=static_cast<MusicView*>(this);
  if (draw_mode==ePiano) { 
    p_draw_scview(start_snr,stop_snr,meter);
    if (sv_type==eMusic)
      mv->draw_tune_names();
    else {
      sv->signsview->hide();
      if (sv->the_chord_name[0]) sv->chord_name->map();
    }
  }
  else {
    draw_scview(start_snr,stop_snr,meter);
    if (sv_type==eMusic)
      mv->draw_tune_names();
    else {
      sv->signsview->map(); sv->drawSigns();
      sv->chord_name->hide();
    }
  }
  draw_start_stop(score);
  draw_endline(true);
}

void ScoreView::assign_score(Score *sc,bool draw_it) {
  score=sc;
  group_nr->set_rbutnr(sc->ngroup,false);
  sc_name->print_text(tnames.txt(sc->name));
  play_start=0; play_stop=-1; leftside=0;
  scroll->value=0;
  if (draw_it) {
    set_scroll_range();
    draw_sc2(true);
  }
}

void ScoreView::modify_sel(int mes) {
  SLList_elem<SectData> *sd;
  ScSection *sect;
  int shift=0, // shift>0 -> midinr increase, linenr decrease
      mnr;
  bool warn=false;
  switch (mes) {
    case 'shmu': case 'shmd':
    case 'shcu': case 'shcd':
      shift=app_local->mouseAction->semi_tones->value;
      if (mes=='shmd' || mes=='shcd') shift= -shift;
      selected.check_direction(-shift,0);
      break;
  }
  for (sd=selected.sd_list.lis;sd;) {
    sect=score->get_section(sd->d.lnr,sd->d.snr);
    switch (mes) {
      case 'uns':
        sect->sel=false;
        break;
      case 'rcol':
        if (sect->sel_color || !sect->nxt_note) {
          sect->s_col=app->act_color;
          if (!sect->sampled && app->sampl->value) {
            sect->sampled=true;
          }
          else if (sect->sampled && !app->sampl->value) {
            sect->sampled=false;
          }
        }
        else 
          alert("trying to re-color multiple note in measure %d",score->get_meas(sd->d.snr));
        break;
      case 'del ':
        if (sect->port_dlnr)
          sect->drawPortaLine(this,sd->d.snr,sd->d.lnr,true);
        if (sect->sel_color && sect->nxt_note)
          *sect=*sect->nxt();
        else
          sect->cat=eSilent; // reset() later
        break;
      case 'shmu': case 'shmd':    // shift move
      case 'shcu': case 'shcd': {  // shift copy
          bool move= mes=='shmu' || mes=='shmd';
          mnr=lnr_to_midinr(sd->d.lnr,sd->d.sign) + shift;
          uchar lnr,sign;
          if (midinr_to_lnr(mnr,lnr,sign,score->signs_mode)) {
            ScSection *to=score->get_section(lnr,sd->d.snr);
            ScSection from1(*sect);
            if (sect->sel_color) from1.nxt_note=0;
            if (move) {
              sect->set_to_silent(this,sd->d.snr,sd->d.lnr);
            }
            else {
              sect->sel=false;
              sect->drawSect(this,other,sd->d.snr,sd->d.lnr);
            }
            if (!to->prepend(&from1,0,0)) return;
            to->rm_duplicates();
            sd->d.sign=to->sign=sign;
            sd->d.lnr=lnr;
            sect=to;
          }
          else {
            warn=true;
            sect->sel=false;
            sect->drawSect(this,other,sd->d.snr,sd->d.lnr);
            sd=selected.sd_list.remove(sd);
            continue;
          }
        }
      break;
    }
    sect->drawSect(this,other,sd->d.snr,sd->d.lnr);
    if (sect->cat==eSilent) sect->reset(); // after drawSect()!
    sd=sd->nxt;
  }
  switch (mes) {
    case 'del ':
    case 'uns':
      selected.reset();
      draw_endline(true);
      break;
    case 'shmu': case 'shmd': if (warn) alert("warning: some notes not shifted"); break;
    case 'shcu': case 'shcd': if (warn) alert("warning: some notes not copied"); break;
  }
}

bool eq(char *word,int colnr,const char *col,const char *param,const char *alt_param,char *&s) {
// word="attack", color_name[colnr]="red", param="attack", alt_param="at"
  static char sbuf[40];  // used for error messages
  s=0;
  if (!strcmp(color_name[colnr],col) && ((alt_param && !strcmp(word,alt_param)) || !strcmp(word,param))) {
    snprintf(sbuf,40,"%s %s",color_name[colnr],param);
    s=sbuf;
    return true;
  }
  return false;
}

bool eq(char *word,int colnr,const char *col,const char *param_0,const char *param_1,const char *param_2,int &p_nr,char *&s) {
// word="ampl-0", color_name[colnr]="black", param_0="ampl-0", param_1="ampl-1", param_2="ampl-2"
  static char sbuf[40];  // used for error messages
  s=0; p_nr=0;
  if (!strcmp(color_name[colnr],col) &&
      (!strcmp(word,param_0) || (!strcmp(word,param_1) && (p_nr=1)) || (!strcmp(word,param_2) && (p_nr=2)))) {
    snprintf(sbuf,40,"%s %s",color_name[colnr],param_0);
    s=sbuf;
    return true;
  }
  return false;
}

bool App::save(const char *fname) {
  FILE *tunes;
  int i;
  Encode enc;
  Score *scp;
  ScSection *sec,*sect,
            *lst_sect;
  int scorenr,
      snr,lnr;
  bool warn_dupl=false,
       warn;
  if ((tunes=fopen(fname,"w"))==0) {
    alert("file '%s' not writable",fname);
    return false;
  }
  fputs(enc.set_meter(act_meter),tunes);
  for (scorenr=0;scorenr<=scores.lst_score;++scorenr) {
    scp=scores[scorenr];
    int met=scp->sc_meter;
    if (met==act_meter) met=0; // end_sect = default
    fprintf(tunes,"\n%s \"%s\" ",
      enc.score_start4(scorenr,met,scp->end_sect,scp->ngroup,scp->sc_key_nr),tnames.txt(scp->name));
    for (snr=0;snr<scp->len-1 && (snr==0 || scp->end_sect!=snr);++snr) {   // 1 extra for end
      for (lnr=0;lnr<sclin_max;lnr++) {
        sect=scp->get_section(lnr,snr);
        switch (sect->cat) {
          case ePlay_x:
          case ePlay:
            if (sect->nxt_note) {
              sect->rm_duplicates(&warn); // else same_color() erroneous
              if (warn) {
                alert("duplicate multiple note in %s (meas nr %d)",
                      tnames.txt(scp->name),
                      snr/(scp->sc_meter==0 ? app->act_meter : scp->sc_meter));
                 warn_dupl=true;
              }
            }
            for (sec=sect;sec;sec=sec->nxt()) {
              if (sec->cat==ePlay) {
                i=same_color(sect,sec,scp->get_section(lnr,0)+scp->len,lst_sect);
                fputs(enc.play_note(lnr,snr,i,sec->sign,
                                    sec->sampled ? 2 : lst_sect->stacc ? 1 : 0,
                                    sec->s_col,sec->s_group,
                                    lst_sect->port_dlnr,
                                    lst_sect->port_dsnr,
                                    sec->del_start,lst_sect->del_end),tunes);
              }
            }
            break;
          case eSilent:
            break;
          default:
            alert("save cat=%u?",sect->cat);
        }
      }
    }
    restore_marked_sects(scp,0);
  }
  putc('\n',tunes); // Unix tools need this
  fclose(tunes);
  if (warn_dupl) alert("Repair duplicates, then save again!");
  return true;
}

bool App::read_tunes(const char *fname,bool add_tunes) {
  static TunesView *tv=app_local->tunesView;
  int fst_scnr;
  if (add_tunes) {
    fst_scnr=scores.lst_score+1;
    if (!restore(fname,true))
      return false;
  }
  else {
    fst_scnr=0;
    tv->rbutwin->empty(); 
    if (!restore(fname,false)) {  // sets scores.lst_score
      for (int n=0;n<2;++n) the_sv(n)->reset();
      return false;
    }
  }
  for (int n=fst_scnr;n<=scores.lst_score;++n) {
    scores[n]->rbut=tv->rbutwin->add_rbut(tnames.txt(scores[n]->name));
    if (!add_tunes && n<2)
      the_sv(n)->assign_score(scores[n],true);
  }
  tv->scroll->value=0; tv->set_scroll_range();
  return true;
}

Score *new_score(const char *name) {
  Score *sc=scores.new_score(name);
  TunesView *tv=app_local->tunesView;
  sc->rbut=tv->rbutwin->add_rbut(tnames.txt(sc->name));
  tv->set_scroll_range();
  return sc;
}

void App::new_tune(const char *tname) {
  Score *sc=new_score(tname);
  dia_wd->dlabel("tune added",cForeground);
  app_local->info_view->set_modif(eSco,true);
  if (act_score>nop) {
    ScoreView *sv=the_sv(act_score);
    if (selected.sv==sv) selected.restore_sel();
    sv->assign_score(sc,true);
    check_reset_as();
  }
}

Score* Scores::exist_name(const char *nam) {
  if (!nam || !nam[0]) { alert("null name"); return 0; }
  for (int i=0;i<=lst_score;++i) {
    if (!strcmp(tnames.txt(buf[i]->name),nam)) return buf[i];
  }
  return 0;
}

void Score::copy(Score *from) { // name and group NOT copied
                                // group, end_sect, sc_meter, amplitude and color copied
  int snr,lnr,
      stop;
  end_sect=from->end_sect;
  sc_meter=from->sc_meter;
  if (end_sect) {
    check_len(end_sect+1); stop=end_sect;
  }
  else {
    stop=from->len; check_len(stop);
  }
  for (lnr=0;lnr<sclin_max;lnr++) {
    lin[lnr].n_sign=from->lin[lnr].n_sign;
    for (snr=0;snr<stop;++snr) {
      ScSection *to=get_section(lnr,snr);
      *to=*from->get_section(lnr,snr);
      to->sel=false;
    }
  }
  ngroup=from->ngroup;
  signs_mode=from->signs_mode;
  sc_key_nr=from->sc_key_nr;
}

void Score::add_copy(Score *sc,int start,int stop,int tim,
                     int shift,int raise,ScoreViewBase* theV,int atcolor) {
    // name, nampl NOT copied
  int snr,snr1,
      lnr;
  uchar lnr1,
        sign;
  ScSection from,*sec,
            *to;
  check_len(tim+stop-start);
  if (sc_type==eMusic) {
    ScInfo info(eText);
    info.text=tnames.txt(sc->name);
    scInfo[tim].add(info);
  }
  for (snr=start,snr1=tim-1;snr<stop;++snr) {
    ++snr1;
    for (lnr=0;lnr<sclin_max;lnr++) {
      from=*sc->get_section(lnr,snr);
      if (from.cat==ePlay) {
        if (raise || shift) {
          if (raise) {
            lnr1=lnr-raise;
            sign=sc->lin[lnr1].n_sign; // sign of to-linenr of from-score
          }
          if (shift) {
            if(!midinr_to_lnr(lnr_to_midinr(lnr,from.sign)+shift,lnr1,sign,signs_mode)) continue;
          }
        }
        else { lnr1=lnr; sign=from.sign; }
        to=get_section(lnr1,snr1);

        uint lst_cp=0;
        if (!to->prepend(&from,sc_type,&lst_cp)) return;
        to->sel=false;
        to->sign=sign;
        if (atcolor>=0) to->s_col=atcolor;
        for (sec=to;;sec=sec->nxt()) {
          if (sc_type==eMusic) // assign tune name
            sec->src_tune= sc->name<tn_max-1 ? sc->name : tn_max-1;
          else if (sc_type!=eScorebuf)  // assign group
            sec->s_group=ngroup;
          if (!lst_cp || sec->nxt_note==lst_cp) break;
        }
        to->rm_duplicates(); // after group or tune name assign!
      }
    }
  }
  if (snr1>lst_sect) {
    end_sect=snr1+1;
    check_len(end_sect+1);
    lst_sect=snr1;
  }
}

void MusicView::draw_tune_name(int snr) {
  int x=(snr - leftside) * sect_len,
      y;
  if (x>scview->dx)
    return;
  ScInfo* sci;
  for (sci=score->scInfo+snr,y=12;sci;sci=sci->next)
    if (sci->tag==eText) {
      xft_draw_string(tnview->xft_win,xft_Black,Point(x,y),sci->text);
      y+=10;
    }
}

void App::copy_tune(const char *tname) {
  if (!act_tune) return;
  Score *sc=scores.exist_name(tname);
  ScoreView *sv;
  if (sc) {
    sc->reset(true);
    sc->copy(act_tune);
    if (app->find_score(sc,sv)) {
      sv->group_nr->set_rbutnr(sc->ngroup,false);
      sv->set_scroll_range();
      sv->draw_sc2(true);
    }
  }
  else {
    sc=new_score(tname);
    sc->copy(act_tune);
  }
  if (act_score>nop) {
    sv=the_sv(act_score);
    if (selected.sv==sv) selected.restore_sel();
    sv->assign_score(sc,true);
    check_reset_as();
  }
  app_local->info_view->set_modif(eSco,true);
}

bool App::find_score(Score* sc,ScoreView*& sv) {
  for (int n=0;n<2;++n) {
    sv=the_sv(n);
    if (sv->score && sv->score==sc)
      return true;
  }
  return false;
}

AppLocal::AppLocal() {
  Rect rect(0,0,0,0),
       rect2(0,0,0,0);

  mouseAction=new MouseAction();

  rect.set(sced_wid+4,0,s_wid+sect_scv*sect_length+score_info_len+2,scview_vmax);

  scViews[0]=new ScoreView(rect,0);
  rect.y+=scview_vmax+4;
  scViews[1]=new ScoreView(rect,1);
  rect.y+=scview_vmax+4;
  for (int n=0;n<2;n++)
    if (n<=scores.lst_score) scViews[n]->assign_score(scores[n],false);
  scViews[0]->other=scViews[1];
  scViews[1]->other=scViews[0];

  rect.set(2,rect.y,sect_mus_max*sect_length,scview_vmax+tnview_height+1);
  musicView=new MusicView(rect);

  rect.set(view_hmax-72,24,68,0);
  new Button(top_win->win,rect,MR,"new tune...",dial_cmds.new_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"copy tune...",dial_cmds.copy_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"rename...",dial_cmds.rename_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,Rect(rect.x,rect.y,18,0),MR,arrow_up,mvup_cmd,Id('up'));
  new Button(top_win->win,Rect(rect.x+22,rect.y,18,0),MR,arrow_down,mvdo_cmd,Id('do'));
  new Button(top_win->win,Rect(rect.x+44,rect.y,18,0),MR,cross_sign,remove_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"clear tune",clear_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"mod times",modt_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,Rect(rect.x,rect.y,44,rect.height),MR,"cmd...",dial_cmds.cmd_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"run script",run_script_cmd);

  rect.y+=but_dist; rect.width=36;
  new Button(top_win->win,rect,MR,"play",mplay_cmd);

  rect.y+=but_dist;
  new Button(top_win->win,rect,MR,"stop",stop_cmd);

  menus=new Menus(Rect(view_hmax-rbutview_left,0,rbutview_left-2,20));

  rect.set(view_hmax-rbutview_left,TDIST+24,100,110);
  tunesView=new TunesView(rect);

  rect2.set(rect.x+rect.width+5,TDIST+24,66,slider_height);
  meterView=new MeterView(rect2);

  rect2.y=rect2.y+rect2.height+13;
  tempoView=new TempoView(rect2);

  rect2.set(rect2.x+10,rect2.y+rect2.height+14,50,colors_max*TDIST);
  colorView=new ColorView(rect2);

  rect.set(rect.x,rect.y+rect.height+4,0,15);
  button_style.set(1,cBackground,0);
  new Button(top_win->win,rect,MR,"chords window",chords_cmd);
  button_style=def_but_st;

  rect.y+=checkbox_height;
  app->sampl=new CheckBox(top_win->win,rect,MR,cBackground,"sampled notes",raw_cmd);

  rect.y+=checkbox_height;
  app->no_set=new CheckBox(top_win->win,rect,MR,cBackground,"ignore set cmd's",0);

  rect.y=rect2.y+rect2.height+2;
  app->conn_mk=new CheckBox(top_win->win,rect,MR,cBackground,"connect midi keyboard",con_mk_cmd);

  rect.set(view_hmax-rbutview_left,rect.y+18,rbutview_left-2,34);
  textView=new Question(rect);

  rect.set(rect.x,rect.y+38,rect.width,ictrl_height);
  app->act_instr_ctrl=app->black_control=new FMCtrl(rect,eBlack);
  (app->red_control=new RedCtrl(rect,eRed))->cview->hide();
  (app->green_control=new GreenCtrl(rect,eGreen))->cview->hide();
  (app->blue_control=new BlueCtrl(rect,eBlue))->cview->hide();
  (app->brown_control=new FMCtrl(rect,eBrown))->cview->hide();
  (app->purple_control=new PurpleCtrl(rect,ePurple))->cview->hide();

  (app->black_phm_control=new PhmCtrl(rect,eBlack))->cview->hide();
  (app->red_phm_control=new PhmCtrl(rect,eRed))->cview->hide();
  (app->green_phm_control=new PhmCtrl(rect,eGreen))->cview->hide();
  (app->blue_phm_control=new PhmCtrl(rect,eBlue))->cview->hide();
  (app->brown_phm_control=new PhmCtrl(rect,eBrown))->cview->hide();
  (app->purple_phm_control=new PhmCtrl(rect,ePurple))->cview->hide();

  rect.y += ictrl_height+4;
  app->scopeView=new ScopeView(Rect(rect.x,rect.y,scope_dim,68));

  revb_view=new ReverbView(Rect(rect.x+171,rect.y,72,68));

  rect.y+=71;
  info_view=new InfoView(Rect(rect.x,rect.y,rect.width,76));

  mv_display=new RButWin(info_view->bgwin,Rect(196,32,47,3*TDIST),FN,"display",false,mv_display_cmd,cBackground);
  mv_display->add_rbut("instr");
  mv_display->add_rbut("timing");
  mv_display->add_rbut("w.keys");

  rect.y+=78;
  editScript=new EditScript(Rect(rect.x,rect.y,view_hmax-rect.x-2,view_vmax-rect.y-2));
}

App::App(char *inf):
    act_tune(0),
    cur_score(0),
    input_file(inf),
    act_score(nop),
    act_color(cBlack),
    act_meter(8),
    act_action(0),
    act_tune_ind(0),
    act_tempo(11),
    mv_key_nr(0),
    nupq(4),
    task(0),
    stop_requested(false),
    repeat(false),
    chordsWin(0) {
  app=this;

  active_scoreCtrl=new ExtRButCtrl(ScoreView::active_cmd);
  scoreBuf=new Score(0,sect_scv,eScorebuf);
  if (inf) {
    modify_cwd(input_file);
    char *s=input_file.get_ext();
    if (s && !strcmp(s,".scr")) {
      script_file.cpy(input_file.s);
      input_file.new_ext(".sco");
      if (!restore(input_file.s,false)) inf=0;
    }
    else if (!restore(input_file.s,false)) inf=0;
  }
  else {
    if (!no_def_tunes) {
      scores.new_score("tune1");
      scores.new_score("tune2");
    }
  }
  if (output_port==eJack) {
    if ((jack_interface=new JackInterf("Amuc",play,'done',i_am_playing))->okay) {
      if (jack_interface->buf_size)
        IBsize=jack_interface->buf_size;
      if (jack_interface->sample_rate) {
        SAMPLE_RATE=jack_interface->sample_rate;
      }
    }
    else {
      jack_interface=0;
      alert("Please start jackd and restart amuc");
    }
  }
  set_time_scale();
  set_buffer_size();
  app_local=new AppLocal();
  if (inf) {
    app_local->info_view->txt[eSco]->print_text(input_file.s);
  }
  if (!init_phmbuf()) return; // physical models
  if (script_file.s[0]) {
    if (read_script(script_file.s)) {
      app_local->info_view->txt[eScr]->print_text(script_file.s);
    }
  }
}

App::~App() {
  if (output_port==eJack) {
    delete jack_interface; delete wfile_play;
  }
}

bool App::read_script(const char *script) {
  static char textbuffer[2000];
  FILE *in=0;
  if (!(in=fopen(script,"r"))) {
    alert("'%s' unknown",script);
    return false;
  }
  EditScript *es=app_local->editScript;
  es->textview->read_file(in);
  fclose(in);
  es->reset_sbar();
  const char *text=es->textview->get_text(textbuffer,2000);
  if (run_script(text)) {
    MusicView *mv=app_local->musicView;
    mv->set_scroll_range(); mv->draw_sc2(false); mv->exec_info_t0();
    return true;
  }
  return false;
}

bool App::save_script(const char *s) {
  FILE *out=0;
  if (!(out=fopen(s,"w"))) {
    alert("'%s' not writable",s);
    return false;
  }
  app_local->editScript->textview->write_file(out);
  fclose(out);
  return true;
}

void App::modify_script(EditScript *editS,int start,int end) {
  char old_text[2000];
  int nr;
  Str str;
  bool ok=true;
  RewrScript rwscr(start,end,act_meter);
  editS->textview->get_text(old_text,2000);
  editS->textview->reset();
  char *s,*p,*ib;
  for (nr=-1,s=old_text;*s;s=p+1) {
    for (p=s,ib=rwscr.in_buf;;++p) {
      if (!*p) { *(ib++)='\n'; break; }
      *(ib++)=*p;
      if (*p=='\n') break;
      if (ib-rwscr.in_buf>=max200) {
        alert("modify_script: line > %d chars",max200);
        return;
      }
    }
    *ib=0;
    rwscr.rewr_line(ok);
    if (debug) printf("inl:[%s] outl:[%s]\n",rwscr.in_buf,rwscr.out_buf);
    if (!ok) break;
    editS->textview->set_line(rwscr.out_buf,++nr);
    if (!*p) break; // last line does not end with '\n'
  }
}

int RewrScript::read_rwtime(Str &str,int &pos) {
  str.strtok(in_buf," .,\n;#",pos);
  int nr=atoi(str.s) * meter;
  if (str.ch=='.') {
    str.strtok(in_buf," ,\n;#",pos);
    nr+=atoi(str.s);
  }
  return nr;
}

void RewrScript::rewr_params(Str &str,int &pos,int mode,char *&ob,bool &omit) {
  int tim;
  char *lst_ob,*prev_ob;
  omit=false;
  for (;;) {
    str.strtok(in_buf," :;\n#",pos);
    if (str.ch==':') {
      if (str=="time") {
        str_cpy(ob,"time:");
        lst_ob=prev_ob=ob;
        for(;;) {
          tim=read_rwtime(str,pos);
          if (insert_gap) {
            if (tim>=start) tim+=stop-start+1;
          }
          else {
            if (debug) printf("tim=%d start=%d stop=%d\n",tim,start,stop);
            switch (mode) {
              case eAdd_Take:
                if (tim>=start) tim+=stop-start+1;
                else if (tim>stop) {
                  ob=lst_ob;
                  goto find_comma;
                }
                break;
              case eSet:
                if (tim>=start) tim+=stop-start+1;
                else if (tim>stop) tim=stop+1;
                break;
            }
          }
          if (tim%meter > 0)
            ob += sprintf(ob,"%d.%d",tim/meter,tim%meter);
          else
            ob += sprintf(ob,"%d",tim/meter);
          find_comma:
          if (str.ch!=',') break;
          lst_ob=ob;
          str_cpy(ob,",");
        }
        if (mode==eAdd_Take && ob==prev_ob) omit=true;
      }
      else {
        str_cpy(ob,str.s);
        str_cpy(ob,":");
        str.strtok(in_buf," ;\n#",pos);
        str_cpy(ob,str.s);
      }
    }
    else
      str_cpy(ob,str.s);
    if (str.ch=='\n' || str.ch=='#') break;
    char_cpy(str.ch,ob);
  }
}

void RewrScript::rewr_line(bool &ok) {
  int pos=0;
  char *ob=out_buf,
       *prev_ob;
  bool omit;
  Str str;
  for (;;) {
    prev_ob=ob;
    str.strtok(in_buf," :;\n#",pos);
    
    if (str.ch=='#') {
      str_cpy(ob,"#");
      str_cpy(ob,in_buf+pos);
      --ob; // strip '\n'
      str.strtok(in_buf,"\n",pos);
      break;
    }
    if (!str.s[0]);
    else if (str=="put" || str=="in-par" || str=="out-par") {
      str_cpy(ob,str.s); str_cpy(ob," ");
      str.strtok(in_buf,";\n",pos);
      str_cpy(ob,str.s);
    }
    else if (str=="exit" || str=="extended-syntax") {
      str_cpy(ob,str.s);
    }
    else if (str=="set") {
      str_cpy(ob,"set ");
      rewr_params(str,pos,eSet,ob,omit);
    }
    else if (str=="add" ||
             str=="take" ||
             str=="take-nc") {
      str_cpy(ob,str.s); str_cpy(ob," ");
      rewr_params(str,pos,eAdd_Take,ob,omit);
      if (omit) ob=prev_ob;
    }
    else {
      alert("modify script: unknown cmd '%s'",str.s);
      ok=false;
      return;
    }
    if (str.ch=='\n') break;
    if (str.ch=='#') {
      str_cpy(ob," #");
      str_cpy(ob,in_buf+pos);
      --ob; // strip '\n'
      str.strtok(in_buf,"\n",pos);
      break;
    }
    char_cpy(str.ch,ob);
  }
  *ob=0;
}

bool App::restore(const char *file,bool add_tunes) {
  bool result=true;
  Encode enc;
  Score* scp=0;
  FILE *in=0;
  selected.reset();
  if (!add_tunes) {
    scores.reset();
    mn_buf.reset(0);
    tnames.reset();
    if (app_local) { the_sv(0)->reset(); the_sv(1)->reset(); }
  }
  if ((in=fopen(file,"r"))==0) {
    alert("file '%s' not found",file);
    return false;
  }
  for (;;) {
    enc.save_buf.rword(in," \n");
    if (!enc.save_buf.s[0]) break;
    if (isdigit(enc.save_buf.s[0]) && isalpha(enc.save_buf.s[1])) {
      if (!enc.decode(in,scp,!add_tunes)) { result=false; break; }
    }
    else {
      alert("bad .sco code: %s",enc.save_buf.s);
      result=false;
      break;
    }
  }
  if (app_local) // 0 at startup, meterView->draw() in constructor
    app_local->meterView->draw(act_meter);
  fclose(in);
  return result;
}

void get_token(Str& str,const char *text,const char* delim,int& pos) {
  //if (debug) printf("get_token: str=[%s] delim=[%s] pos=%d\n",str.s,delim,pos);
  str.strtok(text,delim,pos);
}

bool read_times(const char *text,Str& str,int& pos,Array<int,times_max>& tim,const int stop) {   // e.g. time:2.3,4
  int nr;
  for (int n=0;;++n) {
    get_token(str,text," .\n;,",pos);
    nr=atoi(str.s) * app->act_meter;
    if (str.ch=='.') {
      get_token(str,text," \n;,",pos);
      nr+=atoi(str.s);
    }
    if (nr<0) {
      alert("negative time"); return false;
    }
    tim[n]=nr;
    if (str.ch!=',') { tim[n+1]=stop; return true; }
  }
}

int read_time(Str& str,const char *text,int& pos,int meter) {   // e.g. time:2.3
  get_token(str,text," .\n;",pos);
  bool neg= str.s[0]=='-';
  int nr=atoi(str.s) * meter;
  if (str.ch=='.') {
    get_token(str,text," \n;",pos);
    if (neg) nr-=atoi(str.s); else nr+=atoi(str.s);
  }
  return nr;
}

void set_wave(Str &str,const char *text,int &pos,const char* col,
              int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," ,\n;",pos);
  if (str.ch!=',') { alert("bad %s syntax",col); return; }
  n=atoi(str.s);
  if (n<1 || n>5) { alert("bad %s value: %d",col,n); n=2; }
  info.idata.d0=n;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n<2 || n>5) { alert("bad %s value: %d",col,n); n=3; }
  info.idata.d1=n;
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_mode(Str &str,const char *text,int &pos,const char* colname,
              int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.tag=eMode;
  info.idata.col=col;
  get_token(str,text," \n;",pos);
  info.idata.d1=str.s[0]; // 'n': native, 'm': mono-synth
  if (!strchr("inm",info.idata.d1)) { alert("bad %s: parameter (must be 'nat' or 'msynth')",colname); return; }
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_ph_model(Str &str,const char *text,int &pos,const char* colname,
                    int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.tag=ePhysM;
  info.idata.col=col;
  get_token(str,text," ,\n;",pos);
  if (str.ch!=',') { alert("bad %s syntax, missing ','",colname); return; }
  n=atoi(str.s);
  if (n<1 || n>5) { alert("bad %s value: %d",colname,n); n=2; }
  info.idata.d0=n;
  get_token(str,text," ,\n;",pos);
  n=atoi(str.s);
  if (n<1 || n>5) { alert("bad %s value: %d",colname,n); n=3; }
  info.idata.d1=n;

  get_token(str,text," ,\n;",pos);
  n=atoi(str.s);
  if (n<1 || n>5) { alert("bad %s value: %d",colname,n); n=2; }
  info.idata.d2=n;

  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (!isdigit(str.s[0])) { alert("missing digit for %s",colname); n=0; }
  else if (n>1) { alert("bad %s value: %d (expected: 0 or 1)",colname,n); n=0; }
  info.idata.d3=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_mono_synth(Str &str,const char *text,int &pos,const char* colname,
                    int col,Array<int,times_max>times,MusicView *musV) {
  ScInfo info;
  info.tag=eSynth;
  get_token(str,text," \n;",pos);
  if (info.ms_data.fill_msynth_data(col,str.s)) 
    for (int n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_wavefile(Str &str,const char *text,int &pos,const char* colname,
                  int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  info.tag=eWaveF;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (!isdigit(str.s[0])) { alert("missing digit for %s",colname); n=0; }
  info.idata.d1=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_info(int low,int hi,Str &str,const char *text,int &pos,const char* colname,
                int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n<low || n>hi) { alert("bad %s value: %d",colname,n); n=0; }
  info.tag=en;
  info.n=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_reverb(Str &str,const char *text,int &pos,const char* colname,
              int col,Array<int,times_max>times,MusicView *musV) {
  uint n;
  ScInfo info;
  info.tag=eRevb;
  info.idata.col=col;
  get_token(str,text," ,\n;",pos);
  info.idata.d1=min(4,atoi(str.s));
  if (str.ch==',') {
    get_token(str,text," \n;",pos);
    info.idata.d2=min(1,atoi(str.s));
  }
  else info.idata.d2=0;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_info2(int low,int hi,Str &str,const char *text,int &pos,const char* colname,
               int en,int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n<low || n>hi) { alert("bad %s value: %d",colname,n); n=0; }
  info.tag=en;
  info.idata.col=col;
  info.idata.d1=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_ampl(Str &str,const char *text,int &pos,const char* colname,
              int col,Array<int,times_max>times,MusicView *musV) {
  uint n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n>ampl_max) { alert("bad %s value: %d",colname,n); n=0; }
  info.tag=eAmpl;
  info.idata.col=col;
  info.idata.d1=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_ampl_gr(Str &str,const char *text,int &pos,const char* colname,
                 int col,int gr_nr,Array<int,times_max>times,MusicView *musV) {
  uint n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n>ampl_max) { alert("bad %s value: %d",colname,n); n=0; }
  info.tag=eAmpl_gr;
  info.idata.col=col;
  info.idata.d1=n;
  info.idata.d2=gr_nr;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_fm(Str &str,const char *text,int &pos,const char* col,int en,
            Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," ,\n;",pos);
  if (str.ch!=',') { alert("%s: no comma",col); return; }
  n=atoi(str.s);
  if (n<-1 || n>7) { alert("bad %s value: %d",col,n); n=0; }
  info.idata.d0=n;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n<0 || n>7) { alert("bad %s value: %d",col,n); n=0; }
  info.idata.d1=n;
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_mmod(Str &str,const char *text,int &pos,const char* col,int en,
              Array<int,times_max>times,MusicView *musV) {
  uint n;
  ScInfo info;
  get_token(str,text," ,\n;",pos);
  if (str.ch!=',') { alert("%s: no comma",col); return; }
  n=atoi(str.s);
  if (n>5) { alert("bad %s value: %d",col,n); n=0; }
  info.idata.d0=n;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n>3) { alert("bad %s value: %d",col,n); n=0; }
  info.idata.d1=n;
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_bool(Str &str,const char *text,int &pos,const char* col,
              int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  if (str=="on") info.b=true;
  else if (str=="off") info.b=false;
  else { alert("bad %s value: %s",col,str.s); info.b=false; }
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_bool2(Str &str,const char *text,int &pos,const char* colname,
              int en,int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  if (str=="on") info.idata.b=true;
  else if (str=="off") info.idata.b=false;
  else { alert("bad %s value: %s",col,str.s); info.b=false; }
  info.tag=en;
  info.idata.col=col;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_loc(Str &str,const char *text,int &pos,const char* colname,
             int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  info.tag=eLoc;
  get_token(str,text," \n;",pos);
  if (str=="left") info.idata.d1=eLeft;
  else if (str=="right") info.idata.d1=eRight;
  else if (str=="mid") info.idata.d1=eMid;
  else { alert("loc: bad value: %s",str.s); info.idata.d1=eMid; }
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_pan(Str &str,const char *text,int &pos,const char* colname,
             int col,Array<int,times_max>times,MusicView *musV) {
  int n, val=eMid;
  ScInfo info;
  info.idata.col=col;
  info.tag=eLoc;
  get_token(str,text," \n;",pos);
  if (str=="LL") val=eLLeft;
  else if (str=="L") val=eLeft;
  else if (str=="M") val=eMid;
  else if (str=="R") val=eRight;
  else if (str=="RR") val=eRRight;
  else { alert("pan: bad value: %s",str.s); }
  info.idata.d1=val;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_sampled_loc(Str &str,const char *text,int &pos,const char* colname,
                     int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  info.tag=eSLoc;
  get_token(str,text," \n;",pos);
  if (str=="left") info.idata.d1=eLeft;
  else if (str=="right") info.idata.d1=eRight;
  else if (str=="mid") info.idata.d1=eMid;
  else { alert("sampled-loc: bad value: %s",str.s); info.n=0; }
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_sampled_pan(Str &str,const char *text,int &pos,const char* colname,
                     int col,Array<int,times_max>times,MusicView *musV) {
  int n, val=eMid;
  ScInfo info;
  info.idata.col=col;
  info.tag=eSLoc;
  get_token(str,text," \n;",pos);
  if      (str=="LL") val=eLLeft;
  else if (str=="L") val=eLeft;
  else if (str=="M") val=eMid;
  else if (str=="R") val=eRight;
  else if (str=="RR") val=eRRight;
  else { alert("sampled-pan: bad value: %s",str.s); }
  info.idata.d1=val;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_is_a(Str &str,const char *text,int &pos,const char* colname,
             int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  get_token(str,text," \n;",pos);
  info.idata.d1=color_nr(str.s);
  info.tag=eIsa;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_ms_is_a(Str &str,const char *text,int &pos,const char* colname,
             int col,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  get_token(str,text," \n;",pos);
  info.idata.d1=color_nr(str.s);
  info.tag=eMsIsa;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_isa_gr(Str &str,const char *text,int &pos,const char* colname,
                int col,int gr_nr,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  info.idata.col=col;
  get_token(str,text," \n;",pos);
  info.idata.d1=color_nr(str.s);
  info.idata.d2=gr_nr;
  info.tag=eIsa_gr;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

bool key_name_to_keynr(char *kn,int& key_nr) {
  int n;
  for (n=0;;++n) {
    if (!strcmp(kn,maj_min_keys[n])) break;
    if (n==keys_max*4-1) { alert("unknown key %s",kn); key_nr=0; return false; }
  }
  key_nr=n%(2*keys_max);  // n > 2*keys_max: classic key name
  return true;
}

bool App::run_script(const char *text) {
  Score *from=0,
        *to=0;
  Str str;
  bool ok;
  int n,n1,
      cmd,
      pos,thePos,
      line_nr,
      lst_ch,
      col_nr=eBlack;
  char *s;
  ScInfo info;
  Array<int,times_max> times;
  ScoreViewBase *display=0;
  ScoreView *sv;
  MusicView* musV=app_local->musicView;

  app_local->editScript->meas_info->reset();
  for (n=0;n<colors_max;++n)
    col2ctrl(n)->set_isa(n);
  ps_out.init_ps();
  extended=false;
  str.cmt_ch='#';

  for (pos=thePos=0,line_nr=1;;) {
    lst_ch=str.ch;
    get_token(str,text," :\n;",pos);
    if (!str.s[0] && str.ch==0) break;
    if (!str.s[0]) { ++line_nr; continue; }
    if (debug) printf("cmd:%s\n",str.s);
    if ((cmd=eAdd,str=="add") ||          // from scores to music
        (cmd=eTake,str=="take") ||        // from scores to scoreBuf
        (cmd=eTake_nc,str=="take-nc")) {  // from scores to scoreBuf, no clear
      switch (cmd) {
        case eAdd:
          thePos=pos;
          from=scoreBuf; to=musV->score; display=musV;
          break;
        case eTake:
          from=0; to=scoreBuf; display=0;
          to->reset(false);
          break;
        case eTake_nc:
          from=0; to=scoreBuf; display=0;
          break;
      }
      times[0]=to->lst_sect+1;
      times[1]=eo_arr;
      int start=0,
          stop=nop,
          shift=0,
          raise=0,
          atcolor=-1;
      for (;;) {
        if (strchr("\n;",str.ch)) {
          if (!from) { alert("error in script"); return false; }
          int act_stop = stop>0 ? stop : from->end_sect ? from->end_sect : from->len;
          switch (cmd) {
            case eAdd: break;
            case eTake: // to = scoreBuf
              to->name=from->name; // needed for command "add"
              to->signs_mode=from->signs_mode;
              to->sc_meter=from->sc_meter;
              for (n=0;n<sclin_max;++n)
                to->lin[n].n_sign=from->lin[n].n_sign;
              break;
            case eTake_nc: break;
          }
          for (n=0;times[n]!=eo_arr;++n)
            to->add_copy(from,start,act_stop,times[n],shift,raise,display,atcolor);
          if (cmd==eAdd && lst_ch=='\n')
            app_local->editScript->print_meas(line_nr-1,times[0]);
          break;
        }
        get_token(str,text," :\n;",pos);
        if (debug) printf("add/take cmd: %s ch=[%c]\n",str.s,str.ch);
        if (str.ch==':') {
          if (str=="time") {
            if (!read_times(text,str,pos,times,eo_arr)) return false;
          }
          else if (str=="rt") {
            times[0]=to->lst_sect+1+read_time(str,text,pos,act_meter);
            if (times[0]<0) { alert("negative time at rt: command"); return false; }
            times[1]=eo_arr;
          }
          else if (str=="from") {
            int m=from->sc_meter;
            if (!m) m=act_meter;
            start=read_time(str,text,pos,m);
            if (start<0) { alert("negative time at from: command"); return false; }
          }
          else if (str=="to") {
            int m=from->sc_meter;
            if (!m) m=act_meter;
            stop=read_time(str,text,pos,m);
            if (stop<0) { alert("negative time at stop: command"); return false; }
          }
          else if (str=="shift") { get_token(str,text," \n;",pos); shift=atoi(str.s); }
          else if (str=="raise") { get_token(str,text," \n;",pos); raise=atoi(str.s); }
          else if (str=="color") { 
            get_token(str,text," \n;",pos);
            atcolor=color_nr(str.s);
          }
          else { alert("unk option '%s'",str.s); return false; }
        }
        else {
          from=scores.exist_name(str.s);
          if (!from) {
            if (str==title())
              from=the_mv()->score;
            else {
              alert("tune '%s' unknown, line %d",str.s,line_nr); return false;
            }
          }
        }
      }
    }
    else if (str=="put") {      // from scoreBuf to scores
      if (str.ch!=' ') { alert("tune name missing after put cmd"); return false; }
      get_token(str,text," \n;",pos);
      to=scores.exist_name(str.s);
      if (to) {
        to->reset(false);
        to->copy(scoreBuf);
        if (find_score(to,sv)) { sv->set_scroll_range(); sv->draw_sc2(true); }
      }
      else {
        to=new_score(str.s);
        to->copy(scoreBuf);
      }
    }
    else if (str=="out-par") {
      int group;
      times[0]=0;
      times[1]=eo_arr;
      for(;;) {
        get_token(str,text," :\n;",pos);
        if (debug) printf("out-par cmd:%s\n",str.s);
        if (str.ch==':') {
          group=0;
          if (str=="time") {
            if (!read_times(text,str,pos,times,eo_arr)) return false;
          }
          else if (!strncmp(str.s,"transp-",7)) { // e.g. "transp-0"
            group=atoi(str.s+7);
            if (group<0 || group>=groupnr_max) { alert("transp: group = %d",group); return false; }
            for (n=0;n<colors_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=colors_max-1) { alert("transp: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1<-24 || n1>24) { alert("bad transpose value: %d",n1); n1=0; }
              ps_out.transp[group][n]=n1;
            }
          }
          else if (str=="annotate") {
            for (n=0;;++n) {
              if (n>=annot_max) {
                alert("out-par: annotation > 'Z', line %d",line_nr); break;
              }
              get_token(str,text," \n;,",pos);
              ps_out.annot[n].mnr=atoi(str.s);
              if (str.ch!=',') break;
            }
          }
          else if (str=="key") {
            get_token(str,text," \n;",pos);
            if (!key_name_to_keynr(str.s,app->mv_key_nr)) return false;
          }
          else if (str=="nupq") {  // note units per quarter note
            get_token(str,text," \n;",pos);
            app->nupq=atoi(str.s);
          }
          else if (!strncmp(str.s,"midi-instr-",11)) { // e.g. "midi-instr-0"
            group=atoi(str.s+11);
            if (group<0 || group>=groupnr_max) { alert("midi-instr: group = %d",group); return false; }
            info.tag=eMidiInstr;
            for (n=0;n<colors_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=colors_max-1) { alert("midi-instr: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1>128) {
                alert("bad midi-instr value: %d",n1); n1=0;
              }
              info.midi_data.n6[n]=n1;
              info.midi_data.gnr=group;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (!strncmp(str.s,"midi-perc-",10)) {
            info.tag=eMidiPerc;
            group=atoi(str.s+10);
            if (group<0 || group>=groupnr_max) { alert("midi-perc: group = %d",group); return false; }
            for (n=0;n<colors_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=colors_max-1) { alert("midi-perc: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1 && (n1<35 || n1>81)) {  // not mapped: n1=0
                alert("bad midi-perc value: %d",n1); n1=35;
              }
              info.midi_data.n6[n]=n1;
              info.midi_data.gnr=group;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (str=="header" || str=="abc-header") {
            get_token(str,text," \n;",pos);
            ps_out.header_file=strdup(str.s);
          }
          else {
            alert("unk option '%s:' with out-par cmd, line %d",str.s,line_nr);
            return false;
          }
        }
        else if (str=="single-voice") {
          ps_out.s_voice=true;
        }
        else {
          alert("unk option '%s' at out-par cmd, line %d",str.s,line_nr);
          return false;
        }
        if (strchr("\n;",str.ch)) break;
      }
    }
    else if (str=="sample-dir" && str.ch==':') {  // samples directories
      wave_buf.sample_dirs->reset();
      for (;;) {
        XftColor *dircol;  // fontcolor in wavefile menu
        const char *cs;
        get_token(str,text," ,\n;",pos);
        if (str==cur_dir) { cs=cur_dir; dircol=xft_Black; }
        else if (str==wave_samples) { cs=wave_samples; dircol=xft_Blue; }
        else {
          cs=strdup(str.s);
          static XftColor *dc=xft_calc_color(0,0x70,0); dircol=dc;
        }
        wave_buf.sample_dirs->add_dir(cs,dircol);
        if (str.ch!=',') break;
      }
    }
    else if (str=="set") {
      times[0]=musV->score->lst_sect+1; // default time
      times[1]=eo_arr;
      for (;;) {
        get_token(str,text," :\n;",pos);
        if (debug) printf("set cmd:%s\n",str.s);
        if (str.ch==':') {
          if (str=="time") {
            if (!read_times(text,str,pos,times,eo_arr)) return false;
          }
          else if (str=="rt") {
            times[0]=musV->score->lst_sect+1+read_time(str,text,pos,act_meter);
            if (times[0]<0) { alert("negative time at rt: command"); return false; }
            times[1]=eo_arr;
          }
          else if (str=="tempo") {
            get_token(str,text," \n;",pos);
            info.tag=eTempo;
            info.n=atoi(str.s)/10;
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,col_nr,"red","start-wave","stw",s))
            set_wave(str,text,pos,s,eRed_att_timbre,times,musV);
          else if (eq(str.s,col_nr,"red","sustain-wave","suw",s))
            set_wave(str,text,pos,s,eRed_timbre,times,musV);
          else if (eq(str.s,col_nr,"red","decay","dc",s))
            set_info(0,5,str,text,pos,s,eRed_decay,times,musV);
          else if (eq(str.s,col_nr,"red","startup","su",s))
            set_info(0,5,str,text,pos,s,eRed_attack,times,musV);
          else if (eq(str.s,col_nr,"red","start-amp","sa",s))
            set_info(0,3,str,text,pos,s,eRed_stamp,times,musV);
          else if (eq(str.s,col_nr,"green","freq-ratio","fr",s))
            set_info(0,3,str,text,pos,s,eGreen_fmul,times,musV);
          else if (eq(str.s,col_nr,"green","chorus","ch",s))
            set_info(0,3,str,text,pos,s,eGreen_chorus,times,musV);
          else if (eq(str.s,col_nr,"green","attack","at",s))
            set_info(0,5,str,text,pos,s,eGreen_attack,times,musV);
          else if (eq(str.s,col_nr,"green","decay","dc",s))
            set_info(0,5,str,text,pos,s,eGreen_decay,times,musV);
          else if (eq(str.s,col_nr,"green","tone",0,s)) {
            get_token(str,text," \n;",pos);
            alert("skipping green 'tone' command");
          }
          else if (eq(str.s,col_nr,"green","wave1","w1",s))
            set_wave(str,text,pos,s,eGreen_timbre1,times,musV);
          else if (eq(str.s,col_nr,"green","wave2","w2",s))
            set_wave(str,text,pos,s,eGreen_timbre2,times,musV);
          else if (eq(str.s,col_nr,"purple","decay","dc",s))
            set_info(0,5,str,text,pos,s,ePurple_decay,times,musV);
          else if (eq(str.s,col_nr,"purple","sound","snd",s))
            set_info(0,4,str,text,pos,s,ePurple_tone,times,musV);
          else if (eq(str.s,col_nr,"purple","tone",0,s)) {
            alert("'purple tone:' is deprecated, use 'sound:'");
            get_token(str,text," \n;",pos);
          }
          else if (eq(str.s,col_nr,"blue","attack","at",s))
            set_info(0,5,str,text,pos,s,eBlue_attack,times,musV);
          else if (eq(str.s,col_nr,"blue","decay","dc",s))
            set_info(0,5,str,text,pos,s,eBlue_decay,times,musV);
          else if (eq(str.s,col_nr,"blue","dur-limit","dl",s))
            set_info(0,4,str,text,pos,s,eBlue_durlim,times,musV);
          else if (eq(str.s,col_nr,"blue","lowpass","lp",s))
            set_info(0,4,str,text,pos,s,eBlue_lowp,times,musV);
          else if (eq(str.s,col_nr,"red","dur-limit","dl",s))
            set_info(0,4,str,text,pos,s,eRed_durlim,times,musV);
          else if (eq(str.s,col_nr,"red","tone",0,s))
            set_info(0,1,str,text,pos,s,eRed_tone,times,musV);
          else if (eq(str.s,col_nr,"blue","rich","rich",s))
            set_bool(str,text,pos,s,eBlue_rich,times,musV);
          else if (eq(str.s,col_nr,"blue","chorus","ch",s))
            set_bool(str,text,pos,s,eBlue_chorus,times,musV);
          else if ((eq(str.s,col_nr,"black","fm",0,s)))
            set_fm(str,text,pos,s,eBlack_fm,times,musV);
          else if (eq(str.s,col_nr,"black","decay","dc",s))
            set_info(0,5,str,text,pos,s,eBlack_decay,times,musV);
          else if (eq(str.s,col_nr,"black","attack","at",s))
            set_info(0,5,str,text,pos,s,eBlack_attack,times,musV);
          else if (eq(str.s,col_nr,"black","subband","sb",s) ||
                   eq(str.s,col_nr,"brown","subband","sb",s)) {
            alert("'black/brown subband: (sb:)' deprecated, use e.g. fm:-1,..");
            get_token(str,text," \n;",pos);
          }
          else if (eq(str.s,col_nr,"black","detune","dt",s))
            set_info(0,5,str,text,pos,s,eBlack_detune,times,musV);
          else if ((eq(str.s,col_nr,"black","modmod","mm",s)))
            set_mmod(str,text,pos,s,eBlack_mmod,times,musV);
          else if (eq(str.s,col_nr,"brown","fm",0,s))
            set_fm(str,text,pos,s,eBrown_fm,times,musV);
          else if (eq(str.s,col_nr,"brown","decay","dc",s))
            set_info(0,5,str,text,pos,s,eBrown_decay,times,musV);
          else if (eq(str.s,col_nr,"brown","detune","dt",s))
            set_info(0,5,str,text,pos,s,eBrown_detune,times,musV);
          else if (eq(str.s,col_nr,"brown","attack","at",s))
            set_info(0,5,str,text,pos,s,eBrown_attack,times,musV);
          else if ((eq(str.s,col_nr,"brown","modmod","mm",s)))
            set_mmod(str,text,pos,s,eBrown_mmod,times,musV);
          else if (eq(str.s,col_nr,"purple","start-harm","sth",s)) {
            info.tag=ePurple_a_harm;
            for (n=0;n<harm_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=harm_max-1) { alert("%s: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1<0 || n1>3) { alert("bad %s value: %d",s,n1); n1=0; }
              info.n5[n]=n1;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,col_nr,"purple","sustain-harm","suh",s)) {
            info.tag=ePurple_s_harm;
            for (n=0;n<harm_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=4) { alert("%s: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1<0 || n1>3) { alert("bad %s value: %d",s,n1); n1=0; }
              info.n5[n]=n1;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,col_nr,"purple","startup","su",s)) 
            set_info(0,5,str,text,pos,s,ePurple_attack,times,musV);
          else if (eq(str.s,col_nr,"purple","chorus","ch",s)) {
            alert("'purple chorus: (ch:)' is deprecated, use 'sound:1'");
            get_token(str,text," \n;",pos);
          }
          else if (eq(str.s,col_nr,"black","sampled",0,s) ||
                   eq(str.s,col_nr,"red","sampled",0,s) ||
                   eq(str.s,col_nr,"green","sampled",0,s) ||
                   eq(str.s,col_nr,"blue","sampled",0,s) ||
                   eq(str.s,col_nr,"brown","sampled",0,s) ||
                   eq(str.s,col_nr,"purple","sampled",0,s)) {
            get_token(str,text," \n;",pos);
            alert("'sampled:' is deprecated, use 'phm:' and 'wa:'");
          }
          else if (eq(str.s,col_nr,"black","msynth",0,s) ||
                   eq(str.s,col_nr,"brown","msynth",0,s) ||
                   eq(str.s,col_nr,"blue","msynth",0,s) ||
                   eq(str.s,col_nr,"green","msynth",0,s) ||
                   eq(str.s,col_nr,"red","msynth",0,s) ||
                   eq(str.s,col_nr,"purple","msynth",0,s))
            set_mono_synth(str,text,pos,s,col_nr,times,musV);
          else if (eq(str.s,col_nr,"black","mode","m",s) ||
                   eq(str.s,col_nr,"blue","mode","m",s) ||
                   eq(str.s,col_nr,"red","mode","m",s) ||
                   eq(str.s,col_nr,"green","mode","m",s) ||
                   eq(str.s,col_nr,"purple","mode","m",s) ||
                   eq(str.s,col_nr,"brown","mode","m",s))
            set_mode(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","wavefile","wf",s) ||
                   eq(str.s,col_nr,"red","wavefile","wf",s) ||
                   eq(str.s,col_nr,"green","wavefile","wf",s) ||
                   eq(str.s,col_nr,"blue","wavefile","wf",s) ||
                   eq(str.s,col_nr,"brown","wavefile","wf",s) ||
                   eq(str.s,col_nr,"purple","wavefile","wf",s))
            set_wavefile(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","phys-mod","phm",s) ||
                   eq(str.s,col_nr,"red","phy-mod","phm",s) ||
                   eq(str.s,col_nr,"green","phys-mod","phm",s) ||
                   eq(str.s,col_nr,"blue","phys-mod","phm",s) ||
                   eq(str.s,col_nr,"brown","phys-mod","phm",s) ||
                   eq(str.s,col_nr,"purple","phys-mod","phm",s))
            set_ph_model(str,text,pos,s,col_nr,times,musV);
          else if (eq(str.s,col_nr,"black","reverb","rev",s) ||
                   eq(str.s,col_nr,"red","reverb","rev",s) ||
                   eq(str.s,col_nr,"blue","reverb","rev",s) ||
                   eq(str.s,col_nr,"green","reverb","rev",s) ||
                   eq(str.s,col_nr,"purple","reverb","rev",s) ||
                   eq(str.s,col_nr,"brown","reverb","rev",s))
            set_reverb(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","wave-ampl","wa",s) ||
                   eq(str.s,col_nr,"red","wave-ampl","wa",s) ||
                   eq(str.s,col_nr,"green","wave-ampl","wa",s) ||
                   eq(str.s,col_nr,"blue","wave-ampl","wa",s) ||
                   eq(str.s,col_nr,"brown","wave-ampl","wa",s) ||
                   eq(str.s,col_nr,"purple","wave-ampl","wa",s))
            set_info2(0,7,str,text,pos,s,eWaveAmpl,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","ctr-pitch","cpit",s) ||
                   eq(str.s,col_nr,"red","ctr-pitch","cpit",s) ||
                   eq(str.s,col_nr,"green","ctr-pitch","cpit",s) ||
                   eq(str.s,col_nr,"blue","ctr-pitch","cpit",s) ||
                   eq(str.s,col_nr,"brown","ctr-pitch","cpit",s) ||
                   eq(str.s,col_nr,"purple","ctr-pitch","cpit",s))
            set_bool2(str,text,pos,s,eCtrPitch,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","base_freq","bf",s) ||
                   eq(str.s,col_nr,"brown","base_freq","bf",s))
            set_bool2(str,text,pos,s,eBaseFreq,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","a-0","a-1","a-2",n,s) ||
                   eq(str.s,col_nr,"red","a-0","a-1","a-2",n,s) ||
                   eq(str.s,col_nr,"green","a-0","a-1","a-2",n,s) ||
                   eq(str.s,col_nr,"blue","a-0","a-1","a-2",n,s) ||
                   eq(str.s,col_nr,"purple","a-0","a-1","a-2",n,s) ||
                   eq(str.s,col_nr,"brown","a-0","a-1","a-2",n,s))
            if (extended)
              set_ampl_gr(str,text,pos,s,col_nr,n,times,musV);
            else { alert("%s: only if extended-syntax",str.s); return false; }

          else if (eq(str.s,col_nr,"black","ampl","a",s) ||
                   eq(str.s,col_nr,"brown","ampl","a",s) ||
                   eq(str.s,col_nr,"red","ampl","a",s) ||
                   eq(str.s,col_nr,"green","ampl","a",s) ||
                   eq(str.s,col_nr,"blue","ampl","a",s) ||
                   eq(str.s,col_nr,"purple","ampl","a",s))
            set_ampl(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","loc",0,s) ||
                   eq(str.s,col_nr,"red","loc",0,s) ||
                   eq(str.s,col_nr,"green","loc",0,s) ||
                   eq(str.s,col_nr,"blue","loc",0,s) ||
                   eq(str.s,col_nr,"brown","loc",0,s) ||
                   eq(str.s,col_nr,"purple","loc",0,s))
            set_loc(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","pan",0,s) ||
                   eq(str.s,col_nr,"red","pan",0,s) ||
                   eq(str.s,col_nr,"green","pan",0,s) ||
                   eq(str.s,col_nr,"blue","pan",0,s) ||
                   eq(str.s,col_nr,"brown","pan",0,s) ||
                   eq(str.s,col_nr,"purple","pan",0,s))
            set_pan(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","sampled-loc","sloc",s) ||
                   eq(str.s,col_nr,"red","sampled-loc","sloc",s) ||
                   eq(str.s,col_nr,"green","sampled-loc","sloc",s) ||
                   eq(str.s,col_nr,"blue","sampled-loc","sloc",s) ||
                   eq(str.s,col_nr,"brown","sampled-loc","sloc",s) ||
                   eq(str.s,col_nr,"purple","sampled-loc","sloc",s))
            set_sampled_loc(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","sampled-loc","span",s) ||
                   eq(str.s,col_nr,"red","sampled-loc","span",s) ||
                   eq(str.s,col_nr,"green","sampled-loc","span",s) ||
                   eq(str.s,col_nr,"blue","sampled-loc","span",s) ||
                   eq(str.s,col_nr,"brown","sampled-loc","span",s) ||
                   eq(str.s,col_nr,"purple","sampled-loc","span",s))
            set_sampled_pan(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","eq",0,s) ||
                   eq(str.s,col_nr,"red","eq",0,s) ||
                   eq(str.s,col_nr,"green","eq",0,s) ||
                   eq(str.s,col_nr,"blue","eq",0,s) ||
                   eq(str.s,col_nr,"brown","eq",0,s) ||
                   eq(str.s,col_nr,"purple","eq",0,s))
            set_is_a(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","ms-eq",0,s) ||
                   eq(str.s,col_nr,"red","ms-eq",0,s) ||
                   eq(str.s,col_nr,"green","ms-eq",0,s) ||
                   eq(str.s,col_nr,"blue","ms-eq",0,s) ||
                   eq(str.s,col_nr,"brown","ms-eq",0,s) ||
                   eq(str.s,col_nr,"purple","ms-eq",0,s))
            set_ms_is_a(str,text,pos,s,col_nr,times,musV);

          else if (eq(str.s,col_nr,"black","eq-0","eq-1","eq-2",n,s) ||
                   eq(str.s,col_nr,"red","eq-0","eq-1","eq-2",n,s) ||
                   eq(str.s,col_nr,"blue","eq-0","eq-1","eq-2",n,s) ||
                   eq(str.s,col_nr,"green","eq-0","eq-1","eq-2",n,s) ||
                   eq(str.s,col_nr,"purple","eq-0","eq-1","eq-2",n,s) ||
                   eq(str.s,col_nr,"brown","eq-0","eq-1","eq-2",n,s)) {
            if (extended)
              set_isa_gr(str,text,pos,s,col_nr,n,times,musV);
            else { alert("%s: only if extended-syntax",str.s); return false; }
          }
          else {
            alert("bad option '%s' at set cmd, line %d",str.s,line_nr);
            return false;
          }
          if (strchr("\n;",str.ch)) break;
        }
        else if (col_nr=color_nr(str.s,&ok),ok); // here col_nr is set
        else {
          alert("unk option '%s' at set cmd, line %d",str.s,line_nr);
          return false;
        }
      }
    }
    else if (str=="mute") {
      alert("'mute' not supported, use 'ampl:0'");
      return false;
    }
    else if (str=="exit") break;
    else if (str=="extended-syntax") {
      if (!extended) {
        extended=true;
        for (int col=0;col<colors_max;++col) {
          CtrlBase *ctr=col2ctrl(col);
          ctr->group=new RButWin(ctr->cview,ctr->gr_rect,FN,"gr#",false,rbutwin_cmd,cForeground,Id('fmgr',col));
          for (int i=0;i<3;++i) ctr->group->add_rbut(i==0?"0":i==1?"1":"2");
        }
      }
    }
    else { alert("unknown cmd '%s' in script, line %d",str.s,line_nr); return false; }
    if (str.ch==0) break;
    if (str.ch=='\n') ++line_nr;
  }
  return true;
}

char* App::title() {
  static Str tit;
  tit.cpy(input_file.strip_dir());
  char *p=strrchr(tit.s,'.');
  if (p) p[0]=0;
  return tit.s;
}

void ChordsWindow::force_key(Id) {
  if (app->act_score>nop) {
    ScoreView *sv=the_sv(app->act_score);
    if (sv->draw_mode==ePiano) return;
    if (sv->score) {
      for (int n1=0;n1<sclin_max;++n1) {
        ScLine *lin=&sv->score->lin[n1];
        for (int n2=0;n2<sv->score->len;++n2) {
          ScSection *sec=lin->sect+n2;
          if (sec->cat==ePlay)
            for (;sec;sec=sec->nxt())
              sec->sign=lin->n_sign;
        }
      }
      sv->draw_sc2(false);
    }
    app->check_reset_as();
  }
}

void ChordsWindow::scr_cmd(Id,int val,int,bool) {
  static ChordsWindow *cw=app->chordsWin;
  cw->scales->set_y_off(val);
}

void ChordsWindow::set_key(Id) {
  if (app->act_score>nop) {
    ScoreView *sv=the_sv(app->act_score);
    ChordsWindow *cw=app->chordsWin;
    if (sv->score) {
      sv->score->tone2signs(cw->the_key_nr);
      switch (sv->draw_mode) {
        case eAccValue:
          sv->draw_sc2(true);
          break;
        case ePiano:
          sv->draw_sc2(true);
          break;
        default:
          sv->drawSigns();
      }
    }
    app->check_reset_as();
  }
}

void ScoreView::draw_chordn(bool store_name) {
  if (store_name)
    app->chordsWin->chord_or_scale_name(the_chord_name,score->sc_key_nr);
  int loc=max(2,(chord_name->dx - xft_text_width(the_chord_name))/2);
  chord_name->clear();
  xft_draw_string(chord_name->xft_win,xft_Black,Point(loc,10),the_chord_name);
}

void ChordsWindow::upd_chordname(ScoreView *sv) {
  sv->draw_chordn(true);
  sv->display_mode->re_label(sv->display_mode->nr2rb(3),"chord, scale");
}

void ChordsWindow::upd_scoreview(int scales_nr) {
  if (app->act_score>nop && scales->act_button) {
    ScoreView *sv=the_sv(app->act_score);
    sv->p_set_scline_col(scale_data[scales_nr].valid,scale_data[scales_nr].gt12,shift_12);
    sv->scale_dep= dm==eScDep_no ? eScDep_no : dep_mode->value ? eScDep_lk : eScDep_sk;
    upd_chordname(sv);
    if (sv->draw_mode==ePiano)
      sv->draw_sc2(true);
  }
}

void CtrlBase::draw(Id id) {
  col2ctrl(id.id1)->draw_isa_col();
}

void CtrlBase::draw_isa_col() {
  int gr=group ? group->act_rbutnr() : 0;
  uint line_col= isa[gr]==this ? cForeground : col2color(isa[gr]->ctrl_col);
  set_width_color(2,line_col);
  cview->draw_line(Point(1,0),Point(1,ictrl_height));
}

CtrlBase::CtrlBase(Rect rect,int col):
    cview(new BgrWin(top_win->win,rect,MR,draw,cForeground,1,Id(col,0))),
    bgwin(cview->win),
    ctrl_col(col),
    instrm_val(eNativeMode),
    sample_mode(ePhysmodMode),
    gr_rect(0,0,0,0),
    inst(col<0 ? 0 : col2instr(col)),
    synth_top(700,20),
    synth(0),
    group(0) {
  for (int gr=0;gr<3;++gr) isa[gr]=this;
}

void CtrlBase::init_ampl() {
  ampl->d=ampl_val;
  for (int i=0;i<3;++i) ampl_val[i]=5;
  set_text(ampl->text,"%.2f",ampl_mult[5]);
}

FMCtrl::FMCtrl(Rect rect,int col):  // col: eBlack or eBrown
    CtrlBase(rect,col) {
  fm_ctrl=new HVSlider(cview,Rect(4,TDIST,82,68),18,FN,Int2(-1,0),Int2(7,7),
                       "FM freq/index","-1","7","0","7",hvslider_cmd,cForeground,Id('fm  ',col));
  fm_ctrl->value.set(2,3);
  set_text(fm_ctrl->text_x,"3.0");
  set_text(fm_ctrl->text_y,"2.0");

  mode=new RButWin(cview,Rect(4,96,44,2*TDIST),FN,"mode",false,rbutton_cmd,cForeground,Id('moms',col));
  mode->add_rbut("native"); mode->add_rbut("msynth");

  button_style.param=1;
  eq=new Button(bgwin,Rect(54,97,28,0),FN,"eq?",button_cmd,Id('eq',col));
  button_style=def_but_st;

  detune=new HSlider(cview,Rect(92,TDIST,60,0),FN,0,5,"detune","0","5",slider_cmd,cForeground,Id('fmdt',col));
  detune->value()=0;

  mod_mod=new HVSlider(cview,Rect(86,40+TDIST,70,50),20,FN,Int2(0,0),Int2(5,3),
                       "MM depth/freq","0","5","0","3",hvslider_cmd,cForeground,Id('momo',col));
  mod_mod->value.set(0,0);
  set_text(mod_mod->text_x,"0");
  set_text(mod_mod->text_y,"0.5");

  base_freq=new CheckBox(bgwin,Rect(86,108,0,0),FN,cForeground,"base freq",0);

  gr_rect.set(156,82,18,3*TDIST);

  attack=new HSlider(cview,Rect(184,TDIST,56,0),FN,0,5,"attack","0","5",slider_cmd,cForeground,Id('fmat',col));
  attack->value()=0;

  decay=new HSlider(cview,Rect(184,56,56,0),FN,0,5,"decay","0","5",slider_cmd,cForeground,Id('fmde',col));
  decay->value()=1;

  ampl=new HSlider(cview,Rect(178,98,64,0),FN,0,9,"amplitude","0","9",slider_cmd,cForeground,Id('iamp',col));
  init_ampl();

  synth=new Synth(synth_top,col,false);
}

RedCtrl::RedCtrl(Rect rect,int col): CtrlBase(rect,col) {
  mode=new RButWin(cview,Rect(4,18,44,2*TDIST),FN,"mode",false,rbutton_cmd,cForeground,Id('moms',col));
  mode->add_rbut("native"); mode->add_rbut("msynth");

  button_style.param=1;
  eq=new Button(bgwin,Rect(4,60,28,0),FN,"eq?",button_cmd,Id('eq',col));
  button_style=def_but_st;

  start_timbre=new HVSlider(cview,Rect(50,TDIST,64,46),16,FN,Int2(1,2),Int2(5,4),
                            "diff/nrsin","1","5","2","4",hvslider_cmd,cForeground,Id('radn'));
  start_timbre->value.set(4,3);

  timbre=new HVSlider(cview,Rect(112,TDIST,62,46),14,FN,Int2(1,2),Int2(5,4),
                      "diff/nrsin","1","5","2","4",hvslider_cmd,cForeground,Id('rsdn'));
  timbre->value.set(3,3);

  start_amp=new VSlider(cview,Rect(174,TDIST,24,44),FN,0,3,"start-amp","0","3",slider_cmd,cForeground,Id('samp'));
  start_amp->value=2;

  tone=new RButWin(cview,Rect(202,32,38,2*TDIST),FN,"tone",false,rbutwin_cmd,cForeground,Id('redt'));
  tone->set_rbut(tone->add_rbut("clean"),false);
  tone->add_rbut("bright");

  gr_rect.set(164,80,18,3*TDIST);

  startup=new HSlider(cview,Rect(4,96,50,0),FN,0,5,"startup","0","5",slider_cmd,cForeground,Id('rest'));
  startup->value()=2;

  decay=new HSlider(cview,Rect(56,96,50,0),FN,0,5,"decay","0","5",slider_cmd,cForeground,Id('rede'));
  decay->value()=1;

  dur_limit=new HSlider(cview,Rect(108,96,54,0),FN,0,4,"dur limit","0","4",slider_cmd,cForeground,Id('durl',eRed));
  set_text(dur_limit->text,"no limit");

  ampl=new HSlider(cview,Rect(184,96,60,0),FN,0,9,"amplitude","0","9",slider_cmd,cForeground,Id('iamp',col));
  init_ampl();

  cview->display_cmd=draw;

  synth=new Synth(synth_top,col,false);
}

void RedCtrl::draw(Id id) {
  CtrlBase::draw(id);
  RedCtrl *ctr=app->red_control;
  BgrWin *cv=ctr->cview;
  static XftText txt[2] = {
    XftText(cv->win,cv->xft_win,"startup",Point(50,70)),
    XftText(cv->win,cv->xft_win,"sustain",Point(112,70))
  };
  for (int i=0;i<2;++i) txt[i].draw();
}

GreenCtrl::GreenCtrl(Rect rect,int col): CtrlBase(rect,col) {
  mode=new RButWin(cview,Rect(4,TDIST,44,2*TDIST),FN,"mode",false,rbutton_cmd,cForeground,Id('moms',col));
  mode->add_rbut("native");
  mode->add_rbut("msynth");

  button_style.param=1;
  eq=new Button(bgwin,Rect(4,50,28,0),FN,"eq?",button_cmd,Id('eq',col));
  button_style=def_but_st;

  timbre1=new HVSlider(cview,Rect(54,TDIST,50,44),12,FN,Int2(1,2),Int2(4,4),
                            "diff/nrsin","1","4","2","4",hvslider_cmd,cForeground,Id('grw1'));
  timbre1->value.set(2,3); // var_sinus[1][1]

  timbre2=new HVSlider(cview,Rect(112,TDIST,50,44),12,FN,Int2(1,2),Int2(4,4),
                            "diff/nrsin","1","4","2","4",hvslider_cmd,cForeground,Id('grw2'));
  timbre1->value.set(3,2); // var_sinus[2][0]

  freq_mult=new HSlider(cview,Rect(170,TDIST,54,0),FN,0,3,"freq ratio","0","3",slider_cmd,cForeground,Id('grfm'));
  freq_mult->value()=2;
  set_text(freq_mult->text,"2");

  chorus=new HSlider(cview,Rect(170,52,54,0),FN,0,3,"chorus","0","3",slider_cmd,cForeground,Id('grfm')); // same cmd as freq_mult
  chorus->value()=1;

  attack=new HSlider(cview,Rect(4,96,55,0),FN,0,5,"attack","0","5",slider_cmd,cForeground,Id('grat'));
  attack->value()=1;
  decay=new HSlider(cview,Rect(63,96,55,0),FN,0,5,"decay","0","5",slider_cmd,cForeground,Id('grde'));
  decay->value()=3;

  gr_rect.set(138,82,18,3*TDIST);

  ampl=new HSlider(cview,Rect(170,96,66,0),FN,0,9,"amplitude","0","9",slider_cmd,cForeground,Id('iamp',col));
  init_ampl();

  cview->display_cmd=draw;

  synth=new Synth(synth_top,col,false);
}

void GreenCtrl::draw(Id id) {
  CtrlBase::draw(id);
  GreenCtrl *ctr=app->green_control;
  BgrWin *cv=ctr->cview;
  static XftText txt[2] = {
    XftText(cv->win,cv->xft_win,"wave 1",Point(52,68)),
    XftText(cv->win,cv->xft_win,"wave 2",Point(116,68))
  };
  for (int i=0;i<2;++i) txt[i].draw();
}

BlueCtrl::BlueCtrl(Rect rect,int col): CtrlBase(rect,col) {
  attack=new HSlider(cview,Rect(5,TDIST,55,0),FN,0,5,"attack","0","5",slider_cmd,cForeground,Id('blat'));
  attack->value()=0;

  decay=new HSlider(cview,Rect(5,52,55,0),FN,0,5,"decay","0","5",slider_cmd,cForeground,Id('blde'));
  decay->value()=2;

  rich=new CheckBox(bgwin,Rect(70,10,0,0),FN,cForeground,"rich tone",0);

  chorus=new CheckBox(bgwin,Rect(70,30,0,0),FN,cForeground,"chorus",0);

  dur_limit=new HSlider(cview,Rect(145,TDIST,55,0),FN,0,4,"dur limit","0","4",slider_cmd,cForeground,Id('durl',col));
  set_text(dur_limit->text,"no limit");

  lowpass=new HSlider(cview,Rect(145,54,55,0),FN,0,4,"lowpass","0","4",slider_cmd,cForeground,Id('lpas',col));
  lowpass->value()=1;

  gr_rect.set(214,70,18,3*TDIST);

  ampl=new HSlider(cview,Rect(145,94,64,0),FN,0,9,"amplitude","0","9",slider_cmd,cForeground,Id('iamp',col));
  init_ampl();

  mode=new RButWin(cview,Rect(70,70,44,2*TDIST),FN,"mode",false,rbutton_cmd,cForeground,Id('moms',col));
  mode->add_rbut("native");
  mode->add_rbut("msynth");

  button_style.param=1;
  eq=new Button(bgwin,Rect(5,85,28,0),FN,"eq?",button_cmd,Id('eq',col));
  button_style=def_but_st;

  synth=new Synth(synth_top,col,false);
}

PurpleCtrl::PurpleCtrl(Rect rect,int col): CtrlBase(rect,col) {
  int n,
      left=43;
  static const char *const labels[]={ "1","2","3","6","9" };
  static int init[][harm_max]={{ 3,0,0,1,2 },{ 2,3,3,1,2 }};

  mode=new RButWin(cview,Rect(4,17,36,2*TDIST),FN,"mode",false,rbutton_cmd,cForeground,Id('moms',col));
  mode->add_rbut("nat"); mode->add_rbut("msyn");

  button_style.param=1;
  eq=new Button(bgwin,Rect(4,50,28,0),FN,"eq?",button_cmd,Id('eq',col));
  button_style=def_but_st;

  for (n=0;n<harm_max;++n) {
    if (n==harm_max-1)
      st_harm[n]=new VSlider(cview,Rect(left,TDIST,20,44),FN,0,3,labels[n],"0","3",slider_cmd,cForeground,Id('puah',n));
    else
      st_harm[n]=new VSlider(cview,Rect(left,TDIST,14,44),FN,0,3,labels[n],0,0,slider_cmd,cForeground,Id('puah',n));
    st_harm[n]->value=init[0][n];
    left+=13;
  }
  set_st_hs_ampl(init[0]);
  left+=10;
  for (n=0;n<harm_max;++n) {
    if (n==harm_max-1)
      harm[n]=new VSlider(cview,Rect(left,TDIST,20,44),FN,0,3,labels[n],"0","3",slider_cmd,cForeground,Id('purp',n));
    else
      harm[n]=new VSlider(cview,Rect(left,TDIST,14,44),FN,0,3,labels[n],0,0,slider_cmd,cForeground,Id('purp',n));
    harm[n]->value=init[1][n];
    left+=13;
  }
  set_hs_ampl(init[1]);

  (start_dur=new HSlider(cview,Rect(4,96,50,0),FN,0,5,"startup","0","5",slider_cmd,cForeground,Id('pusu')))->value()=2;

  (decay=new HSlider(cview,Rect(58,96,50,0),FN,0,5,"decay","0","5",slider_cmd,cForeground,Id('purd')))->value()=1;

  sound=new RButWin(cview,Rect(193,TDIST,48,4*TDIST),FN,"sound",false,rbutwin_cmd,cForeground,Id('purt'));
  sound->add_rbut("clean");
  sound->add_rbut("chorus");
  sound->add_rbut("dist'ed");
  sound->add_rbut("dist'ed~");

  gr_rect.set(140,82,18,3*TDIST);

  ampl=new HSlider(cview,Rect(172,100,72,0),FN,0,9,"amplitude","0","9",slider_cmd,cForeground,Id('iamp',col));
  init_ampl();

  cview->display_cmd=draw;

  synth=new Synth(synth_top,col,false);
}

void just_listen_cmd(Id id,bool val) {
  if (output_port!=eJack) return;
  if (val) {
    if (!wfile_play && !(wfile_play=new JackInterf("Amuc play",play_wfile,'wfd',wf_playing))->okay) wfile_play=0;
  }
  else {
    delete wfile_play;
    wfile_play=0;
  }
}
    
PhmCtrl::PhmCtrl(Rect rect,int col):
    CtrlBase(rect,col) {
  cview->display_cmd=0;
  subview1=new BgrWin(bgwin,Rect(-1,-1,rect.width,rect.height),FN,0,cForeground,1,Id(col,0)); // phys model
  subwin1=subview1->win;
  (subview2=new BgrWin(bgwin,Rect(-1,-1,rect.width,rect.height),FN,draw,cForeground,1,Id(col,0)))->hide(); // wave file
  subwin2=subview2->win;

  mode=new RButWin(subview1,Rect(2,TDIST,70,2*TDIST),FN,"samples",false,rbutton_cmd,cForeground,Id('smod',col));
  mode->add_rbut("phys model");
  mode->add_rbut("wave file");
  mode->add_to(subview2);  // mode can be re_parent'ed

  speed_tension=new HVSlider(subview1,Rect(80,TDIST,82,60),32,FN,Int2(1,1),Int2(5,5),
                             "speed/tension","1","5","1","5",hvslider_cmd,cForeground,Id('spte',col));
  decay=new HSlider(subview1,Rect(170,TDIST,60,0),FN,1,5,"decay","1","5",slider_cmd,cForeground,Id('deca',col));
  
  add_noise=new CheckBox(subwin1,Rect(80,76,0,0),FN,cForeground,"dirty",checkbox_cmd,Id('nois',col));
  switch (col) {
    case eBlack:
      speed_tension->value.set(1,3); decay->value()=4; break;
    case eRed:
      speed_tension->value.set(2,3); decay->value()=4; break;
    case eBlue:
      speed_tension->value.set(3,3); decay->value()=4; break;
    case eGreen:
      speed_tension->value.set(4,3); decay->value()=3; break;
    case ePurple:
      speed_tension->value.set(4,4); decay->value()=2; break;
    case eBrown:
      speed_tension->value.set(4,5); decay->value()=1; break;
  }
  static ChMenuData *file_sel_data=new ChMenuData(); // thus single instance
  file_select=new ChMenu(subwin2,Rect(88,TDIST,150,0),FN,show_menu,menu_cmd,
                         "wave file",Style(1,0,2),cForeground,Id('fsel',col));
  file_select->mbuttons=file_sel_data;

  button_style.set(1,cForeground,0);
  new Button(subwin2,Rect(88,38,0,0),FN,"rescan wave files",button_cmd,Id('cwf',col));
  button_style=def_but_st;

  just_listen=new CheckBox(subwin2,Rect(88,60,0,0),FN,cForeground,"just listen",just_listen_cmd);

  wave_ampl=new HSlider(subview1,Rect(2,62,70,0),FN,0,7,"wave ampl","0","7",slider_cmd,cForeground,Id('wamp',col));
  wave_ampl->value()=3;
  set_ampl_txt(3);
  wave_ampl->add_to(subview2);

  ctrl_pitch=new CheckBox(subwin1,Rect(2,94,0,0),FN,cForeground,"controlled pitch",0);
}

void PhmCtrl::show_menu(Id id) {
  PhmCtrl *pct=col2phm_ctrl(id.id2);
  pct->file_select->reset();
  if (wave_buf.fname_nr<0) { // same as button_cmd 'chof', without init_mwin()
    if (!wave_buf.sample_dirs->coll_wavefiles()) return;
  }
  for (int i=0;i<=wave_buf.fname_nr;++i) {
    FileName *fn=wave_buf.filenames+i;
    pct->file_select->add_mbut(fn->name,fn->col);
  }
}

void PurpleCtrl::draw(Id id) {
  CtrlBase::draw(id);
  PurpleCtrl *ctr=app->purple_control;
  BgrWin *cv=ctr->cview;
  static XftText txt[2] = {
    XftText(cv->win,cv->xft_win,"startup harm.",Point(45,70)),
    XftText(cv->win,cv->xft_win,"sustain harm.",Point(120,70))
  };
  for (int i=0;i<2;++i) txt[i].draw();
}

void do_atexit() { if (Xlog) x_widgets_log(); }

int read_conf_file(const char **warnings,int dim) {  // called before init_xwindows()
  char *home=getenv("HOME");
  char buf[100];
  snprintf(buf,100,"%s/.amucrc",home);
  FILE *conf=fopen(buf,"r");
  if (!conf) {
    conf=fopen(buf,"w");
    fputs("# Configuration file for Amuc\n",conf);
    fputs("\n# Version of this configuration file\n",conf);
    fputs("conf_version = 1.7-A\n",conf);
    fputs("\n# Browser called for help menu\n",conf);
    fputs("help_browser = epiphany\n",conf);
    fputs("# help_browser = firefox\n",conf);
    fputs("\n# Nominal font size\n",conf);
    fputs("font_size = 10\n",conf);
    fputs("\n# MIDI keyboard input mode, choose 'jack' or 'dev'\n",conf);
    fputs("midi_mode = jack\n",conf);
    fputs("\n# MIDI device if midi_mode = dev\n",conf);
    fputs("midi_input = /dev/midi1\n",conf);
    fputs("\n# Audio output connection, choose 'jack' or 'alsa'\n",conf);
    fputs("output_port = jack\n",conf);
    fputs("\n# Output device if output_port = alsa, choose 'default' or e.g. 'hw:0,0'\n",conf);
    fputs("pcm_dev_name = default\n",conf);
    fclose(conf);
    warnings[0]="A default configuration file has been created:";
    warnings[1]=strdup(buf);
    return 2;
  }
  Str str;
  str.cmt_ch='#';
  int ind=-1;
  for (;;) {
    str.rword(conf," =");
    if (str.ch==EOF) break;
    if (!str.s[0]) continue;
    if (str=="conf_version") {
       str.rword(conf," \n");
       if (!(str=="1.7-A")) {
         if (ind<dim-1)
           warnings[++ind]="Warning: wrong version .amucrc file (expected: conf_version = 1.7-A)";
         break;
       }
    } 
    else if (str=="help_browser") {
       str.rword(conf," \n");
       help_browser=strdup(str.s);
    }
    else if (str=="ptr_adjust") {
       if (ind<dim-1) warnings[++ind]="Warning: obsolete in .amucrc: ptr_adjust";
    }
    else if (str=="font_size") {
       str.rword(conf," \n");
       nominal_font_size=minmax(7,atoi(str.s),12);
    }
    else if (str=="midi_input") {
       str.rword(conf," \n");
       midi_input_dev=strdup(str.s);
    }
    else if (str=="output_port") {
       str.rword(conf," \n");
       if (str=="jack") output_port=eJack;
       else if (str=="alsa") output_port=eAlsa;
       else warnings[++ind]="Warning: in .amucrc: output_port must be 'jack' or 'alsa'";
    }
    else if (str=="midi_mode") {
       str.rword(conf," \n");
       if (str=="dev") midi_input_mode=eMidiDev;
       else if (str=="jack") midi_input_mode=eMidiJack;
       else warnings[++ind]="Warning: in .amucrc: midi_mode must be 'dev' or 'usb'";
    }
    else if (str=="pcm_dev_name") {
       str.rword(conf," \n");
       pcm_dev_name=strdup(str.s);
    }
    else {
      if (ind<dim-1) {
        char mess[100];
        snprintf(mess,100,"conf file .amucrc: unexpected item '%s'",str.s);
        warnings[++ind]=strdup(mess);
      }
      break;
    }
  }
  fclose(conf);
  return ind+1;
}

int main(int argc,char **argv) {
  char *inf=0;
  for (int an=1;an<argc;++an) {
    if (!strcmp(argv[an],"-h")) {
      puts("Amuc - the Amsterdam Music Composer");
      puts("Usage:");
      puts("  amuc [options...] [file.sco | file.scr]");
      puts("Files:");
      puts("  file.sco : score file");
      puts("  file.scr : script file");
      puts("Options:");
      puts("  -h    : print usage info and exit");
      puts("  -db   : debug");
      puts("  -nc   : in case jack is used, don't connect amuc output automatically");
      puts("  -mm   : print midi messages");
      puts("  -nt   : startup without default tunes");
      puts("  -V    : print version and exit");
      exit(0);
    }
    if (!strcmp(argv[an],"-V")) {
      printf("amuc - the Amsterdam Music Composer, version %s\n",version);
      exit(0);
    }
    if (!strcmp(argv[an],"-db")) debug=Xlog=true; 
    else if (!strcmp(argv[an],"-Xlog")) Xlog=true;
    else if (!strcmp(argv[an],"-nt")) no_def_tunes=true;
    else if (!strcmp(argv[an],"-nc")) no_jack_conn=true;
    else if (!strcmp(argv[an],"-mm")) midi_mes=true;
    else if (argv[an][0]=='-') err("Unexpected option %s (use -h for help)",argv[an]);
    else inf=argv[an];
  }
  const char *warnings[4];
  int nr_warn=read_conf_file(warnings,4);
  init_xwindows();
  for (int i=0;i<nr_warn;++i)
    alert(warnings[i]);
  xft_Purple=xft_calc_color(0xC0,0,0xC0); cPurple=xft_Purple->pixel;
  xft_Green=xft_calc_color(0,0xC0,0); cGreen=xft_Green->pixel;
  xft_Brown=xft_calc_color(0xA0,0x70,0x20); cBrown=xft_Brown->pixel;
  cLightGreen=calc_color("#00F000");
  cLightGrey=calc_color("#E5E5E5");
  cBgrGrey=calc_color("#F0F0F0");
  cDarkGrey=calc_color("#7F7F7F");
  cLightBlue=calc_color("#C0FFFF");
  cScopeBgr=calc_color("#D0E7FF");

  linecol.init();
  def_erb_st.bgcol=calc_color("#F2F2F2");
  slider_style=def_sl_st;
  button_style=def_but_st;
  ext_rbut_style=def_erb_st;
  checkbox_style.set(1,0,0);
  init_cursors();
  wave_buf.init();

  top_win=create_top_window("Amuc",Rect(100,0,view_hmax,view_vmax),true,0,cBackground);

  set_icon(top_win->win,create_pixmap(icon).pm,14,16);

  App ap(inf); // sets app
  map_top_window();
  run_xwindows();
  return 0;
}
