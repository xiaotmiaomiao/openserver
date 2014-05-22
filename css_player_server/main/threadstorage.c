
/*! \file
 *
 * \brief Debugging support for thread-local-storage objects
 *
 * \author Kevin P. Fleming <kpfleming@digium.com>
 */

#include "cssplayer.h"
#include "private.h"

#if !defined(DEBUG_THREADLOCALS)

void threadstorage_init(void)
{
}

#else /* !defined(DEBUG_THREADLOCALS) */

#include "strings.h"
#include "utils.h"
#include "threadstorage.h"
#include "linkedlists.h"
#include "cli.h"

struct tls_object {
	void *key;
	size_t size;
	const char *file;
	const char *function;
	unsigned int line;
	pthread_t thread;
	CSS_LIST_ENTRY(tls_object) entry;
};

static CSS_LIST_HEAD_NOLOCK_STATIC(tls_objects, tls_object);

/* Allow direct use of pthread_mutex_t and friends */
#undef pthread_mutex_t
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_init
#undef pthread_mutex_destroy

/*!
 * \brief lock for the tls_objects list
 *
 * \note We can not use an css_mutex_t for this.  The reason is that this
 *       lock is used within the context of thread-local data destructors,
 *       and the css_mutex_* API uses thread-local data.  Allocating more
 *       thread-local data at that point just causes a memory leak.
 */
static pthread_mutex_t threadstoragelock;

void __css_threadstorage_object_add(void *key, size_t len, const char *file, const char *function, unsigned int line)
{
	struct tls_object *to;

	if (!(to = css_calloc(1, sizeof(*to))))
		return;

	to->key = key;
	to->size = len;
	to->file = file;
	to->function = function;
	to->line = line;
	to->thread = pthread_self();

	pthread_mutex_lock(&threadstoragelock);
	CSS_LIST_INSERT_TAIL(&tls_objects, to, entry);
	pthread_mutex_unlock(&threadstoragelock);
}

void __css_threadstorage_object_remove(void *key)
{
	struct tls_object *to;

	pthread_mutex_lock(&threadstoragelock);
	CSS_LIST_TRAVERSE_SAFE_BEGIN(&tls_objects, to, entry) {
		if (to->key == key) {
			CSS_LIST_REMOVE_CURRENT(entry);
			break;
		}
	}
	CSS_LIST_TRAVERSE_SAFE_END;
	pthread_mutex_unlock(&threadstoragelock);
	if (to)
		css_free(to);
}

void __css_threadstorage_object_replace(void *key_old, void *key_new, size_t len)
{
	struct tls_object *to;

	pthread_mutex_lock(&threadstoragelock);
	CSS_LIST_TRAVERSE_SAFE_BEGIN(&tls_objects, to, entry) {
		if (to->key == key_old) {
			to->key = key_new;
			to->size = len;
			break;
		}
	}
	CSS_LIST_TRAVERSE_SAFE_END;
	pthread_mutex_unlock(&threadstoragelock);
}

static char *handle_cli_threadstorage_show_allocations(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	const char *fn = NULL;
	size_t len = 0;
	unsigned int count = 0;
	struct tls_object *to;

	switch (cmd) {
	case CLI_INIT:
		e->command = "threadstorage show allocations";
		e->usage =
			"Usage: threadstorage show allocations [<file>]\n"
			"       Dumps a list of all thread-specific memory allocations,\n"
			"       optionally limited to those from a specific file\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc > 4)
		return CLI_SHOWUSAGE;

	if (a->argc > 3)
		fn = a->argv[3];

	pthread_mutex_lock(&threadstoragelock);

	CSS_LIST_TRAVERSE(&tls_objects, to, entry) {
		if (fn && strcasecmp(to->file, fn))
			continue;

		css_cli(a->fd, "%10d bytes allocated in %20s at line %5d of %25s (thread %p)\n",
			(int) to->size, to->function, to->line, to->file, (void *) to->thread);
		len += to->size;
		count++;
	}

	pthread_mutex_unlock(&threadstoragelock);

	css_cli(a->fd, "%10d bytes allocated in %d allocation%s\n", (int) len, count, count > 1 ? "s" : "");
	
	return CLI_SUCCESS;
}

static char *handle_cli_threadstorage_show_summary(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	const char *fn = NULL;
	size_t len = 0;
	unsigned int count = 0;
	struct tls_object *to;
	struct file {
		const char *name;
		size_t len;
		unsigned int count;
		CSS_LIST_ENTRY(file) entry;
	} *file;
	CSS_LIST_HEAD_NOLOCK_STATIC(file_summary, file);

	switch (cmd) {
	case CLI_INIT:
		e->command = "threadstorage show summary";
		e->usage =
			"Usage: threadstorage show summary [<file>]\n"
			"       Summarizes thread-specific memory allocations by file, or optionally\n"
			"       by function, if a file is specified\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc > 4)
		return CLI_SHOWUSAGE;

	if (a->argc > 3)
		fn = a->argv[3];

	pthread_mutex_lock(&threadstoragelock);

	CSS_LIST_TRAVERSE(&tls_objects, to, entry) {
		if (fn && strcasecmp(to->file, fn))
			continue;

		CSS_LIST_TRAVERSE(&file_summary, file, entry) {
			if ((!fn && (file->name == to->file)) || (fn && (file->name == to->function)))
				break;
		}

		if (!file) {
			file = alloca(sizeof(*file));
			memset(file, 0, sizeof(*file));
			file->name = fn ? to->function : to->file;
			CSS_LIST_INSERT_TAIL(&file_summary, file, entry);
		}

		file->len += to->size;
		file->count++;
	}

	pthread_mutex_unlock(&threadstoragelock);
	
	CSS_LIST_TRAVERSE(&file_summary, file, entry) {
		len += file->len;
		count += file->count;
		if (fn) {
			css_cli(a->fd, "%10d bytes in %d allocation%ss in function %s\n",
				(int) file->len, file->count, file->count > 1 ? "s" : "", file->name);
		} else {
			css_cli(a->fd, "%10d bytes in %d allocation%s in file %s\n",
				(int) file->len, file->count, file->count > 1 ? "s" : "", file->name);
		}
	}

	css_cli(a->fd, "%10d bytes allocated in %d allocation%s\n", (int) len, count, count > 1 ? "s" : "");

	return CLI_SUCCESS;
}

static struct css_cli_entry cli[] = {
	CSS_CLI_DEFINE(handle_cli_threadstorage_show_allocations, "Display outstanding thread local storage allocations"),
	CSS_CLI_DEFINE(handle_cli_threadstorage_show_summary,     "Summarize outstanding memory allocations")
};

void threadstorage_init(void)
{
	pthread_mutex_init(&threadstoragelock, NULL);
	css_cli_register_multiple(cli, ARRAY_LEN(cli));
}

#endif /* !defined(DEBUG_THREADLOCALS) */


