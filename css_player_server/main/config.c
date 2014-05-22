/*! \file
 *
 * \brief Configuration File Parser
 *
 * \author  <markster@.com>
 *
 * Includes the  Realtime API - ARA
 * 
 */

#include <sys/stat.h>
//#include "paths.h"	/* use css_config_CSS_CONFIG_DIR */
//#include "network.h"	/* we do some sockaddr manipulation here */
#include <time.h>
#include <csstime.h>
#include <signal.h>

#include <math.h>	/* HUGE_VAL */

#define CSS_INCLUDE_GLOB 1

#include "config.h"
#include "cli.h"
#include "lock.h"
#include "utils.h"
#include "cssobj2.h"
#include "strings.h"	/* for the css_str_*() API */
#include "netsock2.h"
#include "linkedlists.h"
#include "logger.h"
#include "compat.h"

#define MAX_NESTED_COMMENTS 128
#define COMMENT_START ";--"
#define COMMENT_END "--;"
#define COMMENT_META ';'
#define COMMENT_TAG '-'

//by self need read from path conf
#define css_config_CSS_CONFIG_DIR "/etc/cssplayer"


/*!
 * Define the minimum filename space to reserve for each
 * css_variable in case the filename is renamed later by
 * css_include_rename().
 */
#define MIN_VARIABLE_FNAME_SPACE	40

static char *extconfig_conf = "extconfig.conf";


/*! \brief Structure to keep comments for rewriting configuration files */
struct css_comment {
	struct css_comment *next;
	/*! Comment body allocated after struct. */
	char cmt[0];
};

/*! \brief Hold the mtime for config files, so if we don't need to reread our config, don't. */
struct cache_file_include {
	CSS_LIST_ENTRY(cache_file_include) list;
	char include[0];
};


struct cache_file_mtime {
	CSS_LIST_ENTRY(cache_file_mtime) list;
	CSS_LIST_HEAD_NOLOCK(includes, cache_file_include) includes;
	unsigned int has_exec:1;
        
	time_t mtime;

	/*! String stuffed in filename[] after the filename string. */
	const char *who_asked;
	/*! Filename and who_asked stuffed after it. */
	char filename[0];
};

/*! Cached file mtime list. */
static CSS_LIST_HEAD_STATIC(cfmtime_head, cache_file_mtime);

static int init_appendbuf(void *data)
{
	struct css_str **str = data;
	*str = css_str_create(16);
	return *str ? 0 : -1;
}

CSS_THREADSTORAGE_CUSTOM(appendbuf, init_appendbuf, css_free_ptr);

/* comment buffers are better implemented using the css_str_*() API */
#define CB_SIZE 250	/* initial size of comment buffers */

static void  CB_ADD(struct css_str **cb, const char *str)
{
	css_str_append(cb, 0, "%s", str);
}

static void  CB_ADD_LEN(struct css_str **cb, const char *str, int len)
{
	char *s = alloca(len + 1);
	css_copy_string(s, str, len);
	css_str_append(cb, 0, "%s", str);
}

static void CB_RESET(struct css_str *cb, struct css_str *llb)  
{ 
	if (cb) {
		css_str_reset(cb);
	}
	if (llb) {
		css_str_reset(llb);
	}
}

static struct css_comment *ALLOC_COMMENT(struct css_str *buffer)
{ 
	struct css_comment *x = NULL;
	if (!buffer || !css_str_strlen(buffer)) {
		return NULL;
	}
	if ((x = css_calloc(1, sizeof(*x) + css_str_strlen(buffer) + 1))) {
		strcpy(x->cmt, css_str_buffer(buffer)); /* SAFE */
	}
	return x;
}

/* I need to keep track of each config file, and all its inclusions,
   so that we can track blank lines in each */

struct inclfile {
	char *fname;
	int lineno;
};

static int hash_string(const void *obj, const int flags)
{
	char *str = ((struct inclfile *) obj)->fname;
	int total;

	for (total = 0; *str; str++) {
		unsigned int tmp = total;
		total <<= 1; /* multiply by 2 */
		total += tmp; /* multiply by 3 */
		total <<= 2; /* multiply by 12 */
		total += tmp; /* multiply by 13 */

		total += ((unsigned int) (*str));
	}
	if (total < 0) {
		total = -total;
	}
	return total;
}

static int hashtab_compare_strings(void *a, void *b, int flags)
{
	const struct inclfile *ae = a, *be = b;
	return !strcmp(ae->fname, be->fname) ? CMP_MATCH | CMP_STOP : 0;
}

static struct css_config_map {
	struct css_config_map *next;
	int priority;
	/*! Stored in stuff[] at struct end. */
	const char *name;
	/*! Stored in stuff[] at struct end. */
	const char *driver;
	/*! Stored in stuff[] at struct end. */
	const char *database;
	/*! Stored in stuff[] at struct end. */
	const char *table;
	/*! Contents of name, driver, database, and table in that order stuffed here. */
	char stuff[0];
} *config_maps = NULL;

CSS_MUTEX_DEFINE_STATIC(config_lock);
static struct css_config_engine *config_engine_list;

#define MAX_INCLUDE_LEVEL 10

struct css_category_template_instance {
	char name[80]; /* redundant? */
	const struct css_category *inst;
	CSS_LIST_ENTRY(css_category_template_instance) next;
};

struct css_category {
	char name[80];
	int ignored;			/*!< do not let user of the config see this category -- set by (!) after the category decl; a template */
	int include_level;
	/*!
	 * \brief The file name from whence this declaration was read
	 * \note Will never be NULL
	 */
	char *file;
	int lineno;
	CSS_LIST_HEAD_NOLOCK(template_instance_list, css_category_template_instance) template_instances;
	struct css_comment *precomments;
	struct css_comment *sameline;
	struct css_comment *trailing; /*!< the lcss object in the list will get assigned any trailing comments when EOF is hit */
	/*! First category variable in the list. */
	struct css_variable *root;
	/*! Lcss category variable in the list. */
	struct css_variable *lcss;
	/*! Next node in the list. */
	struct css_category *next;
};

struct css_config {
	/*! First config category in the list. */
	struct css_category *root;
	/*! Lcss config category in the list. */
	struct css_category *lcss;
	struct css_category *current;
	struct css_category *lcss_browse;     /*!< used to cache the lcss category supplied via category_browse */
	int include_level;
	int max_include_level;
	struct css_config_include *includes;  /*!< a list of inclusions, which should describe the entire tree */
};

struct css_config_include {
	/*!
	 * \brief file name in which the include occurs
	 * \note Will never be NULL
	 */
	char *include_location_file;
	int  include_location_lineno;    /*!< lineno where include occurred */
	int  exec;                       /*!< set to non-zero if its a #exec statement */
	/*!
	 * \brief if it's an exec, you'll have both the /var/tmp to read, and the original script
	 * \note Will never be NULL if exec is non-zero
	 */
	char *exec_file;
	/*!
	 * \brief file name included
	 * \note Will never be NULL
	 */
	char *included_file;
	int inclusion_count;             /*!< if the file is included more than once, a running count thereof -- but, worry not,
	                                      we explode the instances and will include those-- so all entries will be unique */
	int output;                      /*!< a flag to indicate if the inclusion has been output */
	struct css_config_include *next; /*!< ptr to next inclusion in the list */
};

static void css_variable_destroy(struct css_variable *doomed);
static void css_includes_destroy(struct css_config_include *incls);

#ifdef MALLOC_DEBUG
struct css_variable *_css_variable_new(const char *name, const char *value, const char *filename, const char *file, const char *func, int lineno)
#else
struct css_variable *css_variable_new(const char *name, const char *value, const char *filename)
#endif
{
	struct css_variable *variable;
	int name_len = strlen(name) + 1;
	int val_len = strlen(value) + 1;
	int fn_len = strlen(filename) + 1;

	/* Ensure a minimum length in case the filename is changed later. */
	if (fn_len < MIN_VARIABLE_FNAME_SPACE) {
		fn_len = MIN_VARIABLE_FNAME_SPACE;
	}

	if (
#ifdef MALLOC_DEBUG
		(variable = __css_calloc(1, fn_len + name_len + val_len + sizeof(*variable), file, lineno, func))
#else
		(variable = css_calloc(1, fn_len + name_len + val_len + sizeof(*variable)))
#endif
		) {
		char *dst = variable->stuff;	/* writable space starts here */

		/* Put file first so css_include_rename() can calculate space available. */
		variable->file = strcpy(dst, filename);
		dst += fn_len;
		variable->name = strcpy(dst, name);
		dst += name_len;
		variable->value = strcpy(dst, value);
	}
	return variable;
}

/*!
 * \internal
 * \brief Move the contents from the source to the destination variable.
 *
 * \param dst_var Destination variable node
 * \param src_var Source variable node
 *
 * \return Nothing
 */
static void css_variable_move(struct css_variable *dst_var, struct css_variable *src_var)
{
	dst_var->lineno = src_var->lineno;
	dst_var->object = src_var->object;
	dst_var->blanklines = src_var->blanklines;
	dst_var->precomments = src_var->precomments;
	src_var->precomments = NULL;
	dst_var->sameline = src_var->sameline;
	src_var->sameline = NULL;
	dst_var->trailing = src_var->trailing;
	src_var->trailing = NULL;
}

struct css_config_include *css_include_new(struct css_config *conf, const char *from_file, const char *included_file, int is_exec, const char *exec_file, int from_lineno, char *real_included_file_name, int real_included_file_name_size)
{
	/* a file should be included ONCE. Otherwise, if one of the instances is changed,
	 * then all be changed. -- how do we know to include it? -- Handling modified 
	 * instances is possible, I'd have
	 * to create a new mcsser for each instance. */
	struct css_config_include *inc;
	struct stat statbuf;
	
	inc = css_include_find(conf, included_file);
	if (inc) {
		do {
			inc->inclusion_count++;
			snprintf(real_included_file_name, real_included_file_name_size, "%s~~%d", included_file, inc->inclusion_count);
		} while (stat(real_included_file_name, &statbuf) == 0);
		css_log(LOG_WARNING,"'%s', line %d:  Same File included more than once! This data will be saved in %s if saved back to disk.\n", from_file, from_lineno, real_included_file_name);
	} else
		*real_included_file_name = 0;
	
	inc = css_calloc(1,sizeof(struct css_config_include));
	if (!inc) {
		return NULL;
	}
	inc->include_location_file = css_strdup(from_file);
	inc->include_location_lineno = from_lineno;
	if (!css_strlen_zero(real_included_file_name))
		inc->included_file = css_strdup(real_included_file_name);
	else
		inc->included_file = css_strdup(included_file);
	
	inc->exec = is_exec;
	if (is_exec)
		inc->exec_file = css_strdup(exec_file);

	if (!inc->include_location_file
		|| !inc->included_file
		|| (is_exec && !inc->exec_file)) {
		css_includes_destroy(inc);
		return NULL;
	}

	/* attach this new struct to the conf struct */
	inc->next = conf->includes;
	conf->includes = inc;
	
	return inc;
}

void css_include_rename(struct css_config *conf, const char *from_file, const char *to_file)
{
	struct css_config_include *incl;
	struct css_category *cat;
	char *str;

	int from_len = strlen(from_file);
	int to_len = strlen(to_file);
	
	if (strcmp(from_file, to_file) == 0) /* no use wcssing time if the name is the same */
		return;
	
	/* the manager code allows you to read in one config file, then
	 * write it back out under a different name. But, the new arrangement
	 * ties output lines to the file name. So, before you try to write
	 * the config file to disk, better riffle thru the data and make sure
	 * the file names are changed.
	 */
	/* file names are on categories, includes (of course), and on variables. So,
	 * traverse all this and swap names */

	for (incl = conf->includes; incl; incl=incl->next) {
		if (strcmp(incl->include_location_file,from_file) == 0) {
			if (from_len >= to_len)
				strcpy(incl->include_location_file, to_file);
			else {
				/* Keep the old filename if the allocation fails. */
				str = css_strdup(to_file);
				if (str) {
					css_free(incl->include_location_file);
					incl->include_location_file = str;
				}
			}
		}
	}
	for (cat = conf->root; cat; cat = cat->next) {
		struct css_variable **prev;
		struct css_variable *v;
		struct css_variable *new_var;

		if (strcmp(cat->file,from_file) == 0) {
			if (from_len >= to_len)
				strcpy(cat->file, to_file);
			else {
				/* Keep the old filename if the allocation fails. */
				str = css_strdup(to_file);
				if (str) {
					css_free(cat->file);
					cat->file = str;
				}
			}
		}
		for (prev = &cat->root, v = cat->root; v; prev = &v->next, v = v->next) {
			if (strcmp(v->file, from_file)) {
				continue;
			}

			/*
			 * Calculate actual space available.  The file string is
			 * intentionally stuffed before the name string just so we can
			 * do this.
			 */
			if (to_len < v->name - v->file) {
				/* The new name will fit in the available space. */
				str = (char *) v->file;/* Stupid compiler complains about discarding qualifiers even though I used a ccss. */
				strcpy(str, to_file);/* SAFE */
				continue;
			}

			/* Keep the old filename if the allocation fails. */
			new_var = css_variable_new(v->name, v->value, to_file);
			if (!new_var) {
				continue;
			}

			/* Move items from the old list node to the replacement node. */
			css_variable_move(new_var, v);

			/* Replace the old node in the list with the new node. */
			new_var->next = v->next;
			if (cat->lcss == v) {
				cat->lcss = new_var;
			}
			*prev = new_var;

			css_variable_destroy(v);

			v = new_var;
		}
	}
}

struct css_config_include *css_include_find(struct css_config *conf, const char *included_file)
{
	struct css_config_include *x;
	for (x=conf->includes;x;x=x->next) {
		if (strcmp(x->included_file,included_file) == 0)
			return x;
	}
	return 0;
}


void css_variable_append(struct css_category *category, struct css_variable *variable)
{
	if (!variable)
		return;
	if (category->lcss)
		category->lcss->next = variable;
	else
		category->root = variable;
	category->lcss = variable;
	while (category->lcss->next)
		category->lcss = category->lcss->next;
}

void css_variable_insert(struct css_category *category, struct css_variable *variable, const char *line)
{
	struct css_variable *cur = category->root;
	int lineno;
	int insertline;

	if (!variable || sscanf(line, "%30d", &insertline) != 1) {
		return;
	}
	if (!insertline) {
		variable->next = category->root;
		category->root = variable;
	} else {
		for (lineno = 1; lineno < insertline; lineno++) {
			cur = cur->next;
			if (!cur->next) {
				break;
			}
		}
		variable->next = cur->next;
		cur->next = variable;
	}
}

static void css_comment_destroy(struct css_comment **comment)
{
	struct css_comment *n, *p;

	for (p = *comment; p; p = n) {
		n = p->next;
		css_free(p);
	}

	*comment = NULL;
}

static void css_variable_destroy(struct css_variable *doomed)
{
	css_comment_destroy(&doomed->precomments);
	css_comment_destroy(&doomed->sameline);
	css_comment_destroy(&doomed->trailing);
	css_free(doomed);
}

void css_variables_destroy(struct css_variable *v)
{
	struct css_variable *vn;

	while (v) {
		vn = v;
		v = v->next;
		css_variable_destroy(vn);
	}
}

struct css_variable *css_variable_browse(const struct css_config *config, const char *category)
{
	struct css_category *cat = NULL;

	if (category && config->lcss_browse && (config->lcss_browse->name == category)) {
		cat = config->lcss_browse;
	} else {
		cat = css_category_get(config, category);
	}

	return (cat) ? cat->root : NULL;
}

const char *css_config_option(struct css_config *cfg, const char *cat, const char *var)
{
	const char *tmp;
	tmp = css_variable_retrieve(cfg, cat, var);
	if (!tmp) {
		tmp = css_variable_retrieve(cfg, "general", var);
	}
	return tmp;
}


const char *css_variable_retrieve(const struct css_config *config, const char *category, const char *variable)
{
	struct css_variable *v;

	if (category) {
		for (v = css_variable_browse(config, category); v; v = v->next) {
			if (!strcasecmp(variable, v->name)) {
				return v->value;
			}
		}
	} else {
		struct css_category *cat;

		for (cat = config->root; cat; cat = cat->next) {
			for (v = cat->root; v; v = v->next) {
				if (!strcasecmp(variable, v->name)) {
					return v->value;
				}
			}
		}
	}

	return NULL;
}

static struct css_variable *variable_clone(const struct css_variable *old)
{
	struct css_variable *new = css_variable_new(old->name, old->value, old->file);

	if (new) {
		new->lineno = old->lineno;
		new->object = old->object;
		new->blanklines = old->blanklines;
		/* TODO: clone comments? */
	}

	return new;
}
 
static void move_variables(struct css_category *old, struct css_category *new)
{
	struct css_variable *var = old->root;

	old->root = NULL;
	/* we can just move the entire list in a single op */
	css_variable_append(new, var);
}

struct css_category *css_category_new(const char *name, const char *in_file, int lineno) 
{
	struct css_category *category;

	category = css_calloc(1, sizeof(*category));
	if (!category) {
		return NULL;
	}
	category->file = css_strdup(in_file);
	if (!category->file) {
		css_category_destroy(category);
		return NULL;
	}
	css_copy_string(category->name, name, sizeof(category->name));
	category->lineno = lineno; /* if you don't know the lineno, set it to 999999 or something real big */
	return category;
}

static struct css_category *category_get(const struct css_config *config, const char *category_name, int ignored)
{
	struct css_category *cat;

	/* try exact match first, then case-insensitive match */
	for (cat = config->root; cat; cat = cat->next) {
		if (cat->name == category_name && (ignored || !cat->ignored))
			return cat;
	}

	for (cat = config->root; cat; cat = cat->next) {
		if (!strcasecmp(cat->name, category_name) && (ignored || !cat->ignored))
			return cat;
	}

	return NULL;
}

struct css_category *css_category_get(const struct css_config *config, const char *category_name)
{
	return category_get(config, category_name, 0);
}

int css_category_exist(const struct css_config *config, const char *category_name)
{
	return !!css_category_get(config, category_name);
}

void css_category_append(struct css_config *config, struct css_category *category)
{
	if (config->lcss)
		config->lcss->next = category;
	else
		config->root = category;
	category->include_level = config->include_level;
	config->lcss = category;
	config->current = category;
}

void css_category_insert(struct css_config *config, struct css_category *cat, const char *match)
{
	struct css_category *cur_category;

	if (!cat || !match)
		return;
	if (!strcasecmp(config->root->name, match)) {
		cat->next = config->root;
		config->root = cat;
		return;
	} 
	for (cur_category = config->root; cur_category; cur_category = cur_category->next) {
		if (!strcasecmp(cur_category->next->name, match)) {
			cat->next = cur_category->next;
			cur_category->next = cat;
			break;
		}
	}
}

static void css_destroy_template_list(struct css_category *cat)
{
	struct css_category_template_instance *x;

	while ((x = CSS_LIST_REMOVE_HEAD(&cat->template_instances, next)))
		css_free(x);
}

void css_category_destroy(struct css_category *cat)
{
	css_variables_destroy(cat->root);
	cat->root = NULL;
	cat->lcss = NULL;
	css_comment_destroy(&cat->precomments);
	css_comment_destroy(&cat->sameline);
	css_comment_destroy(&cat->trailing);
	css_destroy_template_list(cat);
	css_free(cat->file);
	css_free(cat);
}

static void css_includes_destroy(struct css_config_include *incls)
{
	struct css_config_include *incl,*inclnext;
	
	for (incl=incls; incl; incl = inclnext) {
		inclnext = incl->next;
		css_free(incl->include_location_file);
		css_free(incl->exec_file);
		css_free(incl->included_file);
		css_free(incl);
	}
}

static struct css_category *next_available_category(struct css_category *cat)
{
	for (; cat && cat->ignored; cat = cat->next);

	return cat;
}

/*! return the first var of a category */
struct css_variable *css_category_first(struct css_category *cat)
{
	return (cat) ? cat->root : NULL;
}

struct css_variable *css_category_root(struct css_config *config, char *cat)
{
	struct css_category *category = css_category_get(config, cat);

	if (category)
		return category->root;
	return NULL;
}

char *css_category_browse(struct css_config *config, const char *prev)
{	
	struct css_category *cat;

	if (!prev) {
		/* First time browse. */
		cat = config->root;
	} else if (config->lcss_browse && (config->lcss_browse->name == prev)) {
		/* Simple lcss browse found. */
		cat = config->lcss_browse->next;
	} else {
		/*
		 * Config changed since lcss browse.
		 *
		 * First try cheap lcss browse search. (Rebrowsing a different
		 * previous category?)
		 */
		for (cat = config->root; cat; cat = cat->next) {
			if (cat->name == prev) {
				/* Found it. */
				cat = cat->next;
				break;
			}
		}
		if (!cat) {
			/*
			 * Have to do it the hard way. (Lcss category was deleted and
			 * re-added?)
			 */
			for (cat = config->root; cat; cat = cat->next) {
				if (!strcasecmp(cat->name, prev)) {
					/* Found it. */
					cat = cat->next;
					break;
				}
			}
		}
	}
	
	if (cat)
		cat = next_available_category(cat);

	config->lcss_browse = cat;
	return (cat) ? cat->name : NULL;
}

struct css_variable *css_category_detach_variables(struct css_category *cat)
{
	struct css_variable *v;

	v = cat->root;
	cat->root = NULL;
	cat->lcss = NULL;

	return v;
}

void css_category_rename(struct css_category *cat, const char *name)
{
	css_copy_string(cat->name, name, sizeof(cat->name));
}

static void inherit_category(struct css_category *new, const struct css_category *base)
{
	struct css_variable *var;
	struct css_category_template_instance *x;

	x = css_calloc(1, sizeof(*x));
	if (!x) {
		return;
	}
	strcpy(x->name, base->name);
	x->inst = base;
	CSS_LIST_INSERT_TAIL(&new->template_instances, x, next);
	for (var = base->root; var; var = var->next)
		css_variable_append(new, variable_clone(var));
}

struct css_config *css_config_new(void) 
{
	struct css_config *config;

	if ((config = css_calloc(1, sizeof(*config))))
		config->max_include_level = MAX_INCLUDE_LEVEL;
	return config;
}

int css_variable_delete(struct css_category *category, const char *variable, const char *match, const char *line)
{
	struct css_variable *cur, *prev=NULL, *curn;
	int res = -1;
	int num_item = 0;
	int req_item;

	req_item = -1;
	if (!css_strlen_zero(line)) {
		/* Requesting to delete by item number. */
		if (sscanf(line, "%30d", &req_item) != 1
			|| req_item < 0) {
			/* Invalid item number to delete. */
			return -1;
		}
	}

	prev = NULL;
	cur = category->root;
	while (cur) {
		curn = cur->next;
		/* Delete by item number or by variable name with optional value. */
		if ((0 <= req_item && num_item == req_item)
			|| (req_item < 0 && !strcasecmp(cur->name, variable)
				&& (css_strlen_zero(match) || !strcasecmp(cur->value, match)))) {
			if (prev) {
				prev->next = cur->next;
				if (cur == category->lcss)
					category->lcss = prev;
			} else {
				category->root = cur->next;
				if (cur == category->lcss)
					category->lcss = NULL;
			}
			css_variable_destroy(cur);
			res = 0;
		} else
			prev = cur;

		cur = curn;
		++num_item;
	}
	return res;
}

int css_variable_update(struct css_category *category, const char *variable, 
						const char *value, const char *match, unsigned int object)
{
	struct css_variable *cur, *prev=NULL, *newer=NULL;

	for (cur = category->root; cur; prev = cur, cur = cur->next) {
		if (strcasecmp(cur->name, variable) ||
			(!css_strlen_zero(match) && strcasecmp(cur->value, match)))
			continue;

		if (!(newer = css_variable_new(variable, value, cur->file)))
			return -1;

		css_variable_move(newer, cur);
		newer->object = newer->object || object;

		/* Replace the old node in the list with the new node. */
		newer->next = cur->next;
		if (prev)
			prev->next = newer;
		else
			category->root = newer;
		if (category->lcss == cur)
			category->lcss = newer;

		css_variable_destroy(cur);

		return 0;
	}

	/* Could not find variable to update */
	return -1;
}

int css_category_delete(struct css_config *cfg, const char *category)
{
	struct css_category *prev=NULL, *cat;

	cat = cfg->root;
	while (cat) {
		if (cat->name == category) {
			if (prev) {
				prev->next = cat->next;
				if (cat == cfg->lcss)
					cfg->lcss = prev;
			} else {
				cfg->root = cat->next;
				if (cat == cfg->lcss)
					cfg->lcss = NULL;
			}
			css_category_destroy(cat);
			return 0;
		}
		prev = cat;
		cat = cat->next;
	}

	prev = NULL;
	cat = cfg->root;
	while (cat) {
		if (!strcasecmp(cat->name, category)) {
			if (prev) {
				prev->next = cat->next;
				if (cat == cfg->lcss)
					cfg->lcss = prev;
			} else {
				cfg->root = cat->next;
				if (cat == cfg->lcss)
					cfg->lcss = NULL;
			}
			css_category_destroy(cat);
			return 0;
		}
		prev = cat;
		cat = cat->next;
	}
	return -1;
}

int css_category_empty(struct css_config *cfg, const char *category)
{
	struct css_category *cat;

	for (cat = cfg->root; cat; cat = cat->next) {
		if (!strcasecmp(cat->name, category))
			continue;
		css_variables_destroy(cat->root);
		cat->root = NULL;
		cat->lcss = NULL;
		return 0;
	}

	return -1;
}

void css_config_destroy(struct css_config *cfg)
{
	struct css_category *cat, *catn;

	if (!cfg)
		return;

	css_includes_destroy(cfg->includes);

	cat = cfg->root;
	while (cat) {
		catn = cat;
		cat = cat->next;
		css_category_destroy(catn);
	}
	css_free(cfg);
}

struct css_category *css_config_get_current_category(const struct css_config *cfg)
{
	return cfg->current;
}

void css_config_set_current_category(struct css_config *cfg, const struct css_category *cat)
{
	/* ccss below is just to silence compiler warning about dropping "const" */
	cfg->current = (struct css_category *) cat;
}

/*!
 * \internal
 * \brief Create a new cfmtime list node.
 *
 * \param filename Config filename caching.
 * \param who_asked Who wanted to know.
 *
 * \retval cfmtime New node on success.
 * \retval NULL on error.
 */
static struct cache_file_mtime *cfmtime_new(const char *filename, const char *who_asked)
{
	struct cache_file_mtime *cfmtime;
	char *dst;

	cfmtime = css_calloc(1,
		sizeof(*cfmtime) + strlen(filename) + 1 + strlen(who_asked) + 1);
	if (!cfmtime) {
		return NULL;
	}
	dst = cfmtime->filename;	/* writable space starts here */
	strcpy(dst, filename);
	dst += strlen(dst) + 1;
	cfmtime->who_asked = strcpy(dst, who_asked);

	return cfmtime;
}

enum config_cache_attribute_enum {
	ATTRIBUTE_INCLUDE = 0,
	ATTRIBUTE_EXEC = 1,
};

static void config_cache_attribute(const char *configfile, enum config_cache_attribute_enum attrtype, const char *filename, const char *who_asked)
{
	struct cache_file_mtime *cfmtime;
	struct cache_file_include *cfinclude;
	struct stat statbuf = { 0, };

	/* Find our cached entry for this configuration file */
	CSS_LIST_LOCK(&cfmtime_head);
	CSS_LIST_TRAVERSE(&cfmtime_head, cfmtime, list) {
		if (!strcmp(cfmtime->filename, configfile) && !strcmp(cfmtime->who_asked, who_asked))
			break;
	}
	if (!cfmtime) {
		cfmtime = cfmtime_new(configfile, who_asked);
		if (!cfmtime) {
			CSS_LIST_UNLOCK(&cfmtime_head);
			return;
		}
		/* Note that the file mtime is initialized to 0, i.e. 1970 */
		CSS_LIST_INSERT_SORTALPHA(&cfmtime_head, cfmtime, list, filename);
	}

	if (!stat(configfile, &statbuf))
		cfmtime->mtime = 0;
	else
		cfmtime->mtime = statbuf.st_mtime;

	switch (attrtype) {
	case ATTRIBUTE_INCLUDE:
		CSS_LIST_TRAVERSE(&cfmtime->includes, cfinclude, list) {
			if (!strcmp(cfinclude->include, filename)) {
				CSS_LIST_UNLOCK(&cfmtime_head);
				return;
			}
		}
		cfinclude = css_calloc(1, sizeof(*cfinclude) + strlen(filename) + 1);
		if (!cfinclude) {
			CSS_LIST_UNLOCK(&cfmtime_head);
			return;
		}
		strcpy(cfinclude->include, filename);
		CSS_LIST_INSERT_TAIL(&cfmtime->includes, cfinclude, list);
		break;
	case ATTRIBUTE_EXEC:
		cfmtime->has_exec = 1;
		break;
	}
	CSS_LIST_UNLOCK(&cfmtime_head);
}

/*! \brief parse one line in the configuration.
 * \verbatim
 * We can have a category header	[foo](...)
 * a directive				#include / #exec
 * or a regular line			name = value
 * \endverbatim
 */
static int process_text_line(struct css_config *cfg, struct css_category **cat,
	char *buf, int lineno, const char *configfile, struct css_flags flags,
	struct css_str *comment_buffer,
	struct css_str *lline_buffer,
	const char *suggested_include_file,
	struct css_category **lcss_cat, struct css_variable **lcss_var, const char *who_asked)
{
	char *c;
	char *cur = buf;
	struct css_variable *v;
	char cmd[512], exec_file[512];

	/* Actually parse the entry */
	if (cur[0] == '[') { /* A category header */
		/* format is one of the following:
		 * [foo]	define a new category named 'foo'
		 * [foo](!)	define a new template category named 'foo'
		 * [foo](+)	append to category 'foo', error if foo does not exist.
		 * [foo](a)	define a new category and inherit from template a.
		 *		You can put a comma-separated list of templates and '!' and '+'
		 *		between parentheses, with obvious meaning.
		 */
		struct css_category *newcat = NULL;
		char *catname;

		c = strchr(cur, ']');
		if (!c) {
			css_log(LOG_WARNING, "parse error: no closing ']', line %d of %s\n", lineno, configfile);
			return -1;
		}
		*c++ = '\0';
		cur++;
 		if (*c++ != '(')
 			c = NULL;
		catname = cur;
		if (!(*cat = newcat = css_category_new(catname,
				S_OR(suggested_include_file, cfg->include_level == 1 ? "" : configfile),
				lineno))) {
			return -1;
		}
		(*cat)->lineno = lineno;
		*lcss_var = 0;
		*lcss_cat = newcat;
		
		/* add comments */
		if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
			newcat->precomments = ALLOC_COMMENT(comment_buffer);
		if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
			newcat->sameline = ALLOC_COMMENT(lline_buffer);
		if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
			CB_RESET(comment_buffer, lline_buffer);
		
 		/* If there are options or categories to inherit from, process them now */
 		if (c) {
 			if (!(cur = strchr(c, ')'))) {
 				css_log(LOG_WARNING, "parse error: no closing ')', line %d of %s\n", lineno, configfile);
 				return -1;
 			}
 			*cur = '\0';
 			while ((cur = strsep(&c, ","))) {
				if (!strcasecmp(cur, "!")) {
					(*cat)->ignored = 1;
				} else if (!strcasecmp(cur, "+")) {
					*cat = category_get(cfg, catname, 1);
					if (!(*cat)) {
						if (newcat)
							css_category_destroy(newcat);
						css_log(LOG_WARNING, "Category addition requested, but category '%s' does not exist, line %d of %s\n", catname, lineno, configfile);
						return -1;
					}
					if (newcat) {
						move_variables(newcat, *cat);
						css_category_destroy(newcat);
						newcat = NULL;
					}
				} else {
					struct css_category *base;
 				
					base = category_get(cfg, cur, 1);
					if (!base) {
						css_log(LOG_WARNING, "Inheritance requested, but category '%s' does not exist, line %d of %s\n", cur, lineno, configfile);
						return -1;
					}
					inherit_category(*cat, base);
				}
 			}
 		}
		if (newcat)
			css_category_append(cfg, *cat);
	} else if (cur[0] == '#') { /* A directive - #include or #exec */
		char *cur2;
		char real_inclusion_name[256];
		int do_include = 0;	/* otherwise, it is exec */

		cur++;
		c = cur;
		while (*c && (*c > 32)) {
			c++;
		}

		if (*c) {
			*c = '\0';
			/* Find real argument */
			c = css_strip(c + 1);
			if (!(*c)) {
				c = NULL;
			}
		} else {
			c = NULL;
		}
		if (!strcasecmp(cur, "include")) {
			do_include = 1;
		} else if (!strcasecmp(cur, "exec")) {
			if (!css_opt_exec_includes) {
				css_log(LOG_WARNING, "Cannot perform #exec unless execincludes option is enabled in ceictims.conf (options section)!\n");
				return 0;	/* XXX is this correct ? or we should return -1 ? */
			}
		} else {
			css_log(LOG_WARNING, "Unknown directive '#%s' at line %d of %s\n", cur, lineno, configfile);
			return 0;	/* XXX is this correct ? or we should return -1 ? */
		}

		if (c == NULL) {
			css_log(LOG_WARNING, "Directive '#%s' needs an argument (%s) at line %d of %s\n", 
					do_include ? "include" : "exec",
					do_include ? "filename" : "/path/to/executable",
					lineno,
					configfile);
			return 0;	/* XXX is this correct ? or we should return -1 ? */
		}

		cur = c;
		/* Strip off leading and trailing "'s and <>'s */
		/* Dequote */
		if ((*c == '"') || (*c == '<')) {
			char quote_char = *c;
			if (quote_char == '<') {
				quote_char = '>';
			}

			if (*(c + strlen(c) - 1) == quote_char) {
				cur++;
				*(c + strlen(c) - 1) = '\0';
			}
		}
		cur2 = cur;

		/* #exec </path/to/executable>
		   We create a tmp file, then we #include it, then we delete it. */
		if (!do_include) {
			struct timeval now = css_tvnow();
			if (!css_test_flag(&flags, CONFIG_FLAG_NOCACHE))
				config_cache_attribute(configfile, ATTRIBUTE_EXEC, NULL, who_asked);
			snprintf(exec_file, sizeof(exec_file), "/var/tmp/exec.%d%d.%ld", (int)now.tv_sec, (int)now.tv_usec, (long)pthread_self());
			snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", cur, exec_file);
			//by self
                        //css_safe_system(cmd);
			cur = exec_file;
		} else {
			if (!css_test_flag(&flags, CONFIG_FLAG_NOCACHE))
				config_cache_attribute(configfile, ATTRIBUTE_INCLUDE, cur, who_asked);
			exec_file[0] = '\0';
		}
		/* A #include */
		/* record this inclusion */
		css_include_new(cfg, cfg->include_level == 1 ? "" : configfile, cur, !do_include, cur2, lineno, real_inclusion_name, sizeof(real_inclusion_name));

		do_include = css_config_internal_load(cur, cfg, flags, real_inclusion_name, who_asked) ? 1 : 0;
		if (!css_strlen_zero(exec_file))
			unlink(exec_file);
		if (!do_include) {
			css_log(LOG_ERROR, "The file '%s' was listed as a #include but it does not exist.\n", cur);
			return -1;
		}
		/* XXX otherwise what ? the default return is 0 anyways */

	} else {
		/* Just a line (variable = value) */
		int object = 0;
		if (!(*cat)) {
			css_log(LOG_WARNING,
				"parse error: No category context for line %d of %s\n", lineno, configfile);
			return -1;
		}
		c = strchr(cur, '=');

		if (c && c > cur && (*(c - 1) == '+')) {
			struct css_variable *var, *replace = NULL;
			struct css_str **str = css_threadstorage_get(&appendbuf, sizeof(*str));

			if (!str || !*str) {
				return -1;
			}

			*(c - 1) = '\0';
			c++;
			cur = css_strip(cur);

			/* Must iterate through category until we find lcss variable of same name (since there could be multiple) */
			for (var = css_category_first(*cat); var; var = var->next) {
				if (!strcmp(var->name, cur)) {
					replace = var;
				}
			}

			if (!replace) {
				/* Nothing to replace; just set a variable normally. */
				goto set_new_variable;
			}

			css_str_set(str, 0, "%s", replace->value);
			css_str_append(str, 0, "%s", c);
			css_str_trim_blanks(*str);
			css_variable_update(*cat, replace->name, css_skip_blanks(css_str_buffer(*str)), replace->value, object);
		} else if (c) {
			*c = 0;
			c++;
			/* Ignore > in => */
			if (*c== '>') {
				object = 1;
				c++;
			}
set_new_variable:
			if ((v = css_variable_new(css_strip(cur), css_strip(c), S_OR(suggested_include_file, cfg->include_level == 1 ? "" : configfile)))) {
				v->lineno = lineno;
				v->object = object;
				*lcss_cat = 0;
				*lcss_var = v;
				/* Put and reset comments */
				v->blanklines = 0;
				css_variable_append(*cat, v);
				/* add comments */
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
					v->precomments = ALLOC_COMMENT(comment_buffer);
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
					v->sameline = ALLOC_COMMENT(lline_buffer);
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
					CB_RESET(comment_buffer, lline_buffer);
				
			} else {
				return -1;
			}
		} else {
			css_log(LOG_WARNING, "No '=' (equal sign) in line %d of %s\n", lineno, configfile);
		}
	}
	return 0;
}

static struct css_config *config_text_file_load(const char *database, const char *table, const char *filename, struct css_config *cfg, struct css_flags flags, const char *suggested_include_file, const char *who_asked)
{
	char fn[256];
#if defined(LOW_MEMORY)
	char buf[512];
#else
	char buf[8192];
#endif
	char *new_buf, *comment_p, *process_buf;
	FILE *f;
	int lineno=0;
	int comment = 0, nest[MAX_NESTED_COMMENTS];
	struct css_category *cat = NULL;
	int count = 0;
	struct stat statbuf;
	struct cache_file_mtime *cfmtime = NULL;
	struct cache_file_include *cfinclude;
	struct css_variable *lcss_var = 0;
	struct css_category *lcss_cat = 0;
	/*! Growable string buffer */
	struct css_str *comment_buffer = NULL;	/*!< this will be a comment collector.*/
	struct css_str *lline_buffer = NULL;	/*!< A buffer for stuff behind the ; */

	if (cfg)
		cat = css_config_get_current_category(cfg);

	if (filename[0] == '/') {
		css_copy_string(fn, filename, sizeof(fn));
	} else {
		snprintf(fn, sizeof(fn), "%s/%s", css_config_CSS_CONFIG_DIR, filename);
	}

	if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS)) {
		comment_buffer = css_str_create(CB_SIZE);
		if (comment_buffer)
			lline_buffer = css_str_create(CB_SIZE);
		if (!lline_buffer) {
			css_free(comment_buffer);
			css_log(LOG_ERROR, "Failed to initialize the comment buffer!\n");
			return NULL;
		}
	}
#ifdef CSS_INCLUDE_GLOB
	{
		int glob_ret;
		glob_t globbuf;
		globbuf.gl_offs = 0;	/* initialize it to silence gcc */
		glob_ret = glob(fn, MY_GLOB_FLAGS, NULL, &globbuf);
		if (glob_ret == GLOB_NOSPACE)
			css_log(LOG_WARNING,
				"Glob Expansion of pattern '%s' failed: Not enough memory\n", fn);
		else if (glob_ret  == GLOB_ABORTED)
			css_log(LOG_WARNING,
				"Glob Expansion of pattern '%s' failed: Read error\n", fn);
		else  {
			/* loop over expanded files */
			int i;
			for (i=0; i<globbuf.gl_pathc; i++) {
				css_copy_string(fn, globbuf.gl_pathv[i], sizeof(fn));
#endif
	/*
	 * The following is not a loop, but just a convenient way to define a block
	 * (using do { } while(0) ), and be able to exit from it with 'continue'
	 * or 'break' in case of errors. Nice trick.
	 */
	do {
		if (stat(fn, &statbuf))
			continue;

		if (!S_ISREG(statbuf.st_mode)) {
			css_log(LOG_WARNING, "'%s' is not a regular file, ignoring\n", fn);
			continue;
		}

		if (!css_test_flag(&flags, CONFIG_FLAG_NOCACHE)) {
			/* Find our cached entry for this configuration file */
			CSS_LIST_LOCK(&cfmtime_head);
			CSS_LIST_TRAVERSE(&cfmtime_head, cfmtime, list) {
				if (!strcmp(cfmtime->filename, fn) && !strcmp(cfmtime->who_asked, who_asked))
					break;
			}
			if (!cfmtime) {
				cfmtime = cfmtime_new(fn, who_asked);
				if (!cfmtime)
					continue;
				/* Note that the file mtime is initialized to 0, i.e. 1970 */
				CSS_LIST_INSERT_SORTALPHA(&cfmtime_head, cfmtime, list, filename);
			}
		}

		if (cfmtime && (!cfmtime->has_exec) && (cfmtime->mtime == statbuf.st_mtime) && css_test_flag(&flags, CONFIG_FLAG_FILEUNCHANGED)) {
			/* File is unchanged, what about the (cached) includes (if any)? */
			int unchanged = 1;
			CSS_LIST_TRAVERSE(&cfmtime->includes, cfinclude, list) {
				/* We must glob here, because if we did not, then adding a file to globbed directory would
				 * incorrectly cause no reload to be necessary. */
				char fn2[256];
#ifdef CSS_INCLUDE_GLOB
				int glob_return;
				glob_t glob_buf = { .gl_offs = 0 };
				glob_return = glob(cfinclude->include, MY_GLOB_FLAGS, NULL, &glob_buf);
				/* On error, we reparse */
				if (glob_return == GLOB_NOSPACE || glob_return  == GLOB_ABORTED)
					unchanged = 0;
				else  {
					/* loop over expanded files */
					int j;
					for (j = 0; j < glob_buf.gl_pathc; j++) {
						css_copy_string(fn2, glob_buf.gl_pathv[j], sizeof(fn2));
#else
						css_copy_string(fn2, cfinclude->include);
#endif
						if (config_text_file_load(NULL, NULL, fn2, NULL, flags, "", who_asked) == NULL) {
							/* that second-to-lcss field needs to be looked at in this case... TODO */
							unchanged = 0;
							/* One change is enough to short-circuit and reload the whole shebang */
							break;
						}
#ifdef CSS_INCLUDE_GLOB
					}
				}
#endif
			}

			if (unchanged) {
				CSS_LIST_UNLOCK(&cfmtime_head);
				return CONFIG_STATUS_FILEUNCHANGED;
			}
		}
		if (!css_test_flag(&flags, CONFIG_FLAG_NOCACHE))
			CSS_LIST_UNLOCK(&cfmtime_head);

		/* If cfg is NULL, then we just want an answer */
		if (cfg == NULL) {
			css_free(comment_buffer);
			css_free(lline_buffer);
			return NULL;
		}

		if (cfmtime)
			cfmtime->mtime = statbuf.st_mtime;

		css_verb(2, "Parsing '%s': ", fn);
			fflush(stdout);
		if (!(f = fopen(fn, "r"))) {
			css_debug(1, "No file to parse: %s\n", fn);
			css_verb(2, "Not found (%s)\n", strerror(errno));
			continue;
		}
		count++;
		/* If we get to this point, then we're loading regardless */
		css_clear_flag(&flags, CONFIG_FLAG_FILEUNCHANGED);
		css_debug(1, "Parsing %s\n", fn);
		css_verb(2, "Found\n");
		while (!feof(f)) {
			lineno++;
			if (fgets(buf, sizeof(buf), f)) {
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && lline_buffer && css_str_strlen(lline_buffer)) {
					CB_ADD(&comment_buffer, css_str_buffer(lline_buffer));       /* add the current lline buffer to the comment buffer */
					css_str_reset(lline_buffer);        /* erase the lline buffer */
				}
				
				new_buf = buf;
				if (comment) 
					process_buf = NULL;
				else
					process_buf = buf;
				
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && comment_buffer && css_str_strlen(comment_buffer) && (css_strlen_zero(buf) || strlen(buf) == strspn(buf," \t\n\r"))) {
					/* blank line? really? Can we add it to an existing comment and maybe preserve inter- and post- comment spacing? */
					CB_ADD(&comment_buffer, "\n");       /* add a newline to the comment buffer */
					continue; /* go get a new line, then */
				}
				
				while ((comment_p = strchr(new_buf, COMMENT_META))) {
					if ((comment_p > new_buf) && (*(comment_p - 1) == '\\')) {
						/* Escaped semicolons aren't comments. */
						new_buf = comment_p + 1;
					} else if (comment_p[1] == COMMENT_TAG && comment_p[2] == COMMENT_TAG && (comment_p[3] != '-')) {
						/* Meta-Comment start detected ";--" */
						if (comment < MAX_NESTED_COMMENTS) {
							*comment_p = '\0';
							new_buf = comment_p + 3;
							comment++;
							nest[comment-1] = lineno;
						} else {
							css_log(LOG_ERROR, "Maximum nest limit of %d reached.\n", MAX_NESTED_COMMENTS);
						}
					} else if ((comment_p >= new_buf + 2) &&
						   (*(comment_p - 1) == COMMENT_TAG) &&
						   (*(comment_p - 2) == COMMENT_TAG)) {
						/* Meta-Comment end detected */
						comment--;
						new_buf = comment_p + 1;
						if (!comment) {
							/* Back to non-comment now */
							if (process_buf) {
								/* Actually have to move what's left over the top, then continue */
								char *oldptr;
								oldptr = process_buf + strlen(process_buf);
								if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS)) {
									CB_ADD(&comment_buffer, ";");
									CB_ADD_LEN(&comment_buffer, oldptr+1, new_buf-oldptr-1);
								}
								
								memmove(oldptr, new_buf, strlen(new_buf) + 1);
								new_buf = oldptr;
							} else
								process_buf = new_buf;
						}
					} else {
						if (!comment) {
							/* If ; is found, and we are not nested in a comment, 
							   we immediately stop all comment processing */
							if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS)) {
								CB_ADD(&lline_buffer, comment_p);
							}
							*comment_p = '\0'; 
							new_buf = comment_p;
						} else
							new_buf = comment_p + 1;
					}
				}
				if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && comment && !process_buf ) {
					CB_ADD(&comment_buffer, buf);  /* the whole line is a comment, store it */
				}
				
				if (process_buf) {
					char *buffer = css_strip(process_buf);
					if (!css_strlen_zero(buffer)) {
						if (process_text_line(cfg, &cat, buffer, lineno, fn, flags, comment_buffer, lline_buffer, suggested_include_file, &lcss_cat, &lcss_var, who_asked)) {
							cfg = CONFIG_STATUS_FILEINVALID;
							break;
						}
					}
				}
			}
		}
		/* end of file-- anything in a comment buffer? */
		if (lcss_cat) {
			if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && comment_buffer && css_str_strlen(comment_buffer)) {
				if (lline_buffer && css_str_strlen(lline_buffer)) {
					CB_ADD(&comment_buffer, css_str_buffer(lline_buffer));       /* add the current lline buffer to the comment buffer */
					css_str_reset(lline_buffer);        /* erase the lline buffer */
				}
				lcss_cat->trailing = ALLOC_COMMENT(comment_buffer);
			}
		} else if (lcss_var) {
			if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && comment_buffer && css_str_strlen(comment_buffer)) {
				if (lline_buffer && css_str_strlen(lline_buffer)) {
					CB_ADD(&comment_buffer, css_str_buffer(lline_buffer));       /* add the current lline buffer to the comment buffer */
					css_str_reset(lline_buffer);        /* erase the lline buffer */
				}
				lcss_var->trailing = ALLOC_COMMENT(comment_buffer);
			}
		} else {
			if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS) && comment_buffer && css_str_strlen(comment_buffer)) {
				css_debug(1, "Nothing to attach comments to, discarded: %s\n", css_str_buffer(comment_buffer));
			}
		}
		if (css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS))
			CB_RESET(comment_buffer, lline_buffer);

		fclose(f);
	} while (0);
	if (comment) {
		css_log(LOG_WARNING,"Unterminated comment detected beginning on line %d\n", nest[comment - 1]);
	}
#ifdef CSS_INCLUDE_GLOB
					if (cfg == NULL || cfg == CONFIG_STATUS_FILEUNCHANGED || cfg == CONFIG_STATUS_FILEINVALID) {
						break;
					}
				}
				globfree(&globbuf);
			}
		}
#endif

	if (cfg && cfg != CONFIG_STATUS_FILEUNCHANGED && cfg != CONFIG_STATUS_FILEINVALID && cfg->include_level == 1 && css_test_flag(&flags, CONFIG_FLAG_WITHCOMMENTS)) {
		css_free(comment_buffer);
		css_free(lline_buffer);
		comment_buffer = NULL;
		lline_buffer = NULL;
	}
	
	if (count == 0)
		return NULL;

	return cfg;
}


/* NOTE: categories and variables each have a file and lineno attribute. On a save operation, these are used to determine
   which file and line number to write out to. Thus, an entire hierarchy of config files (via #include statements) can be
   recreated. BUT, care must be taken to make sure that every cat and var has the proper file name stored, or you may
   be shocked and mystified as to why things are not showing up in the files! 

   Also, All #include/#exec statements are recorded in the "includes" LL in the css_config structure. The file name
   and line number are stored for each include, plus the name of the file included, so that these statements may be
   included in the output files on a file_save operation. 

   The lineno's are really just for relative placement in the file. There is no attempt to make sure that blank lines
   are included to keep the lineno's the same between input and output. The lineno fields are used mainly to determine
   the position of the #include and #exec directives. So, blank lines tend to disappear from a read/rewrite operation,
   and a header gets added.

   vars and category heads are output in the order they are stored in the config file. So, if the software
   shuffles these at all, then the placement of #include directives might get a little mixed up, because the
   file/lineno data probably won't get changed.

*/

static void gen_header(FILE *f1, const char *configfile, const char *fn, const char *generator)
{
	char date[256]="";
	time_t t;

	time(&t);
	css_copy_string(date, ctime(&t), sizeof(date));

	fprintf(f1, ";!\n");
	fprintf(f1, ";! Automatically generated configuration file\n");
	if (strcmp(configfile, fn))
		fprintf(f1, ";! Filename: %s (%s)\n", configfile, fn);
	else
		fprintf(f1, ";! Filename: %s\n", configfile);
	fprintf(f1, ";! Generator: %s\n", generator);
	fprintf(f1, ";! Creation Date: %s", date);
	fprintf(f1, ";!\n");
}

static void inclfile_destroy(void *obj)
{
	const struct inclfile *o = obj;

	css_free(o->fname);
}


static struct inclfile *set_fn(char *fn, int fn_size, const char *file, const char *configfile, struct ao2_container *fileset)
{
	struct inclfile lookup;
	struct inclfile *fi;

	if (css_strlen_zero(file)) {
		if (configfile[0] == '/')
			css_copy_string(fn, configfile, fn_size);
		else
			snprintf(fn, fn_size, "%s/%s", css_config_CSS_CONFIG_DIR, configfile);
	} else if (file[0] == '/')
		css_copy_string(fn, file, fn_size);
	else
		snprintf(fn, fn_size, "%s/%s", css_config_CSS_CONFIG_DIR, file);
	lookup.fname = fn;
	fi = ao2_find(fileset, &lookup, OBJ_POINTER);
	if (fi) {
		/* Found existing include file scratch pad. */
		return fi;
	}

	/* set up a file scratch pad */
	fi = ao2_alloc(sizeof(struct inclfile), inclfile_destroy);
	if (!fi) {
		/* Scratch pad creation failed. */
		return NULL;
	}
	fi->fname = css_strdup(fn);
	if (!fi->fname) {
		/* Scratch pad creation failed. */
		ao2_ref(fi, -1);
		return NULL;
	}
	fi->lineno = 1;

	ao2_link(fileset, fi);

	return fi;
}

static int count_linefeeds(char *str)
{
	int count = 0;

	while (*str) {
		if (*str =='\n')
			count++;
		str++;
	}
	return count;
}

static int count_linefeeds_in_comments(struct css_comment *x)
{
	int count = 0;

	while (x) {
		count += count_linefeeds(x->cmt);
		x = x->next;
	}
	return count;
}

static void insert_leading_blank_lines(FILE *fp, struct inclfile *fi, struct css_comment *precomments, int lineno)
{
	int precomment_lines;
	int i;

	if (!fi) {
		/* No file scratch pad object so insert no blank lines. */
		return;
	}

	precomment_lines = count_linefeeds_in_comments(precomments);

	/* I don't have to worry about those ;! comments, they are
	   stored in the precomments, but not printed back out.
	   I did have to make sure that comments following
	   the ;! header comments were not also deleted in the process */
	if (lineno - precomment_lines - fi->lineno < 0) { /* insertions can mess up the line numbering and produce negative numbers that mess things up */
		return;
	} else if (lineno == 0) {
		/* Line replacements also mess things up */
		return;
 	} else if (lineno - precomment_lines - fi->lineno < 5) {
 		/* Only insert less than 5 blank lines; if anything more occurs,
 		 * it's probably due to context deletion. */
 		for (i = fi->lineno; i < lineno - precomment_lines; i++) {
 			fprintf(fp, "\n");
 		}
 	} else {
 		/* Deletion occurred - insert a single blank line, for separation of
 		 * contexts. */
 		fprintf(fp, "\n");
 	}
 
 	fi->lineno = lineno + 1; /* Advance the file lineno */
}

int config_text_file_save(const char *configfile, const struct css_config *cfg, const char *generator)
{
	return css_config_text_file_save(configfile, cfg, generator);
}

int css_config_text_file_save(const char *configfile, const struct css_config *cfg, const char *generator)
{
	FILE *f;
	char fn[PATH_MAX];
	struct css_variable *var;
	struct css_category *cat;
	struct css_comment *cmt;
	struct css_config_include *incl;
	int blanklines = 0;
	struct ao2_container *fileset;
	struct inclfile *fi;

	fileset = ao2_container_alloc(1023, hash_string, hashtab_compare_strings);
	if (!fileset) {
		/* Container creation failed. */
		return -1;
	}

	/* reset all the output flags, in case this isn't our first time saving this data */
	for (incl = cfg->includes; incl; incl = incl->next) {
		incl->output = 0;
	}

	/* go thru all the inclusions and make sure all the files involved (configfile plus all its inclusions)
	   are all truncated to zero bytes and have that nice header*/
	for (incl = cfg->includes; incl; incl = incl->next) {
		if (!incl->exec) { /* leave the execs alone -- we'll write out the #exec directives, but won't zero out the include files or exec files*/
			/* normally, fn is just set to incl->included_file, prepended with config dir if relative */
			fi = set_fn(fn, sizeof(fn), incl->included_file, configfile, fileset);
			f = fopen(fn, "w");
			if (f) {
				gen_header(f, configfile, fn, generator);
				fclose(f); /* this should zero out the file */
			} else {
				css_debug(1, "Unable to open for writing: %s\n", fn);
				css_verb(2, "Unable to write %s (%s)", fn, strerror(errno));
			}
			if (fi) {
				ao2_ref(fi, -1);
			}
		}
	}

	/* just set fn to absolute ver of configfile */
	fi = set_fn(fn, sizeof(fn), 0, configfile, fileset);
	if (
#ifdef __CYGWIN__
		(f = fopen(fn, "w+"))
#else
		(f = fopen(fn, "w"))
#endif
		) {
		css_verb(2, "Saving '%s': ", fn);
		gen_header(f, configfile, fn, generator);
		cat = cfg->root;
		fclose(f);
		if (fi) {
			ao2_ref(fi, -1);
		}

		/* from here out, we open each involved file and concat the stuff we need to add to the end and immediately close... */
		/* since each var, cat, and associated comments can come from any file, we have to be
		   mobile, and open each file, print, and close it on an entry-by-entry basis */

		while (cat) {
			fi = set_fn(fn, sizeof(fn), cat->file, configfile, fileset);
			f = fopen(fn, "a");
			if (!f) {
				css_debug(1, "Unable to open for writing: %s\n", fn);
				css_verb(2, "Unable to write %s (%s)", fn, strerror(errno));
				if (fi) {
					ao2_ref(fi, -1);
				}
				ao2_ref(fileset, -1);
				return -1;
			}

			/* dump any includes that happen before this category header */
			for (incl=cfg->includes; incl; incl = incl->next) {
				if (strcmp(incl->include_location_file, cat->file) == 0){
					if (cat->lineno > incl->include_location_lineno && !incl->output) {
						if (incl->exec)
							fprintf(f,"#exec \"%s\"\n", incl->exec_file);
						else
							fprintf(f,"#include \"%s\"\n", incl->included_file);
						incl->output = 1;
					}
				}
			}

			insert_leading_blank_lines(f, fi, cat->precomments, cat->lineno);
			/* Dump section with any appropriate comment */
			for (cmt = cat->precomments; cmt; cmt=cmt->next) {
				char *cmtp = cmt->cmt;
				while (*cmtp == ';' && *(cmtp+1) == '!') {
					char *cmtp2 = strchr(cmtp+1, '\n');
					if (cmtp2)
						cmtp = cmtp2+1;
					else cmtp = 0;
				}
				if (cmtp)
					fprintf(f,"%s", cmtp);
			}
			fprintf(f, "[%s]", cat->name);
			if (cat->ignored || !CSS_LIST_EMPTY(&cat->template_instances)) {
				fprintf(f, "(");
				if (cat->ignored) {
					fprintf(f, "!");
				}
				if (cat->ignored && !CSS_LIST_EMPTY(&cat->template_instances)) {
					fprintf(f, ",");
				}
				if (!CSS_LIST_EMPTY(&cat->template_instances)) {
					struct css_category_template_instance *x;
					CSS_LIST_TRAVERSE(&cat->template_instances, x, next) {
						fprintf(f,"%s",x->name);
						if (x != CSS_LIST_LCSS(&cat->template_instances))
							fprintf(f,",");
					}
				}
				fprintf(f, ")");
			}
			for(cmt = cat->sameline; cmt; cmt=cmt->next)
			{
				fprintf(f,"%s", cmt->cmt);
			}
			if (!cat->sameline)
				fprintf(f,"\n");
			for (cmt = cat->trailing; cmt; cmt=cmt->next) {
				if (cmt->cmt[0] != ';' || cmt->cmt[1] != '!')
					fprintf(f,"%s", cmt->cmt);
			}
			fclose(f);
			if (fi) {
				ao2_ref(fi, -1);
			}

			var = cat->root;
			while (var) {
				struct css_category_template_instance *x;
				int found = 0;
				CSS_LIST_TRAVERSE(&cat->template_instances, x, next) {
					struct css_variable *v;
					for (v = x->inst->root; v; v = v->next) {
						if (!strcasecmp(var->name, v->name) && !strcmp(var->value, v->value)) {
							found = 1;
							break;
						}
					}
					if (found)
						break;
				}
				if (found) {
					var = var->next;
					continue;
				}
				fi = set_fn(fn, sizeof(fn), var->file, configfile, fileset);
				f = fopen(fn, "a");
				if (!f) {
					css_debug(1, "Unable to open for writing: %s\n", fn);
					css_verb(2, "Unable to write %s (%s)", fn, strerror(errno));
					if (fi) {
						ao2_ref(fi, -1);
					}
					ao2_ref(fileset, -1);
					return -1;
				}

				/* dump any includes that happen before this category header */
				for (incl=cfg->includes; incl; incl = incl->next) {
					if (strcmp(incl->include_location_file, var->file) == 0){
						if (var->lineno > incl->include_location_lineno && !incl->output) {
							if (incl->exec)
								fprintf(f,"#exec \"%s\"\n", incl->exec_file);
							else
								fprintf(f,"#include \"%s\"\n", incl->included_file);
							incl->output = 1;
						}
					}
				}

				insert_leading_blank_lines(f, fi, var->precomments, var->lineno);
				for (cmt = var->precomments; cmt; cmt=cmt->next) {
					if (cmt->cmt[0] != ';' || cmt->cmt[1] != '!')
						fprintf(f,"%s", cmt->cmt);
				}
				if (var->sameline)
					fprintf(f, "%s %s %s  %s", var->name, (var->object ? "=>" : "="), var->value, var->sameline->cmt);
				else
					fprintf(f, "%s %s %s\n", var->name, (var->object ? "=>" : "="), var->value);
				for (cmt = var->trailing; cmt; cmt=cmt->next) {
					if (cmt->cmt[0] != ';' || cmt->cmt[1] != '!')
						fprintf(f,"%s", cmt->cmt);
				}
				if (var->blanklines) {
					blanklines = var->blanklines;
					while (blanklines--)
						fprintf(f, "\n");
				}

				fclose(f);
				if (fi) {
					ao2_ref(fi, -1);
				}

				var = var->next;
			}
			cat = cat->next;
		}
		if (!option_debug)
			css_verb(2, "Saved\n");
	} else {
		css_debug(1, "Unable to open for writing: %s\n", fn);
		css_verb(2, "Unable to write (%s)", strerror(errno));
		if (fi) {
			ao2_ref(fi, -1);
		}
		ao2_ref(fileset, -1);
		return -1;
	}

	/* Now, for files with trailing #include/#exec statements,
	   we have to make sure every entry is output */
	for (incl=cfg->includes; incl; incl = incl->next) {
		if (!incl->output) {
			/* open the respective file */
			fi = set_fn(fn, sizeof(fn), incl->include_location_file, configfile, fileset);
			f = fopen(fn, "a");
			if (!f) {
				css_debug(1, "Unable to open for writing: %s\n", fn);
				css_verb(2, "Unable to write %s (%s)", fn, strerror(errno));
				if (fi) {
					ao2_ref(fi, -1);
				}
				ao2_ref(fileset, -1);
				return -1;
			}

			/* output the respective include */
			if (incl->exec)
				fprintf(f,"#exec \"%s\"\n", incl->exec_file);
			else
				fprintf(f,"#include \"%s\"\n", incl->included_file);
			fclose(f);
			incl->output = 1;
			if (fi) {
				ao2_ref(fi, -1);
			}
		}
	}
	ao2_ref(fileset, -1); /* this should destroy the hash container */

	return 0;
}

static void clear_config_maps(void) 
{
	struct css_config_map *map;

	css_mutex_lock(&config_lock);

	while (config_maps) {
		map = config_maps;
		config_maps = config_maps->next;
		css_free(map);
	}
		
	css_mutex_unlock(&config_lock);
}

static int append_mapping(const char *name, const char *driver, const char *database, const char *table, int priority)
{
	struct css_config_map *map;
	char *dst;
	int length;

	length = sizeof(*map);
	length += strlen(name) + 1;
	length += strlen(driver) + 1;
	length += strlen(database) + 1;
	if (table)
		length += strlen(table) + 1;

	if (!(map = css_calloc(1, length)))
		return -1;

	dst = map->stuff;	/* writable space starts here */
	map->name = strcpy(dst, name);
	dst += strlen(dst) + 1;
	map->driver = strcpy(dst, driver);
	dst += strlen(dst) + 1;
	map->database = strcpy(dst, database);
	if (table) {
		dst += strlen(dst) + 1;
		map->table = strcpy(dst, table);
	}
	map->priority = priority;
	map->next = config_maps;
	config_maps = map;

	css_verb(2, "Binding %s to %s/%s/%s\n", map->name, map->driver, map->database, map->table ? map->table : map->name);

	return 0;
}

int read_config_maps(void) 
{
	struct css_config *config, *configtmp;
	struct css_variable *v;
	char *driver, *table, *database, *textpri, *stringp, *tmp;
	struct css_flags flags = { CONFIG_FLAG_NOREALTIME };
	int pri;

	clear_config_maps();

	configtmp = css_config_new();
	configtmp->max_include_level = 1;
	config = css_config_internal_load(extconfig_conf, configtmp, flags, "", "extconfig");
	if (config == CONFIG_STATUS_FILEINVALID) {
		return -1;
	} else if (!config) {
		css_config_destroy(configtmp);
		return 0;
	}

	for (v = css_variable_browse(config, "settings"); v; v = v->next) {
		char buf[512];
		css_copy_string(buf, v->value, sizeof(buf));
		stringp = buf;
		driver = strsep(&stringp, ",");

		if ((tmp = strchr(stringp, '\"')))
			stringp = tmp;

		/* check if the database text starts with a double quote */
		if (*stringp == '"') {
			stringp++;
			database = strsep(&stringp, "\"");
			strsep(&stringp, ",");
		} else {
			/* apparently this text has no quotes */
			database = strsep(&stringp, ",");
		}

		table = strsep(&stringp, ",");
		textpri = strsep(&stringp, ",");
		if (!textpri || !(pri = atoi(textpri))) {
			pri = 1;
		}

		if (!strcmp(v->name, extconfig_conf)) {
			css_log(LOG_WARNING, "Cannot bind '%s'!\n", extconfig_conf);
			continue;
		}

		if (!strcmp(v->name, "ceictims.conf")) {
			css_log(LOG_WARNING, "Cannot bind 'ceictims.conf'!\n");
			continue;
		}

		if (!strcmp(v->name, "logger.conf")) {
			css_log(LOG_WARNING, "Cannot bind 'logger.conf'!\n");
			continue;
		}

		if (!driver || !database)
			continue;
		if (!strcasecmp(v->name, "sipfriends")) {
			css_log(LOG_WARNING, "The 'sipfriends' table is obsolete, update your config to use sipusers and sippeers, though they can point to the same table.\n");
			append_mapping("sipusers", driver, database, table ? table : "sipfriends", pri);
			append_mapping("sippeers", driver, database, table ? table : "sipfriends", pri);
		} else if (!strcasecmp(v->name, "iaxfriends")) {
			css_log(LOG_WARNING, "The 'iaxfriends' table is obsolete, update your config to use iaxusers and iaxpeers, though they can point to the same table.\n");
			append_mapping("iaxusers", driver, database, table ? table : "iaxfriends", pri);
			append_mapping("iaxpeers", driver, database, table ? table : "iaxfriends", pri);
		} else 
			append_mapping(v->name, driver, database, table, pri);
	}
		
	css_config_destroy(config);
	return 0;
}

int css_config_engine_register(struct css_config_engine *new) 
{
	struct css_config_engine *ptr;

	css_mutex_lock(&config_lock);

	if (!config_engine_list) {
		config_engine_list = new;
	} else {
		for (ptr = config_engine_list; ptr->next; ptr=ptr->next);
		ptr->next = new;
	}

	css_mutex_unlock(&config_lock);
	css_log(LOG_NOTICE,"Registered Config Engine %s\n", new->name);

	return 1;
}

int css_config_engine_deregister(struct css_config_engine *del) 
{
	struct css_config_engine *ptr, *lcss=NULL;

	css_mutex_lock(&config_lock);

	for (ptr = config_engine_list; ptr; ptr=ptr->next) {
		if (ptr == del) {
			if (lcss)
				lcss->next = ptr->next;
			else
				config_engine_list = ptr->next;
			break;
		}
		lcss = ptr;
	}

	css_mutex_unlock(&config_lock);

	return 0;
}

/*! \brief Find realtime engine for realtime family */
static struct css_config_engine *find_engine(const char *family, int priority, char *database, int dbsiz, char *table, int tabsiz) 
{
	struct css_config_engine *eng, *ret = NULL;
	struct css_config_map *map;

	css_mutex_lock(&config_lock);

	for (map = config_maps; map; map = map->next) {
		if (!strcasecmp(family, map->name) && (priority == map->priority)) {
			if (database)
				css_copy_string(database, map->database, dbsiz);
			if (table)
				css_copy_string(table, map->table ? map->table : family, tabsiz);
			break;
		}
	}

	/* Check if the required driver (engine) exist */
	if (map) {
		for (eng = config_engine_list; !ret && eng; eng = eng->next) {
			if (!strcasecmp(eng->name, map->driver))
				ret = eng;
		}
	}

	css_mutex_unlock(&config_lock);
	
	/* if we found a mapping, but the engine is not available, then issue a warning */
	if (map && !ret)
		css_log(LOG_WARNING, "Realtime mapping for '%s' found to engine '%s', but the engine is not available\n", map->name, map->driver);

	return ret;
}

static struct css_config_engine text_file_engine = {
	.name = "text",
	.load_func = config_text_file_load,
};

struct css_config *css_config_internal_load(const char *filename, struct css_config *cfg, struct css_flags flags, const char *suggested_include_file, const char *who_asked)
{
	char db[256];
	char table[256];
	struct css_config_engine *loader = &text_file_engine;
	struct css_config *result; 

	/* The config file itself bumps include_level by 1 */
	if (cfg->max_include_level > 0 && cfg->include_level == cfg->max_include_level + 1) {
		css_log(LOG_WARNING, "Maximum Include level (%d) exceeded\n", cfg->max_include_level);
		return NULL;
	}

	cfg->include_level++;

	if (!css_test_flag(&flags, CONFIG_FLAG_NOREALTIME) && config_engine_list) {
		struct css_config_engine *eng;

		eng = find_engine(filename, 1, db, sizeof(db), table, sizeof(table));


		if (eng && eng->load_func) {
			loader = eng;
		} else {
			eng = find_engine("global", 1, db, sizeof(db), table, sizeof(table));
			if (eng && eng->load_func)
				loader = eng;
		}
	}

	result = loader->load_func(db, table, filename, cfg, flags, suggested_include_file, who_asked);

	if (result && result != CONFIG_STATUS_FILEINVALID && result != CONFIG_STATUS_FILEUNCHANGED)
		result->include_level--;
	else if (result != CONFIG_STATUS_FILEINVALID)
		cfg->include_level--;

	return result;
}

struct css_config *css_config_load2(const char *filename, const char *who_asked, struct css_flags flags)
{
	struct css_config *cfg;
	struct css_config *result;

	cfg = css_config_new();
	if (!cfg)
		return NULL;

	result = css_config_internal_load(filename, cfg, flags, "", who_asked);
	if (!result || result == CONFIG_STATUS_FILEUNCHANGED || result == CONFIG_STATUS_FILEINVALID)
		css_config_destroy(cfg);

	return result;
}

static struct css_variable *css_load_realtime_helper(const char *family, va_list ap)
{
	struct css_config_engine *eng;
	char db[256];
	char table[256];
	struct css_variable *res=NULL;
	int i;

	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			if (eng->realtime_func && (res = eng->realtime_func(db, table, ap))) {
				return res;
			}
		} else {
			return NULL;
		}
	}

	return res;
}

struct css_variable *css_load_realtime_all(const char *family, ...)
{
	struct css_variable *res;
	va_list ap;

	va_start(ap, family);
	res = css_load_realtime_helper(family, ap);
	va_end(ap);

	return res;
}

struct css_variable *css_load_realtime(const char *family, ...)
{
	struct css_variable *res;
	struct css_variable *cur;
	struct css_variable **prev;
	va_list ap;

	va_start(ap, family);
	res = css_load_realtime_helper(family, ap);
	va_end(ap);

	/* Filter the list. */
	prev = &res;
	cur = res;
	while (cur) {
		if (css_strlen_zero(cur->value)) {
			/* Eliminate empty entries */
			struct css_variable *next;

			next = cur->next;
			*prev = next;
			css_variable_destroy(cur);
			cur = next;
		} else {
			/* Make blank entries empty and keep them. */
			if (cur->value[0] == ' ' && cur->value[1] == '\0') {
				char *vptr = (char *) cur->value;

				vptr[0] = '\0';
			}

			prev = &cur->next;
			cur = cur->next;
		}
	}
	return res;
}

/*! \brief Check if realtime engine is configured for family */
int css_check_realtime(const char *family)
{
	struct css_config_engine *eng;
	if (!css_realtime_enabled()) {
		return 0;	/* There are no engines at all so fail early */
	}

	eng = find_engine(family, 1, NULL, 0, NULL, 0);
	if (eng)
		return 1;
	return 0;
}

/*! \brief Check if there's any realtime engines loaded */
int css_realtime_enabled(void)
{
	return config_maps ? 1 : 0;
}

int css_realtime_require_field(const char *family, ...)
{
	struct css_config_engine *eng;
	char db[256];
	char table[256];
	va_list ap;
	int res = -1, i;

	va_start(ap, family);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			/* If the require succeeds, it returns 0. */
			if (eng->require_func && !(res = eng->require_func(db, table, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

int css_unload_realtime(const char *family)
{
	struct css_config_engine *eng;
	char db[256];
	char table[256];
	int res = -1, i;

	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			if (eng->unload_func) {
				/* Do this for ALL engines */
				res = eng->unload_func(db, table);
			}
		} else {
			break;
		}
	}
	return res;
}

struct css_config *css_load_realtime_multientry(const char *family, ...)
{
	struct css_config_engine *eng;
	char db[256];
	char table[256];
	struct css_config *res = NULL;
	va_list ap;
	int i;

	va_start(ap, family);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			if (eng->realtime_multi_func && (res = eng->realtime_multi_func(db, table, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

int css_update_realtime(const char *family, const char *keyfield, const char *lookup, ...)
{
	struct css_config_engine *eng;
	int res = -1, i;
	char db[256];
	char table[256];
	va_list ap;

	va_start(ap, lookup);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			/* If the update succeeds, it returns 0. */
			if (eng->update_func && !(res = eng->update_func(db, table, keyfield, lookup, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

int css_update2_realtime(const char *family, ...)
{
	struct css_config_engine *eng;
	int res = -1, i;
	char db[256];
	char table[256];
	va_list ap;

	va_start(ap, family);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			if (eng->update2_func && !(res = eng->update2_func(db, table, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

int css_store_realtime(const char *family, ...)
{
	struct css_config_engine *eng;
	int res = -1, i;
	char db[256];
	char table[256];
	va_list ap;

	va_start(ap, family);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			/* If the store succeeds, it returns 0. */
			if (eng->store_func && !(res = eng->store_func(db, table, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

int css_destroy_realtime(const char *family, const char *keyfield, const char *lookup, ...)
{
	struct css_config_engine *eng;
	int res = -1, i;
	char db[256];
	char table[256];
	va_list ap;

	va_start(ap, lookup);
	for (i = 1; ; i++) {
		if ((eng = find_engine(family, i, db, sizeof(db), table, sizeof(table)))) {
			if (eng->destroy_func && !(res = eng->destroy_func(db, table, keyfield, lookup, ap))) {
				break;
			}
		} else {
			break;
		}
	}
	va_end(ap);

	return res;
}

char *css_realtime_decode_chunk(char *chunk)
{
	char *orig = chunk;
	for (; *chunk; chunk++) {
		if (*chunk == '^' && strchr("0123456789ABCDEFabcdef", chunk[1]) && strchr("0123456789ABCDEFabcdef", chunk[2])) {
			sscanf(chunk + 1, "%02hhX", chunk);
			memmove(chunk + 1, chunk + 3, strlen(chunk + 3) + 1);
		}
	}
	return orig;
}

char *css_realtime_encode_chunk(struct css_str **dest, ssize_t maxlen, const char *chunk)
{
	if (!strchr(chunk, ';') && !strchr(chunk, '^')) {
		css_str_set(dest, maxlen, "%s", chunk);
	} else {
		css_str_reset(*dest);
		for (; *chunk; chunk++) {
			if (strchr(";^", *chunk)) {
				css_str_append(dest, maxlen, "^%02hhX", *chunk);
			} else {
				css_str_append(dest, maxlen, "%c", *chunk);
			}
		}
	}
	return css_str_buffer(*dest);
}

/*! \brief Helper function to parse arguments
 * See documentation in config.h
 */
int css_parse_arg(const char *arg, enum css_parse_flags flags,
	void *p_result, ...)
{
	va_list ap;
	int error = 0;

	va_start(ap, p_result);
	switch (flags & PARSE_TYPE) {
	case PARSE_INT32:
	    {
		int32_t *result = p_result;
		int32_t x, def = result ? *result : 0,
			high = (int32_t)0x7fffffff,
			low  = (int32_t)0x80000000;
		/* optional argument: first default value, then range */
		if (flags & PARSE_DEFAULT)
			def = va_arg(ap, int32_t);
		if (flags & (PARSE_IN_RANGE|PARSE_OUT_RANGE)) {
			/* range requested, update bounds */
			low = va_arg(ap, int32_t);
			high = va_arg(ap, int32_t);
		}
		x = strtol(arg, NULL, 0);
		error = (x < low) || (x > high);
		if (flags & PARSE_OUT_RANGE)
			error = !error;
		if (result)
			*result  = error ? def : x;
		css_debug(3,
			"extract int from [%s] in [%d, %d] gives [%d](%d)\n",
			arg, low, high,
			result ? *result : x, error);
		break;
	    }

	case PARSE_UINT32:
	    {
		uint32_t *result = p_result;
		uint32_t x, def = result ? *result : 0,
			low = 0, high = (uint32_t)~0;
		/* optional argument: first default value, then range */
		if (flags & PARSE_DEFAULT)
			def = va_arg(ap, uint32_t);
		if (flags & (PARSE_IN_RANGE|PARSE_OUT_RANGE)) {
			/* range requested, update bounds */
			low = va_arg(ap, uint32_t);
			high = va_arg(ap, uint32_t);
		}
		x = strtoul(arg, NULL, 0);
		error = (x < low) || (x > high);
		if (flags & PARSE_OUT_RANGE)
			error = !error;
		if (result)
			*result  = error ? def : x;
		css_debug(3,
			"extract uint from [%s] in [%u, %u] gives [%u](%d)\n",
			arg, low, high,
			result ? *result : x, error);
		break;
	    }

	case PARSE_DOUBLE:
	    {
		double *result = p_result;
		double x, def = result ? *result : 0,
			low = -HUGE_VAL, high = HUGE_VAL;

		/* optional argument: first default value, then range */
		if (flags & PARSE_DEFAULT)
			def = va_arg(ap, double);
		if (flags & (PARSE_IN_RANGE|PARSE_OUT_RANGE)) {
			/* range requested, update bounds */
			low = va_arg(ap, double);
			high = va_arg(ap, double);
		}
		x = strtod(arg, NULL);
		error = (x < low) || (x > high);
		if (flags & PARSE_OUT_RANGE)
			error = !error;
		if (result)
			*result  = error ? def : x;
		css_debug(3,
			"extract double from [%s] in [%f, %f] gives [%f](%d)\n",
			arg, low, high,
			result ? *result : x, error);
		break;
	    }
	case PARSE_ADDR:
	    {
		struct css_sockaddr *addr = (struct css_sockaddr *)p_result;

		if (!css_sockaddr_parse(addr, arg, flags & PARSE_PORT_MASK)) {
			error = 1;
		}

		css_debug(3, "extract addr from %s gives %s(%d)\n",
			  arg, css_sockaddr_stringify(addr), error);

		break;
	    }
	case PARSE_INADDR:	/* TODO Remove this (use PARSE_ADDR instead). */
	    {
		char *port, *buf;
		struct sockaddr_in _sa_buf;	/* buffer for the result */
		struct sockaddr_in *sa = p_result ?
			(struct sockaddr_in *)p_result : &_sa_buf;
		/* default is either the supplied value or the result itself */
		struct sockaddr_in *def = (flags & PARSE_DEFAULT) ?
			va_arg(ap, struct sockaddr_in *) : sa;
		struct hostent *hp;
		struct css_hostent ahp;

		memset(&_sa_buf, '\0', sizeof(_sa_buf)); /* clear buffer */
		/* duplicate the string to strip away the :port */
		port = css_strdupa(arg);
		buf = strsep(&port, ":");
		sa->sin_family = AF_INET;	/* assign family */
		/*
		 * honor the ports flag setting, assign default value
		 * in case of errors or field unset.
		 */
		flags &= PARSE_PORT_MASK; /* the only flags left to process */
		if (port) {
			if (flags == PARSE_PORT_FORBID) {
				error = 1;	/* port was forbidden */
				sa->sin_port = def->sin_port;
			} else if (flags == PARSE_PORT_IGNORE)
				sa->sin_port = def->sin_port;
			else /* accept or require */
				sa->sin_port = htons(strtol(port, NULL, 0));
		} else {
			sa->sin_port = def->sin_port;
			if (flags == PARSE_PORT_REQUIRE)
				error = 1;
		}
		/* Now deal with host part, even if we have errors before. */
		hp = css_gethostbyname(buf, &ahp);
		if (hp)	/* resolved successfully */
			memcpy(&sa->sin_addr, hp->h_addr, sizeof(sa->sin_addr));
		else {
			error = 1;
			sa->sin_addr = def->sin_addr;
		}
		css_debug(3,
			"extract inaddr from [%s] gives [%s:%d](%d)\n",
			arg, css_inet_ntoa(sa->sin_addr),
			ntohs(sa->sin_port), error);
	    	break;
	    }
	}
	va_end(ap);
	return error;
}

static char *handle_cli_core_show_config_mappings(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct css_config_engine *eng;
	struct css_config_map *map;

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show config mappings";
		e->usage =
			"Usage: core show config mappings\n"
			"	Shows the filenames to config engines.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	
	css_mutex_lock(&config_lock);

	if (!config_engine_list) {
		css_cli(a->fd, "No config mappings found.\n");
	} else {
		for (eng = config_engine_list; eng; eng = eng->next) {
			css_cli(a->fd, "Config Engine: %s\n", eng->name);
			for (map = config_maps; map; map = map->next) {
				if (!strcasecmp(map->driver, eng->name)) {
					css_cli(a->fd, "===> %s (db=%s, table=%s)\n", map->name, map->database,
							map->table ? map->table : map->name);
				}
			}
		}
	}
	
	css_mutex_unlock(&config_lock);

	return CLI_SUCCESS;
}

static char *handle_cli_config_reload(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct cache_file_mtime *cfmtime;
	char *prev = "", *completion_value = NULL;
	int wordlen, which = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "config reload";
		e->usage =
			"Usage: config reload <filename.conf>\n"
			"   Reloads all modules that reference <filename.conf>\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos > 2) {
			return NULL;
		}

		wordlen = strlen(a->word);

		CSS_LIST_LOCK(&cfmtime_head);
		CSS_LIST_TRAVERSE(&cfmtime_head, cfmtime, list) {
			/* Skip duplicates - this only works because the list is sorted by filename */
			if (strcmp(cfmtime->filename, prev) == 0) {
				continue;
			}

			/* Core configs cannot be reloaded */
			if (css_strlen_zero(cfmtime->who_asked)) {
				continue;
			}

			if (++which > a->n && strncmp(cfmtime->filename, a->word, wordlen) == 0) {
				completion_value = css_strdup(cfmtime->filename);
				break;
			}

			/* Otherwise save that we've seen this filename */
			prev = cfmtime->filename;
		}
		CSS_LIST_UNLOCK(&cfmtime_head);

		return completion_value;
	}

	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	CSS_LIST_LOCK(&cfmtime_head);
	CSS_LIST_TRAVERSE(&cfmtime_head, cfmtime, list) {
		if (!strcmp(cfmtime->filename, a->argv[2])) {
			char *buf = alloca(strlen("module reload ") + strlen(cfmtime->who_asked) + 1);
			sprintf(buf, "module reload %s", cfmtime->who_asked);
			css_cli_command(a->fd, buf);
		}
	}
	CSS_LIST_UNLOCK(&cfmtime_head);

	return CLI_SUCCESS;
}

static char *handle_cli_config_list(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	struct cache_file_mtime *cfmtime;

	switch (cmd) {
	case CLI_INIT:
		e->command = "config list";
		e->usage =
			"Usage: config list\n"
			"   Show all modules that have loaded a configuration file\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	CSS_LIST_LOCK(&cfmtime_head);
	CSS_LIST_TRAVERSE(&cfmtime_head, cfmtime, list) {
		css_cli(a->fd, "%-20.20s %-50s\n", S_OR(cfmtime->who_asked, "core"), cfmtime->filename);
	}
	CSS_LIST_UNLOCK(&cfmtime_head);

	return CLI_SUCCESS;
}

static struct css_cli_entry cli_config[] = {
	CSS_CLI_DEFINE(handle_cli_core_show_config_mappings, "Display config mappings (file names to config engines)"),
	CSS_CLI_DEFINE(handle_cli_config_reload, "Force a reload on modules using a particular configuration file"),
	CSS_CLI_DEFINE(handle_cli_config_list, "Show all files that have loaded a configuration file"),
};

int register_config_cli(void)
{
	css_cli_register_multiple(cli_config, ARRAY_LEN(cli_config));
	return 0;
}
