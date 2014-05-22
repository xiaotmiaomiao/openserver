/* 
 * File:   cli.h
 * Author: root
 *
 * Created on April 23, 2014, 2:33 AM
 */

#ifndef CLI_H
#define	CLI_H

#ifdef	__cplusplus
extern "C" {
#endif

/*! \file
 * \brief Standard Command Line Interface
 */

#include "linkedlists.h"
#include "strings.h"

void css_cli(int fd, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* dont check permissions while passing this option as a 'uid'
 * to the cli_has_permissions() function. */
#define CLI_NO_PERMS		-1

#define RESULT_SUCCESS		0
#define RESULT_SHOWUSAGE	1
#define RESULT_FAILURE		2

#define CLI_SUCCESS	(char *)RESULT_SUCCESS
#define CLI_SHOWUSAGE	(char *)RESULT_SHOWUSAGE
#define CLI_FAILURE	(char *)RESULT_FAILURE

#define CSS_MAX_CMD_LEN 	16

#define CSS_MAX_ARGS 64

#define CSS_CLI_COMPLETE_EOF	"_EOF_"

/*!
 * In many cases we need to print singular or plural
 * words depending on a count. This macro helps us e.g.
 *     printf("we have %d object%s", n, ESS(n));
 */
#define ESS(x) ((x) == 1 ? "" : "s")

/*! \brief return Yes or No depending on the argument.
 * This is used in many places in CLI command, having a function to generate
 * this helps maintaining a consistent output (and possibly emitting the
 * output in other languages, at some point).
 */
#define CSS_CLI_YESNO(x) (x) ? "Yes" : "No"

/*! \brief return On or Off depending on the argument.
 * This is used in many places in CLI command, having a function to generate
 * this helps maintaining a consistent output (and possibly emitting the
 * output in other languages, at some point).
 */
#define CSS_CLI_ONOFF(x) (x) ? "On" : "Off"

/*! \page CLI_command_API CLI command API

   CLI commands are described by a struct css_cli_entry that contains
   all the components for their implementation.

   In the "old-style" format, the record must contain:
   - a NULL-terminated array of words constituting the command, e.g.
	{ "set", "debug", "on", NULL },
   - a summary string (short) and a usage string (longer);
   - a handler which implements the command itself, invoked with
     a file descriptor and argc/argv as typed by the user
   - a 'generator' function which, given a partial string, can
     generate legal completions for it.
   An example is

	int old_setdebug(int fd, int argc, char *argv[]);
	char *dbg_complete(const char *line, const char *word, int pos, int n);

	{ { "set", "debug", "on", NULL }, do_setdebug, "Enable debugging",
	set_debug_usage, dbg_complete },

   In the "new-style" format, all the above functionalities are implemented
   by a single function, and the arguments tell which output is required.
   The prototype is the following:

	char *new_setdebug(const struct css_cli_entry *e, int cmd, struct css_cli_args *a);

	...
	// this is how we create the entry to register 
	CSS_CLI_DEFINE(new_setdebug, "short description")
	...

   To help the transition, we make the pointer to the struct css_cli_entry
   available to old-style handlers via argv[-1].

   An example of new-style handler is the following

\code
static char *test_new_cli(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	static const char * const choices[] = { "one", "two", "three", NULL };

        switch (cmd) {
        case CLI_INIT:
		e->command = "do this well";
                e->usage =
			"Usage: do this well <arg>\n"
			"	typically multiline with body indented\n";
		return NULL;

        case CLI_GENERATE:
                if (a->pos > e->args)
                        return NULL;
        	return css_cli_complete(a->word, choices, a->n);

        default:        
                // we are guaranteed to be called with argc >= e->args;
                if (a->argc > e->args + 1) // we accept one extra argument
                        return CLI_SHOWUSAGE;
                css_cli(a->fd, "done this well for %s\n", e->args[argc-1]);
                return CLI_SUCCESS;
        }
}

\endcode
 
 */

/*! \brief calling arguments for new-style handlers. 
* \arg \ref CLI_command_API
*/
enum css_cli_command {
	CLI_INIT = -2,		/* return the usage string */
	CLI_GENERATE = -3,	/* behave as 'generator', remap argv to struct css_cli_args */
	CLI_HANDLER = -4,	/* run the normal handler */
};

/* argument for new-style CLI handler */
struct css_cli_args {
	const int fd;
	const int argc;
	const char * const *argv;
	const char *line;	/* the current input line */
	const char *word;	/* the word we want to complete */
	const int pos;		/* position of the word to complete */
	const int n;		/* the iteration count (n-th entry we generate) */
};

/*! \brief descriptor for a cli entry. 
 * \arg \ref CLI_command_API
 */
struct css_cli_entry {
	const char * const cmda[CSS_MAX_CMD_LEN];	/*!< words making up the command.
							 * set the first entry to NULL for a new-style entry.
							 */

	const char * const summary; 			/*!< Summary of the command (< 60 characters) */
	const char * usage; 				/*!< Detailed usage information */

	int inuse; 				/*!< For keeping track of usage */
	struct module *module;			/*!< module this belongs to */
	char *_full_cmd;			/*!< built at load time from cmda[] */
	int cmdlen;				/*!< len up to the first invalid char [<{% */
	/*! \brief This gets set in css_cli_register()
	 */
	int args;				/*!< number of non-null entries in cmda */
	char *command;				/*!< command, non-null for new-style entries */
	char *(*handler)(struct css_cli_entry *e, int cmd, struct css_cli_args *a);
	/*! For linking */
	CSS_LIST_ENTRY(css_cli_entry) list;
};

#if defined(__cplusplus) || defined(c_plusplus)
#define CSS_CLI_DEFINE(fn, txt) { { "" }, txt, NULL, 0, NULL, NULL, 0, 0, NULL, fn }
#else
/* XXX the parser in gcc 2.95 gets confused if you don't put a space
 * between the lcss arg before VA_ARGS and the comma */
#define CSS_CLI_DEFINE(fn, txt , ... )	{ .handler = fn, .summary = txt, ## __VA_ARGS__ }
#endif

/*!
 * Helper function to generate cli entries from a NULL-terminated array.
 * Returns the n-th matching entry from the array, or NULL if not found.
 * Can be used to implement generate() for static entries as below
 * (in this example we complete the word in position 2):
  \code
    char *my_generate(const char *line, const char *word, int pos, int n)
    {
        static const char * const choices[] = { "one", "two", "three", NULL };
	if (pos == 2)
        	return css_cli_complete(word, choices, n);
	else
		return NULL;
    }
  \endcode
 */
char *css_cli_complete(const char *word, const char * const choices[], int pos);

/*! 
 * \brief Interprets a command
 * Interpret a command s, sending output to fd if uid:gid has permissions
 * to run this command. uid = CLI_NO_PERMS to avoid checking user permissions
 * gid = CLI_NO_PERMS to avoid checking group permissions.
 * \param uid User ID that is trying to run the command.
 * \param gid Group ID that is trying to run the command.
 * \param fd pipe
 * \param s incoming string
 * \retval 0 on success
 * \retval -1 on failure
 */
int css_cli_command_full(int uid, int gid, int fd, const char *s);

#define css_cli_command(fd,s) css_cli_command_full(CLI_NO_PERMS, CLI_NO_PERMS, fd, s) 

/*! 
 * \brief Executes multiple CLI commands
 * Interpret strings separated by NULL and execute each one, sending output to fd
 * if uid has permissions, uid = CLI_NO_PERMS to avoid checking users permissions.
 * gid = CLI_NO_PERMS to avoid checking group permissions.
 * \param uid User ID that is trying to run the command.
 * \param gid Group ID that is trying to run the command.
 * \param fd pipe
 * \param size is the total size of the string
 * \param s incoming string
 * \retval number of commands executed
 */
int css_cli_command_multiple_full(int uid, int gid, int fd, size_t size, const char *s);

#define css_cli_command_multiple(fd,size,s) css_cli_command_multiple_full(CLI_NO_PERMS, CLI_NO_PERMS, fd, size, s)

/*! \brief Registers a command or an array of commands
 * \param e which cli entry to register.
 * Register your own command
 * \retval 0 on success
 * \retval -1 on failure
 */
int css_cli_register(struct css_cli_entry *e);

/*!
 * \brief Register multiple commands
 * \param e pointer to first cli entry to register
 * \param len number of entries to register
 */
int css_cli_register_multiple(struct css_cli_entry *e, int len);

/*! 
 * \brief Unregisters a command or an array of commands
 * \param e which cli entry to unregister
 * Unregister your own command.  You must pass a completed css_cli_entry structure
 * \return 0
 */
int css_cli_unregister(struct css_cli_entry *e);

/*!
 * \brief Unregister multiple commands
 * \param e pointer to first cli entry to unregister
 * \param len number of entries to unregister
 */
int css_cli_unregister_multiple(struct css_cli_entry *e, int len);

/*! 
 * \brief Readline madness
 * Useful for readline, that's about it
 * \retval 0 on success
 * \retval -1 on failure
 */
char *css_cli_generator(const char *, const char *, int);

int css_cli_generatornummatches(const char *, const char *);

/*!
 * \brief Generates a NULL-terminated array of strings that
 * 1) begin with the string in the second parameter, and
 * 2) are valid in a command after the string in the first parameter.
 *
 * The first entry (offset 0) of the result is the longest common substring
 * in the results, useful to extend the string that has been completed.
 * Subsequent entries are all possible values, followed by a NULL.
 * All strings and the array itself are malloc'ed and must be freed
 * by the caller.
 */
char **css_cli_completion_matches(const char *, const char *);

/*!
 * \brief Command completion for the list of active channels.
 *
 * This can be called from a CLI command completion function that wants to
 * complete from the list of active channels.  'rpos' is the required
 * position in the command.  This function will return NULL immediately if
 * 'rpos' is not the same as the current position, 'pos'.
 */
char *css_complete_channels(const char *line, const char *word, int pos, int state, int rpos);

#ifdef	__cplusplus
}
#endif

#endif	/* CLI_H */

