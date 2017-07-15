#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#include "amuc-headers.h"
#include "snd-interface.h"

static int tscale;

const int
  sbm=512,              // 256 is about okay, but less: no clean low tones
  amp_mul=20000,        // multiplier for instr amplitude
  phm_amp_mul1=20000,   // multiplier for phys model ampl (20000 = distorsion limit)
  at_scale=2,           // attack scaling
  ad_max=4,             // attack and decay
  // act_tempo=10 -> 100 beats/min = 100*4/60 note_units/sec -> tscale = 44100 * 10 / subdiv * 60 / 4 / 100
  voice_max=50,
  mk_voice_max=5,       // midi keyboard voices
  stereo_delay=88;      // delay = stereo_delay * 1/44KHz = 2 ms

const float
  phm_amp_mul2=2.,
  wave_amp_mul=1.5,     // multiplier for sample from wave file
  synth_amp_mul=0.3;    // multiplier for synth amplitude
float
  freq_scale,  // 0.5*sbm/SAMPLE_RATE,
  mid_c;       // freq_scale * 261.65;  // middle C frequency

bool sample_rate_changed=false; // maybe true if output_port = eJack

enum {
  eSleep=1, ePause, eNote, ePortaNote, eSampled, ePhysMod, eSynthNote // note types
};

enum {
  eStarting=1, eOcc, eDecaying  // keyboard notes
};

SndInterf *snd_interface;
JackInterf *jack_interface;
KeybTune kb_tune;
Synth *synths[colors_max];
void *start_sampler(void *arg);

static pthread_mutex_t mtx= PTHREAD_MUTEX_INITIALIZER;

static int minmax(int a,int x,int b,bool& clip) {
  if (x>b) { clip=true; return b; }
  if (x<a) { clip=true; return a; }
  return x;
}
static float fminmax(float a,float x,float b,bool& clip) {
  if (x>b) { clip=true; return b; }
  if (x<a) { clip=true; return a; }
  return x;
}
static int pad(int a,int b) { return a - a % b + b; }  // padding value a to value b

pthread_t thread1; 

void KeybTune::reset() { cur_ind=-1; turn_start=0; }
void KeybTune::nxt_turn() { turn_start+=tune_wid; }
KeybTune::KeybTune() { reset(); }
void KeybTune::add(int lnr,int sign,int snr1,int snr2,int col) {
  if (cur_ind>=keyb_notes_max-1) {
    alert("more then %d notes",keyb_notes_max);
    return;
  }
  ++cur_ind;
  KeybNote *kn=buf+cur_ind;
  kn->lnr=lnr;
  kn->sign=sign;
  kn->snr=snr1;
  kn->dur=max(1,snr2-snr1);
  kn->col=col;
}

struct CurrentValues {   // set by playScore(), used by MidiNoteBufs and play()
  int snr,        // current section nr
      snr_lazy,   // idem, updated when needed, used for sync with keyboard and display
      tcount,
      play_start; // set by fill_note_bufs()
} cur;

struct Sinus {
  int i_buf[sbm];
  float fl_buf[sbm];
  Sinus() {
    int i,f;
    for (i=0;i<sbm/2;++i) {
      f=i*8;
      i_buf[i]=f-(f*f/sbm/4);  // max: buf[i=sbm/4] = sbm
      i_buf[i+sbm/2]=-i_buf[i];
      float val=float(i_buf[i])/sbm;
      fl_buf[i]=val;
      fl_buf[i+sbm/2]=-val;
    }
  }
} sinus;

int i2ind(int i,const int dim) { return i<0 ? i+dim : i>=dim ? i-dim : i; }

void lowpass(float *in,float *out,const int dim,const int coff) {
  for (int i=0;i<dim;++i)
    out[i]= (in[i2ind(i-coff,dim)] + in[i] + in[i2ind(i+coff,dim)])/3;
    // out[i]= (in[i2ind(i-coff,dim)] - in[i] + in[i2ind(i+coff,dim)]);
    // out[i]= (in[i2ind(i-coff,dim)] + in[i]*2 + in[i2ind(i+coff,dim)])/4;
}

struct VarSinus {
  float buf[sbm];
  VarSinus(int formant,int nrsin) { // formant: 1,2,4,7 nrsin: 1-3
    for (int i=0;i<sbm;i++) {
      buf[i]=sinus.fl_buf[nrsin*8*i*sbm/(formant*sbm+(8-formant)*i)%sbm];
    }
  }
} *var_sinus[5][3];

struct Sound {
  float *wave,
        ampl;
  Sound() { }
  Sound(float* w,float a):wave(w),ampl(a) { }
};

struct Lfo {
  float freq_mult,
        val;
  Lfo():freq_mult(2.) { }  // nominal: 1Hz
  void update(float &ind_f) {
    ind_f += freq_mult*freq_scale; // late binding!
    int ind=static_cast<int>(ind_f);
    if (ind >= sbm) { ind-=sbm; ind_f-=sbm; }
    val=sinus.fl_buf[ind];
  }
};

struct Reverb {
  static const int taps=10,
                   phasing_ampl=40;
  float *buf,*buf2;
  int dim,
      cur_pos,
      revb_val; // 0 - 4
  const int *dpos;
  bool is_ph;   // phasing?
  double lp,lp2;
  float lfo_pos;
  Instrument *the_instr;
  Lfo lfosc;
  Reverb():
      is_ph(false),lfo_pos(0.),the_instr(0) {
    dim=set(4,0)+1+phasing_ampl;
    buf=new float[dim];
    buf2=new float[dim];
    clear_bufs();
  }
  void clear_bufs() {
    cur_pos=0; lp=lp2=0.;
    for (int i=0;i<dim;++i) buf[i]=buf2[i]=0.;
  }
  int set(int val,int phasing) {
    static const int fib[]={ 55,89,144,233,377,610,987,1597,2584,4181,6765,10946,17711,28657 };
    revb_val=val;
    is_ph= phasing>0;
    if (is_ph)
      lfosc.freq_mult= phasing==1 ? 1 : 2;
    dpos= val==1 || val==3 ? fib+1 : fib+3;
    return dpos[taps-1]; // needed to calculate buffer size
  }
  int i2ind(int i) { if (i>dim-1) i-=dim; else if (i<0) i+=dim-1; return i; }
  void process(float& in) {
    double val=0,val2=0;
    if (is_ph) {
      lfosc.update(lfo_pos);
      int diff=int(lfosc.val*phasing_ampl-float(rand())/RAND_MAX); // dither
      for (int i=0;i<taps;++i) {
        int ind=cur_pos + dpos[i];
        val+=buf[i2ind(ind + diff)];
        val2+= i&1 ? buf2[i2ind(ind - diff)]
                   : -buf2[i2ind(ind - diff)];
      }
      lp=lp*0.99 + val*0.01; lp2=lp2*0.99 + val2*0.01;
      val=val-lp; val2=val2-lp2;    // highpass to block DC component
      if (--cur_pos<0) cur_pos=dim-1;
      buf [cur_pos]=in + val*0.1;
      buf2[cur_pos]=in + val2*0.1;
      val=(val+val2)*0.5;
    }
    else {
      for (int i=0;i<taps;++i)
        val+=buf[i2ind(cur_pos + dpos[i])];
      lp=lp*0.99 + val*0.01;
      val=val-lp;           // highpass to block DC component
      if (--cur_pos<0) cur_pos=dim-1;
      buf[cur_pos]=in + val*0.05;
    }
    switch (revb_val) {
      case 1: case 2: in+=val*0.2; break;
      case 3: case 4: in=val*0.3; break;
    }
  }
} reverb;

struct Instrument {
  int attack,   // attack or start duration
      decay,    // decay duration
      s_loc,    // stereo location: eMid, eLeft, eRight, ...
      d_ind,    // index stereo delay buffer
      dur_lim;  // duration limit, no limit if 0
  float d_buf[stereo_delay], // stereo delay buffer
        *buf;   // [IBsize], temporary buf
  Sound *sbuf,  // steady-state tone
        *decay_data,
        *attack_data;
  Reverb *revb;
  Instrument(int att,int dec,int loc):
      attack(att),decay(dec),s_loc(loc),d_ind(0),dur_lim(0),
      buf(0),sbuf(0),decay_data(0),attack_data(0),revb(0) {
  }
  virtual void nxt_val(float,struct NBufBase*)=0;
  void set_decay(int n) {
    static const int scale[]={ 1,2,5,10,20,40 }; // decay value is 0 - 5
    decay=scale[n];
  }
  void set_attack(int n) {
    static const int scale[6]={ 0,1,2,5,10,20 }; // attack value is 0 - 5
    attack=scale[n];
  }
  void set_startup(int n) {
    static const int scale[6]={ 0,1,2,5,10,20 }; // attack value is 0 - 5
    attack=scale[n];
  }
  void set_durl(int n,char *text) {
    static const int dl[5]={ 0,1,2,4,8 };
    dur_lim=dl[n];
    if (dur_lim)
      set_text(text,"%d",dur_lim);
    else
      set_text(text,"no limit");
  }
  float delay(float val) {
    int ind=(d_ind+1)%stereo_delay;
    float out=d_buf[ind];
    d_ind=(++d_ind)%stereo_delay;
    d_buf[d_ind]=val;
    return out;
  }
  void clear_buffers() {
    for (int n=0;n<stereo_delay;++n) d_buf[n]=0.;
  }
};

void CtrlBase::set_revb(int val,int phasing) {
  if (reverb.the_instr) reverb.the_instr->revb=0;
  reverb.the_instr=inst;
  inst->revb=&reverb;
  reverb.set(val,phasing);
}

bool is_sampled(int cat) { return cat==eSampled || cat==ePhysMod; }

struct Note {
  uchar cat,
       note_col,
       note_gr,
       midi_nr,    // MIDI freq nr;
       midi_instr,
       note_lnr,note_sign,
       midi_ampl;
  bool first_entry,
       note_stop,
       ctr_pitch; // controlled pitch?
  CtrlBase *note_ctrl;
  int dur,
      dur1, // eNote: delta snr, ePause: pause * subdiv
      attack,
      decay,
      remain,  // remaining time, will be re-assigned
      act_snr, // section nr
      *ampl_ptr;
  float lst_ind, // last index in wave buffer
        freq;
  void set_timing(int d,int t) {
    dur1=d;
    dur=remain=t;
    note_stop=false;
  }
  void set(int as,uchar ncol,uchar ngr,CtrlBase *ctrl,float f,uchar lnr,uchar sign,bool sampled) {
    note_lnr=lnr; note_sign=sign;
    act_snr=as;
    note_col=ncol;
    note_gr=ngr;
    note_ctrl=ctrl;
    freq=f;
    first_entry=true;
    midi_instr=0;
    if (sampled) {
      cat=eSampled;
      if (app->task==eMidiout) {
        if (!(midi_instr=midi_out.col2smp_instr[note_gr][note_col]))
          alert("warning: unmapped midi perc. instr., color %s, group %d",
                color_name[note_col],note_gr);
      }
      ampl_ptr=ctrl->get_ampl_ptr(true,0);
      ctr_pitch=ctrl->ctrl_pitch->value;
      lst_ind=0.;
    }
    else {
      cat=eNote;
      if (app->task==eMidiout) {
        int mi=midi_out.col2midi_instr[ngr][ncol];
        if (mi<0) {
          alert("warning: unmapped midi instrument, color %s, group %d",color_name[ncol],ngr);
          mi=0;
        }
        midi_instr=mi;
      }
      midi_nr=lnr_to_midinr(lnr,sign);// needed for midi output and mono-synth
      ampl_ptr=ctrl->get_ampl_ptr(false,note_gr);
    }
    midi_ampl=midi_amplitude();
  }
  int midi_amplitude() {
    int mult=0;
    switch (cat) {
      case eNote:
      case ePortaNote:
      case eSynthNote: mult=500; break;
      case eSampled:
      case ePhysMod: mult=700; break;
      default: alert("midi_amp: cat=%d",cat);
    }
    return  minmax(1,int(ampl_mult[*ampl_ptr] * mult),127);
  }
};

Note* NOTES[voice_max];

struct MidiNote {
  int ev_time;
  Note *note;
  bool start; // note on?
  MidiNote(Note *n,int ev,bool st):ev_time(ev),note(n),start(st) { }
  bool operator<(MidiNote &other) {
    return ev_time < other.ev_time || (ev_time==other.ev_time && !start);
  }
  bool operator==(MidiNote &other) { return false; }   // always insert
};

SLinkedList<MidiNote> midi_events[groupnr_max][colors_max],
                      midi_perc_events;

struct NBufBase {
  float ind_f,ind_f2,ind_f3;
};

struct NoteBuffer:NBufBase {
  int busy,      // note duration + decay
      len,
      cur_note;
  Note *notes;
  CtrlBase *ctrl;
  NoteBuffer():
      len(20),
      notes(new Note[len]) {
    reset();
  }
  void reset();
  void renew();
  void report(int);
};

struct MidiNoteBuf:NBufBase {
  int lnr,sign,start_snr,
      occ,      // eOcc, eDecaying
      mn_col;
  float freq;
  Instrument *m_instr;
  MidiNoteBuf() { reset(); }
  void reset();
};

const float ampl_mult[ampl_max+1]={ 0,0.06,0.09,0.12,0.18,0.25,0.35,0.5,0.7,1.0 };  // index 0 - 9

struct iRed:Instrument {
  float *at,
        *b,
        b1,b2,
        at1;
  int tone_val;
  void set(int x,int y,int formant,int nr) {
    var_sinus[x][y]=new VarSinus(formant,nr);
  }
  iRed():Instrument(3,2,eRight) {
    set(0,0,7,2); set(0,1,7,3); set(0,2,7,5);
    set(1,0,5,2); set(1,1,5,3); set(1,2,5,5);
    set(2,0,3,2); set(2,1,3,3); set(2,2,3,5);
    set(3,0,2,2); set(3,1,2,3); set(3,2,2,5);
    set(4,0,1,2); set(4,1,1,3); set(4,2,1,5);
    at=var_sinus[3][1]->buf;
    b=var_sinus[2][1]->buf;
    sbuf=new Sound(&b1,1.0);
    float ad2[4]={ 1.0,1.0,1.0,1.0 };
    float dd[4]={ 1.0,0.6,0.4,0.2 };
    attack_data=new Sound[4];
    decay_data=new Sound[4];
    float *ad1[4]={ &at1,&at1,&b2,&b1 };
    for (int i=0;i<4;++i) {
      attack_data[i]=Sound(ad1[i],ad2[i]);
      decay_data[i]=Sound(&b1,dd[i]);
    }
  }
  void nxt_val(float freq,NBufBase *nb) {
    nb->ind_f += freq;
    int ind=static_cast<int>(nb->ind_f);
    if (ind >= sbm) { ind-=sbm; nb->ind_f-=sbm; }
    switch (tone_val) {
      case 0:
        at1=at[ind];
        b1=b[ind];
        break;
      case 1:
        nb->ind_f2 += freq*2.01;
        int ind2=static_cast<int>(nb->ind_f2);
        if (ind2 >= sbm) { ind2-=sbm; nb->ind_f2-=sbm; }
        at1=(at[ind] + at[ind2]) / 2;
        b1=(b[ind] + b[ind2]) / 2;
        break;
    }
    b2=(at1+b1)/2;
  }
  void set_start_timbre(int f,int n) {
    at=var_sinus[f][n]->buf;
  }
  void set_timbre(int f,int n) {
    b=var_sinus[f][n]->buf;
  }
  void set_start_ampl(int n) {  // n from 0 to 3
    static const float sa[4][3] = { { 0,0.5,0.7 },{ 0.5,0.7,0.9 },
                                  { 1.0,1.0,1.0 },{ 1.5,1.5,1.3 } };
    for (int i=0;i<3;++i) attack_data[i].ampl=sa[n][i];
  }
} ired;

struct FMinstr:Instrument {
  float fm_freq,
        fm_mod,
        detun,
        mm_val;
  float b;
  bool mm_enab; // fm-modulation modulation?
  int mult_freq;
  Lfo lfosc;
  FMinstr(int loc):Instrument(0,1,loc),
      fm_freq(3.0),
      fm_mod(2.0/sbm),
      detun(0),
      mm_enab(false),
      mult_freq(1) {
    float ad[]={ 0.2,0.5,0.7,1.0 };
    float dd[]={ 1.0,0.7,0.4,0.2 };
    attack_data=new Sound[4];
    decay_data=new Sound[4];
    sbuf=new Sound(&b,1.0);
    for (int i=0;i<4;++i) {
      attack_data[i]=Sound(&b,ad[i]); 
      decay_data[i]=Sound(&b,dd[i]); 
    }
  }
  void nxt_val(float freq,NBufBase *nb) {
    int fm,ind_fm,ind;
    freq*=mult_freq;
    nb->ind_f2 += (freq+detun)*fm_freq;
    ind_fm=static_cast<int>(nb->ind_f2);
    if (ind_fm >= sbm) { ind_fm-=sbm; nb->ind_f2-=sbm; }
    fm=sinus.i_buf[ind_fm];
    if (mm_enab) {
      lfosc.update(nb->ind_f3);
      nb->ind_f += freq*(1.0+fm*(fm_mod*(1.0+lfosc.val*mm_val)));
    }
    else
      nb->ind_f += freq*(1.0+fm*fm_mod);
    ind=static_cast<int>(nb->ind_f);
    if (ind>=sbm) { ind-=sbm; nb->ind_f-=sbm; }
    else if (ind<0) { ind+=sbm; nb->ind_f+=sbm; }
    b=sinus.fl_buf[ind];
  }
  void set_detune(int n) {
    static const float scale[6]={ 0,1,2.5,5,10,20 }; // detune value 0 - 5
    detun=scale[n]*freq_scale;
  }
} iblack(eLeft),ibrown(eRight);

void FMCtrl::setfm(int m) {
  static const float
    freq_arr[9]  = { 0.5, 1, 2, 3, 4, 5, 6, 7, 8 },
    index_arr[8] = { 0, 0.5, 1, 2, 3, 5, 7, 10 };
  static const int mf_arr[9]={ 2,1,1,1,1,1,1,1,1 };
  float index;
  FMinstr *instr=static_cast<FMinstr*>(inst);
  switch (m) {
    case eModFreq:
      instr->fm_freq=freq_arr[fm_ctrl->value.x+1];  // value: -1 - 7
      instr->mult_freq=base_freq->value ? mf_arr[fm_ctrl->value.x+1] : 1;
      set_text(fm_ctrl->text_x,"%.1f",instr->fm_freq);
      break;
    case eModIndex:
      index=index_arr[fm_ctrl->value.y];  // value: 0 - 7
      instr->fm_mod=index/sbm;
      set_text(fm_ctrl->text_y,"%.1f",index);
      break;
  }
}

void FMCtrl::set_mmod() {
  static const float
    mmval_arr[6]  = { 0, 0.2, 0.3, 0.4, 0.6, 0.8 },
    mmfreq_arr[4]= { 0.5, 1., 3., 10. };
  FMinstr *instr=static_cast<FMinstr*>(inst);
  instr->mm_val=mmval_arr[mod_mod->value.x];
  instr->mm_enab= mod_mod->value.x!=0;
  float mmfreq=mmfreq_arr[mod_mod->value.y];
  instr->lfosc.freq_mult=mmfreq;
  set_text(mod_mod->text_x,"%.1f",instr->mm_val);
  set_text(mod_mod->text_y,"%.1f",mmfreq);
}

void FMCtrl::set_attack() { inst->set_attack(attack->value()); }

void FMCtrl::set_decay() { inst->set_decay(decay->value()); }

void FMCtrl::set_detune() { static_cast<FMinstr*>(inst)->set_detune(detune->value()); }

void RedCtrl::set_startup() { ired.set_startup(startup->value()); }

void RedCtrl::set_decay() { ired.set_decay(decay->value()); }

void RedCtrl::set_start_amp() { ired.set_start_ampl(start_amp->value); }

void RedCtrl::set_start_timbre() {
  ired.set_start_timbre(start_timbre->value.x-1,start_timbre->value.y-2);  // value: 1 - 4, 2 - 4
}

void RedCtrl::set_timbre() {
  ired.set_timbre(timbre->value.x-1,timbre->value.y-2);  // value: 1 - 5, 2 - 4
}

void RedCtrl::set_tone() { ired.tone_val=tone->act_rbutnr(); }

struct iGreen:Instrument {
  float b,
        *wave1,*wave2;
  float fmult;
  iGreen():Instrument(1,3,eRight),
      wave1(var_sinus[1][1]->buf),
      wave2(var_sinus[2][0]->buf),
      fmult(2.01) {
    float ad[4]={ 0.2,0.5,0.7,1.0 };
    float dd[4]={ 1.0,0.7,0.4,0.2 };
    sbuf=new Sound(&b,1.0);
    attack_data=new Sound[4];
    decay_data=new Sound[4];
    for (int i=0;i<4;++i) {
      attack_data[i]=Sound(&b,ad[i]);
      decay_data[i]=Sound(&b,dd[i]);
    }
  }
  void set_timbre1(int f,int n) {
    wave1=var_sinus[f][n]->buf;
  }
  void set_timbre2(int f,int n) {
    wave2=var_sinus[f][n]->buf;
  }
  void nxt_val(float freq,NBufBase *nb) {
    nb->ind_f += freq;
    int ind=static_cast<int>(nb->ind_f);
    if (ind >= sbm) { ind-=sbm; nb->ind_f-=sbm; }
    nb->ind_f2 -= fmult*freq;
    int ind2=static_cast<int>(nb->ind_f2);
    if (ind2<0) { ind2+=sbm; nb->ind_f2+=sbm; }
    b=(wave1[ind] + wave2[ind2])/2;
  }
} igreen;

void GreenCtrl::set_attack() { igreen.set_attack(attack->value()); }

void GreenCtrl::set_decay() { igreen.set_decay(decay->value()); }

void GreenCtrl::set_timbre1() {
  igreen.set_timbre1(timbre1->value.x-1,timbre1->value.y-2);  // value: 1 - 3, 2 - 4
}

void GreenCtrl::set_timbre2() {
  igreen.set_timbre2(timbre2->value.x-1,timbre2->value.y-2);  // value: 1 - 3, 2 - 4
}

void GreenCtrl::set_freq_mult() {
  static const float fmul[4][4]={ { 1.001,1.501,2.001,3.001 }, // freq mult
                                  { 1.002,1.502,2.002,3.002 },
                                  { 1.003,1.503,2.003,3.003 },
                                  { 1.006,1.506,2.006,3.006 } };
  int ind1=freq_mult->value(),
      ind2=chorus->value();
  igreen.fmult=fmul[ind2][ind1];
  set_text(freq_mult->text,"%.1f",igreen.fmult);
}

struct Pulse {
  float puls[sbm],
        buf[sbm],
        buf3[sbm];
  int coff;  // lowpass cutoff freq
  Pulse():coff(20) {
    const float slope=0.032,
                bias=-0.25;
    int n,
        i1=int(1/slope),
        i2=4*i1,
        i3=12*i1;
    puls[0]=bias;
    for (n=1;n<i1;++n) puls[n]=puls[n-1]+slope;
    for (;n<i2;++n) puls[n]=1+bias;
    for (;n<i3;++n) puls[n]=puls[n-1]-slope/8;
    for (;n<sbm;++n) puls[n]=bias;
    fill_buf();
  }
  void fill_buf() {
    lowpass(puls,buf,sbm,coff);
    for (int n=0;n<sbm;++n) buf3[n]=(buf[n]+buf[2*n%sbm]-buf[3*n%sbm])*0.7;
  }
} pulse;

struct iBlue:Instrument {
  float b;
  void set_att_mode() {
    float ad[4]={ 0.25,0.5,1.0,1.0 };
    attack_data=new Sound[4];
    float dd[4]={ 1.0,0.7,0.3,0.1 };
    decay_data=new Sound[4];
    for (int i=0;i<4;++i) {
      attack_data[i]=Sound(&b,ad[i]);
      decay_data[i]=Sound(&b,dd[i]);
    }
  }
  iBlue():Instrument(0,4,eLeft) {
    sbuf=new Sound(&b,1.0);  // attack can be 0
    set_att_mode();
  }
  void set_p_attack(int n) {
    static const int scale[6]={ 0,5,10,20,50,100 }; // n is 0 - 5
    attack=scale[n];
  }
  void nxt_val(float freq,NBufBase *nb) {
    int ind;
    const float diff=freq_scale/2;
    static bool &chorus=app->blue_control->chorus->value,
                &rich=app->blue_control->rich->value;
    nb->ind_f += freq;
    ind=static_cast<int>(nb->ind_f);
    if (ind >= sbm) { ind-=sbm; nb->ind_f-=sbm; }
    if (chorus) {
      nb->ind_f2 -= freq + diff;
      int ind2=static_cast<int>(nb->ind_f2);
      if (ind2 < 0) { ind2 += sbm; nb->ind_f2 += sbm; }
      if (rich) b=pulse.buf3[ind] - pulse.buf3[ind2];
      else b=pulse.buf[ind] - pulse.buf[ind2];
    }
    else if (rich)
      b=pulse.buf3[ind];
    else
      b=pulse.buf[ind];
  }
} iblue;

void BlueCtrl::set_attack() { iblue.set_attack(attack->value()); }

void BlueCtrl::set_decay() { iblue.set_decay(decay->value()); }

void BlueCtrl::set_durlim() { iblue.set_durl(dur_limit->value(),dur_limit->text); }

void BlueCtrl::set_lowpass() {
  static int coff[5]={ 2,10,20,40,80 };
  pulse.coff=coff[lowpass->value()];
  pulse.fill_buf();
}

void RedCtrl::set_durlim() { ired.set_durl(dur_limit->value(),dur_limit->text); }

struct iPurple:Instrument {
  float *at,
        *b,
        at1,at2,b1;
  int snd_val;
  float st_ampl[harm_max],  // startup amplitudes
        ampl[harm_max];     // steady state
  const int *frm;
  Lfo lfosc;
  iPurple():Instrument(2,2,eLeft) {
    static const int freq_mult[harm_max]={ 1,2,3,6,9 };
    frm=freq_mult;
    st_ampl[0]=st_ampl[1]=st_ampl[2]=st_ampl[3]=st_ampl[4]=2;
    ampl[0]=ampl[1]=ampl[2]=ampl[3]=ampl[4]=1;
    float *bd[4]={ &at1,&at1,&at2,&b1 },
          dd[4]={ 0.4,0.3,0.2,0.1 };
    sbuf=new Sound(&b1,0.4);
    attack_data=new Sound[4];
    decay_data=new Sound[4];
    for (int i=0;i<4;++i) {
      attack_data[i]=Sound(bd[i],0.4);
      decay_data[i]=Sound(&b1,dd[i]);
    }
    lfosc.freq_mult=1;
  }
  void set_a(int *harmon,float *amp) {
    static const float scale_ampl[4]={ 0,0.25,0.5,1.0 };
    for (int i=0;i<harm_max;++i) amp[i]=scale_ampl[harmon[i]];
  }
  void nxt_val(float freq,NBufBase *nb) {
    int ind,ind2;
    const float diff=freq_scale/2.;
    float aa,bb;
    at=sinus.fl_buf;
    b=sinus.fl_buf;
    nb->ind_f += freq;
    ind=int(nb->ind_f);
    while (ind >= sbm) { ind-=sbm; nb->ind_f-=sbm; }
    switch (snd_val) {
      case 0:
        at1=b1=0;
        for (int i=0;i<harm_max;++i) {
          int ind3=ind*frm[i]%sbm;
          at1 += at[ind3] * st_ampl[i];
          b1  +=  b[ind3] * ampl[i];
        }
        at2=at1+b1;
        break;
      case 1:  // chorus
        nb->ind_f2 += freq * 1.001 + diff;
        ind2=int(nb->ind_f2);
        if (ind2 >= sbm) { ind2-=sbm; nb->ind_f2-=sbm; }
        at1=b1=0;
        for (int i=0;i<harm_max;++i) {
          int ind3=ind*frm[i]%sbm;
          at1 += at[ind3] * st_ampl[i];
          b1  += (b[ind3] + b[ind2*frm[i]%sbm]) * ampl[i];
        }
        at2=(at1+b1)/2;
        break;
      case 2:  // dist'ed
      case 3:  // dist'ed, detuned
        if (snd_val==2) {
          aa=1.2; bb=(1.-aa)/sbm;
          ind=int(aa*ind + bb*ind*ind);
        }
        else {
          lfosc.update(nb->ind_f3);
          aa=1.+lfosc.val/2.;        // 0.8 -> 1.2
          bb=(1.-aa)/sbm;
          ind=int(aa*ind + bb*ind*ind);
        }
        at1=b1=0;
        for (int i=0;i<harm_max;++i) {
          int ind3=ind*frm[i]%sbm;
          at1 += at[ind3] * st_ampl[i];
          b1  +=  b[ind3] * ampl[i];
        }
        at2=at1+b1;
        break;
    }
  }
} ipurple;

struct iSampled {
  int samp_loc;
  float *sample_buf;
  iSampled():samp_loc(eMid),sample_buf(0) { }
} sampled_instr[colors_max];

void PurpleCtrl::set_st_hs_ampl(int *ah) { ipurple.set_a(ah,ipurple.st_ampl); }

void PurpleCtrl::set_hs_ampl(int *h) { ipurple.set_a(h,ipurple.ampl); }

void PurpleCtrl::set_start_dur() { ipurple.set_startup(start_dur->value()); }

void PurpleCtrl::set_tone() { ipurple.snd_val=sound->act_rbutnr(); }

void PurpleCtrl::set_decay() { ipurple.set_decay(decay->value()); }

void CtrlBase::set_loc(int loc) { inst->s_loc=loc; }

void PhmCtrl::set_sampled_loc(int loc) { sampled_instr[ctrl_col].samp_loc=loc; }

Instrument *col2instr(int note_col) {
  switch (note_col) {
    case eBlack:  return &iblack;
    case eRed:    return &ired;
    case eGreen:  return &igreen;
    case eBlue:   return &iblue;
    case eBrown:  return &ibrown;
    case ePurple: return &ipurple;
    default: alert("col2instr: instr %d?",note_col); return &iblack;
  }
}

void MidiNoteBuf::reset() {
  occ=0;
  m_instr=0;
  ind_f=ind_f2=ind_f3=0.;
}

void NoteBuffer::reset() {
  busy=0;
  cur_note=-1;
  ctrl=0;
  ind_f=0;
  ind_f2=ind_f3=rand()%sbm;
}

void NoteBuffer::renew() {
  int old_len=len;
  len*=2;
  Note *new_notes=new Note[len];
  for (int i=0;i<old_len;i++) new_notes[i]=notes[i];
  delete[] notes;
  notes=new_notes;
}

void NoteBuffer::report(int n) {
  if (notes->cat==eSleep)
    return;
  printf("Voice %d\n",n);
  static const char *cat_name[]={ "","eSleep","ePause","eNote","ePortaNote","eSampled","ePhysMod","eSynthNote" };
  for (Note *np=notes;np-notes<=cur_note;np++) {
    printf("  cat=%s ",cat_name[np->cat]);
    if (np->cat!=eSleep) {
      printf("act_snr=%d dur=%d dur1=%d ",np->act_snr,np->dur,np->dur1);
      switch (np->cat) {
        case ePause:
          if (np->note_stop) printf("stop");
          break;
        case eNote:
        case ePortaNote:
          printf("freq=%0.2f col=%u group=%u attack=%d decay=%d ",
            np->freq,np->note_col,np->note_gr,np->attack,np->decay);
          break;
        case eSampled:
          printf("col=%u group=%u dur/tempo=%d",np->note_col,np->note_gr,np->dur/app->act_tempo);
          break;
        case ePhysMod:
          printf("col=%u group=%u dur/tempo=%d",np->note_col,np->note_gr,np->dur/app->act_tempo);
          break;
        case eSynthNote:
          printf("synth=%p freq=%0.2f col=%u group=%u ",
            np->note_ctrl->synth,np->freq,np->note_col,np->note_gr);
          break;
      }
    }
    putchar('\n');
  }
}

float line_freq(int lnr,int sign) {  // ScLine -> frequency
  static const float
    a=440, bes=466.2, b=493.9, c=523.3, cis=554.4, d=587.3,
    dis=622.3, e=659.3, f=698.5, fis=740.0, g=784.0, gis=830.6,

    F[sclin_max]= {
             b*8,a*8,g*4,f*4,e*4,d*4,c*4,
             b*4,a*4,g*2,f*2,e*2,d*2,c*2,
             b*2,a*2,g,  f,  e,  d,  c,
             b,  a,  g/2,f/2,e/2,d/2,c/2,  // F[27] = middle C
             b/2,a/2,g/4,f/4,e/4,d/4,c/4,
             b/4,a/4,g/8,f/8,e/8,d/8,c/8,
             b/8,a/8,g/16
           },
    Fhi[sclin_max]= {
             c*8,bes*8,gis*4,fis*4,f*4,dis*4,cis*4,
             c*4,bes*4,gis*2,fis*2,f*2,dis*2,cis*2,
             c*2,bes*2,gis,  fis,  f,  dis,  cis,
             c,  bes,  gis/2,fis/2,f/2,dis/2,cis/2,
             c/2,bes/2,gis/4,fis/4,f/4,dis/4,cis/4,
             c/4,bes/4,gis/8,fis/8,f/8,dis/8,cis/8,
             c/8,bes/4,gis/16
           },
    Flo[sclin_max]= {
             bes*8,gis*4, fis*4,e*4,dis*4,cis*4,b*4,
             bes*4,gis*2, fis*2,e*2,dis*2,cis*2,b*2,
             bes*2,gis,   fis,  e,  dis,  cis,  b,
             bes,  gis/2, fis/2,e/2,dis/2,cis/2,b/2,
             bes/2,gis/4, fis/4,e/4,dis/4,cis/4,b/4,
             bes/4,gis/8, fis/8,e/8,dis/8,cis/8,b/8,
             bes/8,gis/16,fis/16
           };
  switch (sign) {
    case 0: return F[lnr]*freq_scale;
    case eHi: return Fhi[lnr]*freq_scale;
    case eLo: return Flo[lnr]*freq_scale;
    default: alert("sign=%u?",sign); return F[lnr]*freq_scale;
  }
}

int same_color(ScSection *fst,ScSection *sec,ScSection *end,ScSection*& lst) {
  int n;
  uint col=sec->s_col, group=sec->s_group,
       sign=sec->sign,
       sampled=sec->sampled;
  lst=0;
  for (n=1;;++n,sec=fst) {
    for (;;sec=sec->nxt()) {
      if (sec->s_col==col && sec->s_group==group && sec->sign==sign && sec->sampled==sampled && sec->cat!=ePlay_x) { 
        sec->cat=ePlay_x; lst=sec;
        break;
      }
      if (!sec->nxt_note) return n-1;
    }
    if (sec->stacc || sec->sampled) return n;
    if (++fst>=end) return n;
    if (fst->cat==eSilent) return n;
    for (sec=fst;;sec=sec->nxt()) {
      if (sec->cat==ePlay) break;
      if (!sec->nxt_note) return n;
    }
  }
}

struct NoteBuffers {
  bool init;
  NoteBuffer nbuf[voice_max];
  int voice_end;
  void reset() {
    init=false;
    voice_end=-1;
    for (int i=0;i<voice_max;i++) nbuf[i].reset();
  }
  NoteBuffers() { reset(); }
  void find_free_voice(int col,int group,int start_del,int& v,int& pause,int cur_meas) {
    int i,n;
    if (!init) {
      init=true;
      pause=start_del-nbuf[0].busy; voice_end=v=0; return;
    }
    for (i=0;i<voice_max;++i) {
      n=nbuf[i].busy;
      if (n<=0) {
        pause=start_del-n; v=i; if (voice_end<v) voice_end=v;
        return;
      } // free voice found
    }
    if (cur_meas>=0)
      alert("voices > %d at measure %d",voice_max,cur_meas);
    else
      alert("voices > %d",voice_max);
    v=nop;
  }
  bool fill_note_bufs(int play_start,int play_stop,Score *);
} nbufs;

struct MidiNoteBufs {
  int mk_voice,
      key_arr[128]; // mapping midi nr -> nbuf index
  MidiNoteBuf nbuf[mk_voice_max];
  void reset() {
    int i;
    mk_voice=nop;
    for (i=0;i<128;++i) key_arr[i]=nop;
    for (i=0;i<mk_voice_max;i++) nbuf[i].reset();
  }
  MidiNoteBufs() {
    reset();
  }
  void keyb_note_on(int snr,int midi_nr,int col) {
    int ind;
    uchar lnr,sign;
    if (!midinr_to_lnr(midi_nr,lnr,sign,0)) return;
    for (ind=0;;++ind) {
      if (ind==mk_voice_max) return; // no warning
      if (!nbuf[ind].occ) break;
    }
    pthread_mutex_lock(&mtx);
    MidiNoteBuf *nb=nbuf+ind;
    nb->occ=eStarting;
    nb->start_snr=snr;
    nb->lnr=lnr;
    nb->freq=line_freq(lnr,sign);
    nb->sign=sign;
    nb->mn_col=col;
    nb->m_instr=col2instr(col);
    key_arr[midi_nr]=ind;
    pthread_mutex_unlock(&mtx);
  }
  void keyb_note_off(int snr,int midi_nr) {
    pthread_mutex_lock(&mtx);
    if (key_arr[midi_nr]>nop) {
      MidiNoteBuf *nb=nbuf+key_arr[midi_nr];
      nb->occ=eDecaying;
      kb_tune.add(nb->lnr,nb->sign,nb->start_snr,snr,nb->mn_col);
    }
    key_arr[midi_nr]=nop;
    pthread_mutex_unlock(&mtx);
  }
} mk_nbufs;

void keyb_noteOn(int midi_nr) {
  mk_nbufs.keyb_note_on(cur.snr_lazy+kb_tune.turn_start,midi_nr,app->act_color);
}

void keyb_noteOff(int midi_nr) {
  mk_nbufs.keyb_note_off(cur.snr_lazy+kb_tune.turn_start,midi_nr);
}

void MidiOut::make_midi() { // supposed: task = eMidiout
  Note *note;
  int voice,
      col,
      gr,
      nr_tracks=0,
      nr;
  for (voice=0;voice<=nbufs.voice_end;++voice) {
    for (note=NOTES[voice];note;++note) {
      switch (note->cat) {
        case ePause:
          break;
        case eSleep:
          goto next_voice;
        case eSampled:
        case ePhysMod:
          midi_perc_events.insert(MidiNote(note, note->act_snr, true), true);
          break;
        case eNote:
        case ePortaNote:
        case eSynthNote:
          midi_events[note->note_gr][note->note_col].insert(MidiNote(note, note->act_snr, true), true);
          midi_events[note->note_gr][note->note_col].insert(MidiNote(note, note->act_snr+note->dur1, false), true);
          break;
      }
    }
    next_voice:;
  }
  for (gr=0;gr<groupnr_max;++gr)
    for (col=0;col<colors_max;++col)
      if (midi_events[gr][col].lis) ++nr_tracks;
  if (midi_perc_events.lis)
    ++nr_tracks;
  init2(nr_tracks+1); // track nr 1 used for initialisation
  write_track1();
  close_track();
  SLList_elem<MidiNote> *mn;
  for (nr=0,gr=0;gr<groupnr_max;++gr)
    for (col=0;col<colors_max;++col) {
      mn=midi_events[gr][col].lis;
      if (mn) {
        init_track(++nr,gr,col);
        for (;mn;mn=mn->nxt) {
          note=mn->d.note;
          note_onoff(mn->d.start, col, gr, note->midi_instr,
                     false, mn->d.ev_time, note->midi_nr, note->midi_ampl);
        }
        close_track();
        midi_events[gr][col].reset();
      }
    }
  mn=midi_perc_events.lis;
  if (mn) {
    init_perc_track(++nr);
    for (;mn;mn=mn->nxt) {
      note=mn->d.note;
      note_onoff(mn->d.start, note->note_col, note->note_gr, note->midi_instr,
                 true, mn->d.ev_time, note->midi_nr, note->midi_ampl);
    }
    close_track();
    midi_perc_events.reset();
  }
  close();
  app->dia_wd->dlabel("midi file created",cForeground);
}

void PostscriptOut::create_postscript(Score *score) { // supposed: task = ePsout or eAbcout
  Note *note;
  int voice;
  set(app->act_meter,app->nupq,app->mv_key_nr,app->title());

  for (voice=0;voice<=nbufs.voice_end;++voice) {
    for (note=NOTES[voice];note;++note) {
      switch (note->cat) {
        case ePause:
          break;
        case eSleep:
          goto next_voice;
        case eSampled:
        case ePhysMod:
          insert_perc(note->note_col,note->note_gr,note->act_snr);
          break;
        case eNote:
        case ePortaNote:
        case eSynthNote:
          insert(note->note_col,note->note_gr,note->act_snr,note->note_lnr,note->note_sign,note->dur1);
          break;
      }
    }
    next_voice:
    continue;
  }
  write_ps(app->task==eAbcout);
  reset_ps();
  if (app->task==ePsout)
    app->dia_wd->dlabel("postscript file created",cForeground);
  else if (app->task==eAbcout)
    app->dia_wd->dlabel("abc file created",cForeground);
}

bool NoteBuffers::fill_note_bufs(int play_start,int play_stop,Score *score) {
  int samecol=0,
      v,   // voice
      lnr,snr,
      pause,
      decay;
  bool no_durlim= app->task==eMidiout || app->task==ePsout || app->task==eAbcout,
       no_readwav=no_durlim;
  ScSection *sec,*lst_sect;
  Note *note;
  NoteBuffer *lst_nb=0;
  reset();
  cur.play_start=play_start;

  for (snr=0;snr<play_start && snr<score->len;++snr) // initialisation
    if (!exec_info(snr,score,false)) return false;

  for (snr=play_start;snr<score->len;++snr) {
    if ((play_stop>0 && snr>play_stop) || (score->end_sect && snr==score->end_sect)) {
      kb_tune.tune_wid=snr-play_start;
      break;
    }
    if (!exec_info(snr,score,false))  // there might be timing, decay modifications
      return false;  
    for (lnr=0;lnr<sclin_max;lnr++) {
      ScLine *const sc_line=&score->lin[lnr];
      sec=score->get_section(lnr,snr);
      if (sec->cat==ePlay || sec->cat==ePlay_x) {
        for (;sec;sec=sec->nxt()) {
          if (sec->cat==ePlay &&
              (app->play_1_col<0 || (sec->s_col==(uint)app->play_1_col && sec->sampled==app->sampl->value))) {
            find_free_voice(sec->s_col,sec->s_group,sec->del_start,v,pause,snr/app->act_meter); // uses: busy
            if (v==nop) break;
            NoteBuffer *const nb=nbuf+v;
            lst_nb=nb;
            samecol=same_color(sc_line->sect+snr,sec,sc_line->sect+score->len,lst_sect);
            nb->ctrl=sec->sampled ? col2phm_ctrl(sec->s_col) : col2ctrl(sec->s_col);
            const Instrument *inst=nb->ctrl->isa[sec->s_group]->inst;
            if (inst->dur_lim>0 && !no_durlim) {
              samecol=min(samecol,inst->dur_lim);
            }
            nb->busy=samecol*subdiv + lst_sect->del_end;
            if (pause > 0) {
              if (++nb->cur_note >= nb->len) nb->renew();
              note=nb->notes + nb->cur_note;
              note->cat=ePause;
              note->set_timing(pause,tscale*pause);
            }
            if (++nb->cur_note>=nb->len) nb->renew();
            note=nb->notes+nb->cur_note;
            note->set(
              snr,
              sec->s_col,sec->s_group,
              nb->ctrl,
              line_freq(lnr,sec->sign),
              lnr,sec->sign,
              sec->sampled
            );
            if (note->cat==eSampled) {
              if (no_readwav) {
                note->set_timing(1,0);
                nb->busy=0;
              }
              else {
                int dur;
                if (nb->ctrl->sample_mode==eWaveMode) {
                  int size=wave_buf.w_buf[sec->s_col].size;
                  if (!size) {
                    alert("%s: no wave data",color_name[sec->s_col]);
                    return false;
                  }
                  if (size<0 || size>100*SAMPLE_RATE) {
                    alert("wave data corrupted? (size=%d)",size);
                    return false;
                  }
                  dur=size * app->act_tempo;
                }
                else if (nb->ctrl->sample_mode==ePhysmodMode) {
                  note->cat=ePhysMod;
                  dur=phm_buf.var_data[sec->s_col]->size * app->act_tempo;
                }
                else {
                  alert("fill_note_bufs: unexpected sample_mode %d",nb->ctrl->sample_mode);
                  dur=0;
                }
                if (note->ctr_pitch) dur=pad(int(dur * mid_c / note->freq),tscale);
                else if (sample_rate_changed) dur=pad(int(float(dur) * SAMPLE_RATE / 44100),tscale);
                else dur=pad(dur,tscale);
                nb->busy=dur / tscale + sec->del_start;
                note->set_timing(1,dur);
                note->attack=note->decay=0;
              }
            }
            else {  // note->cat = eNote
              if (note->note_ctrl->instrm_val==eSynthMode) {
                note->cat=eSynthNote;
                int dur=samecol * subdiv - sec->del_start + lst_sect->del_end - 1; // -1: staccato notes separated
                nb->busy=dur + sec->del_start;
                note->set_timing(samecol,dur*tscale);
                note->attack=note->decay=0;
              }
              else {
                if ((!inst->dur_lim || no_durlim) && lst_sect->port_dlnr)
                  note->cat=ePortaNote;
                note->set_timing(samecol,tscale * (samecol * subdiv - sec->del_start + lst_sect->del_end));
                if (inst->attack)
                  note->attack=min(tscale * inst->attack / at_scale,note->dur);
                else note->attack=0;
                if (note->cat==ePortaNote) {
                  int dlnr,
                      dsnr,
                      new_snr=snr,
                      new_lnr=lnr;
                  ScLine *new_sc_line;
                  Note *new_note;
                  for (;;) {
                    dlnr=lst_sect->port_dlnr;
                    dsnr=lst_sect->port_dsnr;
                    new_snr+=samecol+dsnr;
                    new_lnr+=dlnr;
                    new_sc_line=&score->lin[new_lnr];
                    for (sec=new_sc_line->sect+new_snr;sec;sec=sec->nxt())
                      if (sec->cat==ePlay && sec->s_col==note->note_col && sec->s_group==note->note_gr) break;
                    if (sec && sec->cat==ePlay) {
                      note->decay=tscale * (dsnr * subdiv - lst_sect->del_end + sec->del_start);
                      samecol=same_color(new_sc_line->sect+new_snr,sec,new_sc_line->sect+score->len,lst_sect);
                      if (++nb->cur_note>=nb->len) nb->renew();
                      new_note=nb->notes+nb->cur_note;
                      new_note->set(
                        new_snr,note->note_col,note->note_gr,note->note_ctrl,
                        line_freq(new_lnr,sec->sign),
                        new_lnr,sec->sign,
                        false // cannot be sampled
                      );
                      if (inst->dur_lim>0 && !no_durlim) {
                        samecol=min(samecol,inst->dur_lim);
                      }
                      else if (lst_sect->port_dlnr)
                        new_note->cat=ePortaNote;
                      new_note->set_timing(samecol,
                                           tscale * (samecol*subdiv - sec->del_start + lst_sect->del_end));
                      new_note->attack=0;
                      if (new_note->cat==eNote) {
                        if (inst->decay) {
                          if (lst_sect->stacc && inst->dur_lim==0) { decay=1; new_note->decay=tscale; }
                          else {
                            decay=inst->decay; 
                            new_note->decay=tscale * decay;
                          }
                        }
                        else
                          new_note->decay=decay=0;
                        nb->busy=(new_snr-snr+samecol)*subdiv+decay;
                        break;
                      }
                    }
                    else {
                      alert("unterminated portando");
                      note->decay=tscale;
                      nb->busy=(new_snr-snr+samecol)*subdiv+1;
                      break;
                    }
                    note=new_note;
                  }
                  if (!sec) break; // unterminated portando?
                }
                else if (inst->decay) {
                  if (lst_sect->stacc && inst->dur_lim==0) {
                    nb->busy+=1; note->decay=tscale;
                  }
                  else {
                    decay=inst->decay; nb->busy+=decay;
                    note->decay=tscale * decay;
                  }
                }
                else note->decay=0;
              }
            }
          }
        }
      }
    }
    for (v=0;v<voice_max;++v) nbuf[v].busy-=subdiv;
  }
  if (!lst_nb) {
    alert("empty score");
    return false;
  }

  NoteBuffer *nb1;
  if (play_stop>0 || score->end_sect>0) {
    find_free_voice(0,0,0,v,pause,nop);   // add pause note at end of last NoteBuffer
    if (v>nop) {
      nb1=nbuf+v;
      if (++nb1->cur_note>=nb1->len) nb1->renew();
      note=nb1->notes+nb1->cur_note;
      note->cat=ePause;
      note->set_timing(pause,pause*tscale);
      if (app->repeat) note->note_stop=true;
    }
  }
  for (v=0;v<=voice_end;++v) { // all note bufs end with cat=eSleep
    nb1=nbuf+v;
    if (++nb1->cur_note>=nb1->len) nb1->renew();
    nb1->notes[nb1->cur_note].cat=eSleep;
  }
  if (debug) {
    puts("-----------");
    for (v=0;v<=nbufs.voice_end;++v) nbuf[v].report(v);
  }
  for (v=0;v<=nbufs.voice_end;++v) NOTES[v]=nbufs.nbuf[v].notes; // nbuf[v] may have been realloced
  exec_info(0,score,true);  // maybe initialize timing
  return true;
}

void restore_marked_sects(Score* sc,int play_start) {  // restore ePlay_x notes
  ScSection *sec;
  for (int lnr=0;lnr<sclin_max;lnr++) {
    ScSection *const sect=sc->get_section(lnr,0);
    for (int snr=play_start;snr<sc->len;++snr) {
      for (sec=sect+snr;sec;sec=sec->nxt()) {
        if (sec->cat==ePlay_x) sec->cat=ePlay;
      }
    }
  }
}

bool init_phmbuf() { return phm_buf.init(phm_amp_mul1); }

void App::playScore(int play_start,int play_stop) {
  cur.snr=cur.snr_lazy=cur.tcount=0;

  if (!repeat) {
    for (int n=0;n<colors_max;++n) {
      col2instr(n)->clear_buffers();
      col2ctrl(n)->synth->init(false);
      synths[n]=0;
    }
    reverb.clear_bufs();
  }
  else {
    for (int n=0;n<colors_max;++n) {
      col2ctrl(n)->synth->init(true);
    }
  }
  bool ok=nbufs.fill_note_bufs(play_start,play_stop,cur_score);
  restore_marked_sects(cur_score,play_start);
  if (ok) {
    if (task==eMidiout) {
      midi_out.make_midi();
      task=0;
    }
    else if (task==ePsout || task==eAbcout) {
      ps_out.create_postscript(cur_score);
      task=0;
    }
    else if (task==eDumpwav) {
      pthread_create(&thread1, 0, start_sampler, 0);
    }
    else if (output_port==eAlsa)
      pthread_create(&thread1, 0, start_sampler, 0);
    else if (output_port==eJack) { 
      send_uev('repm',cur.snr);
      if (jack_interface)
        i_am_playing=true;
      else { alert("jack not running?"); i_am_playing=false; }
    }
    else alert("task?");
  }
  else {
    if (task==eMidiout) {
      midi_out.close();
      for (int gr=0;gr<groupnr_max;++gr)
        for (int col=0;col<colors_max;++col)
          midi_events[gr][col].reset();
      midi_perc_events.reset();
    }
    else if (task==ePsout || task==eAbcout) {
      ps_out.reset_ps();
    }
    i_am_playing=false;
    task=0;
  }
}

bool play(float *jack_buf_left,float *jack_buf_right) {
  Score *score=app->cur_score;
  int n,col,
      ind_instr_buf;  // index for instrument buffers
  float tmp_buf_r[IBsize],
        tmp_buf_l[IBsize],
        tmp_buf_sc[IBsize],
        value,
        mix;

  Sound *snd1,*snd2;
  Note *note;
  NoteBuffer *nbuf;
  MidiNoteBuf *mk_nbuf;
  Instrument *instr;
  iSampled *samp_ins;
  Synth *the_synth;
  int tempo=app->act_tempo;
  int stop_req=1;
  ShortBuffer *phm=0,
              *wav=0;
  n=cur.tcount * tempo * IBsize / tscale / subdiv;
  if (cur.tcount==0 || n>cur.snr_lazy) // needed for keyboard tune
    cur.snr_lazy=n;
  ++cur.tcount;
  for (col=0;col<colors_max;++col) {      // reset instr->buf's
    instr=col2instr(col);
    samp_ins=sampled_instr+col;
    for (n=0;n<IBsize;++n) {
      instr->buf[n]=0.;
      samp_ins->sample_buf[n]=0.;
    }
  }
  if (mk_connected)
    for (int mk_voice=0;mk_voice<mk_voice_max;++mk_voice) { // midi keyboard
      mk_nbuf=mk_nbufs.nbuf+mk_voice;
      pthread_mutex_lock(&mtx);
      if (mk_nbuf->occ) {
        instr=mk_nbuf->m_instr;
        for (n=0;n<IBsize;++n) {
          instr->nxt_val(mk_nbuf->freq,mk_nbuf);
          snd1=instr->sbuf;
          switch (mk_nbuf->occ) {
            case eStarting:
              value=snd1->wave[0] * n / IBsize;
              break;
            case eDecaying:
              value=snd1->wave[0] * (IBsize-n) / IBsize;
              break;
            default:
              value=snd1->wave[0];
          }
          int ampval=col2ctrl(mk_nbuf->mn_col)->ampl_val[0].value;
          instr->buf[n]+=value * ampl_mult[ampval];
        }
        switch (mk_nbuf->occ) {
          case eStarting: mk_nbuf->occ=eOcc; break;
          case eDecaying: mk_nbuf->occ=0; break;
        }
      }
      pthread_mutex_unlock(&mtx);
    }
  for (int voice=0;voice<=nbufs.voice_end;++voice) {
    nbuf=nbufs.nbuf+voice;
    ind_instr_buf=-1;
    note=NOTES[voice];
    loop_start:
    ++ind_instr_buf;
    switch (note->cat) {
      case eSleep:
        break;
      case ePause:
        if (stop_req!=2) stop_req=0;
        for (;ind_instr_buf<IBsize;++ind_instr_buf) {
          n=note->remain;
          if (n > 0) note->remain-=tempo;
          else if (note->note_stop)
            stop_req=2;
          else {
            note = ++NOTES[voice];
            note->remain += n;
            goto loop_start;
          }
        }
        break;
      case eSampled:
      case ePhysMod: {
          if (stop_req!=2) stop_req=0;
          if (cur.snr < note->act_snr)
            for (++cur.snr;;++cur.snr) {
              if (!exec_info(cur.snr,score,true)) { app->stop_requested=true; break; }
              if (cur.snr % app->act_meter == 0 && cur.snr>=cur.play_start)  // update measure nr display
                send_uev('repm',cur.snr);
              if (cur.snr == note->act_snr) break;
            }
          if (note->cat==eSampled) 
            wav=wave_buf.w_buf+note->note_col;
          else if (note->cat==ePhysMod)
            phm=phm_buf.var_data[note->note_col];
          else { alert("play sampled: cat=%d",note->cat); return false; }
          float cnt_f,
                *sb=sampled_instr[note->note_col].sample_buf;
          const float inc_f=note->ctr_pitch ? note->freq/mid_c : sample_rate_changed ? 44100./SAMPLE_RATE : 1.,
                      amp=ampl_mult[*note->ampl_ptr] * (note->cat==eSampled ? wave_amp_mul : phm_amp_mul2) / amp_mul;
          for (cnt_f=note->lst_ind;
               ind_instr_buf<IBsize;
               ++ind_instr_buf,cnt_f+=inc_f) {
            int ind=int(cnt_f);
            if (note->cat==eSampled) {
              if (ind>=0 && ind<wav->size)
                sb[ind_instr_buf] += wav->buf[ind] * amp;
            }
            else {
              if (ind>=0 && ind<phm->size)
                sb[ind_instr_buf] += phm->buf[ind] * amp;
            }
            n=note->remain;
            if (n > 0) note->remain-=tempo;
            else {
              note = ++NOTES[voice];
              note->remain += n;
              goto loop_start;
            }
          }
          note->lst_ind=cnt_f;
        }
        break;
      case eNote:
      case ePortaNote:
      case eSynthNote:
        if (stop_req!=2) stop_req=0;
        if (cur.snr < note->act_snr) {
          for (++cur.snr;;++cur.snr) {
            if (!exec_info(cur.snr,score,true)) { app->stop_requested=true; break; }
            if (cur.snr % app->act_meter == 0 && cur.snr>=cur.play_start)  // update measure nr display
              send_uev('repm',cur.snr);
            if (cur.snr == note->act_snr) break;
          }
        }
        the_synth= note->cat==eSynthNote ? note->note_ctrl->synth : 0;
        if (note->first_entry) {
          note->first_entry=false;
          if (the_synth) {
            synths[note->note_col]=the_synth;
            the_synth->set_values(note->midi_nr,note->freq/freq_scale/2.);
          }
        }
        instr=note->note_ctrl->isa[note->note_gr]->inst;

        for (;ind_instr_buf<IBsize;++ind_instr_buf) {
          if (the_synth) {
            n=note->remain;
            if (n > 0) note->remain-=tempo;
            else {
              the_synth->note_off();
              note = ++NOTES[voice];
              note->remain += n;
              goto loop_start;
            }
            continue;
          }
          if (note->remain>=0) {
            instr->nxt_val(note->freq,nbuf);
            if (note->attack) {
              div_t d=div(max(0,ad_max * (note->dur - note->remain)),note->attack);
              n=d.quot;
              if (n >= ad_max-1) {
                snd1=instr->attack_data + ad_max-1;
                value=snd1->wave[0] * snd1->ampl;
              }
              else {
                snd1=instr->attack_data + n;
                snd2=snd1+1;
                mix=float(d.rem) / note->attack;
                value=snd1->wave[0] * snd1->ampl * (1.-mix) + snd2->wave[0] * snd2->ampl * mix;
              }
            }
            else {
              snd1=instr->sbuf;
              value=snd1->wave[0] * snd1->ampl;
            }
          }
          else if (note->cat==ePortaNote) {
            Note *new_note=note+1;
            if (note->decay) {
              mix=float(-note->remain)/note->decay;
              instr->nxt_val(note->freq*(1.0-mix) + new_note->freq*mix,nbuf);
            }
            else instr->nxt_val(note->freq,nbuf);
            snd1=instr->sbuf;
            value=snd1->wave[0] * snd1->ampl;
          }
          else if (note->decay) {
            instr->nxt_val(note->freq,nbuf);
            div_t d=div(max(0,-ad_max*note->remain),note->decay);
            n=d.quot;
            snd1=instr->decay_data + n;
            mix=float(d.rem)/note->decay;
            if (n<ad_max-1) {
              snd2=snd1+1;
              value=snd1->wave[0] * snd1->ampl * (1.-mix) + snd2->wave[0] * snd2->ampl * mix;
            }
            else if (n==ad_max-1)
              value=snd1->wave[0] * snd1->ampl * (1.-mix);
            else value=0;
          }
          else
            value=0;
          note->note_ctrl->inst->buf[ind_instr_buf]+=value*ampl_mult[*note->ampl_ptr];
            // own buffer is used, not the isa buffer, so stereo location will be kept
          n=note->remain + note->decay;
          if (n > 0) note->remain-=tempo;
          else {
            if (note->cat!=ePortaNote) {
              nbuf->ind_f=0.; // ind_f2 not reset
            }
            note = ++NOTES[voice];
            note->remain += n;
            goto loop_start;
          }
        }
        break;
      default:
        alert("unknown note %d",note->cat);
    }
  }
  float val,
        val_r,val_l,
        val_sc;  // for scope
  for (n=0;n<IBsize;n++)
    tmp_buf_sc[n]=tmp_buf_r[n]=tmp_buf_l[n]=0.;

  for (col=0;col<colors_max;++col) { // read instr->buf's
    instr=col2instr(col);
    samp_ins=sampled_instr+col;
    if (synths[col]) {  // any synthesizer active?
      if (!synths[col]->fill_buffer(instr->buf,IBsize,synth_amp_mul)) synths[col]=0;
    }
    if (instr->revb && instr->revb->revb_val) {
      for (n=0;n<IBsize;++n)
        instr->revb->process(instr->buf[n]);
    }
    for (n=0;n<IBsize;++n) {
      val_sc=val=samp_ins->sample_buf[n];
      switch (samp_ins->samp_loc) {
        case eRRight:
          val_r=val; val_l=0;
          break;
        case eRight:
          val_r=val; val_l=val*0.7;
          break;
        case eLeft:
          val_l=val; val_r=val*0.7;
          break;
        case eLLeft:
          val_l=val; val_r=0;
          break;
        default:
          val_l=val_r=val;
      }
      val=instr->buf[n];
      val_sc+=val;
      switch (instr->s_loc) {
        case eRRight:
          val_r+=val;
          break;
        case eRight:
          val_r+=val; val_l+=instr->delay(val_r)*0.7;
          break;
        case eLeft:
          val_l+=val; val_r+=instr->delay(val_l)*0.7;
          break;
        case eLLeft:
          val_l+=val;
          break;
        default:
          val_l+=val*0.8; val_r+=val*0.8; //instr->delay(val*0.8);
          break;
      }
      tmp_buf_r[n]+=val_r;
      tmp_buf_l[n]+=val_l;
      tmp_buf_sc[n]+=val_sc; // scope
    }
  }
  bool clipped=false;
  if (output_port==eAlsa || app->task==eDumpwav) {
    short buffer[IBsize*2];
    for (n=0;n<IBsize;++n) {
      buffer[2*n+1]=minmax(-30000,int(amp_mul*tmp_buf_r[n]),30000,clipped);
      buffer[2*n]  =minmax(-30000,int(amp_mul*tmp_buf_l[n]),30000,clipped);
    }
    if (app->task==eDumpwav) {
      if (!dump_wav((char*)buffer,IBsize*4)) {
        alert("dump wave problem");
        app->task=0;
      }
    }
    else 
      snd_interface->snd_write(buffer);
  }
  else { // output_port = eJack
    for (n=0;n<IBsize;++n) {
      jack_buf_left[n] =fminmax(-1.,tmp_buf_r[n],1.,clipped);
      jack_buf_right[n]=fminmax(-1.,tmp_buf_l[n],1.,clipped);
    }
  }
  if (clipped) send_uev('clip');
  for (n=0;n<IBsize;n+=8) {
    // scopewindow: height=2*32
    app->scopeView->insert(int(minmax(-20000,int(amp_mul*tmp_buf_sc[n]),20000)/700));
  }
  send_uev('scop');
  if (app->stop_requested)
    return false;
  if (stop_req)
    return false;
  return true;
}

void *start_sampler(void *arg) {  // used if output_port = eAlsa, or if task = eDumpwav
  if (app->task==0) {
    snd_interface=new SndInterf();
    if (!snd_interface->okay) {
      snd_interface=0;
      i_am_playing=false;
      return 0;
    }
    send_uev('repm',cur.snr);
    i_am_playing=true;
    while (play(0,0));
    delete snd_interface;
    snd_interface=0;
  }
  else { // task = eDumpwav
    while (play(0,0));
  }
  send_uev('done');
  return 0;
}

bool play_wfile(float *buf_left,float *buf_right) {
  if (output_port!=eJack) { alert("play_wfile: output_port?"); wf_playing=false; return false; }
  if (!wf_playing) return false;
  bool stop=false;
  int i,ind=0;
  float ind_f;
  const float inc_f=sample_rate_changed ? 44100./SAMPLE_RATE : 1.;
  for (i=0,ind_f=0;i<IBsize;++i,ind_f+=inc_f) {
    ind=int(ind_f);
    if (wl_buf.bpos+ind < wl_buf.size) {
      float val=wl_buf.buf[wl_buf.bpos+ind]/30000.;
      buf_left[i]=buf_right[i]=val;
      if (i % 8 == 0)
        app->scopeView->insert(int(amp_mul*val/700));
    }
    else {
      stop=true;
      break;
    }
  }
  wl_buf.bpos+=ind;
  send_uev('scop');
  if (!stop && !app->stop_requested) return true;
  wf_playing=false;
  return false;
}

void *wave_listen(void *arg) {  // used if output_port = eAlsa
  if (output_port!=eAlsa) { alert("wave_listen: output_port?"); i_am_playing=false; return 0; }
  SndInterf *snd_interf=new SndInterf();
  if (!snd_interf->okay) {
    snd_interface=0;
    i_am_playing=false;
    return 0;
  }
  short buffer[IBsize*2];
  int i,
      ind;
  bool stop=false;
  i_am_playing=true;
  for (ind=0;!stop && !app->stop_requested;ind+=IBsize) {
    for (i=0;i<IBsize;++i) {
      int val=0;
      if (ind+i < wl_buf.size)
        val=wl_buf.buf[ind+i]/2;
      else
        stop=true;
      buffer[2*i]=buffer[2*i+1]=val;
      if (i % 8 == 0)
        app->scopeView->insert(val/700);
    }
    send_uev('scop');
    snd_interf->snd_write(buffer);
  }
  delete snd_interf;
  i_am_playing=false;
  return 0;
}

void set_time_scale() {
  sample_rate_changed= SAMPLE_RATE!=44100;
  tscale=SAMPLE_RATE/2;
  freq_scale=0.5 * sbm / SAMPLE_RATE;
  mid_c=261.65 * freq_scale;  // middle C frequency, 1 octave lower then standard
}

void set_buffer_size() {
  iblack.buf=new float[IBsize];
  ired.buf=new float[IBsize];
  iblue.buf=new float[IBsize];
  igreen.buf=new float[IBsize];
  ipurple.buf=new float[IBsize];
  ibrown.buf=new float[IBsize];
  for (int n=0;n<colors_max;++n)
    sampled_instr[n].sample_buf=new float[IBsize];
}

void reset_mk_nbuf() { mk_nbufs.reset(); }
