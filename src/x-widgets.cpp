#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
//#include <X11/xpm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "x-widgets.h"

struct Color5 {
  uint *c;
  Color5():c(0) { }
  void set_color(const char* c0,const char* c1,const char* c2,const char* c3,const char* c4) {
    c=new uint[5];
    c[0]=calc_color(c0); c[1]=calc_color(c1); c[2]=calc_color(c2); c[3]=calc_color(c3); c[4]=calc_color(c4);
  }
};

uint key_pressed,  // keyboard event for top window
     ctrl_key_pressed,
     cBlack, cWhite, cGrey, cRed, cBlue, cRose, cButBground, cMenuBground, cCheckBox,
     cBorder, cBackground, cForeground, cSelRBut,
     nominal_font_size=10;
bool x_running; // false upto map_top_window(), true at start of run_xwindows()

XftColor *xft_White,*xft_Black,*xft_Blue,*xft_Red;
cairo_pattern_t *cai_White, *cai_Black, *cai_Blue, *cai_Red, *cai_Border;

Style button_style(0,0,0),
      ext_rbut_style(0,0,0),
      checkbox_style(0,0,0);

SliderStyle slider_style(1,true);

static Color5 cGradientBlue, cGradientRose, cGradientWheat, cGradientGreen, cGradientGrey;
static uint cSelCmdM, cSlBackgr, cPointer, cScrollbar, cScrollbar1,
            go_ticks;
static const int
  LBMAX=20,     // labels etc.
  MBDIST=18;    // cmd menu buttons
static int
  char_wid,
  pixdepth;

struct Repeat {
  bool on;
  XEvent ev;
} repeat;

static GC
  def_gc,    // default
  clip_gc;   // clip mask
static XGCValues gcv;
static XGlyphInfo glyphinfo;
static XftFont *xft_def_font,
        *xft_bold_font,
        *xft_small_font,
        *xft_mono_font;
static XWMHints wm_icon_hints;
static Display *dis;
static Visual *vis;
static Colormap cmap;
static uint screen,
            root_window;
//Cursor cursor_xterm;
static Atom WM_DELETE_WINDOW,
     WM_PROTOCOLS,
     CLIPBOARD;

static const char *lamp_pm[]={
"12 12 4 1",
"# c #606060",
"a c #a0a0a0",
"b c #e0e0e0",
". c #ffffff",
"....##aa....",
"..####aaaa..",
".#####aaaaa.",
".#####aaaaa.",
"######aaaaaa",
"######aaaaaa",
"######bbbbbb",
"aaaaaabbbbbb",
".aaaaabbbbb.",
".aaaaabbbbb.",
"..aaaabbbb..",
"....aabb...."
};

static struct AlertWin *alert_win;

struct TheCursor {
  struct DialogWin *diawd;
  struct EditWin *edwd;
  void unset();
} the_cursor;

static pthread_mutex_t mtx= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t xwin_thread;

/* This could be used in all drawing functions:
   if (!pthread_equal(pthread_self(),xwin_thread)) puts("X event from other thread");
*/

static int min(int a, int b) { return a<=b ? a : b; }
static int max(int a, int b) { return a>=b ? a : b; }
static int minmax(int a, int x, int b) { return x>=b ? b : x<=a ? a : x; }
static int divide(int a,int b) { return (2 * a + b)/b/2; }

void err(const char *form,...) {
  va_list ap;
  va_start(ap,form);
  printf("Error: ");
  vprintf(form,ap);
  va_end(ap);
  putchar('\n');
  exit(1);
}

void say(const char *form,...) {   // for debugging
  va_list ap;
  va_start(ap,form);
  printf("say: "); vprintf(form,ap); putchar('\n');
  va_end(ap);
  fflush(stdout);
}

Id::Id(int _id1):id1(_id1),id2(-1) { }
Id::Id(int _id1,int _id2):id1(_id1),id2(_id2) { }

Label::Label(const char* t):txt(t),undersc(-1),draw(0) { }
Label::Label(const char* t,int us):txt(t),undersc(us),draw(0) { }
Label::Label(void (*dr)(uint win,XftDraw *xft_win,Id id,int par,int y_off)):txt(0),undersc(-1),draw(dr) { }
Label::Label(const char *t,void (*dr)(uint win,XftDraw *xft_win,Id id,int par,int y_off)):txt(t),undersc(-1),draw(dr) { }

Style::Style(int _st,uint col,int par):st(_st),bgcol(col),param(par) { }
void Style::set(int _st,uint col,int par) { st=_st; bgcol=col; param=par; }

SliderStyle::SliderStyle(int _st,bool gr):st(_st),grid(gr) { }
void SliderStyle::set(int _st,bool gr) { st=_st; grid=gr; }

Pixmap2::Pixmap2():pm(0),cm(0) { }

template<class T> T* re_alloc(T* arr,int& len,T ival) {
  int i;
  T* new_arr=new T[len*2];
  for (i=0;i<len;++i) new_arr[i]=arr[i];
  delete[] arr;
  len*=2;
  for(;i<len;++i) new_arr[i]=ival;
  return new_arr;
}
 
struct TopWin:WinBase {
  void (*display_cmd)(XftDraw *win,Rect exp_rect);
  TopWin(const char* title,Rect,bool resize,void (*draw)(XftDraw *,Rect),uint bg);
  bool handle_key(XKeyEvent *ev);
  void handle_key_release(XKeyEvent *ev);
} *top_wd; // top window

struct AlertWin {
  uint awin;
  TextWin *textwin;
  SubWin *subw;
  AlertWin() {
    const int dim=8;
    Rect rect(0,0,404,dim*TDIST+10+4);
    subw=new SubWin("Alert",rect,x_running,cRose,0,del_cmd,Id(0,0)); // only mapped if x_running
    awin=subw->win;
    textwin=new TextWin(awin,Rect(4,4,rect.width-10,rect.height-10),FN,dim);
  }
  static void del_cmd(Id) {
    delete_window(alert_win->awin);
    delete alert_win; alert_win=0;
  }
  ~AlertWin() { delete textwin; delete subw; }
};

void alert(const char *form,...) {
  char buf[100];
  va_list ap;
  va_start(ap,form);
  vsnprintf(buf,100,form,ap);
  va_end(ap);
  if (dis) {
    if (!alert_win) alert_win=new AlertWin();
    alert_win->textwin->print_text(buf);
  }
  else
    puts(buf);
}

template<class T>
struct WinBuf {
  int win_nr,
      wmax;
  T **wbuf;
  const char* type;
  WinBuf(int wm,const char *t):
      win_nr(-1),
      wmax(wm),
      wbuf(new T*[wmax]),
      type(t) {
    for (int i=0;i<wmax;++i) wbuf[i]=0;
  }
  T*& next_win() {
    if (win_nr==wmax-1)
      wbuf=re_alloc<T*>(wbuf,wmax,0);
    return wbuf[++win_nr];
  }
  bool in_a_win(Window win,T*& rbw) {
    int i;
    T *wd;
    for (i=0;i<=win_nr;++i) {
      wd=wbuf[i];
      if (win==wd->win) { rbw=wd; return true; }
    }
    return false;
  }
  void unlist_widget(T* wd) {  // remove widget adress from wbuf[]
    int i;
    for (i=0;i<=win_nr;++i) {
      if (wd==wbuf[i]) {
        wbuf[i]=wbuf[win_nr];
        --win_nr;
        return;
      }
    }
    alert("unlist_widget: widget not found");
  }
};

WinBuf<DialogWin>   dia_winb(1,"DialogWin");  // dialogs
WinBuf<TextWin>     t_winb(10,"TextWin");     // text windows
WinBuf<Button>      b_winb(100,"Button");     // push buttons
WinBuf<BgrWin>      gr_winb(10,"BgrWin (graph)");   // graph windows
WinBuf<BgrWin>      bg_winb(100,"BgrWin (backgr)"); // background windows
WinBuf<RButWin>     rb_winb(100,"RButWin");   // radio-buttons
WinBuf<HSlider>     hsl_winb(100,"HSlider");  // horizontal sliders
WinBuf<VSlider>     vsl_winb(50,"VSlider");   // vertical sliders
WinBuf<HVSlider>    hvsl_winb(40,"HVSlider"); // 2-dim sliders
WinBuf<HScrollbar>  hsc_winb(20,"HScrollbar");// horizontal scrollbars
WinBuf<VScrollbar>  vsc_winb(20,"VScrollbar");// vertical scrollbars
WinBuf<EditWin>     ed_winb(5,"EditWin");     // editable text windows
WinBuf<ExtRButCtrl> rxb_buf(30,"ExtRButCtrl");// radio exportable buffers
WinBuf<CheckBox>    chb_winb(40,"CheckBox");  // checkboxes
WinBuf<SubWin>      subw_winb(20,"SubWin");   // subwindows
WinBuf<ChMenu>      chmenu_winb(10,"ChMenu");   // choice menus
WinBuf<CmdMenu>     cmdmenu_winb(10,"CmdMenu"); // command menus
                                                                                    
Point::Point(short x1,short y1) { x=x1; y=y1; }
Point::Point() { x=0; y=0; }
void Point::set(short x1,short y1) { x=x1; y=y1; }
bool Point::operator==(Point b) { return x==b.x && y==b.y; }
bool Point::operator!=(Point b) { return x!=b.x || y!=b.y; }

void Int2::set(int x1,int y1) { x=x1; y=y1; }
Int2::Int2():x(0),y(0) { }
Int2::Int2(int x1,int y1):x(x1),y(y1) { }
bool Int2::operator!=(Int2 b) { return x!=b.x || y!=b.y; }

Segment::Segment():x1(0),y1(0),x2(0),y2(0) { }
Segment::Segment(short _x1,short _y1,short _x2,short _y2):x1(_x1),y1(_y1),x2(_x2),y2(_y2) { }
void Segment::set(short _x1,short _y1,short _x2,short _y2) { x1=_x1; y1=_y1; x2=_x2; y2=_y2; }

Rect::Rect(short _x,short _y,ushort _dx,ushort _dy) { x=_x; y=_y; width=_dx; height=_dy; }
void Rect::set(short _x,short _y,ushort _dx,ushort _dy) { x=_x; y=_y; width=_dx; height=_dy; }

static void set_win_attr(bool is_topwin,uint &win,XftDraw *&xft_win,Rect rect,const char* title,bool resize,uint bg) {
  XSetWindowAttributes attr;
  attr.event_mask= KeyPressMask|     // for menu's
                   KeyReleaseMask|   // for menu's
                   ExposureMask|     // for draw_title()'s
                   ButtonPressMask;  // to unset cursor
  if (resize) attr.event_mask|=StructureNotifyMask;  // for ConfigureNotify events
  attr.background_pixel = bg;
  uint mask=CWBackPixel|  // for attr.background_pixel
            CWEventMask;  // for attr.event_mask
  win=XCreateWindow(dis,root_window,rect.x,rect.y,rect.width,rect.height,0,0,
                    InputOutput,CopyFromParent,mask,&attr);
  xft_win=XftDrawCreate(dis,win,vis,cmap);

  XSizeHints sizehint;
  if (resize) {
    sizehint.flags= PSize|PPosition|PMinSize|PResizeInc;
    sizehint.width=sizehint.base_width=sizehint.min_width=rect.width;
    sizehint.height=sizehint.base_height=sizehint.min_height=rect.height;
    sizehint.width_inc=sizehint.height_inc=10;
  }
  else {
    sizehint.flags= PSize|PPosition|PMinSize|PMaxSize;
    sizehint.width=sizehint.max_width=sizehint.min_width=rect.width;
    sizehint.height=sizehint.max_height=sizehint.min_height=rect.height;
  }
  XTextProperty win_title;
  XStringListToTextProperty((char**)(&title),1,&win_title);

  XClassHint classHint;  // only used if is_topwin
  classHint.res_class=classHint.res_name=(char*)title;
  /*
  XWMHints wmHint;
  wmHint.flags = (InputHint | StateHint | WindowGroupHint);
  wmHint.input = True;
  wmHint.initial_state = NormalState;
  wmHint.window_group = root_window;
  XSetWMProperties(dis,win,&win_title,&win_title,0,0,&sizeHint,&wmHint,&classHint);
  */
  XSetWMProperties(dis,win,&win_title,&win_title,0,0,&sizehint,0,is_topwin ? &classHint : 0);
  XChangeProperty(dis,win,WM_PROTOCOLS,XA_ATOM,32,PropModeReplace,(uchar*)&WM_DELETE_WINDOW,1);
}

TopWin::TopWin(const char* top_t,Rect rect,bool resize,void (*draw)(XftDraw *win,Rect exp_rect),uint bg):
    WinBase(0,0,rect.x,rect.y,rect.width,rect.height,FN,0) {
  set_win_attr(true,win,xft_win,rect,top_t,resize,bg);
  pixdepth=DefaultDepth(dis,screen);
  display_cmd=draw;
}

SubWin::SubWin(const char* sub_t,Rect rect,bool do_map,uint bg,void(*disp_cmd)(Rect,Id),void(*dcmd)(Id),Id _id):
    WinBase(0,0,rect.x,rect.y,rect.width,rect.height,FN,_id),
    display_cmd(disp_cmd),
    del_cmd(dcmd) {
  set_win_attr(false,win,xft_win,rect,sub_t,false,bg);
  if (do_map) XMapWindow(dis, win);
  subw_winb.next_win()=this;
}

SubWin::~SubWin() { subw_winb.unlist_widget(this); }

static XftFont *font2xft_font(uint font) {
  return font==BOLDfont ? xft_bold_font :
         font==SMALLfont ? xft_small_font :
         xft_def_font;
}

void xft_draw_string(XftDraw *xft_win,XftColor *col,Point pt,const char* s,uint font) {
  if (s) XftDrawString8(xft_win,col,font2xft_font(font),pt.x,pt.y,(XftChar8*)s,strlen(s));
}

static int xft_text_width(const char *s,XftFont *font) {
  if (!s) return 0;
  XGlyphInfo glyph;
  XftTextExtents8(dis,font,(const FcChar8*)s,strlen(s),&glyph);
  return glyph.xOff;
}

int xft_text_width(const char *s) { return xft_text_width(s,xft_def_font); }

void xft_draw_form_string(XftDraw *xft_win,XftColor *col,Point pt,const char* s) {
  if (s) {
    char *p,*lst_p;
    int n=0,n1=0,lst_n1=0;
    for (p=lst_p=(char*)s;*p;++p) {
      if (*p=='#') {
        n=atoi(p+1);
        if (n>0) {
          lst_n1=n1;
          n1=n;
          XftDrawString8(xft_win,col,xft_def_font,pt.x+lst_n1,pt.y,(XftChar8*)lst_p,p-lst_p);
          //XDrawString(dis,win,def_gc,pt.x+lst_n1,pt.y,lst_p,p-lst_p);
          do ++p; while (isdigit(*p));
          lst_p=p;
          --p;
        }
      }
    }
    XftDrawString8(xft_win,col,xft_def_font,pt.x+n1,pt.y,(XftChar8*)lst_p,p-lst_p);
    //XDrawString(dis,win,def_gc,pt.x+n1,pt.y,lst_p,p-lst_p);
  }
}

WinBase::WinBase(uint pw,const char *t,int _x,int _y,int _dx,int _dy,uint r_m,Id _id):
    pwin(pw),
    win(0),
    children(0),
    xft_win(0),
    x(_x),y(_y),dx(_dx),dy(_dy),
    lst_child(-1),
    id(_id),
    rez_mode(r_m),
    t_rect(0,0,0,0),
    title(t) {
}

void WinBase::add_child(WinBase *child) {
  if (!children) {
    end_child=5;
    children=new WinBase*[end_child];
  }
  else if (lst_child==end_child-1)
    children=re_alloc<WinBase*>(children,end_child,0);
  children[++lst_child]=child;
}

bool a_in_b(Rect a,Rect b) { // a and b overlap?
  return ((a.x>=b.x && a.x<=b.x+b.width) || (a.x+a.width>=b.x && a.x+a.width<=b.x+b.width) || (a.x<b.x && a.x+a.width>b.x+b.width)) &&
         ((a.y>=b.y && a.y<=b.y+b.height) || (a.y+a.height>=b.y && a.y+a.height<=b.y+b.height) || (a.y<b.y && a.y+a.height>b.y+b.height));
}

void WinBase::draw_title(XftDraw *xft_pwin,Rect exp_rect) {
  if (!a_in_b(exp_rect,t_rect)) return;
  clear(pwin,t_rect);
  xft_draw_string(xft_pwin,xft_Black,Point(x+2,y-4),title,BOLDfont);
}

void WinBase::mv_resize(uint w,int ddx,int ddy) {
  if (w!=pwin) return;
  switch (rez_mode) {
    case FN:
      break;
    case MR:
      x+=ddx;
      t_rect.x=x+2;
      XMoveWindow(dis,win,x,y);
      break;
    case FR:
      dx += ddx;
      XResizeWindow(dis,win,dx,dy);
      break;
    default:
      alert("unknown resize mode %d",rez_mode);
  }
}

TextWin::TextWin(uint pw,Rect rect,uint r_m,int lm,Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height ? rect.height : lm*TDIST,r_m,_id),
    linenr(-1),
    lmax(lm),
    xft_textcol(new XftColor*[lmax]),
    textbuf(new char[lmax][SMAX]) {
  t_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,cWhite);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask);
  XMapWindow(dis, win);
}

void TextWin::reset() {
  linenr=-1;
  XClearWindow(dis,win);
}

void TextWin::draw_text() {
  int n,n1;
  char *p;
  XClearWindow(dis,win);
  n= linenr<lmax ? 0 : linenr-lmax+1;
  for (n1=n;n1<n+lmax && n1<=linenr;++n1) {
    int ind=n1%lmax;
    p=textbuf[ind];
    xft_draw_form_string(xft_win,xft_textcol[ind],Point(4,TDIST*(n1-n+1)-2),p);
  }
}

struct Line {
  int dlen,     // occupied in data[]
      leftside,
      dmax;     // length of data[]
  char *data;
  Line():dlen(0),leftside(0),dmax(50),data(new char[dmax]) {
    data[0]=0;
  }
  int xpos(int nr) {
    return 2+char_wid*(nr-leftside);
  }
  void reset() { data[0]=0; dlen=leftside=0; }
  void insert_char(KeySym ks,int& n) {
    int i;
    if (dlen>=dmax-1) data=re_alloc<char>(data,dmax,0);
    for (i=dlen;i>n;--i) data[i]=data[i-1];
    data[i]=ks;
    ++dlen; ++n; data[dlen]=0;
  }
  void rm_char(int& n) {
    int i;
    if (dlen==0 || n==0) return;
    for (i=n-1;i<dlen;++i) data[i]=data[i+1];
    --dlen; --n;
  }
  void cpy(const char *s) {
    dlen=strlen(s);
    while (dmax<=dlen) data=re_alloc<char>(data,dmax,0);
    strcpy(data,s);
  }
  void cat(char *s) {
    dlen+=strlen(s);
    while (dmax<=dlen) data=re_alloc<char>(data,dmax,0);
    strcat(data,s);
  }
};

XftText::XftText(uint w,XftDraw *xft_w,const char *txt,Point pt,uint fnt_nr):
    win(w),
    fontnr(fnt_nr),
    xft_win(xft_w),
    text(txt),
    start(pt),
    rect(0,0,0,0) {
  XGlyphInfo glyph;
  XftTextExtents8(dis,font2xft_font(fontnr),(const FcChar8*)text,strlen(text),&glyph);
  rect.set(start.x, start.y-glyph.y, glyph.xOff, glyph.height);
}

void XftText::draw() {
  clear(win,rect);
  xft_draw_string(xft_win,xft_Black,start,text,fontnr);
}

DialogWin::DialogWin(uint pw,Rect rect,uint r_m,uint bg,Id _id):
    WinBase(pw,0,rect.x,rect.y-TDIST,rect.width,2*TDIST+2,r_m,_id),
    show_menu(false),
    cursor(-1),
    cmd_id(0),
    label(0),
    bgcol(cWhite),
    lin(new Line),
    fill_menu(0),
    cmd(0) {
  dia_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bg);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|PropertyChangeMask);
  //XDefineCursor(dis,win,cursor_xterm);
  XMapWindow(dis, win);
}

DialogWin::DialogWin(uint pw,Rect rect,uint r_m,bool al_lab,uint bg,Id _id):
    WinBase(pw,0,rect.x,rect.y-TDIST,rect.width,2*TDIST+2,r_m,_id),
    show_menu(false),
    alloc_lab(al_lab),
    cursor(-1),
    butnr(-1),
    mb_max(20),
    cmd_id(0),  // assigned by ddefault()
    but(new ChMButton[mb_max]),
    label(0),
    bgcol(cWhite),
    mbut_win(0),
    xft_mbut_win(0),
    lin(new Line),
    fill_menu(0),  // assigned by ddefault()
    cmd(0) {
  dia_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bg);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|PropertyChangeMask);
  //XDefineCursor(dis,win,cursor_xterm);
  XMapWindow(dis, win);
}
void DialogWin::print_line() {
  char *s=lin->data;
  const int rmargin=15;
  set_color(cWhite); XFillRectangle(dis,win,def_gc,0,TDIST,dx-1,dy-TDIST);
  if (cursor>=0) {
    if (lin->xpos(cursor)>=dx-rmargin)
      lin->leftside=cursor - (dx-rmargin)/char_wid;
    int x1=lin->xpos(cursor);
    draw_line(win,2,cBlack,Point(x1,TDIST+2),Point(x1,TDIST+14));
  }
  XftDrawString8(xft_win,xft_Black,xft_mono_font,2,dy-4,(XftChar8*)(s+lin->leftside),lin->dlen-lin->leftside);
  if (show_menu) {
    fill_rectangle(win,cGradientGrey.c[3],Rect(dx-14,TDIST,14,dy-TDIST));
    const int x1=dx-7,y1=TDIST+10;
    fill_triangle(win,cBlack,Point(x1,y1),Point(x1-4,y1-6),Point(x1+4,y1-6));
  }
  set_width_color(1,cBorder); XDrawRectangle(dis,win,def_gc,0,TDIST,dx-1,dy-TDIST-1);
}
void DialogWin::unset_cursor() {
  if (cursor>=0) {
    cursor=-1;
    lin->leftside=0;
    print_line();
  }
}
void DialogWin::draw_dialabel() {
  const char *s=label;
  if (!s) return;
  XClearArea(dis,win,0,0,dx,TDIST,false);
  XSetForeground(dis, def_gc, bgcol);
  XFillRectangle(dis,win,def_gc,0,0,xft_text_width(s)+4,TDIST);
  xft_draw_string(xft_win,xft_Black,Point(2,10),s);
}
void DialogWin::dlabel(const char *s,uint col) {
  label=s;
  bgcol=col;
  draw_dialabel();
}
void DialogWin::copy(const char *s) {
  strcpy(lin->data,s);
  lin->dlen=strlen(lin->data);
  cursor=-1; lin->leftside=0;
  print_line();
}

void DialogWin::ddefault(const char *s,void(*cmd1)(const char*)) { ddefault(s,cmd1,false,0,0); }

void DialogWin::ddefault(const char *s,void(*cmd1)(const char*),bool show_m,void (*fill_m)(int mode),int cmdid) {
  show_menu=show_m;
  copy(s);
  cmd=cmd1;
  fill_menu=fill_m;
  cmd_id=cmdid;
  if (mbut_win) reset_mwin();
}
void DialogWin::dok() {
  if (cmd) cmd(lin->data);
  else alert("dialog: no action specified");
}
void DialogWin::insert_string(uchar *s) {
  if (cursor<0) return;
  for (;*s;++s) {
    if (*s>=0x20 && *s<0x80) 
      lin->insert_char(*s,cursor);
  }
  print_line();
}

bool DialogWin::handle_key(KeySym ks) {  // see: /usr/include/X11/keysymdef.h
  switch (ks) {
    case XK_Return:
      unset_cursor();
      return 1;
    case XK_Control_L:
      return 0;
    case XK_BackSpace:
      lin->rm_char(cursor);
      if (lin->xpos(cursor)<10 && lin->leftside>0) --lin->leftside;
      print_line();
      break;
    case XK_Left:
      if (cursor>0) {
        --cursor;
        if (lin->xpos(cursor)<10 && lin->leftside>0) --lin->leftside;
        print_line();
      }
      break;
    case XK_Right:
      if (cursor<lin->dlen) {
        ++cursor;
        if (lin->xpos(cursor)>=dx-10) ++lin->leftside;
        print_line();
      }
      break;
    default:
      if (ks>=0x20 && ks<0x80) {
        if (ctrl_key_pressed==XK_Control_L || ctrl_key_pressed==XK_Control_R) {
          if (ks=='u') {
            lin->reset();
            cursor=0; 
            print_line();
          }
        }
        else {
          lin->insert_char(ks,cursor);
          if (lin->xpos(cursor)>dx-10) ++lin->leftside;
          print_line();
        }
      }
  }
  return 0;
}

EditWin::EditWin(int pw,Rect rect,uint r_m,bool edble,void(*_cmd)(Id,int ctrl_key,int key),Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    linenr(-1),
    lmax(100),
    y_off(0),
    editable(edble),
    lines(new Line*[lmax]),
    cmd(_cmd) {
  ed_winb.next_win()=this;
  cursor.y=cursor.x=-1;
  for (int i=0;i<lmax;++i) lines[i]=0;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,cWhite);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|PropertyChangeMask);
  //if (editable) XDefineCursor(dis,win,cursor_xterm);
  XMapWindow(dis, win);
}
void EditWin::print_line(int vpos) {
  int y1=vpos*TDIST-y_off;
  Line *lin=lines[vpos];
  if (y1<-TDIST || y1>dy) return;
  XClearArea(dis,win,0,y1,dx,TDIST+1,false);
  if (editable && cursor.y==vpos && cursor.x>=0) {
    int x1;
    if (lin) {
      if (lin->xpos(cursor.x)>=dx-10) lin->leftside=cursor.x - (dx-10)/char_wid;
      x1=lin->xpos(cursor.x);
    }
    else
      x1=2;
    draw_line(win,2,cBlack,Point(x1,y1+1),Point(x1,y1+13));
  }
  if (!lin || !lin->dlen) return;
  XftDrawString8(xft_win,xft_Black,xft_mono_font,2,y1+10,(XftChar8*)(lin->data+lin->leftside),lin->dlen-lin->leftside);
}

void EditWin::set_y_off(int yoff) {
  if (abs(yoff-y_off)<TDIST && yoff>TDIST) return;
  int i,
      lst_y=linenr*TDIST-yoff+10;
  y_off=yoff;
  XClearArea(dis,win,0,lst_y,dx,dy-lst_y,false);
  for (i=0;i<=linenr;++i)
    print_line(i);
}
void EditWin::set_cursor(int x1,int y1) {
  if (!editable) return;
  if (the_cursor.diawd) {
    the_cursor.diawd->unset_cursor(); the_cursor.diawd=0;
  }
  else if (the_cursor.edwd && (the_cursor.edwd!=this || the_cursor.edwd->y!=y1)) {
    the_cursor.edwd->unset_cursor();
  }
  the_cursor.edwd=this;
  if (cursor.y>=0)   // print line without cursor
    print_line(cursor.y);
  if (linenr<0) y1=linenr=0;
  cursor.y=(y1+y_off)/TDIST;
  if (cursor.y>linenr) {
    ++linenr;
    cursor.y=linenr;
  }
  Line *lin=lines[cursor.y];
  if (!lin)
    cursor.x=0;
  else 
    cursor.x=min(lin->dlen,x1/char_wid);
  print_line(cursor.y);
}
void EditWin::unset_cursor() {
  if (editable && cursor.x>=0) {
    cursor.x=-1;
    if (cursor.y>=0) {
      Line *lin=lines[cursor.y];
      if (lin) lin->leftside=0;
      print_line(cursor.y);
    }
  }
}
void EditWin::read_file(FILE* in) {
  int len;
  char textbuf[1000];
  Line *lin;
  y_off=0;
  reset();
  for (;;) {
    if (!fgets(textbuf,1000-1,in)) break;
    len=strlen(textbuf)-1;
    textbuf[len]=0; // strip '\n'
    ++linenr;
    if (linenr>=lmax) lines=re_alloc<Line*>(lines,lmax,0);
    if (!lines[linenr]) lines[linenr]=new Line;
    lin=lines[linenr];
    lin->cpy(textbuf);
    lin->dlen=len;
    if (x_running) print_line(linenr);
  }
}
void EditWin::set_line(char *s,int n) {
  if (n>=lmax-1) lines=re_alloc<Line*>(lines,lmax,0);
  if (n>linenr) linenr=n;
  if (!lines[n]) lines[n]=new Line;
  lines[n]->cpy(s);
  print_line(n);
}
void EditWin::insert_line(const char *s,int n) {
  int i;
  if (the_cursor.edwd==this && cursor.y>=n) ++cursor.y;
  if (n>linenr)
    linenr=n;
  else {
    ++linenr;
    for (i=linenr+1;i>n;--i) {
      lines[i]=lines[i-1];
      print_line(i);
    }
  }
  if (linenr>=lmax-1) lines=re_alloc<Line*>(lines,lmax,0);
  (lines[n]=new Line)->cpy(s);
  print_line(n);
}
void EditWin::write_file(FILE *out) {
  for (int i=0;i<=linenr;++i) {
    Line *lin=lines[i];
    if (lin)
      fputs(lin->data,out);
    putc('\n',out);
  }
}
char* EditWin::get_text(char *buf,int maxlen) {
  char *p=buf;
  int ln;
  for (ln=0;ln<=linenr;++ln) {
    if (lines[ln]) {
      if (p-buf >= maxlen-lines[ln]->dlen) {
        alert("get_text: buffer too small: %d",maxlen); 
        break;
      }
      p=stpncpy(p,lines[ln]->data,lines[ln]->dlen);
    }
    *p='\n';
    if (p-buf<maxlen) ++p;
    else { alert("get_text: buffer too small"); break; }
  }
  *p=0;
  return buf;
}
void EditWin::reset() { // y_off is kept
  for (int i=0;i<=linenr;++i)
    if (lines[i]) { lines[i]->data[0]=0; lines[i]->dlen=0; }
  linenr=-1;
  XClearWindow(dis,win);
}

bool TopWin::handle_key(XKeyEvent *ev) {
  KeySym ks;
  char buf[10];
  buf[0]=0;
  XLookupString(ev, buf, 10, &ks, 0);
  switch (ks) {
    case XK_Control_L:
    case XK_Control_R:
      ctrl_key_pressed=ks;
      break;
    default:
      key_pressed=ks;
  }
  if (the_cursor.diawd)
    return the_cursor.diawd->handle_key(ks);
  if (the_cursor.edwd) {
    the_cursor.edwd->handle_key(ks);
    return false;
  }
  return true;
}

void TopWin::handle_key_release(XKeyEvent *ev) {
  KeySym ks;
  char buf[10];
  buf[0]=0;
  XLookupString(ev, buf, 10, &ks, 0);
  switch (ks) {
    case XK_Control_L:
    case XK_Control_R:
      ctrl_key_pressed=0;
      break;
    default:
      key_pressed=0;
  }
}

void DialogWin::set_cursor(int x1) {
  cursor=min(lin->dlen,x1/char_wid);
  if (the_cursor.edwd) { the_cursor.edwd->unset_cursor(); the_cursor.edwd=0; }
  the_cursor.diawd=this;
  print_line();
}

bool DialogWin::set_menu(int x1) {
  if (dx-x1 < 14) { // at triangle?
    fill_menu(cmd_id);
    return true;
  }
  return false;
}

void DialogWin::next() {
  if (butnr==mb_max-1) {
    ChMButton* new_but=new ChMButton[mb_max*2];
    for (int i=0;i<mb_max;++i) new_but[i]=but[i];
    delete[] but;
    but=new_but;
    mb_max *= 2;
  }
  ++butnr;
}

void DialogWin::add_mbut(const char* lab) {  // label is allocated!
  next();
  ChMButton *mb=but+butnr;
  if (alloc_lab) {
    mb->label=new char[strlen(lab)+1]; strcpy(const_cast<char*>(mb->label),lab);
  }
  else mb->label=lab;
}

void EditWin::handle_key(KeySym ks1) {
  if (!editable) return;
  if (cursor.y<0) return;
  int i;
  Line *lin=lines[cursor.y],
       *lin2;

  switch (ks1) {
    case XK_Return:
      if (cmd) cmd(id,0,ks1);
      ++linenr;
      if (linenr>=lmax) lines=re_alloc<Line*>(lines,lmax,0);
      for (i=linenr;i>cursor.y+1;--i) {
        lines[i]=lines[i-1];
        print_line(i);
      }
      lin=lines[cursor.y];
      ++cursor.y;
      if (lin) {
        if (lin->data[cursor.x])
          (lines[cursor.y]=new Line)->cpy(lin->data+cursor.x);
        else
          lines[cursor.y]=0;
        lin->dlen=cursor.x;
        lin->data[lin->dlen]=0;
        lin->leftside=0;
      }
      cursor.x=0;
      print_line(cursor.y-1);
      print_line(cursor.y);
      break;
    case XK_Control_L:
      break;
    case XK_BackSpace:
      if (cmd) cmd(id,0,ks1);
      if (cursor.x==0) {
        if (cursor.y==0) return;
        if (lines[cursor.y-1]) {
          lin=lines[cursor.y-1];
          cursor.x=lin->dlen;
          lin2=lines[cursor.y];
          if (lin2) {
            while (lin->dlen+lin2->dlen>=lin->dmax-1)
              lin->data=re_alloc<char>(lin->data,lin->dmax,0);
            lin->cat(lin2->data);
          }
        }
        else {
          lines[cursor.y-1]=lines[cursor.y];
          cursor.x=0;
        }
        --cursor.y;
        print_line(cursor.y);

        for (i=cursor.y+1;i<linenr;++i) {
          lines[i]=lines[i+1];
          print_line(i);
        }
        lines[linenr]=0;
        print_line(linenr);
        --linenr;
      }
      else {
        lin->rm_char(cursor.x);
        if (lin->xpos(cursor.x)<10 && lin->leftside>0) --lin->leftside;
        print_line(cursor.y);
      }
      break;
    case XK_Left:
      if (cursor.x>0) {
        --cursor.x;
        if (lin->xpos(cursor.x)<10 && lin->leftside>0) --lin->leftside;
        print_line(cursor.y);
      }
      break;
    case XK_Right:
      if (cursor.x<lines[cursor.y]->dlen) {
        ++cursor.x;
        print_line(cursor.y);
      }
      break;
    case XK_Up:
      if (cursor.y>0) {
        --cursor.y;
        lin=lines[cursor.y];
        if (lin) {
          if (cursor.x>lin->dlen) cursor.x=lin->dlen;
        }
        else cursor.x=0;
        print_line(cursor.y);
        lin=lines[cursor.y+1];
        if (lin) lin->leftside=0;
        print_line(cursor.y+1);
      }
      break;
    case XK_Down:
      if (cursor.y<linenr) {
        ++cursor.y;
        lin=lines[cursor.y];
        if (lin) {
          if (cursor.x>lin->dlen) cursor.x=lin->dlen;
        }
        else cursor.x=0;
        print_line(cursor.y);
        lin=lines[cursor.y-1];
        if (lin) lin->leftside=0;
        print_line(cursor.y-1);
      }
      break;
    default:
      if (cmd) cmd(id,ctrl_key_pressed,ks1);
      if (ks1>=0x20 && ks1<0x80) {
        if (ctrl_key_pressed==XK_Control_L || ctrl_key_pressed==XK_Control_R) {
          if (ks1=='u') {
            if (lin) lin->reset();
            cursor.x=0;
            print_line(cursor.y);
          }
        }
        else {
          if (!lin) lin=lines[cursor.y]=new Line;
          lin->insert_char(ks1,cursor.x);
          if (lin->xpos(cursor.x)>dx-10) ++lin->leftside;
          print_line(cursor.y);
        }
     }
  }
}

void EditWin::insert_string(uchar *s) {
  if (!editable) return;
  if (cursor.y<0) return;
  Line *lin;
  if (!(lin=lines[cursor.y])) lin=lines[cursor.y]=new Line;
  for (;*s;++s) {
    if (*s=='\n') {
      handle_key(XK_Return);
      if (!(lin=lines[cursor.y])) lin=lines[cursor.y]=new Line;
    }
    else if (*s>=0x20 && *s<0x80)
      lin->insert_char(*s,cursor.x);
  }
  print_line(cursor.y);
}

BgrWin::BgrWin(uint pw,
               Rect rect,
               uint r_m,
               void (*_display_cmd)(Id id),
               void (*_down_cmd)(Id id,int,int,int),
               void (*_moved_cmd)(Id id,int,int,int),
               void (*_up_cmd)(Id id,int,int,int),
               uint wcol,
               Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    wincol(wcol),
    display_cmd(_display_cmd),
    down_cmd(_down_cmd),
    moved_cmd(_moved_cmd),
    up_cmd(_up_cmd),
    is_graphw(true),
    surface(0) {
  gr_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,wincol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|
                       ButtonPressMask|
                       Button1MotionMask|  // for drawing
                       ButtonReleaseMask);
  XMapWindow(dis, win);
}

BgrWin::BgrWin(uint pw,
               Rect rect,
               uint r_m,
               void (*dcmd)(Id id),
               uint wcol,
               int bwid,
               Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    wincol(wcol),
    display_cmd(dcmd),
    down_cmd(0),
    moved_cmd(0),
    up_cmd(0),
    is_graphw(false),
    surface(0) {
  bg_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,bwid,cBorder,wincol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|
                       ButtonPressMask);
  XMapWindow(dis, win);
}

Button::Button(int pw,Rect rect,uint r_m,Label lab,void (*_cmd)(Id),Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width?rect.width:16,rect.height?rect.height:16,r_m,_id),
    is_down(0),
    xft_text_col(xft_Black),
    style(button_style),
    label(lab),
    cmd(_cmd) {
  b_winb.next_win()=this;
  switch (style.st) {
    case 0:
      win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,cWhite);
      xft_win=XftDrawCreate(dis,win,vis,cmap);
      break;
    case 1: {
      int wid=18;
      if (lab.txt) wid+=xft_text_width(lab.txt);
      win=XCreateSimpleWindow(dis,pwin,x,y,wid,dy,0,0,style.bgcol);
      xft_win=XftDrawCreate(dis,win,vis,cmap);
    }
  }
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|ButtonReleaseMask);
  XMapWindow(dis, win);
}

static void draw_gradient_rect(uint win,Rect rect,Color5 col,bool hollow=false) {
  int y[6]={ 0,rect.height/5,rect.height*2/5,rect.height*3/5,rect.height*4/5,rect.height };
  for (int i=0;i<5;++i) {
    if (hollow) set_color(col.c[4-i]);
    else set_color(col.c[i]);
    XFillRectangle(dis,win,def_gc,rect.x,rect.y+y[i],rect.width,y[i+1]-y[i]);
  }
  set_width_color(1,cBorder);
  XDrawRectangle(dis,win,def_gc,rect.x,rect.y,rect.width-1,rect.height-1);
}
/*
static void draw_rect3d_2(uint win,Rect rect,Color3 col,bool up) {
  set_color(col.b);
  XFillRectangle(dis,win,def_gc,rect.x+2,rect.y+2,rect.width-4,rect.height-4);
  set_width_color(1,cBorder);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x+rect.width, rect.y);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x, rect.y+rect.height);
  set_color(up ? cWhite : col.c);
  XDrawLine(dis, win, def_gc, rect.x+1, rect.y+1, rect.x+rect.width-2, rect.y+1);
  XDrawLine(dis, win, def_gc, rect.x+1, rect.y+1, rect.x+1, rect.y+rect.height-2);
  set_color(up ? col.c : col.a);
  XDrawLine(dis, win, def_gc, rect.x, rect.y+rect.height-1, rect.x+rect.width, rect.y+rect.height-1);
  XDrawLine(dis, win, def_gc, rect.x+1, rect.y+rect.height-2, rect.x+rect.width-2, rect.y+rect.height-2);
  XDrawLine(dis, win, def_gc, rect.x+rect.width-1, rect.y, rect.x+rect.width-1, rect.y+rect.height);
  XDrawLine(dis, win, def_gc, rect.x+rect.width-2, rect.y+1, rect.x+rect.width-2, rect.y+rect.height);
}
*/
/*
static void draw_hslknob(uint win,Point pt) {
  static const char *hsl_knob[]={
  "6 11 4 1",
  "b c #c00000",
  "a c #ff0000",
  "# c #ffc0c0",
  ": c #ffd0d0",
  "::::::",
  "#::::a",
  "##::aa",
  "###aaa",
  "###aaa",
  "###aaa",
  "###aaa",
  "###aaa",
  "##bbaa",
  "#bbbba",
  "bbbbbb"};

  static uint pm=create_pixmap(hsl_knob).pm;
  XCopyArea(dis,pm,win,def_gc,0,0,6,11,pt.x,pt.y);
}
*/
static void draw_rect3d_1(uint win,Rect rect,uint col,int lwid,bool up) {
  set_color(col);
  XFillRectangle(dis,win,def_gc,rect.x,rect.y,rect.width,rect.height);
  set_width_color(lwid,up ? cWhite : cBorder);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x+rect.width, rect.y);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x, rect.y+rect.height);
  set_color(up ? cBorder : cWhite);
  XDrawLine(dis, win, def_gc, rect.x, rect.y+rect.height-1, rect.x+rect.width, rect.y+rect.height-1);
  XDrawLine(dis, win, def_gc, rect.x+rect.width-1, rect.y, rect.x+rect.width-1, rect.y+rect.height);
}

void BgrWin::raise(uint light_col,uint dark_col) {
  set_width_color(1,light_col);
  XDrawLine(dis, win, def_gc, 0, 0, dx, 0);
  XDrawLine(dis, win, def_gc, 0, 0, 0, dy);
  set_color(dark_col);
  XDrawLine(dis, win, def_gc, 0, dy-1, dx, dy-1);
  XDrawLine(dis, win, def_gc, dx-1, 0, dx-1, dy);
}
/*
static void draw_hsl_knob(uint win,Point pt,Color3 col) {
  set_color(col.a);
  XFillRectangle(dis,win,def_gc,pt.x,pt.y,3,11);
  set_color(col.b);
  XFillRectangle(dis,win,def_gc,pt.x+3,pt.y,3,11);
  set_width_color(1,cGrey);
  XDrawRectangle(dis,win,def_gc,pt.x-1,pt.y,6,11);
}

static void draw_arrowdown3d_1(uint win,Rect rect,Color3 col) {
  set_color(col.b);
  Point points[3]={ Point(rect.x,rect.y),Point(rect.x+rect.width,rect.y),Point(rect.x+rect.width/2,rect.y+rect.height) };
  XFillPolygon(dis,win,def_gc,(XPoint*)points,3,Convex,CoordModeOrigin);
  set_width_color(1,col.a);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x+rect.width, rect.y);
  XDrawLine(dis, win, def_gc, rect.x, rect.y, rect.x+rect.width/2-1, rect.y+rect.height);
  set_color(col.c);
  XDrawLine(dis, win, def_gc, rect.x+rect.width, rect.y, rect.x+rect.width/2, rect.y+rect.height);
}
*/
void Button::draw_button() {
  XClearWindow(dis,win); // needed if style = 1
  Color5 bcol=
    style.param==1 ? cGradientGrey :
    style.param==2 ? cGradientGreen :
    style.param==3 ? cGradientWheat :
    cGradientBlue;
  switch (style.st) {
    case 0:
      if (is_down)
        draw_rect3d_1(win,Rect(0,0,dx,dy),bcol.c[2],1,false);
      else
        draw_gradient_rect(win,Rect(0,0,dx,dy),bcol);
      if (label.txt) xft_draw_string(xft_win,xft_text_col,Point(3,dy-4),label.txt);
      if (label.draw) label.draw(win,xft_win,id,0,0);
      break;
    case 1:
      if (is_down)
        draw_rect3d_1(win,Rect(1,1,dy-2,dy-2),bcol.c[2],1,false);
      else
        draw_gradient_rect(win,Rect(1,1,dy-2,dy-2),bcol);
      if (label.draw) label.draw(win,xft_win,id,0,0);
      if (label.txt) xft_draw_string(xft_win,xft_Black,Point(dy+1,dy-4),label.txt);
      break;
   }
}
//void Button::command(Id _id) { if (cmd) cmd(_id); }

CheckBox::CheckBox(int pw,Rect rect,uint r_m,uint bgcol,const char* lab,void (*_cmd)(Id,bool),Id _id):
    WinBase(pw,0,
            rect.x,rect.y,18+xft_text_width(lab),rect.height?rect.height:16,
            r_m,_id),
    value(false),
    label(lab),
    style(checkbox_style),
    cmd(_cmd) {
  chb_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask);
  XMapWindow(dis, win);
}
void CheckBox::draw() {
  XClearWindow(dis,win);
  switch (style.st) {
    case 0: {
      uint col= value ? cRed : cWhite;
      draw_line(win,2,col,Point(2,4),Point(5,10));
      draw_line(win,2,col,Point(5,10),Point(10,1));
      if (label) xft_draw_string(xft_win,xft_Black,Point(14,dy-3),label);         
    }
    break;
    case 1:
      draw_rect3d_1(win,Rect(2,2,dy-3,dy-3),cCheckBox,2,false); // equal to Button, style 1
      if (value) {
        draw_line(win,2,cBlack,Point(4,6),Point(7,10));
        draw_line(win,2,cBlack,Point(7,10),Point(10,4));
      }
      if (label) xft_draw_string(xft_win,xft_Black,Point(dy+2,dy-4),label);
  }
}

RButton::RButton(int nr1,Label lab):
  nr(nr1),
  label(lab),
  xft_text_col(xft_Black) {
}

ChMButton::ChMButton():nr(-1),xft_col(xft_Black),label(0) { }

void ChMenu::cp_value(int nr,const char* label) {
  value.nr=nr;
  value.xft_col=xft_Black;
  if (label) {
    if (value.label) {
      if (strcmp(value.label,label)) {
        free(const_cast<char*>(value.label));
        value.label=strdup(label);
      }
    }
    else value.label=strdup(label);
  }
  else value.label=0;
}

RButWinData::RButWinData():
    butnr(-1),
    rb_max(20),
    but(new RButton*[rb_max]) {
}

RButWin::RButWin(WinBase *parent,Rect rect,uint r_m,const char *t,bool mbz,
                 void (*cmd)(Id id,int,int),uint bg,Id _id):
    WinBase(parent->win,t,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    y_off(0),
    maybe_z(mbz),
    rb_cmd(cmd),
    act_button(0),prev_abut(0),
    buttons(&def_buttons) {
  rb_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,cButBground);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask);
  XMapWindow(dis,win);
  if (!buttons) buttons=new RButWinData();
  if (title) { parent->add_child(this); t_rect.set(x+2,y-12,xft_text_width(title,xft_bold_font),12); }
}
void WinBase::add_to(WinBase *other) {
  other->add_child(this);
}
int RButWinData::next() {
  if (butnr==rb_max-1) but=re_alloc<RButton*>(but,rb_max,0);
  return ++butnr;
}
RButton* RButWin::is_in_rbutton(XButtonEvent *ev) {
  int nr=(ev->y + y_off)/TDIST;
  if (nr>buttons->butnr) return 0;
  return buttons->but[nr];
}
void RButWin::draw_rbutton(RButton *rb) {
  int y1=rb->nr*TDIST-y_off;
  if (rb==act_button) XSetForeground(dis, def_gc, cSelRBut);
  else XSetForeground(dis, def_gc, cButBground);
  XFillRectangle(dis,win,def_gc,1,y1,dx-2,TDIST);
  if (rb->label.txt)
    xft_draw_string(xft_win,rb->xft_text_col,Point(4,(rb->nr+1)*TDIST-3-y_off),rb->label.txt);
  else
    rb->label.draw(win,xft_win,id,rb->nr,y_off); // is empty
}
int RButWin::act_rbutnr() {
  if (!act_button) return -1;
  for (int i=0;i<=buttons->butnr;++i)
    if (buttons->but[i]==act_button) return i;
  alert("act_rbutnr: act button unknown");
  return 0;
}
void RButWin::empty() {
  for (int n=0;n<=buttons->butnr;++n) delete buttons->but[n];
  buttons->butnr=-1; y_off=0;
  act_button=prev_abut=0;
  XClearWindow(dis,win);
}
void RButWin::no_actb() {
  act_button=0;
  draw_actb();
}
void RButWin::set_rbut(RButton *rb,int fire) {
  act_button=rb;
  if (fire && act_button->cmd) act_button->cmd(id,act_button->nr,fire);
  draw_actb();
}
void RButWin::set_rbutnr(int nr,int fire,bool do_draw) {
  if (nr<0 || nr>buttons->butnr)
    alert("set_rbutnr: bad index %d",nr);
  else {
    act_button=buttons->but[nr];
    if (do_draw) draw_actb();
    if (fire && act_button->cmd) act_button->cmd(id,act_button->nr,fire);
  }
}
RButton* RButWin::nr2rb(int nr) {
  if (nr<0 || nr>buttons->butnr) {
    alert("nr2rb: bad index %d",nr);
    return 0;
  }
  return buttons->but[nr];
}

void RButWin::draw_actb() {
  if (prev_abut && prev_abut!=act_button) draw_rbutton(prev_abut);
  prev_abut=act_button;
  if (act_button) draw_rbutton(act_button);
}
void RButWin::del_rbut(RButton *rb) {
  int i;
  if (act_button==rb) act_button=prev_abut=0;
  for (i=0;;++i) {
    if (buttons->but[i]==rb) break;
    if (i==buttons->butnr) { alert("del_rbut: rbutton unknown"); return; }
  }
  XSetForeground(dis,def_gc,cButBground);
  XFillRectangle(dis,win,def_gc,0,buttons->but[buttons->butnr]->nr*TDIST-y_off,dx,TDIST);
  for (;i<buttons->butnr;++i) {
    buttons->but[i]=buttons->but[i+1];
    --buttons->but[i]->nr;
    draw_rbutton(buttons->but[i]);
  }
  --buttons->butnr;
  delete rb;
}
void RButWin::mv_resize(uint w,int ddx,int ddy) {
  if (w!=pwin) return;
  WinBase::mv_resize(w,ddx,ddy);
}

ChMenuData::ChMenuData():
    butnr(-1),
    mb_max(20),
    but(new ChMButton[mb_max]) {
}

void ChMenuData::next() {
  if (butnr==mb_max-1) {
    ChMButton* new_but=new ChMButton[mb_max*2];
    for (int i=0;i<mb_max;++i) new_but[i]=but[i];
    delete[] but;
    but=new_but;
    mb_max *= 2;
  }
  ++butnr;
}

ChMenu::ChMenu(int pw,Rect rect,uint r_m,void (*fill)(Id),void (*_cmd)(Id,ChMButton*),
           const char *t,Style st,uint bgcol,Id _id):
    WinBase(pw,t,rect.x,rect.y-TDIST,rect.width,2*TDIST+2,r_m,_id),
    style(st),
    mbut_win(0),
    xft_mbut_win(0),
    fill_menu(fill),
    cmd(_cmd),
    mbuttons(&mdata) {
  value.label=0;
  chmenu_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask);
  XMapWindow(dis,win);
}

CmdMenu::CmdMenu(int pw,Rect rect,uint r_m,void (*fill)(),const char *t,uint bgcol):
    WinBase(pw,t,rect.x,rect.y,
            rect.width ? rect.width : xft_text_width(t,xft_bold_font)+2,
            TDIST,r_m,Id(0,0)),
    butnr(-1),
    mb_max(20),
    mbut_win(0),
    xft_mbut_win(0),
    mbuttons(new CmdMButton[mb_max]),
    act_mbut(0),
    fill_menu(fill) {
  cmdmenu_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,
    ExposureMask|
    EnterWindowMask|LeaveWindowMask|
    ButtonPressMask|ButtonReleaseMask);
  XMapWindow(dis,win);
}

CmdMButton::CmdMButton():id(0,0) { }

void CmdMenu::add_mbut(const char* label,void (*cmd)(Id),Id _id) {
  if (butnr>=mb_max-1) { alert("menu buttons > %d",mb_max); return; }
  ++butnr;
  CmdMButton *mb=mbuttons+butnr;
  mb->label=label; mb->cmd=cmd; mb->id=_id;
}

void ChMenu::draw() {
  XClearWindow(dis,win);
  set_color(cWhite); XFillRectangle(dis,win,def_gc,0,TDIST,dx,dy-TDIST);
  if (value.label) xft_draw_string(xft_win,xft_Black,Point(4,dy-4),value.label);
  fill_rectangle(win,cGradientGrey.c[3],Rect(dx-14,TDIST,14,dy));
  const int x1=dx-7,y1=TDIST+10;
  fill_triangle(win,cBlack,Point(x1,y1),Point(x1-4,y1-6),Point(x1+4,y1-6));
  set_width_color(1,cBorder); XDrawRectangle(dis,win,def_gc,0,TDIST,dx-1,dy-TDIST-1);
  xft_draw_string(xft_win,xft_Black,Point(1,10),title,BOLDfont);
}

void CmdMenu::draw(bool hilight) {
  if (hilight) {
    XSetForeground(dis,def_gc,cSelCmdM);
    XFillRectangle(dis,win,def_gc,0,0,dx,dy);
    xft_draw_string(xft_win,xft_White,Point(1,dy-3),title,BOLDfont);
  }
  else { 
    XClearWindow(dis,win);
    xft_draw_string(xft_win,xft_Black,Point(1,dy-3),title,BOLDfont);
  }
}

void ChMenu::draw_mbutton(ChMButton *mb) {
  xft_draw_string(xft_mbut_win,mb->xft_col,Point(4,(mb->nr+1)*TDIST-3),mb->label);
}

void CmdMenu::draw_mbutton(CmdMButton *mb) {
  uint bcol=cMenuBground; // background
  XftColor *xft_tcol=xft_Black;   // text
  if (mb==act_mbut) { xft_tcol=xft_White; bcol=cSelCmdM; }
  int nr=mb-mbuttons;
  fill_rectangle(mbut_win,bcol,Rect(0,nr*MBDIST,mw_dx,MBDIST));
  xft_draw_string(xft_mbut_win,xft_tcol,Point(4,(nr+1)*MBDIST-4),mb->label);
}

void ChMenu::add_mbut(const char* lab,XftColor *col) {
  mbuttons->next();
  ChMButton *mb=mbuttons->but+mbuttons->butnr;
  mb->label=lab; mb->xft_col=col;
}

uint create_menu_win(uint pwin,int left,int top,int mdy,int *mw_x,int *mw_y,int mw_dx) {
  Window dum;
  XTranslateCoordinates(dis,pwin,top_wd->win,left,top,mw_x,mw_y,&dum);
  uint mbut_win=XCreateSimpleWindow(dis,top_wd->win,*mw_x,*mw_y,mw_dx,mdy,1,cBorder,cWhite);
  XSetTransientForHint(dis,mbut_win,top_wd->win); // on top
  return mbut_win;
}

void ChMenu::init_mwin() {   // to be called after fill_menu()
  if (mbuttons->butnr<0) return;
  mw_dx=0;
  for (int n=0;n<=mbuttons->butnr;++n) {
    mbuttons->but[n].nr=n;  // numbering done here
    const char *s=mbuttons->but[n].label;
    mw_dx=max(mw_dx,xft_text_width(s)+6);
  }
  int mdy=(mbuttons->butnr+1)*TDIST;
  int ypos= style.param==1 ? -mdy/2 : style.param==2 ? -mdy/3 : 0;
  switch (style.st) {
    case 1:  // place at left side
      mbut_win=create_menu_win(win,-mw_dx-3,ypos,mdy,&mw_x,&mw_y,mw_dx);
      break;
    case 2: // place underneath
      mbut_win=create_menu_win(win,0,dy+2,mdy,&mw_x,&mw_y,mw_dx);
      break;
    case 3:  // place at right side
      mbut_win=create_menu_win(win,dx+2,ypos,mdy,&mw_x,&mw_y,mw_dx);
      break;
    default:
      mbut_win=0;
  }
  xft_mbut_win=XftDrawCreate(dis,mbut_win,vis,cmap);
  XSelectInput(dis, mbut_win, ExposureMask|ButtonPressMask);
  XMapWindow(dis,mbut_win);
  for (int n=0;n<=mbuttons->butnr;++n)
    draw_mbutton(mbuttons->but+n);
}

void DialogWin::init_mwin() {   // to be called after fill_menu()
  if (butnr<0) return;
  mw_dx=0;
  for (int n=0;n<=butnr;++n) {
    but[n].nr=n;  // numbering done here
    const char *s=but[n].label;
    mw_dx=max(mw_dx,xft_text_width(s)+6);
  }
  int mdy=(butnr+1)*TDIST;
  mbut_win=create_menu_win(win,0,dy+2,mdy,&mw_x,&mw_y,mw_dx);  // underneath
  xft_mbut_win=XftDrawCreate(dis,mbut_win,vis,cmap);
  XSelectInput(dis, mbut_win, ExposureMask|ButtonPressMask);
  XMapWindow(dis,mbut_win);
  for (int n=0;n<=butnr;++n) {
    ChMButton *mb=but+n;
    xft_draw_string(xft_mbut_win,mb->xft_col,Point(4,(mb->nr+1)*TDIST-3),mb->label);
  }
}

void DialogWin::reset_mwin() {
  if (alloc_lab)
    for (int i=0;i<=butnr;++i) delete but[i].label;
  butnr=-1;
  XftDrawDestroy(xft_mbut_win); xft_mbut_win=0;
  delete_window(mbut_win);  // after XftDrawDestroy()
}

void CmdMenu::init_mwin() {   // to be called after fill_menu()
  if (butnr<0) return;
  mw_dx=dx;
  for (int n=0;n<=butnr;++n) {
    const char *s=mbuttons[n].label;
    mw_dx=max(mw_dx,xft_text_width(s)+5);
  }
  mbut_win=create_menu_win(win,0,dy+2,(butnr+1)*MBDIST,&mw_x,&mw_y,mw_dx);
  xft_mbut_win=XftDrawCreate(dis,mbut_win,vis,cmap);
  XSelectInput(dis, mbut_win,
    ExposureMask|
    LeaveWindowMask|
    Button1MotionMask|ButtonReleaseMask);
  XMapWindow(dis,mbut_win);
  for (int n=0;n<=butnr;++n)
    draw_mbutton(mbuttons+n);
}

bool in_a_cmbutton(XButtonEvent* ev,ChMenu*& menuw,ChMButton*& mb) {
  ChMenu *wd; 
  uint w=ev->window;
  for (int i=0;i<=chmenu_winb.win_nr;++i) {
    wd=chmenu_winb.wbuf[i];
    if (w==wd->mbut_win) {
      menuw=wd;
      mb=wd->mbuttons->but + ev->y/TDIST;
      return true;
    }
  }
  menuw=0; mb=0;
  return false;
}

bool in_a_diambutton(XButtonEvent* ev,DialogWin*& menuw,ChMButton*& mb) {
  DialogWin *wd; 
  uint w=ev->window;
  for (int i=0;i<=dia_winb.win_nr;++i) {
    wd=dia_winb.wbuf[i];
    if (w==wd->mbut_win) {
      menuw=wd;
      mb=wd->but + ev->y/TDIST;
      return true;
    }
  }
  menuw=0; mb=0;
  return false;
}

bool in_a_cmdmbutton(XButtonEvent* ev,CmdMenu*&menuw,CmdMButton*& mb) {
  CmdMenu *wd;
  uint w=ev->window;
  for (int i=0;i<=cmdmenu_winb.win_nr;++i) {
    wd=cmdmenu_winb.wbuf[i];
    if (w==wd->mbut_win) {
      menuw=wd;
      mb=wd->mbuttons + minmax(0,ev->y/MBDIST,wd->butnr);
      return true;
    }
  }
  menuw=0; mb=0;
  return false;
}

void ChMenu::reset() { // value.label is not reset
  mbuttons->butnr=-1;
  if (mbut_win) {
    XftDrawDestroy(xft_mbut_win); xft_mbut_win=0;
    delete_window(mbut_win);
  }
}

void CmdMenu::reset() {
  butnr=-1;
  act_mbut=0;
  draw();
  if (mbut_win) {
    XftDrawDestroy(xft_mbut_win); xft_mbut_win=0;
    delete_window(mbut_win);
  }
}

RExtButton::RExtButton(int pw,Rect rect,uint r_m,Label lab,Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height?rect.height:16,r_m,_id),
    style(ext_rbut_style),
    is_act(0),
    label(lab) {
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,style.bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask);
  XMapWindow(dis,win);
}
void RExtButton::draw_rxbut() {
  //XClearWindow(dis,win);
  switch (style.st) {
    case 0:
      set_width_color(1,cBorder);
      XDrawRectangle(dis,win,def_gc,0,0,dx-1,dy-1);
      set_color(is_act ? cRed : cForeground);
      XFillRectangle(dis,win,def_gc,1,1,8,dy-2);
      xft_draw_string(xft_win,xft_Black,Point(12,dy-5),label.txt);
      break;
    case 1:
      draw_gradient_rect(win,Rect(1,1,dx-2,dy-2),cGradientWheat,true);
      if (is_act) {
        set_width_color(2,cRed);
        XDrawRectangle(dis,win,def_gc,1,1,dx-2,dy-2);
      }
      else {
        set_width_color(1,cWhite);
        XDrawRectangle(dis,win,def_gc,0,0,dx-2,dy-2);
        set_color(cBorder);
        XDrawRectangle(dis,win,def_gc,1,1,dx-2,dy-2);
      }
      if (label.txt) {
        if (label.undersc>=0) {
          xft_draw_string(xft_win,xft_Black,Point(4,dy-5),label.txt);
          XGlyphInfo glyph;
          XftTextExtents8(dis,xft_def_font,(const FcChar8*)label.txt,label.undersc,&glyph);
          int ux=glyph.xOff+4;
          draw_line(win,1,cBlack,Point(ux,dy-4),Point(ux+6,dy-4));
        }
        else
          xft_draw_string(xft_win,xft_Black,Point(4,dy-4),label.txt);
      }
      else label.draw(win,xft_win,id,0,0);
  }
}

ExtRButCtrl::ExtRButCtrl(void(*cmd)(Id,bool)):
    butnr(-1),
    rxb_max(20),
    but(new RExtButton*[rxb_max]),
    act_lbut(0),
    reb_cmd(cmd) {
  rxb_buf.next_win()=this;
}

int ExtRButCtrl::next() {
  if (butnr==rxb_max-1) {
    RExtButton **new_but=new RExtButton*[rxb_max*2];
    for (int i=0;i<rxb_max;++i) new_but[i]=but[i];
    delete[] but;
    but=new_but;
    rxb_max *= 2;
  }
  return ++butnr;
}

void ExtRButCtrl::reset() {
  if (act_lbut) {
    act_lbut->is_act=0;
    act_lbut->draw_rxbut();
    act_lbut=0;
  }
}

void ExtRButCtrl::set_rbut(RExtButton* rb,bool fire) {
  int i;
  if (act_lbut) {
    act_lbut->is_act=0;
    act_lbut->draw_rxbut();
  }
  for (i=0;i<=butnr;++i) {
    if (rb==but[i]) {
      act_lbut=rb;
      rb->is_act=1;
      rb->draw_rxbut();
      if (fire && rb->cmd) rb->cmd(rb->id,1);
      return;
    }
  }
  alert("set_rbut: exp button not found");
}

SliderData::SliderData():value(0),old_val(0) { }

SliderData::SliderData(int val):value(val),old_val(val) { }

HSlider::HSlider(WinBase *parent,Rect rect,uint r_m,int minval,int maxval,
               const char* t,const char *ll,const char *lr,void (*_cmd)(Id id,int val,int fire,char*&,bool),uint bgcol,Id _id):
    WinBase(parent->win,t,rect.x,rect.y,rect.width,9+TDIST,r_m,_id),
    sdx(rect.width-8),
    minv(minval),maxv(max(minval+1,maxval)),
    def_data(minval),
    d(&def_data),
    lab_left(ll),
    lab_right(lr),
    text(0),
    style(slider_style),
    cmd(_cmd) {
  hsl_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);
  XMapWindow(dis, win);
  if (title) { parent->add_child(this); t_rect.set(x+2,y-12,xft_text_width(title,xft_bold_font),12); }
}
int &HSlider::value() { return d->value; }

void HSlider::draw_sliderbar(int val) {
  int i,x2,
      range=maxv-minv,
      xpos=4 + (val - minv) * sdx / range;
  switch (style.st) {
    case 0:
      set_color(cWhite); XFillRectangle(dis,win,def_gc,0,0,sdx+7,dy-1);
      set_width_color(1,cBorder); XDrawRectangle(dis,win,def_gc,0,0,sdx+7,dy-1);
      set_width_color(1,cGrey);
      XDrawLine(dis,win,def_gc,4,6,sdx+4,6);
      for (i=0;i<=range;++i) {
        x2=4 + sdx * i / range;
        XDrawLine(dis,win,def_gc,x2,4,x2,10);
      }
      break;
    case 1:
      draw_rect3d_1(win,Rect(4,3,sdx,8),cSlBackgr,1,false);
      set_width_color(1,cWhite);
      for (i=1;i<range;++i) {
        x2=4 + sdx * i / range;
        XDrawLine(dis,win,def_gc,x2,4,x2,9);
      }
      break;
  }
  draw_rect3d_1(win,Rect(xpos-3,1,6,11),cPointer,1,true);
  //draw_hsl_knob(win,Point(xpos-3,1),cPointer);
  //draw_arrowdown3d_1(win,Rect(xpos-4,1,8,8),cPointer);
}

void HSlider::draw() {
  XClearWindow(dis,win);
  draw_sliderbar(d->value);
  if (text)
    xft_draw_string(xft_win,xft_Blue,Point((sdx+8-xft_text_width(text,xft_small_font))/2,dy-2),text,SMALLfont);
  if (lab_left)
    xft_draw_string(xft_win,xft_Black,Point(1,dy-2),lab_left);
  if (lab_right) {
    int x1=sdx-xft_text_width(lab_right)+6;
    xft_draw_string(xft_win,xft_Black,Point(x1,dy-2),lab_right);
  }
}

void HSlider::set_hsval(int val,int fire,bool do_draw) {
  d->value=val;
  if (fire && cmd) cmd(id,d->value,fire,text,true);
  if (do_draw) draw();
}

void HSlider::calc_hslval(int x1) {
  d->value=minmax(minv,minv + divide((x1-4) * (maxv - minv),sdx),maxv);
}

VSlider::VSlider(WinBase *parent,Rect rect,uint r_m,int minval,int maxval,
                 const char* t,const char *lb,const char *lt,void (*_cmd)(Id id,int val,int fire,char*&,bool),uint bgcol,Id _id):
    WinBase(parent->win,t,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    sdy(dy-8),
    minv(minval),maxv(max(minval+1,maxval)),
    value(minval),old_val(minval),
    lab_top(lt),
    lab_bottom(lb),
    text(0),
    style(slider_style),
    cmd(_cmd) {
  vsl_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);
  XMapWindow(dis, win);
  if (title) { parent->add_child(this); t_rect.set(x+2,y-12,xft_text_width(title,xft_bold_font),12); }
}
void VSlider::draw_sliderbar(int val) {
  int i,y3,
      range=maxv-minv,
      ypos=2 + sdy - (val - minv) * sdy / range;
  switch (style.st) {
    case 0:
      set_color(cWhite); XFillRectangle(dis,win,def_gc,0,0,dx-1,dy-1);
      set_width_color(1,cBorder); XDrawRectangle(dis,win,def_gc,0,0,dx-1,dy-1);
      set_width_color(1,cGrey);
      XDrawLine(dis,win,def_gc,5,4,5,dy-4);
      for (i=0;i<=range;++i) {
        y3=3 + sdy - sdy * i / range;
        XDrawLine(dis,win,def_gc,2,y3,8,y3);
      }
      break;
    case 1:
      draw_rect3d_1(win,Rect(3,3,8,sdy),cSlBackgr,1,false);
      set_width_color(1,cWhite);
      for (i=1;i<range;++i) {
        y3=3 + sdy - sdy * i / range;
        XDrawLine(dis,win,def_gc,4,y3,8,y3);
      }
      break;
  }
  draw_rect3d_1(win,Rect(1,ypos,11,6),cPointer,1,true);
}
void VSlider::draw() {
  XClearWindow(dis,win);
  draw_sliderbar(value);
  if (text)
    xft_draw_string(xft_win,xft_Blue,Point(13,5+dy/2),text,SMALLfont);
  if (lab_top)
    xft_draw_string(xft_win,xft_Black,Point(12,11),lab_top);
  if (lab_bottom)
    xft_draw_string(xft_win,xft_Black,Point(12,dy-3),lab_bottom);
}

void VSlider::set_vsval(int val,bool fire,bool do_draw) {
  value=val;
  if (fire && cmd) cmd(id,value,fire,text,true);
  if (do_draw) draw();
}

void VSlider::calc_vslval(int y1) {
  value=minmax(minv,minv + divide((sdy+4-y1) * (maxv - minv),sdy),maxv);
}

HVSlider::HVSlider(WinBase *parent,Rect rect,int x_ins,uint r_m,
                Int2 minval,Int2 maxval,
                const char* t,const char *ll,const char *lr,const char *lb,const char *lt,
                void (*_cmd)(Id id,Int2 val,char*&,char*&,bool),uint bgcol,Id _id):
    WinBase(parent->win,t,rect.x,rect.y,rect.width,rect.height,r_m,_id),
    x_inset(x_ins),
    sdx(dx-4-x_inset),sdy(dy-5-TDIST),
    minv(minval),maxv(maxval),
    value(minval),old_val(minval),
    lab_left(ll),
    lab_right(lr),
    lab_top(lt),
    lab_bottom(lb),
    text_x(0),text_y(0),
    style(slider_style),
    cmd(_cmd) {
  hvsl_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,0,0,bgcol);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);
  XMapWindow(dis, win);
  if (title) { parent->add_child(this); t_rect.set(x+2,y-12,xft_text_width(title,xft_bold_font),12); }
}
void HVSlider::draw_2dim_slider(Int2 val) {
  Int2 range(maxv.x - minv.x,maxv.y - minv.y),
       pos(4 + (val.x - minv.x) * sdx / range.x,
           4 + sdy - (val.y - minv.y) * sdy / range.y);
  int i;
  switch (style.st) {
    case 0:
      set_color(cWhite); XFillRectangle(dis,win,def_gc,0,0,dx-1,dy-1);
      set_width_color(1,cBorder); XDrawRectangle(dis,win,def_gc,0,0,dx-1,dy-1);
      set_width_color(1,cGrey);
      for (i=0;i<=range.x;++i) {
        if (style.grid || i==0 || i==range.x) {
          int x1=sdx * i / range.x + 4;
          XDrawLine(dis,win,def_gc,x1,4,x1,4+sdy);
        }
      }
      for (i=0;i<=range.y;++i) {
        if (style.grid || i==0 || i==range.y) {
          int y1=4 + sdy - sdy * i / range.y;
          XDrawLine(dis,win,def_gc,4,y1,4+sdx,y1);
        }
      }
      break;
    case 1:
      draw_rect3d_1(win,Rect(4,4,sdx,sdy),cSlBackgr,1,false);
      set_width_color(1,cWhite);
      if (style.grid) {
        for (i=1;i<range.x;++i) {
          int x1=4 + sdx * i / range.x;
          XDrawLine(dis,win,def_gc,x1,sdy,x1,3+sdy);
        }
        for (i=1;i<range.y;++i) {
          int y1=4 + sdy - sdy * i / range.y;
          XDrawLine(dis,win,def_gc,sdx,y1,3+sdx,y1);
        }
        set_color(cWhite);
        if (val.x>minv.x && val.x<maxv.x) // vertical line
          XDrawLine(dis,win,def_gc,pos.x,5,pos.x,sdy+2);
        if (val.y>minv.y && val.y<maxv.y) // horizontal line
          XDrawLine(dis,win,def_gc,5,pos.y,sdx+2,pos.y);
      }
      break;
  }
  draw_rect3d_1(win,Rect(pos.x-4,pos.y-4,8,8),cPointer,1,true);
}

void HVSlider::draw() {
  XClearWindow(dis,win);
  draw_2dim_slider(value);
  if (text_x) {
    int x1=8+(sdx-xft_text_width(text_x))/2;
    xft_draw_string(xft_win,xft_Blue,Point(x1,dy-2),text_x,SMALLfont);
  }
  if (lab_left)
    xft_draw_string(xft_win,xft_Black,Point(1,dy-2),lab_left);
  if (lab_right) {
    int x1=4 + sdx - xft_text_width(lab_right)/2;
    xft_draw_string(xft_win,xft_Black,Point(x1,dy-2),lab_right);
  }
  if (text_y)
    xft_draw_string(xft_win,xft_Blue,Point(8+sdx,8+sdy/2),text_y,SMALLfont);
  if (lab_top)
    xft_draw_string(xft_win,xft_Black,Point(8+sdx,11),lab_top);
  if (lab_bottom)
    xft_draw_string(xft_win,xft_Black,Point(8+sdx,5+sdy),lab_bottom);
}

void HVSlider::set_hvsval(Int2 val,bool fire,bool do_draw) {
  value=val;
  if (fire && cmd) cmd(id,value,text_x,text_y,true);
  if (do_draw) draw();
}

void HVSlider::calc_hvslval(Int2 i2) {
  Int2 range(maxv.x - minv.x,maxv.y - minv.y),
       val1(minv.x + divide((i2.x-4) * range.x,sdx),
            minv.y + divide((sdy + 4 - i2.y) * range.y,sdy));
  value.set(minmax(minv.x,val1.x,maxv.x),minmax(minv.y,val1.y,maxv.y));
}

HScrollbar::HScrollbar(int pw,Rect rect,Style st,uint r_m,int r,void (*_cmd)(Id,int,int,bool),Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width,rect.height ? rect.height : 8,r_m,_id),
    p0(0),xpos(0),value(0),
    style(st),
    ssdim(st.st==1 ? 8 : 0),
    cmd(_cmd) {
  hsc_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,cWhite);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|Button1MotionMask|ButtonReleaseMask);
  XMapWindow(dis,win);
  calc_params(r);
}
void HScrollbar::calc_params(int r) {
  range=max(dx-2*ssdim,r-2*ssdim);
  wid=(dx-2*ssdim) * (dx-2*ssdim) / range;
}
void HScrollbar::draw_hscrollbar() {
  XClearWindow(dis,win);
  draw_rect3d_1(win,Rect(xpos+ssdim,0,wid,dy),cScrollbar,1,true);
  if (style.st==1) {
    draw_rect3d_1(win,Rect(0,0,ssdim,dy),cScrollbar1,1,true);
    draw_rect3d_1(win,Rect(dx-ssdim,0,ssdim,dy),cScrollbar1,1,true);
/*
    static char arrow_r[]= { 0x01,0x03,0x07,0x0f,0x0f,0x07,0x03,0x01 };
    static uint ar_r=get_pixmap(arrow_r,5,8);
    XCopyArea(dis,ar_r,win,def_gc,0,0,5,8,2,0); <-- no transparent background
*/ 
    fill_triangle(win,cBlack,Point(5,0),Point(5,7),Point(1,3));
    fill_triangle(win,cBlack,Point(dx-5,0),Point(dx-2,3),Point(dx-5,7));
  }
}
void HScrollbar::mv_resize(uint w,int ddx,int ddy) {
  if (w!=pwin) return;
  switch (rez_mode) {
    case FN: case MR:
      WinBase::mv_resize(w,ddx,ddy);
      break;
    case FR:
      dx += ddx;
      calc_params(range+ddx); // supposed: the scrolled widget is resized too
      XResizeWindow(dis,win,dx,dy);
      break;
  }
}
void HScrollbar::set_xpos(int newx) {
  int xp=minmax(0,newx*(dx-2*ssdim)/range,dx-2*ssdim-wid);
  if (xp!=xpos) {
    xpos=xp;
    value=xpos * range / (dx-2*ssdim);
    draw_hscrollbar();
    if (cmd) cmd(id,value,range,false);
  }
}
void HScrollbar::inc_value(bool incr) {
  int val=value,
      xp;
  if (incr) { 
    val+=style.param;
    xp=val*(dx-2*ssdim)/range;
    if (xp>dx-2*ssdim-wid) return;
  }
  else {
    val-=style.param;
    if (val<0) return;
    xp=val*(dx-2*ssdim)/range;
  }
  value=val;
  if (cmd) cmd(id,value,range,repeat.on);
  if (xp!=xpos) {
    xpos=xp;
    draw_hscrollbar();
  }
}

void HScrollbar::calc_xpos(int newx) {
  int xp=minmax(0,xpos + newx - p0,dx-2*ssdim-wid);
  p0=newx;
  if (xp!=xpos) {
    xpos=xp;
    value=xpos * range / (dx-2*ssdim);
    draw_hscrollbar();
    if (cmd) cmd(id,value,range,repeat.on);
  }
}

bool HScrollbar::in_ss_area(XButtonEvent *ev,bool *dir) {
  if (style.st==0) return false;
  if (ev->x >= dx - ssdim) { if (dir) *dir=true; return true; }
  if (ev->x <= ssdim) { if (dir) *dir=false; return true; }
  return false;
}

VScrollbar::VScrollbar(int pw,Rect rect,uint r_m,int r,void (*_cmd)(Id,int,int,bool),Id _id):
    WinBase(pw,0,rect.x,rect.y,rect.width ? rect.width : 8,rect.height,r_m,_id),
    p0(0),ypos(0),value(0),
    cmd(_cmd) {
  vsc_winb.next_win()=this;
  win=XCreateSimpleWindow(dis,pwin,x,y,dx,dy,1,cBorder,cWhite);
  xft_win=XftDrawCreate(dis,win,vis,cmap);
  XSelectInput(dis,win,ExposureMask|ButtonPressMask|Button1MotionMask);
  XMapWindow(dis,win);
  calc_params(r);
}
void VScrollbar::calc_params(int r) {
  range=max(dy,r);
  height=dy * dy / range;
}
void VScrollbar::draw_vscrollbar() {
  XSetForeground(dis,def_gc,cWhite);  // XClearWin: flicker
  XFillRectangle(dis,win,def_gc,0,0,dx,ypos);
  XFillRectangle(dis,win,def_gc,0,ypos+height,dx,dy-ypos-height);
  draw_rect3d_1(win,Rect(0,ypos,dx,height),cScrollbar,1,true);
}
void VScrollbar::set_ypos(int newy) {
  int yp=minmax(0,newy*dy/range,dy - height);
  if (yp!=ypos) {
    ypos=yp;
    value=ypos * range / dy;
    draw_vscrollbar();
    if (cmd) cmd(id,value,range,false);
  }
}

void VScrollbar::calc_ypos(int newy) {
  int yp=minmax(0,ypos + newy - p0,dy - height);
  p0=newy;
  if (yp!=ypos) {
    ypos=yp;
    value=ypos * range / dy;
    draw_vscrollbar();
    if (cmd) cmd(id,value,range,false);
  }
}
TextWin::~TextWin() { t_winb.unlist_widget(this); }

void TextWin::add_text(const char *s,XftColor *col) {
  ++linenr;
  xft_textcol[linenr % lmax]=col;
  strncpy(textbuf[linenr % lmax],s,SMAX);
}

void TextWin::print_text(const char *s,XftColor *col) {
  add_text(s,col);
  draw_text();
}

Button::~Button() { b_winb.unlist_widget(this); }

void WinBase::hide() { XUnmapWindow(dis,win); }
void WinBase::map() { XMapWindow(dis,win); }

bool WinBase::is_hidden() { 
  XWindowAttributes attr;
  XGetWindowAttributes(dis,win,&attr);
  return attr.map_state!=IsViewable;
}

RButton* RButWin::add_rbut(Label lab,XftColor *col) {
  RButton *rb;
  int nr=buttons->next();
  rb=buttons->but[nr]=new RButton(nr,lab);
  if (nr==0) act_button=prev_abut=rb;
  rb->cmd=rb_cmd;
  rb->xft_text_col=col;
  if (x_running) draw_rbutton(rb);
  return rb;
}

ExtRButCtrl::~ExtRButCtrl() {
  for (int i=0;i<=rxb_buf.win_nr;++i) delete but[i];
  delete[] but;
  rxb_buf.unlist_widget(this);
}

RExtButton *ExtRButCtrl::add_extrbut(uint pwin,Rect rect,uint r_m,Label lab,Id id) {
  RExtButton *rb;
  int nr=next();
  rb=but[nr]=new RExtButton(pwin,rect,r_m,lab,id);
  rb->cmd=reb_cmd;
  return rb;
}

void RButWin::re_label(RButton *rb,const char *lab) {
  rb->label=lab;
  draw_rbutton(rb);
}

void RButWin::set_y_off(int yoff) {
  if (abs(yoff-y_off)<10 && yoff>TDIST) return;
  y_off=yoff;
  XClearWindow(dis,win);
  for (int i=0;i<=buttons->butnr;++i)
    draw_rbutton(buttons->but[i]);
}

void EditWin::get_info(int* nr_lines,int* cursor_ypos,int *nr_chars) {
  if (nr_lines) *nr_lines=linenr;
  if (cursor_ypos)
    *cursor_ypos= the_cursor.edwd==this ? cursor.y : -1;
  if (nr_chars) {
    if (the_cursor.edwd==this)
      *nr_chars= lines[cursor.y] ? lines[cursor.y]->dlen : 0;
    else 
      *nr_chars=-1;
  }
}

bool in_a_rexpbutwin(Window win,RExtButton*& rxbw,ExtRButCtrl*& rxb_ctr) {
  int i,i2;
  ExtRButCtrl *dat;
  for (i=0;i<=rxb_buf.win_nr;++i) {
    dat=rxb_buf.wbuf[i];
    for (i2=0;i2<=dat->butnr;++i2) {
      if (win==dat->but[i2]->win) {
        rxbw=dat->but[i2];
        rxb_ctr=dat;
        return 1;
      }
    }
  }
  return 0;
}

void set_text(char *&txt,const char *val) {
  if (!txt) txt=new char[LBMAX];
  if (val) strncpy(txt,val,LBMAX);
  else txt[0]=0;
}
void set_text(char *&txt,const char *fs,int val) {
  if (!txt) txt=new char[LBMAX];
  snprintf(txt,LBMAX,fs,val);
}
void set_text(char*&txt,const char *fs,int val1,int val2) {
  if (!txt) txt=new char[LBMAX];
  snprintf(txt,LBMAX,fs,val1,val2);
}
void set_text(char *&txt,const char *fs,float val) {
  if (!txt) txt=new char[LBMAX];
  snprintf(txt,LBMAX,fs,val);
  for (char *p=txt+strlen(txt)-1;p>=txt;--p) { // strip zero's
    if (*p=='.') { *p=0; break; }
    if (*p=='0') *p=0; else break;
  }
}

void HScrollbar::set_range(int r) {
  calc_params(r);
  int xp=minmax(0,value*(dx-2*ssdim)/range,dx-2*ssdim-wid);
  p0-=xpos-xp;
  xpos=xp;
  draw_hscrollbar();
}

void VScrollbar::set_range(int r) {
  calc_params(r);
  int yp=minmax(0,value*dy/range,dy-height);
  p0-=ypos-yp;
  ypos=yp;
  draw_vscrollbar();
}

int io_handler(Display*) {
  puts("Goodbye!");
  exit(0);
}

void handle_event(XEvent *ev) {
  XButtonEvent *bp_ev=(XButtonEvent *)ev;
  static int the_button;
  if (ev->type==ButtonPress) the_button=bp_ev->button;
  if (the_button>Button3) // disenable mouse scrollwheel
    return;

  Button *b=0;
  BgrWin *grw=0;
  BgrWin *bgw=0;
  TextWin *textw=0;
  RButton *rb=0; // radio button
  ChMButton *chmb=0, // choice-menu button
            *dmb=0;  // dialog-menu button
  CmdMButton *cmdmb=0; // cmd-menu button
  RButWin *rbw=0;
  ChMenu *chmw=0;
  CmdMenu *cmdmw=0;
  RExtButton *rxbw=0;
  ExtRButCtrl *rxb_ctr=0;
  HSlider *hslw=0;
  VSlider *vslw=0;
  HVSlider *hvslw=0;
  HScrollbar *hscw=0;
  VScrollbar *vscw=0;
  EditWin *edw=0;
  DialogWin *diaw=0;
  CheckBox *chbw=0;
  SubWin *subw=0;
  TopWin *topw=0;
  int i;
  XAnyEvent *any_ev=(XAnyEvent*)ev;
  XExposeEvent *exp_ev=(XExposeEvent*)ev;
  Rect exp_rect(0,0,0,0); // for Expose events
  (top_wd->win==any_ev->window && (topw=top_wd)) ||
  dia_winb.in_a_win(any_ev->window,diaw) ||
  in_a_diambutton(bp_ev,diaw,dmb) ||
  gr_winb.in_a_win(any_ev->window,grw) ||
  ed_winb.in_a_win(any_ev->window,edw) ||
  subw_winb.in_a_win(any_ev->window,subw) ||
  bg_winb.in_a_win(any_ev->window,bgw) ||
  rb_winb.in_a_win(any_ev->window,rbw) ||
  chmenu_winb.in_a_win(any_ev->window,chmw) ||
  cmdmenu_winb.in_a_win(any_ev->window,cmdmw) ||
  in_a_cmbutton(bp_ev,chmw,chmb) ||
  in_a_cmdmbutton(bp_ev,cmdmw,cmdmb) ||
  in_a_rexpbutwin(any_ev->window,rxbw,rxb_ctr) ||
  hsl_winb.in_a_win(any_ev->window,hslw) ||
  vsl_winb.in_a_win(any_ev->window,vslw) ||
  hvsl_winb.in_a_win(any_ev->window,hvslw) ||
  hsc_winb.in_a_win(any_ev->window,hscw) ||
  vsc_winb.in_a_win(any_ev->window,vscw) ||
  chb_winb.in_a_win(any_ev->window,chbw) ||
  b_winb.in_a_win(any_ev->window,b) ||
  t_winb.in_a_win(any_ev->window,textw);

  switch (ev->type) {
    case ClientMessage: {
        XClientMessageEvent* clmes_ev=(XClientMessageEvent*)ev;
        if (clmes_ev->send_event &&
            static_cast<Atom>(clmes_ev->data.l[0]) == WM_DELETE_WINDOW) {
          if (subw) {
            if (subw->del_cmd) subw->del_cmd(subw->id);
          }
          else
            io_handler(dis);
        }
      }
      break; 
    case ButtonPress:
      if (b) {
        b->is_down=true;
        if (b->cmd) b->cmd(b->id);
        b->draw_button();
      }
      else if (grw) {
        if (grw->down_cmd)
          grw->down_cmd(grw->id,bp_ev->x,bp_ev->y,the_button); // button = 1, 2, 3
      }
      else if (rbw) {
        rb=rbw->is_in_rbutton(bp_ev);
        if (rb) {
          if (rb==rbw->act_button) {
            if (rbw->maybe_z) {
              rbw->act_button=0;
              rbw->draw_actb();
            }
          }
          else {
            rbw->act_button=rb;
            rbw->draw_actb();
          }
          if (rb->cmd) rb->cmd(rbw->id,rb->nr,1);
        }
      }
      else if (chmb) { // chmw is valid
        if (chmw->cmd) chmw->cmd(chmw->id,chmb);
        // menu window not reset
      }
      else if (chmw) {  // must be tested after chmb
        if (chmw->mbut_win) chmw->reset();
        else {
          if (chmw->fill_menu) {
            chmw->fill_menu(chmw->id);
            chmw->init_mwin();
          }
        }
      }
      else if (cmdmw) {
        cmdmw->fill_menu();
        cmdmw->init_mwin();
      }
      else if (rxbw) {
        if (rxbw->is_act) {
          rxbw->is_act=0;
          rxbw->draw_rxbut();
          rxb_ctr->act_lbut=0;
        }
        else {
          rxbw->is_act=1;
          if (rxb_ctr->act_lbut) {
            rxb_ctr->act_lbut->is_act=0;
            rxb_ctr->act_lbut->draw_rxbut();
          }
          rxb_ctr->act_lbut=rxbw;
          rxbw->draw_rxbut();
        }
        if (rxbw->cmd)
          rxbw->cmd(rxbw->id,rxbw->is_act);
      }
      else if (dmb) { // diaw is valid
        diaw->copy(dmb->label);
        diaw->reset_mwin();
      }
      else if (diaw) {
        if (the_button==Button1) {
          if (diaw->show_menu) {
            if (diaw->mbut_win) diaw->reset_mwin();
            else if (diaw->set_menu(bp_ev->x)) diaw->init_mwin();
            else diaw->set_cursor(bp_ev->x);
          }
          else
            diaw->set_cursor(bp_ev->x);
        }
        else if (the_button==Button2) {
          if (XGetSelectionOwner(dis,XA_PRIMARY) != None)
            XConvertSelection(dis,XA_PRIMARY,XA_STRING,CLIPBOARD,diaw->win,CurrentTime);
        }
      }
      else if (chbw) {
        chbw->value=!chbw->value;
        chbw->draw();
        if (chbw->cmd) chbw->cmd(chbw->id,chbw->value);
      }
      else if (edw) {
        if (the_button==Button1)
          edw->set_cursor(bp_ev->x,bp_ev->y);
        else if (the_button==Button2) {
          if (XGetSelectionOwner(dis,XA_PRIMARY) != None)
            XConvertSelection(dis,XA_PRIMARY,XA_STRING,CLIPBOARD,edw->win,CurrentTime);
        }
      }
      else if (hscw) {
        bool direction;
        if (hscw->in_ss_area(bp_ev,&direction)) {
          if (repeat.on)
            hscw->inc_value(direction);
          else {
            repeat.ev=*ev;
            repeat.on=true;
          }
        }
        else if (hscw->wid<=hscw->dx)  // maybe after resize
          hscw->p0=bp_ev->x;
      }
      else if (vscw) {
        if (vscw->height<=vscw->dy) // maybe after resize
          vscw->p0=bp_ev->y;
      }
      if (!edw && !diaw)
        the_cursor.unset(); // after possible actions, so application can issue a warning if needed
      if (grw || bgw || topw || subw)
        for (i=0;i<=chmenu_winb.win_nr;++i)
          chmenu_winb.wbuf[i]->reset();
      break;
    case MotionNotify:
      if (grw) {
        if (grw->moved_cmd)
          grw->moved_cmd(grw->id,bp_ev->x,bp_ev->y,the_button);
      }
      else if (hslw) {
        hslw->calc_hslval(bp_ev->x);
        if (hslw->d->old_val!=hslw->d->value) {
          hslw->d->old_val=hslw->d->value;
          if (hslw->cmd) hslw->cmd(hslw->id,hslw->d->value,1,hslw->text,false);
          hslw->draw();
        }
      }
      else if (vslw) {
        vslw->calc_vslval(bp_ev->y);
        if (vslw->old_val!=vslw->value) {
          vslw->old_val=vslw->value;
          if (vslw->cmd) vslw->cmd(vslw->id,vslw->value,1,vslw->text,false);
          vslw->draw();
        }
      }
      else if (hvslw) {
        hvslw->calc_hvslval(Int2(bp_ev->x,bp_ev->y));
        if (hvslw->old_val!=hvslw->value) {
          hvslw->old_val=hvslw->value;
          if (hvslw->cmd) hvslw->cmd(hvslw->id,hvslw->value,hvslw->text_x,hvslw->text_y,false);
          hvslw->draw();
        }
      }
      else if (hscw) {
        if (!repeat.on && hscw->wid<=hscw->dx)   // maybe after resize
          hscw->calc_xpos(bp_ev->x);
      }
      else if (vscw) {
        if (vscw->height<=vscw->dy) // maybe after resize
          vscw->calc_ypos(bp_ev->y);
      }
      else if (cmdmb) {  // cmdmw is valid
        if (cmdmw->act_mbut!=cmdmb) {
          CmdMButton *lst=cmdmw->act_mbut;
          cmdmw->act_mbut=cmdmb;
          if (lst) cmdmw->draw_mbutton(lst);
          cmdmw->draw_mbutton(cmdmb);
        }
      }
      break;
    case ButtonRelease:
      if (grw) {
        if (grw->up_cmd)
          grw->up_cmd(grw->id,bp_ev->x,bp_ev->y,the_button);
      }
      else if (b) {
        b->is_down=0;
        b->draw_button();
      }
      else if (hslw) {
        if (hslw->cmd) {
          hslw->cmd(hslw->id,hslw->d->value,1,hslw->text,true);
          if (hslw->text) hslw->draw();  // text might be modified
        }
      }
      else if (vslw) {
        if (vslw->cmd) {
          vslw->cmd(vslw->id,vslw->value,1,vslw->text,true);
          if (vslw->text) vslw->draw();
        }
      }
      else if (hvslw) {
        if (hvslw->cmd) {
          hvslw->cmd(hvslw->id,hvslw->value,hvslw->text_x,hvslw->text_y,true);
          if (hvslw->text_x || hvslw->text_y) hvslw->draw();
        }
      }
      else if (hscw) {
        repeat.on=false;
      }
      else if (cmdmb) { // cmdmw is valid
        cmdmb->cmd(cmdmb->id);
        cmdmw->reset();
      }
      else if (cmdmw)  // tested after cmdmb
        cmdmw->reset();
      break;
    case EnterNotify: {
        XCrossingEvent *cev=(XCrossingEvent*)ev;
        if (cev->mode!=NotifyNormal) break;  // else 'print screen' makes disappear the menu
        if (cmdmw)
          cmdmw->draw(true);
      }
      break;
    case LeaveNotify: { 
        XCrossingEvent *cev=(XCrossingEvent*)ev;
        if (cev->mode!=NotifyNormal) break;
        if (cmdmb)  // cmdmw is valid
          cmdmw->reset();
        else if (cmdmw) {
          if (cev->y<cmdmw->dy || cmdmw->mbut_win==0)
            cmdmw->reset();
          else
            XUngrabPointer(dis,CurrentTime); // else no MotionNotify event to menu window
        }
      }
      break;
    case KeyPress:
      if (top_wd->handle_key((XKeyEvent *)ev) &&  // handle edit windows, dialog window
          the_cursor.diawd)                      // dialog command?
        the_cursor.diawd->dok();
      break;
    case KeyRelease:
      top_wd->handle_key_release((XKeyEvent *)ev);
      break;
    case Expose:
      //if (exp_ev->count>0) break; <-- not useful
      exp_rect.set(exp_ev->x,exp_ev->y,exp_ev->width,exp_ev->height);
      if (topw) {
        if (topw->display_cmd) topw->display_cmd(topw->xft_win,exp_rect);
        // clear(topw->win,exp_rect); <-- doesn't help
        for (i=0;i<=topw->lst_child;++i)
          topw->children[i]->draw_title(topw->xft_win,exp_rect);
      }
      else if (subw) {
        if (subw->display_cmd) subw->display_cmd(exp_rect,subw->id);
        for (i=0;i<=subw->lst_child;++i)
          subw->children[i]->draw_title(subw->xft_win,exp_rect);
      }
      else if (rbw) {
        for (i=0;i<=rbw->buttons->butnr;++i) rbw->draw_rbutton(rbw->buttons->but[i]);
        rbw->prev_abut=rbw->act_button;
      }
      else if (chmb) { // chmw is valid
        clear(chmw->mbut_win);
        for (int n=0;n<=chmw->mbuttons->butnr;++n)
          chmw->draw_mbutton(chmw->mbuttons->but+n);
      }
      else if (chmw)
        chmw->draw();
      else if (cmdmw) // also an Expose event occurs at ButtonPress, because of Enter/LeaveNotify
        cmdmw->draw(cmdmw->mbut_win!=0);
      else if (hslw)
        hslw->draw();
      else if (vslw)
        vslw->draw();
      else if (hvslw)
        hvslw->draw();
      else if (hscw)
        hscw->draw_hscrollbar();
      else if (vscw)
        vscw->draw_vscrollbar();
      else if (diaw) {
        diaw->draw_dialabel();
        diaw->print_line();
      }
      else if (grw) {
        if (grw->display_cmd) grw->display_cmd(grw->id);
      }
      else if (bgw) {
        if (bgw->display_cmd) bgw->display_cmd(bgw->id);
        for (i=0;i<=bgw->lst_child;++i)
          bgw->children[i]->draw_title(bgw->xft_win,exp_rect);
      }
      else if (edw) {
        for (i=0;i<=edw->linenr;++i) edw->print_line(i);
      }
      else if (b)
        b->draw_button();
      else if (chbw)
        chbw->draw();
      else if (rxbw)
        rxbw->draw_rxbut();
      else if (textw)
        textw->draw_text();
      break;
    case ConfigureNotify: {
        XConfigureEvent *conf_ev=(XConfigureEvent*)ev;
        uint w=conf_ev->window;
        int diff_dx=conf_ev->width - top_wd->dx;
        if (diff_dx==0) break;
        top_wd->dx=conf_ev->width;
  
        for (i=0;i<=dia_winb.win_nr;++i) {
          dia_winb.wbuf[i]->mv_resize(w,diff_dx,0);
          dia_winb.wbuf[i]->draw_dialabel();
        }
        for (i=0;i<=b_winb.win_nr;++i)
          b_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=gr_winb.win_nr;++i)
          gr_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=ed_winb.win_nr;++i)
          ed_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=bg_winb.win_nr;++i)
          bg_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=hsc_winb.win_nr;++i)
          hsc_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=vsc_winb.win_nr;++i)
          vsc_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=rb_winb.win_nr;++i)
          rb_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=hsl_winb.win_nr;++i)
          hsl_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=vsl_winb.win_nr;++i)
          vsl_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=hvsl_winb.win_nr;++i)
          hvsl_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=chb_winb.win_nr;++i)
          chb_winb.wbuf[i]->mv_resize(w,diff_dx,0);
        for (i=0;i<=chmenu_winb.win_nr;++i) {
          chmw=chmenu_winb.wbuf[i];
          chmw->mv_resize(w,diff_dx,0);
        }
      }
      break;
    case PropertyNotify:
      if (ev->xproperty.state != PropertyNewValue) break;
      if (edw || diaw) {
        if (edw && !edw->editable) break;
        uint the_win=edw ? edw->win : diaw->win;
        int bytesread = 0;
        for (;;) {
          Atom actual; int format; unsigned long count, remaining;
          unsigned char* portion=0;
          if (XGetWindowProperty(dis,the_win,
                                 CLIPBOARD,
                                 bytesread/4,0x1000/4,1,AnyPropertyType,
                                 &actual, &format, &count, &remaining, &portion)) break;
          //printf("bytesread=%d portion=%p actual=%d format=%d count=%d\n",bytesread,portion,actual,format,count);
          if (portion) {
            if (edw) edw->insert_string(portion);
            else diaw->insert_string(portion);
            XFree(portion);
          }
          else break;
          if (!remaining) break;
          bytesread += count*format/8;
        }
      }
      break;
    case MapNotify:       // 19
    case ReparentNotify:  // 21
    case UnmapNotify:     // 18
    case SelectionNotify: // 31
/*
    case CreateNotify:    // 16
    case DestroyNotify:   // 17
*/
      break;
    default:
      printf("ev type = %d\n",ev->type);
  }
}

void sig_handler(int sig) {
  switch (sig) {
    case SIGINT: puts("Bye!"); exit(0);
    default: printf("signal: %d\n",sig);
  }
}

int X_sig_handler(Display*,XErrorEvent* ev) {
  char buf[100];
  XGetErrorText(dis,ev->error_code,buf,100);
  printf("X_sig_handler: X error nr %d: %s\n",ev->error_code,buf);
  abort(); // so a debugger can be used
}

uint calc_color(const char* cname) {
  XColor col,col1;
  if (!dis) err("calc_color: display = 0");
  XAllocNamedColor(dis,cmap,cname,&col,&col1);
  return col.pixel;
}

XftColor *xft_calc_color(uchar r,uchar g,uchar b) {
  XRenderColor col;
  XftColor *xft_col = new XftColor;
  col.red=r*0x100; col.green=g*0x100; col.blue=b*0x100; col.alpha=0xffff;
  XftColorAllocValue(dis,vis,cmap,&col,xft_col);
  return xft_col;
}

void init_xwindows() {
  if (!XInitThreads()) err("X threads");
  char *display=getenv("DISPLAY");
  if (!display) err("display?");
  dis=XOpenDisplay(display);
  screen=DefaultScreen(dis);
  vis=DefaultVisual(dis,screen);
  cmap=DefaultColormap(dis, screen);
  root_window=DefaultRootWindow(dis);
  //XSynchronize(dis,True); // useful during debug
  //cursor_xterm = XCreateFontCursor(dis, XC_xterm);
  wm_icon_hints.flags=0;

  cBlack = BlackPixel(dis, screen);
  cWhite = WhitePixel(dis, screen);
  cGrey  = calc_color("#C0C0C0");
  cButBground= calc_color("#F0F0F0");
  cMenuBground= calc_color("#E2E2E2");
  cRose  = calc_color("#FFC6BE");
  xft_White=xft_calc_color(0xff,0xff,0xff);
  xft_Black=xft_calc_color(0,0,0);
  xft_Blue=xft_calc_color(0,0,0xff);
  xft_Red=xft_calc_color(0xff,0,0);

  cBlue=xft_Blue->pixel;
  cRed=xft_Red->pixel;
  cGradientBlue.set_color("#F0FFFF","#D7F7FF","#C0F0FF","#B0E0E7","#A0D0E0");
  cGradientGrey.set_color("#FFFFFF","#F0F0F0","#E0E0E0","#D0D0D0","#C7C7C7");
  cGradientGreen.set_color("#E0FFE0","#B7F0B7","#B0E0B0","#84D084","#77C077");
  cGradientWheat.set_color("#FFF0D7","#EEE8CD","#E0E0C0","#D0D0B0","#C0C0A0");
  //cGradientRose.set_color("#FFE0E0","#F0D0D0","#E0C0C0","#C0B7B7","#B09090");
  cBorder      = calc_color("#707070");
  cForeground  = calc_color("#CDBA96"); // wheat3
  cBackground  = calc_color("#EEE8CD"); // cornsilk2
  cPointer     = calc_color("#FF7D7D"); // slider pointer
  cSlBackgr    = calc_color("#B0B0B0"); // slider background
  cSelRBut     = calc_color("#E7E760"); // selected radiobutton
  cSelCmdM     = calc_color("#6495ED"); // selected cmd menu item
  cScrollbar   = calc_color("#03ABFF"); // scrollbar
  cScrollbar1  = calc_color("#C0FFFF"); // scrollbar
  cCheckBox    = calc_color("#E2E2E2"); // checkbox
  cai_White=cairo_pattern_create_rgb(1,1,1);
  cai_Black=cairo_pattern_create_rgb(0,0,0);
  cai_Blue=cairo_pattern_create_rgb(0,0,1);
  cai_Red=cairo_pattern_create_rgb(1,0,0);
  cai_Border=cairo_pattern_create_rgb(70/256.,70/256.,70/256.);

  WM_DELETE_WINDOW = XInternAtom(dis,"WM_DELETE_WINDOW",0); // topwin: exit(), subwin: delete subwin only
  WM_PROTOCOLS = XInternAtom(dis, "WM_PROTOCOLS", 0);
  CLIPBOARD = XInternAtom(dis, "CLIPBOARD", 0);

  signal(SIGINT,sig_handler);
  XSetErrorHandler(X_sig_handler);
  XSetIOErrorHandler(io_handler);
  char buf[50];
  sprintf(buf,"FreeSans-%d",nominal_font_size);
  xft_def_font=XftFontOpenName(dis,screen,buf); // can't fail!
  sprintf(buf,"FreeSans-%d:bold",nominal_font_size);
  xft_bold_font=XftFontOpenName(dis,screen,buf);
  sprintf(buf,"FreeSans-%d",nominal_font_size-2);
  xft_small_font=XftFontOpenName(dis,screen,buf);
  sprintf(buf,"Monospace-%d",nominal_font_size-1);
  xft_mono_font=XftFontOpenName(dis,screen,buf);

  FT_UInt nn=32; // don't care
  XftGlyphExtents(dis,xft_mono_font,&nn,1,&glyphinfo);
  char_wid=glyphinfo.xOff; // = 7

  def_gc= XCreateGC(dis,root_window, 0, 0);
  clip_gc= XCreateGC(dis,root_window, 0, 0);
  //XSetForeground(dis, clip_gc, 0);

  gcv.graphics_exposures=0;
  XChangeGC(dis,def_gc,GCGraphicsExposures,&gcv); // prevents NoExpose events
  XChangeGC(dis,clip_gc,GCGraphicsExposures,&gcv);
}

WinBase *create_top_window(const char* title,Rect rect,bool resize,void (*draw)(XftDraw *win,Rect exp_rect),uint bgcol) {
  top_wd=new TopWin(title,rect,resize,draw,bgcol);   // top window
  return top_wd;
}

void map_top_window() {
  XMapWindow(dis,top_wd->win);
}

BgrWin::~BgrWin() {
  if (is_graphw) gr_winb.unlist_widget(this);
  else bg_winb.unlist_widget(this);
}

void clear(uint win) { XClearWindow(dis,win); }

void clear(uint win,Rect rect) {
  XClearArea(dis,win,rect.x,rect.y,rect.width,rect.height,false);
}

void BgrWin::clear() { XClearWindow(dis,win); }

void BgrWin::clear(Rect rect) {
  XClearArea(dis,win,rect.x,rect.y,rect.width,rect.height,false);
}

void BgrWin::draw_point(Point p) {
  XDrawPoint(dis, win, def_gc, p.x, p.y);
}

void BgrWin::draw_line(Point p1,Point p2) {
  XDrawLine(dis, win, def_gc, p1.x, p1.y, p2.x, p2.y);
}

void BgrWin::draw_lines(Point *points,int len) {
  XDrawLines(dis,win,def_gc,(XPoint*)points,len,CoordModeOrigin);
}

void BgrWin::draw_segments(Segment *segs,int len) {
  XDrawSegments(dis,win,def_gc,(XSegment*)segs,len);
}

void draw_pixmap(uint win,Point pt,Pixmap pm,int _dx,int _dy) {
  XCopyArea(dis,pm,win,def_gc,0,0,_dx,_dy,pt.x,pt.y);
}

void draw_pixmap(uint win,Point pt,Pixmap2 pm,int _dx,int _dy) {
  gcv.clip_mask = pm.cm;
  gcv.clip_x_origin =pt.x;
  gcv.clip_y_origin =pt.y;
  XChangeGC(dis, clip_gc, GCClipMask | GCClipXOrigin | GCClipYOrigin, &gcv);

  XCopyArea(dis,pm.pm,win,clip_gc,0,0,_dx,_dy,pt.x,pt.y);
}

uint bit2pix(const char* bits,int dx,int dy) {
  return XCreatePixmapFromBitmapData(dis,root_window,(char*)bits,dx,dy,cBlack,cWhite,pixdepth);
}

/* example pixmap:
const char *wav2s[]={
"16 16 3 1",
"# c #0000ff",
"a c #ff0000",
". c #ffffff",
"....##......aa..",
"...#..#...aa.a..",
"...#..#.aa...a..",
"..#...aa....aa..",
"..#..a.#..aa.a..",
".#...a.#aa...a..",
".#...aaa.....a..",
"#....a.#.....a..",
"#....a..#....a..",
".....a..#.aaaa.#",
".....a..#aaaaa.#",
".....a..#aaaaa#.",
"..aaaa..#.aaa.#.",
".aaaaa...#...#..",
".aaaaa...#..#...",
"..aaaa....##...."};
*/

Pixmap2 create_pixmap(const char* pm_data[]) {
  int i,dx,dy,nr_col;
  char ch;
  Pixmap2 pm;
#if 0
  XpmAttributes attributes;  // using Xpm lib
  attributes.valuemask = XpmSize;
  int status=XpmCreatePixmapFromData (
            dis,
            root_window,
            (char**)pm_data,
            &pm.pm,
            &pm.cm,
            &attributes);
  int width=attributes.width,
      height=attributes.height;
#else
  sscanf(pm_data[0],"%d %d %d",&dx,&dy,&nr_col);
  pm.pm=XCreatePixmap(dis,root_window,dx,dy,pixdepth);
  // printf("dx=%d dy=%d nr_col=%d\n",dx,dy,nr_col);
  struct ColSym {
    char ch;
    char col[8];
  };
  ColSym *colsym=new ColSym[nr_col];
  for (i=0;i<nr_col;++i)
    sscanf(pm_data[i+1],"%c c %7s",&colsym[i].ch,colsym[i].col);
  Point *points=new Point[dx*dy];
  for (int col=0;col<nr_col;++col) {
    ch=colsym[col].ch;
    uint pixcol=calc_color(colsym[col].col);
    int nxt;
    if (ch=='.') {
      const int mul=(dx/8+1)*8,
                dim=dx+dy*mul/8;
      uchar *bm_buf=new uchar[dim]; // bitmap buffer
      for (i=0;i<dim;++i) bm_buf[i]=0;
      nxt=0;
      for (int y=0;y<dy;++y) {
        for (int x=0;x<dx;++x)
          if (pm_data[y+nr_col+1][x]==ch)
            points[nxt++].set(x,y);
          else
            bm_buf[(x+y*mul)/8] += 1 << x%8;
        }
      // printf("buf[0]=0x%x buf[1]=0x%x\n",bm_buf[0],bm_buf[1]);
      pm.cm=XCreateBitmapFromData(dis,root_window,(char*)bm_buf,dx,dy); // clip mask
      delete[] bm_buf;
      set_color(pixcol);
      XDrawPoints(dis,pm.pm,def_gc,(XPoint*)points,nxt,CoordModeOrigin);
    }
    else {
      nxt=0;
      for (int y=0;y<dy;++y)
        for (int x=0;x<dx;++x)
          if (pm_data[y+nr_col+1][x]==ch) points[nxt++].set(x,y);
      set_color(pixcol);
      XDrawPoints(dis,pm.pm,def_gc,(XPoint*)points,nxt,CoordModeOrigin);
    }
  }
  delete[] colsym; delete[] points;
#endif
  return pm;
}

void set_icon(uint win,uint pixmap,int dx,int dy) {
  wm_icon_hints.flags=IconPixmapHint;
  wm_icon_hints.icon_pixmap=pixmap;
  XSetWMHints(dis,win,&wm_icon_hints);
//  XFreePixmap(dis,wm_icon_hints.icon_pixmap);
}

void set_icon(uint win,const uchar *bits,int dx,int dy,uint col) {
  uint pixmap=XCreatePixmapFromBitmapData(
    dis,root_window,(char*)bits,dx,dy,col,cGrey,pixdepth);
  set_icon(win,pixmap,dx,dy);
}

void BgrWin::fill_circle(int x1,int y1,int r) {
  XFillArc(dis, win,def_gc,x1-r,y1-r,2*r,2*r,0,360*64);
}

void BgrWin::fill_polygon(Point *points,int len) {
  XFillPolygon(dis, win, def_gc,(XPoint*)points,len,Convex,CoordModeOrigin);
}

void BgrWin::fill_triangle(Point pt1,Point pt2,Point pt3) {
  Point points[3]= { pt1,pt2,pt3 };
  XFillPolygon(dis, win, def_gc,(XPoint*)points,3,Convex,CoordModeOrigin);
}

void BgrWin::fill_rectangle(Rect rect) {
  XFillRectangle(dis,win,def_gc,rect.x,rect.y,rect.width,rect.height);
}

void BgrWin::move_contents_h(int delta) {
  //XSetClipMask(dis,def_gc,None);
  if (delta<0) {
    XCopyArea(dis,win,win,def_gc,-delta,0,dx+delta,dy,0,0);
    XClearArea(dis,win,dx+delta,0,dx,dy,false);
  }
  else if (delta>0) {
    XCopyArea(dis,win,win,def_gc,0,0,dx-delta,dy,delta,0);
    XClearArea(dis,win,0,0,delta,dy,false);
  }
}

RButWin::~RButWin() {
  for (int i=0;i<=buttons->butnr;++i) delete buttons->but[i];
  delete[] buttons->but;
  rb_winb.unlist_widget(this);
}

HSlider::~HSlider() { hsl_winb.unlist_widget(this); }

VSlider::~VSlider() { vsl_winb.unlist_widget(this); }

HVSlider::~HVSlider() { hvsl_winb.unlist_widget(this); }

HScrollbar::~HScrollbar() { hsc_winb.unlist_widget(this); }

VScrollbar::~VScrollbar() { vsc_winb.unlist_widget(this); }

EditWin::~EditWin() { ed_winb.unlist_widget(this); }

CheckBox::~CheckBox() { chb_winb.unlist_widget(this); }

void CheckBox::set_cbval(bool val,int fire,bool do_draw) {
  value=val;
  if (do_draw) draw();
  if (fire && cmd) cmd(id,val);
}

void set_width(int wid) {
  gcv.line_width=wid;
  XChangeGC(dis, def_gc, GCLineWidth, &gcv);
}

void set_color(uint col) {
  XSetForeground(dis, def_gc, col);
}

void set_width_color(int wid,uint col) {
  gcv.line_width=wid;
  gcv.foreground=col;
  XChangeGC(dis, def_gc, GCLineWidth|GCForeground, &gcv);
}

void BgrWin::set_selected(bool sel,uint bgcol) {
  if (sel) { gcv.dashes=1; gcv.line_style=LineDoubleDash; gcv.background=bgcol; }
  else gcv.line_style=LineSolid; 
  XChangeGC(dis, def_gc, GCLineStyle|GCDashList|GCBackground, &gcv);
}

void TheCursor::unset() {
  if (diawd) diawd->unset_cursor();
  if (edwd) edwd->unset_cursor();
  diawd=0; edwd=0;
}

bool textcursor_active() {
  return the_cursor.diawd || the_cursor.edwd;
}

struct UevQueue {  // user-event queue
  int queue_len,
    (*buffer)[3],
    ptr1,
    ptr2;
  UevQueue():
      queue_len(50),
      buffer(new int[queue_len][3]),
      ptr1(0),
      ptr2(0) {
  }
  void push(int param0,int param1,int param2) {
    int n=(ptr2+1)%queue_len;
    if (n==ptr1) {
      printf("push: increase buffer len to %d\n",2*queue_len);
      int (*prev)[3]=buffer,
          i1,i2,i3;
      buffer=new int[2*queue_len][3];
      for (i1=ptr1,i2=0;i1!=ptr2;i1=(i1+1)%queue_len,++i2)
        for (i3=0;i3<3;++i3)
          buffer[i2][i3]=prev[i1][i3];
      ptr1=0;
      n=queue_len;
      ptr2=n-1;
      queue_len*=2;
      delete[] prev;
    }
    buffer[ptr2][0]=param0;
    buffer[ptr2][1]=param1;
    buffer[ptr2][2]=param2;
    ptr2=n;
  }
  bool is_empty() { return ptr1==ptr2; }
  void pop(int *dat) {
    for (int i=0;i<3;++i)
      dat[i]=buffer[ptr1][i];
    ptr1=(ptr1+1)%queue_len;
  }
} uev_queue;

void x_widgets_log() {
  printf("uev_queue: queue_len = %d\n",uev_queue.queue_len);
}

void send_uev(int cmd,int param1,int param2) {
  pthread_mutex_lock(&mtx);
  uev_queue.push(cmd,param1,param2);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mtx);
}

void *keep_alivefun(void* data) {
  while (true) {
    pthread_mutex_lock(&mtx);
    ++go_ticks;
    pthread_mutex_unlock(&mtx);
    usleep(50000); 
    send_uev('go');
  }
}

void run_xwindows() {
  XEvent ev;
  atexit(do_atexit);
  x_running=true;
  xwin_thread=pthread_self();
  if (alert_win) XMapWindow(dis,alert_win->awin); // alert window mapped now
  pthread_t keep_alive;
  pthread_create(&keep_alive,0,keep_alivefun,0);
  while (true) {
    while (true) {
      pthread_mutex_lock(&mtx);
      if (uev_queue.is_empty()) {
        pthread_mutex_unlock(&mtx);
        break; 
      }
      int uev_dat[3];
      uev_queue.pop(uev_dat);
      pthread_mutex_unlock(&mtx);
      if (uev_dat[0]=='go') {
        if (repeat.on && go_ticks%3==0)  // 3 * 50 ms
          handle_event(&repeat.ev);
      }
      else
        handle_uev(uev_dat[0],uev_dat[1],uev_dat[2]);
    }
    if (XPending(dis)) {
      XNextEvent(dis,&ev);
      handle_event(&ev);
    }
    else {
      pthread_mutex_lock(&mtx);
      pthread_cond_wait(&cond, &mtx);
      pthread_mutex_unlock(&mtx);
    }
  }
}

void flush_X() { XFlush(dis); }

void delete_window(uint& win) {
  if (win) {
    XUnmapWindow(dis,win);
    XDestroyWindow(dis,win);
    win=0;
  }
}

void hide_window(uint win) { XUnmapWindow(dis,win); }

void map_window(uint win) { XMapWindow(dis,win); }

void set_cursor(uint win,uint cursor) {
  Cursor curs=XCreateFontCursor(dis,cursor);
  XDefineCursor(dis,win,curs);
  XFreeCursor(dis,curs);
}
void set_custom_cursor(uint win,uint cursor) {
  XDefineCursor(dis,win,cursor);
}
uint create_cursor(const uchar *bm,const uchar *clip_bm,int dx,int dy,Point hot,bool invert) {
  uint curs= bm ? XCreatePixmapFromBitmapData(dis,root_window,(char*)bm,dx,dy,1,0,1) : 0,
       clip= clip_bm ? XCreatePixmapFromBitmapData(dis,root_window,(char*)clip_bm,dx,dy,1,0,1) : 0;
  static XColor col1,col2;
  static bool init=true;
  if (init) {
    init=false;
    XParseColor(dis,cmap,"black",&col2);
    XParseColor(dis,cmap,"white",&col1);
  }
  XColor *col= invert ? &col2 : &col1,
         *bg= invert ? &col1 : &col2;
  return XCreatePixmapCursor(dis,curs,clip,col,bg,hot.x,hot.y);
}

void reset_cursor(uint win) {
  XUndefineCursor(dis,win);
}
void fill_polygon(uint win,uint col,Point *points,int len) {
  set_color(col);
  XFillPolygon(dis,win,def_gc,(XPoint*)points,len,Convex,CoordModeOrigin);
}
void fill_rectangle(uint win,uint col,Rect rect) {
  set_color(col);
  XFillRectangle(dis,win,def_gc,rect.x,rect.y,rect.width,rect.height);
}
void fill_triangle(uint win,uint col,Point pt1,Point pt2,Point pt3) {
  set_color(col);
  Point points[3]= { pt1,pt2,pt3 };
  XFillPolygon(dis, win, def_gc,(XPoint*)points,3,Convex,CoordModeOrigin);
}
void draw_line(uint win,int wid,uint col,Point p1,Point p2) {
  set_width_color(wid,col);
  XDrawLine(dis, win, def_gc, p1.x, p1.y, p2.x, p2.y);
}
void draw_circle(uint win,int width,uint col,Point mid,int radius) {
  set_width_color(width,col);
  XDrawArc(dis, win, def_gc,mid.x-radius,mid.y-radius,2*radius,2*radius,0,360*64);
}
void fill_circle(uint win,int width,uint col,Point mid,int radius) {
  set_color(col);
  XFillArc(dis, win,def_gc,mid.x-radius,mid.y-radius,2*radius,2*radius,0,360*64);
}
bool WinBase::re_parent(uint new_pw) {
  if (pwin!=new_pw) {
    XReparentWindow(dis,win,new_pw,x,y);
    pwin=new_pw;
    return true;
  }
  return false;
}

Lamp::Lamp(uint _pwin,Rect _rect,uint bgcol):
    pwin(_pwin),
    rect(_rect),
    col(cWhite) {
  static Pixmap2 pm_st=create_pixmap(lamp_pm);
  pm=pm_st;
}
void Lamp::draw() {
  draw_pixmap(pwin,Point(rect.x,rect.y),pm,12,12);
  set_color(col);
  XFillArc(dis,pwin,def_gc,rect.x+2,rect.y+2,8,8,0,360*64);
}
void Lamp::lamp_color(uint _col) {
  col=_col;
  draw();
}

cairo_surface_t *cai_get_surface(BgrWin *bgr) {
  if (!bgr->surface)
    bgr->surface=cairo_xlib_surface_create(dis,bgr->win,vis,bgr->dx,bgr->dy);
  return bgr->surface;
}

void BgrWin::cai_draw_lines(Point *points,int len,int lwid,cairo_pattern_t *col) {
  if (!surface)
    surface=cairo_xlib_surface_create(dis,win,vis,dx,dy);
  cairo_t *cr=cairo_create(surface);
  cairo_set_source(cr,col);
  cairo_set_line_width(cr,lwid);
  cairo_move_to(cr,points[0].x+0.5,points[0].y+0.5);
  for (int i=1;i<len;++i) cairo_line_to(cr,points[i].x+0.5,points[i].y+0.5);
  //cairo_move_to(cr,points[0].x,points[0].y);
  //for (int i=1;i<len;++i) cairo_line_to(cr,points[i].x,points[i].y);
  cairo_stroke(cr);
  if (cairo_status(cr)>0) alert("cai_draw_lines: status=%d",cairo_status(cr));
  cairo_destroy(cr);
}
