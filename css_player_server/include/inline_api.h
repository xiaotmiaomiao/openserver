/* 
 * File:   Inline_api.h
 * Author: root
 *
 * Created on April 17, 2014, 7:38 PM
 */

#ifndef INLINE_API_H
#define	INLINE_API_H

#ifdef	__cplusplus
extern "C" {
#endif

/*! \file
 * \brief Inlinable API function macro

  Small API functions that are candidates for inlining need to be specially
  declared and defined, to ensure that the 'right thing' always happens.
  For example:
  	- there must _always_ be a non-inlined version of the function
	available for modules compiled out of the tree to link to
	- references to a function that cannot be inlined (for any
	reason that the compiler deems proper) must devolve into an
	'extern' reference, instead of 'static', so that multiple
	copies of the function body are not built in different modules
	- when LOW_MEMORY is defined, inlining should be disabled
	completely, even if the compiler is configured to support it

  The CSS_INLINE_API macro allows this to happen automatically, when
  used to define your function. Proper usage is as follows:
  - define your function one place, in a header file, using the macro
  to wrap the function (see strings.h or time.h for examples)
  - choose a module to 'host' the function body for non-inline
  usages, and in that module _only_, define CSS_API_MODULE before
  including the header file
 */

#if !defined(LOW_MEMORY)
    
#if !defined(CSS_API_MODULE)
#define CSS_INLINE_API(hdr, body) hdr; extern inline hdr body
#else
#define CSS_INLINE_API(hdr, body) hdr; hdr body
#endif

#else /* defined(LOW_MEMORY) */

#if !defined(AST_API_MODULE)
#define CSS_INLINE_API(hdr, body) hdr;
#else
#define CSS_INLINE_API(hdr, body) hdr; hdr body
#endif

#endif

#undef CSS_API_MODULE


#ifdef	__cplusplus
}
#endif

#endif	/* INLINE_API_H */
