#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <x-widgets.h>
#include <snd-interface.h>
#include <str.h>
#include "fft.h"

typedef unsigned int uint;
typedef unsigned char uchar;

const int
  spect_size=800, // spectrum window
  ww_size=800,    // wave window 
  fftw_size=696,  // fft-sample window
  total_width=806,
  peaks_max=3,    // spectrum peaks
  sclin_dist=3,   // score line distance
  midi_min=30, midi_max=110, // displayed score notes
  notes_max=40,   // displayed notes
  note_len=5;     // displayed note length

WinBase *top_win;

bool debug,
     options;

char *inf=0;  // wave file
Str configf;  // configuration file

enum {
  eSelect1=1,
  eSelect2,
  eUnselect1,
  eUnselect2
};

static uint cGreen,cLightYellow,cFreqScale,cWaveBgr,cLightGrey,cWave;

const char *wav2s_icon[]={ // made with kiconedit
"16 16 3 1",
"# c #000000",
"a c #ff0000",
". c #ffffff",
"....##......aa..",
"...#..#...aaaa..",
"..#...#.aaaaaa..",
"..#...aaaaaa.a..",
".#...aaaaa...a..",
".#...aaa.....a..",
"#....a.#.....a..",
"#....a.#.....a..",
"#....a..#....a.#",
".....a..#.aaaa.#",
".....a..#aaaaa.#",
".....a..#aaaaa#.",
"..aaaa..#.aaa.#.",
".aaaaa...#...#..",
".aaaaa...#..#...",
"..aaa.....##...."};

template <class T>
struct Arr {
  T *buf;
  uint dim;
  T nul;
  Arr<T>():dim(0) { }
  void init(uint d) { dim=d; buf=new T[dim]; }
  T& operator[](uint ind) {
    if (ind<dim) return buf[ind];
    if (debug) abort();
    alert("array index=%d (expected 0 - %d)",ind,dim-1);
    return nul;
  }
};
/*
template <class T,uint dim>
struct FixedArr {
  T buf[dim];
  T& operator[](uint ind) {
    if (ind<dim) return buf[ind];
    alert("array index=%d (expected 0 - %d)",ind,dim-1);
    if (debug) abort();
    return buf[0];
  }
};
*/
struct Gui {
  BgrWin *spectrum_win,
         *wave_win,
         *fft_win;
  HSlider *fine,
          *course;
  TextWin *info;
  Button *play;
  Gui();
  int f_c_value();
  void draw_info(int fft_nr);
} *gui;

struct SpectData {
  float base_freq,
        assigned_freq,   // by the user
        peaks[peaks_max];
  int maxmag,   // max peak value
      lst_sline;  // last spect line
  uchar midinr;
  char nr_peaks;
  bool skipped,
       bf_valid,  // base freq valid?
       asf_valid; // assigned freq?
  Arr<uchar> spectlines; // buffer for spectrum lines, lines are 2 or 4 pixels apart
  SpectData():
    base_freq(1.),
    maxmag(0),
    lst_sline(-1),
    midinr(0),
    nr_peaks(-1),
    skipped(false),
    bf_valid(false),
    asf_valid(false) { }
};

struct ScoreView {
  uint svwin;
  SubWin *subw;
  ScoreView(Point);
};

struct Wav2Score {
  int called,     // eval() called
      data_size,  // nr bytes in wav file
      nr_fft_samples,
      nr_wav_samples,
      buf_size,   // fft buffer size
      wwh,        // wave window height
      the_xpos1,  // selection in wave window
      the_xpos2,
      mult_spl,   // multiplier spectrum line
      min_midinr,
      max_midinr,
      start, stop; // freq range
  bool wave_init,
       stereo,
       use_each_sample,
       i_am_playing,
       stop_requested;
  char select_chan; // 0, 'L' or 'R'
  float *hamming,
        mid_C,        // midi nr for middle-C, default 48
        spect_thresh, // spectrum-line threshold
        dur_inc,      // note duration increase, default 1.0
        mult;         // freq scaling
  Arr<SpectData> spectdata;
  short *shortbuf; // contents of the wav file
  struct WavLin {
    char yhi1,ylo1,
         yhi2,ylo2;
  } *wavelines;
  ScoreView *sv_win;
  Wav2Score():
    buf_size(0),
    min_midinr(0),
    max_midinr(0),
    use_each_sample(false),
    select_chan(0),
    mid_C(48.0),
    spect_thresh(0.2),
    dur_inc(1.),
    sv_win(0) {
  }
  void init();
  bool read_wav_head(const char *inf,FILE*& wav);
  bool read_wav(FILE *wav,short *fb1,short *fb2,const int bufsize);
  void eval(float *buffer1,float *buffer2);
  void eval_wavf(FILE* wav);
  void eval_wavf_gui(FILE* wav);
  void fill_wavelines();
  void sel_wave(int xpos);
  void draw_waveline(int xpos,int mode);
  void draw_fft_sample(int nr);
  void wav_listen();
  int freq2mnr(float freq) {
    float fl_midinr= 12./logf(2)*logf(freq*mult*2);
    return lrint(fl_midinr+mid_C);
  }
  void write_conf(FILE *conf);
  void read_conf(bool initialize);
} wav2s;

int max(int a,int b) { return a>=b ? a : b; }
int min(int a,int b) { return a<=b ? a : b; }
int minmax(int a, int x, int b) { return x>=b ? b : x<=a ? a : x; }
int idiv(int a,int b) { return (2*a+b)/(2*b); }

bool Wav2Score::read_wav_head(const char *inf,FILE*& wav) {
  if ((wav=fopen(inf,"r"))==0)
    err("%s not opened",inf);
  char word[20];
  short dum16;
  int dum32,
      er_nr=0;
  if (
    fread(word,4,1,wav)!=1 && (er_nr=1) || strncmp(word,"RIFF",4) && (er_nr=2) || 
    fread(&dum32, 4,1,wav)!=1 && (er_nr=3) ||                         // header size
    fread(word, 8,1,wav)!=1 && (er_nr=4) || strncmp(word,"WAVEfmt ",8) && (er_nr=5) ||
    fread(&dum32, 4,1,wav)!=1 && (er_nr=6) || dum32!=16 && (er_nr=7) ||            // chunk size  
    fread(&dum16, 2,1,wav)!=1 && (er_nr=8)                            // format tag (1 = uncompressed PCM)
  ) goto error;
  
  if (dum16!=1) { alert("format = %d, should be 1",dum16); return false; }
  if (fread(&dum16, 2,1,wav)!=1 && (er_nr=11))                         // no of channels
    goto error;
  if (dum16==2) stereo=true;
  else if (dum16==1) stereo=false;
  else { alert("nr channels = %d, should be 1 or 2",dum16); return false; }
  if (fread(&dum32, 4,1,wav)!=1 && (er_nr=13))                   // rate
    goto error;
  if (dum32!=SAMPLE_RATE) { alert("rate = %d, must be %d",dum32,SAMPLE_RATE); return false; }
  if (fread(&dum32, 4,1,wav)!=1 && (er_nr=14))                   // average bytes/sec
    goto error;
  if (dum32!= SAMPLE_RATE* (stereo ? 4 : 2)) {
    alert("byte/sec = %d, must be 2*%d",dum32,SAMPLE_RATE); return false;
  }
  if (fread(&dum16, 2,1,wav)!=1 && (er_nr=16) ||                // block align
      fread(&dum16, 2,1,wav)!=1 && (er_nr=17))                  // bits per sample
    goto error;
  if (dum16!=16) {
    alert("bits per sample is %d, must be 16",dum16); return false;
  }
  if (fread(word, 4,1,wav)!=1 && (er_nr=19))
    goto error;
  if (strncmp(word,"data",4)) {
    word[4]=0; alert("word: '%s' (expected: 'data')",word); return false;
  }
  if (fread(&data_size, 4,1,wav)!=1 && (er_nr=21))           // sample length
    goto error;
  wave_init=true;
  return true;

  error:
  alert("format error (nr %d) in wave file",er_nr);
  return false;
}

bool Wav2Score::read_wav(FILE *wav,short *fb1,short *fb2,const int bufsize) {
  static short *sbuf=new short[stereo ? 2*bufsize : bufsize];
  if (fread((char*)sbuf,stereo ? 4*bufsize : 2*bufsize,1,wav)!=1) {
    printf("size=%d fft-samples=%d\n",data_size,nr_fft_samples);
    return false;
  }
  if (stereo)
    for (int n=0;n<bufsize;++n) {
      if (select_chan=='R')
        fb1[n]=sbuf[2*n];
      else if (select_chan=='L')
        fb1[n]=sbuf[2*n+1];
      else {
        fb1[n]=sbuf[2*n];
        fb2[n]=sbuf[2*n+1];
      }
    }
  else
    for (int n=0;n<bufsize;++n)
      fb1[n]=sbuf[n];
  return true;
}

const uint
  eHi=1, // sharp
  eLo=2; // flat

const int
  sclin_max=45;

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
      printf("midinr_to_lnr: signs_mode = %d\n",signs_mode);
  }

  // middle C: amuc: lnr=27
  //           midi: 60, 60/12*7 = 35, 27=62-35
  
  lnr2=62 - mnr/12*7 - ar[ind];
  if (lnr2<0 || lnr2>=sclin_max) return false;
  lnr=lnr2;
  sign=s[ind];
  return true;
}

struct AmucOutput {
  FILE *out;
  float min_scale;
  bool out_ok,
       note_on;
  int the_mnr,
      start_time, // start time of last note
      signsmode;
  AmucOutput():
    out(0),
    signsmode(0) {
  }
  void init() {
    out_ok=false;
    if ((out=fopen("fft.sco","w"))==0)
      err("Could not open file fft.sco\n");
    out_ok=true;
    the_mnr=0;
    note_on=false;
    fputs("4m8\n7m0e0g0k0 \"fft-tune\" ",out);
  }
  void close() {
    if (out) fclose(out);
  }
  int time2time(int t) {  // rounded float division, yields best results
    static float mult=wav2s.buf_size / 2048. / 6. * wav2s.dur_inc;
    return wav2s.use_each_sample ? t : lrint(t * mult);
  }
  void add_note(int midi_nr,int time) {
    if (debug)
      printf("mnr=%d the_mnr=%d time=%d st_t=%d dur=%d n_o=%d\n",midi_nr,the_mnr,time,start_time,time2time(time-start_time),note_on);
    if (midi_nr!=the_mnr) {
      if (note_on) {
        int dur;
        if (time-start_time>0 && (dur=time2time(time-start_time))>0) {
          uchar lnr,sign;
          bool ok=midinr_to_lnr(the_mnr,lnr,sign,signsmode);
          if (ok) {
            fprintf(out,"2L%dN%dd%di%ds0c0g0 ",lnr,time2time(start_time),dur,sign);
          }
        }
        if (midi_nr==0) {
          note_on=false;
          the_mnr=0;
        }
        else {
          start_time=time;
          the_mnr=midi_nr;
        }
      }
      else if (midi_nr>0) {
        start_time=time;
        the_mnr=midi_nr;
        note_on=true;
      }
    }
  }
} ao;

void Wav2Score::init() {
  the_xpos1=the_xpos2=-1;
  wave_init=false;
  hamming=new float[buf_size];
  const float A = 0.54,
              B = 0.46;
  for (int i=0;i<buf_size;++i) {
    float ind = i*2*M_PI / (buf_size-1);
    hamming[i]=A-B*cos(ind);
  }
  mult=SAMPLE_RATE/(buf_size*261.6); // see comment within eval()
  start=min_midinr ? max(2,int(exp((min_midinr-60)/12.*logf(2))/mult)) : 0;
  stop= max_midinr ? min(buf_size/2,int(exp((max_midinr-60)/12.*logf(2))/mult)) : buf_size/2;
}

float index_of_peak(float y0,float y1,float y2,bool &valid) {
/* y=a*x*x + b*x + c
   x=0,1,2
   y=0,y1,y2
   c=0

   y1=  a+  b  -> a=-y1 + y2/2
   y2=4*a+2*b     b=2*y1 - y2/2
*/
  if (y0>y1 || y1<y2) { valid=false; return 0.; }
  if (fabs(y0-y1)<0.01) return 0.5;
  if (fabs(y1-y2)<0.01) return 1.5;
  y1-=y0;
  y2-=y0;
  const float
    a=-y1+y2/2,
    b=2*y1-y2/2;
  // printf("ytop=%.2f\n",y0-b*b/(4*a));
  return -b/(2*a);
}

bool near_eq(float a,float b) {  // nearly equal?
  float div=a/b;
  return div<1.06 && div>0.94;
}

void Wav2Score::eval(float *buffer,float *buffer2) { // buffer size = buf_size
  int i,
      lst_top=-1,
      maxval=0,
      maxmag=0,
      mag;
  float toploc[peaks_max];  // location of ampl tops
  bool above_thresh=false;
  SpectData *sd=0;
  if (gui) sd=&spectdata[called];

  for (i=0;i<buf_size;i+=2) {  // replace with abs value
    float re,im;
    re=buffer[i]; im=buffer[i+1];
    buffer[i/2]=hypot(re,im);
    if (buffer2) {
      re=buffer2[i]; im=buffer2[i+1];
      buffer[i/2]+=hypot(re,im); // add buffer2 to buffer
    }
    mag=lrint(buffer[i/2]);
    if (maxmag<mag) maxmag=mag; // find max value
  }
  if (gui) sd->maxmag=maxmag;
  // now buf_size/2 buffer values valid
  if (maxmag<200) {
    if (gui) sd->skipped=true;
    else ao.add_note(0,called);
    return;
  }
  for (i=max(start,2);i<stop;++i) { // start at i=2 because buffer[i-2] will be used
    mag=lrint(buffer[i]);
    if (mag>int(maxmag*spect_thresh)) {
      if (!above_thresh) {
        above_thresh=true;
        if (lst_top>=peaks_max-1) break;
        ++lst_top;
        maxval=0;
        for (;i<buf_size/2;++i) {
          mag=lrint(buffer[i]);
          if (maxval<=mag) {
            maxval=mag;
          }
          else { // 1 position past local top
            bool valid=true;
            float peak=index_of_peak(buffer[i-2],buffer[i-1],buffer[i],valid);
            if (!valid) { --lst_top; break; }
            toploc[lst_top]=i-2+peak;
            if (debug)
              printf("called=%d i=%2d toploc[%d]=%.1f (%.1f %.1f %.1f)\n",
                     called,i,lst_top,toploc[lst_top],buffer[i-2],buffer[i-1],buffer[i]);
            for (;i<buf_size/2;++i)
              if (buffer[i]<int(maxmag*spect_thresh) || buffer[i]<buffer[i+1]) // too small, or increasing again
                break;
            above_thresh=false;
            break;
          }
        }
      }
    }
    else
      above_thresh=false;
  }
  if (gui)
    for (i=0;i<buf_size/2 && i<spect_size/2;++i) {
      if (maxmag>10) {
        sd->spectlines[i]=lrint(buffer[i]) * mult_spl / maxmag;
        sd->lst_sline=i;
      }
      else sd->spectlines[i]=0;
    }
  float tmp,
        freq=0.;
  bool valid=false;
  for (i=0;i<=lst_top;++i) {
    if (debug) printf("freq=%.1f valid=%d\n",freq,valid);
    if (toploc[i]>1.) {
      if (!valid) { valid=true; freq=toploc[i]; }
      else if (near_eq(toploc[i],1.5*freq)) { tmp=toploc[i]/3; if (tmp>=start) freq=tmp; }
      else if (near_eq(toploc[i],2*freq)) { tmp=toploc[i]/2; if (tmp>=start) freq=tmp; }
      else if (near_eq(toploc[i],3*freq)) { tmp=toploc[i]/3; if (tmp>=start) freq=tmp; }
      else if (near_eq(toploc[i],4*freq)) { tmp=toploc[i]/4; if (tmp>=start) freq=tmp; }
      else if (toploc[i]>4*freq) break;
      else { valid=false; break; }
    }
    else {
      valid=false; break;
    }
  }
  /* ln(2) = 0.69 -> delta midinr = 12
     freq = buf_size -> frequency = SAMPLE_RATE
     Amuc mid C: 261.6/2 Hz 
     frequency = 130.8 -> freq = 130.8*buf_size/SAMPLE_RATE -> midinr = 60
     midinr = 12/ln(2)*ln(freq) + 60
  */
  int midinr=0;
  if (gui) {
    if (valid) {
      if (!sd->asf_valid) {  // midnr already assigned by read_conf() ?
        midinr=wav2s.freq2mnr(freq);
        sd->midinr=midinr;
      }
      sd->base_freq=freq;
      sd->nr_peaks=lst_top;
      sd->bf_valid=true;
      for (i=0;i<=lst_top;++i) sd->peaks[i]=toploc[i];
    }
    else sd->nr_peaks=-1;
  }
  else {
    if (valid) {
      midinr=wav2s.freq2mnr(freq);
      if (debug) printf("freq=%.1f midinr=%d\n",freq,midinr);
      ao.add_note(midinr,called);
    }
    else ao.add_note(0,called);
  }
}

void Wav2Score::eval_wavf(FILE *wav) {
  int i;
  float *buffer1=new float[buf_size],
        *buffer2=new float[buf_size];
  short *sbuf1=new short[buf_size],
        *sbuf2=new short[buf_size];
  for (called=0;called<nr_fft_samples;++called) {
    if (called==0) {
      read_wav(wav,sbuf1,sbuf2,buf_size);
    }
    else {
      for (i=0;i<buf_size/2;++i) {
        sbuf1[i]=sbuf1[i+buf_size/2]; // copy buffer 2nd half
        if (stereo) sbuf2[i]=sbuf2[i+buf_size/2];
      }
      if (!read_wav(wav,sbuf1+buf_size/2,sbuf2+buf_size/2,buf_size/2)) {
        alert("wave data less then expected");
        break;
      }
    }
    if (stereo) {
      if (select_chan=='R') {
        for (i=0;i<buf_size;++i)
          buffer1[i]=sbuf1[i]*hamming[i];
        rfft(buffer1,buf_size/2,true);
        eval(buffer1,0);
      }
      else if (select_chan=='L') {
        for (i=0;i<buf_size;++i)
          buffer1[i]=sbuf2[i]*hamming[i];
        rfft(buffer1,buf_size/2,true);
        eval(buffer1,0);
      }
      else {
        for (i=0;i<buf_size;++i) {
          buffer1[i]=sbuf1[i]*hamming[i];
          buffer2[i]=sbuf2[i]*hamming[i];
        }
        rfft(buffer1,buf_size/2,true);
        rfft(buffer2,buf_size/2,true);
        eval(buffer1,buffer2);
      }
    }
    else {
      for (i=0;i<buf_size;++i)
        buffer1[i]=sbuf1[i]*hamming[i];
      rfft(buffer1,buf_size/2,true);
      eval(buffer1,0);
    }
  }
  ao.add_note(0,called); // the last note
  fclose(wav);
}

void Wav2Score::fill_wavelines() {
  int n1,n2,n3,
      step=idiv(nr_wav_samples,ww_size),
      div=(stereo && !select_chan ? 0x20000 : 0x10000)/wwh;
  wavelines=new WavLin[ww_size];
  if (stereo) {
    for (n1=0;n1<ww_size;n1+=1) {
      int yhi1=0,ylo1=0,yhi2=0,ylo2=0;
      for (n2=0;n2<step;n2+=2) {
        n3=step*n1*2;
        if (n3+n2+1<nr_wav_samples*2) {
          int val1=shortbuf[n3+n2]/div,
              val2=shortbuf[n3+n2+1]/div;
          if (yhi1<val1) yhi1=val1; if (yhi2<val2) yhi2=val2;
          if (ylo1>val1) ylo1=val1; if (ylo2>val2) ylo2=val2;
        }
      }
      WavLin& wl=wavelines[n1];
      wl.yhi1=yhi1; wl.ylo1=ylo1;
      wl.yhi2=yhi2; wl.ylo2=ylo2;
    }
  }
  else {
    for (n1=0;n1<ww_size;++n1) {
      int yhi=0,
          ylo=0;
      for (n2=0;n2<step;++n2) {
        n3=step*n1; // n1*nr_wav_samples/ww_size can overflow!
        if (n3+n2<nr_wav_samples) {
          int val=shortbuf[n3+n2]/div;
          if (yhi<val) yhi=val;
          if (ylo>val) ylo=val;
        }
      }
      wavelines[n1].yhi1=yhi?yhi:ylo;
      wavelines[n1].ylo1=ylo?ylo:yhi;
    }
  }
}

static void draw_line(int xpos,int yhi,int ylo,int mid) {
  if (yhi==ylo)
    gui->wave_win->draw_point(Point(xpos,yhi+mid));
  else 
    gui->wave_win->draw_line(Point(xpos,yhi+mid),Point(xpos,ylo+mid));
}

void Wav2Score::draw_waveline(int xpos,int mode) {
  static int mid1= stereo && !select_chan ? wwh/4 : wwh/2,
             mid2=wwh*3/4;
  int yhi,ylo;
  WavLin &wl=wavelines[xpos];
  set_width(1);
  if (mode) {
    switch (mode) {
      case eSelect1:
      case eSelect2:
        set_color(mode==eSelect1 ? cRed : cBlack);
        gui->wave_win->draw_line(Point(xpos,0),Point(xpos,wwh));
        set_color(cGrey);
        break;
      case eUnselect1:
      case eUnselect2:
        set_color(cWaveBgr);
        gui->wave_win->draw_line(Point(xpos,0),Point(xpos,wwh));
        set_color(cWave);
        break;
    }
  }
  else set_color(cWave);
  if (stereo) {
    draw_line(xpos,wl.yhi1,wl.ylo1,mid1);
    if (!select_chan)
      draw_line(xpos,wl.yhi2,wl.ylo2,mid2);
  }
  else
    draw_line(xpos,wl.yhi1,wl.ylo1,mid1);
}

void draw_marks(SpectData &sd,int sl_dist,bool draw_harm) {
  int n,x;
  set_color(cGreen);
  for (n=0;n<=sd.nr_peaks;++n) {
    x=lrint(sd.peaks[n] * sl_dist);
    gui->spectrum_win->fill_triangle(Point(x-3,0),Point(x+3,0),Point(x,5));
  }
  if (sd.bf_valid) {
    set_color(cRed);
    x=lrint(sd.base_freq * sl_dist);
    gui->spectrum_win->fill_triangle(Point(x-3,0),Point(x+3,0),Point(x,5));
  }
  if (sd.asf_valid) {
    set_color(cBlack);
    x=lrint(sd.assigned_freq * sl_dist);
    gui->spectrum_win->fill_triangle(Point(x-3,0),Point(x+3,0),Point(x,5));
    if (draw_harm) {
      for (int harm=2;;++harm) {
        x=lrint(sd.assigned_freq * sl_dist * harm);
        if (int(x)>spect_size) break;
        gui->spectrum_win->fill_triangle(Point(x-2,0),Point(x+2,0),Point(x,5));
      }
    }
  }
}

void draw_spectrum(Id) {
  int n,
      bs=wav2s.buf_size,
      nr=gui->f_c_value(),
      sl_dist= bs==512 ? 8 : bs==8192 ? 2 : 4;
  BgrWin *spwin=gui->spectrum_win;
  if (nr<wav2s.nr_fft_samples) {
    static int ybot=spwin->dy-20,
               ythresh=ybot-int(wav2s.mult_spl*wav2s.spect_thresh);

    set_width_color(1,cRed);
    int start=wav2s.start*sl_dist,       // draw spectrum threshold
        stop=wav2s.stop ? min(wav2s.stop*sl_dist,spect_size) : spect_size;
    for (n=start;n<stop;n+=10)
      spwin->draw_line(Point(n,ythresh),Point(n+5,ythresh));
    if (start>0)
      spwin->draw_line(Point(start-1,ythresh-4),Point(start-1,ythresh+4));
    if (wav2s.stop)
      spwin->draw_line(Point(stop+1,ythresh-4),Point(stop+1,ythresh+4));

    set_color(cBlack);
    SpectData &sd=wav2s.spectdata[nr];
    for (n=0;n<spect_size/sl_dist && n<=sd.lst_sline;++n)
      spwin->draw_line(Point(sl_dist*n,ybot),Point(sl_dist*n,ybot-wav2s.spectdata[nr].spectlines[n]));
    if (!sd.skipped && (sd.midinr || sd.asf_valid))
      draw_marks(sd,sl_dist,false);
  }
  int dist_250hz=bs*250*sl_dist/SAMPLE_RATE;
  static int y=spwin->dy-18;
  set_color(cFreqScale);
  spwin->fill_rectangle(Rect(0,y,spect_size,18));
  set_width(2);
  for (n=0;;++n) {
    int x=n*dist_250hz;
    if (x>=spect_size) break;
    set_color(cRed);
    spwin->draw_line(Point(x,y),Point(x,y+6));
    if (n==0)
      xft_draw_string(spwin->xft_win,xft_Black,Point(0,y+16),"0");
    else {
      char txt[20];
      if (n%4) {
        if (n%2==0) {
          sprintf(txt,"%.1f",n/4.);
          xft_draw_string(spwin->xft_win,xft_Black,Point(x-8,y+16),txt);
        }
        else if (bs>=2048) {
          sprintf(txt,"%.2f",n/4.);
          xft_draw_string(spwin->xft_win,xft_Black,Point(x-8,y+16),txt);
        }
      }
      else {
        sprintf(txt,"%dKHz",n/4);
        xft_draw_string(spwin->xft_win,xft_Black,Point(x-8,y+16),txt);
      }
    }
  }
}

void mouse_down(Id,int x,int y,int butnr) {
  switch (butnr) {
    case 2: {
      int n,
          bs=wav2s.buf_size,
          nr=gui->f_c_value(),
          sl_dist= bs==512 ? 8 : bs==8192 ? 2 : 4,
          sline_nr=x/sl_dist;         // spectrum line nr
      bool valid;
      Arr<uchar> lines=wav2s.spectdata[nr].spectlines;
      uchar val[3] = { lines[sline_nr-1],lines[sline_nr],lines[sline_nr+1] };
      SpectData &sd=wav2s.spectdata[nr];
      if (val[0]>2 && val[1]>2 && val[2]>2) {
        float peak=sline_nr-1+index_of_peak(val[0],val[1],val[2],valid);
        int midinr=wav2s.freq2mnr(peak);
        sd.midinr=midinr;
        sd.assigned_freq=peak;
        sd.asf_valid=true;
        gui->spectrum_win->clear(Rect(0,0,spect_size,5));
        draw_marks(sd,sl_dist,true);
      }
      else {
        sd.midinr=0;
        sd.asf_valid=true; //false;
        sd.assigned_freq=0.;
        gui->spectrum_win->clear(Rect(0,0,spect_size,5));
        draw_marks(sd,sl_dist,false);
      }
      gui->draw_info(nr);
    }
    break;
    case 1:
    case 3: {
      int nr=gui->f_c_value();
      if (butnr==1) { if (nr>0) --nr; else break; }
      else { if (nr<wav2s.nr_fft_samples-1) ++nr; else break; }
      if (gui->course) {
        int val=nr/100;
        set_text(gui->course->text,"%d",val*100);
        gui->course->set_hsval(val,false,true);   // not fire, draw
      }
      gui->fine->set_hsval(nr%100,true,true); // fire, draw
    }
    break;
  }
}
static void display_cmd(Rect,Id);

void Gui::draw_info(int fft_nr) {
  char txt[50],
       *p;
  info->reset();
  sprintf(txt,"fft-sample %d",fft_nr);
  info->add_text(txt);
  SpectData *sd=&wav2s.spectdata[fft_nr],
            *sd1;
  sprintf(txt,"measure %d",ao.time2time(fft_nr)/8);
  info->add_text(txt);
  info->add_text("midi-numbers:");
  for (p=txt,sd1=sd-2;sd1<wav2s.spectdata.buf+wav2s.nr_fft_samples && sd1<=sd+2;++sd1) {
    if (sd1<wav2s.spectdata.buf) continue;
    if (sd==sd1) *p++='[';
    if (sd1->midinr>0)
      p+=sprintf(p,"%d",sd1->midinr);
    else if (sd1->skipped)
      p+=sprintf(p,"-");
    else
      p+=sprintf(p,"?");
    if (sd==sd1) *p++=']';
    *p++=' ';
  }
  *p=0;
  info->add_text(txt,xft_Red);
  sprintf(txt,"max peak %d",sd->maxmag);
  info->add_text(txt);
  info->draw_text();
  if (wav2s.sv_win) {
    clear(wav2s.sv_win->svwin);
    display_cmd(Rect(0,0,0,0),0);
  }  
}

void Wav2Score::sel_wave(int xpos) {
  int fft_nr=min(nr_fft_samples-1,idiv(xpos*nr_fft_samples,ww_size));
  if (gui->course) {
    int val=fft_nr/100;
    set_text(gui->course->text,"%d",val*100);
    gui->course->set_hsval(val,false,true);   // not fire, draw
  }
  gui->fine->set_hsval(fft_nr%100,true,true); // fire, draw
}

void Wav2Score::eval_wavf_gui(FILE *wav) {
  int n;
  float *buffer1=new float[buf_size],
        *buffer2=new float[buf_size];
  short *sbuf1=new short[buf_size],
        *sbuf2=new short[buf_size];
  shortbuf=new short[nr_wav_samples*(stereo ? 2 : 1)];
  spectdata.init(nr_fft_samples);
  for (n=0;n<nr_fft_samples;++n)
    spectdata[n].spectlines.init(min(buf_size/2,spect_size/2));
  read_conf(false);
  if (fread((char*)shortbuf,data_size,1,wav)!=1) {
    alert("wave file: data size < %d",data_size);
    exit(1);
  }
  fclose(wav);
  fill_wavelines();
  int interval= stereo ? buf_size*2 : buf_size;
  for (called=0;called<nr_fft_samples;++called) {
    short *sbuf=shortbuf+called*interval/2; // sbuf overlaps half of preceeding sbuf
    if (stereo) {
      if (select_chan=='R') {
        for (n=0;n<buf_size;++n)
          sbuf1[n]=sbuf[n*2];
        for (n=0;n<buf_size;++n)
          buffer1[n]=sbuf1[n]*hamming[n];
        rfft(buffer1,buf_size/2,true);
        eval(buffer1,0);
      }
      else if (select_chan=='L') {
        for (n=0;n<buf_size;++n)
          sbuf1[n]=sbuf[n*2+1];
        for (n=0;n<buf_size;++n)
          buffer1[n]=sbuf1[n]*hamming[n];
        rfft(buffer1,buf_size/2,true);
        eval(buffer1,0);
      }
      else {
        for (n=0;n<buf_size;++n) {
          sbuf1[n]=sbuf[n*2];   // index should be even
          sbuf2[n]=sbuf[n*2+1]; // index should be odd
        }
        for (n=0;n<buf_size;++n) {
          buffer1[n]=sbuf1[n]*hamming[n];
          buffer2[n]=sbuf2[n]*hamming[n];
        }
        rfft(buffer1,buf_size/2,true);
        rfft(buffer2,buf_size/2,true);
        eval(buffer1,buffer2);
      }
    }
    else {
      for (n=0;n<buf_size;++n)
        sbuf1[n]=sbuf[n];
      for (n=0;n<buf_size;++n)
        buffer1[n]=sbuf1[n]*hamming[n];
      rfft(buffer1,buf_size/2,true);
      eval(buffer1,0);
    }
  }
}

static void draw_waves(Id) {
  set_width(1);
  for (int n=0;n<ww_size;++n) {
    if (n==wav2s.the_xpos2) wav2s.draw_waveline(wav2s.the_xpos2,eSelect2);
    else if (n==wav2s.the_xpos1) wav2s.draw_waveline(wav2s.the_xpos1,eSelect1);
    else wav2s.draw_waveline(n,0);
  }
}

void draw_fft_sample(Id) { wav2s.draw_fft_sample(gui->f_c_value()); }

void Wav2Score::draw_fft_sample(int nr) {
  static int mid=wwh/2,
             mid1=wwh/4,
             mid2=wwh*3/4,
             div=(stereo && !select_chan ? 0x20000 : 0x10000)/wwh;
  set_width_color(1,cWave);
  if (stereo) {
    short *sbuf=shortbuf+nr*buf_size; // buffers overlap
    Point points1[fftw_size],
          points2[fftw_size];
    for (int n=0;n<fftw_size;++n) {
      int ind=n*buf_size/fftw_size*2; // should be even
      if (select_chan=='R')
        points1[n].set(n,sbuf[ind]/div+mid);
      else if (select_chan=='L')
        points1[n].set(n,sbuf[ind+1]/div+mid);
      else {
        points1[n].set(n,sbuf[ind]/div+mid1);
        points2[n].set(n,sbuf[ind+1]/div+mid2);
      }
    }
    gui->fft_win->draw_lines(points1,fftw_size);
    if (!select_chan) gui->fft_win->draw_lines(points2,fftw_size);
  }
  else {
    short *sbuf=shortbuf+nr*buf_size/2; // buffers overlap
    Point points[fftw_size];
    for (int n=0;n<fftw_size;++n)
      points[n].set(n,sbuf[n*buf_size/fftw_size]/div+mid);
    gui->fft_win->draw_lines(points,fftw_size);
  }
}

void select_wave(Id,int xpos,int ypos,int butnr) {
  if (butnr==1) wav2s.sel_wave(xpos);
}

int Gui::f_c_value() {
  int val=fine->value();
  if (course) val+=100*course->value();
  return min(wav2s.nr_fft_samples-1,val);
}

void fine_course_cmd() {
  int fcval=gui->f_c_value();
  gui->spectrum_win->clear();
  draw_spectrum(0);
  gui->draw_info(fcval);
  int xpos=fcval * ww_size / wav2s.nr_fft_samples;
  if (wav2s.the_xpos1!=xpos) {
    if (wav2s.the_xpos1>=0)
      wav2s.draw_waveline(wav2s.the_xpos1,eUnselect1);
    wav2s.the_xpos1=xpos;
    wav2s.draw_waveline(xpos,eSelect1);
  }
  gui->fft_win->clear();
  wav2s.draw_fft_sample(fcval);
}

void fine_cmd(Id,int val,int fire,char*& txt,bool) { set_text(txt,"%d",val); fine_course_cmd(); }
void course_cmd(Id,int val,int,char*& txt,bool) { set_text(txt,"%d",val*100); fine_course_cmd(); }

void *wave_listen(void *arg) {
  wav2s.wav_listen(); 
  return 0;
}

void Wav2Score::wav_listen() {
  SndInterf *snd_interf=new SndInterf();
  short buffer[IBsize*2];
  if (snd_interf->okay) {
    int i,
        ind;
    bool stop=false;
    ind=gui->f_c_value() * buf_size/2;
    for (;!stop && !stop_requested;ind+=IBsize) {
      for (i=0;i<IBsize;++i) {
        if (stereo) {
          int val1=0,val2=0;
          if (ind+i < nr_wav_samples) {
            val1=shortbuf[(ind+i)*2];
            val2=shortbuf[(ind+i)*2+1];
          }
          else
            stop=true;
          if (select_chan=='R') buffer[2*i]=buffer[2*i+1]=val1;
          else if (select_chan=='L') buffer[2*i]=buffer[2*i+1]=val2;
          else { buffer[2*i]=val1; buffer[2*i+1]=val2; }
        }
        else {
          int val=0;
          if (ind+i < nr_wav_samples)
            val=shortbuf[ind+i];
          else
            stop=true;
          buffer[2*i]=buffer[2*i+1]=val;
        }
      }
      int xpos=ind * ww_size / nr_wav_samples;
      if (xpos!=the_xpos2 && xpos!=the_xpos1) {
        send_uev('updi',the_xpos2,xpos);
        the_xpos2=xpos;
      }
      snd_interf->snd_write(buffer);
    }
  }
  delete snd_interf;
  send_uev('arro'); // for play button
  i_am_playing=false;
}

void right_arrow(uint win,XftDraw*,Id,int,int) {
  const int x=6,y=9;
  static Point pts[3]={ Point(x,y-4),Point(x+6,y),Point(x,y+4) };
  fill_polygon(win,cBlack,pts,3);
}

void square(uint win,XftDraw*,Id,int,int) {
  fill_rectangle(win,cRed,Rect(6,6,6,7));
}

static void write_sco_cmd(Id) {
  int nr;
  ao.init();
  SpectData *sd;
  for (nr=0;nr<wav2s.nr_fft_samples;++nr) {
    sd=wav2s.spectdata.buf+nr;
    if (sd->skipped || !sd->midinr) ao.add_note(0,nr);
    else ao.add_note(sd->midinr,nr);
  }
  ao.add_note(0,nr); // the last note
  ao.close();
}

static void write_conff_cmd(Id) {  // write config file
  FILE *config;
  if (!(config=fopen(configf.s,"w")))
    alert("config file %s not written",configf.s);
  else
    wav2s.write_conf(config);
  fclose(config);
}

static void dis_sv_cmd(Id) {
  if (wav2s.sv_win) map_window(wav2s.sv_win->svwin);
  else wav2s.sv_win=new ScoreView(Point(400,450));
}

void play_cmd(Id) {
  if (!wav2s.i_am_playing) {
    wav2s.i_am_playing=true;
    wav2s.stop_requested=false;
    gui->play->label.draw=square;
    pthread_t play_thread;
    pthread_create(&play_thread, 0, wave_listen, 0); // if exit: wav2s.i_am_playing = false
  }
  else {
    wav2s.stop_requested=true;
    gui->play->label.draw=right_arrow;
  }
}

void draw_topw(XftDraw *xft_win,Rect expose_rect) {
  static char input_wave_title[100],
              fft_win_title[50];
  if (!input_wave_title[0]) {
    snprintf(fft_win_title,50,"FFT WINDOW (%d samples)",wav2s.buf_size);
    snprintf(input_wave_title,100,"INPUT WAVE (file: %s)",inf);
  }
  static Point pts[5]={
    Point(2,gui->spectrum_win->y-4),
    Point(2,gui->wave_win->y-4),
    Point(2,gui->info->y-4),
    Point(2,gui->play->y+14),
    Point(gui->fft_win->x,gui->fft_win->y-4)
  };
  static XftText txt[5]= {
    XftText(top_win->win,xft_win,"SPECTRUM",pts[0],BOLDfont),
    XftText(top_win->win,xft_win,input_wave_title,pts[1],BOLDfont),
    XftText(top_win->win,xft_win,"INFO",pts[2],BOLDfont),
    XftText(top_win->win,xft_win,"PLAY",pts[3],BOLDfont),
    XftText(top_win->win,xft_win,fft_win_title,pts[4],BOLDfont)
  };
  for (int i=0;i<5;++i) {
    if (a_in_b(expose_rect,Rect(pts[i].x,pts[i].y-10,200,12)))
      txt[i].draw();
  }
}

Gui::Gui() {
  int y=16;
  spectrum_win=new BgrWin(top_win->win,Rect(2,y,spect_size,100),FN,draw_spectrum,mouse_down,0,0,cLightYellow);
  wav2s.mult_spl=spectrum_win->dy-26;

  y+=124;
  int nr=wav2s.nr_fft_samples/100;
  if (nr>0) {
    fine=new HSlider(top_win,Rect(2,y,spect_size,0),FN,0,99,"FFT SAMPLE - fine","0","99",fine_cmd,cForeground);
    char *txt=new char[10];
    sprintf(txt,"%d",nr);
    course=new HSlider(top_win,Rect(2,y+42,minmax(40,nr*10+10,spect_size),0),FN,0,nr,"FFT SAMPLE - course","0",txt,course_cmd,cForeground);
  }
  else {
    nr=wav2s.nr_fft_samples;
    char *txt=new char[10];
    sprintf(txt,"%d",nr-1);
    fine=new HSlider(top_win,Rect(2,y,min(nr*10+10,spect_size),0),FN,0,nr-1,"FFT SAMPLE","0",txt,fine_cmd,cForeground);
    course=0;
  }

  y+=88;
  wav2s.wwh=wav2s.stereo && !wav2s.select_chan ? 160 :80;
  wave_win=new BgrWin(top_win->win,Rect(2,y,ww_size,wav2s.wwh),FN,draw_waves,select_wave,0,0,cWaveBgr);

  y+=wav2s.stereo && !wav2s.select_chan ? 182 : 102;
  info=new TextWin(top_win->win,Rect(2,y,100,0),FN,5);

  fft_win=new BgrWin(top_win->win,Rect(106,y,fftw_size,wav2s.wwh),FN,draw_fft_sample,0,0,0,cWaveBgr);

  y+=wav2s.wwh+4;
  play=new Button(top_win->win,Rect(40,y,20,20),FN,right_arrow,play_cmd);
  y+=2;
  button_style.set(1,cForeground,0);
  new Button(top_win->win,Rect(106,y,75,0),FN,"write score file: fft.sco",write_sco_cmd);
  y+=20;
  static char buf[100];
  snprintf(buf,100,"write configuration file: %s",configf.s);
  new Button(top_win->win,Rect(106,y,75,0),FN,buf,write_conff_cmd);
  y+=20;
  new Button(top_win->win,Rect(106,y,0,0),FN,"score display window",dis_sv_cmd);
}

void Wav2Score::write_conf(FILE *conf) {
  fprintf(conf,"wave_file = %s\n",inf);
  fprintf(conf,"win = %d\n",buf_size);
  fprintf(conf,"min = %d\n",min_midinr);
  fprintf(conf,"max = %d\n",max_midinr);
  fprintf(conf,"ch = %c\n",select_chan);
  fprintf(conf,"c = %.1f\n",mid_C);
  fprintf(conf,"th = %.3f\n",spect_thresh);
  fprintf(conf,"dur = %.3f\n",dur_inc);
  fprintf(conf,"signs = %d\n",ao.signsmode);
  fprintf(conf,"es = %d\n",use_each_sample);
  fprintf(conf,"assigned_freq =");
  SpectData *sd;
  int nr;
  for (nr=0;nr<nr_fft_samples;++nr) {
    sd=spectdata.buf+nr;
    if (sd->asf_valid)
      fprintf(conf," %d:%d,%.1f",nr,sd->midinr,sd->assigned_freq);
  }
  putc('\n',conf);
}

void Wav2Score::read_conf(bool initialize) {
  static FILE *conf;
  Str str;
  if (initialize) {
    if (!(conf=fopen(configf.s,"r"))) {
      alert("warning: config file %s not found",configf.s);
      return;
    }
    for (;conf;) {
      str.rword(conf," =");
      if (str=="assigned_freq") // rest later
        return;
      if (str=="wave_file") {
        str.rword(conf," \n");
        if (str!=inf) {
          alert("error: wave file mismatch in config file");
          fclose(conf); conf=0;
        }
      }
      else if (str=="win") {
        str.rword(conf," \n");
        int bs=atoi(str.s);
        if (wav2s.buf_size!=bs) {
          if (wav2s.buf_size && options) alert("%s: -win parameter set to %d",configf.s,bs);
          wav2s.buf_size=bs;
        }
      }
      else if (str=="th") {
        str.rword(conf," \n");
        float th=atof(str.s);
        if (th>0.001 && fabs(wav2s.spect_thresh/th-1) > 0.001) {
          if (options) alert("%s: -th parameter set to %.3f",configf.s,th);
          wav2s.spect_thresh=th;
        }
      }
      else if (str=="dur") {
        str.rword(conf," \n");
        float d=atof(str.s);
        if (d>0.01 && fabs(wav2s.dur_inc/d-1) > 0.001) {
          if (options) alert("%s: -dur parameter set to %.3f",configf.s,d);
          wav2s.dur_inc=d;
        }
      }
      else if (str=="ch") {
        str.rword(conf," \n");
        char ch=str.s[0];
        if (wav2s.select_chan!=ch) {
          if (options) alert("%s: -ch parameter set to %c",configf.s,ch);
          wav2s.select_chan=ch;
        }
      }
      else if (str=="es") {
        str.rword(conf," \n");
        bool es=atoi(str.s);
        if (es!=wav2s.use_each_sample) {
          if (options) alert("%s: -es set to %d",configf.s,es);
          wav2s.use_each_sample=es;
        }
      }
      else if (str=="min") {
        str.rword(conf," \n");
        wav2s.min_midinr=atoi(str.s);
      }
      else if (str=="max") {
        str.rword(conf," \n");
        wav2s.max_midinr=atoi(str.s);
      }
      else if (str=="c") {
        str.rword(conf," \n");
        wav2s.mid_C=atof(str.s);
      }
      else if (str=="signs") {
        str.rword(conf," \n");
        ao.signsmode=atoi(str.s);
      }
      else {
        alert("unknown keyword '%s' in config file",str.s);
        fclose(conf); conf=0;
      }
    }
  }
  if (!conf) {
    if (initialize) alert("config file error");
    return;
  }
  // so now at keyword 'assigned_freq'
  int nr,mnr,
      res;
  float asf;
  for (;;) {
    res=fscanf(conf," %d:%d,%f",&nr,&mnr,&asf);
    if (res!=3) break;
    SpectData &sd=spectdata[nr];
    sd.midinr=mnr;
    sd.assigned_freq=asf;
    sd.asf_valid=true;
  }
  fclose(conf);
}

static void display_cmd(Rect exp_rect,Id) {
  int i;
  const int fc_val=gui->f_c_value(),
            half=notes_max/2;
  SpectData *sd=&wav2s.spectdata[fc_val],
            *sd1;
  SubWin *sv_win=wav2s.sv_win->subw;
  for (i=0,sd1=sd-half;sd1<wav2s.spectdata.buf+wav2s.nr_fft_samples && sd1<=sd+half;++i,++sd1) {
    if (sd1<wav2s.spectdata.buf) continue;
    int x=i*note_len,
        y;
    if (sd1==sd) fill_rectangle(sv_win->win,cLightGrey,Rect(x,0,note_len,sv_win->dy));
    if (sd1->midinr>0) {
      y=sv_win->dy-(sd1->midinr-midi_min)*sclin_dist;
      draw_line(sv_win->win,3,cBlack,Point(x,y),Point(x+note_len,y));
    }
    else if (sd1->skipped);
    else {
      y=(midi_max-midi_min)/2*sclin_dist;
      draw_line(sv_win->win,1,cRed,Point(x,y),Point(x+note_len,y));
    }
  }
}

static void hide_diswin(Id) {
  hide_window(wav2s.sv_win->svwin);
}

ScoreView::ScoreView(Point top) {
  subw=new SubWin("score display",Rect(top.x,top.y,notes_max*note_len,(midi_max-midi_min)*sclin_dist),
                          true,cLightYellow,display_cmd,hide_diswin);
  svwin=subw->win;
};

void do_atexit() {
}

void handle_uev(int cmd,int par1,int par2) {
  switch (cmd) {
    case 'updi':  // update select pointer in wave display (par1 = previous xpos, par2 = xpos)
      if (par1>=0)
        wav2s.draw_waveline(par1,eUnselect2);
      wav2s.draw_waveline(par2,eSelect2);
      break;
    case 'arro':
      gui->play->label.draw=right_arrow;
      gui->play->draw_button();
      break;
    default: alert("handle_uev: cmd=%d",cmd);
  }
}

int main(int argc, char ** argv) {
  bool with_gui=true;
  int nr;
  for (int an=1;an<argc;++an) {
    if (argv[an][0]=='-') {
      if (!strcmp(argv[an],"-h")) {
        puts("Wav2score - create Amuc scorefile from WAVE file (mono or stereo, 44.1 KHz)"); 
        puts("Usage:");
        puts("  wav2score [options...] <wave file>");
        puts("Options:");
        puts("  -h           : print usage info and exit");
        puts("  -nogui       : command-line interface");
        puts("  -db          : debug");
        puts("  -ch L|R      : if stereo, process only left or right channel");
        puts("  -win <nr>    : fft window = <nr> samples (between 512 and 8192, default: 2048)");
        puts("  -min <nr>    : midi number of lowest fundamental (default: 0)");
        puts("  -max <nr>    : midi number of highest harmonic (default: dependant on -win parameter)");
        puts("  -c <value>   : midi number for middle-C, default: 48.0");
        puts("  -th <value>  : threshold level for spectrum lines, default: 0.2");
        puts("  -dur <value> : modify samples/note-duration, default: 1.0");
        puts("  -es          : use each sample:");
        puts("                 samples/note-duration factor such that each valid sample yields 1 note unit");
        puts("  -signs hi|lo : preference for sharp or flat accidentals in output score file");
        puts("Output file:");
        puts("  fft.sco");
        exit(0);
      }
      options=false;
      if (!strcmp(argv[an],"-gui"))
        err("option -gui is deprecated");
      if (!strcmp(argv[an],"-db"))
        debug=true;
      else if (!strcmp(argv[an],"-nogui"))
        with_gui=false;
      else if (!strcmp(argv[an],"-win")) {
        if (++an>=argc) err("-win parameter missing");
        nr=atoi(argv[an]);
        if (nr<500 || nr>8192) err("-win option = %d, expected 512 - 8192",nr);
        for (int shift=0;shift<5;++shift) {
          if (nr<(512<<shift)) {
            wav2s.buf_size=512<<shift; break;
          }
        }
      }
      else if (!strcmp(argv[an],"-min")) {
        if (++an>=argc) err("-min parameter missing");
        nr=atoi(argv[an]);
        if (nr<0 || nr>100) err("-min value out-of-range");
        wav2s.min_midinr=nr;
      }
      else if (!strcmp(argv[an],"-max")) {
        if (++an>=argc) err("-max parameter missing");
        nr=atoi(argv[an]);
        if (nr<0 ||nr>128) err("-max value out-of-range");
        wav2s.max_midinr=nr;
      }
      else if (!strcmp(argv[an],"-ch")) {
        if (++an>=argc) err("-ch parameter missing");
        wav2s.select_chan=argv[an][0];
        if (!strchr("LR",wav2s.select_chan)) err("-ch must be L or R");
      }
      else if (!strcmp(argv[an],"-c")) {
        if (++an>=argc) err("-c parameter missing");
        wav2s.mid_C=atof(argv[an]);
      }
      else if (!strcmp(argv[an],"-th")) {
        if (++an>=argc) err("-th parameter missing");
        float th=atof(argv[an]);
        if (th<0.05 || th>1.0) err("bad -th value (expected 0.05 - 1.0)");
        wav2s.spect_thresh=th;
      }
      else if (!strcmp(argv[an],"-dur")) {
        if (++an>=argc) err("-dur parameter missing");
        wav2s.dur_inc=atof(argv[an]);
        if (wav2s.dur_inc<0.1 || wav2s.dur_inc>100) err("bad -dur value");
      }
      else if (!strcmp(argv[an],"-signs")) {
        if (++an>=argc) err("-signs parameter missing");
        if (!strcmp(argv[an],"hi")) ao.signsmode=eHi;
        else if (!strcmp(argv[an],"lo")) ao.signsmode=eLo;
        else err("bad -signs value");
      }
      else if (!strcmp(argv[an],"-es")) {
        wav2s.use_each_sample=true;
      }
      else
        err("Unexpected option %s (use -h for help)",argv[an]);
    }
    else inf=argv[an];
  }
  if (!inf) err("no wave file (use -h for help)");
  if (argc>2 && with_gui) options=true;

  configf.cpy(inf);
  configf.new_ext(".w2s");
  if (with_gui) {
    init_xwindows();  // needed for alert messages
    wav2s.read_conf(true);
    if (!wav2s.buf_size) wav2s.buf_size=2048;
  }
  else {
    if (!wav2s.buf_size) wav2s.buf_size=2048;
  }
  wav2s.init();
  FILE *wav;
  if (!wav2s.read_wav_head(inf,wav)) exit(1);
  wav2s.nr_wav_samples=wav2s.data_size/(wav2s.stereo ? 4 : 2);
  wav2s.nr_fft_samples=max(1,wav2s.nr_wav_samples*2/wav2s.buf_size-1); // times 2, minus 1, because buffers overlap
  int total_height= wav2s.stereo && !wav2s.select_chan ? 636 : 478;

  if (with_gui) {
    //init_xwindows();
    cGreen=calc_color("#00C000");
    cWave=calc_color("#009000");
    cWaveBgr=calc_color("#D0E0F0"); // sky blue
    cLightYellow=calc_color("#FFFFD0");
    cFreqScale=calc_color("#E0E0B0");
    cLightGrey=calc_color("#D7D7D7");
    top_win=create_top_window("wav2score",Rect(100,0,total_width,total_height),false,draw_topw,cForeground);
    set_icon(top_win->win,create_pixmap(wav2s_icon).pm,16,16);
    button_style.set(0,0,1);
    slider_style.set(1,true);
    checkbox_style.set(1,0,0);
    gui=new Gui();
    wav2s.eval_wavf_gui(wav);
    if (wav2s.nr_fft_samples<=1)
      gui->fine->hide();
    else
      gui->fine->set_hsval(min(wav2s.nr_fft_samples/2,10),true,true);
    map_top_window();
    run_xwindows();
  }
  else {
    ao.init();
    wav2s.eval_wavf(wav);
    ao.close();
    const char *mode;
    if (wav2s.stereo) {
      if (wav2s.select_chan=='R') mode="stereo, right channel";
      else if (wav2s.select_chan=='L') mode="stereo, left channel";
      else mode="stereo";
    }
    else
      mode="mono";
    printf("%d FFT-samples (%s). Score file fft.sco created.\n",
      wav2s.nr_fft_samples,mode);
  }
  return 0;
}
