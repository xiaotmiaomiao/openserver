/* 
 * File:   select.h
 * Author: root
 *
 * Created on April 21, 2014, 7:11 PM
 */

#ifndef SELECT_H
#define	SELECT_H

#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>

#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int css_FD_SETSIZE;

#define TYPEOF_FD_SET_FDS_BITS int
#define SIZEOF_FD_SET_FDS_BITS 4

#if !defined(HAVE_VARIABLE_FDSET) && defined(CONFIGURE_RAN_AS_ROOT)
#define ast_fdset fd_set
#else
#define ast_FDMAX	32768
typedef struct {
	TYPEOF_FD_SET_FDS_BITS fds_bits[ast_FDMAX / 8 / SIZEOF_FD_SET_FDS_BITS]; /* 32768 bits */
} ast_fdset;

#define _bitsize(a)	(sizeof(a) * 8)

#undef FD_ZERO
#define FD_ZERO(a) \
	do { \
		TYPEOF_FD_SET_FDS_BITS *bytes = (TYPEOF_FD_SET_FDS_BITS *) a; \
		int i; \
		for (i = 0; i < ast_FDMAX / _bitsize(TYPEOF_FD_SET_FDS_BITS); i++) { \
			bytes[i] = 0; \
		} \
	} while (0)
#undef FD_SET
#define FD_SET(fd, fds) \
	do { \
		TYPEOF_FD_SET_FDS_BITS *bytes = (TYPEOF_FD_SET_FDS_BITS *) fds; \
		/* 32bit: FD / 32 + ((FD + 1) % 32 ? 1 : 0) < 1024 */ \
		/* 64bit: FD / 64 + ((FD + 1) % 64 ? 1 : 0) < 512 */ \
		if (fd / _bitsize(*bytes) + ((fd + 1) % _bitsize(*bytes) ? 1 : 0) < sizeof(*(fds)) / SIZEOF_FD_SET_FDS_BITS) { \
			bytes[fd / _bitsize(*bytes)] |= ((TYPEOF_FD_SET_FDS_BITS) 1) << (fd % _bitsize(*bytes)); \
		} else { \
			fprintf(stderr, "FD %d exceeds the maximum size of ast_fdset!\n", fd); \
		} \
	} while (0)
#endif /* HAVE_VARIABLE_FDSET */

/*! \brief Waits for activity on a group of channels 
 * \param nfds the maximum number of file descriptors in the sets
 * \param rfds file descriptors to check for read availability
 * \param wfds file descriptors to check for write availability
 * \param efds file descriptors to check for exceptions (OOB data)
 * \param tvp timeout while waiting for events
 * This is the same as a standard select(), except it guarantees the
 * behaviour where the passed struct timeval is updated with how much
 * time was not slept while waiting for the specified events
 */
static inline int ast_select(int nfds, ast_fdset *rfds, ast_fdset *wfds, ast_fdset *efds, struct timeval *tvp)
{
#ifdef __linux__
	return select(nfds, (fd_set *) rfds, (fd_set *) wfds, (fd_set *) efds, tvp);
#else
	int save_errno = 0;
	if (tvp) {
		struct timeval tv, tvstart, tvend, tvlen;
		int res;

		tv = *tvp;
		gettimeofday(&tvstart, NULL);
		res = select(nfds, (fd_set *) rfds, (fd_set *) wfds, (fd_set *) efds, tvp);
		save_errno = errno;
		gettimeofday(&tvend, NULL);
		timersub(&tvend, &tvstart, &tvlen);
		timersub(&tv, &tvlen, tvp);
		if (tvp->tv_sec < 0 || (tvp->tv_sec == 0 && tvp->tv_usec < 0)) {
			tvp->tv_sec = 0;
			tvp->tv_usec = 0;
		}
		errno = save_errno;
		return res;
	}
	else
		return select(nfds, (fd_set *) rfds, (fd_set *) wfds, (fd_set *) efds, NULL);
#endif
}

#ifdef __cplusplus
}
#endif

#endif	/* SELECT_H */

