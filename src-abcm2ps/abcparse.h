/*++
 * Declarations for abcparse.c.
 *
 *-*/

#define MAXVOICE 32	/* max number of voices */

#define MAXHD	8	/* max heads on one stem */
#define MAXDC	7	/* max decorations */

#define BASE_LEN 1536	/* basic note length (semibreve or whole note - same as MIDI) */

/* accidentals */
enum accidentals {
	A_NULL,		/* none */
	A_SH,		/* sharp */
	A_NT,		/* natural */
	A_FT,		/* flat */
	A_DS,		/* double sharp */
	A_DF		/* double flat */
};

/* bar types - 4 bits per symbol */
#define B_BAR 1		/* | */
#define B_OBRA 2	/* [ */
#define B_CBRA 3	/* ] */
#define B_COL 4		/* : */

/* note structure */
struct deco {			/* describes decorations */
	char n;			/* number of decorations */
	unsigned char t[MAXDC];	/* decoration type */
};

struct Note {		/* note or rest */
	signed char pits[MAXHD]; /* pitches for notes */
	short lens[MAXHD];	/* note lengths as multiple of BASE */
	char accs[MAXHD]; /* code for accidentals */
	char sl1[MAXHD];	/* which slur starts on this head */
	char sl2[MAXHD];	/* which slur ends on this head */
	char ti1[MAXHD];	/* flag to start tie here */
	char ti2[MAXHD];	/* flag to end tie here */
	short len;		/* note length (shortest in chords) */
	unsigned invis:1;	/* invisible rest */
	unsigned word_end:1;	/* 1 if word ends here */
	unsigned stemless:1;	/* note with no stem */
	unsigned lyric_start:1;	/* may start a lyric here */
	unsigned grace:1;	/* grace note */
	unsigned sappo:1;	/* short appoggiatura */
	char nhd;		/* number of notes in chord - 1 */
	char p_plet, q_plet, r_plet; /* data for n-plets */
	char slur_st; 		/* how many slurs start here */
	char slur_end;		/* how many slurs end here */
	signed char brhythm;	/* broken rhythm */
	struct deco dc;		/* decorations */
};

struct clef_s {		/* clef */
			char type;
#define TREBLE 0
#define ALTO 1
#define BASS 2
#define PERC 3
			char line;
			signed char octave;
			signed char transpose;
			char invis;
#ifndef CLEF_TRANSPOSE
                        char check_pitch;       /* check if old abc2ps transposition */
#endif
};
struct key_s {          /* K: info */
                        signed char sf;         /* sharp (> 0) flats (< 0) */
                        char bagpipe;           /* HP or Hp */
                        char minor;             /* major (0) / minor (1) */
                        char empty;             /* clef alone if 1 */
                        char nacc;              /* explicit accidentals */
                        char pits[8];
                        char accs[8];
};
struct meter_s {	/* M: info */
			short wmeasure;		/* duration of a measure */
			short nmeter;		/* number of meter elements */
#define MAX_MEASURE 6
			struct {
				char top[8];	/* top value */
				char bot[2];	/* bottom value */
			} meter[MAX_MEASURE];
 };
struct staff_s {	/* %%staves */
			char voice;
			char flags;
#define OPEN_BRACE 0x01
#define CLOSE_BRACE 0x02
#define OPEN_BRACKET 0x04
#define CLOSE_BRACKET 0x08
#define OPEN_PARENTH 0x10
#define CLOSE_PARENTH 0x20
#define STOP_BAR 0x40
			char *name;
};
enum {
  ABC_T_NULL,
  ABC_T_INFO,	/* (text[0] gives the info type) */
  ABC_T_PSCOM,
  ABC_T_CLEF,
  ABC_T_NOTE,
  ABC_T_REST,
  ABC_T_BAR,
  ABC_T_EOLN,
  ABC_T_INFO2,	/* (info without header - H:) */
  ABC_T_MREST,	/* multi-measure rest */
  ABC_T_MREP,	/* measure repeat */
  ABC_T_V_OVER	/* voice overlay */
};

enum {
  ABC_S_GLOBAL,	/* global */
  ABC_S_HEAD, 	/* in header (after X:) */
  ABC_S_TUNE,	/* in tune (after K:) */
  ABC_S_EMBED	/* embedded header (between [..]) */
};

enum {
  V_OVER_S,	/* single & */
  V_OVER_D,	/* && */
  V_OVER_SS,	/* (& */
  V_OVER_SD,	/* (&& */
  V_OVER_E	/* )& */
};

/* symbol definition */
struct abctune;
struct abcsym {
	struct abctune *tune;	/* tune */
	struct SYMBOL *sym_next; /* next symbol */
	struct SYMBOL *sym_prev; /* previous symbol */
	char sym_type;		/* symbol type */
	char state;		/* symbol state in file/tune */
	short linenum;		/* line number / ABC file */
	char *text;		/* main text (INFO, PSCOM),
				 * guitar chord (NOTE, REST, BAR) */
	char *comment;		/* comment part (when keep_comment) */
	union {			/* type dependent part */
		struct key_s key;
		struct {		/* L: info */
			int base_length;	/* basic note length */
		} length;
		struct meter_s meter;
		struct {		/* Q: info */
			char *str1;		/* string before */
			short length[4];	/* up to 4 note lengths */
			short value;		/* tempo value */
			char *str2;		/* string after */
		} tempo;
		struct {		/* V: info */
			char *name;		/* name */
			char *fname;		/* full name */
			char *nname;		/* nick name */
			char voice;	/* voice number */
			char merge;		/* merge with previous voice */
			signed char stem;	/* have all stems up or down */
		} voice;
		struct {		/* bar, mrest or mrep */
			struct deco dc;		/* decorations */
			int type;
			char repeat_bar;
			char len;		/* len if mrest or mrep */
		} bar;
		struct clef_s clef;
		struct Note note;	/* note, rest */
		struct {		/* user defined accent */
			char symbol;
			char value;
		} user;
		struct staff_s staves[MAXVOICE];
		struct {		/* voice overlay */
			char type;
			char voice;
		} v_over;
	} un;
};

/* tune definition */
struct abctune {
	struct abctune *next;	/* next tune */
	struct abctune *prev;	/* previous tune */
	struct SYMBOL *first_sym; /* first symbol */
	struct SYMBOL *last_sym; /* last symbol */
	int client_data;	/* client data */
};

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

extern char *deco_tb[];
extern int severity;
void abc_delete(struct SYMBOL *as);
void abc_free(struct abctune *first_tune);
void abc_insert(char *file_api,
		struct SYMBOL *s);
struct SYMBOL *abc_new(struct abctune *t,
		       char *p,
		       char *comment);
struct abctune *abc_parse(char *file_api);
char *get_str(char *d,
	      char *s,
	      int maxlen);
void note_sort(struct SYMBOL *s);
char *parse_deco(char *p,
			  struct deco *deco);
bool abc_init(int);
