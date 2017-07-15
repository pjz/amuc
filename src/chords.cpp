#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <math.h>
#include "amuc-headers.h"

static const char *chords_and_scales="chords-and-scales";
const char
  *sharp[] = { 
    "6 10 2 1",
    ". c #ffffff",
    "# c #0000ff",
    ".#..#.",
    ".#..##",
    ".####.",
    "##..#.",
    ".#..#.",
    ".#..##",
    ".####.",
    "##..#.",
    ".#..#.",
    ".#..#."
  },
  *flat[] = {
    "6 10 2 1",
    "* c #0000ff",
    ". c #ffffff",
    "*.....",
    "*.....",
    "*.....",
    "*.....",
    "*.***.",
    "**...*",
    "*....*",
    "*...*.",
    "*.**..",
    "**...."
  };


static const char
  *nr_signs[keys_max] = {
    0,"5","2","3","4","1","6","6","1","4","3","2","5"
  };
static Pixmap2
  nul,
  *sign,
  sharp_pm,
  flat_pm;

void keys_text(uint win,XftDraw *xft_win,Id,int n,int) {
  int ind=keys_max-1-n;
  xft_draw_string(xft_win,xft_Black,Point(4,n*TDIST+10),maj_min_keys[ind]);
  xft_draw_string(xft_win,xft_Black,Point(34,n*TDIST+10),maj_min_keys[ind+keys_max]);
  if (ind > 0) {
    xft_draw_string(xft_win,xft_Blue,Point(64,n*TDIST+10),nr_signs[ind]);
    draw_pixmap(win,Point(74,n*TDIST),sign[ind],6,10);
  }
}

ChordsWindow::ChordsWindow(Point top):
    the_key_nr(0),dm(eScDep_no),shift_12(false) {
  const int dx=300,
            dy=430;
  subw=new SubWin("keys, chords, scales",Rect(top.x,top.y,dx,dy),true,cForeground,0,hide_chwin);
  chwin=subw->win; xft_chwin=subw->xft_win;

  sharp_pm=create_pixmap(sharp);
  flat_pm=create_pixmap(flat);
  static Pixmap2 sgn[keys_max]= {
    nul,flat_pm,sharp_pm,flat_pm,sharp_pm,flat_pm,sharp_pm,flat_pm,sharp_pm,flat_pm,sharp_pm,flat_pm,sharp_pm
  };
  sign=sgn;

  keys=new RButWin(subw,Rect(0,TDIST,86,keys_max*TDIST),FN,"keys",false,base_cmd,cForeground);
  but1=new Button(chwin,Rect(2,212,48,0),FN,"set key",set_key);
  but2=new Button(chwin,Rect(52,212,58,0),FN,"force key",force_key);
  get_scales=new Button(chwin,Rect(120,212,72,0),FN,"read scales",read_scales);
  dep_mode=new CheckBox(chwin,Rect(210,212,0,15),FN,cForeground,"local dep.",depm_cmd);
  tone_nrs=new EditWin(chwin,Rect(90,TDIST,17,keys_max*TDIST),FN,false,0);
  scales=new RButWin(subw,Rect(120,TDIST,164,190),FN,"scales, chords",false,scales_cmd,cForeground);
  scroll=new VScrollbar(chwin,Rect(286,TDIST,0,190),FN,0,scr_cmd);
  for (int n=0;;++n) {
    RButton *rbut=keys->add_rbut(keys_text);
    if (n==keys_max-1) {
      keys->set_rbut(rbut,false);
      break;
    }
  }
  draw_tone_numbers();
  circle=new BgrWin(chwin,Rect(50,235,190,190),FN,draw_circ,cForeground,0,Id(0,0));
}

void ChordsWindow::draw_circ(Id id) {
  ChordsWindow *chw=app->chordsWin;
  BgrWin *bgwin=chw->circle;
  static Point mid(95,95);
  static int butnr_to_i[13]={ 0,5,10,3,8,1,6,6,11,4,9,2,7 },
             angle_to_i[12]={ 0,5,11,3,9,1,7,12,4,10,2,8 };
  static Point
    dot[12],
    label[12];
  static bool init=false;
  int i;
  bool draw_dots=id.id2; // only update dot's?
  if (!init) {
    init=true;
    for (i=0;i<12;++i) {
      dot[i].set(mid.x+int(60*sin(i*M_PI/6)),mid.y-int(60*cos(i*M_PI/6)));
      label[i].set(mid.x-6+int(80*sin(i*M_PI/6)),int(mid.y-80*cos(i*M_PI/6)));
    }
  }
  if (draw_dots)
    for (i=0;i<12;++i)
      fill_circle(bgwin->win,1,i==butnr_to_i[12-chw->keys->act_rbutnr()] ? cRed : cForeground,dot[i],4);
  else {
    bgwin->clear();
    cairo_t *cr=cairo_create(cai_get_surface(bgwin));
    cairo_set_line_width(cr,2);
    cairo_set_source(cr,cai_Border);
    cairo_arc(cr,mid.x+0.5,mid.y+0.5,60,0,2*M_PI);
    cairo_stroke(cr);
    cairo_set_line_width(cr,2);
    for (i=0;i<12;++i) {
      fill_circle(bgwin->win,1,i==butnr_to_i[12-chw->keys->act_rbutnr()] ? cRed : cForeground,dot[i],5);
      cairo_arc(cr,dot[i].x+0.5,dot[i].y+0.5,5,0,2*M_PI);
      cairo_stroke(cr);
      xft_draw_string(bgwin->xft_win,xft_Black,label[i],maj_min_keys[angle_to_i[i]]);
      if (i>0) {
        int ind=angle_to_i[i];
        xft_draw_string(bgwin->xft_win,xft_Blue,Point(label[i].x,label[i].y+12),nr_signs[ind]);
        draw_pixmap(bgwin->win,Point(label[i].x+10,label[i].y+2),sign[ind],6,10);
      }
    }
    cairo_destroy(cr);
  }
}

void ChordsWindow::base_cmd(Id,int nr,int) { // nr between 0 and keys_max-1
  ChordsWindow *cw=app->chordsWin;
  cw->the_key_nr=keys_max-1-nr;
  cw->draw_tone_numbers();
  cw->upd_scoreview(cw->scales->act_rbutnr());
  cw->draw_circ(Id(0,1));
}

void ChordsWindow::depm_cmd(Id,bool val) {
  ChordsWindow *cw=app->chordsWin;
  if (cw->dm!=eScDep_no) cw->dm= val ? eScDep_lk : eScDep_sk;
}

void ChordsWindow::scales_cmd(Id,int nr,int) {
  static int prev_nr,
             dmode,prev_dmode;
  ChordsWindow *cw=app->chordsWin;
  dmode=cw->scale_data[cw->scales->act_rbutnr()].depmode;
  if (prev_dmode!=dmode) {
    if (dmode==eScDep_lk) { cw->dep_mode->set_cbval(true,0); cw->dm=eScDep_lk; }
    else if (dmode==eScDep_sk) { cw->dep_mode->set_cbval(false,0); cw->dm=eScDep_sk; }
    else cw->dm=eScDep_no;
  }
  prev_dmode=dmode;
  cw->shift_12= prev_nr==nr ? !cw->shift_12 : false;  // if gt12, then shift start of chord 1 octave?
  prev_nr=nr;
  cw->upd_scoreview(nr);
}

void ChordsWindow::hide_chwin(Id) {
  hide_window(app->chordsWin->chwin);
}
void ChordsWindow::chord_or_scale_name(char *buf,int sc_key_nr) {   // returns temporary string
  if (!scales->act_button) {
    alert("chord_or_scale_name: scales not yet read");
    return;
  }
  int key_nr= dm==eScDep_lk ? the_key_nr : sc_key_nr;
  switch (scale_data[scales->act_rbutnr()].depmode) {
    case eScDep_lk:
      snprintf(buf,50,"chord: %s%s",
        maj_min_keys[key_nr],          // e.g. "Db"
        scales->act_button->label.txt+1);  // e.g. "C7", skip 'C'
      break;
    case eScDep_sk:
      snprintf(buf,50,"scale: %s %s",
        maj_min_keys[key_nr],          // e.g. "Db"
        scales->act_button->label.txt);    // e.g. "major"
      break;
    default:
      strncpy(buf,scales->act_button->label.txt,50); // e.g. "white keys"
  }
}

int keynr2ind(int key_nr) {  // key_nr -> array index
  static int ind[keys_max] = { 0,1,2,3,4,5,6,6,7,8,9,10,11 };
  return ind[key_nr];
}

void ChordsWindow::draw_tone_numbers() {
  int n;
  static const char *nrs[8]={ 0,"1","2","3","4","5","6","7" };
  static int toneNrs[13][keys_max] = {
    // C   D   E F     G   A   B  <-- C major
     { 1,0,2,0,3,4,0,0,5,0,6,0,7 }, // C
     { 7,1,0,2,0,3,4,4,0,5,0,6,0 }, // Des
     { 0,7,1,0,2,0,3,3,0,4,5,0,6 }, // D
     { 6,0,7,1,0,2,0,0,3,4,0,5,0 }, // Es
     { 0,6,0,7,1,0,2,2,0,3,4,0,5 }, // E
     { 5,0,6,0,7,1,0,0,2,0,3,4,0 }, // F
     { 0,5,0,6,0,7,1,1,0,2,0,3,4 }, // Fis
     { 0,5,0,6,0,7,1,1,0,2,0,3,4 }, // Ges
     { 4,0,5,0,6,0,7,7,1,0,2,0,3 }, // G
     { 3,4,0,5,0,6,0,0,7,1,0,2,0 }, // As
     { 0,3,4,0,5,0,6,6,0,7,1,0,2 }, // A
     { 2,0,3,4,0,5,0,0,6,0,7,1,0 }, // Bes
     { 0,2,0,3,4,0,5,5,0,6,0,7,1 }, // B
  };
  tone_nrs->reset();
  int *act_tone_nrs=toneNrs[the_key_nr];
  for (n=0;n<keys_max;++n)
    if (act_tone_nrs[n])
      tone_nrs->set_line((char*)nrs[act_tone_nrs[n]],keys_max-1-n);
}

bool ChordsWindow::read_scalef(const char *dname) {
  char buf[max100];
  FILE *fp;
  Str str;
  int n,nr;
  XftColor *text_col;
  snprintf(buf,max100,"%s/%s",dname,chords_and_scales);
  if ((fp=fopen(buf,"r")) == 0) return false;
  scales->empty();
  scroll->value=0;
  str.cmt_ch='#';
  for (nr=0;;++nr) {
    str.rword(fp," \t\"\n");
    if (!str.s[0]) {
      if (str.ch=='"') {  // quoted?
        str.rword(fp,"\"\n");
        if (str.ch=='"') fgetc(fp);
        else { alert("missing '\"'"); return true; }
      }
      else break;
    }
    for (char *p=str.s;*p;++p) if (*p=='\t') *p=' '; // replace tabs
    scale_data[nr].name=strndup(str.s,ScaleData::nmax);
    for (n=0;n<24;++n) scale_data[nr].valid[n]=false;
    scale_data[nr].gt12=false;
    scale_data[nr].depmode=eScDep_no;
    text_col=xft_Black;
    for (;;) {
      str.rword(fp,", \t\n");
      n=atoi(str.s);
      if (n>24) { alert("note nr %d (should be < 24)",n); return true; }
      if (n>12) scale_data[nr].gt12=true;
      scale_data[nr].valid[n]=true;
      if (strchr("\n \t",str.ch)) break;
    }
    if (str.ch!='\n') {
      str.rword(fp," \t\n");
      if (str=="no") scale_data[nr].depmode=eScDep_no;
      else if (str=="chord") { scale_data[nr].depmode=eScDep_lk; text_col=xft_Blue; }
      else if (str=="scale") { scale_data[nr].depmode=eScDep_sk; text_col=xft_Red; }
      else {
        alert("scale/chord type should be 'no', 'chord' or 'scale'");
        return true;
      }
    }
    scales->add_rbut(scale_data[nr].name,text_col);
    scroll->set_range((scales->buttons->butnr+2)*TDIST);
  }
  return true;
}

void ChordsWindow::read_scales(Id) {
  if (!app->chordsWin->read_scalef(cur_dir) && !app->chordsWin->read_scalef(amuc_data))
    alert("file %s not found in . or in %s",chords_and_scales,amuc_data);
}
