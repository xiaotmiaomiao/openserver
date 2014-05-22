/* 
 * File: css_monitor.c
 * Author: root
 *
 * Created on April 16, 2014, 6:29 PM
 */

/*** MAKEOPTS
<category name="MENUSELECT_CFLAGS" displayname="Compiler Flags" positive_output="yes">
	<member name="DEBUG_OPAQUE" displayname="Change css_str internals to detect improper usage" touch_on_change="include/ceictims/strings.h">
		<defaultenabled>yes</defaultenabled>
	</member>
</category>
 ***/

#include "cssplayer.h"

#include "strings.h"
//#include "pbx.h"

/*!
 * core handler for dynamic strings.
 * This is not meant to be called directly, but rather through the
 * various wrapper macros
 *	css_str_set(...)
 *	css_str_append(...)
 *	css_str_set_va(...)
 *	css_str_append_va(...)
 */

#if (defined(MALLOC_DEBUG) && !defined(STANDALONE))
int __css_debug_str_helper(struct css_str **buf, ssize_t max_len,
	int append, const char *fmt, va_list ap, const char *file, int lineno, const char *function)
#else
int __css_str_helper(struct css_str **buf, ssize_t max_len,
	int append, const char *fmt, va_list ap)
#endif
{
	int res, need;
	int offset = (append && (*buf)->__CSS_STR_LEN) ? (*buf)->__CSS_STR_USED : 0;
	va_list aq;

	do {
		if (max_len < 0) {
			max_len = (*buf)->__CSS_STR_LEN;	/* don't exceed the allocated space */
		}
		/*
		 * Ask vsnprintf how much space we need. Remember that vsnprintf
		 * does not count the final <code>'\0'</code> so we must add 1.
		 */
		va_copy(aq, ap);
		res = vsnprintf((*buf)->__CSS_STR_STR + offset, (*buf)->__CSS_STR_LEN - offset, fmt, aq);

		need = res + offset + 1;
		/*
		 * If there is not enough space and we are below the max length,
		 * reallocate the buffer and return a message telling to retry.
		 */
		if (need > (*buf)->__CSS_STR_LEN && (max_len == 0 || (*buf)->__CSS_STR_LEN < max_len) ) {
			int len = (int)(*buf)->__CSS_STR_LEN;
			if (max_len && max_len < need) {	/* truncate as needed */
				need = max_len;
			} else if (max_len == 0) {	/* if unbounded, give more room for next time */
				need += 16 + need / 4;
			}
			if (0) {	/* debugging */
				//css_verbose("extend from %d to %d\n", len, need);
			}
			if (
#if (defined(MALLOC_DEBUG) && !defined(STANDALONE))
					_css_str_make_space(buf, need, file, lineno, function)
#else
					css_str_make_space(buf, need)
#endif
				) {
				//css_verbose("failed to extend from %d to %d\n", len, need);
				va_end(aq);
				return CSS_DYNSTR_BUILD_FAILED;
			}
			(*buf)->__CSS_STR_STR[offset] = '\0';	/* Truncate the partial write. */

			/* Restart va_copy before calling vsnprintf() again. */
			va_end(aq);
			continue;
		}
		va_end(aq);
		break;
	} while (1);
	/* update space used, keep in mind the truncation */
	(*buf)->__CSS_STR_USED = (res + offset > (*buf)->__CSS_STR_LEN) ? (*buf)->__CSS_STR_LEN - 1: res + offset;

	return res;
}

char *__css_str_helper2(struct css_str **buf, ssize_t maxlen, const char *src, size_t maxsrc, int append, int escapecommas)
{
	int dynamic = 0;
	char *ptr = append ? &((*buf)->__CSS_STR_STR[(*buf)->__CSS_STR_USED]) : (*buf)->__CSS_STR_STR;

	if (maxlen < 1) {
		if (maxlen == 0) {
			dynamic = 1;
		}
		maxlen = (*buf)->__CSS_STR_LEN;
	}

	while (*src && maxsrc && maxlen && (!escapecommas || (maxlen - 1))) {
		if (escapecommas && (*src == '\\' || *src == ',')) {
			*ptr++ = '\\';
			maxlen--;
			(*buf)->__CSS_STR_USED++;
		}
		*ptr++ = *src++;
		maxsrc--;
		maxlen--;
		(*buf)->__CSS_STR_USED++;

		if ((ptr >= (*buf)->__CSS_STR_STR + (*buf)->__CSS_STR_LEN - 3) ||
			(dynamic && (!maxlen || (escapecommas && !(maxlen - 1))))) {
			char *oldbase = (*buf)->__CSS_STR_STR;
			size_t old = (*buf)->__CSS_STR_LEN;
			if (css_str_make_space(buf, (*buf)->__CSS_STR_LEN * 2)) {
				/* If the buffer can't be extended, end it. */
				break;
			}
			/* What we extended the buffer by */
			maxlen = old;

			ptr += (*buf)->__CSS_STR_STR - oldbase;
		}
	}
	if (__builtin_expect(!maxlen, 0)) {
		ptr--;
	}
	*ptr = '\0';
	return (*buf)->__CSS_STR_STR;
}


