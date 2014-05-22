/* 
 * File: css_player_background.c
 * Author: root
 *
 * Created on April 16, 2014, 6:29 PM
 */

#include "lock.h"

/* Allow direct use of pthread_mutex_* / pthread_cond_* */
#undef pthread_mutex_init
#undef pthread_mutex_destroy
#undef pthread_mutex_lock
#undef pthread_mutex_trylock
#undef pthread_mutex_t
#undef pthread_mutex_unlock
#undef pthread_cond_init
#undef pthread_cond_signal
#undef pthread_cond_broadcast
#undef pthread_cond_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait

int __css_pthread_mutex_init(int tracking, const char *filename, int lineno, const char *func,
						const char *mutex_name, css_mutex_t *t)
{
	int res;
	pthread_mutexattr_t  attr;

	t->track = NULL;
#ifdef DEBUG_THREADS
#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) != ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
/*
		int canlog = strcmp(filename, "logger.c") & track;
		__css_mutex_logger("%s line %d (%s): NOTICE: mutex '%s' is already initialized.\n",
				   filename, lineno, func, mutex_name);
		DO_THREAD_CRASH;
*/
		return 0;
	}

#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if ((t->tracking = tracking)) {
		css_reentrancy_init(&t->track);
	}
#endif /* DEBUG_THREADS */

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, CSS_MUTEX_KIND);

	res = pthread_mutex_init(&t->mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	return res;
}

int __css_pthread_mutex_destroy(const char *filename, int lineno, const char *func,
						const char *mutex_name, css_mutex_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		/* Don't try to uninitialize non initialized mutex
		 * This may no effect on linux
		 * And always ganerate core on *BSD with
		 * linked libpthread
		 * This not error condition if the mutex created on the fly.
		 */
		__css_mutex_logger("%s line %d (%s): NOTICE: mutex '%s' is uninitialized.\n",
				   filename, lineno, func, mutex_name);
		return 0;
	}
#endif

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	res = pthread_mutex_trylock(&t->mutex);
	switch (res) {
	case 0:
		pthread_mutex_unlock(&t->mutex);
		break;
	case EINVAL:
		__css_mutex_logger("%s line %d (%s): Error: attempt to destroy invalid mutex '%s'.\n",
				  filename, lineno, func, mutex_name);
		break;
	case EBUSY:
		__css_mutex_logger("%s line %d (%s): Error: attempt to destroy locked mutex '%s'.\n",
				   filename, lineno, func, mutex_name);
		if (t->tracking) {
			css_reentrancy_lock(lt);
			__css_mutex_logger("%s line %d (%s): Error: '%s' was locked here.\n",
				    lt->file[ROFFSET], lt->lineno[ROFFSET], lt->func[ROFFSET], mutex_name);
#ifdef HAVE_BKTR
			__dump_backtrace(&lt->backtrace[ROFFSET], canlog);
#endif
			css_reentrancy_unlock(lt);
		}
		break;
	}
#endif /* DEBUG_THREADS */

	res = pthread_mutex_destroy(&t->mutex);

#ifdef DEBUG_THREADS
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error destroying mutex %s: %s\n",
				   filename, lineno, func, mutex_name, strerror(res));
	}
	if (t->tracking) {
		css_reentrancy_lock(lt);
		lt->file[0] = filename;
		lt->lineno[0] = lineno;
		lt->func[0] = func;
		lt->reentrancy = 0;
		lt->thread[0] = 0;
#ifdef HAVE_BKTR
		memset(&lt->backtrace[0], 0, sizeof(lt->backtrace[0]));
#endif
		css_reentrancy_unlock(lt);
		delete_reentrancy_cs(&t->track);
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_pthread_mutex_lock(const char *filename, int lineno, const char *func,
                                           const char* mutex_name, css_mutex_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		/* Don't warn abount uninitialized mutex.
		 * Simple try to initialize it.
		 * May be not needed in linux system.
		 */
		res = __css_pthread_mutex_init(t->tracking, filename, lineno, func, mutex_name, t);
		if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
			__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized and unable to initialize.\n",
					 filename, lineno, func, mutex_name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t, bt);
#else
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t);
#endif
	}
#endif /* DEBUG_THREADS */

#if defined(DETECT_DEADLOCKS) && defined(DEBUG_THREADS)
	{
		time_t seconds = time(NULL);
		time_t wait_time, reported_wait = 0;
		do {
#ifdef	HAVE_MTX_PROFILE
			css_mark(mtx_prof, 1);
#endif
			res = pthread_mutex_trylock(&t->mutex);
#ifdef	HAVE_MTX_PROFILE
			css_mark(mtx_prof, 0);
#endif
			if (res == EBUSY) {
				wait_time = time(NULL) - seconds;
				if (wait_time > reported_wait && (wait_time % 5) == 0) {
					__css_mutex_logger("%s line %d (%s): Deadlock? waited %d sec for mutex '%s'?\n",
							   filename, lineno, func, (int) wait_time, mutex_name);
					css_reentrancy_lock(lt);
#ifdef HAVE_BKTR
					__dump_backtrace(&lt->backtrace[lt->reentrancy], canlog);
#endif
					__css_mutex_logger("%s line %d (%s): '%s' was locked here.\n",
							   lt->file[ROFFSET], lt->lineno[ROFFSET],
							   lt->func[ROFFSET], mutex_name);
#ifdef HAVE_BKTR
					__dump_backtrace(&lt->backtrace[ROFFSET], canlog);
#endif
					css_reentrancy_unlock(lt);
					reported_wait = wait_time;
				}
				usleep(200);
			}
		} while (res == EBUSY);
	}
#else /* !DETECT_DEADLOCKS || !DEBUG_THREADS */
#ifdef	HAVE_MTX_PROFILE
	css_mark(mtx_prof, 1);
	res = pthread_mutex_trylock(&t->mutex);
	css_mark(mtx_prof, 0);
	if (res)
#endif
	res = pthread_mutex_lock(&t->mutex);
#endif /* !DETECT_DEADLOCKS || !DEBUG_THREADS */

#ifdef DEBUG_THREADS
	if (t->tracking && !res) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = lineno;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		} else {
			__css_mutex_logger("%s line %d (%s): '%s' really deep reentrancy!\n",
							   filename, lineno, func, mutex_name);
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			css_reentrancy_lock(lt);
			bt = &lt->backtrace[lt->reentrancy-1];
			css_reentrancy_unlock(lt);
		} else {
			bt = NULL;
		}
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error obtaining mutex: %s\n",
				   filename, lineno, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_pthread_mutex_trylock(const char *filename, int lineno, const char *func,
                                              const char* mutex_name, css_mutex_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		/* Don't warn abount uninitialized mutex.
		 * Simple try to initialize it.
		 * May be not needed in linux system.
		 */
		res = __css_pthread_mutex_init(t->tracking, filename, lineno, func, mutex_name, t);
		if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
			__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized and unable to initialize.\n",
					 filename, lineno, func, mutex_name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t, bt);
#else
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_mutex_trylock(&t->mutex);

#ifdef DEBUG_THREADS
	if (t->tracking && !res) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = lineno;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		} else {
			__css_mutex_logger("%s line %d (%s): '%s' really deep reentrancy!\n",
					   filename, lineno, func, mutex_name);
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
		css_mark_lock_failed(t);
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_pthread_mutex_unlock(const char *filename, int lineno, const char *func,
					     const char *mutex_name, css_mutex_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized.\n",
				   filename, lineno, func, mutex_name);
		res = __css_pthread_mutex_init(t->tracking, filename, lineno, func, mutex_name, t);
		if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
			__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized and unable to initialize.\n",
					 filename, lineno, func, mutex_name);
		}
		return res;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy && (lt->thread[ROFFSET] != pthread_self())) {
			__css_mutex_logger("%s line %d (%s): attempted unlock mutex '%s' without owning it!\n",
					   filename, lineno, func, mutex_name);
			__css_mutex_logger("%s line %d (%s): '%s' was locked here.\n",
					   lt->file[ROFFSET], lt->lineno[ROFFSET], lt->func[ROFFSET], mutex_name);
#ifdef HAVE_BKTR
			__dump_backtrace(&lt->backtrace[ROFFSET], canlog);
#endif
			DO_THREAD_CRASH;
		}

		if (--lt->reentrancy < 0) {
			__css_mutex_logger("%s line %d (%s): mutex '%s' freed more times than we've locked!\n",
					   filename, lineno, func, mutex_name);
			lt->reentrancy = 0;
		}

		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = NULL;
			lt->lineno[lt->reentrancy] = 0;
			lt->func[lt->reentrancy] = NULL;
			lt->thread[lt->reentrancy] = 0;
		}

#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			bt = &lt->backtrace[lt->reentrancy - 1];
		}
#endif
		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_mutex_unlock(&t->mutex);

#ifdef DEBUG_THREADS
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error releasing mutex: %s\n",
				   filename, lineno, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}


int __css_cond_init(const char *filename, int lineno, const char *func,
				  const char *cond_name, css_cond_t *cond, pthread_condattr_t *cond_attr)
{
	return pthread_cond_init(cond, cond_attr);
}

int __css_cond_signal(const char *filename, int lineno, const char *func,
				    const char *cond_name, css_cond_t *cond)
{
	return pthread_cond_signal(cond);
}

int __css_cond_broadcast(const char *filename, int lineno, const char *func,
				       const char *cond_name, css_cond_t *cond)
{
	return pthread_cond_broadcast(cond);
}

int __css_cond_destroy(const char *filename, int lineno, const char *func,
				     const char *cond_name, css_cond_t *cond)
{
	return pthread_cond_destroy(cond);
}

int __css_cond_wait(const char *filename, int lineno, const char *func,
				  const char *cond_name, const char *mutex_name,
				  css_cond_t *cond, css_mutex_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized.\n",
				   filename, lineno, func, mutex_name);
		res = __css_pthread_mutex_init(t->tracking, filename, lineno, func, mutex_name, t);
		if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
			__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized and unable to initialize.\n",
					 filename, lineno, func, mutex_name);
		}
		return res;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy && (lt->thread[ROFFSET] != pthread_self())) {
			__css_mutex_logger("%s line %d (%s): attempted unlock mutex '%s' without owning it!\n",
					   filename, lineno, func, mutex_name);
			__css_mutex_logger("%s line %d (%s): '%s' was locked here.\n",
					   lt->file[ROFFSET], lt->lineno[ROFFSET], lt->func[ROFFSET], mutex_name);
#ifdef HAVE_BKTR
			__dump_backtrace(&lt->backtrace[ROFFSET], canlog);
#endif
			DO_THREAD_CRASH;
		}

		if (--lt->reentrancy < 0) {
			__css_mutex_logger("%s line %d (%s): mutex '%s' freed more times than we've locked!\n",
				   filename, lineno, func, mutex_name);
			lt->reentrancy = 0;
		}

		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = NULL;
			lt->lineno[lt->reentrancy] = 0;
			lt->func[lt->reentrancy] = NULL;
			lt->thread[lt->reentrancy] = 0;
		}

#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			bt = &lt->backtrace[lt->reentrancy - 1];
		}
#endif
		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_cond_wait(cond, &t->mutex);

#ifdef DEBUG_THREADS
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error waiting on condition mutex '%s'\n",
				   filename, lineno, func, strerror(res));
		DO_THREAD_CRASH;
	} else if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = lineno;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
#ifdef HAVE_BKTR
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
#endif
			lt->reentrancy++;
		} else {
			__css_mutex_logger("%s line %d (%s): '%s' really deep reentrancy!\n",
							   filename, lineno, func, mutex_name);
		}
		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t, bt);
#else
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t);
#endif
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_cond_timedwait(const char *filename, int lineno, const char *func, 
        const char *cond_name, const char *mutex_name, css_cond_t *cond, 
        css_mutex_t *t, const struct timespec *abstime)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
		__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized.\n",
				   filename, lineno, func, mutex_name);
		res = __css_pthread_mutex_init(t->tracking, filename, lineno, func, mutex_name, t);
		if ((t->mutex) == ((pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER)) {
			__css_mutex_logger("%s line %d (%s): Error: mutex '%s' is uninitialized and unable to initialize.\n",
					 filename, lineno, func, mutex_name);
		}
		return res;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy && (lt->thread[ROFFSET] != pthread_self())) {
			__css_mutex_logger("%s line %d (%s): attempted unlock mutex '%s' without owning it!\n",
					   filename, lineno, func, mutex_name);
			__css_mutex_logger("%s line %d (%s): '%s' was locked here.\n",
					   lt->file[ROFFSET], lt->lineno[ROFFSET], lt->func[ROFFSET], mutex_name);
#ifdef HAVE_BKTR
			__dump_backtrace(&lt->backtrace[ROFFSET], canlog);
#endif
			DO_THREAD_CRASH;
		}

		if (--lt->reentrancy < 0) {
			__css_mutex_logger("%s line %d (%s): mutex '%s' freed more times than we've locked!\n",
					   filename, lineno, func, mutex_name);
			lt->reentrancy = 0;
		}

		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = NULL;
			lt->lineno[lt->reentrancy] = 0;
			lt->func[lt->reentrancy] = NULL;
			lt->thread[lt->reentrancy] = 0;
		}
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			bt = &lt->backtrace[lt->reentrancy - 1];
		}
#endif
		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_cond_timedwait(cond, &t->mutex, abstime);

#ifdef DEBUG_THREADS
	if (res && (res != ETIMEDOUT)) {
		__css_mutex_logger("%s line %d (%s): Error waiting on condition mutex '%s'\n",
				   filename, lineno, func, strerror(res));
		DO_THREAD_CRASH;
	} else if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = lineno;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
#ifdef HAVE_BKTR
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
#endif
			lt->reentrancy++;
		} else {
			__css_mutex_logger("%s line %d (%s): '%s' really deep reentrancy!\n",
							   filename, lineno, func, mutex_name);
		}
		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t, bt);
#else
		css_store_lock_info(CSS_MUTEX, filename, lineno, func, mutex_name, t);
#endif
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_init(int tracking, const char *filename, int lineno, const char *func, const char *rwlock_name, css_rwlock_t *t)
{
	int res;
	pthread_rwlockattr_t attr;

#ifdef DEBUG_THREADS

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	int canlog = strcmp(filename, "logger.c") & t->tracking;

	if (t->lock != ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		__css_mutex_logger("%s line %d (%s): Warning: rwlock '%s' is already initialized.\n",
				filename, lineno, func, rwlock_name);
		return 0;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if ((t->tracking = tracking)) {
		css_reentrancy_init(&t->track);
	}
#endif /* DEBUG_THREADS */

	pthread_rwlockattr_init(&attr);

#ifdef HAVE_PTHREAD_RWLOCK_PREFER_WRITER_NP
	pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
#endif

	res = pthread_rwlock_init(&t->lock, &attr);
	pthread_rwlockattr_destroy(&attr);
	return res;
}

int __css_rwlock_destroy(const char *filename, int lineno, const char *func, const char *rwlock_name, css_rwlock_t *t)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt = t->track;
	int canlog = strcmp(filename, "logger.c") & t->tracking;

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if (t->lock == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		__css_mutex_logger("%s line %d (%s): Warning: rwlock '%s' is uninitialized.\n",
				   filename, lineno, func, rwlock_name);
		return 0;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

#endif /* DEBUG_THREADS */

	res = pthread_rwlock_destroy(&t->lock);

#ifdef DEBUG_THREADS
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error destroying rwlock %s: %s\n",
				filename, lineno, func, rwlock_name, strerror(res));
	}
	if (t->tracking) {
		css_reentrancy_lock(lt);
		lt->file[0] = filename;
		lt->lineno[0] = lineno;
		lt->func[0] = func;
		lt->reentrancy = 0;
		lt->thread[0] = 0;
#ifdef HAVE_BKTR
		memset(&lt->backtrace[0], 0, sizeof(lt->backtrace[0]));
#endif
		css_reentrancy_unlock(lt);
		delete_reentrancy_cs(&t->track);
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_unlock(const char *filename, int line, const char *func, css_rwlock_t *t, const char *name)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif
	int lock_found = 0;


#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		__css_mutex_logger("%s line %d (%s): Warning: rwlock '%s' is uninitialized.\n",
				   filename, line, func, name);
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
		}
		return res;
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy) {
			int i;
			pthread_t self = pthread_self();
			for (i = lt->reentrancy - 1; i >= 0; --i) {
				if (lt->thread[i] == self) {
					lock_found = 1;
					if (i != lt->reentrancy - 1) {
						lt->file[i] = lt->file[lt->reentrancy - 1];
						lt->lineno[i] = lt->lineno[lt->reentrancy - 1];
						lt->func[i] = lt->func[lt->reentrancy - 1];
						lt->thread[i] = lt->thread[lt->reentrancy - 1];
					}
#ifdef HAVE_BKTR
					bt = &lt->backtrace[i];
#endif
					lt->file[lt->reentrancy - 1] = NULL;
					lt->lineno[lt->reentrancy - 1] = 0;
					lt->func[lt->reentrancy - 1] = NULL;
					lt->thread[lt->reentrancy - 1] = CSS_PTHREADT_NULL;
					break;
				}
			}
		}

		if (lock_found && --lt->reentrancy < 0) {
			__css_mutex_logger("%s line %d (%s): rwlock '%s' freed more times than we've locked!\n",
					filename, line, func, name);
			lt->reentrancy = 0;
		}

		css_reentrancy_unlock(lt);

#ifdef HAVE_BKTR
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_rwlock_unlock(&t->lock);

#ifdef DEBUG_THREADS
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error releasing rwlock: %s\n",
				filename, line, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_rdlock(const char *filename, int line, const char *func, css_rwlock_t *t, const char *name)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_RDLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_RDLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

#if defined(DETECT_DEADLOCKS) && defined(DEBUG_THREADS)
	{
		time_t seconds = time(NULL);
		time_t wait_time, reported_wait = 0;
		do {
			res = pthread_rwlock_tryrdlock(&t->lock);
			if (res == EBUSY) {
				wait_time = time(NULL) - seconds;
				if (wait_time > reported_wait && (wait_time % 5) == 0) {
					__css_mutex_logger("%s line %d (%s): Deadlock? waited %d sec for readlock '%s'?\n",
						filename, line, func, (int)wait_time, name);
					if (t->tracking) {
						css_reentrancy_lock(lt);
#ifdef HAVE_BKTR
						__dump_backtrace(&lt->backtrace[lt->reentrancy], canlog);
#endif
						__css_mutex_logger("%s line %d (%s): '%s' was locked  here.\n",
								lt->file[lt->reentrancy-1], lt->lineno[lt->reentrancy-1],
								lt->func[lt->reentrancy-1], name);
#ifdef HAVE_BKTR
						__dump_backtrace(&lt->backtrace[lt->reentrancy-1], canlog);
#endif
						css_reentrancy_unlock(lt);
					}
					reported_wait = wait_time;
				}
				usleep(200);
			}
		} while (res == EBUSY);
	}
#else /* !DETECT_DEADLOCKS || !DEBUG_THREADS */
	res = pthread_rwlock_rdlock(&t->lock);
#endif /* !DETECT_DEADLOCKS || !DEBUG_THREADS */

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			css_reentrancy_lock(lt);
			bt = &lt->backtrace[lt->reentrancy-1];
			css_reentrancy_unlock(lt);
		} else {
			bt = NULL;
		}
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}

	if (res) {
		__css_mutex_logger("%s line %d (%s): Error obtaining read lock: %s\n",
				filename, line, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_wrlock(const char *filename, int line, const char *func, css_rwlock_t *t, const char *name)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

#if defined(DETECT_DEADLOCKS) && defined(DEBUG_THREADS)
	{
		time_t seconds = time(NULL);
		time_t wait_time, reported_wait = 0;
		do {
			res = pthread_rwlock_trywrlock(&t->lock);
			if (res == EBUSY) {
				wait_time = time(NULL) - seconds;
				if (wait_time > reported_wait && (wait_time % 5) == 0) {
					__css_mutex_logger("%s line %d (%s): Deadlock? waited %d sec for writelock '%s'?\n",
						filename, line, func, (int)wait_time, name);
					if (t->tracking) {
						css_reentrancy_lock(lt);
#ifdef HAVE_BKTR
						__dump_backtrace(&lt->backtrace[lt->reentrancy], canlog);
#endif
						__css_mutex_logger("%s line %d (%s): '%s' was locked  here.\n",
								lt->file[lt->reentrancy-1], lt->lineno[lt->reentrancy-1],
								lt->func[lt->reentrancy-1], name);
#ifdef HAVE_BKTR
						__dump_backtrace(&lt->backtrace[lt->reentrancy-1], canlog);
#endif
						css_reentrancy_unlock(lt);
					}
					reported_wait = wait_time;
				}
				usleep(200);
			}
		} while (res == EBUSY);
	}
#else /* !DETECT_DEADLOCKS || !DEBUG_THREADS */
	res = pthread_rwlock_wrlock(&t->lock);
#endif /* !DETECT_DEADLOCKS || !DEBUG_THREADS */

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			css_reentrancy_lock(lt);
			bt = &lt->backtrace[lt->reentrancy-1];
			css_reentrancy_unlock(lt);
		} else {
			bt = NULL;
		}
		if (t->tracking) {
			css_remove_lock_info(t, bt);
		}
#else
		if (t->tracking) {
			css_remove_lock_info(t);
		}
#endif
	}
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error obtaining write lock: %s\n",
				filename, line, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_timedrdlock(const char *filename, int lineno, const char *func, 
        css_rwlock_t *t, const char *name, const struct timespec *abs_timeout)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

#ifdef HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK
	res = pthread_rwlock_timedrdlock(&t->lock, abs_timeout);
#else
	do {
		struct timeval _start = css_tvnow(), _diff;
		for (;;) {
			if (!(res = pthread_rwlock_tryrdlock(&t->lock))) {
				break;
			}
			_diff = css_tvsub(css_tvnow(), _start);
			if (_diff.tv_sec > abs_timeout->tv_sec || (_diff.tv_sec == abs_timeout->tv_sec && _diff.tv_usec * 1000 > abs_timeout->tv_nsec)) {
				break;
			}
			usleep(1);
		}
	} while (0);
#endif

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			css_reentrancy_lock(lt);
			bt = &lt->backtrace[lt->reentrancy-1];
			css_reentrancy_unlock(lt);
		} else {
			bt = NULL;
		}
		css_remove_lock_info(t, bt);
#else
		css_remove_lock_info(t);
#endif
	}
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error obtaining read lock: %s\n",
				filename, line, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_timedwrlock(const char *filename, int line, const char *func, 
        css_rwlock_t *t, const char *name,
	const struct timespec *abs_timeout)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
	int canlog = strcmp(filename, "logger.c") & t->tracking;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif

#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

#ifdef HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK
	res = pthread_rwlock_timedwrlock(&t->lock, abs_timeout);
#else
	do {
		struct timeval _start = css_tvnow(), _diff;
		for (;;) {
			if (!(res = pthread_rwlock_trywrlock(&t->lock))) {
				break;
			}
			_diff = css_tvsub(css_tvnow(), _start);
			if (_diff.tv_sec > abs_timeout->tv_sec || (_diff.tv_sec == abs_timeout->tv_sec && _diff.tv_usec * 1000 > abs_timeout->tv_nsec)) {
				break;
			}
			usleep(1);
		}
	} while (0);
#endif

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
#ifdef HAVE_BKTR
		if (lt->reentrancy) {
			css_reentrancy_lock(lt);
			bt = &lt->backtrace[lt->reentrancy-1];
			css_reentrancy_unlock(lt);
		} else {
			bt = NULL;
		}
		if (t->tracking) {
			css_remove_lock_info(t, bt);
		}
#else
		if (t->tracking) {
			css_remove_lock_info(t);
		}
#endif
	}
	if (res) {
		__css_mutex_logger("%s line %d (%s): Error obtaining read lock: %s\n",
				filename, line, func, strerror(res));
		DO_THREAD_CRASH;
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_tryrdlock(const char *filename, int line, const char *func, css_rwlock_t *t, const char *name)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif
#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	int canlog = strcmp(filename, "logger.c") & t->tracking;

	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_RDLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_RDLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_rwlock_tryrdlock(&t->lock);

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		if (t->tracking) {
			css_mark_lock_acquired(t);
		}
	} else if (t->tracking) {
		css_mark_lock_failed(t);
	}
#endif /* DEBUG_THREADS */

	return res;
}

int __css_rwlock_trywrlock(const char *filename, int line, const char *func, css_rwlock_t *t, const char *name)
{
	int res;

#ifdef DEBUG_THREADS
	struct css_lock_track *lt;
#ifdef HAVE_BKTR
	struct css_bt *bt = NULL;
#endif
#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS) && defined(CAN_COMPARE_MUTEX_TO_INIT_VALUE)
	int canlog = strcmp(filename, "logger.c") & t->tracking;

	if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
		 /* Don't warn abount uninitialized lock.
		  * Simple try to initialize it.
		  * May be not needed in linux system.
		  */
		res = __css_rwlock_init(t->tracking, filename, line, func, name, t);
		if ((t->lock) == ((pthread_rwlock_t) __CSS_RWLOCK_INIT_VALUE)) {
			__css_mutex_logger("%s line %d (%s): Error: rwlock '%s' is uninitialized and unable to initialize.\n",
					filename, line, func, name);
			return res;
		}
	}
#endif /* CSS_MUTEX_INIT_W_CONSTRUCTORS */

	if (t->tracking && !t->track) {
		css_reentrancy_init(&t->track);
	}
	lt = t->track;

	if (t->tracking) {
#ifdef HAVE_BKTR
		css_reentrancy_lock(lt);
		if (lt->reentrancy != CSS_MAX_REENTRANCY) {
			css_bt_get_addresses(&lt->backtrace[lt->reentrancy]);
			bt = &lt->backtrace[lt->reentrancy];
		}
		css_reentrancy_unlock(lt);
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t, bt);
#else
		css_store_lock_info(CSS_WRLOCK, filename, line, func, name, t);
#endif
	}
#endif /* DEBUG_THREADS */

	res = pthread_rwlock_trywrlock(&t->lock);

#ifdef DEBUG_THREADS
	if (!res && t->tracking) {
		css_reentrancy_lock(lt);
		if (lt->reentrancy < CSS_MAX_REENTRANCY) {
			lt->file[lt->reentrancy] = filename;
			lt->lineno[lt->reentrancy] = line;
			lt->func[lt->reentrancy] = func;
			lt->thread[lt->reentrancy] = pthread_self();
			lt->reentrancy++;
		}
		css_reentrancy_unlock(lt);
		css_mark_lock_acquired(t);
	} else if (t->tracking) {
		css_mark_lock_failed(t);
	}
#endif /* DEBUG_THREADS */

	return res;
}

