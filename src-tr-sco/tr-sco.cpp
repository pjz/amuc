#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <str.h>
typedef unsigned int uint;

enum {  // from amuc.cpp
  eScore_start,  // saving and restoring from file
  eScore_end,    // not used
  ePlayNote=2,
  eGlobal=4,
  eScore_start4=7,
};
const uint eHi=1,eLo=2;

static char task;
static int st_shift,
           signs_mode=eLo;
static const int sclin_max=45; // from amuc.h

void alert(const char *form,...) {
  char buf[100];
  va_list ap;
  va_start(ap,form);
  vsnprintf(buf,100,form,ap);
  va_end(ap);
  puts(buf);
}

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

bool midinr_to_lnr(int mnr,int& lnr,int& sign) {
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

bool transform(FILE *in,FILE *out,char mode) {
  int res;
  uint opc=0;
  Str save_buf;
  char textbuf[100];
  for (;;) {
    save_buf.rword(in," \n");
    if (!save_buf.s[0]) break;
    opc=atoi(save_buf.s);
    switch (opc) {
      case eGlobal: {
          int meter;
          if (sscanf(save_buf.s,"%*um%d",&meter)!=1) {
            alert("bad code: %s",save_buf.s); return false;
          }
          fprintf(out,"%um%d",eGlobal,mode=='h' ? meter/2 : mode=='d' ? meter*2 : meter);
        }
        break;
      case eScore_start4: {
          int sc_meter,end_sect,ngroup,sc_key_nr;
          res=sscanf(save_buf.s,"%*um%de%dg%dk%d",
                     &sc_meter,
                     &end_sect,&ngroup,&sc_key_nr);
          if (res!=4) {
            alert("bad code: %s",save_buf.s);
            return false;
          };
          if (fscanf(in," \"%50[^\"]s",textbuf)!=1) { alert("missing text"); return false; }
          textbuf[50]=0;
          getc(in);
          switch (mode) {
            case 'h': end_sect/=2; break;
            case 'd': end_sect*=2; break;
            case 's': sc_key_nr=0; break;
          }
          fprintf(out,"\n%um%de%dg%dk%d ",eScore_start4,sc_meter,end_sect,ngroup,sc_key_nr);
          fprintf(out,"\"%s\" ",textbuf);
        }
        break;
      case ePlayNote: {
          int lnr,snr,dur,sign,stacc_sampl,col,gnr,dlnr=0,dsnr=0,del_s=0,del_e=0;
          res=sscanf(save_buf.s,"%*uL%dN%dd%di%ds%dc%dg%dp%d,%dD%d,%d",
                     &lnr,&snr,&dur,&sign,&stacc_sampl,&col,&gnr,&dlnr,&dsnr,&del_s,&del_e);
          if (res<7 || res>11) {
            alert("bad code: %s",save_buf.s); return false;
          }
          const char *fstring="%uL%dN%dd%di%ds%dc%dg%d ";
          int sampled= (stacc_sampl>>1) & 1;
          if (mode=='h') {
            if (sampled)
              fprintf(out,fstring,ePlayNote,lnr,snr/2,dur,sign,stacc_sampl,col,gnr);
            else if (dur>1)
              fprintf(out,fstring,ePlayNote,lnr,snr/2,dur/2,sign,stacc_sampl,col,gnr);
          }
          else if (mode=='d') {
            if (sampled)
              fprintf(out,fstring,ePlayNote,lnr,snr*2,dur,sign,stacc_sampl,col,gnr);
            else
              fprintf(out,fstring,ePlayNote,lnr,snr*2,dur*2,sign,stacc_sampl,col,gnr);
          }
          else if (mode=='q') { 
            if (sampled)
              fprintf(out,fstring,ePlayNote,lnr,snr,dur,sign,stacc_sampl,col,gnr);
            else
              fprintf(out,fstring,ePlayNote,lnr,snr,dur,sign,stacc_sampl,col,gnr);
          }
          else if (mode=='s') {
            if (sampled)
              fprintf(out,fstring,ePlayNote,lnr,snr,dur,sign,stacc_sampl,col,gnr);
            else {
              int midinr=lnr_to_midinr(lnr,sign);
              bool ok=midinr_to_lnr(midinr+st_shift,lnr,sign);
              if (ok) 
                fprintf(out,fstring,ePlayNote,lnr,snr,dur,sign,stacc_sampl,col,gnr);
            }
          }
          else { alert("transform: mode %c?",mode); exit(1); }
        }
        break;
      default: alert("tr-sco: unknown opcode %d",opc); return false;
    }
  }
  return true;
}

void usage() {
    puts("Usage:");
    puts("  tr-sco <options> <score-file>");
    puts("Options:");
    puts("  -trh : write file tr.sco, notes halved");
    puts("  -trd : write file tr.sco, notes doubled");
    puts("  -trq : write file tr.sco, notes quantitized, delays omitted");
    puts("  -trs <n> : write file tr.sco, notes shifted n semi-tones (accidentals will be flat's)");
    puts("  -hi  : with -trs, accidentals will be sharp's");
    puts("Modified score file written to tr.sco");
}

int main(int argc,char **argv) {
  char *inf=0;
  for (int an=1;an<argc;++an) {
    if (!strcmp(argv[an],"-h")) {
      usage(); exit(1);
    }
    if (!strncmp(argv[an],"-tr",3)) {
      task=argv[an][3];
      if (!task || !strchr("hdqs",task)) {
        alert("tr-sco: option should be -trh, -trd, -trq or -trs <n>");
        exit(1);
      }
      if (task=='s') {
        if (++an==argc) { alert("number after -trs missing"); exit(1); }
        st_shift=atoi(argv[an]);
      }
    }
    else if (!strcmp(argv[an],"-hi"))
      signs_mode=eHi;
    else inf=argv[an];
  }
  if (!inf) { alert("input file missing"); exit(1); }
  if (!task) { alert("what must be done?"); exit(1); }
  if (!strcmp(inf,"tr.sco")) { alert("please rename input file tr.sco"); exit(1); }

  FILE *in_tr=fopen(inf,"r");
  if (!in_tr) { alert("Input file %s could not be opened",inf); exit(1); }
  FILE *out_tr=fopen("tr.sco","w");
  if (!out_tr) { alert("Output file tr.sco could not be opened"); exit(1); }
  
  if (!transform(in_tr,out_tr,task)) exit(1);
  puts("Output file: tr.sco"); 
}
