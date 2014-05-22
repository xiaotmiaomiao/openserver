/* 
 * File:   poll-compat.h
 * Author: root
 *
 * Created on April 21, 2014, 12:50 AM
 */

#ifndef POLL_COMPAT_H
#define	POLL_COMPAT_H

#include "select.h"

#ifndef CSS_POLL_COMPAT

#include <sys/poll.h>

#define css_poll(a, b, c) poll(a, b, c)

#else /* CSS_POLL_COMPAT */

#define POLLIN		0x01
#define POLLPRI		0x02
#define POLLOUT		0x04
#define POLLERR		0x08
#define POLLHUP		0x10
#define POLLNVAL	0x20

struct pollfd {
    int     fd;
    short   events;
    short   revents;
};

#ifdef __cplusplus
extern "C" {
#endif

#define css_poll(a, b, c) css_internal_poll(a, b, c)

int css_internal_poll(struct pollfd *pArray, unsigned long n_fds, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* CSS_POLL_COMPAT */

/*!
 * \brief Same as poll(2), except the time is specified in microseconds and
 * the tv argument is modified to indicate the time remaining.
 */
int css_poll2(struct pollfd *pArray, unsigned long n_fds, struct timeval *tv);

/*!
 * \brief Shortcut for conversion of FD_ISSET to poll(2)-based
 */
static inline int css_poll_fd_index(struct pollfd *haystack, int nfds, int needle)
{
	int i;
	for (i = 0; i < nfds; i++) {
		if (haystack[i].fd == needle) {
			return i;
		}
	}
	return -1;
}

#endif	/* POLL_COMPAT_H */

