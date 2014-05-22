/* 
 * File:   syslog.h
 * Author: root
 *
 * Created on April 23, 2014, 1:30 AM
 */

#ifndef SYSLOG_H
#define	SYSLOG_H

#ifdef	__cplusplus
extern "C" {
#endif

/*!
 * \since 1.8
 * \brief Maps a syslog facility name from a string to a syslog facility
 *        constant.
 *
 * \param facility Facility name to map (i.e. "daemon")
 *
 * \retval syslog facility constant (i.e. LOG_DAEMON) if found
 * \retval -1 if facility is not found
 */
int css_syslog_facility(const char *facility);

/*!
 * \since 1.8
 * \brief Maps a syslog facility constant to a string.
 *
 * \param facility syslog facility constant to map (i.e. LOG_DAEMON)
 *
 * \retval facility name (i.e. "daemon") if found
 * \retval NULL if facility is not found
 */
const char *css_syslog_facility_name(int facility);

/*!
 * \since 1.8
 * \brief Maps a syslog priority name from a string to a syslog priority
 *        constant.
 *
 * \param priority Priority name to map (i.e. "notice")
 *
 * \retval syslog priority constant (i.e. LOG_NOTICE) if found
 * \retval -1 if priority is not found
 */
int css_syslog_priority(const char *priority);

/*!
 * \since 1.8
 * \brief Maps a syslog priority constant to a string.
 *
 * \param priority syslog priority constant to map (i.e. LOG_NOTICE)
 *
 * \retval priority name (i.e. "notice") if found
 * \retval NULL if priority is not found
 */
const char *css_syslog_priority_name(int priority);

/*!
 * \since 1.8
 * \brief Maps an Ceictims log level (i.e. LOG_ERROR) to a syslog priority
 *        constant.
 *
 * \param level Ceictims log level constant (i.e. LOG_ERROR)
 *
 * \retval syslog priority constant (i.e. LOG_ERR) if found
 * \retval -1 if priority is not found
 */
int css_syslog_priority_from_loglevel(int level);


#ifdef	__cplusplus
}
#endif

#endif	/* SYSLOG_H */

