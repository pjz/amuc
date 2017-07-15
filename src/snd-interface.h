struct SndInterf {
  int periodsize;  // from alsa
  bool okay;
  SndInterf();
  ~SndInterf();
  void snd_write(short *buf);
};

struct JackInterf {
  const char *client_name;
  bool okay,
       &is_playing;
  bool (*play)(float *buf_left,float *buf_right);
  int done,  // e.g. 'done'
      buf_size,  // from jack
      sample_rate; // from jack
  struct Local *d;
  JackInterf(const char *client_name,bool (*_play)(float *buf_left,float *buf_right),int _done,bool &is_playing);
  ~JackInterf();
};
 
extern int SAMPLE_RATE,
           IBsize;
extern bool no_jack_conn;
