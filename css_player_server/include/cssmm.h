/* 
 * File:   cssmm.h
 * Author: root
 *
 * Created on April 17, 2014, 2:43 AM
 */

#ifndef CSSMM_H
#define	CSSMM_H

#ifdef	__cplusplus
extern "C" {
#endif

/* Include these now to prevent them from being needed later */
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Undefine any macros */
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef strndup
#undef asprintf
#undef vasprintf
#undef free

void *__css_calloc(size_t nmemb, size_t size, const char *file, int lineno, const char *func);
void *__css_calloc_cache(size_t nmemb, size_t size, const char *file, int lineno, const char *func);
void *__css_malloc(size_t size, const char *file, int lineno, const char *func);
void __css_free(void *ptr, const char *file, int lineno, const char *func);
void *__css_realloc(void *ptr, size_t size, const char *file, int lineno, const char *func);
char *__css_strdup(const char *s, const char *file, int lineno, const char *func);
char *__css_strndup(const char *s, size_t n, const char *file, int lineno, const char *func);
int __css_asprintf(const char *file, int lineno, const char *func, char **strp, const char *format, ...)
	__attribute__((format(printf, 5, 6)));
int __css_vasprintf(char **strp, const char *format, va_list ap, const char *file, int lineno, const char *func)
	__attribute__((format(printf, 2, 0)));
void __css_mm_init(void);


/* Provide our own definitions */
#define calloc(a,b) \
	__css_calloc(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_calloc(a,b) \
	__css_calloc(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_calloc_cache(a,b) \
	__css_calloc_cache(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define malloc(a) \
	__css_malloc(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_malloc(a) \
	__css_malloc(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define free(a) \
	__css_free(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_free(a) \
	__css_free(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define realloc(a,b) \
	__css_realloc(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_realloc(a,b) \
	__css_realloc(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define strdup(a) \
	__css_strdup(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_strdup(a) \
	__css_strdup(a,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define strndup(a,b) \
	__css_strndup(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_strndup(a,b) \
	__css_strndup(a,b,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define asprintf(a, b, c...) \
	__css_asprintf(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, b, c)

#define css_asprintf(a, b, c...) \
	__css_asprintf(__FILE__, __LINE__, __PRETTY_FUNCTION__, a, b, c)

#define vasprintf(a,b,c) \
	__css_vasprintf(a,b,c,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define css_vasprintf(a,b,c) \
	__css_vasprintf(a,b,c,__FILE__, __LINE__, __PRETTY_FUNCTION__)

#ifdef	__cplusplus
}
#endif

#endif	/* CSSMM_H */

