/*!
  \file logger.h
  \brief Support for logging to various files, console and syslog
	Configuration in file logger.conf
*/

#ifndef _LOGGER_H
#define _LOGGER_H

#include "options.h"	/* need option_debug */
#include "compiler.h"

#include <stdarg.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define EVENTLOG "event_log"
#define	QUEUELOG "queue_log"

#define DEBUG_M(a) { \
	a; \
}
    
    
#define CSS_MODULE "logger"
    
int option_verbose;
int option_debug;

#define VERBOSE_PREFIX_1 " "
#define VERBOSE_PREFIX_2 "  == "
#define VERBOSE_PREFIX_3 "    -- "
#define VERBOSE_PREFIX_4 "       > "

/*! \brief Used for sending a log message
	This is the standard logger function.  Probably the only way you will invoke it would be something like this:
	css_log(CSS_LOG_WHATEVER, "Problem with the %s Captain.  We should get some more.  Will %d be enough?\n", "flux capacitor", 10);
	where WHATEVER is one of ERROR, DEBUG, EVENT, NOTICE, or WARNING depending
	on which log you wish to output to. These are implemented as macros, that
	will provide the function with the needed arguments.

 	\param level	Type of log event
	\param file	Will be provided by the CSS_LOG_* macro
	\param line	Will be provided by the CSS_LOG_* macro
	\param function	Will be provided by the CSS_LOG_* macro
	\param fmt	This is what is important.  The format is the same as your favorite breed of printf.  You know how that works, right? :-)
 */

void css_log(int level, const char *file, int line, const char *function, const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));

void css_backtrace(void);

/*! \brief Reload logger without rotating log files */
int logger_reload(void);

void __attribute__((format(printf, 5, 6))) css_queue_log(const char *queuename, const char *callid, const char *agent, const char *event, const char *fmt, ...);

/*! Send a verbose message (based on verbose level)
 	\brief This works like css_log, but prints verbose messages to the console depending on verbosity level set.
 	css_verbose(VERBOSE_PREFIX_3 "Whatever %s is happening\n", "nothing");
 	This will print the message to the console if the verbose level is set to a level >= 3
 	Note the abscence of a comma after the VERBOSE_PREFIX_3.  This is important.
 	VERBOSE_PREFIX_1 through VERBOSE_PREFIX_3 are defined.
 */

void __attribute__((format(printf, 4, 5))) __css_verbose(const char *file, int line, const char *func, const char *fmt, ...);

#define css_verbose(...) __css_verbose(__FILE__, __LINE__, __PRETTY_FUNCTION__,  __VA_ARGS__)

void __attribute__((format(printf, 4, 0))) __css_verbose_ap(const char *file, int line, const char *func, const char *fmt, va_list ap);

#define css_verbose_ap(fmt, ap)	__css_verbose_ap(__FILE__, __LINE__, __PRETTY_FUNCTION__, fmt, ap)

void __attribute__((format(printf, 2, 3))) css_child_verbose(int level, const char *fmt, ...);

int css_register_verbose(void (*verboser)(const char *string)) attribute_warn_unused_result;
int css_unregister_verbose(void (*verboser)(const char *string)) attribute_warn_unused_result;

void css_console_puts(const char *string);

/*!
 * \brief log the string to the console, and all attached
 * console clients
 * \version 1.6.1 added level parameter
 */
void css_console_puts_mutable(const char *string, int level);
void css_console_toggle_mute(int fd, int silent);

/*!
 * \brief enables or disables logging of a specified level to the console
 * fd specifies the index of the console receiving the level change
 * level specifies the index of the logging level being toggled
 * state indicates whether logging will be on or off (0 for off, 1 for on)
 */
void css_console_toggle_loglevel(int fd, int level, int state);

/* Note: The CSS_LOG_* macros below are the same as
 * the LOG_* macros and are intended to eventually replace
 * the LOG_* macros to avoid name collisions as has been
 * seen in app_voicemail. However, please do NOT remove
 * the LOG_* macros from the source since these may be still
 * needed for third-party modules
 */

#define _A_ __FILE__, __LINE__, __PRETTY_FUNCTION__

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#define __LOG_DEBUG    0
#define LOG_DEBUG      __LOG_DEBUG, _A_

#ifdef CSS_LOG_DEBUG
#undef CSS_LOG_DEBUG
#endif
#define CSS_LOG_DEBUG      __LOG_DEBUG, _A_

#ifdef LOG_NOTICE
#undef LOG_NOTICE
#endif
#define __LOG_NOTICE   2
#define LOG_NOTICE     __LOG_NOTICE, _A_

#ifdef CSS_LOG_NOTICE
#undef CSS_LOG_NOTICE
#endif
#define CSS_LOG_NOTICE     __LOG_NOTICE, _A_

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#define __LOG_WARNING  3
#define LOG_WARNING    __LOG_WARNING, _A_

#ifdef CSS_LOG_WARNING
#undef CSS_LOG_WARNING
#endif
#define CSS_LOG_WARNING    __LOG_WARNING, _A_

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#define __LOG_ERROR    4
#define LOG_ERROR      __LOG_ERROR, _A_

#ifdef CSS_LOG_ERROR
#undef CSS_LOG_ERROR
#endif
#define CSS_LOG_ERROR      __LOG_ERROR, _A_

#ifdef LOG_VERBOSE
#undef LOG_VERBOSE
#endif
#define __LOG_VERBOSE  5
#define LOG_VERBOSE    __LOG_VERBOSE, _A_

#ifdef CSS_LOG_VERBOSE
#undef CSS_LOG_VERBOSE
#endif
#define LOG_VERBOSE    __LOG_VERBOSE, _A_

#ifdef LOG_DTMF
#undef LOG_DTMF
#endif
#define __LOG_DTMF  6
#define LOG_DTMF    __LOG_DTMF, _A_

#ifdef CSS_LOG_DTMF
#undef CSS_LOG_DTMF
#endif
#define CSS_LOG_DTMF    __LOG_DTMF, _A_

#define NUMLOGLEVELS 7

/*!
 * \brief Get the debug level for a module
 * \param module the name of module
 * \return the debug level
 */
unsigned int css_debug_get_by_module(const char *module);

/*!
 * \brief Get the verbose level for a module
 * \param module the name of module
 * \return the verbose level
 */
unsigned int css_verbose_get_by_module(const char *module);

/*!
 * \brief Register a new logger level
 * \param name The name of the level to be registered
 * \retval -1 if an error occurs
 * \retval non-zero level to be used with css_log for sending messages to this level
 * \since 1.8
 */
int css_logger_register_level(const char *name);

/*!
 * \brief Unregister a previously registered logger level
 * \param name The name of the level to be unregistered
 * \return nothing
 * \since 1.8
 */
void css_logger_unregister_level(const char *name);

/*!
 * \brief Send a log message to a dynamically registered log level
 * \param level The log level to send the message to
 *
 * Like css_log, the log message may include printf-style formats, and
 * the data for these must be provided as additional parameters after
 * the log message.
 *
 * \return nothing
 * \since 1.8
 */

#define css_log_dynamic_level(level, ...) css_log(level, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)

/*!
 * \brief Log a DEBUG message
 * \param level The minimum value of option_debug for this message
 *        to get logged
 */
#define css_debug(level, ...) do {       \
	if (option_debug >= (level) || (css_opt_dbg_module && css_debug_get_by_module(CSS_MODULE) >= (level)) ) \
		css_log(CSS_LOG_DEBUG, __VA_ARGS__); \
} while (0)

#define VERBOSITY_ATLECSS(level) (option_verbose >= (level) || (css_opt_verb_module && css_verbose_get_by_module(CSS_MODULE) >= (level)))

#define css_verb(level, ...) do { \
	if (VERBOSITY_ATLECSS((level)) ) { \
		if (level >= 4) \
			css_verbose(VERBOSE_PREFIX_4 __VA_ARGS__); \
		else if (level == 3) \
			css_verbose(VERBOSE_PREFIX_3 __VA_ARGS__); \
		else if (level == 2) \
			css_verbose(VERBOSE_PREFIX_2 __VA_ARGS__); \
		else if (level == 1) \
			css_verbose(VERBOSE_PREFIX_1 __VA_ARGS__); \
		else \
			css_verbose(__VA_ARGS__); \
	} \
} while (0)

#ifndef _LOGGER_BACKTRACE_H
#define _LOGGER_BACKTRACE_H
#ifdef HAVE_BKTR
#define CSS_MAX_BT_FRAMES 32
/* \brief
 *
 * A structure to hold backtrace information. This structure provides an easy means to
 * store backtrace information or pass backtraces to other functions.
 */
struct css_bt {
	/*! The addresses of the stack frames. This is filled in by calling the glibc backtrace() function */
	void *addresses[CSS_MAX_BT_FRAMES];
	/*! The number of stack frames in the backtrace */
	int num_frames;
	/*! Tells if the css_bt structure was dynamically allocated */
	unsigned int alloced:1;
};

/* \brief
 * Allocates memory for an css_bt and stores addresses and symbols.
 *
 * \return Returns NULL on failure, or the allocated css_bt on success
 * \since 1.6.1
 */
struct css_bt *css_bt_create(void);

/* \brief
 * Fill an allocated css_bt with addresses
 *
 * \retval 0 Success
 * \retval -1 Failure
 * \since 1.6.1
 */
int css_bt_get_addresses(struct css_bt *bt);

/* \brief
 *
 * Free dynamically allocated portions of an css_bt
 *
 * \retval NULL.
 * \since 1.6.1
 */
void *css_bt_destroy(struct css_bt *bt);

/* \brief Retrieve symbols for a set of backtrace addresses
 *
 * \param addresses A list of addresses, such as the ->addresses structure element of struct css_bt.
 * \param num_frames Number of addresses in the addresses list
 * \retval NULL Unable to allocate memory
 * \return List of strings
 * \since 1.6.2.16
 */
char **css_bt_get_symbols(void **addresses, size_t num_frames);

#endif /* HAVE_BKTR */
#endif /* _LOGGER_BACKTRACE_H */

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _LOGGER_H */