#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>

#include "amuc-headers.h"

WaveBuffer wave_buf;
static const int sample_rate=44100;

void WaveBuffer::init() { // must be called after start of X, because colours are used
  fname_nr=-1;
  filenames=new FileName[wavef_max];
  sample_dirs=new SampleDirs();
}

void WaveBuffer::reset_buffers() { // non-alloc'ed buf's are pointing to ScInfo::wavedata, which maybe has been deleted
  for (int i=0;i<colors_max;++i) {
    if (!w_buf[i].alloced) w_buf[i]=phm_buf.const_data[i];
  }
}

SampleDirs::SampleDirs() {
  dirs[0].name=cur_dir; dirs[0].col=xft_Black;
  dirs[1].name=wave_samples; dirs[1].col=xft_Blue;
  dirs_end=1;
}

void SampleDirs::reset() {
  wave_buf.fname_nr=-1;
  dirs_end=-1;
}

void SampleDirs::add_dir(const char *dname,XftColor *dircol) {
  if (dirs_end==dirs_max-1) {
    alert("add_dir: > %d directories",dirs_max);
    return;
  }
  ++dirs_end;
  dirs[dirs_end].name=dname;
  dirs[dirs_end].col=dircol;
}

bool SampleDirs::coll_wavefiles() {
  bool res=false;
  wave_buf.fname_nr=-1;
  for (int i=0;i<=dirs_end;++i) {
    if (wave_buf.coll_wavefiles(dirs[i].name,dirs[i].col)) res=true;
  }
  if (!res) alert("no wave files found");
  return res;
}

static int compar_fn(const void* a,const void* b) {
  return reinterpret_cast<const FileName*>(a)->nr > reinterpret_cast<const FileName*>(b)->nr;
}

bool WaveBuffer::fill_fname_array(const char *data_dir,const char *fnames[],int *fnumbers) {
  int nr;
  DIR *dp;
  dirent *dir;
  bool file_found=false;
  char *ext;
  if (!(dp=opendir(data_dir))) {
    alert("dir %s not accessable",data_dir);
    return false;
  }
  while ((dir=readdir(dp))!=0) {
    if (!dir->d_ino) { alert("d_ino?"); continue; }
    ext=strrchr(dir->d_name,'.');
    if (!ext || strcmp(ext,".wav")) continue;
    if (isdigit(dir->d_name[0])) {
      nr=atoi(dir->d_name);
      file_found=true;
      if (fname_nr==wavef_max-1) { alert("files > %d",wavef_max); return false; }
      ++fname_nr;
      fnames[fname_nr]=strdup(dir->d_name);
      fnumbers[fname_nr]=nr;
      if (debug) printf("f_fn_arr: %s %d\n",fnames[fname_nr],nr);
    }
  }
  closedir(dp);
  return file_found;
}

bool WaveBuffer::coll_wavefiles(const char *data_dir,XftColor *dircol) {
  const char *fnames[wavef_max];
  int fnumbers[wavef_max];
  int fst_fname_nr=fname_nr+1; // starts at 0
  const char *dir=strdup(data_dir);
  if (fill_fname_array(data_dir,fnames,fnumbers)) {
    for (int i=fst_fname_nr;i<=fname_nr;++i) {
      FileName *fn=filenames+i;
      fn->name=fnames[i];
      fn->nr=fnumbers[i];
      fn->dir=dir;
      fn->col=dircol;
    }
    qsort(filenames+fst_fname_nr,fname_nr+1-fst_fname_nr,sizeof(FileName),compar_fn);
    return true;
  }
  return false;
}

bool read_wav(FILE *src,ShortBuffer *sb) {
  char word[20];
  short dum16;
  int dum32,
      size,
      er_nr=0;
  if (
    (fread(word,4,1,src)!=1 && (er_nr=1)) || (strncmp(word,"RIFF",4) && (er_nr=2)) || 
    (fread(&dum32, 4,1,src)!=1 && (er_nr=3)) ||                      // header size
    (fread(word, 8,1,src)!=1 && (er_nr=4)) || (strncmp(word,"WAVEfmt ",8) && (er_nr=5)) || 
    (fread(&dum32, 4,1,src)!=1 && (er_nr=6)) || (dum32!=16 && (er_nr=7)) ||            // chunk size  
    (fread(&dum16, 2,1,src)!=1 && (er_nr=8))                         // format tag (1 = uncompressed PCM)
  ) goto error_rw; // { printf("erno=%d du=%d\n",er_nr,dum32); goto error_rw; }

  if (dum16!=1) { alert("format = %d, should be 1",dum16); return false; }
  if (fread(&dum16, 2,1,src)!=1) {                        // no of channels
    er_nr=11; goto error_rw;
  }
  if (dum16!=1) { alert("nr channels = %d, should be 1",dum16); return false; }
  if (fread(&dum32, 4,1,src)!=1) {                  // rate
    er_nr=13; goto error_rw;
  }
  if (dum32!=sample_rate) { alert("rate = %d, must be %d",dum32,sample_rate); return false; }
  if (fread(&dum32, 4,1,src)!=1) {                  // average bytes/sec
    er_nr=15; goto error_rw;
  }
  if (dum32!=sample_rate*2) { alert("byte/sec = %d, must be 2*%d",dum32,sample_rate); return false; }
  if ((fread(&dum16, 2,1,src)!=1 && (er_nr=16)) ||                // block align
      (fread(&dum16, 2,1,src)!=1 && (er_nr=17)))                  // bits per sample
    goto error_rw;
  if (dum16!=16) {
    alert("bits per sample is %d, must be 16",dum16); return false;
  }
  if ((fread(word, 4,1,src)!=1 && (er_nr=18)) || (strncmp(word,"data",4) && (er_nr=19)))
    goto error_rw;
  if (fread(&size, 4,1,src)!=1 && (er_nr=20))           // sample length
    goto error_rw;

  error_rw:   // this label must be before allocation of buf, else g++ will complain
  if (er_nr) {
    alert("format error (nr %d) in wave file",er_nr);
    return false;
  }
  char *buf=new char[size];
  if (fread(buf,size,1,src)!=1 && (er_nr=21)) {
    delete[] buf;
    goto error_rw;
  }
  sb->reset();
  sb->size=size/2; // sizeof(char)/sizeof(short) = 2
  sb->buf=new short[sb->size];
  if (debug) printf("wave size=%d\n",sb->size);
  sb->alloced=true;
  for (int n=0;n<sb->size;++n)
    sb->buf[n]=*(short*)(buf+2*n);
  delete[] buf;
  return true;
}

bool read_wav(const char *dir,const char *file,ShortBuffer *sb) {
  FILE *src=0;
  if (dir) {
    char path[max200];
    snprintf(path,max200,"%s/%s",dir,file);
    path[max200-1]=0;
    src=fopen(path,"r");
    if (!src) {
      alert("file '%s' not found",path); return false;
    }
  }
  else {
    src=fopen(file,"r");
    if (!src) {
      alert("file '%s' not found",file); return false;
    }
  }
  bool res=read_wav(src,sb);
  fclose(src);
  return res;
}
 
