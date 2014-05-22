/* 
 * File:   lock.h
 * Author: root
 *
 * Created on April 17, 2014, 3:01 AM
 */

#ifndef LOCK_H
#define	LOCK_H

#include <pthread.h>

#include <sys/time.h>
#include <sys/param.h>
#ifdef HAVE_BKTR
#include <execinfo.h>
#endif

//BY SELF
#define HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK 1
#define HAVE_PTHREAD_RWLOCK_INITIALIZER 1    
#ifndef HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK 
#include "time.h"
#endif

#include "inline_api.h"
#include "compiler.h"
#include "logger.h"

#define CSS_PTHREADT_NULL (pthread_t) -1
#define CSS_PTHREADT_STOP (pthread_t) -2


#if (defined(SOLARIS) || defined(BSD))
#define CSS_MUTEX_INIT_W_CONSTRUCTORS
#endif /* SOLARIS || BSD */

/* Ceictims REQUIRES recursive (not error checking) mutexes
   and will not run without them. */
#if defined(HAVE_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP) && defined(HAVE_PTHREAD_MUTEX_RECURSIVE_NP)
#define PTHREAD_MUTEX_INIT_VALUE	PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define CSS_MUTEX_KIND			PTHREAD_MUTEX_RECURSIVE_NP
#else
#define PTHREAD_MUTEX_INIT_VALUE	PTHREAD_MUTEX_INITIALIZER
#define CSS_MUTEX_KIND			PTHREAD_MUTEX_RECURSIVE
#endif /* PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP */

#ifdef HAVE_PTHREAD_RWLOCK_INITIALIZER
#define __CSS_RWLOCK_INIT_VALUE		PTHREAD_RWLOCK_INITIALIZER
#else  /* HAVE_PTHREAD_RWLOCK_INITIALIZER */
#define __CSS_RWLOCK_INIT_VALUE		{0}
#endif /* HAVE_PTHREAD_RWLOCK_INITIALIZER */

#ifdef HAVE_BKTR
#define CSS_LOCK_TRACK_INIT_VALUE { { NULL }, { 0 }, 0, { NULL }, { 0 }, {{{ 0 }}}, PTHREAD_MUTEX_INIT_VALUE }
#else
#define CSS_LOCK_TRACK_INIT_VALUE { { NULL }, { 0 }, 0, { NULL }, { 0 }, PTHREAD_MUTEX_INIT_VALUE }
#endif

#define CSS_MUTEX_INIT_VALUE { PTHREAD_MUTEX_INIT_VALUE, NULL, 1 }
#define CSS_MUTEX_INIT_VALUE_NOTRACKING { PTHREAD_MUTEX_INIT_VALUE, NULL, 0 }

#define CSS_RWLOCK_INIT_VALUE { __CSS_RWLOCK_INIT_VALUE, NULL, 1 }
#define CSS_RWLOCK_INIT_VALUE_NOTRACKING { __CSS_RWLOCK_INIT_VALUE, NULL, 0 }

#define CSS_MAX_REENTRANCY 10

//struct css_channel;

struct css_lock_track {
	const char *file[CSS_MAX_REENTRANCY];
	int lineno[CSS_MAX_REENTRANCY];
	int reentrancy;
	const char *func[CSS_MAX_REENTRANCY];
	pthread_t thread[CSS_MAX_REENTRANCY];
#ifdef HAVE_BKTR
	struct css_bt backtrace[CSS_MAX_REENTRANCY];
#endif
	pthread_mutex_t reentr_mutex;
};

/*! \brief Structure for mutex and tracking information.
 *
 * We have tracking information in this structure regardless of DEBUG_THREADS being enabled.
 * The information will just be ignored in the core if a module does not request it..
 */
struct css_mutex_info {
	pthread_mutex_t mutex;
	/*! Track which thread holds this mutex */
	struct css_lock_track *track;
	unsigned int tracking:1;
};

/*! \brief Structure for rwlock and tracking information.
 *
 * We have tracking information in this structure regardless of DEBUG_THREADS being enabled.
 * The information will just be ignored in the core if a module does not request it..
 */
struct css_rwlock_info {
	pthread_rwlock_t lock;
	/*! Track which thread holds this lock */
	struct css_lock_track *track;
	unsigned int tracking:1;
};

typedef struct css_mutex_info css_mutex_t;

typedef struct css_rwlock_info css_rwlock_t;

typedef pthread_cond_t css_cond_t;

int __css_pthread_mutex_init(int tracking, const char *filename, int lineno, const char *func, const char *mutex_name, css_mutex_t *t);
int __css_pthread_mutex_destroy(const char *filename, int lineno, const char *func, const char *mutex_name, css_mutex_t *t);
int __css_pthread_mutex_lock(const char *filename, int lineno, const char *func, const char* mutex_name, css_mutex_t *t);
int __css_pthread_mutex_trylock(const char *filename, int lineno, const char *func, const char* mutex_name, css_mutex_t *t);
int __css_pthread_mutex_unlock(const char *filename, int lineno, const char *func, const char *mutex_name, css_mutex_t *t);

#define css_mutex_init(pmutex)            __css_pthread_mutex_init(1, __FILE__, __LINE__, __PRETTY_FUNCTION__, #pmutex, pmutex)
#define css_mutex_init_notracking(pmutex) __css_pthread_mutex_init(0, __FILE__, __LINE__, __PRETTY_FUNCTION__, #pmutex, pmutex)
#define css_mutex_destroy(a)              __css_pthread_mutex_destroy(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define css_mutex_lock(a)                 __css_pthread_mutex_lock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define css_mutex_unlock(a)               __css_pthread_mutex_unlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)
#define css_mutex_trylock(a)              __css_pthread_mutex_trylock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a)


int __css_cond_init(const char *filename, int lineno, const char *func, const char *cond_name, css_cond_t *cond, pthread_condattr_t *cond_attr);
int __css_cond_signal(const char *filename, int lineno, const char *func, const char *cond_name, css_cond_t *cond);
int __css_cond_broadcast(const char *filename, int lineno, const char *func, const char *cond_name, css_cond_t *cond);
int __css_cond_destroy(const char *filename, int lineno, const char *func, const char *cond_name, css_cond_t *cond);
int __css_cond_wait(const char *filename, int lineno, const char *func, const char *cond_name, const char *mutex_name, css_cond_t *cond, css_mutex_t *t);
int __css_cond_timedwait(const char *filename, int lineno, const char *func, const char *cond_name, const char *mutex_name, css_cond_t *cond, css_mutex_t *t, const struct timespec *abstime);

#define css_cond_init(cond, attr)             __css_cond_init(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, cond, attr)
#define css_cond_destroy(cond)                __css_cond_destroy(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, cond)
#define css_cond_signal(cond)                 __css_cond_signal(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, cond)
#define css_cond_broadcast(cond)              __css_cond_broadcast(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, cond)
#define css_cond_wait(cond, mutex)            __css_cond_wait(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, #mutex, cond, mutex)
#define css_cond_timedwait(cond, mutex, time) __css_cond_timedwait(__FILE__, __LINE__, __PRETTY_FUNCTION__, #cond, #mutex, cond, mutex, time)


int __css_rwlock_init(int tracking, const char *filename, int lineno, const char *func, const char *rwlock_name, css_rwlock_t *t);
int __css_rwlock_destroy(const char *filename, int lineno, const char *func, const char *rwlock_name, css_rwlock_t *t);
int __css_rwlock_unlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name);
int __css_rwlock_rdlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name);
int __css_rwlock_wrlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name);
int __css_rwlock_timedrdlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name, const struct timespec *abs_timeout);
int __css_rwlock_timedwrlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name, const struct timespec *abs_timeout);
int __css_rwlock_tryrdlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name);
int __css_rwlock_trywrlock(const char *filename, int lineno, const char *func, css_rwlock_t *t, const char *name);

/*!
 * \brief wrapper for rwlock with tracking enabled
 * \return 0 on success, non zero for error
 * \since 1.6.1
 */
#define css_rwlock_init(rwlock)            __css_rwlock_init(1, __FILE__, __LINE__, __PRETTY_FUNCTION__, #rwlock, rwlock)

/*!
 * \brief wrapper for css_rwlock_init with tracking disabled
 * \return 0 on success, non zero for error
 * \since 1.6.1
 */
#define css_rwlock_init_notracking(rwlock) __css_rwlock_init(0, __FILE__, __LINE__, __PRETTY_FUNCTION__, #rwlock, rwlock)

#define css_rwlock_destroy(rwlock)         __css_rwlock_destroy(__FILE__, __LINE__, __PRETTY_FUNCTION__, #rwlock, rwlock)
#define css_rwlock_unlock(a)               __css_rwlock_unlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a)
#define css_rwlock_rdlock(a)               __css_rwlock_rdlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a)
#define css_rwlock_wrlock(a)               __css_rwlock_wrlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a)
#define css_rwlock_tryrdlock(a)            __css_rwlock_tryrdlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a)
#define css_rwlock_trywrlock(a)            __css_rwlock_trywrlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a)
#define css_rwlock_timedrdlock(a, b)       __css_rwlock_timedrdlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a, b)
#define css_rwlock_timedwrlock(a, b)       __css_rwlock_timedwrlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, #a, b)

#define	ROFFSET	((lt->reentrancy > 0) ? (lt->reentrancy-1) : 0)

#ifdef DEBUG_THREADS

#define _ css_mutex_logger(...)  do { if (canlog) css_log(LOG_ERROR, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__); } while (0)

#ifdef THREAD_CRASH
#define DO_THREAD_CRASH do { *((int *)(0)) = 1; } while(0)
#else
#define DO_THREAD_CRASH do { } while (0)
#endif

#include <errno.h>

enum css_lock_type {
	CSS_MUTEX,
	CSS_RDLOCK,
	CSS_WRLOCK,
};

/*!
 * \brief Store lock info for the current thread
 *
 * This function gets called in css_mutex_lock() and css_mutex_trylock() so
 * that information about this lock can be stored in this thread's
 * lock info struct.  The lock is marked as pending as the thread is waiting
 * on the lock.  css_mark_lock_acquired() will mark it as held by this thread.
 */
#if !defined(LOW_MEMORY)
#ifdef HAVE_BKTR
void css_store_lock_info(enum css_lock_type type, const char *filename,
	int line_num, const char *func, const char *lock_name, void *lock_addr, struct css_bt *bt);
#else
void css_store_lock_info(enum css_lock_type type, const char *filename,
	int line_num, const char *func, const char *lock_name, void *lock_addr);
#endif /* HAVE_BKTR */

#else

#ifdef HAVE_BKTR
#define css_store_lock_info(I,DONT,CARE,ABOUT,THE,PARAMETERS,BUD)
#else
#define css_store_lock_info(I,DONT,CARE,ABOUT,THE,PARAMETERS)
#endif /* HAVE_BKTR */
#endif /* !defined(LOW_MEMORY) */

/*!
 * \brief Mark the  css lock as acquired
 */
#if !defined(LOW_MEMORY)
void css_mark_lock_acquired(void *lock_addr);
#else
#define css_mark_lock_acquired(ignore)
#endif

/*!
 * \brief Mark the  css lock as failed (trylock)
 */
#if !defined(LOW_MEMORY)
void css_mark_lock_failed(void *lock_addr);
#else
#define css_mark_lock_failed(ignore)
#endif

/*!
 * \brief remove lock info for the current thread
 *
 * this gets called by css_mutex_unlock so that information on the lock can
 * be removed from the current thread's lock info struct.
 */
#if !defined(LOW_MEMORY)
#ifdef HAVE_BKTR
void css_remove_lock_info(void *lock_addr, struct css_bt *bt);
#else
void css_remove_lock_info(void *lock_addr);
#endif /* HAVE_BKTR */
#else
#ifdef HAVE_BKTR
#define css_remove_lock_info(ignore,me)
#else
#define css_remove_lock_info(ignore)
#endif /* HAVE_BKTR */
#endif /* !defined(LOW_MEMORY) */

#ifdef HAVE_BKTR
static inline void __dump_backtrace(struct css_bt *bt, int canlog)
{
	char **strings;

	ssize_t i;

	strings = backtrace_symbols(bt->addresses, bt->num_frames);

	for (i = 0; i < bt->num_frames; i++)
		_ css_mutex_logger("%s\n", strings[i]);

	free(strings);
}
#endif

/*!
 * \brief log info for the current lock with css_log().
 *
 * this function would be mostly for debug. If you come across a lock
 * that is unexpectedly but momentarily locked, and you wonder who
 * are fighting with for the lock, this routine could be called, IF
 * you have the thread debugging stuff turned on.
 * \param this_lock_addr lock address to return lock information
 * \since 1.6.1
 */
void log_show_lock(void *this_lock_addr);

/*!
 * \brief retrieve lock info for the specified mutex
 *
 * this gets called during deadlock avoidance, so that the information may
 * be preserved as to what location originally acquired the lock.
 */
#if !defined(LOW_MEMORY)
int css_find_lock_info(void *lock_addr, char *filename, size_t filename_size, int *lineno, char *func, size_t func_size, char *mutex_name, size_t mutex_name_size);
#else
#define css_find_lock_info(a,b,c,d,e,f,g,h) -1
#endif

/*!
 * \brief Unlock a lock briefly
 *
 * used during deadlock avoidance, to preserve the original location where
 * a lock was originally acquired.
 */
#define CHANNEL_DEADLOCK_AVOIDANCE(chan) \
	do { \
		char __filename[80], __func[80], __mutex_name[80]; \
		int __lineno; \
		int __res = css_find_lock_info(ao2_object_get_lockaddr(chan), __filename, sizeof(__filename), &__lineno, __func, sizeof(__func), __mutex_name, sizeof(__mutex_name)); \
		int __res2 = css_channel_unlock(chan); \
		usleep(1); \
		if (__res < 0) { /* Shouldn't ever happen, but just in case... */ \
			if (__res2) { \
			 css_log(LOG_WARNING, "Could not unlock channel '%s': %s and no lock info found!  I will NOT try to relock.\n", #chan, strerror(__res2)); \
			} else { \
			 css_channel_lock(chan); \
			} \
		} else { \
			if (__res2) { \
			 css_log(LOG_WARNING, "Could not unlock channel '%s': %s.  {{{Originally locked at %s line %d: (%s) '%s'}}}  I will NOT try to relock.\n", #chan, strerror(__res2), __filename, __lineno, __func, __mutex_name); \
			} else { \
				__ao2_lock(chan, __filename, __func, __lineno, __mutex_name); \
			} \
		} \
	} while (0)

#define DEADLOCK_AVOIDANCE(lock) \
	do { \
		char __filename[80], __func[80], __mutex_name[80]; \
		int __lineno; \
		int __res = css_find_lock_info(lock, __filename, sizeof(__filename), &__lineno, __func, sizeof(__func), __mutex_name, sizeof(__mutex_name)); \
		int __res2 = css_mutex_unlock(lock); \
		usleep(1); \
		if (__res < 0) { /* Shouldn't ever happen, but just in case... */ \
			if (__res2 == 0) { \
			 css_mutex_lock(lock); \
			} else { \
			 css_log(LOG_WARNING, "Could not unlock mutex '%s': %s and no lock info found!  I will NOT try to relock.\n", #lock, strerror(__res2)); \
			} \
		} else { \
			if (__res2 == 0) { \
				_ css_pthread_mutex_lock(__filename, __lineno, __func, __mutex_name, lock); \
			} else { \
			 css_log(LOG_WARNING, "Could not unlock mutex '%s': %s.  {{{Originally locked at %s line %d: (%s) '%s'}}}  I will NOT try to relock.\n", #lock, strerror(__res2), __filename, __lineno, __func, __mutex_name); \
			} \
		} \
	} while (0)

/*!
 * \brief Deadlock avoidance unlock
 *
 * In certain deadlock avoidance scenarios, there is more than one lock to be
 * unlocked and relocked.  Therefore, this pair of macros is provided for that
 * purpose.  Note that every DLA_UNLOCK _MUST_ be paired with a matching
 * DLA_LOCK.  The intent of this pair of macros is to be used around another
 * set of deadlock avoidance code, mainly CHANNEL_DEADLOCK_AVOIDANCE, as the
 * locking order specifies that we may safely lock a channel, followed by its
 * pvt, with no worries about a deadlock.  In any other scenario, this macro
 * may not be safe to use.
 */
#define DLA_UNLOCK(lock) \
	do { \
		char __filename[80], __func[80], __mutex_name[80]; \
		int __lineno; \
		int __res = css_find_lock_info(lock, __filename, sizeof(__filename), &__lineno, __func, sizeof(__func), __mutex_name, sizeof(__mutex_name)); \
		int __res2 = css_mutex_unlock(lock);

/*!
 * \brief Deadlock avoidance lock
 *
 * In certain deadlock avoidance scenarios, there is more than one lock to be
 * unlocked and relocked.  Therefore, this pair of macros is provided for that
 * purpose.  Note that every DLA_UNLOCK _MUST_ be paired with a matching
 * DLA_LOCK.  The intent of this pair of macros is to be used around another
 * set of deadlock avoidance code, mainly CHANNEL_DEADLOCK_AVOIDANCE, as the
 * locking order specifies that we may safely lock a channel, followed by its
 * pvt, with no worries about a deadlock.  In any other scenario, this macro
 * may not be safe to use.
 */
#define DLA_LOCK(lock) \
		if (__res < 0) { /* Shouldn't ever happen, but just in case... */ \
			if (__res2) { \
			 css_log(LOG_WARNING, "Could not unlock mutex '%s': %s and no lock info found!  I will NOT try to relock.\n", #lock, strerror(__res2)); \
			} else { \
			 css_mutex_lock(lock); \
			} \
		} else { \
			if (__res2) { \
			 css_log(LOG_WARNING, "Could not unlock mutex '%s': %s.  {{{Originally locked at %s line %d: (%s) '%s'}}}  I will NOT try to relock.\n", #lock, strerror(__res2), __filename, __lineno, __func, __mutex_name); \
			} else { \
				_ css_pthread_mutex_lock(__filename, __lineno, __func, __mutex_name, lock); \
			} \
		} \
	} while (0)

static inline void css_reentrancy_lock(struct css_lock_track *lt)
{
	pthread_mutex_lock(&lt->reentr_mutex);
}

static inline void css_reentrancy_unlock(struct css_lock_track *lt)
{
	pthread_mutex_unlock(&lt->reentr_mutex);
}

static inline void css_reentrancy_init(struct css_lock_track **plt)
{
	int i;
	pthread_mutexattr_t reentr_attr;
	struct css_lock_track *lt = *plt;

	if (!lt) {
		lt = *plt = (struct css_lock_track *) calloc(1, sizeof(*lt));
	}

	for (i = 0; i < CSS_MAX_REENTRANCY; i++) {
		lt->file[i] = NULL;
		lt->lineno[i] = 0;
		lt->func[i] = NULL;
		lt->thread[i] = 0;
#ifdef HAVE_BKTR
		memset(&lt->backtrace[i], 0, sizeof(lt->backtrace[i]));
#endif
	}

	lt->reentrancy = 0;

	pthread_mutexattr_init(&reentr_attr);
	pthread_mutexattr_settype(&reentr_attr, CSS_MUTEX_KIND);
	pthread_mutex_init(&lt->reentr_mutex, &reentr_attr);
	pthread_mutexattr_destroy(&reentr_attr);
}

static inline void delete_reentrancy_cs(struct css_lock_track **plt)
{
	struct css_lock_track *lt;
	if (*plt) {
		lt = *plt;
		pthread_mutex_destroy(&lt->reentr_mutex);
		free(lt);
		*plt = NULL;
	}
}

#else /* !DEBUG_THREADS */

#define	CHANNEL_DEADLOCK_AVOIDANCE(chan) \
 css_channel_unlock(chan); \
	usleep(1); \
 css_channel_lock(chan);

#define	DEADLOCK_AVOIDANCE(lock) \
	do { \
		int __res; \
		if (!(__res = css_mutex_unlock(lock))) { \
			usleep(1); \
		 css_mutex_lock(lock); \
		} else { \
		 css_log(LOG_WARNING, "Failed to unlock mutex '%s' (%s).  I will NOT try to relock. {{{ THIS IS A BUG. }}}\n", #lock, strerror(__res)); \
		} \
	} while (0)

#define DLA_UNLOCK(lock) css_mutex_unlock(lock)

#define DLA_LOCK(lock) css_mutex_lock(lock)

#endif /* !DEBUG_THREADS */

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS)
/*
 * If CSS_MUTEX_INIT_W_CONSTRUCTORS is defined, use file scope constructors
 * and destructors to create/destroy global mutexes.
 */
#define __CSS_MUTEX_DEFINE(scope, mutex, init_val, track)	\
	scope css_mutex_t mutex = init_val;			\
static void  __attribute__((constructor)) init_##mutex(void)	\
{								\
	if (track)						\
	 css_mutex_init(&mutex);				\
	else							\
	 css_mutex_init_notracking(&mutex);		\
}								\
								\
static void  __attribute__((destructor)) fini_##mutex(void)	\
{								\
 css_mutex_destroy(&mutex);				\
}
#else /* !CSS_MUTEX_INIT_W_CONSTRUCTORS */
/* By default, use static initialization of mutexes. */
#define __CSS_MUTEX_DEFINE(scope, mutex, init_val, track) scope css_mutex_t mutex = init_val
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

#define CSS_MUTEX_DEFINE_STATIC(mutex) __CSS_MUTEX_DEFINE(static, mutex, CSS_MUTEX_INIT_VALUE, 1)
#define CSS_MUTEX_DEFINE_STATIC_NOTRACKING(mutex) __CSS_MUTEX_DEFINE(static, mutex, CSS_MUTEX_INIT_VALUE_NOTRACKING, 0)


/* Statically declared read/write locks */
#ifdef CSS_MUTEX_INIT_W_CONSTRUCTORS
#define __CSS_RWLOCK_DEFINE(scope, rwlock, init_val, track) \
        scope css_rwlock_t rwlock = init_val; \
static void  __attribute__((constructor)) init_##rwlock(void) \
{ \
	if (track) \
         css_rwlock_init(&rwlock); \
	else \
	 css_rwlock_init_notracking(&rwlock); \
} \
static void  __attribute__((destructor)) fini_##rwlock(void) \
{ \
        css_rwlock_destroy(&rwlock); \
}
#else
#define __CSS_RWLOCK_DEFINE(scope, rwlock, init_val, track) scope css_rwlock_t rwlock = init_val
#endif

#define CSS_RWLOCK_DEFINE_STATIC(rwlock) __CSS_RWLOCK_DEFINE(static, rwlock, CSS_RWLOCK_INIT_VALUE, 1)
#define CSS_RWLOCK_DEFINE_STATIC_NOTRACKING(rwlock) __CSS_RWLOCK_DEFINE(static, rwlock, CSS_RWLOCK_INIT_VALUE_NOTRACKING, 0)

#ifndef __CYGWIN__	/* temporary disabled for cygwin */
#define pthread_mutex_t		use_css_mutex_t_instead_of_pthread_mutex_t
#define pthread_cond_t		use_css_cond_t_instead_of_pthread_cond_t
#endif
#define pthread_mutex_lock	use_css_mutex_lock_instead_of_pthread_mutex_lock
#define pthread_mutex_unlock	use_css_mutex_unlock_instead_of_pthread_mutex_unlock
#define pthread_mutex_trylock	use_css_mutex_init_instead_of_pthread_mutex_init
#define pthread_mutex_destroy	use_css_mutex_destroy_instead_of_pthread_mutex_destroy
#define pthread_cond_init	use_css_cond_init_instead_of_pthread_cond_init
#define pthread_cond_destroy	use_css_cond_destroy_instead_of_pthread_cond_destroy
#define pthread_cond_signal	use_css_cond_signal_instead_of_pthread_cond_signal
#define pthread_cond_broadcast	use_css_cond_broadast_instead_of_pthread_cond_broadast
#define pthread_cond_wait	use_css_cond_wait_instead_of_pthread_cond_wait
#define pthread_cond_timedwait	use_css_cond_timedwait_instead_of_pthread_cond_timedwait

#define CSS_MUTEX_INITIALIZER __use_CSS_MUTEX_DEFINE_STATIC_rather_than_CSS_MUTEX_INITIALIZER__

//#define gethostbyname __gethostbyname__is__not__reentrant__use_ css_gethostbyname__instead__

#ifndef __linux__
#define pthread_create __use css_pthread_create_instead__
#endif

/*
 * Support for atomic instructions.
 * For platforms that have it, use the native cpu instruction to
 * implement them. For other platforms, resort to a 'slow' version
 * (defined in utils.c) that protects the atomic instruction with
 * a single lock.
 * The slow versions is always available, for testing purposes,
 * as css_atomic_fetchadd_int_slow()
 */

int css_atomic_fetchadd_int_slow(volatile int *p, int v);

#include "inline_api.h"

#if defined(HAVE_OSX_ATOMICS)
#include "libkern/OSAtomic.h"
#endif

/*! \brief Atomically add v to *p and return * the previous value of *p.
 * This can be used to handle reference counts, and the return value
 * can be used to generate unique identifiers.
 */

#if defined(HAVE_GCC_ATOMICS)
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	return __sync_fetch_and_add(p, v);
})
#elif defined(HAVE_OSX_ATOMICS) && (SIZEOF_INT == 4)
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	return OSAtomicAdd32(v, (int32_t *) p) - v;
})
#elif defined(HAVE_OSX_ATOMICS) && (SIZEOF_INT == 8)
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	return OSAtomicAdd64(v, (int64_t *) p) - v;
#elif defined (__i386__) || defined(__x86_64__)
#ifdef sun
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	__asm __volatile (
	"       lock;  xaddl   %0, %1 ;        "
	: "+r" (v),                     /* 0 (result) */
	  "=m" (*p)                     /* 1 */
	: "m" (*p));                    /* 2 */
	return (v);
})
#else /* ifndef sun */
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	__asm __volatile (
	"       lock   xaddl   %0, %1 ;        "
	: "+r" (v),                     /* 0 (result) */
	  "=m" (*p)                     /* 1 */
	: "m" (*p));                    /* 2 */
	return (v);
})
#endif
#else   /* low performance version in utils.c */
CSS_INLINE_API(int css_atomic_fetchadd_int(volatile int *p, int v),
{
	return css_atomic_fetchadd_int_slow(p, v);
})
#endif

/*! \brief decrement *p by 1 and return true if the variable has reached 0.
 * Useful e.g. to check if a refcount has reached 0.
 */
#if defined(HAVE_GCC_ATOMICS)
CSS_INLINE_API(int css_atomic_dec_and_test(volatile int *p),
{
	return __sync_sub_and_fetch(p, 1) == 0;
})
#elif defined(HAVE_OSX_ATOMICS) && (SIZEOF_INT == 4)
CSS_INLINE_API(int css_atomic_dec_and_test(volatile int *p),
{
	return OSAtomicAdd32( -1, (int32_t *) p) == 0;
})
#elif defined(HAVE_OSX_ATOMICS) && (SIZEOF_INT == 8)
CSS_INLINE_API(int css_atomic_dec_and_test(volatile int *p),
{
	return OSAtomicAdd64( -1, (int64_t *) p) == 0;
#else
CSS_INLINE_API(int css_atomic_dec_and_test(volatile int *p),
{
	int a = css_atomic_fetchadd_int(p, -1);
	return a == 1; /* true if the value is 0 now (so it was 1 previously) */
})
#endif

#endif	/* LOCK_H */

