/*
 * Token from: Xsynth - a real-time software synthesizer
 * Copyright (C) 1999 S. J. Brookes
 * Adapted for Amuc (the Amsterdam Music Composer) by W.Boeke
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "amuc-headers.h"

enum {  // same order as control sliders
  VCO1_pit,
  VCO1_pw,
  VCO1_fm,
  VCO2_pit,
  VCO2_pw,
  VCO3_pit,
  VCO3_ma,
  VCO3_filt,
  LFO_fr,
  LFO_pw,
  LFO_ampl,
  LFO_pit,
  LFO_filt,  // 10
  BAL_o1,
  BAL_o2,
  BAL_ns,
  EG1_att,
  EG1_dec,
  EG1_sus,
  EG1_rel,
  EG1_lfo,
  PORT_tim,
  EG2_att,   // 20
  EG2_dec,
  EG2_sus,
  EG2_rel,
  EG2_pit,
  EG2_filt,
  VCF_coff,
  VCF_q,
  VOL=NSLIDER-1
};

enum {  // same order as checkboxes
  VCO_trig,
  LFO_synced,
  VCF_clip
};

enum {  // same order as radio buttons
  VCO1_wf,
  VCO2_wf,
  LFO_wf,
  Four_pole,
  FilterMode
};

enum {
  LOpass,
  BANDpass,
  HIpass
};

const int
  patch_max=100,
  rrate=16,      // recalculation rate of freq etc. (bigger value may yield clicking attack sound)
  DontDis=2;    // don't display

const float
  FREQMAX=0.825,  // filter only stable to this frequency
  PI2=6.28,
  eg_topval=0.9,  // EG1, EG2 top value
  eg_diffval=0.05;

const Rect
    vcf_disp_area(575,280,10,80),
    eg1_disp_area(375,225,70,30),
    eg2_disp_area(475,225,70,30),
    vco1_disp_area(56,80,60,20),
    vco2_disp_area(56,350,60,20),
    vco3_disp_area(150,80,60,20),
    lfo_disp_area(296,80,60,20);
static Array<struct Synth*,colors_max> monoSynth;
static RButWinData patch_rbutwin_data;

static float fminmax(float a, float x, float b) { return x>b ? b : x<a ? a : x; }
//static float fmin(float a, float b) { return a<b ? a : b; }
//static float fmax(float a, float b) { return a>b ? a : b; }

struct PatchData {
  char *pname,
       *patch;
};

static Array<PatchData,patch_max> pdat;
static int lst_patch=-1;

struct Patches {
  Button *buttons[4];
  RButWin *rbutwin;
  VScrollbar *scroll;
  Patches(Point,WinBase *pwin,int col);
  static void scroll_cmd(Id id,int val,int,bool);
};

float sinus(float x) {
  if (x>0.5) { x-=0.5; return -8*x*(1-2*x); }
  if (x<0.5) return 8*x*(1-2*x);
  return 0;
}

struct WaveSrc {
  int* triggered;
  WaveSrc():triggered(0) { }
};

struct SinusWav:WaveSrc {
  SinusWav():WaveSrc() { }
  float get(float freq,float& pos,bool *trig) {
    if (trig) *trig=false;
    float res=0;
    if (pos>1) {
      if (trig) *trig=true;
      if (triggered && *triggered) { // triggered = 1, 2 or 3
        if (pos>3) res=0;
        else if (pos>2) res= *triggered>=3 ? sinus(pos-2) : 0;
        else res= *triggered>=2 ? sinus(pos-1) : 0;
      }
      else { pos-=1.; res=sinus(pos); }
    }
    else {
      if (pos<0.) pos+=1.;
      res=sinus(pos);
    }
    pos+=freq;
    return res;
  }
};

float triangle(float pos,float pt1,float pt2,float dy1,float dy2) {
  if (pos<pt1) return pos*dy1;
  if (pos>pt2) return -1.+(pos-pt2)*dy1;
  return 1.-(pos-pt1)*dy2;
}  

struct TriWav:WaveSrc {  // triangle wave
  TriWav():WaveSrc() { }
  float pt1,pt2,
        dy1,dy2;
  void set(float asym) { // asym: 0.05 -> 0.95
    pt1=asym/2;
    pt2=1.-pt1;
    dy1=1./pt1;
    dy2=2./(pt2-pt1);
  }
  float get(float freq,float& pos,bool *trig) {
    if (trig) *trig=false;
    float res=0;
    if (pos>1) {
      if (trig) *trig=true;
      if (triggered && *triggered) {
        if (pos>3) res=0;
        else if (pos>2) res= *triggered>=3 ? triangle(pos-2,pt1,pt2,dy1,dy2) : 0;
        else res= *triggered>=2 ? triangle(pos-1,pt1,pt2,dy1,dy2) : 0;
      }     
      else { pos-=1.; res=triangle(pos,pt1,pt2,dy1,dy2); }
    }
    else {
      if (pos<0.) pos+=1.;
      res=triangle(pos,pt1,pt2,dy1,dy2);
    }
    pos+=freq;
    return res;
  }
};

float square(float pos,float pt1) {
  static const float edge=0.01,
                     dy=1.4/0.01;
  if (pos<edge) return -0.7+pos*dy;
  if (pos<pt1) return 0.7;
  if (pos<pt1+edge) return 0.7-(pos-pt1)*dy;
  return -0.7;
}

struct SquareWav:WaveSrc {  // squarewave
  float pt1;
  SquareWav():WaveSrc() { }
  void set(float asym) {
    pt1=asym;
  }
  float get(float freq,float& pos,bool *trig) {
    if (trig) *trig=false;
    float res=0;
    if (pos>1) {
      if (trig) *trig=true;
      if (triggered && *triggered) {
        if (pos>3) res=-0.7;
        else if (pos>2) res= *triggered>=3 ? square(pos-2,pt1) : -0.7;
        else res= *triggered>=2 ? square(pos-1,pt1) : -0.7;
      }
      else { pos-=1.; res=square(pos,pt1); }
    }
    else {
      if (pos<0.) pos+=1.;
      res=square(pos,pt1);
    }
    pos+=freq;
    return res;
  }
};

struct Flutter {
  float pos1,pos2,pos3;
  static const float mult2=1.7,
                     mult3=2.7;
  Flutter():pos1(0),pos2(0),pos3(0) { }
  float get(float freq) {
    pos1+=freq; if (pos1>1.) pos1-=1.;
    pos2+=freq*mult2; if (pos2>1.) pos2-=1.;
    pos3+=freq*mult3; if (pos3>1.) pos3-=1.;
    return (sinus(pos1)+sinus(pos2)+sinus(pos3))/3.;
  }
  void reset() { pos1=pos2=pos3=0.; }
};
/*
float rnd() {
  static int m_white,m_seed;
  m_seed   = (m_seed * 196314165) + 907633515;
  m_white  = (m_seed >> 9) | 0x40000000; 
  return float(m_white)/0x40000000; 
};
*/
struct Noise {
  static const int
    pre_dim=100,
    dim=100000; // this value is sufficient for percieving true randomness
  float pre_buf[pre_dim],
        white_buf[dim],
        pink_buf[dim];
  Noise() {
    float white,
          b1=0,b2=0;
    for (int i=-pre_dim;i<dim;++i) { // such that end and begin of buf are matching
      white= i<dim-pre_dim ? float(rand())/RAND_MAX - 0.5 : pre_buf[i-dim+pre_dim];
      white-=(b1+b2)/20.; // DC blocker
      b1 =0.98 * b1 + white * 0.4; // correct from about 100 Hz
      b2 =0.6 * b2 + white;
      if (i<0)
        pre_buf[i+pre_dim]=white; 
      else
        pink_buf[i]=b1 + b2;
/*
      b0 = 0.998 * b0 + white * 0.1; <-- correct from 9 Hz, from: musicdsp.org/files/pink.txt
      b1 = 0.963 * b1 + white * 0.3;
      b2 = 0.57 * b2 + white;
      buf[i] = b0 + b1 + b2 + white * 0.18;
*/
    }
    for (int i=0;i<dim;++i) {
      white= i<dim-pre_dim ? float(rand())/RAND_MAX - 0.5 :
                             pre_buf[dim-i-1];
      b2 = 0.6 * b2 + white;
      white_buf[i]=2 * b2;
    }
  }
} noise;
 
struct MSynth {
  struct Values {
    int dete[NRBUT],        // radio buttons
        filter_mode,        // LO, BAND, HI
        trig_val;           // value of VCO2 trigger
    bool on_off[NCHECKB];   // check boxes
    float cont[NSLIDER];    // sliders
    bool is_porta,          // portamento?
         ringm_vco,         // ring modulation by vco3?
         ringm_lfo,         // ring modulation by lfo?
         is_fm,             // VCO2 fm-modulated by VCO1?
         is_pole4,          // VCF 4-pole?
         eg1_attack_0, eg2_attack_0;
    SinusWav sin_wav[LFO_wf+1];
    TriWav tri_wav[LFO_wf+1];
    SquareWav square_wav[LFO_wf+1];
    Flutter flutter_wav;
    Values() {
      tri_wav[VCO2_wf].triggered=    // set pointer triggered to address of HSlider value
      square_wav[VCO2_wf].triggered=
      sin_wav[VCO2_wf].triggered=&trig_val;
    }
  } values,
    *isa;
  int note_on,
      note_diff,          // 1 if new note higher, -1 if lower
      cur_patch,
      note,prev_note,
      lfo_waveform,
      eg1_phase,
      eg2_phase,
      osc1_waveform,
      osc2_waveform,
      osc3_waveform,
      noise_ind;
  float fund_pitch,the_pitch,
        osc_freq4,lfo_pw,
        lfo_amount_a,lfo_amount_o,lfo_amount_f,
        osc1_pos,osc2_pos,osc3_pos,lfo_pos,
        vco3_amount_a,vco3_amount_f,
        fm_amount,
        eg1,
        eg1_amount_o,
        eg2,
        eg2_amount_o,
        osc_freq1,osc1_pw,
        osc_freq2,osc2_pw,
        osc_freq3,osc3_pw,
        bal_o1,bal_o2,bal_ns,
        freq,freqcut,freqkey,freqeg2,qres,
        d1,d2,d3,d4,
        cutoff_val, vco1_val, vco2_val;  // to be displayed
  MSynth();
  void reset(bool soft);
  void set_values(int,float freq);
  bool fill_buffer(float *buf,const int buf_size,const float amp_mul);
  float oscillator(int vco_nr,float& pos,float osc_freq,int waveform,bool *trig=0);
  void calc_eg(float &eg,int &eg_phase,int EG_att,int EG_dec,int EG_sus,int EG_rel);
  float get_noise() {
    if (++noise_ind>=noise.dim) noise_ind=0;
    return noise.pink_buf[noise_ind];
  }
};

void setpatch_cmd(Id id,int nr,int fire) {
  Synth *synth=monoSynth[id.id1];
  MSynthData ms_data;
  if (ms_data.fill_msynth_data(id.id1,pdat[nr].patch)) {
    synth->update_values(&ms_data,true);
    synth->m_synth->cur_patch=nr;
  }
}

void Synth::button_cmd(Id id) {
  int i;
  XftColor *textcol;
  FILE *fp;
  char patch_file[max100],
       buf[max100],buf2[max100];
  PatchData *pd;

  switch (id.id2) {
    case 'eq':
      isa_col=(isa_col+1)%colors_max;
      draw_isa_col(true);
      set_isa(monoSynth[isa_col]);
      break;
    case 'ldp':
      snprintf(patch_file,max100,"%s/monosynth-patches",cur_dir);
      fp=fopen(patch_file,"r");
      if (fp) textcol=xft_Black;
      else {
        snprintf(patch_file,max100,"%s/monosynth-patches",amuc_data);
        fp=fopen(patch_file,"r");
        if (fp) textcol=xft_Blue;
        else {
          alert("file monosynth-patches not found in %s or in %s",cur_dir,amuc_data);
          return;
        }
      }
      lst_patch=-1;
      set_patch->rbutwin->empty();
      for (i=0;;++i) {
        if (i>=patch_max-1) { alert("patches > %d",patch_max); break; }
        char form[20];
        sprintf(form,"%%%ds %%%ds\\n",max100,max100); // form = "%100s %100s\n"
        if (fscanf(fp,form,buf,buf2) != 2)
          break;
        pdat[i].pname=strndup(buf,50);
        pdat[i].patch=strndup(buf2,max100);
        lst_patch=i;
        set_patch->rbutwin->add_rbut(pdat[i].pname,textcol);
      }
      fclose(fp);
      set_patch->scroll->set_range((lst_patch+2)*TDIST);
      set_patch->scroll->set_ypos(0);
      set_patch->rbutwin->no_actb(); // no active button
      break;
    case 'modp':
      pd=&pdat[set_patch->rbutwin->act_rbutnr()];
      dump_settings(buf,max100,"");
      pd->patch=strndup(strchr(buf,':')+1,max100);
      break;
    case 'addp':
      if (lst_patch>=patch_max-2) { alert("patches > %d",patch_max); return; }
      ++lst_patch;
      pd=&pdat[lst_patch];
      pd->pname=(char*)"NEW";
      set_patch->rbutwin->set_rbut(set_patch->rbutwin->add_rbut("NEW"),0); // add and make active
      m_synth->cur_patch=set_patch->rbutwin->act_rbutnr();
      set_patch->scroll->set_range((lst_patch+2)*TDIST);
      set_patch->scroll->set_ypos((lst_patch-4)*TDIST);
      dump_settings(buf,max100,""); // buf: "set  msynth:F1_435 ..."
      pd->patch=strndup(strchr(buf,':')+1,max100);
      break;
    case 'wrp':
      if (lst_patch<0) {
        alert("0 patches");
        break;
      }
      snprintf(patch_file,max100,"%s/monosynth-patches",cur_dir);
      if ((fp=fopen(patch_file,"w"))==0) {
        alert("file %s not opened",patch_file);
        break;
      }
      for (i=0;i<=lst_patch;++i) 
        fprintf(fp,"%-20s %s\n",pdat[i].pname,pdat[i].patch);
      fclose(fp);
      alert("file: %s",patch_file);
      break;
    default: alert("button_cmd: %d",id.id2);
  }
}

static void but_cmd(Id id) { monoSynth[id.id1]->button_cmd(id); }

void monosynth_uev(int cmd,int id) {  // user event: display vcf cutoff
  Synth *synth=monoSynth[id];
  MSynth *msynth=synth->m_synth;
  BgrWin *bg=synth->vcf_display;
  switch (cmd) {
    case 'vcfd': {
        static const int scale=12;
        static float sr=log(SAMPLE_RATE);
        int vco1_val=int(scale*(log(msynth->vco1_val)+sr)),
            vco2_val=int(scale*(log(msynth->vco2_val)+sr)),
            cutoff_val=int(scale*log(msynth->cutoff_val)),
            dy=vcf_disp_area.height,
            y1=dy-vco1_val+30,
            y2=dy-vco2_val+30,
            y4=y1-cutoff_val;
        bg->clear();
        set_width_color(2,cBlack);
        bg->draw_line(Point(0,y1),Point(5,y1));
        bg->draw_line(Point(5,y2),Point(10,y2));
        set_color(cRed);
        bg->draw_line(Point(0,y4),Point(10,y4));
      }
      break;
    case 'clvd':
      bg->clear();
      break;
  }
}

Patches::Patches(Point pt,WinBase *pwin,int col) {
  button_style.set(1,cForeground,0);
  buttons[0]=new Button(pwin->win,Rect(pt.x,pt.y-6,80,14),FN,"load patches",but_cmd,Id(col,'ldp'));
  buttons[1]=new Button(pwin->win,Rect(pt.x,pt.y+12,80,14),FN,"update this patch",but_cmd,Id(col,'modp'));
  buttons[2]=new Button(pwin->win,Rect(pt.x,pt.y+30,80,14),FN,"add current patch",but_cmd,Id(col,'addp'));
  buttons[3]=new Button(pwin->win,Rect(pt.x,pt.y+48,80,14),FN,"save patches to '.'",but_cmd,Id(col,'wrp'));
  button_style=def_but_st;
  rbutwin=new RButWin(pwin,Rect(pt.x+135,pt.y,130,90),FN,"patches",false,setpatch_cmd,cForeground,Id(col));
  rbutwin->buttons=&patch_rbutwin_data;
  scroll=new VScrollbar(pwin->win,Rect(pt.x+258,pt.y,0,90),FN,10*TDIST,scroll_cmd,Id(col));
}

void Patches::scroll_cmd(Id id,int val,int,bool) {
  Synth *synth=monoSynth[id.id1];
  synth->set_patch->rbutwin->set_y_off(val);
}

void hide_swin(Id id) {
  monoSynth[id.id1]->map_topwin(false);
}

void draw_panel(Rect exp_rect,Id id) {
  Synth *mono_synth=monoSynth[id.id1];
  uint topw=mono_synth->topwin;
  XftDraw *xft_topw=mono_synth->xft_topwin;
  int i;
  static uint cDarkGrey=calc_color("#505050"),
              cBgText=calc_color("#707070");
  static const int
    xtext[10]={ 30 ,30 ,150,270,150,370,470,590,260,130 }, // labels
    ytext[10]={ 15 ,282,15 ,15 ,357,15 ,15 ,15 ,390,282 },
    xbg[10]=  { 0  ,0  ,120,240,120,360,460,560,240,120 }, // x label backgrounds
    dxbg[10]= { 120,120,120,120,120,100,100,100,120,120 }, // dx label backgrounds
    ybg[10]=  { 0  ,267,0  ,0  ,342,0  ,0  ,0  ,375,267 }; // y label backgrounds
  static const char
    *text[10]={ "VCO1","VCO2","VCO3","LFO","MIXER","EG1: ampl","EG2: pitch,filter","VCF","AMPLITUDE","PORTAMENTO" };

  for(i=0;i<10;++i) {
    Rect t_rect(xbg[i],ybg[i],dxbg[i],20);
    if (a_in_b(exp_rect,t_rect)) {
      fill_rectangle(topw,cBgText,t_rect);
      xft_draw_string(xft_topw,xft_White,Point(xtext[i],ytext[i]),text[i],BOLDfont);
    }
  }
  Rect trect(vcf_disp_area.x+15,vcf_disp_area.y+20,80,50);
  if (a_in_b(exp_rect,trect)) {
    clear(topw,trect); // needed for Xft
    xft_draw_string(xft_topw,xft_Red  ,Point(trect.x,trect.y+10),"vcf cutoff");
    xft_draw_string(xft_topw,xft_Black,Point(trect.x,trect.y+22),"vco1 freq");
    xft_draw_string(xft_topw,xft_Black,Point(trect.x,trect.y+34),"vco2 freq");
  }
  mono_synth->draw_isa_col(false);
  static const int
    x1[10]={ -1 ,121,121,361,241,120,240,360,460,560 },  // lines
    x2[10]={ 120,240,240,660,360,120,240,360,460,560 },
    y1[10]={ 265,340,265,375,375,0  ,0  ,0  ,0  ,0   },
    y2[10]={ 265,340,265,375,375,490,490,490,375,375 };

  for(i=0;i<10;++i) {
    draw_line(topw,1,cWhite,Point(x1[i],y1[i]),Point(x2[i],y2[i]));
    draw_line(topw,1,cDarkGrey,Point(x1[i]+1,y1[i]+1),Point(x2[i]+1,y2[i]+1));
  }
}

void hsl_cmd(Id id,int val,int fire,char *&txt,bool rel) {
  const int valmax=16;
  val=minmax(0,val,valmax-1);
  struct Vals { float val; const char *txt; };
  float cval10=val/10.;
  static float volume[valmax]={ 0,0.1,0.15,0.25,0.4,0.6,0.8,1,1.5 },
               filter_q[valmax]={ 1.4,1.0,0.6,0.4,0.3,0.2,0.14,0.1 },
               lfo_freq[valmax]={ 0.2,0.3,0.5,0.8,1.2,2,3,5,8,12,20,30,50,80,120,200 },
               porta_time[valmax]={ 0,0.1,0.2,0.5,1,2,4 },
               semi_exp[valmax]={ 0,0.02,0.04,0.08,0.15,0.3,0.5,0.7,1.4,2. },
               eg_diff[valmax]={ 200,100,60,30,20,10,5,3,2,1.2,0.6 },
               vco_fm[valmax]={ 0,0.1,0.2,0.4,0.8,1.5,2 };
  static Vals
    pitch1[valmax]={
      {1.,"0"},{1.002,"0.002"},{1.003,"0.003"},{1.004,"0.004"},
      {1.006,"0.006"},{1.008,"0.008"},{1.01,"0.01"},{1.015,"0.015"},{1.02,"0.02"}
    },
    pitch2[valmax]={
      {0.5,"1/2"},{2./3.,"2/3"},{3./4.,"3/4"},{1,"1"},{4./3.,"4/3"},{1.5,"3/2"},{2,"2"},{3,"3"},{4,"4"},{6,"6"},{8,"8"}
    };
  int index=id.id2;
  Synth *synth=monoSynth[id.id1];
  MSynth *mono_synth=synth->m_synth;
  float *c=synth->m_synth->values.cont,
        &cont=c[index];
  switch (index) {
    case VCO1_pit:  		// pitch VCO 1
      cont=pitch1[val].val;
      set_text(txt,pitch1[val].txt);
      break;
    case VCO1_fm:  		// VCO1 FM modulation of VCO2
      cont=vco_fm[val];
      mono_synth->values.is_fm= val>0;
      set_text(txt,"%.1f",cont);
      break;
    case VCO2_pit:  		// pitch VCO 2
      cont=pitch2[val].val;
      set_text(txt,pitch2[val].txt);
      if (fire!=DontDis) synth->draw_wave_display(2);
      if (mono_synth->values.on_off[VCO_trig])  // update trig_val
        mono_synth->values.trig_val= int(cont)>1 ? int(cont)/2 : 1;
      break;
    case VCO3_pit:  		// pitch VCO 3
      cont=pitch2[val].val;
      set_text(txt,pitch2[val].txt);
      if (fire!=DontDis) synth->draw_wave_display(3);
      break;
    case VCO1_pw:  		// pulsewidth VCO 1
      cont=fminmax(0.05,val/8.,0.95);
      mono_synth->values.tri_wav[VCO1_wf].set(cont);
      mono_synth->values.square_wav[VCO1_wf].set(cont);
      set_text(txt,"%.2f",cont);
      if (fire!=DontDis) synth->draw_wave_display(1,true);
      break;
    case VCO2_pw:  		// pulsewidth VCO 2
      cont=fminmax(0.05,val/8.,0.95);
      mono_synth->values.tri_wav[VCO2_wf].set(cont);
      mono_synth->values.square_wav[VCO2_wf].set(cont);
      set_text(txt,"%.2f",cont);
      if (fire!=DontDis) synth->draw_wave_display(2,true);
      break;
    case BAL_o1:  		// mix VCO1
    case BAL_o2:  		// mix VCO2
    case BAL_ns:		// mix noise
      cont=val/5.;
      set_text(txt,"%.1f",cont);
      break;
    case EG1_sus: 		// sustain level
      cont=cval10*eg_topval;
      set_text(txt,"%.0f%%",cval10*100);
      if (fire!=DontDis) synth->draw_eg_display(1);
      break;
    case EG2_sus: 		// sustain level
      cont=cval10*eg_topval;
      set_text(txt,"%.0f%%",cval10*100);
      if (fire!=DontDis) synth->draw_eg_display(2);
      break;
    case VCO3_ma:  		// VCO3 mod amplitude
      if (val>=9) {
        mono_synth->values.ringm_vco=true;
        cont=0; set_text(txt,"ring mod.");
      }
      else {
        mono_synth->values.ringm_vco=false;
        cont=val/8.; set_text(txt,"%.0f%%",cont*100);
      }
      break;
    case VCO3_filt:		// VCO3 mod filter
    case EG2_pit: 		// pitch depth
      cont=semi_exp[val];
      set_text(txt,"%.0f%%",cont*100);
      break;
    case LFO_ampl:		// amplitude depth LFO
      if (val>=9) {
        mono_synth->values.ringm_lfo=true;
        cont=0; set_text(txt,"ring mod.");
      }
      else {
        mono_synth->values.ringm_lfo=false;
        cont=val/8.; set_text(txt,"%.0f%%",cont*100);
      }
      break;
    case LFO_filt:  		// filter depth LFO
      cont=semi_exp[val];
      set_text(txt,"+/- %.2f",cont);
      break;
    case VOL:	 		// volume
      cont=volume[val];
      set_text(txt,"%.2f",cont);
      break;
    case EG1_lfo: 		// lfo control by EG1
      cont=min(val,2)/2.;
      break;
    case LFO_fr:  		// lfo frequency
      cont=lfo_freq[val];
      set_text(txt,"%.1f",cont);
      break;
    case LFO_pw:  		// pulsewidth LFO
      cont=fminmax(0.05,val/8.,0.95);
      mono_synth->values.tri_wav[LFO_wf].set(cont);
      mono_synth->values.square_wav[LFO_wf].set(cont);
      set_text(txt,"%.2f",cont);
      if (fire!=DontDis) synth->draw_wave_display(4,true);
      break;
    case LFO_pit:  		// pitch depth LFO
      cont=semi_exp[val]*0.35;
      set_text(txt,"+/- %.3f",cont);
      break;
    case PORT_tim:  		// glide time
      if (val) {
        cont=porta_time[val];
        set_text(txt,"%.2f",cont);
        cont=1-rrate/cont/SAMPLE_RATE;
        mono_synth->values.is_porta=true;
      }
      else {
        cont=0;
        set_text(txt,"0");
        mono_synth->values.is_porta=false;
      }
      break;
    case EG1_att:  		// attack time EG 1
    case EG1_dec: 		// decay time EG 1
    case EG1_rel: 		// release time EG 1
      cont=eg_diff[val];
      if (index==EG1_att) mono_synth->values.eg1_attack_0= val==0;
      set_text(txt,"%.2f",1/cont);
      cont=cont*rrate/SAMPLE_RATE;
      if (fire!=DontDis) { synth->draw_eg_display(1); if (rel) synth->draw_eg_display(2); } // draw both
      break;
    case EG2_att: 		// attack time EG 2
    case EG2_dec: 		// decay time EG 2
    case EG2_rel: 		// release time EG 2
      cont=eg_diff[val];
      if (index==EG2_att) mono_synth->values.eg2_attack_0= val==0;
      set_text(txt,"%.2f",1/cont);
      cont=cont*rrate/SAMPLE_RATE;
      if (fire!=DontDis) synth->draw_eg_display(2);
      break;
    case EG2_filt: 		// filter modulation EG 2
      cont=20*semi_exp[val];
      set_text(txt,"%.1f",cont);
      break;
    case VCF_coff: 		// cutoff VCF
      cont=val-4.; // = 15.*(val/15.)-4.;
      set_text(txt,"%.1f",float(cont-1.));
      break;
    case VCF_q: 		// resonance VCF
      cont=filter_q[val];
      set_text(txt,"%.1f",1/cont);
      break;
    default:
      alert("hsl_cmd: case %d?",index);
  }
}

static void rbut_cmd(Id id,int nr,int fire) {
  int index=id.id2;
  Synth *synth=monoSynth[id.id1];
  MSynth *mono_synth=synth->m_synth;
  switch (index) {
    case VCO1_wf:
      mono_synth->values.dete[index]=nr;
      if (fire!=DontDis) synth->draw_wave_display(1);
      break;
    case VCO2_wf:
      mono_synth->values.dete[index]=nr;
      if (fire!=DontDis) synth->draw_wave_display(2);
      break;
    case LFO_wf:
      mono_synth->values.dete[index]=nr;
      if (fire!=DontDis) synth->draw_wave_display(4);
      break;
    case Four_pole:
      mono_synth->values.dete[index]=nr;
      mono_synth->values.is_pole4= nr==0;
      break;
    case FilterMode:
      mono_synth->values.dete[index]=nr;
      mono_synth->values.filter_mode= nr==0 ? BANDpass : nr==1 ? LOpass : HIpass;
      break;
    default:
      alert("case %d?\n",index);
  }
}

void chbox_cmd(Id id,bool on) {
  int index=id.id2;
  MSynth *mono_synth=monoSynth[id.id1]->m_synth;
  switch (index) {
    case VCO_trig:
      if (on) {
        int pmult=int(mono_synth->values.cont[VCO2_pit]);
        mono_synth->values.trig_val=pmult>1 ? pmult/2 : 1;
      }
      else mono_synth->values.trig_val=0;
      // no break;
    case LFO_synced:
    case VCF_clip:
      mono_synth->values.on_off[index]=on;
      break;
    default:
      alert("case %d?\n",index);
  }
}

RButWin *waveform_win(int x,int y,Synth *synth,int id) {
  int dy1= id==LFO_wf ? 5*TDIST : 3*TDIST;
  RButWin *rbw=new RButWin(synth->subw,Rect(x,y,50,dy1),FN,"wave",false,rbut_cmd,cForeground,Id(synth->col,id));
  rbw->add_rbut("sine");
  rbw->add_rbut("triangle");
  rbw->add_rbut("pulse");
  if (id==LFO_wf) {
    rbw->add_rbut("noise"); rbw->add_rbut("flutter");
  }
  return rbw;
}

void MSynth::reset(bool soft) {  // rest is set by set_values()
  note_on=0;
  prev_note=0;
  if (!soft) {   // mode != repeat
    osc1_pos=osc2_pos=osc3_pos=lfo_pos=0.;
    d1=d2=d3=d4=0.;
    eg1=eg2=0.;
    eg1_phase=eg2_phase=0;
    cur_patch=nop;
  }
}

void Synth::init(bool soft) { m_synth->reset(soft); }
void Synth::set_values(int mnr,float f) { m_synth->set_values(mnr,f); }
bool Synth::fill_buffer(float *buf,const int buf_size,const float amp_mul) {
  bool res=m_synth->fill_buffer(buf,buf_size,amp_mul);
  if (res) send_uev('vcfd',col);   // VCF display
  else send_uev('clvd',col);       // clear VCF display
  return res;
}
void Synth::note_off() {
  if (m_synth->note_on>0) --m_synth->note_on;
}

static void strip_points(Point *points,int& nr) { // decrease points
  if (nr<=2) return;
  Point pt0(points[0]),
        pt1(points[1]);
  bool horizontal= abs(pt1.y-pt0.y) < abs(pt1.x-pt1.x);
  float angle=horizontal ? float(pt1.y-pt0.y) / (pt1.x-pt0.x) : float(pt1.x-pt0.x) / (pt1.y-pt0.y);
  int i,
      ind=0;
  for (i=2;;++i) {
    bool horizontal2= abs(points[i].y-points[i-1].y) < abs(points[i].x-points[i-1].x);
    float angle2=horizontal2 ? float(points[i].y-points[i-1].y) / (points[i].x-points[i-1].x) :
                               float(points[i].x-points[i-1].x) / (points[i].y-points[i-1].y);
    if (horizontal!=horizontal2 || fabs(angle-angle2)>0.1) {
      points[++ind]=points[i-1];
      angle=angle2;
      horizontal=horizontal2;
      // printf("ind=%d x=%d y=%d hor=%d\n",ind,points[ind].x,points[ind].y,horizontal);
    }
    if (i==nr-1) {
      points[++ind]=points[i];
      break;
    }
  }
  nr=ind+1;
}      

void Synth::draw_wave_display(int vco_nr,bool test_whether_needed) {  // default: test_whether_needed = false
  BgrWin *bgwin= vco_nr==1 ? vco1_display : vco_nr==2 ? vco2_display : vco_nr==3 ? vco3_display : lfo_display;
  if (bgwin->is_hidden()) return;
  const int dx=vco1_disp_area.width,
            dy=vco1_disp_area.height;
  int waveform=0,
      wf_index;
  float wf_pw=0,
        wf_pit=1;
  switch (vco_nr) {
    case 1:
      waveform=m_synth->values.dete[VCO1_wf];
      wf_index=VCO1_wf; wf_pw=m_synth->values.cont[VCO1_pw];
      break;
    case 2:
      waveform=m_synth->values.dete[VCO2_wf];
      wf_index=VCO2_wf; wf_pw=m_synth->values.cont[VCO2_pw];
      wf_pit=m_synth->values.cont[VCO2_pit];
      break;
    case 3:
      wf_index=0;
      wf_pit=m_synth->values.cont[VCO3_pit];
      break;
    case 4:
      waveform=m_synth->values.dete[LFO_wf];
      wf_index=LFO_wf; wf_pw=m_synth->values.cont[LFO_pw];
      break;
  }
  if (test_whether_needed && (waveform==0 || waveform==3))
    return;
  Point points[dx];
  int i,x1,x2,
      val,
      nr_points=0;
  float inc,
        fi;
  switch (waveform) {
    case 0:  // sinus
      inc=wf_pit/dx;
      for (i=0,fi=0;i<=dx;++i) {
        points[i].set(i,int(dy-dy*sinus(fi))/2);
        if ((fi+=inc)>1.) fi-=1.;
      }
      nr_points=dx+1;
      strip_points(points,nr_points);
      break;
    case 1:  // triangle
      for (i=0;;i+=2) {
        x1=int(i*dx/2/wf_pit); x2=x1+int(wf_pw*dx/wf_pit);
        points[i].set(x1,dy);
        points[i+1].set(x2,0);
        if (x2>=dx-1) { nr_points=i+2; break; }
      }
      break;
    case 2:  // pulse
      val=dy*7/10; 
      for (i=0;;i+=4) {
        x1=int(i*dx/4/wf_pit); x2=x1+int(wf_pw*dx/wf_pit);
        points[i+1].set(x1,(dy-val)/2); points[i+2].set(x2,(dy-val)/2);
        points[i].set(x1,(dy+val)/2); points[i+3].set(x2,(dy+val)/2);
        if (x2>=dx-1) { nr_points=i+4; break; }
      }
      break;
    case 3:  // white noise
      for (i=0;i<=dx;i+=2) {
        points[i/2].set(i,int(dy*(2.+noise.white_buf[i])/4));
      }
      nr_points=dx/2+1;
      break;
    case 4:  // flutter
      inc=wf_pit/dx;
      for (i=0,fi=0;i<=dx;++i) {
        float fi2=fi*Flutter::mult2; while (fi2>1.) fi2-=1.;
        float fi3=fi*Flutter::mult3; while (fi3>1.) fi3-=1.;
        float fval=dy*(sinus(fi)+sinus(fi2)+sinus(fi3))/3.;
        points[i].set(i,int(dy-fval)/2);
        if ((fi+=inc)>1.) fi-=1.;
      }
      nr_points=dx+1;
      strip_points(points,nr_points);
      break;
  }
  bgwin->clear();
  bgwin->cai_draw_lines(points,nr_points,1,cai_Blue);
}

void Synth::draw_eg_display(int eg_nr) {
  BgrWin *bgwin= eg_nr==1 ? eg1_display : eg2_display;
  if (bgwin->is_hidden()) return;
  const int dx=eg1_disp_area.width,
            dy=eg1_disp_area.height;
  float c=dx*2;
  bgwin->clear();
  int i;
  float *cont=m_synth->values.cont,
        a=cont[EG1_att],
        d=cont[EG1_dec],
        s=cont[EG1_sus],
        r=cont[EG1_rel];
  bool eg_attack_0=m_synth->values.eg1_attack_0;
  mult_eg=0.07*fmax(1/a + (eg_topval-s)/d + s/r, c); // EG1 used for scaling
  if (eg_nr==2) {
    a=cont[EG2_att]; d=cont[EG2_dec]; s=cont[EG2_sus]; r=cont[EG2_rel]; eg_attack_0=m_synth->values.eg2_attack_0;
  }
  //printf("w=%p a=%.2f d=%.2f s=%.2f r=%.2f\n",bgwin,a,d,s,r);
  a*=mult_eg;
  d*=mult_eg;
  s*=dy;
  r*=mult_eg;
  c/=mult_eg;
  if (c<8) c=8; // better display in case s has high value
  Point points[dx];
  float val=0;

  if (eg_attack_0) {
    val=eg_topval*dy;
    points[0].set(0,dy-int(val));
    i=1;
  }
  else
    for (i=0;i<dx;++i) {  // attack
      if (val > eg_topval*dy) {
        val=eg_topval*dy;
        points[i].set(i,dy-int(val));
        break;
      }
      points[i].set(i,dy-int(val));
      val+=a*(dy-val);
    }
  for (;i<dx;++i) {  // decay
    val-=d*(val-s);
    if (val < s+eg_diffval*dy) {
      val=s+eg_diffval*dy;
      points[i].set(i,dy-int(val));
      break;
    }
    points[i].set(i,dy-int(val));
  }
  int start_c=i;
  for (;i-start_c<=int(c) && i<dx;++i)  // sustain
    points[i].set(i,dy-int(val));
  int nr_pts=dx;
  for (;i<dx;++i) {  // release
    if (val>0.1) val-=r*val;
    else if (val>0.) val-=r*0.1;
    else {
      val=0; nr_pts=i;
      points[i].set(i,dy);
      break;
    }
    points[i].set(i,dy-int(val));
  }
  set_color(cGrey);
  bgwin->fill_rectangle(Rect(start_c,0,int(c),dy));
  strip_points(points,nr_pts);
  bgwin->cai_draw_lines(points,nr_pts,1,cai_Blue);
}

static void draw_eg_dis(Id id) { monoSynth[id.id1]->draw_eg_display(id.id2); }

static void draw_wave_dis(Id id) { monoSynth[id.id1]->draw_wave_display(id.id2); }

Synth::Synth(Point top,int c,bool do_map):
    m_synth(new MSynth()),
    col(c),
    isa_col(c) {
  monoSynth[col]=this;
  static uint cDispBgr=cBackground;
  subw=new SubWin("Mono Synth",Rect(top.x,top.y,660,490),do_map,cForeground,draw_panel,hide_swin,Id(col));
  topwin=subw->win; xft_topwin=subw->xft_win;
  // VCO 1
  hsliders[VCO1_pit]=new HSlider(subw,Rect(15,40,68,0),FN,0,8,"detune","0","8",hsl_cmd,cForeground,Id(col,VCO1_pit));
  hsliders[VCO1_pw]=new HSlider(subw,Rect(15,145,68,0),FN,0,8,"waveform","0","8",hsl_cmd,cForeground,Id(col,VCO1_pw));  
  hsliders[VCO1_fm]=new HSlider(subw,Rect(10,190,60,0),FN,0,6,"FM VCO2 ","0","6",hsl_cmd,cForeground,Id(col,VCO1_fm));  
  checkboxs[VCO_trig]=new CheckBox(topwin,Rect(10,235,0,0),FN,cForeground,"trigger VCO2 ",chbox_cmd,Id(col,VCO_trig));  

  // VCO 2
  hsliders[VCO2_pit]=new HSlider(subw,Rect(15,310,90,0),FN,0,10,"pitch mult.","0","10",hsl_cmd,cForeground,Id(col,VCO2_pit));  
  hsliders[VCO2_pw]=new HSlider(subw,Rect(15,415,68,0),FN,0,8,"waveform","0","8",hsl_cmd,cForeground,Id(col,VCO2_pw));

  // VCO 3
  hsliders[VCO3_pit]=new HSlider(subw,Rect(135,40,90,0),FN,0,10,"pitch mult.","0","10",hsl_cmd,cForeground,Id(col,VCO3_pit));  
  hsliders[VCO3_ma]=new HSlider(subw,Rect(135,145,68,0),FN,0,9,"ampl. mod.","0","9",hsl_cmd,cForeground,Id(col,VCO3_ma));
  hsliders[VCO3_filt]=new HSlider(subw,Rect(135,190,68,0),FN,0,8,"filter mod.","0","8",hsl_cmd,cForeground,Id(col,VCO3_filt));

  // PORTAMENTO
  hsliders[PORT_tim]=new HSlider(subw,Rect(135,310,68,0),FN,0,6,"glide time","0","6",hsl_cmd,cForeground,Id(col,PORT_tim));

  // LFO
  hsliders[LFO_fr]=new HSlider(subw,Rect(245,40,110,0),FN,0,15,"frequency","0","15",hsl_cmd,cForeground,Id(col,LFO_fr));
  hsliders[LFO_pw]=new HSlider(subw,Rect(255,170,72,0),FN,0,8,"waveform","0","8",hsl_cmd,cForeground,Id(col,LFO_pw));
  hsliders[LFO_ampl]=new HSlider(subw,Rect(255,214,82,0),FN,0,9,"ampl. mod.","0","9",hsl_cmd,cForeground,Id(col,LFO_ampl));
  hsliders[LFO_pit]=new HSlider(subw,Rect(255,258,82,0),FN,0,9,"VCO2 mod.","0","9",hsl_cmd,cForeground,Id(col,LFO_pit));
  hsliders[LFO_filt]=new HSlider(subw,Rect(255,302,72,0),FN,0,8,"filter mod.","0","8",hsl_cmd,cForeground,Id(col,LFO_filt));

  // MIXER
  hsliders[BAL_o1]=new HSlider(subw,Rect(135,380,55,0),FN,0,5,"vco1","0","5",hsl_cmd,cForeground,Id(col,BAL_o1));
  hsliders[BAL_o2]=new HSlider(subw,Rect(135,420,55,0),FN,0,5,"vco2","0","5",hsl_cmd,cForeground,Id(col,BAL_o2));
  hsliders[BAL_ns]=new HSlider(subw,Rect(135,460,55,0),FN,0,5,"noise","0","5",hsl_cmd,cForeground,Id(col,BAL_ns));

  // EG 1
  hsliders[EG1_att]=new HSlider(subw,Rect(375,40,74,0),FN,0,10,"attack time","0","10",hsl_cmd,cForeground,Id(col,EG1_att));
  hsliders[EG1_dec]=new HSlider(subw,Rect(375,90,74,0),FN,0,10,"decay time","0","10",hsl_cmd,cForeground,Id(col,EG1_dec));
  hsliders[EG1_sus]=new HSlider(subw,Rect(375,140,74,0),FN,0,10,"sustain level","0","10",hsl_cmd,cForeground,Id(col,EG1_sus));
  hsliders[EG1_rel]=new HSlider(subw,Rect(375,190,74,0),FN,0,10,"release time","0","10",hsl_cmd,cForeground,Id(col,EG1_rel));
  hsliders[EG1_lfo]=new HSlider(subw,Rect(375,295,30,0),FN,0,2,"LFO effect","0","2",hsl_cmd,cForeground,Id(col,EG1_lfo));

  // EG 2
  hsliders[EG2_att]=new HSlider(subw,Rect(475,40,74,0),FN,0,10,"attack time","0","10",hsl_cmd,cForeground,Id(col,EG2_att));
  hsliders[EG2_dec]=new HSlider(subw,Rect(475,90,74,0),FN,0,10,"decay time","0","10",hsl_cmd,cForeground,Id(col,EG2_dec));
  hsliders[EG2_sus]=new HSlider(subw,Rect(475,140,74,0),FN,0,10,"sustain level","0","10",hsl_cmd,cForeground,Id(col,EG2_sus));
  hsliders[EG2_rel]=new HSlider(subw,Rect(475,190,74,0),FN,0,10,"release time","0","10",hsl_cmd,cForeground,Id(col,EG2_rel));
  hsliders[EG2_pit]=new HSlider(subw,Rect(475,295,74,0),FN,0,8,"VCO2 mod.","0","8",hsl_cmd,cForeground,Id(col,EG2_pit));
  hsliders[EG2_filt]=new HSlider(subw,Rect(475,345,74,0),FN,0,8,"filter mod.","0","8",hsl_cmd,cForeground,Id(col,EG2_filt));

  // VCF
  hsliders[VCF_coff]=new HSlider(subw,Rect(565,40,90,0),FN,0,15,"cutoff","0","15",hsl_cmd,cForeground,Id(col,VCF_coff));
  hsliders[VCF_q]=new HSlider(subw,Rect(565,90,68,0),FN,0,7,"resonance","0","7",hsl_cmd,cForeground,Id(col,VCF_q));

  // VOLUME
  hsliders[VOL]=new HSlider(subw,Rect(250,420,90,0),FN,0,8,"master ampl.","0","8",hsl_cmd,cForeground,Id(col,VOL));
  ms_ampl=&hsliders[VOL]->value();

  rbutwins[VCO1_wf]=waveform_win(2,80,this,VCO1_wf);
  vco1_display=new BgrWin(topwin,vco1_disp_area,FN,draw_wave_dis,cDispBgr,1,Id(col,1));

  rbutwins[VCO2_wf]=waveform_win(2,350,this,VCO2_wf);
  vco2_display=new BgrWin(topwin,vco2_disp_area,FN,draw_wave_dis,cDispBgr,1,Id(col,2));

  vco3_display=new BgrWin(topwin,vco3_disp_area,FN,draw_wave_dis,cDispBgr,1,Id(col,3));

  rbutwins[LFO_wf]=waveform_win(242,80,this,LFO_wf);
  lfo_display=new BgrWin(topwin,lfo_disp_area,FN,draw_wave_dis,cDispBgr,1,Id(col,4));

  RButWin *rbw;
  rbutwins[Four_pole]=rbw=new RButWin(subw,Rect(575,135,60,2*TDIST),FN,"slope",false,rbut_cmd,cForeground,Id(col,Four_pole));
  rbw->add_rbut("24 db/oct");
  rbw->add_rbut("12 db/oct");

  rbutwins[FilterMode]=rbw=new RButWin(subw,Rect(575,190,60,3*TDIST),FN,"mode",false,rbut_cmd,cForeground,Id(col,FilterMode));
  rbw->add_rbut("bandpass");
  rbw->add_rbut("lowpass");
  rbw->add_rbut("highpass");

  checkboxs[VCF_clip]=new CheckBox(topwin,Rect(574,245,0,0),FN,cForeground,"clip",chbox_cmd,Id(col,VCF_clip));

  vcf_display=new BgrWin(topwin,vcf_disp_area,FN,0,cWhite);
  eg1_display=new BgrWin(topwin,eg1_disp_area,FN,draw_eg_dis,cDispBgr,1,Id(col,1));
  eg2_display=new BgrWin(topwin,eg2_disp_area,FN,draw_eg_dis,cDispBgr,1,Id(col,2));

  checkboxs[LFO_synced]=new CheckBox(topwin,Rect(255,335,0,0),FN,cForeground,"sync'ed",chbox_cmd,Id(col,LFO_synced));

  set_patch=new Patches(Point(370,395),subw,col);

  button_style.param=1;
  eq=new Button(topwin,Rect(10,470,28,0),FN,"eq?",but_cmd,Id(col,'eq'));
  button_style.param=0;

  MSynthData ms_data;
  int i;
  ms_data.fill_msynth_data(col,"F5,32300,314,346,5000021,500,17680,0,065703,64010,5");
  for (i=0;i<NCHECKB;++i)
    checkboxs[i]->set_cbval(ms_data.cb_buf[i],DontDis,false); // fire, not draw
  for (i=0;i<NSLIDER;++i)
    hsliders[i]->set_hsval(ms_data.hsl_buf[i],DontDis,false); // should come after checkboxes, because of VCO2_pit
  for (i=0;i<NRBUT;++i)
    rbutwins[i]->set_rbutnr(ms_data.rb_buf[i],DontDis,false);
}

void Synth::map_topwin(bool do_map) {
  if (do_map) {
    set_patch->scroll->set_range((lst_patch+2)*TDIST);
    map_window(topwin);
  }
  else
    hide_window(topwin);
}

MSynth::MSynth():
    isa(&values),
    noise_ind(0) {
  reset(false);
}

float MSynth::oscillator(int vco_nr,float& pos,float osc_freq,int waveform,bool *trig) {
  //float osc_freq=omega/SAMPLE_RATE;
  const int nmult=20; // pos multiplier in case of lfo noise
  static const float ndim=float(noise.dim/nmult);
  switch(waveform) {
    case 0:            // sine wave
      return isa->sin_wav[vco_nr].get(osc_freq,pos,trig);
    case 1:            // triangle wave
      return isa->tri_wav[vco_nr].get(osc_freq,pos,trig);
    case 2:            // pulse wave
      return isa->square_wav[vco_nr].get(osc_freq,pos,trig);
    case 3:            // noise
      pos+=osc_freq;
      if (pos>ndim) pos-=ndim;
      else if (pos<0) pos+=ndim;
      return noise.white_buf[int(pos*nmult)];
    case 4:            // flutter wave
      return isa->flutter_wav.get(osc_freq);
    default:
      alert("waveform %d?",waveform);
      return 0.;
  }
}

void Synth::dump_settings(char *buf,int bmax,const char *c) {
  HSlider **hsl=hsliders;
  RButWin **rb=rbutwins;
  CheckBox **cb=checkboxs;
  snprintf(buf,bmax,"set %s msynth:F5,%x%x%x%x%x,%x%x%x,%x%x%x,%x%x%x%x%x%x%d,%x%x%x,%x%x%x%x%x,%x,%x%x%x%x%x%x,%x%x%x%x%d,%x",
    c,
    hsl[VCO1_pit]->value(),rb[VCO1_wf]->act_rbutnr(),hsl[VCO1_pw]->value(),hsl[VCO1_fm]->value(),cb[VCO_trig]->value,   // VCO1
    hsl[VCO2_pit]->value(),rb[VCO2_wf]->act_rbutnr(),hsl[VCO2_pw]->value(),   // VCO2
    hsl[VCO3_pit]->value(),hsl[VCO3_ma]->value(),hsl[VCO3_filt]->value(), // VCO3
    hsl[LFO_fr]->value(),rb[LFO_wf]->act_rbutnr(),hsl[LFO_pw]->value(),hsl[LFO_ampl]->value(), // LFO
    hsl[LFO_pit]->value(),hsl[LFO_filt]->value(),cb[LFO_synced]->value, // LFO
    hsl[BAL_o1]->value(),hsl[BAL_o2]->value(),hsl[BAL_ns]->value(),       // MIXER
    hsl[EG1_att]->value(),hsl[EG1_dec]->value(),hsl[EG1_sus]->value(),hsl[EG1_rel]->value(),hsl[EG1_lfo]->value(), // EG1
    hsl[PORT_tim]->value(),                                           // PORTAMENTO
    hsl[EG2_att]->value(),hsl[EG2_dec]->value(),hsl[EG2_sus]->value(),hsl[EG2_rel]->value(),hsl[EG2_pit]->value(),hsl[EG2_filt]->value(), // EG2
    hsl[VCF_coff]->value(),hsl[VCF_q]->value(),rb[Four_pole]->act_rbutnr(),rb[FilterMode]->act_rbutnr(),cb[VCF_clip]->value,  // VCF
    hsl[VOL]->value());                                               // VOLUME
}

bool MSynthData::fill_msynth_data(int _col,const char *str) {
  col=_col;
  int n=0;
  n=sscanf(str,"F5,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx",
      hsl_buf+VCO1_pit,rb_buf+VCO1_wf,hsl_buf+VCO1_pw,hsl_buf+VCO1_fm,cb_buf+VCO_trig, // VCO 1
      hsl_buf+VCO2_pit,rb_buf+VCO2_wf,hsl_buf+VCO2_pw,        // VCO 2
      hsl_buf+VCO3_pit,hsl_buf+VCO3_ma,hsl_buf+VCO3_filt, // VCO 3
      hsl_buf+LFO_fr,rb_buf+LFO_wf,hsl_buf+LFO_pw,hsl_buf+LFO_ampl,hsl_buf+LFO_pit,hsl_buf+LFO_filt,cb_buf+LFO_synced, // LFO
      hsl_buf+BAL_o1,hsl_buf+BAL_o2,hsl_buf+BAL_ns,   // MIXER
      hsl_buf+EG1_att,hsl_buf+EG1_dec,hsl_buf+EG1_sus,hsl_buf+EG1_rel,hsl_buf+EG1_lfo, // EG 1
      hsl_buf+PORT_tim,                // PORTAMENTO
      hsl_buf+EG2_att,hsl_buf+EG2_dec,hsl_buf+EG2_sus,hsl_buf+EG2_rel,hsl_buf+EG2_pit,hsl_buf+EG2_filt, // EG 2
      hsl_buf+VCF_coff,hsl_buf+VCF_q,rb_buf+Four_pole,rb_buf+FilterMode,cb_buf+VCF_clip,  // VCF
      hsl_buf+VOL);                                     // VOLUME
  if (n==0) {  // old format
    n=sscanf(str,"F4_%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%*c%*c%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx",
      hsl_buf+VCO1_pit,rb_buf+VCO1_wf,hsl_buf+VCO1_pw,hsl_buf+VCO1_fm, // VCO 1
      hsl_buf+VCO2_pit,rb_buf+VCO2_wf,hsl_buf+VCO2_pw,        // VCO 2
      hsl_buf+VCO3_pit,hsl_buf+VCO3_ma,hsl_buf+VCO3_filt, // VCO 3
      hsl_buf+LFO_fr,rb_buf+LFO_wf,hsl_buf+LFO_pw,hsl_buf+LFO_ampl,hsl_buf+LFO_pit,hsl_buf+LFO_filt,cb_buf+LFO_synced, // LFO
      hsl_buf+BAL_o1,hsl_buf+BAL_o2,hsl_buf+BAL_ns,   // MIXER
      hsl_buf+EG1_att,hsl_buf+EG1_dec,hsl_buf+EG1_sus,hsl_buf+EG1_rel,hsl_buf+EG1_lfo, // EG 1
      hsl_buf+PORT_tim,                // PORTAMENTO
      hsl_buf+EG2_att,hsl_buf+EG2_dec,hsl_buf+EG2_sus,hsl_buf+EG2_rel,hsl_buf+EG2_pit,hsl_buf+EG2_filt, // EG 2
      hsl_buf+VCF_coff,hsl_buf+VCF_q,rb_buf+Four_pole,rb_buf+FilterMode,cb_buf+VCF_clip,  // VCF
      hsl_buf+VOL);                                     // VOLUME
    cb_buf[VCO_trig]=false;
    if (n>0) n+=1;
  }  
  if (n==0) {
    n=sscanf(str,"F3_%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx",
      hsl_buf+VCO1_pit,rb_buf+VCO1_wf,hsl_buf+VCO1_pw,hsl_buf+VCO1_fm,          // VCO 1
      hsl_buf+VCO2_pit,rb_buf+VCO2_wf,hsl_buf+VCO2_pw,hsl_buf+VCO3_ma,hsl_buf+VCO3_filt,        // VCO 2
      hsl_buf+LFO_fr,rb_buf+LFO_wf,hsl_buf+LFO_pw,hsl_buf+LFO_ampl,hsl_buf+LFO_pit,hsl_buf+LFO_filt,cb_buf+LFO_synced, // LFO
      hsl_buf+BAL_o1,hsl_buf+BAL_o2,hsl_buf+BAL_ns,   // MIXER
      hsl_buf+EG1_att,hsl_buf+EG1_dec,hsl_buf+EG1_sus,hsl_buf+EG1_rel,hsl_buf+EG1_lfo, // EG 1
      hsl_buf+PORT_tim,                // PORTAMENTO
      hsl_buf+EG2_att,hsl_buf+EG2_dec,hsl_buf+EG2_sus,hsl_buf+EG2_rel,hsl_buf+EG2_pit,hsl_buf+EG2_filt, // EG 2
      hsl_buf+VCF_coff,hsl_buf+VCF_q,rb_buf+Four_pole,rb_buf+FilterMode,cb_buf+VCF_clip,  // VCF
      hsl_buf+VOL);                                     // VOLUME
    hsl_buf[VCO3_pit]=hsl_buf[VCO2_pit];
    cb_buf[VCO_trig]=false;
    if (n>0) n+=2;
  }
  if (n==0) {
    n=sscanf(str,"F2_%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx,%1hhx%1hhx%1hhx%1hhx%1hhx%1hhx,%1hhx%1hhx%1hhx%1hhx,%1hhx",
      hsl_buf+VCO1_pit,rb_buf+VCO1_wf,hsl_buf+VCO1_pw,          // VCO 1
      hsl_buf+VCO2_pit,rb_buf+VCO2_wf,hsl_buf+VCO2_pw,hsl_buf+VCO3_ma,hsl_buf+VCO3_filt,        // VCO 2
      hsl_buf+LFO_fr,rb_buf+LFO_wf,hsl_buf+LFO_pw,hsl_buf+LFO_ampl,hsl_buf+LFO_pit,hsl_buf+LFO_filt,cb_buf+LFO_synced, // LFO
      hsl_buf+BAL_o1,hsl_buf+BAL_o2,hsl_buf+BAL_ns,   // MIXER
      hsl_buf+EG1_att,hsl_buf+EG1_dec,hsl_buf+EG1_sus,hsl_buf+EG1_rel,hsl_buf+EG1_lfo, // EG 1
      hsl_buf+PORT_tim,                // PORTAMENTO
      hsl_buf+EG2_att,hsl_buf+EG2_dec,hsl_buf+EG2_sus,hsl_buf+EG2_rel,hsl_buf+EG2_pit,hsl_buf+EG2_filt, // EG 2
      hsl_buf+VCF_coff,hsl_buf+VCF_q,rb_buf+Four_pole,rb_buf+FilterMode,  // VCF
      hsl_buf+VOL);                                     // VOLUME
    cb_buf[VCF_clip]=0;
    hsl_buf[VCO1_fm]=0;
    hsl_buf[VCO3_pit]=hsl_buf[VCO2_pit];
    cb_buf[VCO_trig]=false;
    if (n>0) n+=4;
  }
  if (n!=NCONTROLS) {
    if (n!=NCONTROLS) alert("set msynth: %d values read (expected %d)",n,NCONTROLS);
    else alert("set msynth: %d values read (expected %d)",n,NCONTROLS);
    alert("string: %s",str);
    return false;
  }
  return true;
}

void Synth::set_isa(Synth *other) { 
  if (other==this) {
    isa_col=col;
    ms_ampl=&hsliders[VOL]->value();
    m_synth->isa=&m_synth->values;
  }
  else {
    isa_col=other->col;
    m_synth->isa=&other->m_synth->values;
    ms_ampl=other->ms_ampl;
  }
}

void Synth::draw_isa_col(bool clear) {
  if (isa_col==col && !clear) return;
  uint line_col= isa_col==col ? cForeground : col2color(isa_col);
  draw_line(topwin,2,line_col,Point(1,0),Point(1,subw->dy));
}

void Synth::update_values(MSynthData *d,bool setpatch) {
  int i,
      val;
  bool dr_eg=false,
       dr_wav1=false,dr_wav2=false,dr_wav3=false,dr_wav4=false;
  for (i=0;i<NSLIDER;++i) {
    val=d->hsl_buf[i];
    if (hsliders[i]->value()!=val) {
      switch (i) {
        case EG1_att: case EG1_dec: case EG1_sus: case EG1_rel:
        case EG2_att: case EG2_dec: case EG2_sus: case EG2_rel:
          dr_eg=true;
          break;
        case VCO1_pw: dr_wav1=true; break;
        case VCO2_pw: case VCO2_pit: dr_wav2=true; break;
        case LFO_pw: dr_wav4=true; break;
      }
      if (setpatch) // sent by setpatch_cmd()
        hsliders[i]->set_hsval(val,DontDis,true);  // set value, fire, draw
      else {
        hsliders[i]->set_hsval(val,DontDis,false); // fire, not draw
        send_uev('dr66',col,i); // draw sliders
      }
    }
  }
  if (dr_eg) send_uev('dr69',col,i); // draw eg display

  for (i=0;i<NRBUT;++i) {
    val=d->rb_buf[i];
    if (rbutwins[i]->act_rbutnr() != val) {
      switch (i) {
        case VCO1_wf: dr_wav1=true; break;
        case VCO2_wf: dr_wav2=true; break;
        case LFO_wf: dr_wav4=true; break;
      }
      if (setpatch)
        rbutwins[i]->set_rbutnr(val,DontDis,true);  // set value, fire and draw
      else {
        rbutwins[i]->set_rbutnr(val,DontDis,false);
        send_uev('dr67',col,i); // draw radio buttons
      }
    }
  }
  if (dr_wav1) send_uev('dr70',col,i); // draw waveforms
  if (dr_wav2) send_uev('dr71',col,i);
  if (dr_wav3) send_uev('dr72',col,i);
  if (dr_wav4) send_uev('dr73',col,i);
  for (i=0;i<NCHECKB;++i) {
    val=d->cb_buf[i];
    if (checkboxs[i]->value!=val) {
      if (setpatch)
        checkboxs[i]->set_cbval(val,1,true);  // set value, fire and draw
      else {
        checkboxs[i]->set_cbval(val,1,false);
        send_uev('dr68',col,i); // draw check boxes
      }
    }
  }
}

void MSynth::set_values(int mnr,float f) {
  if (note_on) {
    note_diff= prev_note<mnr ? 1 : prev_note>mnr ? -1 : 0;
    if (isa->is_porta) eg1_phase=eg2_phase=1;
  }
  else {
    note_diff=0;
    fund_pitch=f;
  }
  if (!note_on || !isa->is_porta) { // NOT reset d1 etc, eg1, eg2, else clicks!
    if (isa->eg1_attack_0) { eg1_phase=1; eg1=eg_topval; }
    else eg1_phase=0;
    if (isa->eg2_attack_0) { eg2_phase=1; eg2=eg_topval; }
    else eg2_phase=0;
    if (isa->on_off[LFO_synced]) lfo_pos=0.;
  }
  ++note_on;
  prev_note=mnr;
  the_pitch=f;
  osc1_waveform=isa->dete[VCO1_wf];
  osc1_pw=isa->cont[VCO1_pw];
  osc2_waveform=isa->dete[VCO2_wf];
  osc2_pw=isa->cont[VCO2_pw];
  osc_freq4=isa->cont[LFO_fr]/SAMPLE_RATE;
  lfo_pw=isa->cont[LFO_pw];
  lfo_waveform=isa->dete[LFO_wf];
  lfo_amount_a=isa->cont[LFO_ampl];
  lfo_amount_o=isa->cont[LFO_pit];
  lfo_amount_f=isa->cont[LFO_filt];
  fm_amount=isa->cont[VCO1_fm];
  vco3_amount_a=isa->cont[VCO3_ma];
  vco3_amount_f=isa->cont[VCO3_filt];
  eg1_amount_o=isa->cont[EG1_lfo];
  eg2_amount_o=isa->cont[EG2_pit];
  qres=isa->cont[VCF_q];
  bal_o1=isa->cont[BAL_o1];
  bal_o2=isa->cont[BAL_o2];
  bal_ns=isa->cont[BAL_ns];
}
/*
static float clip1(float in) {
  if (in>0)
    return in>2 ? 1 : in - in*in/4;
  return in<-2 ? -2 : in + in*in/4;
}
*/
static float clip2(float in) {
  if (in>0)
    return in>6 ? 3 : in - in*in/12;
  return in<-6 ? -3 : in + in*in/12;
}

void MSynth::calc_eg(float &eg,int &eg_phase,int EG_att,int EG_dec,int EG_sus,int EG_rel) {
  switch (eg_phase) {
    case 0:
      eg+=isa->cont[EG_att]+(1.-eg)*isa->cont[EG_att];
      if (eg>eg_topval) { eg=eg_topval; eg_phase=1; }   // flip from attack to decay
      break;
    case 1:
      eg-=isa->cont[EG_dec]*(eg-isa->cont[EG_sus]);
      if (eg-isa->cont[EG_sus] < eg_diffval) eg_phase=3;
      break;
    case 2:
      if (eg>0.1)
        eg-=isa->cont[EG_rel]*eg;
      else
        eg-=isa->cont[EG_rel]*0.1;
      break;
    case 3:
      if (isa->cont[EG_sus]<0.05)
        eg-=isa->cont[EG_rel]*0.1;
      break;
  }
  if (eg<0.) eg=0.;
}

bool MSynth::fill_buffer(float *buffer,const int buf_size,const float amp_mul) {
  float lfo,freq_mod_2=1,osc1,osc2,osc3,input,output;
  bool trig_vco1_2;
  if (!note_on) {
    eg1_phase=eg2_phase=2;
    if (eg1 < 0.01)  {   // allow sound to complete its release phase
      d1=d2=d3=d4=0.;    // no sound, so reset filter delay storage
  
      if (note_on>0) --note_on;
      return false;
    }
  }
  for(int i=0; i<buf_size; ++i) {    // loop to calculate the sound
    if (i%rrate==0) {   // calculate the frequency each rrate samples
      if (isa->is_porta) {
        if (note_diff>0) {
          fund_pitch /= isa->cont[PORT_tim];
          if (fund_pitch>the_pitch) {
            fund_pitch=the_pitch;
            note_diff=0;
          }
        }
        else if (note_diff<0) {
          fund_pitch *= isa->cont[PORT_tim];
          if (fund_pitch<the_pitch) {
            fund_pitch=the_pitch;
            note_diff=0;
          }
        }  
      }
      else
        fund_pitch=the_pitch;
      freq=PI2*fund_pitch/SAMPLE_RATE;
      freqkey=freq*isa->cont[VCF_coff];
      freqeg2=freq*isa->cont[EG2_filt];
      osc_freq1=isa->cont[VCO1_pit]*fund_pitch/SAMPLE_RATE;
      osc_freq2=isa->cont[VCO2_pit]*fund_pitch/SAMPLE_RATE;
      osc_freq3=isa->cont[VCO3_pit]*fund_pitch/SAMPLE_RATE;

      // EG1 section
      calc_eg(eg1,eg1_phase,EG1_att,EG1_dec,EG1_sus,EG1_rel);

      // EG2 section
      calc_eg(eg2,eg2_phase,EG2_att,EG2_dec,EG2_sus,EG2_rel);
    }
    // LFO section
    lfo=oscillator(LFO_wf,lfo_pos,osc_freq4,lfo_waveform) * (1.+eg1_amount_o*(eg1-1.));

    // VCO 1 section
    osc1=oscillator(VCO1_wf,osc1_pos,osc_freq1,osc1_waveform,&trig_vco1_2);
    if (trig_vco1_2 && isa->trig_val) osc2_pos=0;

    freq_mod_2=(1.+eg2*eg2_amount_o)*(1.+lfo*lfo_amount_o); // only VCO2 is modulated
    // VCO 2 section
    osc2=oscillator(VCO2_wf,osc2_pos,
                    isa->is_fm ? osc_freq2*freq_mod_2*(1+fm_amount*osc1) : osc_freq2*freq_mod_2,
                    osc2_waveform);

    // VCO 3 section
    osc3=oscillator(0,osc3_pos,osc_freq3, osc3_waveform);

    float ns=get_noise();
    
    // mixer section
    input=bal_o1*osc1 + bal_o2*osc2 + bal_ns*ns;
    if (isa->ringm_vco) input *= osc3;
    else input *= 1. + osc3*vco3_amount_a;
    if (isa->ringm_lfo) input *= lfo;
    else input *= 1. + lfo*lfo_amount_a;

    // VCF section - Hal Chamberlin's state variable filter
    // 20Hz - 6.7KHz
    freqcut=fminmax(0.005,(freqkey+freqeg2*eg2)*(1. + lfo*lfo_amount_f + osc3*vco3_amount_f),FREQMAX);

    int fmo=isa->filter_mode;
    if (isa->is_pole4) {   //  24db per octave
      float fc1=freqcut/1.2,  // flatter peak
            fc2=freqcut*1.2;
      d2+=fc1*d1;                    // d2 = lowpass output (no clipping)
      float highpass=input-d2-qres*d1;
      d1+=fc1*highpass;              // d1 = bandpass output
      output= fmo==LOpass ? d2 : fmo==BANDpass ? d1 : highpass;

      if (isa->on_off[VCF_clip]) d4+=fc2*clip2(d3);   // d4 = lowpass output
      else d4+=fc2*d3;
      highpass=output-d4-qres*d3;
      d3+=fc2*highpass;            // d3 = bandpass output (clipping here yields strange effects)
      output= fmo==LOpass ? d4 : fmo==BANDpass ? d3 : highpass;
    }
    else {
      d2+=freqcut*d1;                    // d2 = lowpass output (no clipping)
      float highpass=input-d2-qres*d1;
      d1+=freqcut*highpass;              // d1 = bandpass output
      output= fmo==LOpass ? d2 : fmo==BANDpass ? d1 : highpass;
    }
    buffer[i] += amp_mul * output * eg1 * isa->cont[VOL]; // update the buffer
  }
  vco1_val=osc_freq1;
  vco2_val=osc_freq2*freq_mod_2;
  cutoff_val=freqcut/freq;
  return true;
}
