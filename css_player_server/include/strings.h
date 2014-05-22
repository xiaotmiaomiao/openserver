/* 
 * File:   string.h
 * Author: root
 *
 * Created on April 17, 2014, 7:10 PM
 */

#ifndef STRING_H
#define	STRING_H

#ifdef	__cplusplus
extern "C" {
#endif

/* #define DEBUG_OPAQUE */

#include <ctype.h>

#include "utils.h"
#include "threadstorage.h"
#include "inline_api.h"
#include "compiler.h"

#if defined(DEBUG_OPAQUE)
#define __CSS_STR_USED used2
#define __CSS_STR_LEN len2
#define __CSS_STR_STR str2
#define __CSS_STR_TS ts2
#else
#define __CSS_STR_USED used
#define __CSS_STR_LEN len
#define __CSS_STR_STR str
#define __CSS_STR_TS ts
#endif

/* You may see ccsss in this header that may seem useless but they ensure this file is C++ clean */

#define AS_OR(a,b)	(a && css_str_strlen(a)) ? css_str_buffer(a) : (b)

#ifdef CSS_DEVMODE
#define css_strlen_zero(foo)	_css_strlen_zero(foo, __FILE__, __PRETTY_FUNCTION__, __LINE__)
    
static force_inline int _css_strlen_zero(const char *s, const char *file, const char *function, int line)
{
	if (!s || (*s == '\0')) {
		return 1;
	}
	if (!strcmp(s, "(null)")) {
		//css_log(__LOG_WARNING, file, line, function, "Possible programming error: \"(null)\" is not NULL!\n");
	}
	return 0;
}

#else
static force_inline int attribute_pure css_strlen_zero(const char *s)
{
	return (!s || (*s == '\0'));
}
#endif

#ifdef SENSE_OF_HUMOR
#define css_strlen_real(a)	(a) ? strlen(a) : 0
#define css_strlen_imaginary(a)	css_random()
#endif

/*! \brief returns the equivalent of logic or for strings:
 * first one if not empty, otherwise second one.
 */
#define S_OR(a, b) ({typeof(&((a)[0])) __x = (a); css_strlen_zero(__x) ? (b) : __x;})

/*! \brief returns the equivalent of logic or for strings, with an additional boolean check:
 * second one if not empty and first one is true, otherwise third one.
 * example: S_COR(usewidget, widget, "<no widget>")
 */
#define S_COR(a, b, c) ({typeof(&((b)[0])) __x = (b); (a) && !css_strlen_zero(__x) ? (__x) : (c);})

/*!
  \brief Gets a pointer to the first non-whitespace character in a string.
  \param str the input string
  \return a pointer to the first non-whitespace character
 */
CSS_INLINE_API(
char * attribute_pure css_skip_blanks(const char *str),
{
	while (*str && ((unsigned char) *str) < 33)
		str++;
	return (char *) str;
}
)

/*!
  \brief Trims trailing whitespace characters from a string.
  \param str the input string
  \return a pointer to the modified string
 */
CSS_INLINE_API(
char *css_trim_blanks(char *str),
{
	char *work = str;

	if (work) {
		work += strlen(work) - 1;
		/* It's tempting to only want to erase after we exit this loop, 
		   but since css_trim_blanks *could* receive a constant string
		   (which we presumably wouldn't have to touch), we shouldn't
		   actually set anything unless we must, and it's easier just
		   to set each position to \0 than to keep track of a variable
		   for it */
		while ((work >= str) && ((unsigned char) *work) < 33)
			*(work--) = '\0';
	}
	return str;
}
)

/*!
  \brief Gets a pointer to first whitespace character in a string.
  \param str the input string
  \return a pointer to the first whitespace character
 */
CSS_INLINE_API(
char * attribute_pure css_skip_nonblanks(const char *str),
{
	while (*str && ((unsigned char) *str) > 32)
		str++;
	return (char *) str;
}
)
  
/*!
  \brief Strip leading/trailing whitespace from a string.
  \param s The string to be stripped (will be modified).
  \return The stripped string.

  This functions strips all leading and trailing whitespace
  characters from the input string, and returns a pointer to
  the resulting string. The string is modified in place.
*/
CSS_INLINE_API(
char *css_strip(char *s),
{
	if ((s = css_skip_blanks(s))) {
		css_trim_blanks(s);
	}
	return s;
} 
)

/*!
  \brief Strip leading/trailing whitespace and quotes from a string.
  \param s The string to be stripped (will be modified).
  \param beg_quotes The list of possible beginning quote characters.
  \param end_quotes The list of matching ending quote characters.
  \return The stripped string.

  This functions strips all leading and trailing whitespace
  characters from the input string, and returns a pointer to
  the resulting string. The string is modified in place.

  It can also remove beginning and ending quote (or quote-like)
  characters, in matching pairs. If the first character of the
  string matches any character in beg_quotes, and the lcss
  character of the string is the matching character in
  end_quotes, then they are removed from the string.

  Examples:
  \code
  css_strip_quoted(buf, "\"", "\"");
  css_strip_quoted(buf, "'", "'");
  css_strip_quoted(buf, "[{(", "]})");
  \endcode
 */
char *css_strip_quoted(char *s, const char *beg_quotes, const char *end_quotes);

/*!
  \brief Strip backslash for "escaped" semicolons, 
	the string to be stripped (will be modified).
  \return The stripped string.
 */
char *css_unescape_semicolon(char *s);

/*!
  \brief Convert some C escape sequences  \verbatim (\b\f\n\r\t) \endverbatim into the
	equivalent characters. The string to be converted (will be modified).
  \return The converted string.
 */
char *css_unescape_c(char *s);

/*!
  \brief Size-limited null-terminating string copy.
  \param dst The destination buffer.
  \param src The source string
  \param size The size of the destination buffer
  \return Nothing.

  This is similar to \a strncpy, with two important differences:
    - the destination buffer will \b always be null-terminated
    - the destination buffer is not filled with zeros pcss the copied string length
  These differences make it slightly more efficient, and safer to use since it will
  not leave the destination buffer unterminated. There is no need to pass an artificially
  reduced buffer size to this function (unlike \a strncpy), and the buffer does not need
  to be initialized to zeroes prior to calling this function.
*/
CSS_INLINE_API(
void css_copy_string(char *dst, const char *src, size_t size),
{
	while (*src && size) {
		*dst++ = *src++;
		size--;
	}
	if (__builtin_expect(!size, 0))
		dst--;
	*dst = '\0';
}
)

/*!
  \brief Build a string in a buffer, designed to be called repeatedly
  
  \note This method is not recommended. New code should use css_str_*() instead.

  This is a wrapper for snprintf, that properly handles the buffer pointer
  and buffer space available.

  \param buffer current position in buffer to place string into (will be updated on return)
  \param space remaining space in buffer (will be updated on return)
  \param fmt printf-style format string
  \retval 0 on success
  \retval non-zero on failure.
*/
int css_build_string(char **buffer, size_t *space, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/*!
  \brief Build a string in a buffer, designed to be called repeatedly
  
  This is a wrapper for snprintf, that properly handles the buffer pointer
  and buffer space available.

  \return 0 on success, non-zero on failure.
  \param buffer current position in buffer to place string into (will be updated on return)
  \param space remaining space in buffer (will be updated on return)
  \param fmt printf-style format string
  \param ap varargs list of arguments for format
*/
int css_build_string_va(char **buffer, size_t *space, const char *fmt, va_list ap) __attribute__((format(printf, 3, 0)));

/*! 
 * \brief Make sure something is true.
 * Determine if a string containing a boolean value is "true".
 * This function checks to see whether a string passed to it is an indication of an "true" value.  
 * It checks to see if the string is "yes", "true", "y", "t", "on" or "1".  
 *
 * \retval 0 if val is a NULL pointer.
 * \retval -1 if "true".
 * \retval 0 otherwise.
 */
int attribute_pure css_true(const char *val);

/*! 
 * \brief Make sure something is false.
 * Determine if a string containing a boolean value is "false".
 * This function checks to see whether a string passed to it is an indication of an "false" value.  
 * It checks to see if the string is "no", "false", "n", "f", "off" or "0".  
 *
 * \retval 0 if val is a NULL pointer.
 * \retval -1 if "true".
 * \retval 0 otherwise.
 */
int attribute_pure css_false(const char *val);

/*
 *  \brief Join an array of strings into a single string.
 * \param s the resulting string buffer
 * \param len the length of the result buffer, s
 * \param w an array of strings to join.
 *
 * This function will join all of the strings in the array 'w' into a single
 * string.  It will also place a space in the result buffer in between each
 * string from 'w'.
*/
void css_join(char *s, size_t len, const char * const w[]);

/*
  \brief Parse a time (integer) string.
  \param src String to parse
  \param dst Destination
  \param _default Value to use if the string does not contain a valid time
  \param consumed The number of characters 'consumed' in the string by the parse (see 'man sscanf' for details)
  \retval 0 on success
  \retval non-zero on failure.
*/

//int css_get_time_t(const char *src, time_t *dst, time_t _default, int *consumed);

/*
  \brief Parse a time (float) string.
  \param src String to parse
  \param dst Destination
  \param _default Value to use if the string does not contain a valid time
  \param consumed The number of characters 'consumed' in the string by the parse (see 'man sscanf' for details)
  \return zero on success, non-zero on failure
*/
int css_get_timeval(const char *src, struct timeval *tv, struct timeval _default, int *consumed);

/*!
 * Support for dynamic strings.
 *
 * A dynamic string is just a C string prefixed by a few control fields
 * that help setting/appending/extending it using a printf-like syntax.
 *
 * One should never declare a variable with this type, but only a pointer
 * to it, e.g.
 *
 *	struct css_str *ds;
 *
 * The pointer can be initialized with the following:
 *
 *	ds = css_str_create(init_len);
 *		creates a malloc()'ed dynamic string;
 *
 *	ds = css_str_alloca(init_len);
 *		creates a string on the stack (not very dynamic!).
 *
 *	ds = css_str_thread_get(ts, init_len)
 *		creates a malloc()'ed dynamic string associated to
 *		the thread-local storage key ts
 *
 * Finally, the string can be manipulated with the following:
 *
 *	css_str_set(&buf, max_len, fmt, ...)
 *	css_str_append(&buf, max_len, fmt, ...)
 *
 * and their varargs variant
 *
 *	css_str_set_va(&buf, max_len, ap)
 *	css_str_append_va(&buf, max_len, ap)
 *
 * \param max_len The maximum allowed capacity of the css_str. Note that
 *  if the value of max_len is less than the current capacity of the
 *  css_str (as returned by css_str_size), then the parameter is effectively
 *  ignored.
 * 	0 means unlimited, -1 means "at most the available space"
 *
 * \return All the functions return <0 in case of error, or the
 *	length of the string added to the buffer otherwise. Note that
 *	in most cases where an error is returned, characters ARE written
 *	to the css_str.
 */

/*! \brief The descriptor of a dynamic string
 *  XXX storage will be optimized later if needed
 * We use the ts field to indicate the type of storage.
 * Three special constants indicate malloc, alloca() or static
 * variables, all other values indicate a
 * struct css_threadstorage pointer.
 */
struct css_str {
	size_t __CSS_STR_LEN;			/*!< The current maximum length of the string */
	size_t __CSS_STR_USED;			/*!< Amount of space used */
	struct css_threadstorage *__CSS_STR_TS;	/*!< What kind of storage is this ? */
#define DS_MALLOC	((struct css_threadstorage *)1)
#define DS_ALLOCA	((struct css_threadstorage *)2)
#define DS_STATIC	((struct css_threadstorage *)3)	/* not supported yet */
	char __CSS_STR_STR[0];			/*!< The string buffer */
};

/*!
 * \brief Create a malloc'ed dynamic length string
 *
 * \param init_len This is the initial length of the string buffer
 *
 * \return This function returns a pointer to the dynamic string length.  The
 *         result will be NULL in the case of a memory allocation error.
 *
 * \note The result of this function is dynamically allocated memory, and must
 *       be free()'d after it is no longer needed.
 */
#if (defined(MALLOC_DEBUG) && !defined(STANDALONE))
#define	css_str_create(a)	_css_str_create(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
CSS_INLINE_API(
struct css_str * attribute_malloc _css_str_create(size_t init_len,
		const char *file, int lineno, const char *func),
{
	struct css_str *buf;

	buf = (struct css_str *)__css_calloc(1, sizeof(*buf) + init_len, file, lineno, func);
	if (buf == NULL)
		return NULL;

	buf->__CSS_STR_LEN = init_len;
	buf->__CSS_STR_USED = 0;
	buf->__CSS_STR_TS = DS_MALLOC;

	return buf;
}
)
#else
CSS_INLINE_API(
struct css_str * attribute_malloc css_str_create(size_t init_len),
{
	struct css_str *buf;

	buf = (struct css_str *)css_calloc(1, sizeof(*buf) + init_len);
	if (buf == NULL)
		return NULL;

	buf->__CSS_STR_LEN = init_len;
	buf->__CSS_STR_USED = 0;
	buf->__CSS_STR_TS = DS_MALLOC;

	return buf;
}
)
#endif

/*! \brief Reset the content of a dynamic string.
 * Useful before a series of css_str_append.
 */
CSS_INLINE_API(
void css_str_reset(struct css_str *buf),
{
	if (buf) {
		buf->__CSS_STR_USED = 0;
		if (buf->__CSS_STR_LEN) {
			buf->__CSS_STR_STR[0] = '\0';
		}
	}
}
)

/*! \brief Update the length of the buffer, after using css_str merely as a buffer.
 *  \param buf A pointer to the css_str string.
 */
CSS_INLINE_API(
void css_str_update(struct css_str *buf),
{
	buf->__CSS_STR_USED = strlen(buf->__CSS_STR_STR);
}
)

/*! \brief Trims trailing whitespace characters from an css_str string.
 *  \param buf A pointer to the css_str string.
 */
CSS_INLINE_API(
void css_str_trim_blanks(struct css_str *buf),
{
	if (!buf) {
		return;
	}
	while (buf->__CSS_STR_USED && buf->__CSS_STR_STR[buf->__CSS_STR_USED - 1] < 33) {
		buf->__CSS_STR_STR[--(buf->__CSS_STR_USED)] = '\0';
	}
}
)

/*!\brief Returns the current length of the string stored within buf.
 * \param buf A pointer to the css_str structure.
 */
CSS_INLINE_API(
size_t attribute_pure css_str_strlen(const struct css_str *buf),
{
	return buf->__CSS_STR_USED;
}
)

/*!\brief Returns the current maximum length (without reallocation) of the current buffer.
 * \param buf A pointer to the css_str structure.
 * \retval Current maximum length of the buffer.
 */
CSS_INLINE_API(
size_t attribute_pure css_str_size(const struct css_str *buf),
{
	return buf->__CSS_STR_LEN;
}
)

/*!\brief Returns the string buffer within the css_str buf.
 * \param buf A pointer to the css_str structure.
 * \retval A pointer to the enclosed string.
 */
CSS_INLINE_API(
char * attribute_pure css_str_buffer(const struct css_str *buf),
{
	/* for now, ccss away the const qualifier on the pointer
	 * being returned; eventually, it should become truly const
	 * and only be modified via accessor functions
	 */
	return (char *) buf->__CSS_STR_STR;
}
)

/*!\brief Truncates the enclosed string to the given length.
 * \param buf A pointer to the css_str structure.
 * \param len Maximum length of the string.
 * \retval A pointer to the resulting string.
 */
CSS_INLINE_API(
char *css_str_truncate(struct css_str *buf, ssize_t len),
{
	if (len < 0) {
		buf->__CSS_STR_USED += ((ssize_t) abs(len)) > (ssize_t) buf->__CSS_STR_USED ? -buf->__CSS_STR_USED : len;
	} else {
		buf->__CSS_STR_USED = len;
	}
	buf->__CSS_STR_STR[buf->__CSS_STR_USED] = '\0';
	return buf->__CSS_STR_STR;
}
)
	
/*
 * CSS_INLINE_API() is a macro that takes a block of code as an argument.
 * Using preprocessor #directives in the argument is not supported by all
 * compilers, and it is a bit of an obfuscation anyways, so avoid it.
 * As a workaround, define a macro that produces either its argument
 * or nothing, and use that instead of #ifdef/#endif within the
 * argument to CSS_INLINE_API().
 */
#if defined(DEBUG_THREADLOCALS)
#define	_DB1(x)	x
#else
#define _DB1(x)
#endif

/*!
 * Make space in a new string (e.g. to read in data from a file)
 */
#if (defined(MALLOC_DEBUG) && !defined(STANDALONE))
CSS_INLINE_API(
int _css_str_make_space(struct css_str **buf, size_t new_len, const char *file, int lineno, const char *function),
{
	struct css_str *old_buf = *buf;

	if (new_len <= (*buf)->__CSS_STR_LEN) 
		return 0;	/* success */
	if ((*buf)->__CSS_STR_TS == DS_ALLOCA || (*buf)->__CSS_STR_TS == DS_STATIC)
		return -1;	/* cannot extend */
	*buf = (struct css_str *)__css_realloc(*buf, new_len + sizeof(struct css_str), file, lineno, function);
	if (*buf == NULL) {
		*buf = old_buf;
		return -1;
	}
	if ((*buf)->__CSS_STR_TS != DS_MALLOC) {
		pthread_setspecific((*buf)->__CSS_STR_TS->key, *buf);
		_DB1(__css_threadstorage_object_replace(old_buf, *buf, new_len + sizeof(struct css_str));)
	}

	(*buf)->__CSS_STR_LEN = new_len;
	return 0;
}
)
#define css_str_make_space(a,b)	_css_str_make_space(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#else
CSS_INLINE_API(
int css_str_make_space(struct css_str **buf, size_t new_len),
{
	struct css_str *old_buf = *buf;

	if (new_len <= (*buf)->__CSS_STR_LEN) 
		return 0;	/* success */
	if ((*buf)->__CSS_STR_TS == DS_ALLOCA || (*buf)->__CSS_STR_TS == DS_STATIC)
		return -1;	/* cannot extend */
	*buf = (struct css_str *)css_realloc(*buf, new_len + sizeof(struct css_str));
	if (*buf == NULL) {
		*buf = old_buf;
		return -1;
	}
	if ((*buf)->__CSS_STR_TS != DS_MALLOC) {
		pthread_setspecific((*buf)->__CSS_STR_TS->key, *buf);
		_DB1(__css_threadstorage_object_replace(old_buf, *buf, new_len + sizeof(struct css_str));)
	}

	(*buf)->__CSS_STR_LEN = new_len;
	return 0;
}
)
#endif

CSS_INLINE_API(
int css_str_copy_string(struct css_str **dst, struct css_str *src),
{

	/* make sure our destination is large enough */
	if (src->__CSS_STR_USED + 1 > (*dst)->__CSS_STR_LEN) {
		if (css_str_make_space(dst, src->__CSS_STR_USED + 1)) {
			return -1;
		}
	}

	memcpy((*dst)->__CSS_STR_STR, src->__CSS_STR_STR, src->__CSS_STR_USED + 1);
	(*dst)->__CSS_STR_USED = src->__CSS_STR_USED;
	return 0;
}
)

#define css_str_alloca(init_len)			\
	({						\
		struct css_str *__css_str_buf;			\
		__css_str_buf = alloca(sizeof(*__css_str_buf) + init_len);	\
		__css_str_buf->__CSS_STR_LEN = init_len;			\
		__css_str_buf->__CSS_STR_USED = 0;				\
		__css_str_buf->__CSS_STR_TS = DS_ALLOCA;			\
		__css_str_buf->__CSS_STR_STR[0] = '\0';			\
		(__css_str_buf);					\
	})

/*!
 * \brief Retrieve a thread locally stored dynamic string
 *
 * \param ts This is a pointer to the thread storage structure declared by using
 *      the CSS_THREADSTORAGE macro.  If declared with 
 *      CSS_THREADSTORAGE(my_buf, my_buf_init), then this argument would be 
 *      (&my_buf).
 * \param init_len This is the initial length of the thread's dynamic string. The
 *      current length may be bigger if previous operations in this thread have
 *      caused it to increase.
 *
 * \return This function will return the thread locally stored dynamic string
 *         associated with the thread storage management variable passed as the
 *         first argument.
 *         The result will be NULL in the case of a memory allocation error.
 *
 * Example usage:
 * \code
 * CSS_THREADSTORAGE(my_str, my_str_init);
 * #define MY_STR_INIT_SIZE   128
 * ...
 * void my_func(const char *fmt, ...)
 * {
 *      struct css_str *buf;
 *
 *      if (!(buf = css_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
 *           return;
 *      ...
 * }
 * \endcode
 */
#if !defined(DEBUG_THREADLOCALS)
CSS_INLINE_API(
struct css_str *css_str_thread_get(struct css_threadstorage *ts,
	size_t init_len),
{
	struct css_str *buf;

	buf = (struct css_str *)css_threadstorage_get(ts, sizeof(*buf) + init_len);
	if (buf == NULL)
		return NULL;

	if (!buf->__CSS_STR_LEN) {
		buf->__CSS_STR_LEN = init_len;
		buf->__CSS_STR_USED = 0;
		buf->__CSS_STR_TS = ts;
	}

	return buf;
}
)
#else /* defined(DEBUG_THREADLOCALS) */
CSS_INLINE_API(
struct css_str *__css_str_thread_get(struct css_threadstorage *ts,
	size_t init_len, const char *file, const char *function, unsigned int line),
{
	struct css_str *buf;

	buf = (struct css_str *)__css_threadstorage_get(ts, sizeof(*buf) + init_len, file, function, line);
	if (buf == NULL)
		return NULL;

	if (!buf->__CSS_STR_LEN) {
		buf->__CSS_STR_LEN = init_len;
		buf->__CSS_STR_USED = 0;
		buf->__CSS_STR_TS = ts;
	}

	return buf;
}
)

#define css_str_thread_get(ts, init_len) __css_str_thread_get(ts, init_len, __FILE__, __PRETTY_FUNCTION__, __LINE__)
#endif /* defined(DEBUG_THREADLOCALS) */

/*!
 * \brief Error codes from __css_str_helper()
 * The undelying processing to manipulate dynamic string is done
 * by __css_str_helper(), which can return a success or a
 * permanent failure (e.g. no memory).
 */
enum {
	/*! An error has occurred and the contents of the dynamic string
	 *  are undefined */
	CSS_DYNSTR_BUILD_FAILED = -1,
	/*! The buffer size for the dynamic string had to be increased, and
	 *  __css_str_helper() needs to be called again after
	 *  a va_end() and va_start().  This return value is legacy and will
	 *  no longer be used.
	 */
	CSS_DYNSTR_BUILD_RETRY = -2
};

/*!
 * \brief Core functionality of css_str_(set|append)_va
 *
 * The arguments to this function are the same as those described for
 * css_str_set_va except for an addition argument, append.
 * If append is non-zero, this will append to the current string instead of
 * writing over it.
 *
 * CSS_DYNSTR_BUILD_RETRY is a legacy define.  It should probably never
 * again be used.
 *
 * A return of CSS_DYNSTR_BUILD_FAILED indicates a memory allocation error.
 *
 * A return value greater than or equal to zero indicates the number of
 * characters that have been written, not including the terminating '\0'.
 * In the append case, this only includes the number of characters appended.
 *
 * \note This function should never need to be called directly.  It should
 *       through calling one of the other functions or macros defined in this
 *       file.
 */
#if (defined(MALLOC_DEBUG) && !defined(STANDALONE))
int __attribute__((format(printf, 4, 0))) __css_debug_str_helper(struct css_str **buf, ssize_t max_len,
							   int append, const char *fmt, va_list ap, const char *file, int lineno, const char *func);
#define __css_str_helper(a,b,c,d,e)	__css_debug_str_helper(a,b,c,d,e,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#else
int __attribute__((format(printf, 4, 0))) __css_str_helper(struct css_str **buf, ssize_t max_len,
							   int append, const char *fmt, va_list ap);
#endif
char *__css_str_helper2(struct css_str **buf, ssize_t max_len,
	const char *src, size_t maxsrc, int append, int escapecommas);

/*!
 * \brief Set a dynamic string from a va_list
 *
 * \param buf This is the address of a pointer to a struct css_str.
 *	If it is retrieved using css_str_thread_get, the
	struct css_threadstorage pointer will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accommodate a longer string than what it currently has space for.
 * \param max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 * \param fmt This is the format string (printf style)
 * \param ap This is the va_list
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * Example usage (the first part is only for thread-local storage)
 * \code
 * CSS_THREADSTORAGE(my_str, my_str_init);
 * #define MY_STR_INIT_SIZE   128
 * ...
 * void my_func(const char *fmt, ...)
 * {
 *      struct css_str *buf;
 *      va_list ap;
 *
 *      if (!(buf = css_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
 *           return;
 *      ...
 *      va_start(fmt, ap);
 *      css_str_set_va(&buf, 0, fmt, ap);
 *      va_end(ap);
 * 
 *      printf("This is the string we just built: %s\n", buf->str);
 *      ...
 * }
 * \endcode
 */
CSS_INLINE_API(int __attribute__((format(printf, 3, 0))) css_str_set_va(struct css_str **buf, ssize_t max_len, const char *fmt, va_list ap),
{
	return __css_str_helper(buf, max_len, 0, fmt, ap);
}
)

/*!
 * \brief Append to a dynamic string using a va_list
 *
 * Same as css_str_set_va(), but append to the current content.
 */
CSS_INLINE_API(int __attribute__((format(printf, 3, 0))) css_str_append_va(struct css_str **buf, ssize_t max_len, const char *fmt, va_list ap),
{
	return __css_str_helper(buf, max_len, 1, fmt, ap);
}
)

/*!\brief Set a dynamic string to a non-NULL terminated substring. */
CSS_INLINE_API(char *css_str_set_substr(struct css_str **buf, ssize_t maxlen, const char *src, size_t maxsrc),
{
	return __css_str_helper2(buf, maxlen, src, maxsrc, 0, 0);
}
)

/*!\brief Append a non-NULL terminated substring to the end of a dynamic string. */
CSS_INLINE_API(char *css_str_append_substr(struct css_str **buf, ssize_t maxlen, const char *src, size_t maxsrc),
{
	return __css_str_helper2(buf, maxlen, src, maxsrc, 1, 0);
}
)

/*!\brief Set a dynamic string to a non-NULL terminated substring, with escaping of commas. */
CSS_INLINE_API(char *css_str_set_escapecommas(struct css_str **buf, ssize_t maxlen, const char *src, size_t maxsrc),
{
	return __css_str_helper2(buf, maxlen, src, maxsrc, 0, 1);
}
)

/*!\brief Append a non-NULL terminated substring to the end of a dynamic string, with escaping of commas. */
CSS_INLINE_API(char *css_str_append_escapecommas(struct css_str **buf, ssize_t maxlen, const char *src, size_t maxsrc),
{
	return __css_str_helper2(buf, maxlen, src, maxsrc, 1, 1);
}
)

/*!
 * \brief Set a dynamic string using variable arguments
 *
 * \param buf This is the address of a pointer to a struct css_str which should
 *      have been retrieved using css_str_thread_get.  It will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accomodate a longer string than what it currently has space for.
 * \param max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 *	If set to -1, we are bound to the current maximum length.
 * \param fmt This is the format string (printf style)
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * All the rest is the same as css_str_set_va()
 */
CSS_INLINE_API(
int __attribute__((format(printf, 3, 4))) css_str_set(
	struct css_str **buf, ssize_t max_len, const char *fmt, ...),
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = css_str_set_va(buf, max_len, fmt, ap);
	va_end(ap);

	return res;
}
)

/*!
 * \brief Append to a thread local dynamic string
 *
 * The arguments, return values, and usage of this function are the same as
 * css_str_set(), but the new data is appended to the current value.
 */
CSS_INLINE_API(
int __attribute__((format(printf, 3, 4))) css_str_append(
	struct css_str **buf, ssize_t max_len, const char *fmt, ...),
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = css_str_append_va(buf, max_len, fmt, ap);
	va_end(ap);

	return res;
}
)

/*!
 * \brief Compute a hash value on a string
 *
 * This famous hash algorithm was written by Dan Bernstein and is
 * commonly used.
 *
 * http://www.cse.yorku.ca/~oz/hash.html
 */
static force_inline int attribute_pure css_str_hash(const char *str)
{
	int hash = 5381;

	while (*str)
		hash = hash * 33 ^ *str++;

	return abs(hash);
}

/*!
 * \brief Compute a hash value on a string
 *
 * \param[in] str The string to add to the hash
 * \param[in] hash The hash value to add to
 * 
 * \details
 * This version of the function is for when you need to compute a
 * string hash of more than one string.
 *
 * This famous hash algorithm was written by Dan Bernstein and is
 * commonly used.
 *
 * \sa http://www.cse.yorku.ca/~oz/hash.html
 */
static force_inline int css_str_hash_add(const char *str, int hash)
{
	while (*str)
		hash = hash * 33 ^ *str++;

	return abs(hash);
}

/*!
 * \brief Compute a hash value on a case-insensitive string
 *
 * Uses the same hash algorithm as css_str_hash, but converts
 * all characters to lowercase prior to computing a hash. This
 * allows for easy case-insensitive lookups in a hash table.
 */
static force_inline int attribute_pure css_str_case_hash(const char *str)
{
	int hash = 5381;

	while (*str) {
		hash = hash * 33 ^ tolower(*str++);
	}

	return abs(hash);
}

#ifdef	__cplusplus
}
#endif

#endif	/* STRING_H */

