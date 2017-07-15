/*
 * abcm2ps: a program to typeset tunes written in abc format using PostScript
 * Adapted from abcm2ps:
 *   Copyright (C) 1998-2003 Jean-François Moine
 * Adapted from abc2ps-1.2.5:
 *   Copyright (C) 1996,1997  Michael Methfessel
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
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h"

/* -- global variables -- */

struct ISTRUCT info, default_info;
char deco_glob[128], deco_tune[128];
struct SYMBOL *sym;		/* (points to the symbols of the current voice) */

char page_init[201];		/* initialization string after page break */
//char outf[STRL1];               /* output file name */
int tunenum;			/* number of current tune */
int pagenum = 1;		/* current page in output file */

int in_page;

/* switches modified by flags: */
int pagenumbers;		/* write page numbers ? */
int epsf;			/* for EPSF postscript output */
int choose_outname;		/* 1 names outfile w. title/fnam */

int  file_initialized;		/* for output file */
FILE *fout;			/* output file */

int s_argc;			/* command line arguments */
char **s_argv;

struct WHISTLE_S whistle_tb[MAXWHISTLE];
int nwhistle;

/* -- local variables -- */

static int include_xrefs = -1;	/* to include xref numbers in title */
static int one_per_page = -1;	/* new page for each tune */
static int splittune = -1;	/* tune may be splitted */
static int write_history = -1;	/* write history and notes */
static float alfa_c = -1.0;	/* max compression allowed */
static int bars_per_line = -1;	/* bars for auto linebreaking */
static int encoding = -1;	/* latin encoding number */
static int continue_lines = 1;	/* flag to continue all lines */
static int landscape = -1;	/* flag for landscape output */
static float lmargin = -1.0;	/* left margin */
static float indent = -1.0;	/* 1st line indentation */
static int music_only = -1;	/* no vocals if 1 */
static int flatbeams = -1;	/* flat beams when bagpipe */
static int graceslurs = -1;	/* slurs in grace notes */
static float scalefac = -1.0;	/* scale factor for symbol size */
static float staffsep = -1.0;	/* staff separation */
static const char *styd = DEFAULT_FDIR; /* format search directory */
static int def_fmt_done = 0;	/* default format read */
static float swidth = -1.0;	/* staff width */
static int measurenb = 0;	/* measure numbering (-1: none, 0: on the left, or every n bars) */
static int measurebox = 0;	/* display measure numbers in a box */
static int measurefirst = -1;	/* first measure number */
static int printtempo = -1;	/* print the tempo indications */
const char *in_fname;		/* current input file name */

/* -- local functions -- */
static void read_def_format(const char*);
#ifdef READ_FILE
static void output_file(void);
static char *read_file(void);

/* -- return the file extension -- */
static char *getext(const char *fid) {
        char *p;
                                                                                                         
        if ((p = (char*)strrchr(fid, DIRSEP)) == 0)
                p = (char*)fid;
        if ((p = strrchr(p, '.')) != 0)
                return p + 1;
        return 0;
}

/* -- read an input file -- */
static char *read_file(void) {
	int fsize;
	FILE *fin;
	char *file;

		if ((fin = fopen(in_fname, "rb")) == 0) {
			return 0;
		}
		if (fseek(fin, 0L, SEEK_END) < 0) {
			fclose(fin);
			return 0;
		}
		fsize = ftell(fin);
		rewind(fin);
		if ((file = (char*)malloc(fsize + 2)) == 0) {
			fclose(fin);
			return 0;
		}

		if (fread(file, 1, fsize, fin) != (unsigned int)fsize) {
			fclose(fin);
			free(file);
			return 0;
		}
		file[fsize] = '\0';
		fclose(fin);
	return file;
}
#endif
/* -- set_page_format --- */
static void set_page_format(const char *def_fmt)
{
        if (def_fmt) read_def_format(def_fmt);
        ops_into_fmt();
        make_font_list();
}

/* -- read the default format -- */
static void read_def_format(const char *def_fmt)
{
        if (!def_fmt || def_fmt_done)
                return;
        def_fmt_done = 1;
        if (read_fmt_file(def_fmt, styd) < 0)
		alert("Format file %s not found - using defaults",def_fmt);
}

void ops_into_fmt(void)
{
	struct FORMAT *fmt;

	fmt = &cfmt;
	if (landscape >= 0)
		fmt->landscape = landscape;
	if (scalefac > 0)
		fmt->scale = scalefac;
	if (lmargin >= 0)
		fmt->leftmargin = lmargin;
	if (indent >= 0)
		fmt->indent = indent;
	if (swidth >= 0) {
		fmt->rightmargin = fmt->pagewidth - swidth - fmt->leftmargin;
		if (fmt->rightmargin < 0)
			alert("Warning: staffwidth too big");
	}
	if (continue_lines >= 0)
		fmt->continueall = continue_lines;
	if (write_history >= 0)
		fmt->writehistory = write_history;
	if (bars_per_line >= 0)
		fmt->barsperstaff = bars_per_line;
	if (encoding >= 0)
		fmt->encoding = encoding;
	if (include_xrefs >= 0)
		fmt->withxrefs = include_xrefs;
	if (staffsep >= 0)
		fmt->staffsep = staffsep;
	if (one_per_page >= 0)
		fmt->oneperpage = one_per_page;
	if (splittune >= 0)
		fmt->splittune = splittune;
	if (music_only >= 0)
		fmt->musiconly = music_only;
	if (graceslurs >= 0)
		fmt->graceslurs = graceslurs;
	if (flatbeams >= 0)
		fmt->flatbeams = flatbeams;
	if (measurenb >= 0)
		fmt->measurenb = measurenb;
	if (measurebox >= 0)
		fmt->measurebox = measurebox;
	if (measurefirst >= 0)
		fmt->measurefirst = measurefirst;
	if (printtempo >= 0)
		fmt->printtempo = printtempo;
	if (alfa_c >= 0)
		fmt->maxshrink = alfa_c;
}

#ifdef READ_FILE
#include <stdarg.h>
void alert(const char *form,...) {
  va_list ap;
  va_start(ap,form);
  vprintf(form,ap);
  va_end(ap);
  putchar('\n');
  fflush(stdout);
}

/* -- do a tune selection -- */
static void do_select(struct abctune *t,int tune_nr)
{	int i;
	bool print_tune=0;
	while (t != 0) {
		struct abcsym *s;

		for (s = t->first_sym; s != 0; s = s->sym_next) {
			if (s->sym_type == ABC_T_INFO
			    && s->text[0] == 'X') {
				if (sscanf(s->text, "X:%d", &i) == 1 &&
				    (tune_nr < 0 || tune_nr==i))
					print_tune=1;
				break;
			}
		}
		do_tune(t,!print_tune);
		t->client_data = 1;	/* treated */
		t = t->next;
		print_tune=0;
	}
}

void usage() {
  puts("ABC to Postscript translator.");
  puts("Usage:");
  puts("  abcm2ps [options] <abc-file>");
  puts("Options:");
  puts("  -h                - usage info");
  puts("  -F <format-file>  - read format file");
  puts("  -s <nr>           - set scale to nr (default: 0.65)");
  puts("  +l                - set landscape mode on");
  puts("  -l                - set landscape mode off");
  puts("  -e <nr>           - select one tune");
  puts("Output file:\n  out.ps");
}

int main(int argc,const char **argv) {
  const char *def_fmt=0; // "/usr/share/amuc/default.fmt";
  int sel_nr=-1;
  for (int an=1;an<argc;an++) {
    if (!strcmp(argv[an],"-h")) { usage(); exit(1); }
    if (!strcmp(argv[an],"-F")) {
      if (++an==argc) { alert("File name after -F missing"); exit(1); }
      def_fmt=argv[an];
    }
    else if (!strcmp(argv[an],"+l"))
      landscape=1;
    else if (!strcmp(argv[an],"-l"))
      landscape=0;
    else if (!strcmp(argv[an],"-e")) {
      if (++an==argc) { alert("Number after -e missing"); exit(1); }
      sel_nr=atoi(argv[an]);
    }
    else if (!strcmp(argv[an],"-s")) {
      if (++an==argc) { alert("Number after -s missing"); exit(1); }
      scalefac=atof(argv[an]);
    }
    else if (argv[an][0]=='-') {
      alert("Unexpected option %s\n",argv[an]);
      usage();
      exit(1);
    }
    else
      in_fname = argv[an];
  }
  if (!in_fname) { alert("no input file (use -h for help)"); exit(1); }
  const char* outfname="out.ps";
#else
int gen_ps(char *prog,const char *outfname) {
  in_fname=outfname; // in_fname used as Title in ps file
  char *def_fmt=0;
#endif
  clear_buffer();
  if (abc_init(0)) {
    set_format();
    set_page_format(def_fmt);
    memset(&default_info, 0, sizeof(ISTRUCT));
    default_info.title[0] = "(notitle)";
    memcpy(&info, &default_info, sizeof(ISTRUCT));
    reset_deco();
    memcpy(&deco_tune, &deco_glob, sizeof deco_tune);
  }
  open_output_file(outfname);

#ifdef READ_FILE
  char *prog;
  if ((prog = read_file()) == 0) {  // fill char* file (here)
    alert("++++ Cannot read input file '%s'",in_fname);
    exit(1);
  }
  abctune *t = abc_parse(prog);
  do_select(t,sel_nr);
  close_output_file(true);
#else
  abctune *t = abc_parse(prog);
  do_tune(t,0);
  close_output_file(false);
#endif
  return 0;
}
