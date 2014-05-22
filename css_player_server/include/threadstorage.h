/* 
 * File:   threadstorage.h
 * Author: root
 *
 * Created on April 17, 2014, 11:10 PM
 */

#ifndef THREADSTORAGE_H
#define	THREADSTORAGE_H

#ifdef	__cplusplus
extern "C" {
#endif

/*!
 * \file threadstorage.h
 * \author Russell Bryant <russell@digium.com>
 * \brief Definitions to aid in the use of thread local storage
 *
 * \arg \ref AstThreadStorage
 */

/*!
 * \page AstThreadStorage The Ceictims Thread Storage API
 *
 *
 * The POSIX threads (pthreads) API provides the ability to define thread
 * specific data.  The functions and structures defined here are intended
 * to centralize the code that is commonly used when using thread local
 * storage.
 *
 * The motivation for using this code in Ceictims is for situations where
 * storing data on a thread-specific basis can provide some amount of
 * performance benefit.  For example, there are some call types in Ceictims
 * where css_frame structures must be allocated very rapidly (easily 50, 100,
 * 200 times a second).  Instead of doing the equivalent of that many calls
 * to malloc() and free() per second, thread local storage is used to keep a
 * list of unused frame structures so that they can be continuously reused.
 *
 * - \ref threadstorage.h
 */

#include <pthread.h>
    
#include "utils.h"
#include "inline_api.h"
 

/*!
 * \brief data for a thread locally stored variable
 */
struct css_threadstorage {
	pthread_once_t once;	/*!< Ensure that the key is only initialized by one thread */
	pthread_key_t key;	/*!< The key used to retrieve this thread's data */
	void (*key_init)(void);	/*!< The function that initializes the key */
	int (*custom_init)(void *); /*!< Custom initialization function specific to the object */
};

#if defined(DEBUG_THREADLOCALS)
void __css_threadstorage_object_add(void *key, size_t len, const char *file, const char *function, unsigned int line);
void __css_threadstorage_object_remove(void *key);
void __css_threadstorage_object_replace(void *key_old, void *key_new, size_t len);
#endif /* defined(DEBUG_THREADLOCALS) */

/*!
 * \brief Define a thread storage variable
 *
 * \param name The name of the thread storage object
 *
 * This macro would be used to declare an instance of thread storage in a file.
 *
 * Example usage:
 * \code
 * CSS_THREADSTORAGE(my_buf);
 * \endcode
 */
#define CSS_THREADSTORAGE(name) \
	CSS_THREADSTORAGE_CUSTOM_SCOPE(name, NULL, css_free_ptr, static) 
#define CSS_THREADSTORAGE_PUBLIC(name) \
	CSS_THREADSTORAGE_CUSTOM_SCOPE(name, NULL, css_free_ptr,) 
#define CSS_THREADSTORAGE_EXTERNAL(name) \
	extern struct css_threadstorage name

/*!
 * \brief Define a thread storage variable, with custom initialization and cleanup
 *
 * \param a The name of the thread storage object
 * \param b This is a custom function that will be called after each thread specific
 *           object is allocated, with the allocated block of memory passed
 *           as the argument.
 * \param c This is a custom function that will be called instead of css_free
 *              when the thread goes away.  Note that if this is used, it *MUST*
 *              call free on the allocated memory.
 *
 * Example usage:
 * \code
 * CSS_THREADSTORAGE_CUSTOM(my_buf, my_init, my_cleanup);
 * \endcode
 */
#define CSS_THREADSTORAGE_CUSTOM(a,b,c)	CSS_THREADSTORAGE_CUSTOM_SCOPE(a,b,c,static)

#if defined(PTHREAD_ONCE_INIT_NEEDS_BRACES)
# define CSS_PTHREAD_ONCE_INIT { PTHREAD_ONCE_INIT }
#else
# define CSS_PTHREAD_ONCE_INIT PTHREAD_ONCE_INIT
#endif

#if !defined(DEBUG_THREADLOCALS)
#define CSS_THREADSTORAGE_CUSTOM_SCOPE(name, c_init, c_cleanup, scope)	\
static void __init_##name(void);                \
scope struct css_threadstorage name = {         \
	.once = CSS_PTHREAD_ONCE_INIT,              \
	.key_init = __init_##name,                  \
	.custom_init = c_init,                      \
};                                              \
static void __init_##name(void)                 \
{                                               \
	pthread_key_create(&(name).key, c_cleanup); \
}
#else /* defined(DEBUG_THREADLOCALS) */
#define CSS_THREADSTORAGE_CUSTOM_SCOPE(name, c_init, c_cleanup, scope) \
static void __init_##name(void);                \
scope struct css_threadstorage name = {         \
	.once = CSS_PTHREAD_ONCE_INIT,              \
	.key_init = __init_##name,                  \
	.custom_init = c_init,                      \
};                                              \
static void __cleanup_##name(void *data)        \
{                                               \
	__css_threadstorage_object_remove(data);    \
	c_cleanup(data);                            \
}                                               \
static void __init_##name(void)                 \
{                                               \
	pthread_key_create(&(name).key, __cleanup_##name); \
}
#endif /* defined(DEBUG_THREADLOCALS) */

/*!
 * \brief Retrieve thread storage
 *
 * \param ts This is a pointer to the thread storage structure declared by using
 *      the CSS_THREADSTORAGE macro.  If declared with 
 *      CSS_THREADSTORAGE(my_buf), then this argument would be (&my_buf).
 * \param init_size This is the amount of space to be allocated the first time
 *      this thread requests its data. Thus, this should be the size that the
 *      code accessing this thread storage is assuming the size to be.
 *
 * \return This function will return the thread local storage associated with
 *         the thread storage management variable passed as the first argument.
 *         The result will be NULL in the case of a memory allocation error.
 *
 * Example usage:
 * \code
 * CSS_THREADSTORAGE(my_buf);
 * #define MY_BUF_SIZE   128
 * ...
 * void my_func(const char *fmt, ...)
 * {
 *      void *buf;
 *
 *      if (!(buf = css_threadstorage_get(&my_buf, MY_BUF_SIZE)))
 *           return;
 *      ...
 * }
 * \endcode
 */
#if !defined(DEBUG_THREADLOCALS)
CSS_INLINE_API(
void *css_threadstorage_get(struct css_threadstorage *ts, size_t init_size),
{
	void *buf;

	pthread_once(&ts->once, ts->key_init);
	if (!(buf = pthread_getspecific(ts->key))) {
		if (!(buf = css_calloc(1, init_size)))
			return NULL;
		if (ts->custom_init && ts->custom_init(buf)) {
			free(buf);
			return NULL;
		}
		pthread_setspecific(ts->key, buf);
	}

	return buf;
}
)
#else /* defined(DEBUG_THREADLOCALS) */
CSS_INLINE_API(
void *__css_threadstorage_get(struct css_threadstorage *ts, size_t init_size, const char *file, const char *function, unsigned int line),
{
	void *buf;

	pthread_once(&ts->once, ts->key_init);
	if (!(buf = pthread_getspecific(ts->key))) {
		if (!(buf = css_calloc(1, init_size)))
			return NULL;
		if (ts->custom_init && ts->custom_init(buf)) {
			free(buf);
			return NULL;
		}
		pthread_setspecific(ts->key, buf);
		__css_threadstorage_object_add(buf, init_size, file, function, line);
	}

	return buf;
}
)

#define css_threadstorage_get(ts, init_size) __css_threadstorage_get(ts, init_size, __FILE__, __PRETTY_FUNCTION__, __LINE__)
#endif /* defined(DEBUG_THREADLOCALS) */


#ifdef	__cplusplus
}
#endif

#endif	/* THREADSTORAGE_H */

