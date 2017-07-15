struct FileName {
  int nr;
  XftColor *col;
  const char *name;
  const char *dir;
};

struct DirName {
  XftColor *col;
  const char *name;
};
  
struct SampleDirs {
  static const int dirs_max=3;
  int dirs_end;
  DirName dirs[dirs_max];
  SampleDirs();
  void reset();
  void add_dir(const char *dname,XftColor *dircol);
  bool coll_wavefiles();
};

struct WaveBuffer {
  int fname_nr;
  ShortBuffer w_buf[colors_max];  // modified at runtime
  FileName *filenames;
  SampleDirs *sample_dirs;
  void init();
  void reset_buffers();
  bool coll_wavefiles(const char *dir,XftColor *dircol);
  bool fill_fname_array(const char *data_dir,const char *fnames[],int *fnumbers);
};

bool read_wav(const char *dir,const char *file,ShortBuffer*);

extern WaveBuffer wave_buf;
extern const int wavef_max;
