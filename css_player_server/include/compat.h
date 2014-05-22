/* 
 * File:   compat.h
 * Author: root
 *
 * Created on April 21, 2014, 2:41 AM
 */

#ifndef COMPAT_H
#define	COMPAT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "compiler.h"

#ifndef __STDC_VERSION__
/* flex output wants to find this defined. */
#define	__STDC_VERSION__ 0
#endif

#define HAVE_INTTYPES_H 1    
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#define HAVE_LIMITS_H 1     
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define HAVE_UNISTD_H 1     
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define HAVE_STDDEF_H 1     
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

//#define HAVE_STDINT_H 1     
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#define HAVE_SYS_TYPES_H 1     
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <stdarg.h>

#define HAVE_STDLIB_H 1     
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#define HAVE_ALLOCA_H 1     
#ifdef HAVE_ALLOCA_H
#include <alloca.h>    /* not necessarily present - could be in stdlib */
#elif defined(HAVE_ALLOCA) && defined(__MINGW32__)
#include <malloc.h>    /* see if it is here... */
#endif

#include <stdio.h>	/* this is always present */

#define HAVE_STRING_H 1     
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifndef CSS_POLL_COMPAT
#include <sys/poll.h>
#else
#include "poll-compat.h"
#endif

#define HAVE_LLONG_MAX 1     
#ifndef HAVE_LLONG_MAX
#define	LLONG_MAX	9223372036854775807LL
#endif


#ifndef HAVE_CLOSEFROM
void closefrom(int lowfd);
#endif

#define HAVE_ASPRINTF 1 
#if !defined(HAVE_ASPRINTF) && !defined(__CSS_DEBUG_MALLOC)
int __attribute__((format(printf, 2, 3))) asprintf(char **str, const char *fmt, ...);
#endif

#define HAVE_FFSLL 1 
#ifndef HAVE_FFSLL
int ffsll(long long n);
#endif

#define HAVE_GETLOADAVG 1 
#ifndef HAVE_GETLOADAVG
int getloadavg(double *list, int nelem);
#endif

#ifndef HAVE_HTONLL
uint64_t htonll(uint64_t host64);
#endif

#define HAVE_MKDTEMP 1 
#ifndef HAVE_MKDTEMP
char *mkdtemp(char *template_s);
#endif

#ifndef HAVE_NTOHLL
uint64_t ntohll(uint64_t net64);
#endif

#define HAVE_SETENV 1 
#ifndef HAVE_SETENV
int setenv(const char *name, const char *value, int overwrite);
#endif

//#define HAVE_STRCASESTR 1 
#ifndef HAVE_STRCASESTR
char *strcasestr(const char *, const char *);
#endif

#define HAVE_STRNDUP 1 
#if !defined(HAVE_STRNDUP) && !defined(__CSS_DEBUG_MALLOC)
char *strndup(const char *, size_t);
#endif

#define HAVE_STRNLEN 1 
#ifndef HAVE_STRNLEN
size_t strnlen(const char *, size_t);
#endif

#define HAVE_STRSEP 1 
#ifndef HAVE_STRSEP
char* strsep(char** str, const char* delims);
#endif

#define HAVE_STRTOQ 1
#ifndef HAVE_STRTOQ
uint64_t strtoq(const char *nptr, char **endptr, int base);
#endif

#define HAVE_UNSETENV 1
#ifndef HAVE_UNSETENV
int unsetenv(const char *name);
#endif

#define HAVE_VASPRINTF
#if !defined(HAVE_VASPRINTF) && !defined(__CSS_DEBUG_MALLOC)
int __attribute__((format(printf, 2, 0))) vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#define HAVE_TIMERSUB
#ifndef HAVE_TIMERSUB
void timersub(struct timeval *tvend, struct timeval *tvstart, struct timeval *tvdiff);
#endif

#define	strlcat	__use__css_str__functions_not__strlcat__
#define	strlcpy	__use__css_copy_string__not__strlcpy__

#include <errno.h>

#ifdef SOLARIS
#define __BEGIN_DECLS
#define __END_DECLS

#ifndef __P
#define __P(p) p
#endif

#include <alloca.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/loadavg.h>
#include <dat/dat_platform_specific.h>

#ifndef BYTE_ORDER
#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN	4321

#ifdef __sparc__
#define BYTE_ORDER	BIG_ENDIAN
#else
#define BYTE_ORDER	LITTLE_ENDIAN
#endif
#endif

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#define __BYTE_ORDER BYTE_ORDER
#endif

#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__
typedef unsigned char	u_int8_t;
typedef unsigned short	u_int16_t;
typedef unsigned int	u_int32_t;
typedef unsigned int	uint;
#endif

#endif /* SOLARIS */

#ifdef __CYGWIN__
#define _WIN32_WINNT 0x0500
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN  16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
#endif /* __CYGWIN__ */

#ifdef __CYGWIN__
typedef unsigned long long uint64_t;
#endif

/* glob compat stuff */ 
#if defined(__Darwin__) || defined(__CYGWIN__)
#define GLOB_ABORTED GLOB_ABEND
#endif
#include <glob.h>
#if !defined(HAVE_GLOB_NOMAGIC) || !defined(HAVE_GLOB_BRACE)
#define MY_GLOB_FLAGS   GLOB_NOCHECK
#else
#define MY_GLOB_FLAGS   (GLOB_NOMAGIC | GLOB_BRACE)
#endif


#ifdef	__cplusplus
}
#endif

#endif	/* COMPAT_H */

