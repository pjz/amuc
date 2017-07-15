#include <alsa/asoundlib.h>
#include <jack/jack.h>
#include "snd-interface.h"
#include "colors.h"

void alert(const char *form,...);  // from x-widgets.h
void send_uev(int cmd,int param1=0,int param2=0);

int
  SAMPLE_RATE=44100,
  IBsize=1024;      // sound buffers, may be modified if output_port = eJack

bool no_jack_conn;

snd_pcm_t *pcm_handle;  // global, only one instance of SndInterf can be active
snd_pcm_hw_params_t *hwparams;
const char *pcm_dev_name="default";

typedef jack_default_audio_sample_t sample_t;

static uint
  nom_rate = 44100, // Sample rate
  exact_rate,   // Sample rate returned by snd_pcm_hw_params_set_rate_near
  periods = 8;  // Number of periods

SndInterf::SndInterf():
    periodsize(1024*8/periods),
    okay(false) {
  snd_pcm_hw_params_alloca(&hwparams);
  if (snd_pcm_open(&pcm_handle, pcm_dev_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    alert("Error opening PCM device %s (maybe it's busy)", pcm_dev_name); return;
  }

//Simple: yields xruns
//  if (snd_pcm_set_params(pcm_handle,
//                         SND_PCM_FORMAT_S16_LE,
//                         SND_PCM_ACCESS_RW_INTERLEAVED,
//                         2,
//                         44100,
//                         1,
//                         periodsize * periods / 4) < 0) {
//    alert("Can not configure this PCM device");
//    return;
//  }
//  okay=true; return;

  if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
    alert("Can not configure this PCM device."); return;
  }
  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    alert("Error setting access."); return;
  }
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    alert("Error setting format."); return;
  }
  exact_rate = nom_rate;
  if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
    alert("Error setting rate."); return;
  }
  if (nom_rate != exact_rate) {
    alert("The rate %d Hz is not supported, using %d Hz instead.", nom_rate, exact_rate);
  }
  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2) < 0) {
    alert("Error setting channels"); return;
  }
  if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0) < 0) {
    alert("Error setting periods"); return;
  }
  // Set buffer size (in frames). The resulting latency is given by:
  //   latency = periodsize * periods / (rate * bytes_per_frame)
  snd_pcm_uframes_t buffer_frames=periodsize * periods / 4;
  if (snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hwparams,&buffer_frames) < 0) {
    alert("Error setting buffersize"); return;
  }
  //printf("buffer_frames=%u\n",buffer_frames);
  IBsize=buffer_frames/4;
  periodsize=buffer_frames;
  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    alert("Error setting HW params"); return;
  }
  okay=true;
} 

SndInterf::~SndInterf() {
  if (okay && snd_pcm_close(pcm_handle) < 0)
    alert("Error closing PCM device %s", pcm_dev_name);
}

void SndInterf::snd_write(short *buffer) {
  if (snd_pcm_writei(pcm_handle, buffer, periodsize/4) < 0) {
    snd_pcm_prepare(pcm_handle);
    puts("buffer not full"); // no warning
  }
}

struct Local {
  jack_client_t *client;
  jack_port_t *output_port_left,
              *output_port_right;
  jack_status_t status;
};

int process (jack_nframes_t nframes, void *arg) {
  JackInterf *own=reinterpret_cast<JackInterf*>(arg);
//printf("is_play? %d\n",own->is_playing); fflush(stdout);
  sample_t *buffer_left=(sample_t*)jack_port_get_buffer(own->d->output_port_left,nframes),
           *buffer_right=(sample_t*)jack_port_get_buffer(own->d->output_port_right,nframes);
  if (own->is_playing) {
    if (!own->play(buffer_left,buffer_right))
      send_uev(own->done);
  }
  else
    for (uint i=0;i<nframes;++i)
      buffer_left[i]=buffer_right[i]=0;
  return 0;
}
void client_error(const char *desc) {
  alert("jack error: %s", desc);
}
void client_shutdown(void *arg) {
  alert("jack shutdown");
}

JackInterf::~JackInterf() {
  jack_client_close(d->client);
  delete d;
}
  
JackInterf::JackInterf(const char *_client_name,bool (*_play)(float *buf_left,float *buf_right),int _done,bool &is_p):
    client_name(_client_name),
    okay(false),
    is_playing(is_p),
    play(_play),
    done(_done),
    buf_size(0),
    sample_rate(0),
    d(new Local()) {
  if ((d->client = jack_client_open (client_name, JackNoStartServer, &d->status)) == 0) {
    alert("Jack server not running?");
    return;
  }
  int sr=jack_get_sample_rate(d->client);
  if (sr!=SAMPLE_RATE) {
    printf("Sample rate %d set to %d\n",SAMPLE_RATE,sr); sample_rate=sr;
  }
  int bs=jack_get_buffer_size(d->client);
  if (bs!=IBsize) {
    printf("Buffer size %d set to %d\n",IBsize,bs); buf_size=bs;
  }
  jack_set_process_callback(d->client, process,this);
  jack_set_error_function(client_error);
  jack_on_shutdown(d->client, client_shutdown, 0);

  d->output_port_left = jack_port_register(d->client,"out-left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput,0);
  d->output_port_right = jack_port_register(d->client,"out-right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput,0);

  if (jack_activate(d->client)) {
    alert("Cannot activate client");
    return;
  }
  if (no_jack_conn) {
    okay=true;
    return;
  }
  int res=0;
  const char *port1="playback_1", *port2="playback_2";
  //const char *port1="system:playback_1", *port2="system:playback_2";
  //const char *port1="alsa_pcm:playback_1", *port2="alsa_pcm:playback_2";
  const char **ports=jack_get_ports(d->client,0,JACK_DEFAULT_AUDIO_TYPE,0); //alsa*",0,0);
  if (ports && ports[0]) {
    for (int i=0;ports[i] && res<2;++i) {
      if (strstr(ports[i],port1)) { ++res; port1=ports[i]; }
      else if (strstr(ports[i],port2)) { ++res; port2=ports[i]; }
      printf("port[%d]=%s\n",i,ports[i]);
    }
  }
  else {
    alert("No ports: ports=%p ports[0]=%d",ports,ports?ports[0]:0);
    return;
  }
  if (res==2) {
    if (jack_connect(d->client,jack_port_name(d->output_port_left),port1) ||
        jack_connect(d->client,jack_port_name(d->output_port_right),port2))
      alert("Cannot connect output ports");
  }
  else {
    alert("No alsa ports");
    alert("Expected: %s, %s",port1,port2);
    // still okay!
  }
  free(ports);
  okay=true;
}
