/* 
 * File:   localtime.h
 * Author: root
 *
 * Created on April 20, 2014, 8:07 PM
 */

#ifndef LOCALTIME_H
#define	LOCALTIME_H

#define HAVE_LOCALE_T_IN_LOCALE_H 1
#ifdef HAVE_LOCALE_T_IN_LOCALE_H
#include <locale.h>
#elif defined(HAVE_LOCALE_T_IN_XLOCALE_H)
#include <xlocale.h>
#else
typedef void * locale_t;
#endif

struct css_tm {
	int tm_sec;             /*!< Seconds. [0-60] (1 leap second) */
	int tm_min;             /*!< Minutes. [0-59] */
	int tm_hour;            /*!< Hours.   [0-23] */
	int tm_mday;            /*!< Day.     [1-31] */
	int tm_mon;             /*!< Month.   [0-11] */
	int tm_year;            /*!< Year - 1900.  */
	int tm_wday;            /*!< Day of week. [0-6] */
	int tm_yday;            /*!< Days in year.[0-365]	*/
	int tm_isdst;           /*!< DST.		[-1/0/1]*/
	long int tm_gmtoff;     /*!< Seconds ecss of UTC.  */
	char *tm_zone;          /*!< Timezone abbreviation.  */
	/* NOTE: do NOT reorder this final item.  The order needs to remain compatible with struct tm */
	int tm_usec;        /*!< microseconds */
};

/*!\brief Timezone-independent version of localtime_r(3).
 * \param timep Current time, including microseconds
 * \param p_tm Pointer to memory where the broken-out time will be stored
 * \param zone Text string of a standard system zoneinfo file.  If NULL, the system localtime will be used.
 * \retval p_tm is returned for convenience
 */
struct css_tm *css_localtime(const struct timeval *timep, struct css_tm *p_tm, const char *zone);

//void css_get_dst_info(const time_t * const timep, int *dst_enabled, time_t *dst_start, time_t *dst_end, int *gmt_off, const char * const zone);

/*!\brief Timezone-independent version of mktime(3).
 * \param tmp Current broken-out time, including microseconds
 * \param zone Text string of a standard system zoneinfo file.  If NULL, the system localtime will be used.
 * \retval A structure containing both seconds and fractional thereof since January 1st, 1970 UTC
 */
struct timeval css_mktime(struct css_tm * const tmp, const char *zone);

/*!\brief Set the thread-local representation of the current locale. */
const char *css_setlocale(const char *locale);

/*!\brief Special version of strftime(3) that handles fractions of a second.
 * Takes the same arguments as strftime(3), with the addition of %q, which
 * specifies microseconds.
 * \param buf Address in memory where the resulting string will be stored.
 * \param len Size of the chunk of memory buf.
 * \param format A string specifying the format of time to be placed into buf.
 * \param tm Pointer to the broken out time to be used for the format.
 * \param locale Text string specifying the locale to be used for language strings.
 * \retval An integer value specifying the number of bytes placed into buf or -1 on error.
 */
int css_strftime(char *buf, size_t len, const char *format, const struct css_tm *tm);
int css_strftime_locale(char *buf, size_t len, const char *format, const struct css_tm *tm, const char *locale);

/*!\brief Special version of strptime(3) which places the answer in the common
 * structure css_tm.  Also, unlike strptime(3), css_strptime() initializes its
 * memory prior to use.
 * \param s A string specifying some portion of a date and time.
 * \param format The format in which the string, s, is expected.
 * \param tm The broken-out time structure into which the parsed data is expected.
 * \param locale Text string specifying the locale to be used for language strings.
 * \retval A pointer to the first character within s not used to parse the date and time.
 */
char *css_strptime(const char *s, const char *format, struct css_tm *tm);
char *css_strptime_locale(const char *s, const char *format, struct css_tm *tm, const char *locale);

/*!\brief Wakeup localtime monitor thread
 * For use in testing.  Normally, the failsafe monitor thread waits 60 seconds
 * between checks to verify whether a timezone file has changed.  This routine
 * forces the monitor thread to wakeup immediately and check the timezone files.
 */
//struct css_test;
//void css_localtime_wakeup_monitor(struct css_test *info);

#endif	/* LOCALTIME_H */

