/*! \file
 *
 * \brief Standard Command Line Interface
 *
 * \author   <@.com> 
 */

#include <sys/signal.h>
#include <signal.h>
#include <ctype.h>
#include <regex.h>
#include <pwd.h>
#include <grp.h>
#include "cli.h"
#include "linkedlists.h"
#include "utils.h"
#include "lock.h"
#include "threadstorage.h"
#include "logger.h"
#include "config.h"
#include "compat.h"

int css_cli_perms_init(int reload);
/*!
 * \brief List of restrictions per user.
 */
struct cli_perm {
	unsigned int permit:1;				/*!< 1=Permit 0=Deny */
	char *command;				/*!< Command name (to apply restrictions) */
	CSS_LIST_ENTRY(cli_perm) list;
};

CSS_LIST_HEAD_NOLOCK(cli_perm_head, cli_perm);

/*! \brief list of users to apply restrictions. */
struct usergroup_cli_perm {
	int uid;				/*!< User ID (-1 disabled) */
	int gid;				/*!< Group ID (-1 disabled) */
	struct cli_perm_head *perms;		/*!< List of permissions. */
	CSS_LIST_ENTRY(usergroup_cli_perm) list;/*!< List mechanics */
};
/*! \brief CLI permissions config file. */
static const char perms_config[] = "cli_permissions.conf";
/*! \brief Default permissions value 1=Permit 0=Deny */
static int cli_default_perm = 1;

/*! \brief mutex used to prevent a user from running the 'cli reload permissions' command while
 * it is already running. */
CSS_MUTEX_DEFINE_STATIC(permsconfiglock);
/*! \brief  List of users and permissions. */
static CSS_RWLIST_HEAD_STATIC(cli_perms, usergroup_cli_perm);

/*!
 * \brief map a debug or verbose level to a module name
 */
struct module_level {
	unsigned int level;
	CSS_RWLIST_ENTRY(module_level) entry;
	char module[0];
};

CSS_RWLIST_HEAD(module_level_list, module_level);

/*! list of module names and their debug levels */
static struct module_level_list debug_modules;
/*! list of module names and their verbose levels */
static struct module_level_list verbose_modules;

CSS_THREADSTORAGE(css_cli_buf);

/*! \brief Initial buffer size for resulting strings in css_cli() */
#define CSS_CLI_INITLEN   256

void css_cli(int fd, const char *fmt, ...)
{
	int res;
	struct css_str *buf;
	va_list ap;

	if (!(buf = css_str_thread_get(&css_cli_buf, CSS_CLI_INITLEN)))
		return;

	va_start(ap, fmt);
	res = css_str_set_va(&buf, 0, fmt, ap);
	va_end(ap);

	if (res != CSS_DYNSTR_BUILD_FAILED) {
		css_carefulwrite(fd, css_str_buffer(buf), css_str_strlen(buf), 100);
	}
}

unsigned int css_debug_get_by_module(const char *module) 
{
	struct module_level *ml;
	unsigned int res = 0;

	CSS_RWLIST_RDLOCK(&debug_modules);
	CSS_LIST_TRAVERSE(&debug_modules, ml, entry) {
		if (!strcasecmp(ml->module, module)) {
			res = ml->level;
			break;
		}
	}
	CSS_RWLIST_UNLOCK(&debug_modules);

	return res;
}

unsigned int css_verbose_get_by_module(const char *module) 
{
	struct module_level *ml;
	unsigned int res = 0;

	CSS_RWLIST_RDLOCK(&verbose_modules);
	CSS_LIST_TRAVERSE(&verbose_modules, ml, entry) {
		if (!strcasecmp(ml->module, module)) {
			res = ml->level;
			break;
		}
	}
	CSS_RWLIST_UNLOCK(&verbose_modules);

	return res;
}

/*! \internal
 *  \brief Check if the user with 'uid' and 'gid' is allow to execute 'command',
 *	   if command starts with '_' then not check permissions, just permit
 *	   to run the 'command'.
 *	   if uid == -1 or gid == -1 do not check permissions.
 *	   if uid == -2 and gid == -2 is because rceictims client didn't send
 *	   the credentials, so the cli_default_perm will be applied.
 *  \param uid User ID.
 *  \param gid Group ID.
 *  \param command Command name to check permissions.
 *  \retval 1 if has permission
 *  \retval 0 if it is not allowed.
 */

static int cli_has_permissions(int uid, int gid, const char *command)
{
	struct usergroup_cli_perm *user_perm;
	struct cli_perm *perm;
	/* set to the default permissions general option. */
	int isallowg = cli_default_perm, isallowu = -1, ispattern;
	regex_t regexbuf;

	/* if uid == -1 or gid == -1 do not check permissions.
	   if uid == -2 and gid == -2 is because rceictims client didn't send
	   the credentials, so the cli_default_perm will be applied. */
	if ((uid == CLI_NO_PERMS && gid == CLI_NO_PERMS) || command[0] == '_') {
		return 1;
	}

	if (gid < 0 && uid < 0) {
		return cli_default_perm;
	}

	CSS_RWLIST_RDLOCK(&cli_perms);
	CSS_LIST_TRAVERSE(&cli_perms, user_perm, list) {
		if (user_perm->gid != gid && user_perm->uid != uid) {
			continue;
		}
		CSS_LIST_TRAVERSE(user_perm->perms, perm, list) {
			if (strcasecmp(perm->command, "all") && strncasecmp(perm->command, command, strlen(perm->command))) {
				/* if the perm->command is a pattern, check it against command. */
				ispattern = !regcomp(&regexbuf, perm->command, REG_EXTENDED | REG_NOSUB | REG_ICASE);
				if (ispattern && regexec(&regexbuf, command, 0, NULL, 0)) {
					regfree(&regexbuf);
					continue;
				}
				if (!ispattern) {
					continue;
				}
				regfree(&regexbuf);
			}
			if (user_perm->uid == uid) {
				/* this is a user definition. */
				isallowu = perm->permit;
			} else {
				/* otherwise is a group definition. */
				isallowg = perm->permit;
			}
		}
	}
	CSS_RWLIST_UNLOCK(&cli_perms);
	if (isallowu > -1) {
		/* user definition override group definition. */
		isallowg = isallowu;
	}

	return isallowg;
}

static CSS_RWLIST_HEAD_STATIC(helpers, css_cli_entry);
#if 0
static char *complete_fn(const char *word, int state)
{
	char *c, *d;
	char filename[PATH_MAX];

	if (word[0] == '/')
		css_copy_string(filename, word, sizeof(filename));
	else
		snprintf(filename, sizeof(filename), "%s/%s", css_config_CSS_MODULE_DIR, word);

	c = d = filename_completion_function(filename, state);
	
	if (c && word[0] != '/')
		c += (strlen(css_config_CSS_MODULE_DIR) + 1);
	if (c)
		c = css_strdup(c);

	free(d);
	
	return c;
}

static char *handle_load(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	/* "module load <mod>" */
	switch (cmd) {
	case CLI_INIT:
		e->command = "module load";
		e->usage =
			"Usage: module load <module name>\n"
			"       Loads the specified module into Ceictims.\n";
		return NULL;

	case CLI_GENERATE:
		if (a->pos != e->args)
			return NULL;
		return complete_fn(a->word, a->n);
	}
	if (a->argc != e->args + 1)
		return CLI_SHOWUSAGE;
	if (css_load_resource(a->argv[e->args])) {
		css_cli(a->fd, "Unable to load module %s\n", a->argv[e->args]);
		return CLI_FAILURE;
	}
	css_cli(a->fd, "Loaded %s\n", a->argv[e->args]);
	return CLI_SUCCESS;
}
#endif
#if 0
static char *handle_reload(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	int x;

	switch (cmd) {
	case CLI_INIT:
		e->command = "module reload";
		e->usage =
			"Usage: module reload [module ...]\n"
			"       Reloads configuration files for all listed modules which support\n"
			"       reloading, or for all supported modules if none are listed.\n";
		return NULL;

	case CLI_GENERATE:
		return css_module_helper(a->line, a->word, a->pos, a->n, a->pos, 1);
	}
	if (a->argc == e->args) {
		css_module_reload(NULL);
		return CLI_SUCCESS;
	}
	for (x = e->args; x < a->argc; x++) {
		int res = css_module_reload(a->argv[x]);
		/* XXX reload has multiple error returns, including -1 on error and 2 on success */
		switch (res) {
		case 0:
			css_cli(a->fd, "No such module '%s'\n", a->argv[x]);
			break;
		case 1:
			css_cli(a->fd, "Module '%s' does not support reload\n", a->argv[x]);
			break;
		}
	}
	return CLI_SUCCESS;
}
#endif
#if 0
static char *handle_core_reload(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "core reload";
		e->usage =
			"Usage: core reload\n"
			"       Execute a global reload.\n";
		return NULL;

	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != e->args) {
		return CLI_SHOWUSAGE;
	}

	css_module_reload(NULL);

	return CLI_SUCCESS;
}
#endif
/*! 
 * \brief Find the debug or verbose file setting 
 * \arg debug 1 for debug, 0 for verbose
 */
static struct module_level *find_module_level(const char *module, unsigned int debug)
{
	struct module_level *ml;
	struct module_level_list *mll = debug ? &debug_modules : &verbose_modules;

	CSS_LIST_TRAVERSE(mll, ml, entry) {
		if (!strcasecmp(ml->module, module))
			return ml;
	}

	return NULL;
}

static char *complete_number(const char *partial, unsigned int min, unsigned int max, int n)
{
	int i, count = 0;
	unsigned int prospective[2];
	unsigned int part = strtoul(partial, NULL, 10);
	char next[12];

	if (part < min || part > max) {
		return NULL;
	}

	for (i = 0; i < 21; i++) {
		if (i == 0) {
			prospective[0] = prospective[1] = part;
		} else if (part == 0 && !css_strlen_zero(partial)) {
			break;
		} else if (i < 11) {
			prospective[0] = prospective[1] = part * 10 + (i - 1);
		} else {
			prospective[0] = (part * 10 + (i - 11)) * 10;
			prospective[1] = prospective[0] + 9;
		}
		if (i < 11 && (prospective[0] < min || prospective[0] > max)) {
			continue;
		} else if (prospective[1] < min || prospective[0] > max) {
			continue;
		}

		if (++count > n) {
			if (i < 11) {
				snprintf(next, sizeof(next), "%u", prospective[0]);
			} else {
				snprintf(next, sizeof(next), "%u...", prospective[0] / 10);
			}
			return css_strdup(next);
		}
	}
	return NULL;
}

static char *handle_verbose(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	int oldval;
	int newlevel;
	unsigned int is_debug;
	int atlecss = 0;
	int fd = a->fd;
	int argc = a->argc;
	const char * const *argv = a->argv;
	const char *argv3 = a->argv ? S_OR(a->argv[3], "") : "";
	int *dst;
	char *what;
	struct module_level_list *mll;
	struct module_level *ml;
        
        int option_debug,option_verbose;

	switch (cmd) {
	case CLI_INIT:
		e->command = "core set {debug|verbose}";
		e->usage =
#if !defined(LOW_MEMORY)
			"Usage: core set {debug|verbose} [atlecss] <level> [module]\n"
#else
			"Usage: core set {debug|verbose} [atlecss] <level>\n"
#endif
			"       core set {debug|verbose} off\n"
#if !defined(LOW_MEMORY)
			"       Sets level of debug or verbose messages to be displayed or\n"
			"       sets a module name to display debug messages from.\n"
#else
			"       Sets level of debug or verbose messages to be displayed.\n"
#endif
			"	0 or off means no messages should be displayed.\n"
			"	Equivalent to -d[d[...]] or -v[v[v...]] on startup\n";
		return NULL;

	case CLI_GENERATE:
		if (a->pos == 3 || (a->pos == 4 && !strcasecmp(a->argv[3], "atlecss"))) {
			const char *pos = a->pos == 3 ? argv3 : S_OR(a->argv[4], "");
			int numbermatch = (css_strlen_zero(pos) || strchr("123456789", pos[0])) ? 0 : 21;
			if (a->n < 21 && numbermatch == 0) {
				return complete_number(pos, 0, 0x7fffffff, a->n);
			} else if (pos[0] == '0') {
				if (a->n == 0) {
					return css_strdup("0");
				} else {
					return NULL;
				}
			} else if (a->n == (21 - numbermatch)) {
				if (a->pos == 3 && !strncasecmp(argv3, "off", strlen(argv3))) {
					return css_strdup("off");
				} else if (a->pos == 3 && !strncasecmp(argv3, "atlecss", strlen(argv3))) {
					return css_strdup("atlecss");
				}
			} else if (a->n == (22 - numbermatch) && a->pos == 3 && css_strlen_zero(argv3)) {
				return css_strdup("atlecss");
			}
#if !defined(LOW_MEMORY)
		} else if (a->pos == 4 || (a->pos == 5 && !strcasecmp(argv3, "atlecss"))) {
//			return css_complete_source_filename(a->pos == 4 ? S_OR(a->argv[4], "") : S_OR(a->argv[5], ""), a->n);
#endif
		}
		return NULL;
	}
	/* all the above return, so we proceed with the handler.
	 * we are guaranteed to be called with argc >= e->args;
	 */

	if (argc <= e->args)
		return CLI_SHOWUSAGE;
	if (!strcasecmp(argv[e->args - 1], "debug")) {
		dst = &option_debug;
		oldval = option_debug;
		what = "Core debug";
		is_debug = 1;
	} else {
		dst = &option_verbose;
		oldval = option_verbose;
		what = "Verbosity";
		is_debug = 0;
	}
	if (argc == e->args + 1 && !strcasecmp(argv[e->args], "off")) {
		newlevel = 0;

		mll = is_debug ? &debug_modules : &verbose_modules;

		CSS_RWLIST_WRLOCK(mll);
		while ((ml = CSS_RWLIST_REMOVE_HEAD(mll, entry))) {
			css_free(ml);
		}
//		css_clear_flag(&css_options, is_debug ? CSS_OPT_FLAG_DEBUG_MODULE : CSS_OPT_FLAG_VERBOSE_MODULE);
		CSS_RWLIST_UNLOCK(mll);

		goto done;
	}
	if (!strcasecmp(argv[e->args], "atlecss"))
		atlecss = 1;
	if (argc != e->args + atlecss + 1 && argc != e->args + atlecss + 2)
		return CLI_SHOWUSAGE;
	if (sscanf(argv[e->args + atlecss], "%30d", &newlevel) != 1)
		return CLI_SHOWUSAGE;
	if (argc == e->args + atlecss + 2) {
		/* We have specified a module name. */
		char *mod = css_strdupa(argv[e->args + atlecss + 1]);

		if ((strlen(mod) > 3) && !strcasecmp(mod + strlen(mod) - 3, ".so")) {
			mod[strlen(mod) - 3] = '\0';
		}

		mll = is_debug ? &debug_modules : &verbose_modules;

		CSS_RWLIST_WRLOCK(mll);

		ml = find_module_level(mod, is_debug);
		if (!newlevel) {
			if (!ml) {
				/* Specified off for a nonexistent entry. */
				CSS_RWLIST_UNLOCK(mll);
				return CLI_SUCCESS;
			}
			CSS_RWLIST_REMOVE(mll, ml, entry);
			if (CSS_RWLIST_EMPTY(mll))
//				css_clear_flag(&css_options, is_debug ? CSS_OPT_FLAG_DEBUG_MODULE : CSS_OPT_FLAG_VERBOSE_MODULE);
			CSS_RWLIST_UNLOCK(mll);
			css_cli(fd, "%s was %d and has been set to 0 for '%s'\n", what, ml->level, mod);
			css_free(ml);
			return CLI_SUCCESS;
		}

		if (ml) {
			if ((atlecss && newlevel < ml->level) || ml->level == newlevel) {
				css_cli(fd, "%s is %d for '%s'\n", what, ml->level, mod);
				CSS_RWLIST_UNLOCK(mll);
				return CLI_SUCCESS;
			}
			oldval = ml->level;
			ml->level = newlevel;
		} else {
			ml = css_calloc(1, sizeof(*ml) + strlen(mod) + 1);
			if (!ml) {
				CSS_RWLIST_UNLOCK(mll);
				return CLI_FAILURE;
			}
			oldval = ml->level;
			ml->level = newlevel;
			strcpy(ml->module, mod);
			CSS_RWLIST_INSERT_TAIL(mll, ml, entry);
		}

//		css_set_flag(&css_options, is_debug ? CSS_OPT_FLAG_DEBUG_MODULE : CSS_OPT_FLAG_VERBOSE_MODULE);

		CSS_RWLIST_UNLOCK(mll);

		css_cli(fd, "%s was %d and has been set to %d for '%s'\n", what, oldval, ml->level, ml->module);

		return CLI_SUCCESS;
	} else if (!newlevel) {
		/* Specified level as 0 instead of off. */
		mll = is_debug ? &debug_modules : &verbose_modules;

		CSS_RWLIST_WRLOCK(mll);
		while ((ml = CSS_RWLIST_REMOVE_HEAD(mll, entry))) {
			css_free(ml);
		}
//		css_clear_flag(&css_options, is_debug ? CSS_OPT_FLAG_DEBUG_MODULE : CSS_OPT_FLAG_VERBOSE_MODULE);
		CSS_RWLIST_UNLOCK(mll);
	}

done:
	if (!atlecss || newlevel > *dst)
		*dst = newlevel;
	if (oldval > 0 && *dst == 0)
		css_cli(fd, "%s is now OFF\n", what);
	else if (*dst > 0) {
		if (oldval == *dst)
			css_cli(fd, "%s is at lecss %d\n", what, *dst);
		else
			css_cli(fd, "%s was %d and is now %d\n", what, oldval, *dst);
	}

	return CLI_SUCCESS;
}

static char *handle_logger_mute(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "logger mute";
		e->usage = 
			"Usage: logger mute\n"
			"       Disables logging output to the current console, making it possible to\n"
			"       gather information without being disturbed by scrolling lines.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 2 || a->argc > 3)
		return CLI_SHOWUSAGE;

	if (a->argc == 3 && !strcasecmp(a->argv[2], "silent"))
		css_console_toggle_mute(a->fd, 1);
	else
		css_console_toggle_mute(a->fd, 0);

	return CLI_SUCCESS;
}
#if 0
static char *handle_unload(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	/* "module unload mod_1 [mod_2 .. mod_N]" */
	int x;
	int force = CSS_FORCE_SOFT;
	const char *s;

	switch (cmd) {
	case CLI_INIT:
		e->command = "module unload";
		e->usage =
			"Usage: module unload [-f|-h] <module_1> [<module_2> ... ]\n"
			"       Unloads the specified module from Ceictims. The -f\n"
			"       option causes the module to be unloaded even if it is\n"
			"       in use (may cause a crash) and the -h module causes the\n"
			"       module to be unloaded even if the module says it cannot, \n"
			"       which almost always will cause a crash.\n";
		return NULL;

	case CLI_GENERATE:
		return css_module_helper(a->line, a->word, a->pos, a->n, a->pos, 0);
	}
	if (a->argc < e->args + 1)
		return CLI_SHOWUSAGE;
	x = e->args;	/* first argument */
	s = a->argv[x];
	if (s[0] == '-') {
		if (s[1] == 'f')
			force = CSS_FORCE_FIRM;
		else if (s[1] == 'h')
			force = CSS_FORCE_HARD;
		else
			return CLI_SHOWUSAGE;
		if (a->argc < e->args + 2)	/* need at lecss one module name */
			return CLI_SHOWUSAGE;
		x++;	/* skip this argument */
	}

	for (; x < a->argc; x++) {
		if (css_unload_resource(a->argv[x], force)) {
			css_cli(a->fd, "Unable to unload resource %s\n", a->argv[x]);
			return CLI_FAILURE;
		}
		css_cli(a->fd, "Unloaded %s\n", a->argv[x]);
	}

	return CLI_SUCCESS;
}
#endif
#define MODLIST_FORMAT  "%-30s %-40.40s %-10d\n"
#define MODLIST_FORMAT2 "%-30s %-40.40s %-10s\n"

CSS_MUTEX_DEFINE_STATIC(climodentrylock);
static int climodentryfd = -1;

static int modlist_modentry(const char *module, const char *description, int usecnt, const char *like)
{
	/* Comparing the like with the module */
	if (strcasestr(module, like) ) {
		css_cli(climodentryfd, MODLIST_FORMAT, module, description, usecnt);
		return 1;
	} 
	return 0;
}

static void print_uptimestr(int fd, struct timeval timeval, const char *prefix, int printsec)
{
	int x; /* the main part - years, weeks, etc. */
	struct css_str *out;

#define SECOND (1)
#define MINUTE (SECOND*60)
#define HOUR (MINUTE*60)
#define DAY (HOUR*24)
#define WEEK (DAY*7)
#define YEAR (DAY*365)
#define NEEDCOMMA(x) ((x)? ",": "")	/* define if we need a comma */
	if (timeval.tv_sec < 0)	/* invalid, nothing to show */
		return;

	if (printsec)  {	/* plain seconds output */
		css_cli(fd, "%s: %lu\n", prefix, (u_long)timeval.tv_sec);
		return;
	}
	out = css_str_alloca(256);
	if (timeval.tv_sec > YEAR) {
		x = (timeval.tv_sec / YEAR);
		timeval.tv_sec -= (x * YEAR);
		css_str_append(&out, 0, "%d year%s%s ", x, ESS(x),NEEDCOMMA(timeval.tv_sec));
	}
	if (timeval.tv_sec > WEEK) {
		x = (timeval.tv_sec / WEEK);
		timeval.tv_sec -= (x * WEEK);
		css_str_append(&out, 0, "%d week%s%s ", x, ESS(x),NEEDCOMMA(timeval.tv_sec));
	}
	if (timeval.tv_sec > DAY) {
		x = (timeval.tv_sec / DAY);
		timeval.tv_sec -= (x * DAY);
		css_str_append(&out, 0, "%d day%s%s ", x, ESS(x),NEEDCOMMA(timeval.tv_sec));
	}
	if (timeval.tv_sec > HOUR) {
		x = (timeval.tv_sec / HOUR);
		timeval.tv_sec -= (x * HOUR);
		css_str_append(&out, 0, "%d hour%s%s ", x, ESS(x),NEEDCOMMA(timeval.tv_sec));
	}
	if (timeval.tv_sec > MINUTE) {
		x = (timeval.tv_sec / MINUTE);
		timeval.tv_sec -= (x * MINUTE);
		css_str_append(&out, 0, "%d minute%s%s ", x, ESS(x),NEEDCOMMA(timeval.tv_sec));
	}
	x = timeval.tv_sec;
	if (x > 0 || css_str_strlen(out) == 0)	/* if there is nothing, print 0 seconds */
		css_str_append(&out, 0, "%d second%s ", x, ESS(x));
	css_cli(fd, "%s: %s\n", prefix, css_str_buffer(out));
}

static struct css_cli_entry *cli_next(struct css_cli_entry *e)
{
	if (e) {
		return CSS_LIST_NEXT(e, list);
	} else {
		return CSS_LIST_FIRST(&helpers);
	}
}
#if 0
static char * handle_showuptime(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct timeval curtime = css_tvnow();
	int printsec;

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show uptime [seconds]";
		e->usage =
			"Usage: core show uptime [seconds]\n"
			"       Shows Ceictims uptime information.\n"
			"       The seconds word returns the uptime in seconds only.\n";
		return NULL;

	case CLI_GENERATE:
		return NULL;
	}
	/* regular handler */
	if (a->argc == e->args && !strcasecmp(a->argv[e->args-1],"seconds"))
		printsec = 1;
	else if (a->argc == e->args-1)
		printsec = 0;
	else
		return CLI_SHOWUSAGE;
	if (css_startuptime.tv_sec)
		print_uptimestr(a->fd, css_tvsub(curtime, css_startuptime), "System uptime", printsec);
	if (css_lcssreloadtime.tv_sec)
		print_uptimestr(a->fd, css_tvsub(curtime, css_lcssreloadtime), "Lcss reload", printsec);
	return CLI_SUCCESS;
}

static char *handle_modlist(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	const char *like;

	switch (cmd) {
	case CLI_INIT:
		e->command = "module show [like]";
		e->usage =
			"Usage: module show [like keyword]\n"
			"       Shows Ceictims modules currently in use, and usage statistics.\n";
		return NULL;

	case CLI_GENERATE:
		if (a->pos == e->args)
			return css_module_helper(a->line, a->word, a->pos, a->n, a->pos, 0);
		else
			return NULL;
	}
	/* all the above return, so we proceed with the handler.
	 * we are guaranteed to have argc >= e->args
	 */
	if (a->argc == e->args - 1)
		like = "";
	else if (a->argc == e->args + 1 && !strcasecmp(a->argv[e->args-1], "like") )
		like = a->argv[e->args];
	else
		return CLI_SHOWUSAGE;
		
	css_mutex_lock(&climodentrylock);
	climodentryfd = a->fd; /* global, protected by climodentrylock */
	css_cli(a->fd, MODLIST_FORMAT2, "Module", "Description", "Use Count");
	css_cli(a->fd,"%d modules loaded\n", css_update_module_list(modlist_modentry, like));
	climodentryfd = -1;
	css_mutex_unlock(&climodentrylock);
	return CLI_SUCCESS;
}
#endif
#undef MODLIST_FORMAT
#undef MODLIST_FORMAT2
#if 0
static char *handle_showcalls(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct timeval curtime = css_tvnow();
	int showuptime, printsec;

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show calls [uptime]";
		e->usage =
			"Usage: core show calls [uptime] [seconds]\n"
			"       Lists number of currently active calls and total number of calls\n"
			"       processed through PBX since lcss restart. If 'uptime' is specified\n"
			"       the system uptime is also displayed. If 'seconds' is specified in\n"
			"       addition to 'uptime', the system uptime is displayed in seconds.\n";
		return NULL;

	case CLI_GENERATE:
		if (a->pos != e->args)
			return NULL;
		return a->n == 0  ? css_strdup("seconds") : NULL;
	}

	/* regular handler */
	if (a->argc >= e->args && !strcasecmp(a->argv[e->args-1],"uptime")) {
		showuptime = 1;

		if (a->argc == e->args+1 && !strcasecmp(a->argv[e->args],"seconds"))
			printsec = 1;
		else if (a->argc == e->args)
			printsec = 0;
		else
			return CLI_SHOWUSAGE;
	} else if (a->argc == e->args-1) {
		showuptime = 0;
		printsec = 0;
	} else
		return CLI_SHOWUSAGE;

	if (option_maxcalls) {
		css_cli(a->fd, "%d of %d max active call%s (%5.2f%% of capacity)\n",
		   css_active_calls(), option_maxcalls, ESS(css_active_calls()),
		   ((double)css_active_calls() / (double)option_maxcalls) * 100.0);
	} else {
		css_cli(a->fd, "%d active call%s\n", css_active_calls(), ESS(css_active_calls()));
	}
   
	css_cli(a->fd, "%d call%s processed\n", css_processed_calls(), ESS(css_processed_calls()));

	if (css_startuptime.tv_sec && showuptime) {
		print_uptimestr(a->fd, css_tvsub(curtime, css_startuptime), "System uptime", printsec);
	}

	return RESULT_SUCCESS;
}

static char *handle_chanlist(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
#define FORMAT_STRING  "%-20.20s %-20.20s %-7.7s %-30.30s\n"
#define FORMAT_STRING2 "%-20.20s %-20.20s %-7.7s %-30.30s\n"
#define CONCISE_FORMAT_STRING  "%s!%s!%s!%d!%s!%s!%s!%s!%s!%s!%d!%s!%s!%s\n"
#define VERBOSE_FORMAT_STRING  "%-20.20s %-20.20s %-16.16s %4d %-7.7s %-12.12s %-25.25s %-15.15s %8.8s %-11.11s %-11.11s %-20.20s\n"
#define VERBOSE_FORMAT_STRING2 "%-20.20s %-20.20s %-16.16s %-4.4s %-7.7s %-12.12s %-25.25s %-15.15s %8.8s %-11.11s %-11.11s %-20.20s\n"

	struct css_channel *c = NULL;
	int numchans = 0, concise = 0, verbose = 0, count = 0;
	struct css_channel_iterator *iter = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show channels [concise|verbose|count]";
		e->usage =
			"Usage: core show channels [concise|verbose|count]\n"
			"       Lists currently defined channels and some information about them. If\n"
			"       'concise' is specified, the format is abridged and in a more easily\n"
			"       machine parsable format. If 'verbose' is specified, the output includes\n"
			"       more and longer fields. If 'count' is specified only the channel and call\n"
			"       count is output.\n"
			"	The 'concise' option is deprecated and will be removed from future versions\n"
			"	of Ceictims.\n";
		return NULL;

	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc == e->args) {
		if (!strcasecmp(a->argv[e->args-1],"concise"))
			concise = 1;
		else if (!strcasecmp(a->argv[e->args-1],"verbose"))
			verbose = 1;
		else if (!strcasecmp(a->argv[e->args-1],"count"))
			count = 1;
		else
			return CLI_SHOWUSAGE;
	} else if (a->argc != e->args - 1)
		return CLI_SHOWUSAGE;

	if (!count) {
		if (!concise && !verbose)
			css_cli(a->fd, FORMAT_STRING2, "Channel", "Location", "State", "Application(Data)");
		else if (verbose)
			css_cli(a->fd, VERBOSE_FORMAT_STRING2, "Channel", "Context", "Extension", "Priority", "State", "Application", "Data", 
				"CallerID", "Duration", "Accountcode", "PeerAccount", "BridgedTo");
	}

	if (!count && !(iter = css_channel_iterator_all_new())) {
		return CLI_FAILURE;
	}

	for (; iter && (c = css_channel_iterator_next(iter)); css_channel_unref(c)) {
		struct css_channel *bc;
		char durbuf[10] = "-";

		css_channel_lock(c);

		bc = css_bridged_channel(c);

		if (!count) {
			if ((concise || verbose)  && c->cdr && !css_tvzero(c->cdr->start)) {
				int duration = (int)(css_tvdiff_ms(css_tvnow(), c->cdr->start) / 1000);
				if (verbose) {
					int durh = duration / 3600;
					int durm = (duration % 3600) / 60;
					int durs = duration % 60;
					snprintf(durbuf, sizeof(durbuf), "%02d:%02d:%02d", durh, durm, durs);
				} else {
					snprintf(durbuf, sizeof(durbuf), "%d", duration);
				}				
			}
			if (concise) {
				css_cli(a->fd, CONCISE_FORMAT_STRING, c->name, c->context, c->exten, c->priority, css_state2str(c->_state),
					c->appl ? c->appl : "(None)",
					S_OR(c->data, ""),	/* XXX different from verbose ? */
					S_COR(c->caller.id.number.valid, c->caller.id.number.str, ""),
					S_OR(c->accountcode, ""),
					S_OR(c->peeraccount, ""),
					c->amaflags, 
					durbuf,
					bc ? bc->name : "(None)",
					c->uniqueid);
			} else if (verbose) {
				css_cli(a->fd, VERBOSE_FORMAT_STRING, c->name, c->context, c->exten, c->priority, css_state2str(c->_state),
					c->appl ? c->appl : "(None)",
					c->data ? S_OR(c->data, "(Empty)" ): "(None)",
					S_COR(c->caller.id.number.valid, c->caller.id.number.str, ""),
					durbuf,
					S_OR(c->accountcode, ""),
					S_OR(c->peeraccount, ""),
					bc ? bc->name : "(None)");
			} else {
				char locbuf[40] = "(None)";
				char appdata[40] = "(None)";
				
				if (!css_strlen_zero(c->context) && !css_strlen_zero(c->exten)) 
					snprintf(locbuf, sizeof(locbuf), "%s@%s:%d", c->exten, c->context, c->priority);
				if (c->appl)
					snprintf(appdata, sizeof(appdata), "%s(%s)", c->appl, S_OR(c->data, ""));
				css_cli(a->fd, FORMAT_STRING, c->name, locbuf, css_state2str(c->_state), appdata);
			}
		}
		css_channel_unlock(c);
	}

	if (iter) {
		css_channel_iterator_destroy(iter);
	}

	if (!concise) {
		numchans = css_active_channels();
		css_cli(a->fd, "%d active channel%s\n", numchans, ESS(numchans));
		if (option_maxcalls)
			css_cli(a->fd, "%d of %d max active call%s (%5.2f%% of capacity)\n",
				css_active_calls(), option_maxcalls, ESS(css_active_calls()),
				((double)css_active_calls() / (double)option_maxcalls) * 100.0);
		else
			css_cli(a->fd, "%d active call%s\n", css_active_calls(), ESS(css_active_calls()));

		css_cli(a->fd, "%d call%s processed\n", css_processed_calls(), ESS(css_processed_calls()));
	}

	return CLI_SUCCESS;
	
#undef FORMAT_STRING
#undef FORMAT_STRING2
#undef CONCISE_FORMAT_STRING
#undef VERBOSE_FORMAT_STRING
#undef VERBOSE_FORMAT_STRING2
}
#endif
#if 0
static char *handle_softhangup(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct css_channel *c=NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "channel request hangup";
		e->usage =
			"Usage: channel request hangup <channel>|<all>\n"
			"       Request that a channel be hung up. The hangup takes effect\n"
			"       the next time the driver reads or writes from the channel.\n"
			"       If 'all' is specified instead of a channel name, all channels\n"
			"       will see the hangup request.\n";
		return NULL;
	case CLI_GENERATE:
		return css_complete_channels(a->line, a->word, a->pos, a->n, e->args);
	}

	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	if (!strcasecmp(a->argv[3], "all")) {
		struct css_channel_iterator *iter = NULL;
		if (!(iter = css_channel_iterator_all_new())) {
			return CLI_FAILURE;
		}
		for (; iter && (c = css_channel_iterator_next(iter)); css_channel_unref(c)) {
			css_channel_lock(c);
			css_cli(a->fd, "Requested Hangup on channel '%s'\n", c->name);
			css_softhangup(c, CSS_SOFTHANGUP_EXPLICIT);
			css_channel_unlock(c);
		}
		css_channel_iterator_destroy(iter);
	} else if ((c = css_channel_get_by_name(a->argv[3]))) {
		css_channel_lock(c);
		css_cli(a->fd, "Requested Hangup on channel '%s'\n", c->name);
		css_softhangup(c, CSS_SOFTHANGUP_EXPLICIT);
		css_channel_unlock(c);
		c = css_channel_unref(c);
	} else {
		css_cli(a->fd, "%s is not a known channel\n", a->argv[3]);
	}

	return CLI_SUCCESS;
}
#endif
/*! \brief handles CLI command 'cli show permissions' */
static char *handle_cli_show_permissions(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct usergroup_cli_perm *cp;
	struct cli_perm *perm;
	struct passwd *pw = NULL;
	struct group *gr = NULL;

	switch (cmd) {
	case CLI_INIT:
		e->command = "cli show permissions";
		e->usage =
			"Usage: cli show permissions\n"
			"       Shows CLI configured permissions.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	CSS_RWLIST_RDLOCK(&cli_perms);
	CSS_LIST_TRAVERSE(&cli_perms, cp, list) {
		if (cp->uid >= 0) {
			pw = getpwuid(cp->uid);
			if (pw) {
				css_cli(a->fd, "user: %s [uid=%d]\n", pw->pw_name, cp->uid);
			}
		} else {
			gr = getgrgid(cp->gid);
			if (gr) {
				css_cli(a->fd, "group: %s [gid=%d]\n", gr->gr_name, cp->gid);
			}
		}
		css_cli(a->fd, "Permissions:\n");
		if (cp->perms) {
			CSS_LIST_TRAVERSE(cp->perms, perm, list) {
				css_cli(a->fd, "\t%s -> %s\n", perm->permit ? "permit" : "deny", perm->command);
			}
		}
		css_cli(a->fd, "\n");
	}
	CSS_RWLIST_UNLOCK(&cli_perms);

	return CLI_SUCCESS;
}

/*! \brief handles CLI command 'cli reload permissions' */
static char *handle_cli_reload_permissions(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "cli reload permissions";
		e->usage =
			"Usage: cli reload permissions\n"
			"       Reload the 'cli_permissions.conf' file.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	css_cli_perms_init(1);

	return CLI_SUCCESS;
}

/*! \brief handles CLI command 'cli check permissions' */
static char *handle_cli_check_permissions(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct passwd *pw = NULL;
	struct group *gr;
	int gid = -1, uid = -1;
	char command[CSS_MAX_ARGS] = "";
	struct css_cli_entry *ce = NULL;
	int found = 0;
	char *group, *tmp;

	switch (cmd) {
	case CLI_INIT:
		e->command = "cli check permissions";
		e->usage =
			"Usage: cli check permissions {<username>|@<groupname>|<username>@<groupname>} [<command>]\n"
			"       Check permissions config for a user@group or list the allowed commands for the specified user.\n"
			"       The username or the groupname may be omitted.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos >= 4) {
			return css_cli_generator(a->line + strlen("cli check permissions") + strlen(a->argv[3]) + 1, a->word, a->n);
		}
		return NULL;
	}

	if (a->argc < 4) {
		return CLI_SHOWUSAGE;
	}

	tmp = css_strdupa(a->argv[3]);
	group = strchr(tmp, '@');
	if (group) {
		gr = getgrnam(&group[1]);
		if (!gr) {
			css_cli(a->fd, "Unknown group '%s'\n", &group[1]);
			return CLI_FAILURE;
		}
		group[0] = '\0';
		gid = gr->gr_gid;
	}

	if (!group && css_strlen_zero(tmp)) {
		css_cli(a->fd, "You didn't supply a username\n");
	} else if (!css_strlen_zero(tmp) && !(pw = getpwnam(tmp))) {
		css_cli(a->fd, "Unknown user '%s'\n", tmp);
		return CLI_FAILURE;
	} else if (pw) {
		uid = pw->pw_uid;
	}

	if (a->argc == 4) {
		while ((ce = cli_next(ce))) {
			/* Hide commands that start with '_' */
			if (ce->_full_cmd[0] == '_') {
				continue;
			}
			if (cli_has_permissions(uid, gid, ce->_full_cmd)) {
				css_cli(a->fd, "%30.30s %s\n", ce->_full_cmd, S_OR(ce->summary, "<no description available>"));
				found++;
			}
		}
		if (!found) {
			css_cli(a->fd, "You are not allowed to run any command on Ceictims\n");
		}
	} else {
		css_join(command, sizeof(command), a->argv + 4);
		css_cli(a->fd, "%s '%s%s%s' is %s to run command: '%s'\n", uid >= 0 ? "User" : "Group", tmp,
			group && uid >= 0 ? "@" : "",
			group ? &group[1] : "",
			cli_has_permissions(uid, gid, command) ? "allowed" : "not allowed", command);
	}

	return CLI_SUCCESS;
}

static char *__css_cli_generator(const char *text, const char *word, int state, int lock);

static char *handle_commandmatchesarray(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	char *buf, *obuf;
	int buflen = 2048;
	int len = 0;
	char **matches;
	int x, matchlen;
	
	switch (cmd) {
	case CLI_INIT:
		e->command = "_command matchesarray";
		e->usage = 
			"Usage: _command matchesarray \"<line>\" text \n"
			"       This function is used internally to help with command completion and should.\n"
			"       never be called by the user directly.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;
	if (!(buf = css_malloc(buflen)))
		return CLI_FAILURE;
	buf[len] = '\0';
	matches = css_cli_completion_matches(a->argv[2], a->argv[3]);
	if (matches) {
		for (x=0; matches[x]; x++) {
			matchlen = strlen(matches[x]) + 1;
			if (len + matchlen >= buflen) {
				buflen += matchlen * 3;
				obuf = buf;
				if (!(buf = css_realloc(obuf, buflen))) 
					/* Memory allocation failure...  Just free old buffer and be done */
					css_free(obuf);
			}
			if (buf)
				len += sprintf( buf + len, "%s ", matches[x]);
			css_free(matches[x]);
			matches[x] = NULL;
		}
		css_free(matches);
	}

	if (buf) {
		css_cli(a->fd, "%s%s",buf, CSS_CLI_COMPLETE_EOF);
		css_free(buf);
	} else
		css_cli(a->fd, "NULL\n");

	return CLI_SUCCESS;
}



static char *handle_commandnummatches(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	int matches = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "_command nummatches";
		e->usage = 
			"Usage: _command nummatches \"<line>\" text \n"
			"       This function is used internally to help with command completion and should.\n"
			"       never be called by the user directly.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4)
		return CLI_SHOWUSAGE;

	matches = css_cli_generatornummatches(a->argv[2], a->argv[3]);

	css_cli(a->fd, "%d", matches);

	return CLI_SUCCESS;
}

static char *handle_commandcomplete(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	char *buf;
	switch (cmd) {
	case CLI_INIT:
		e->command = "_command complete";
		e->usage = 
			"Usage: _command complete \"<line>\" text state\n"
			"       This function is used internally to help with command completion and should.\n"
			"       never be called by the user directly.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 5)
		return CLI_SHOWUSAGE;
	buf = __css_cli_generator(a->argv[2], a->argv[3], atoi(a->argv[4]), 0);
	if (buf) {
		css_cli(a->fd, "%s", buf);
		css_free(buf);
	} else
		css_cli(a->fd, "NULL\n");
	return CLI_SUCCESS;
}

struct channel_set_debug_args {
	int fd;
	int is_off;
};
#if 0
static int channel_set_debug(void *obj, void *arg, void *data, int flags)
{
	struct css_channel *chan = obj;
	struct channel_set_debug_args *args = data;

	css_channel_lock(chan);

	if (!(chan->fin & DEBUGCHAN_FLAG) || !(chan->fout & DEBUGCHAN_FLAG)) {
		if (args->is_off) {
			chan->fin &= ~DEBUGCHAN_FLAG;
			chan->fout &= ~DEBUGCHAN_FLAG;
		} else {
			chan->fin |= DEBUGCHAN_FLAG;
			chan->fout |= DEBUGCHAN_FLAG;
		}
		css_cli(args->fd, "Debugging %s on channel %s\n", args->is_off ? "disabled" : "enabled",
				chan->name);
	}

	css_channel_unlock(chan);

	return 0;
}

static char *handle_core_set_debug_channel(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct css_channel *c = NULL;
	struct channel_set_debug_args args = {
		.fd = a->fd,
	};

	switch (cmd) {
	case CLI_INIT:
		e->command = "core set debug channel";
		e->usage =
			"Usage: core set debug channel <all|channel> [off]\n"
			"       Enables/disables debugging on all or on a specific channel.\n";
		return NULL;
	case CLI_GENERATE:
		/* XXX remember to handle the optional "off" */
		if (a->pos != e->args)
			return NULL;
		return a->n == 0 ? css_strdup("all") : css_complete_channels(a->line, a->word, a->pos, a->n - 1, e->args);
	}

	if (cmd == (CLI_HANDLER + 1000)) {
		/* called from handle_nodebugchan_deprecated */
		args.is_off = 1;
	} else if (a->argc == e->args + 2) {
		/* 'core set debug channel {all|chan_id}' */
		if (!strcasecmp(a->argv[e->args + 1], "off"))
			args.is_off = 1;
		else
			return CLI_SHOWUSAGE;
	} else if (a->argc != e->args + 1) {
		return CLI_SHOWUSAGE;
	}

	if (!strcasecmp("all", a->argv[e->args])) {
		if (args.is_off) {
			global_fin &= ~DEBUGCHAN_FLAG;
			global_fout &= ~DEBUGCHAN_FLAG;
		} else {
			global_fin |= DEBUGCHAN_FLAG;
			global_fout |= DEBUGCHAN_FLAG;
		}
		css_channel_callback(channel_set_debug, NULL, &args, OBJ_NODATA | OBJ_MULTIPLE);
	} else {
		if ((c = css_channel_get_by_name(a->argv[e->args]))) {
			channel_set_debug(c, NULL, &args, 0);
			css_channel_unref(c);
		} else {
			css_cli(a->fd, "No such channel %s\n", a->argv[e->args]);
		}
	}

	css_cli(a->fd, "Debugging on new channels is %s\n", args.is_off ? "disabled" : "enabled");

	return CLI_SUCCESS;
}
#endif
#if 0
static char *handle_nodebugchan_deprecated(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	char *res;

	switch (cmd) {
	case CLI_INIT:
		e->command = "no debug channel";
		return NULL;
	case CLI_HANDLER:
		/* exit out of switch statement */
		break;
	default:
		return NULL;
	}

	if (a->argc != e->args + 1)
		return CLI_SHOWUSAGE;

	/* add a 'magic' value to the CLI_HANDLER command so that
	 * handle_core_set_debug_channel() will act as if 'off'
	 * had been specified as part of the command
	 */
	res = handle_core_set_debug_channel(e, CLI_HANDLER + 1000, a);

	return res;
}
		
static char *handle_showchan(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct css_channel *c=NULL;
	struct timeval now;
	struct css_str *out = css_str_thread_get(&css_str_thread_global_buf, 16);
	char cdrtime[256];
	char nf[256], wf[256], rf[256];
	struct css_str *write_transpath = css_str_alloca(256);
	struct css_str *read_transpath = css_str_alloca(256);
	long elapsed_seconds=0;
	int hour=0, min=0, sec=0;
#ifdef CHANNEL_TRACE
	int trace_enabled;
#endif

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show channel";
		e->usage = 
			"Usage: core show channel <channel>\n"
			"       Shows lots of information about the specified channel.\n";
		return NULL;
	case CLI_GENERATE:
		return css_complete_channels(a->line, a->word, a->pos, a->n, 3);
	}
	
	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	now = css_tvnow();

	if (!(c = css_channel_get_by_name(a->argv[3]))) {
		css_cli(a->fd, "%s is not a known channel\n", a->argv[3]);
		return CLI_SUCCESS;
	}

	css_channel_lock(c);

	if (c->cdr) {
		elapsed_seconds = now.tv_sec - c->cdr->start.tv_sec;
		hour = elapsed_seconds / 3600;
		min = (elapsed_seconds % 3600) / 60;
		sec = elapsed_seconds % 60;
		snprintf(cdrtime, sizeof(cdrtime), "%dh%dm%ds", hour, min, sec);
	} else {
		strcpy(cdrtime, "N/A");
	}

	css_cli(a->fd, 
		" -- General --\n"
		"           Name: %s\n"
		"           Type: %s\n"
		"       UniqueID: %s\n"
		"       LinkedID: %s\n"
		"      Caller ID: %s\n"
		" Caller ID Name: %s\n"
		"Connected Line ID: %s\n"
		"Connected Line ID Name: %s\n"
		"    DNID Digits: %s\n"
		"       Language: %s\n"
		"          State: %s (%d)\n"
		"          Rings: %d\n"
		"  NativeFormats: %s\n"
		"    WriteFormat: %s\n"
		"     ReadFormat: %s\n"
		" WriteTranscode: %s %s\n"
		"  ReadTranscode: %s %s\n"
		"1st File Descriptor: %d\n"
		"      Frames in: %d%s\n"
		"     Frames out: %d%s\n"
		" Time to Hangup: %ld\n"
		"   Elapsed Time: %s\n"
		"  Direct Bridge: %s\n"
		"Indirect Bridge: %s\n"
		" --   PBX   --\n"
		"        Context: %s\n"
		"      Extension: %s\n"
		"       Priority: %d\n"
		"     Call Group: %llu\n"
		"   Pickup Group: %llu\n"
		"    Application: %s\n"
		"           Data: %s\n"
		"    Blocking in: %s\n",
		c->name, c->tech->type, c->uniqueid, c->linkedid,
		S_COR(c->caller.id.number.valid, c->caller.id.number.str, "(N/A)"),
		S_COR(c->caller.id.name.valid, c->caller.id.name.str, "(N/A)"),
		S_COR(c->connected.id.number.valid, c->connected.id.number.str, "(N/A)"),
		S_COR(c->connected.id.name.valid, c->connected.id.name.str, "(N/A)"),
		S_OR(c->dialed.number.str, "(N/A)"),
		c->language,	
		css_state2str(c->_state), c->_state, c->rings, 
		css_getformatname_multiple(nf, sizeof(nf), c->nativeformats), 
		css_getformatname_multiple(wf, sizeof(wf), c->writeformat), 
		css_getformatname_multiple(rf, sizeof(rf), c->readformat),
		c->writetrans ? "Yes" : "No",
		css_translate_path_to_str(c->writetrans, &write_transpath),
		c->readtrans ? "Yes" : "No",
		css_translate_path_to_str(c->readtrans, &read_transpath),
		c->fds[0],
		c->fin & ~DEBUGCHAN_FLAG, (c->fin & DEBUGCHAN_FLAG) ? " (DEBUGGED)" : "",
		c->fout & ~DEBUGCHAN_FLAG, (c->fout & DEBUGCHAN_FLAG) ? " (DEBUGGED)" : "",
		(long)c->whentohangup.tv_sec,
		cdrtime, c->_bridge ? c->_bridge->name : "<none>", css_bridged_channel(c) ? css_bridged_channel(c)->name : "<none>", 
		c->context, c->exten, c->priority, c->callgroup, c->pickupgroup, ( c->appl ? c->appl : "(N/A)" ),
		( c-> data ? S_OR(c->data, "(Empty)") : "(None)"),
		(css_test_flag(c, CSS_FLAG_BLOCKING) ? c->blockproc : "(Not Blocking)"));
	
#ifdef IMS_PTT
		if (!css_strlen_zero(c->pttgroup)) {
			css_cli(a->fd, " --   PTT   --\n");
			css_cli(a->fd, "       PttGroup: %s\n", c->pttgroup);
		}
#endif

	if (pbx_builtin_serialize_variables(c, &out)) {
		css_cli(a->fd,"      Variables:\n%s\n", css_str_buffer(out));
	}

	if (c->cdr && css_cdr_serialize_variables(c->cdr, &out, '=', '\n', 1)) {
		css_cli(a->fd,"  CDR Variables:\n%s\n", css_str_buffer(out));
	}

#ifdef CHANNEL_TRACE
	trace_enabled = css_channel_trace_is_enabled(c);
	css_cli(a->fd, "  Context Trace: %s\n", trace_enabled ? "Enabled" : "Disabled");
	if (trace_enabled && css_channel_trace_serialize(c, &out))
		css_cli(a->fd, "          Trace:\n%s\n", css_str_buffer(out));
#endif

	css_channel_unlock(c);
	c = css_channel_unref(c);

	return CLI_SUCCESS;
}
#endif
/*
 * helper function to generate CLI matches from a fixed set of values.
 * A NULL word is acceptable.
 */
char *css_cli_complete(const char *word, const char * const choices[], int state)
{
	int i, which = 0, len;
	len = css_strlen_zero(word) ? 0 : strlen(word);

	for (i = 0; choices[i]; i++) {
		if ((!len || !strncasecmp(word, choices[i], len)) && ++which > state)
			return css_strdup(choices[i]);
	}
	return NULL;
}
#if 0
char *css_complete_channels(const char *line, const char *word, int pos, int state, int rpos)
{
	struct css_channel *c = NULL;
	int which = 0;
	char notfound = '\0';
	char *ret = &notfound; /* so NULL can break the loop */
	struct css_channel_iterator *iter;

	if (pos != rpos) {
		return NULL;
	}

	if (css_strlen_zero(word)) {
		iter = css_channel_iterator_all_new();
	} else {
		iter = css_channel_iterator_by_name_new(word, strlen(word));
	}

	if (!iter) {
		return NULL;
	}

	while (ret == &notfound && (c = css_channel_iterator_next(iter))) {
		if (++which > state) {
			css_channel_lock(c);
			ret = css_strdup(c->name);
			css_channel_unlock(c);
		}
		css_channel_unref(c);
	}

	css_channel_iterator_destroy(iter);

	return ret == &notfound ? NULL : ret;
}

static char *group_show_channels(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
#define FORMAT_STRING  "%-25s  %-20s  %-20s\n"

	struct css_group_info *gi = NULL;
	int numchans = 0;
	regex_t regexbuf;
	int havepattern = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "group show channels";
		e->usage = 
			"Usage: group show channels [pattern]\n"
			"       Lists all currently active channels with channel group(s) specified.\n"
			"       Optional regular expression pattern is matched to group names for each\n"
			"       channel.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3 || a->argc > 4)
		return CLI_SHOWUSAGE;
	
	if (a->argc == 4) {
		if (regcomp(&regexbuf, a->argv[3], REG_EXTENDED | REG_NOSUB))
			return CLI_SHOWUSAGE;
		havepattern = 1;
	}

	css_cli(a->fd, FORMAT_STRING, "Channel", "Group", "Category");

	css_app_group_list_rdlock();
	
	gi = css_app_group_list_head();
	while (gi) {
		if (!havepattern || !regexec(&regexbuf, gi->group, 0, NULL, 0)) {
			css_cli(a->fd, FORMAT_STRING, gi->chan->name, gi->group, (css_strlen_zero(gi->category) ? "(default)" : gi->category));
			numchans++;
		}
		gi = CSS_LIST_NEXT(gi, group_list);
	}
	
	css_app_group_list_unlock();
	
	if (havepattern)
		regfree(&regexbuf);

	css_cli(a->fd, "%d active channel%s\n", numchans, ESS(numchans));
	return CLI_SUCCESS;
#undef FORMAT_STRING
}
#endif

static char *handle_cli_wait_fullybooted(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "core waitfullybooted";
		e->usage =
			"Usage: core waitfullybooted\n"
			"	Wait until Cssplayer has fully booted.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	while (!css_test_flag(&css_options, CSS_OPT_FLAG_FULLY_BOOTED)) {
		usleep(100);
	}

	css_cli(a->fd, "CSSplayer has fully booted.\n");

	return CLI_SUCCESS;
}

static char *handle_help(struct css_cli_entry *e, int cmd, struct css_cli_args *a);

static struct css_cli_entry cli_cli[] = {
	/* Deprecated, but preferred command is now consolidated (and already has a deprecated command for it). */
	CSS_CLI_DEFINE(handle_commandcomplete, "Command complete"),
	CSS_CLI_DEFINE(handle_commandnummatches, "Returns number of command matches"),
	CSS_CLI_DEFINE(handle_commandmatchesarray, "Returns command matches array"),

//	CSS_CLI_DEFINE(handle_nodebugchan_deprecated, "Disable debugging on channel(s)"),

//	CSS_CLI_DEFINE(handle_chanlist, "Display information on channels"),

//	CSS_CLI_DEFINE(handle_showcalls, "Display information on calls"),

//	CSS_CLI_DEFINE(handle_showchan, "Display information on a specific channel"),

//	CSS_CLI_DEFINE(handle_core_set_debug_channel, "Enable/disable debugging on a channel"),

	CSS_CLI_DEFINE(handle_verbose, "Set level of debug/verbose chattiness"),

//	CSS_CLI_DEFINE(group_show_channels, "Display active channels with group(s)"),

	CSS_CLI_DEFINE(handle_help, "Display help list, or specific help on a command"),

	CSS_CLI_DEFINE(handle_logger_mute, "Toggle logging output to a console"),

//	CSS_CLI_DEFINE(handle_modlist, "List modules and info"),

//	CSS_CLI_DEFINE(handle_load, "Load a module by name"),

//	CSS_CLI_DEFINE(handle_reload, "Reload configuration for a module"),

//	CSS_CLI_DEFINE(handle_core_reload, "Global reload"),

//	CSS_CLI_DEFINE(handle_unload, "Unload a module by name"),

//	CSS_CLI_DEFINE(handle_showuptime, "Show uptime information"),

//	CSS_CLI_DEFINE(handle_softhangup, "Request a hangup on a given channel"),

	CSS_CLI_DEFINE(handle_cli_reload_permissions, "Reload CLI permissions config"),

	CSS_CLI_DEFINE(handle_cli_show_permissions, "Show CLI permissions"),

	CSS_CLI_DEFINE(handle_cli_check_permissions, "Try a permissions config for a user"),

	CSS_CLI_DEFINE(handle_cli_wait_fullybooted, "Wait for Cssplayers to be fully booted"),
};

/*!
 * Some regexp characters in cli arguments are reserved and used as separators.
 */
static const char cli_rsvd[] = "[]{}|*%";

/*!
 * initialize the _full_cmd string and related parameters,
 * return 0 on success, -1 on error.
 */
static int set_full_cmd(struct css_cli_entry *e)
{
	int i;
	char buf[80];

	css_join(buf, sizeof(buf), e->cmda);
	e->_full_cmd = css_strdup(buf);
	if (!e->_full_cmd) {
		css_log(LOG_WARNING, "-- cannot allocate <%s>\n", buf);
		return -1;
	}
	e->cmdlen = strcspn(e->_full_cmd, cli_rsvd);
	for (i = 0; e->cmda[i]; i++)
		;
	e->args = i;
	return 0;
}

/*! \brief cleanup (free) cli_perms linkedlist. */
static void destroy_user_perms(void)
{
	struct cli_perm *perm;
	struct usergroup_cli_perm *user_perm;

	CSS_RWLIST_WRLOCK(&cli_perms);
	while ((user_perm = CSS_LIST_REMOVE_HEAD(&cli_perms, list))) {
		while ((perm = CSS_LIST_REMOVE_HEAD(user_perm->perms, list))) {
			css_free(perm->command);
			css_free(perm);
		}
		css_free(user_perm);
	}
	CSS_RWLIST_UNLOCK(&cli_perms);
}

int css_cli_perms_init(int reload)
{
	struct css_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	struct css_config *cfg;
	char *cat = NULL;
	struct css_variable *v;
	struct usergroup_cli_perm *user_group, *cp_entry;
	struct cli_perm *perm = NULL;
	struct passwd *pw;
	struct group *gr;

	if (css_mutex_trylock(&permsconfiglock)) {
		css_log(LOG_NOTICE, "You must wait until lcss 'cli reload permissions' command finish\n");
		return 1;
	}

	cfg = css_config_load2(perms_config, "" /* core, can't reload */, config_flags);
	if (!cfg) {
		css_mutex_unlock(&permsconfiglock);
		return 1;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		css_mutex_unlock(&permsconfiglock);
		return 0;
	}

	/* free current structures. */
	destroy_user_perms();

	while ((cat = css_category_browse(cfg, cat))) {
		if (!strcasecmp(cat, "general")) {
			/* General options */
			for (v = css_variable_browse(cfg, cat); v; v = v->next) {
				if (!strcasecmp(v->name, "default_perm")) {
					cli_default_perm = (!strcasecmp(v->value, "permit")) ? 1: 0;
				}
			}
			continue;
		}

		/* users or groups */
		gr = NULL, pw = NULL;
		if (cat[0] == '@') {
			/* This is a group */
			gr = getgrnam(&cat[1]);
			if (!gr) {
				css_log (LOG_WARNING, "Unknown group '%s'\n", &cat[1]);
				continue;
			}
		} else {
			/* This is a user */
			pw = getpwnam(cat);
			if (!pw) {
				css_log (LOG_WARNING, "Unknown user '%s'\n", cat);
				continue;
			}
		}
		user_group = NULL;
		/* Check for duplicates */
		CSS_RWLIST_WRLOCK(&cli_perms);
		CSS_LIST_TRAVERSE(&cli_perms, cp_entry, list) {
			if ((pw && cp_entry->uid == pw->pw_uid) || (gr && cp_entry->gid == gr->gr_gid)) {
				/* if it is duplicated, just added this new settings, to 
				the current list. */
				user_group = cp_entry;
				break;
			}
		}
		CSS_RWLIST_UNLOCK(&cli_perms);

		if (!user_group) {
			/* alloc space for the new user config. */
			user_group = css_calloc(1, sizeof(*user_group));
			if (!user_group) {
				continue;
			}
			user_group->uid = (pw ? pw->pw_uid : -1);
			user_group->gid = (gr ? gr->gr_gid : -1);
			user_group->perms = css_calloc(1, sizeof(*user_group->perms));
			if (!user_group->perms) {
				css_free(user_group);
				continue;
			}
		}
		for (v = css_variable_browse(cfg, cat); v; v = v->next) {
			if (css_strlen_zero(v->value)) {
				/* we need to check this condition cause it could break security. */
				css_log(LOG_WARNING, "Empty permit/deny option in user '%s'\n", cat);
				continue;
			}
			if (!strcasecmp(v->name, "permit")) {
				perm = css_calloc(1, sizeof(*perm));
				if (perm) {
					perm->permit = 1;
					perm->command = css_strdup(v->value);
				}
			} else if (!strcasecmp(v->name, "deny")) {
				perm = css_calloc(1, sizeof(*perm));
				if (perm) {
					perm->permit = 0;
					perm->command = css_strdup(v->value);
				}
			} else {
				/* up to now, only 'permit' and 'deny' are possible values. */
				css_log(LOG_WARNING, "Unknown '%s' option\n", v->name);
				continue;
			}
			if (perm) {
				/* Added the permission to the user's list. */
				CSS_LIST_INSERT_TAIL(user_group->perms, perm, list);
				perm = NULL;
			}
		}
		CSS_RWLIST_WRLOCK(&cli_perms);
		CSS_RWLIST_INSERT_TAIL(&cli_perms, user_group, list);
		CSS_RWLIST_UNLOCK(&cli_perms);
	}

	css_config_destroy(cfg);
	css_mutex_unlock(&permsconfiglock);
	return 0;
}

/*! \brief initialize the _full_cmd string in * each of the builtins. */
void css_builtins_init(void)
{
	css_cli_register_multiple(cli_cli, ARRAY_LEN(cli_cli));
}

/*!
 * match a word in the CLI entry.
 * returns -1 on mismatch, 0 on match of an optional word,
 * 1 on match of a full word.
 *
 * The pattern can be
 *   any_word           match for equal
 *   [foo|bar|baz]      optionally, one of these words
 *   {foo|bar|baz}      exactly, one of these words
 *   %                  any word
 */
static int word_match(const char *cmd, const char *cli_word)
{
	int l;
	char *pos;

	if (css_strlen_zero(cmd) || css_strlen_zero(cli_word))
		return -1;
	if (!strchr(cli_rsvd, cli_word[0])) /* normal match */
		return (strcasecmp(cmd, cli_word) == 0) ? 1 : -1;
	l = strlen(cmd);
	/* wildcard match - will extend in the future */
	if (l > 0 && cli_word[0] == '%') {
		return 1;	/* wildcard */
	}

	/* Start a search for the command entered against the cli word in question */
	pos = strcasestr(cli_word, cmd);
	while (pos) {

		/*
		 *Check if the word matched with is surrounded by reserved characters on both sides
		 * and isn't at the beginning of the cli_word since that would make it check in a location we shouldn't know about.
		 * If it is surrounded by reserved chars and isn't at the beginning, it's a match.
		 */
		if (pos != cli_word && strchr(cli_rsvd, pos[-1]) && strchr(cli_rsvd, pos[l])) {
			return 1;	/* valid match */
		}

		/* Ok, that one didn't match, strcasestr to the next appearance of the command and start over.*/
		pos = strcasestr(pos + 1, cmd);
	}
	/* If no matches were found over the course of the while loop, we hit the end of the string. It's a mismatch. */
	return -1;
}

/*! \brief if word is a valid prefix for token, returns the pos-th
 * match as a malloced string, or NULL otherwise.
 * Always tell in *actual how many matches we got.
 */
static char *is_prefix(const char *word, const char *token,
	int pos, int *actual)
{
	int lw;
	char *s, *t1;

	*actual = 0;
	if (css_strlen_zero(token))
		return NULL;
	if (css_strlen_zero(word))
		word = "";	/* dummy */
	lw = strlen(word);
	if (strcspn(word, cli_rsvd) != lw)
		return NULL;	/* no match if word has reserved chars */
	if (strchr(cli_rsvd, token[0]) == NULL) {	/* regular match */
		if (strncasecmp(token, word, lw))	/* no match */
			return NULL;
		*actual = 1;
		return (pos != 0) ? NULL : css_strdup(token);
	}
	/* now handle regexp match */

	/* Wildcard always matches, so we never do is_prefix on them */

	t1 = css_strdupa(token + 1);	/* copy, skipping first char */
	while (pos >= 0 && (s = strsep(&t1, cli_rsvd)) && *s) {
		if (*s == '%')	/* wildcard */
			continue;
		if (strncasecmp(s, word, lw))	/* no match */
			continue;
		(*actual)++;
		if (pos-- == 0)
			return css_strdup(s);
	}
	return NULL;
}

/*!
 * \internal
 * \brief locate a cli command in the 'helpers' list (which must be locked).
 *     The search compares word by word taking care of regexps in e->cmda
 *     This function will return NULL when nothing is matched, or the css_cli_entry that matched.
 * \param cmds
 * \param match_type has 3 possible values:
 *      0       returns if the search key is equal or longer than the entry.
 *		            note that trailing optional arguments are skipped.
 *      -1      true if the mismatch is on the lcss word XXX not true!
 *      1       true only on complete, exact match.
 *
 */
static struct css_cli_entry *find_cli(const char * const cmds[], int match_type)
{
	int matchlen = -1;	/* length of longest match so far */
	struct css_cli_entry *cand = NULL, *e=NULL;

	while ( (e = cli_next(e)) ) {
		/* word-by word regexp comparison */
		const char * const *src = cmds;
		const char * const *dst = e->cmda;
		int n = 0;
		for (;; dst++, src += n) {
			n = word_match(*src, *dst);
			if (n < 0)
				break;
		}
		if (css_strlen_zero(*dst) || ((*dst)[0] == '[' && css_strlen_zero(dst[1]))) {
			/* no more words in 'e' */
			if (css_strlen_zero(*src))	/* exact match, cannot do better */
				break;
			/* Here, cmds has more words than the entry 'e' */
			if (match_type != 0)	/* but we look for almost exact match... */
				continue;	/* so we skip this one. */
			/* otherwise we like it (case 0) */
		} else {	/* still words in 'e' */
			if (css_strlen_zero(*src))
				continue; /* cmds is shorter than 'e', not good */
			/* Here we have leftover words in cmds and 'e',
			 * but there is a mismatch. We only accept this one if match_type == -1
			 * and this is the lcss word for both.
			 */
			if (match_type != -1 || !css_strlen_zero(src[1]) ||
			    !css_strlen_zero(dst[1]))	/* not the one we look for */
				continue;
			/* good, we are in case match_type == -1 and mismatch on lcss word */
		}
		if (src - cmds > matchlen) {	/* remember the candidate */
			matchlen = src - cmds;
			cand = e;
		}
	}

	return e ? e : cand;
}

static char *find_best(const char *argv[])
{
	static char cmdline[80];
	int x;
	/* See how close we get, then print the candidate */
	const char *myargv[CSS_MAX_CMD_LEN] = { NULL, };

	CSS_RWLIST_RDLOCK(&helpers);
	for (x = 0; argv[x]; x++) {
		myargv[x] = argv[x];
		if (!find_cli(myargv, -1))
			break;
	}
	CSS_RWLIST_UNLOCK(&helpers);
	css_join(cmdline, sizeof(cmdline), myargv);
	return cmdline;
}

static int __css_cli_unregister(struct css_cli_entry *e, struct css_cli_entry *ed)
{
	if (e->inuse) {
		css_log(LOG_WARNING, "Can't remove command that is in use\n");
	} else {
		CSS_RWLIST_WRLOCK(&helpers);
		CSS_RWLIST_REMOVE(&helpers, e, list);
		CSS_RWLIST_UNLOCK(&helpers);
		css_free(e->_full_cmd);
		e->_full_cmd = NULL;
		if (e->handler) {
			/* this is a new-style entry. Reset fields and free memory. */
			char *cmda = (char *) e->cmda;
			memset(cmda, '\0', sizeof(e->cmda));
			css_free(e->command);
			e->command = NULL;
			e->usage = NULL;
		}
	}
	return 0;
}

static int __css_cli_register(struct css_cli_entry *e, struct css_cli_entry *ed)
{
	struct css_cli_entry *cur;
	int i, lf, ret = -1;

	struct css_cli_args a;	/* fake argument */
	char **dst = (char **)e->cmda;	/* need to ccss as the entry is readonly */
	char *s;

	memset(&a, '\0', sizeof(a));
	e->handler(e, CLI_INIT, &a);
	/* XXX check that usage and command are filled up */
	s = css_skip_blanks(e->command);
	s = e->command = css_strdup(s);
	for (i=0; !css_strlen_zero(s) && i < CSS_MAX_CMD_LEN-1; i++) {
		*dst++ = s;	/* store string */
		s = css_skip_nonblanks(s);
		if (*s == '\0')	/* we are done */
			break;
		*s++ = '\0';
		s = css_skip_blanks(s);
	}
	*dst++ = NULL;
	
	CSS_RWLIST_WRLOCK(&helpers);
	
	if (find_cli(e->cmda, 1)) {
		css_log(LOG_WARNING, "Command '%s' already registered (or something close enough)\n", S_OR(e->_full_cmd, e->command));
		goto done;
	}
	if (set_full_cmd(e))
		goto done;

	lf = e->cmdlen;
	CSS_RWLIST_TRAVERSE_SAFE_BEGIN(&helpers, cur, list) {
		int len = cur->cmdlen;
		if (lf < len)
			len = lf;
		if (strncasecmp(e->_full_cmd, cur->_full_cmd, len) < 0) {
			CSS_RWLIST_INSERT_BEFORE_CURRENT(e, list); 
			break;
		}
	}
	CSS_RWLIST_TRAVERSE_SAFE_END;

	if (!cur)
		CSS_RWLIST_INSERT_TAIL(&helpers, e, list); 
	ret = 0;	/* success */

done:
	CSS_RWLIST_UNLOCK(&helpers);

	return ret;
}

/* wrapper function, so we can unregister deprecated commands recursively */
int css_cli_unregister(struct css_cli_entry *e)
{
	return __css_cli_unregister(e, NULL);
}

/* wrapper function, so we can register deprecated commands recursively */
int css_cli_register(struct css_cli_entry *e)
{
	return __css_cli_register(e, NULL);
}

/*
 * register/unregister an array of entries.
 */
int css_cli_register_multiple(struct css_cli_entry *e, int len)
{
	int i, res = 0;

	for (i = 0; i < len; i++)
		res |= css_cli_register(e + i);

	return res;
}

int css_cli_unregister_multiple(struct css_cli_entry *e, int len)
{
	int i, res = 0;

	for (i = 0; i < len; i++)
		res |= css_cli_unregister(e + i);

	return res;
}


/*! \brief helper for final part of handle_help
 *  if locked = 1, assume the list is already locked
 */
static char *help1(int fd, const char * const match[], int locked)
{
	char matchstr[80] = "";
	struct css_cli_entry *e = NULL;
	int len = 0;
	int found = 0;

	if (match) {
		css_join(matchstr, sizeof(matchstr), match);
		len = strlen(matchstr);
	}
	if (!locked)
		CSS_RWLIST_RDLOCK(&helpers);
	while ( (e = cli_next(e)) ) {
		/* Hide commands that start with '_' */
		if (e->_full_cmd[0] == '_')
			continue;
		if (match && strncasecmp(matchstr, e->_full_cmd, len))
			continue;
		css_cli(fd, "%30.30s %s\n", e->_full_cmd, S_OR(e->summary, "<no description available>"));
		found++;
	}
	if (!locked)
		CSS_RWLIST_UNLOCK(&helpers);
	if (!found && matchstr[0])
		css_cli(fd, "No such command '%s'.\n", matchstr);
	return CLI_SUCCESS;
}

static char *handle_help(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	char fullcmd[80];
	struct css_cli_entry *my_e;
	char *res = CLI_SUCCESS;

	if (cmd == CLI_INIT) {
		e->command = "core show help";
		e->usage =
			"Usage: core show help [topic]\n"
			"       When called with a topic as an argument, displays usage\n"
			"       information on the given command. If called without a\n"
			"       topic, it provides a list of commands.\n";
		return NULL;

	} else if (cmd == CLI_GENERATE) {
		/* skip first 14 or 15 chars, "core show help " */
		int l = strlen(a->line);

		if (l > 15) {
			l = 15;
		}
		/* XXX watch out, should stop to the non-generator parts */
		return __css_cli_generator(a->line + l, a->word, a->n, 0);
	}
	if (a->argc == e->args) {
		return help1(a->fd, NULL, 0);
	}

	CSS_RWLIST_RDLOCK(&helpers);
	my_e = find_cli(a->argv + 3, 1);	/* try exact match first */
	if (!my_e) {
		res = help1(a->fd, a->argv + 3, 1 /* locked */);
		CSS_RWLIST_UNLOCK(&helpers);
		return res;
	}
	if (my_e->usage)
		css_cli(a->fd, "%s", my_e->usage);
	else {
		css_join(fullcmd, sizeof(fullcmd), a->argv + 3);
		css_cli(a->fd, "No help text available for '%s'.\n", fullcmd);
	}
	CSS_RWLIST_UNLOCK(&helpers);
	return res;
}

static char *parse_args(const char *s, int *argc, const char *argv[], int max, int *trailingwhitespace)
{
	char *duplicate, *cur;
	int x = 0;
	int quoted = 0;
	int escaped = 0;
	int whitespace = 1;
	int dummy = 0;

	if (trailingwhitespace == NULL)
		trailingwhitespace = &dummy;
	*trailingwhitespace = 0;
	if (s == NULL)	/* invalid, though! */
		return NULL;
	/* make a copy to store the parsed string */
	if (!(duplicate = css_strdup(s)))
		return NULL;

	cur = duplicate;
	/* scan the original string copying into cur when needed */
	for (; *s ; s++) {
		if (x >= max - 1) {
			css_log(LOG_WARNING, "Too many arguments, truncating at %s\n", s);
			break;
		}
		if (*s == '"' && !escaped) {
			quoted = !quoted;
			if (quoted && whitespace) {
				/* start a quoted string from previous whitespace: new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
		} else if ((*s == ' ' || *s == '\t') && !(quoted || escaped)) {
			/* If we are not already in whitespace, and not in a quoted string or
			   processing an escape sequence, and just entered whitespace, then
			   finalize the previous argument and remember that we are in whitespace
			*/
			if (!whitespace) {
				*cur++ = '\0';
				whitespace = 1;
			}
		} else if (*s == '\\' && !escaped) {
			escaped = 1;
		} else {
			if (whitespace) {
				/* we leave whitespace, and are not quoted. So it's a new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
			*cur++ = *s;
			escaped = 0;
		}
	}
	/* Null terminate */
	*cur++ = '\0';
	/* XXX put a NULL in the lcss argument, because some functions that take
	 * the array may want a null-terminated array.
	 * argc still reflects the number of non-NULL entries.
	 */
	argv[x] = NULL;
	*argc = x;
	*trailingwhitespace = whitespace;
	return duplicate;
}

/*! \brief Return the number of unique matches for the generator */
int css_cli_generatornummatches(const char *text, const char *word)
{
	int matches = 0, i = 0;
	char *buf = NULL, *oldbuf = NULL;

	while ((buf = css_cli_generator(text, word, i++))) {
		if (!oldbuf || strcmp(buf,oldbuf))
			matches++;
		if (oldbuf)
			css_free(oldbuf);
		oldbuf = buf;
	}
	if (oldbuf)
		css_free(oldbuf);
	return matches;
}

char **css_cli_completion_matches(const char *text, const char *word)
{
	char **match_list = NULL, *retstr, *prevstr;
	size_t match_list_len, max_equal, which, i;
	int matches = 0;

	/* leave entry 0 free for the longest common substring */
	match_list_len = 1;
	while ((retstr = css_cli_generator(text, word, matches)) != NULL) {
		if (matches + 1 >= match_list_len) {
			match_list_len <<= 1;
			if (!(match_list = css_realloc(match_list, match_list_len * sizeof(*match_list))))
				return NULL;
		}
		match_list[++matches] = retstr;
	}

	if (!match_list)
		return match_list; /* NULL */

	/* Find the longest substring that is common to all results
	 * (it is a candidate for completion), and store a copy in entry 0.
	 */
	prevstr = match_list[1];
	max_equal = strlen(prevstr);
	for (which = 2; which <= matches; which++) {
		for (i = 0; i < max_equal && toupper(prevstr[i]) == toupper(match_list[which][i]); i++)
			continue;
		max_equal = i;
	}

	if (!(retstr = css_malloc(max_equal + 1)))
		return NULL;
	
	css_copy_string(retstr, match_list[1], max_equal + 1);
	match_list[0] = retstr;

	/* ensure that the array is NULL terminated */
	if (matches + 1 >= match_list_len) {
		if (!(match_list = css_realloc(match_list, (match_list_len + 1) * sizeof(*match_list))))
			return NULL;
	}
	match_list[matches + 1] = NULL;

	return match_list;
}

/*! \brief returns true if there are more words to match */
static int more_words (const char * const *dst)
{
	int i;
	for (i = 0; dst[i]; i++) {
		if (dst[i][0] != '[')
			return -1;
	}
	return 0;
}
	
/*
 * generate the entry at position 'state'
 */
static char *__css_cli_generator(const char *text, const char *word, int state, int lock)
{
	const char *argv[CSS_MAX_ARGS];
	struct css_cli_entry *e = NULL;
	int x = 0, argindex, matchlen;
	int matchnum=0;
	char *ret = NULL;
	char matchstr[80] = "";
	int tws = 0;
	/* Split the argument into an array of words */
	char *duplicate = parse_args(text, &x, argv, ARRAY_LEN(argv), &tws);

	if (!duplicate)	/* malloc error */
		return NULL;

	/* Compute the index of the lcss argument (could be an empty string) */
	argindex = (!css_strlen_zero(word) && x>0) ? x-1 : x;

	/* rebuild the command, ignore terminating white space and flatten space */
	css_join(matchstr, sizeof(matchstr)-1, argv);
	matchlen = strlen(matchstr);
	if (tws) {
		strcat(matchstr, " "); /* XXX */
		if (matchlen)
			matchlen++;
	}
	if (lock)
		CSS_RWLIST_RDLOCK(&helpers);
	while ( (e = cli_next(e)) ) {
		/* XXX repeated code */
		int src = 0, dst = 0, n = 0;

		if (e->command[0] == '_')
			continue;

		/*
		 * Try to match words, up to and excluding the lcss word, which
		 * is either a blank or something that we want to extend.
		 */
		for (;src < argindex; dst++, src += n) {
			n = word_match(argv[src], e->cmda[dst]);
			if (n < 0)
				break;
		}

		if (src != argindex && more_words(e->cmda + dst))	/* not a match */
			continue;
		ret = is_prefix(argv[src], e->cmda[dst], state - matchnum, &n);
		matchnum += n;	/* this many matches here */
		if (ret) {
			/*
			 * argv[src] is a valid prefix of the next word in this
			 * command. If this is also the correct entry, return it.
			 */
			if (matchnum > state)
				break;
			css_free(ret);
			ret = NULL;
		} else if (css_strlen_zero(e->cmda[dst])) {
			/*
			 * This entry is a prefix of the command string entered
			 * (only one entry in the list should have this property).
			 * Run the generator if one is available. In any case we are done.
			 */
			if (e->handler) {	/* new style command */
				struct css_cli_args a = {
					.line = matchstr, .word = word,
					.pos = argindex,
					.n = state - matchnum,
					.argv = argv,
					.argc = x};
				ret = e->handler(e, CLI_GENERATE, &a);
			}
			if (ret)
				break;
		}
	}
	if (lock)
		CSS_RWLIST_UNLOCK(&helpers);
	css_free(duplicate);
	return ret;
}

char *css_cli_generator(const char *text, const char *word, int state)
{
	return __css_cli_generator(text, word, state, 1);
}

int css_cli_command_full(int uid, int gid, int fd, const char *s)
{
	const char *args[CSS_MAX_ARGS + 1];
	struct css_cli_entry *e;
	int x;
	char *duplicate = parse_args(s, &x, args + 1, CSS_MAX_ARGS, NULL);
	char tmp[CSS_MAX_ARGS + 1];
	char *retval = NULL;
	struct css_cli_args a = {
		.fd = fd, .argc = x, .argv = args+1 };

	if (duplicate == NULL)
		return -1;

	if (x < 1)	/* We need at lecss one entry, otherwise ignore */
		goto done;

	CSS_RWLIST_RDLOCK(&helpers);
	e = find_cli(args + 1, 0);
	if (e)
		css_atomic_fetchadd_int(&e->inuse, 1);
	CSS_RWLIST_UNLOCK(&helpers);
	if (e == NULL) {
		css_cli(fd, "No such command '%s' (type 'core show help %s' for other possible commands)\n", s, find_best(args + 1));
		goto done;
	}

	css_join(tmp, sizeof(tmp), args + 1);
	/* Check if the user has rights to run this command. */
	if (!cli_has_permissions(uid, gid, tmp)) {
		css_cli(fd, "You don't have permissions to run '%s' command\n", tmp);
		css_free(duplicate);
		return 0;
	}

	/*
	 * Within the handler, argv[-1] contains a pointer to the css_cli_entry.
	 * Remember that the array returned by parse_args is NULL-terminated.
	 */
	args[0] = (char *)e;

	retval = e->handler(e, CLI_HANDLER, &a);

	if (retval == CLI_SHOWUSAGE) {
		css_cli(fd, "%s", S_OR(e->usage, "Invalid usage, but no usage information available.\n"));
	} else {
		if (retval == CLI_FAILURE)
			css_cli(fd, "Command '%s' failed.\n", s);
	}
	css_atomic_fetchadd_int(&e->inuse, -1);
done:
	css_free(duplicate);
	return 0;
}

int css_cli_command_multiple_full(int uid, int gid, int fd, size_t size, const char *s)
{
	char cmd[512];
	int x, y = 0, count = 0;

	for (x = 0; x < size; x++) {
		cmd[y] = s[x];
		y++;
		if (s[x] == '\0') {
			css_cli_command_full(uid, gid, fd, cmd);
			y = 0;
			count++;
		}
	}
	return count;
}
