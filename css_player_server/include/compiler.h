/* 
 * File:   Compiler.h
 * Author: root
 *
 * Created on April 17, 2014, 7:19 PM
 */

#ifndef COMPILER_H
#define	COMPILER_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef HAVE_ATTRIBUTE_always_inline
#define force_inline __attribute__((always_inline)) inline
#else
#define force_inline inline
#endif

#ifdef HAVE_ATTRIBUTE_pure
#define attribute_pure __attribute__((pure))
#else
#define attribute_pure
#endif

#ifdef HAVE_ATTRIBUTE_const
#define attribute_const __attribute__((const))
#else
#define attribute_const
#endif

#ifdef HAVE_ATTRIBUTE_deprecated
#define attribute_deprecated __attribute__((deprecated))
#else
#define attribute_deprecated
#endif

#ifdef HAVE_ATTRIBUTE_unused
#define attribute_unused __attribute__((unused))
#else
#define attribute_unused
#endif

#ifdef HAVE_ATTRIBUTE_malloc
#define attribute_malloc __attribute__((malloc))
#else
#define attribute_malloc
#endif

#ifdef HAVE_ATTRIBUTE_sentinel
#define attribute_sentinel __attribute__((sentinel))
#else
#define attribute_sentinel
#endif

#ifdef HAVE_ATTRIBUTE_warn_unused_result
#define attribute_warn_unused_result __attribute__((warn_unused_result))
#else
#define attribute_warn_unused_result
#endif

/* Some older version of GNU gcc (3.3.5 on OpenBSD 4.3 for example) dont like 'NULL' as sentinel */
#define SENTINEL ((char *)NULL)


#ifdef	__cplusplus
}
#endif

#endif	/* COMPILER_H */

