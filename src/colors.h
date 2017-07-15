// used by all .cpp files
typedef unsigned int uint;

enum Color {
  eBlack,eRed,eBlue,eGreen,ePurple,eBrown,  // <-- used by instruments
  eGrey
};
                                                                                       
extern int SAMPLE_RATE;

const int
  colors_max=6,
  groupnr_max=3,   // max color group-nr
  keys_max=13,
  max100=100,   // size of char buffers
  max200=200;
const uint
  ampl_max=9;

extern const char *const color_name[colors_max];
int color_nr(const char *coln,bool *ok=0); 
