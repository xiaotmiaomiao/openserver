/*! \file
 *
 * \brief Terminal Routines
 *
 * \author Mark Spencer <markster@digium.com>
 */

//#include "ceictims.h"

#include "private.h"
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "term.h"
#include "lock.h"
#include "utils.h"
#include "threadstorage.h"

static int vt100compat;

static char prepdata[80] = "";
static char enddata[80] = "";
static char quitdata[80] = "";

static const char * const termpath[] = {
	"/usr/share/terminfo",
	"/usr/local/share/misc/terminfo",
	"/usr/lib/terminfo",
	NULL
	};

static int opposite(int color)
{
	int lookup[] = {
		/* BLACK */ COLOR_BLACK,
		/* RED */ COLOR_MAGENTA,
		/* GREEN */ COLOR_GREEN,
		/* BROWN */ COLOR_BROWN,
		/* BLUE */ COLOR_CYAN,
		/* MAGENTA */ COLOR_RED,
		/* CYAN */ COLOR_BLUE,
		/* WHITE */ COLOR_BLACK };
	return color ? lookup[color - 30] : 0;
}

/* Ripped off from Ross Ridge, but it's public domain code (libmytinfo) */
static short convshort(char *s)
{
	register int a, b;

	a = (int) s[0] & 0377;
	b = (int) s[1] & 0377;

	if (a == 0377 && b == 0377)
		return -1;
	if (a == 0376 && b == 0377)
		return -2;

	return a + b * 256;
}

int css_term_init(void)
{
	char *term = getenv("TERM");
	char termfile[256] = "";
	char buffer[512] = "";
	int termfd = -1, parseokay = 0, i;
            
	if (css_opt_no_color) {
		return 0;
	}

	if (!css_opt_console) {
		/* If any remote console is not compatible, we'll strip the color codes at that point */
		vt100compat = 1;
		goto end;
	}

	if (!term) {
		return 0;
	}

	for (i = 0;; i++) {
		if (termpath[i] == NULL) {
			break;
		}
		snprintf(termfile, sizeof(termfile), "%s/%c/%s", termpath[i], *term, term);
		termfd = open(termfile, O_RDONLY);
		if (termfd > -1) {
			break;
		}
	}
	if (termfd > -1) {
		int actsize = read(termfd, buffer, sizeof(buffer) - 1);
		short sz_names = convshort(buffer + 2);
		short sz_bools = convshort(buffer + 4);
		short n_nums   = convshort(buffer + 6);

		/* if ((sz_names + sz_bools) & 1)
			sz_bools++; */

		if (sz_names + sz_bools + n_nums < actsize) {
			/* Offset 13 is defined in /usr/include/term.h, though we do not
			 * include it here, as it conflicts with include/ceictims/term.h */
			short max_colors = convshort(buffer + 12 + sz_names + sz_bools + 13 * 2);
			if (max_colors > 0) {
				vt100compat = 1;
			}
			parseokay = 1;
		}
		close(termfd);
	}

	if (!parseokay) {
		/* These comparisons should not be substrings nor case-insensitive, as
		 * terminal types are very particular about how they treat suffixes and
		 * capitalization.  For example, terminal type 'linux-m' does NOT
		 * support color, while 'linux' does.  Not even all vt100* terminals
		 * support color, either (e.g. 'vt100+fnkeys'). */
		if (!strcmp(term, "linux")) {
			vt100compat = 1;
		} else if (!strcmp(term, "xterm")) {
			vt100compat = 1;
		} else if (!strcmp(term, "xterm-color")) {
			vt100compat = 1;
		} else if (!strcmp(term, "xterm-256color")) {
			vt100compat = 1;
		} else if (!strncmp(term, "Eterm", 5)) {
			/* Both entries which start with Eterm support color */
			vt100compat = 1;
		} else if (!strcmp(term, "vt100")) {
			vt100compat = 1;
		} else if (!strncmp(term, "crt", 3)) {
			/* Both crt terminals support color */
			vt100compat = 1;
		}
	}

end:
	if (vt100compat) {
		/* Make commands show up in nice colors */
		if (css_opt_light_background) {
			snprintf(prepdata, sizeof(prepdata), "%c[%dm", ESC, COLOR_BROWN);
			snprintf(enddata, sizeof(enddata), "%c[%dm", ESC, COLOR_BLACK);
			snprintf(quitdata, sizeof(quitdata), "%c[0m", ESC);
		} else if (css_opt_force_black_background) {
			snprintf(prepdata, sizeof(prepdata), "%c[%d;%d;%dm", ESC, ATTR_BRIGHT, COLOR_BROWN, COLOR_BLACK + 10);
			snprintf(enddata, sizeof(enddata), "%c[%d;%d;%dm", ESC, ATTR_RESET, COLOR_WHITE, COLOR_BLACK + 10);
			snprintf(quitdata, sizeof(quitdata), "%c[0m", ESC);
		} else {
			snprintf(prepdata, sizeof(prepdata), "%c[%d;%dm", ESC, ATTR_BRIGHT, COLOR_BROWN);
			snprintf(enddata, sizeof(enddata), "%c[%d;%dm", ESC, ATTR_RESET, COLOR_WHITE);
			snprintf(quitdata, sizeof(quitdata), "%c[0m", ESC);
		}
	}
	return 0;
}

char *term_color(char *outbuf, const char *inbuf, int fgcolor, int bgcolor, int maxout)
{
	int attr = 0;

	if (!vt100compat) {
		css_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if (!fgcolor) {
		css_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}

	if (fgcolor & 128) {
		attr = css_opt_light_background ? 0 : ATTR_BRIGHT;
		fgcolor &= ~128;
	}

	if (bgcolor) {
		bgcolor &= ~128;
	}

	if (css_opt_light_background) {
		fgcolor = opposite(fgcolor);
	}

	if (css_opt_force_black_background) {
		snprintf(outbuf, maxout, "%c[%d;%d;%dm%s%c[%d;%dm", ESC, attr, fgcolor, bgcolor + 10, inbuf, ESC, COLOR_WHITE, COLOR_BLACK + 10);
	} else {
		snprintf(outbuf, maxout, "%c[%d;%dm%s%c[0m", ESC, attr, fgcolor, inbuf, ESC);
	}
	return outbuf;
}

static void check_fgcolor(int *fgcolor, int *attr)
{
	if (*fgcolor & 128) {
		*attr = css_opt_light_background ? 0 : ATTR_BRIGHT;
		*fgcolor &= ~128;
	}
	
	if (css_opt_light_background) {
		*fgcolor = opposite(*fgcolor);
	}
}

static void check_bgcolor(int *bgcolor)
{
	if (*bgcolor) {
		*bgcolor &= ~128;
	}
}

static int check_colors_allowed(int fgcolor)
{
	return (!vt100compat || !fgcolor) ? 0 : 1;
}
#if 0
int css_term_color_code(struct css_str **str, int fgcolor, int bgcolor)
{
	int attr = 0;

	if (!check_colors_allowed(fgcolor)) {
		return -1;
	}

	check_fgcolor(&fgcolor, &attr);
	check_bgcolor(&bgcolor);
	
	if (css_opt_force_black_background) {
		css_str_append(str, 0, "%c[%d;%d;%dm", ESC, attr, fgcolor, COLOR_BLACK + 10);
	} else if (bgcolor) {
		css_str_append(str, 0, "%c[%d;%d;%dm", ESC, attr, fgcolor, bgcolor + 10);
	} else {
		css_str_append(str, 0, "%c[%d;%dm", ESC, attr, fgcolor);
	}

	return 0;
}
#endif
char *term_color_code(char *outbuf, int fgcolor, int bgcolor, int maxout)
{
	int attr = 0;

	if (!check_colors_allowed(fgcolor)) {
		*outbuf = '\0';
		return outbuf;
	}

	check_fgcolor(&fgcolor, &attr);
	check_bgcolor(&bgcolor);

	if (css_opt_force_black_background) {
		snprintf(outbuf, maxout, "%c[%d;%d;%dm", ESC, attr, fgcolor, COLOR_BLACK + 10);
	} else if (bgcolor) {
		snprintf(outbuf, maxout, "%c[%d;%d;%dm", ESC, attr, fgcolor, bgcolor + 10);
	} else {
		snprintf(outbuf, maxout, "%c[%d;%dm", ESC, attr, fgcolor);
	}

	return outbuf;
}

char *term_strip(char *outbuf, const char *inbuf, int maxout)
{
	char *outbuf_ptr = outbuf;
	const char *inbuf_ptr = inbuf;

	while (outbuf_ptr < outbuf + maxout) {
		switch (*inbuf_ptr) {
			case ESC:
				while (*inbuf_ptr && (*inbuf_ptr != 'm'))
					inbuf_ptr++;
				break;
			default:
				*outbuf_ptr = *inbuf_ptr;
				outbuf_ptr++;
		}
		if (! *inbuf_ptr)
			break;
		inbuf_ptr++;
	}
	return outbuf;
}

char *term_prompt(char *outbuf, const char *inbuf, int maxout)
{
	if (!vt100compat) {
		css_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if (css_opt_force_black_background) {
		snprintf(outbuf, maxout, "%c[%d;%dm%c%c[%d;%dm%s",
			ESC, COLOR_BLUE, COLOR_BLACK + 10,
			inbuf[0],
			ESC, COLOR_WHITE, COLOR_BLACK + 10,
			inbuf + 1);
	} else if (css_opt_light_background) {
		snprintf(outbuf, maxout, "%c[%d;0m%c%c[%d;0m%s",
			ESC, COLOR_BLUE,
			inbuf[0],
			ESC, COLOR_BLACK,
			inbuf + 1);
	} else {
		snprintf(outbuf, maxout, "%c[%d;%d;0m%c%c[%d;%d;0m%s",
			ESC, ATTR_BRIGHT, COLOR_BLUE,
			inbuf[0],
			ESC, 0, COLOR_WHITE,
			inbuf + 1);
	}
	return outbuf;
}

/* filter escape sequences */
void term_filter_escapes(char *line)
{
	int i;
	int len = strlen(line);

	for (i = 0; i < len; i++) {
		if (line[i] != ESC)
			continue;
		if ((i < (len - 2)) &&
		    (line[i + 1] == 0x5B)) {
			switch (line[i + 2]) {
		 	case 0x30:
			case 0x31:
			case 0x33:
				continue;
			}
		}
		/* replace ESC with a space */
		line[i] = ' ';
	}
}

char *term_prep(void)
{
	return prepdata;
}

char *term_end(void)
{
	return enddata;
}

char *term_quit(void)
{
	return quitdata;
}

