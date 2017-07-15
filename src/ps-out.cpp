#include <stdio.h>
#include <string.h>
#include "amuc-headers.h"

PostscriptOut ps_out;

static const int
  subv_max=3,
  voice_len_max=5000;  // max chars

static char
  program[colors_max*2*voice_len_max],
  *prog,
  buf1[10],buf2[10];

struct SubVoice {
  char buf[voice_len_max], *bp;
  int busy,
      prev_mnr;     // previous non-rest measure nr
  void reset() {
    buf[0]=0;
    bp=buf;
    busy=0;
    prev_mnr=0;
  }
};

struct Voice {
  SubVoice sv[subv_max];
  uchar local_lsign[sclin_max];
  int voice_key_nr;
  bool chord_warn;  // if s_voice: warning about chord given?
  void reset() {
    chord_warn=false;
    for (int i=0;i<subv_max;++i) sv[i].reset();
  }
};

Voice voices[groupnr_max][colors_max],
      perc_voices[groupnr_max];

const int good_len[]={ 32,24,16,14,12,8,7,6,4,3,2,1,0 };

struct PsNote {
  int mnr;
  uchar time,
        col,notegr,
        dur,extra_dur,next_dur,
        lnr,note_sign;
  char pref_subv;   // preferred subvoice (in case of tie to previous note)
  bool sampled;
  PsNote *tied;     // tied to next note
  PsNote(int c,int ng,int mn,int t,int ln,int ns,int d):
      mnr(mn),
      time(t),
      col(c),
      notegr(ng),
      dur(d),
      extra_dur(0),
      next_dur(0),
      lnr(ln),
      note_sign(ns),
      pref_subv(nop),
      sampled(false),
      tied(0) {
    int trans=ps_out.transp[notegr][col],
        subd=ps_out.nu_subd;
    if (trans) {
      if (!midinr_to_lnr(lnr_to_midinr(lnr,note_sign)+trans,
                         lnr,note_sign,signsMode[voices[notegr][col].voice_key_nr])) {
        alert("out of range midi note");
        lnr=35; note_sign=0; // middle C
      }
    }
    if (time + dur>ps_out.meter) {
      next_dur=dur - ps_out.meter + time;
      dur-=next_dur;
    }
    extra_dur= time%subd && time%subd+dur>subd ? dur - subd + time%subd : 0;
  }
  PsNote(int c,int ng,int mn,int t,int d):  // sampled note
      mnr(mn),
      time(t),
      col(c),
      notegr(ng),
      dur(d),
      extra_dur(0),
      sampled(true),
      tied(0) {
  }

  bool operator<(PsNote &other) { return time < other.time; }
  bool operator==(PsNote &other) { return false; }   // always insert
  const char* abc_note_name();
  const char* abc_perc_note_name();
};

const char *PsNote::abc_note_name() {
  // abc note name: midi_nr=60 -> abc note = 'c'
  static const char *name_hi[84]={
     "C,,,","^C,,,","D,,,","^D,,,","E,,,","F,,,","^F,,,","G,,,","^G,,,","A,,,","^A,,,","B,,,",
     "C,," ,"^C,," ,"D,," ,"^D,," ,"E,," ,"F,," ,"^F,," ,"G,," ,"^G,," ,"A,," ,"^A,," ,"B,,",
     "C,"  ,"^C,"  ,"D,"  ,"^D,"  ,"E,"  ,"F,"  ,"^F,"  ,"G,"  ,"^G,"  ,"A,"  ,"^A,"  ,"B,",
     "C"   ,"^C"   ,"D"   ,"^D"   ,"E"   ,"F"   ,"^F"   ,"G"   ,"^G"   ,"A"   ,"^A"   ,"B",
     "c"   ,"^c"   ,"d"   ,"^d"   ,"e"   ,"f"   ,"^f"   ,"g"   ,"^g"   ,"a"   ,"^a"   ,"b",
     "c'"  ,"^c'"  ,"d'"  ,"^d'"  ,"e'"  ,"f'"  ,"^f'"  ,"g'"  ,"^g'"  ,"a'"  ,"^a'"  ,"b'",
     "c''" ,"^c''" ,"d''" ,"^d''" ,"e''" ,"f''" ,"^f''" ,"g''" ,"^g''" ,"a''" ,"^a''" ,"b''" };
  static const char *name_lo[84]={
     "C,,,","_D,,,","D,,,","_E,,,","E,,,","F,,,","_G,,,","G,,,","_A,,,","A,,,","_B,,,","B,,,",
     "C,," ,"_D,," ,"D,," ,"_E,," ,"E,," ,"F,," ,"_G,," ,"G,," ,"_A,," ,"A,," ,"_B,," ,"B,,",
     "C,"  ,"_D,"  ,"D,"  ,"_E,"  ,"E,"  ,"F,"  ,"_G,"  ,"G,"  ,"_A,"  ,"A,"  ,"_B,"  ,"B,",
     "C"   ,"_D"   ,"D"   ,"_E"   ,"E"   ,"F"   ,"_G"   ,"G"   ,"_A"   ,"A"   ,"_B"   ,"B",
     "c"   ,"_d"   ,"d"   ,"_e"   ,"e"   ,"f"   ,"_g"   ,"g"   ,"_a"   ,"a"   ,"_b"   ,"b",
     "c'"  ,"_d'"  ,"d'"  ,"_e'"  ,"e'"  ,"f'"  ,"_g'"  ,"g'"  ,"_a'"  ,"a'"  ,"_b'"  ,"b'",
     "c''" ,"_d''" ,"d''" ,"_e''" ,"e''" ,"f''" ,"_g''" ,"g''" ,"_a''" ,"a''" ,"_b''" ,"b''" };
  static const char *eq_name[84]={
     "=C,,,",0,"=D,,,",0,"=E,,,","=F,,,",0,"=G,,,",0,"=A,,,",0,"=B,,,",
     "=C,," ,0,"=D,," ,0,"=E,," ,"=F,," ,0,"=G,," ,0,"=A,," ,0,"=B,,",
     "=C,"  ,0,"=D,"  ,0,"=E,"  ,"=F,"  ,0,"=G,"  ,0,"=A,"  ,0,"=B,",
     "=C"   ,0,"=D"   ,0,"=E"   ,"=F"   ,0,"=G"   ,0,"=A"   ,0,"=B",
     "=c"   ,0,"=d"   ,0,"=e"   ,"=f"   ,0,"=g"   ,0,"=a"   ,0,"=b",
     "=c'"  ,0,"=d'"  ,0,"=e'"  ,"=f'"  ,0,"=g'"  ,0,"=a'"  ,0,"=b'",
     "=c''" ,0,"=d''" ,0,"=e''" ,"=f''" ,0,"=g''" ,0,"=a''" ,0,"=b''" };
  int ind;
  bool eq=false;
  uchar line_sign=voices[notegr][col].local_lsign[lnr];
  if (line_sign==note_sign)
    ind=lnr_to_midinr(lnr,0) - 24;
  else if (line_sign && !note_sign) {
    ind=lnr_to_midinr(lnr,0) - 24;
    eq=true;
  }
  else
    ind=lnr_to_midinr(lnr,note_sign) - 24;
  if (ind<0) { alert("abc_note_nname: note too low"); return 0; }
  if (ind>=84) { alert("abc_note_nname: note too high"); return 0; }
  if (debug) printf("abc_nname: ind:%d lsign:%d nsign:%d eq:%d nhi:%s nlo:%s neq:%s\n",
    ind,line_sign,note_sign,eq,name_hi[ind],name_lo[ind],eq_name[ind]);
  if (eq) return eq_name[ind];
  switch (note_sign) {
    case eHi:
    case 0: return name_hi[ind];
    case eLo: return name_lo[ind];
    default: return 0;
  }
}

const char *PsNote::abc_perc_note_name() {
  static const char *name[colors_max]={ "C","E","G","B","^d","^f" };
  return name[col];
}

Array<SLinkedList<struct PsNote>,1000> psn_buf;  // max 1000 measures

int transp_key(int key_nr,int tr) { // notice: key_names[6] and key_names[7] are equivalent
  if (!tr) return key_nr;
  int ind=0;
  const int max1=keys_max-1;
  if (tr<0) tr=-(-tr%12);
  else tr=tr%12;
  if (key_nr>5) --key_nr;
  ind=key_nr+tr;
  if (ind<0) ind+=max1;
  else if (ind>=max1) ind-=max1;
  if (ind>5) ++ind;
  return ind;
}

void PostscriptOut::set(int m,int nu,int knr,char *t) {
  meter=m;
  nupq=nu_subd=nu;
  lst_ind=-1;
  key_nr=knr % keys_max; // only major keys
  title=t;
  for (int i=0;i<colors_max;++i)
    for (int grn=0;grn<groupnr_max;++grn)
      voices[grn][i].voice_key_nr=transp_key(key_nr,transp[grn][i]);
}

void PostscriptOut::init_ps() {
  for (int c=0;c<colors_max;++c)
    for (int g=0;g<groupnr_max;++g) transp[g][c]=0;
  header_file=0;
  s_voice=false;
  for (int i=0;i<annot_max;++i) { annot[i].mnr=-1; annot[i].ch='A'+i; }
  // rest is handled by set()
}

void PostscriptOut::reset_ps() {
  for (int i=0;i<=lst_ind;++i) psn_buf[i].reset();
}

static void test_dur(int mnr,uchar& dur) {
  for (int i=0;;++i) {
    if (good_len[i]<dur) {
      int old_dur=dur;
      dur=good_len[i];
      alert("warning: measure %d: note length %d -> %d",mnr,old_dur,dur);
      break;
    }
    if (dur==good_len[i]) break;
  }
}

void PostscriptOut::insert(int col,int grn,int ev_time,int lnr,int note_sign,int dur) {
  int ind=ev_time/meter;
  if (ind>lst_ind)
    lst_ind=ind;
  PsNote note(col,grn,ind,ev_time % meter,lnr,note_sign,dur);
  if (debug) printf("insert ps-note: tim=%d dur=%d\n",ev_time % meter,dur);
  PsNote *np=&psn_buf[ind].insert(note,true)->d;
  if (np->extra_dur>0) test_dur(ind,np->extra_dur);
  else test_dur(ind,np->dur);
  while (np->next_dur>0) {
    ++ind;
    PsNote note1(col,grn,ind,0,lnr,note_sign,np->next_dur);
    test_dur(ind,note1.dur);
    np->tied=&psn_buf[ind].prepend(note1)->d;
    if (ind>lst_ind) lst_ind=ind;
    np=&note1;
    if (np->extra_dur>0) test_dur(ind,np->extra_dur);
    else test_dur(ind,np->dur);
  }
}

void PostscriptOut::insert_perc(int col,int grn,int ev_time) {
  int ind=ev_time/meter;
  if (ind>lst_ind)
    lst_ind=ind;
  PsNote perc_note(col,grn,ind,ev_time % meter,nupq/2);
  if (debug) printf("insert ps-perc-note: tim=%d\n",ev_time % meter);
  psn_buf[ind].insert(perc_note,true);
}

static char *n_len(char *buf,int dur) {  // abc note length code
  if (dur==1) strcpy(buf,"");
  else sprintf(buf,"%d",dur);
  return buf;
}

static char *r_len(char *buf,int mnr,int dur,const char *r) {  // abc rest code
  for (int i=0;;++i) {
    if (good_len[i]<dur) {
      for (int j=0;;++j) {
        if (!good_len[j]) {
          alert("measure %d: bad rest length %d",mnr,dur);
          sprintf(buf,"%s%d",r,dur);
          break;
        }
        if (good_len[j]<dur) {
          sprintf(buf,"%s%d%s%d",r,good_len[j],r,dur-good_len[j]);
          break;
        }
      }
      break;
    }
    if (dur==good_len[i]) {
      sprintf(buf,"%s%d",r,dur);
      break;
    }
  }
  return buf;
}

static bool is_chord(SLList_elem<PsNote> *psn,SLList_elem<PsNote> *psn1) {
  if (psn1->d.sampled != psn->d.sampled) return false;
  return psn1->d.col==psn->d.col && psn1->d.notegr==psn->d.notegr && psn1->d.time==psn->d.time && psn1->d.dur==psn->d.dur;
}

static char *add_rest(int mnr,int time,char *bp,const char *rest,int len) {
  int subd=ps_out.nu_subd,
      extra_len= time%subd && time%subd+len>subd ? len - subd + time%subd : 0;
  if (time>0 && time%4==0) strcpy(bp++," "); // interrupt note beaming
  if (extra_len) {
    bp += sprintf(bp,"%s",r_len(buf1,mnr,len-extra_len,rest));
    if ((time+len-extra_len)%4==0) strcpy(bp++," ");
    bp += sprintf(bp,"%s",r_len(buf2,mnr,extra_len,rest));
  }
  else
    bp += sprintf(bp,"%s",r_len(buf1,mnr,len,rest));
  return bp;
}

bool PostscriptOut::find_ai(int mnr,int& ai) {
  for (ai=0;ai<annot_max && annot[ai].mnr>=0;++ai)
    if (mnr==annot[ai].mnr) return true;
  return false;
}

void PostscriptOut::add_Z_rest(Voice *voice,int subv_nr,int meas_nr) {
  SubVoice *subv=voice->sv+subv_nr;
  if (meas_nr-subv->prev_mnr > 0) {
    int mnr,d_mnr,ai;
    for (mnr=subv->prev_mnr;mnr<meas_nr;++mnr) {
      if (find_ai(mnr,ai)) {
        d_mnr=mnr - subv->prev_mnr;
        if (d_mnr==1)
          subv->bp += sprintf(subv->bp,"z%d|",meter);
        else if (d_mnr>1) // could be 0
          subv->bp += sprintf(subv->bp,"Z%d|",d_mnr);
        subv->bp += sprintf(subv->bp,"\"^%c\"",annot[ai].ch);
        subv->prev_mnr=mnr;
      }
    }
    d_mnr=meas_nr-subv->prev_mnr;
    if (d_mnr==1)
      subv->bp += sprintf(subv->bp,"z%d|",meter);
    else
      subv->bp += sprintf(subv->bp,"Z%d|",d_mnr);
  }
  subv->prev_mnr=meas_nr;
}

void PostscriptOut::add_first_rests(Voice *voice,int subv_nr,int meas_nr,const char* rest) {
  int i,ai=0;
  SubVoice *subv=voice->sv+subv_nr;
  for (i=0;i< meas_nr;++i) {
    if (subv_nr==0 && i==annot[ai].mnr) {
      subv->bp += sprintf(subv->bp,"\"^%c\"",annot[ai].ch);
      ++ai;
    }
    subv->bp += sprintf(subv->bp,"%s%d|",rest,meter);
  }
}

void PostscriptOut::add_last_rest(Voice *voice,int subv_nr,int meas_nr,const char* rest,int annot_ch) {
  SubVoice *subv=voice->sv+subv_nr;
  if (subv->buf[0]) {
    if (subv->busy<meter) {
      if (subv_nr==0 && annot_ch && subv->busy==0)
        subv->bp += sprintf(subv->bp,"\"^%c\"",annot_ch);
      subv->bp=add_rest(meas_nr,subv->busy,subv->bp,rest,meter - subv->busy);
    }
    subv->bp=stpcpy(subv->bp,"|");
  }
}

bool PostscriptOut::fill_subvoice(const int meas_nr,int subv_nr,int annot_ch,bool& done) {
  SLList_elem<PsNote> *psn,*psn1;
  Voice *voice;
  SubVoice *subv;
  const char *rest= subv_nr==0 || s_voice ? "z" : "x";
  if (debug) printf("fsub meas=%d subv_nr=%d\n",meas_nr,subv_nr);
  for (int c=0;c<colors_max;++c)
    for (int n=0;n<groupnr_max;++n)
      voices[n][c].sv[subv_nr].busy=0;
  for (int n=0;n<groupnr_max;++n)
    perc_voices[n].sv[subv_nr].busy=0;
  done=true;
  for (psn=psn_buf[meas_nr].lis;psn;) {
    if (psn->d.sampled) {
      psn=psn->nxt; continue;
    }
    done=false;
    bool chord=false;
    voice=voices[psn->d.notegr]+psn->d.col;
    subv=voice->sv+subv_nr;
    if (debug) printf("meas_nr=%d tim=%d dur=%d busy=%d col=%s tied=%p pref_subv=%d\n",
      meas_nr,psn->d.time,psn->d.dur,subv->busy,color_name[psn->d.col],psn->d.tied,psn->d.pref_subv);
    if (psn->d.time<subv->busy ||
        (psn->d.pref_subv >=0 && psn->d.pref_subv != subv_nr)) {  // test preferred subvoice nr
      if (debug) puts("skip");
      psn=psn->nxt;
      continue;
    }
    if (!subv->buf[0] && !s_voice)
      add_first_rests(voice,subv_nr,meas_nr,rest);
    if (s_voice && subv_nr>0)
      alert("warning: multiple voice in measure %d (color:%s group:%d)",
             meas_nr,color_name[psn->d.col],psn->d.notegr);
    if ((subv_nr==0 || s_voice) && annot_ch && subv->busy==0) {
      if (s_voice) add_Z_rest(voice,subv_nr,meas_nr);
      subv->bp += sprintf(subv->bp,"\"^%c\"",annot_ch);
    }
    else if (s_voice)
      add_Z_rest(voice,subv_nr,meas_nr); // now: prev_mnr = meas_nr
    if (subv->busy < psn->d.time)
      subv->bp=add_rest(psn->d.mnr,subv->busy,subv->bp,rest,psn->d.time - subv->busy);
    if (psn->d.time>0 && psn->d.time%4==0)  // interrupt note beaming
      strcpy(subv->bp++," ");
    subv->busy=psn->d.time + psn->d.dur;
    for (psn1=psn->nxt;psn1 && psn1->d.time < subv->busy;psn1=psn1->nxt) {
      if (psn1->d.sampled)
        continue;
      if (is_chord(psn,psn1)) {
        chord=true;
        if (s_voice && !voice->chord_warn) {
          voice->chord_warn=true;
          alert("warning: chord in measure %d (color:%s group:%d)",meas_nr,color_name[psn->d.col],psn->d.notegr);
        }
        subv->bp=stpcpy(subv->bp,"[");
        break; 
      }
    }
    if (subv->bp - subv->buf > voice_len_max-100) {
      alert("fill_subvoice: buffer overflow");
      return false;
    }
    //if (psn->d.fst_of_triplet) subv->bp=stpcpy(subv->bp,"(3");
    const char *nn1=psn->d.abc_note_name();
    voices[psn->d.notegr][psn->d.col].local_lsign[psn->d.lnr]=psn->d.note_sign;
    if (debug) printf("time=%d, abc nname:%s\n",psn->d.time,nn1);
    if (psn->d.extra_dur) {
      if (chord)  // too difficult!
        subv->bp += sprintf(subv->bp,"%s%s", nn1, n_len(buf1,psn->d.dur));
      else subv->bp += sprintf(subv->bp,"%s%s-%s%s",
                               nn1,n_len(buf1,psn->d.dur-psn->d.extra_dur),
                               psn->d.abc_note_name(),   // maybe different name
                               n_len(buf2,psn->d.extra_dur));
    }
    else
      subv->bp += sprintf(subv->bp,"%s%s", nn1, n_len(buf1,psn->d.dur));
    if (psn->d.tied) {
      psn->d.tied->pref_subv=subv_nr;
      strcpy(subv->bp++,"-");
    }
    if (chord) {
      for (psn1=psn->nxt;psn1 && psn1->d.time < subv->busy;)
        if (is_chord(psn,psn1)) {
          nn1=psn1->d.abc_note_name();
          subv->bp += sprintf(subv->bp,"%s%s",nn1,n_len(buf1,psn->d.dur));
          psn1=psn_buf[meas_nr].remove(psn1);
        }
        else psn1=psn1->nxt;
      subv->bp=stpcpy(subv->bp,"]");
    }
    psn=psn_buf[meas_nr].remove(psn);
  }
  for (int i=0;i<colors_max;++i)
    for (int n=0;n<groupnr_max;++n) {
      voice=voices[n]+i;
      subv=voice->sv+subv_nr;
      if (s_voice) {
        if (subv->busy) {
          add_last_rest(voice,subv_nr,meas_nr,rest,annot_ch);
          ++subv->prev_mnr;
        }
      }
      else {
        add_last_rest(voice,subv_nr,meas_nr,rest,annot_ch);
      }
    }
  if (debug) puts("---");
  return true;
}

bool PostscriptOut::fill_perc_subv(const int meas_nr,int subv_nr,int annot_ch,bool& done) {
  SLList_elem<PsNote> *psn,*psn1;
  Voice *voice;
  SubVoice *subv;
  const char *rest= subv_nr==0 || s_voice ? "z" : "x";
  done=true;
  for (psn=psn_buf[meas_nr].lis;psn;) {
    if (!psn->d.sampled) {
      psn=psn->nxt; continue;
    }
    done=false;
    bool chord=false;
    voice=perc_voices+psn->d.notegr;
    subv=voice->sv+subv_nr;
    if (psn->d.time<subv->busy) {
      if (debug) puts("skip");
      psn=psn->nxt;
      continue;
    }
    if (!subv->buf[0] && !s_voice) 
      add_first_rests(voice,subv_nr,meas_nr,rest);
    if ((subv_nr==0 || s_voice) && annot_ch && subv->busy==0) {
      if (s_voice) add_Z_rest(voice,subv_nr,meas_nr);
      subv->bp += sprintf(subv->bp,"\"^%c\"",annot_ch);
    }
    else
      if (s_voice) add_Z_rest(voice,subv_nr,meas_nr);
    if (subv->busy < psn->d.time)
      subv->bp=add_rest(meas_nr,subv->busy,subv->bp,rest,psn->d.time - subv->busy);
    subv->busy=psn->d.time + psn->d.dur;
    if (psn->d.time>0 && psn->d.time%4==0)  // interrupt percussion-note beaming
      strcpy(subv->bp++," ");
    for (psn1=psn->nxt;psn1 && psn1->d.time < subv->busy;psn1=psn1->nxt) {
      if (!psn1->d.sampled)
        continue;
      chord=true;
      subv->bp=stpcpy(subv->bp,"[");
      break; 
    }
    if (subv->bp - subv->buf > voice_len_max-100) {
      alert("fill_perc_subv: buffer overflow");
      return false;
    }
    const char *nn=psn->d.abc_perc_note_name();
    subv->bp += sprintf(subv->bp,"%s%s", nn, n_len(buf1,psn->d.dur));
    if (chord) {
      for (psn1=psn->nxt;psn1 && psn1->d.time < subv->busy;) {
        nn=psn1->d.abc_perc_note_name();
        subv->bp += sprintf(subv->bp,"%s%s",nn,n_len(buf1,psn->d.dur));
        psn1=psn_buf[meas_nr].remove(psn1);
      }
      subv->bp=stpcpy(subv->bp,"]");
    }
    psn=psn_buf[meas_nr].remove(psn);
  }
  for (int n=0;n<groupnr_max;++n) {
    voice=perc_voices+n;
    subv=voice->sv+subv_nr;
    if (s_voice) {
      if (subv->busy) {
        add_last_rest(voice,subv_nr,meas_nr,rest,annot_ch);
        ++subv->prev_mnr;
      }
    }
    else {
      add_last_rest(voice,subv_nr,meas_nr,rest,annot_ch);
    }
  }
  return true;
}

static int calc_svnr(int gr_nr,int voice_nr,int subv_nr) {
  return subv_max*(gr_nr*colors_max + voice_nr) + subv_nr + 1;
}

static int calc_perc_svnr(int gr_nr,int subv_nr) {
  return subv_max*gr_nr + subv_nr + 1;
}

void PostscriptOut::print_subvoice(int voice_nr,int group_nr,Voice *voice,int subv) {
  if (!voice->sv[subv].buf[0]) return;
  prog += sprintf(prog,"V:%d\n",calc_svnr(group_nr,voice_nr,subv));
  if (subv==0)
    prog += sprintf(prog,"[K:%s]",maj_min_keys[voice->voice_key_nr]);
  prog += sprintf(prog,"%s|\n",voice->sv[subv].buf);
}

void PostscriptOut::print_perc_subvoice(int group_nr,Voice *voice,int subv) {
  if (!voice->sv[subv].buf[0]) return;
  prog += sprintf(prog,"V:P%d\n",calc_perc_svnr(group_nr,subv));
  prog += sprintf(prog,"%s|\n",voice->sv[subv].buf);
}

void PostscriptOut::write_ps(bool abc_only) {
  int meas_nr,
      subv_nr,
      v_nr,
      gr_nr,
      annot_char,
      a_ind=0;
  bool done;
  SubVoice *subv;
  for (int c=0;c<colors_max;++c)
    for (int n=0;n<groupnr_max;++n)
      voices[n][c].reset();
  for (int n=0;n<groupnr_max;++n)
    perc_voices[n].reset();
  for (meas_nr=0;meas_nr<=lst_ind;++meas_nr) {
    if (find_ai(meas_nr,a_ind)) // annotation
      annot_char=annot[a_ind].ch;
    else annot_char=0;
    for (done=false,subv_nr=0;subv_nr<subv_max;++subv_nr) {
      for (int c=0;c<colors_max;++c)
        for (int g=0;g<groupnr_max;++g)
          for (int lnr=0;lnr<sclin_max;++lnr)  // reset line sign inside measure to global value
            voices[g][c].local_lsign[lnr]=signs[voices[g][c].voice_key_nr][lnr%7];
      if (!fill_subvoice(meas_nr,subv_nr,annot_char,done)) return;
      if (done) break;
    }
    for (done=false,subv_nr=0;subv_nr<subv_max;++subv_nr) {
      if (!fill_perc_subv(meas_nr,subv_nr,annot_char,done)) return;
      if (done) break;
    }
  }
  prog=program;
  if (header_file) {
    FILE *hf=fopen(header_file,"r");
    if (!hf) { alert("header %s not found",header_file); return; }
    for (;;) {
      if (!fgets(prog,100,hf)) break;
      prog+=strlen(prog);
    }
    fclose(hf);
    printf("Header:%s\nVoices:\n",header_file);
    for (v_nr=0;v_nr<colors_max;++v_nr)    // stave name
      for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {
        subv=voices[gr_nr][v_nr].sv;
        if (subv->buf[0])
          printf("  name:%-2d color:%-6s group:%d\n",
                      calc_svnr(gr_nr,v_nr,0),color_name[v_nr],gr_nr);
      }
    for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {
      subv=perc_voices[gr_nr].sv;
      if (subv->buf[0])
        printf("  name:P%d (percussion) group:%d\n",calc_perc_svnr(gr_nr,0),gr_nr);
    }
  }
  else {
    prog += sprintf(prog,"X:1\n");             // start
    prog += sprintf(prog,"T:%s\n",title);      // title
    prog += sprintf(prog,"M:%d/4\n",meter*4/nupq/4);  // meter
    prog += sprintf(prog,"L:1/%d\n",4*nupq);   // unit note length
    prog += sprintf(prog,"K:%s\n",maj_min_keys[key_nr]); // key
    prog += sprintf(prog,"%%%%staves [");      // stave numbers
    for (v_nr=0;v_nr<colors_max;++v_nr)
      for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {
        subv=voices[gr_nr][v_nr].sv;
        if (subv->buf[0]) {
          if (voices[gr_nr][v_nr].sv[1].buf[0]) {
            prog=stpcpy(prog,"(");
            for (int i=0;i<subv_max && voices[gr_nr][v_nr].sv[i].buf[0];++i)
              prog += sprintf(prog,"%d ",calc_svnr(gr_nr,v_nr,i));
            prog=stpcpy(prog-1,") ");
          }
          else
            prog += sprintf(prog,"%d ",calc_svnr(gr_nr,v_nr,0));
        }   
      }
    for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {
      subv=perc_voices[gr_nr].sv;
      if (subv->buf[0]) {
        if (perc_voices[gr_nr].sv[1].buf[0]) {
          prog=stpcpy(prog,"(");
          for (int i=0;i<subv_max && perc_voices[gr_nr].sv[i].buf[0];++i)
            prog += sprintf(prog,"P%d ",calc_perc_svnr(gr_nr,i));
          prog=stpcpy(prog-1,") ");
        }
        else
          prog += sprintf(prog,"P%d ",calc_perc_svnr(gr_nr,0));
      }   
    }
    prog=stpcpy(prog-1,"]\n");
    for (v_nr=0;v_nr<colors_max;++v_nr)    // stave name
      for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {
        subv=voices[gr_nr][v_nr].sv;
        if (debug) printf("v=%d g=%d buf[0]=%d svnr=%d\n",v_nr,gr_nr,subv->buf[0],calc_svnr(gr_nr,v_nr,0));
        if (subv->buf[0]) {
          prog += sprintf(prog,"V:%d\tnm=\"%s-%d\"\tsnm=\"%s-%d\"\tclef=treble\n",
                          calc_svnr(gr_nr,v_nr,0),color_name[v_nr],gr_nr,color_name[v_nr],gr_nr);
        }
      }
    for (gr_nr=0;gr_nr<groupnr_max;++gr_nr) {  // perc stave name
      subv=perc_voices[gr_nr].sv;
      if (debug) printf("g=%d buf[0]=%d svnr=%d\n",gr_nr,subv->buf[0],calc_perc_svnr(gr_nr,0));
      if (subv->buf[0]) {
        prog += sprintf(prog,"V:P%d\tnm=\"perc-%d\"\tsnm=\"perc-%d\"\tclef=perc\n",
                        calc_perc_svnr(gr_nr,0),gr_nr,gr_nr);
      }
    }
  }
  for (v_nr=0;v_nr<colors_max;++v_nr)
    for (gr_nr=0;gr_nr<groupnr_max;++gr_nr)
      for (subv_nr=0;subv_nr<subv_max;++subv_nr)
        print_subvoice(v_nr,gr_nr,&voices[gr_nr][v_nr],subv_nr);
  for (gr_nr=0;gr_nr<groupnr_max;++gr_nr)
    for (subv_nr=0;subv_nr<subv_max;++subv_nr)
      print_perc_subvoice(gr_nr,perc_voices+gr_nr,subv_nr);
  if (abc_only) {
    FILE *ps_f;
    if ((ps_f=fopen(abc_or_ps_file,"w"))==0)
      alert("abc-file '%s' not opened",abc_or_ps_file);
    else {
      fputs(program,ps_f);
      fclose(ps_f);
    }
  }
  else {
    int severity;
    if ((severity=gen_ps(program,abc_or_ps_file))!=0)
      alert("troubles, severity=%d",severity);
  }
}
 
