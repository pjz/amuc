void *conn_mk_alsa(void*);     // in midi-keyb.cpp
void *conn_mk_jack(void*);     // in midi-keyb-jack.cpp
void stop_conn_mk();           // in amuc.cpp
void keyb_noteOn(int midi_nr); // in sound.cpp
void keyb_noteOff(int midi_nr);

void alert(const char *form,...);
void say(const char *form,...);

extern bool debug,
            mk_connected,
            midi_mes;
extern const char *midi_input_dev;
