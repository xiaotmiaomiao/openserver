/* 
 * File:   options.h
 * Author: root
 *
 * Created on April 28, 2014, 1:35 AM
 */

#ifndef OPTIONS_H
#define	OPTIONS_H

#ifdef	__cplusplus
extern "C" {
#endif


/*! \file
 * \brief Options provided by main Cssplayer program
 */

#define CSS_CACHE_DIR_LEN 	512
#define CSS_FILENAME_MAX	80
#define CSS_CHANNEL_NAME    80  /*!< Max length of an css_channel name */


/*! \ingroup main_options */
enum css_option_flags {
	/*! Allow \#exec in config files */
	CSS_OPT_FLAG_EXEC_INCLUDES = (1 << 0),
	/*! Do not fork() */
	CSS_OPT_FLAG_NO_FORK = (1 << 1),
	/*! Keep quiet */
	CSS_OPT_FLAG_QUIET = (1 << 2),
	/*! Console mode */
	CSS_OPT_FLAG_CONSOLE = (1 << 3),
	/*! Run in realtime Linux priority */
	CSS_OPT_FLAG_HIGH_PRIORITY = (1 << 4),
	/*! Initialize keys for RSA authentication */
	CSS_OPT_FLAG_INIT_KEYS = (1 << 5),
	/*! Remote console */
	CSS_OPT_FLAG_REMOTE = (1 << 6),
	/*! Execute an Cssplayer CLI command upon startup */
	CSS_OPT_FLAG_EXEC = (1 << 7),
	/*! Don't use termcap colors */
	CSS_OPT_FLAG_NO_COLOR = (1 << 8),
	/*! Are we fully started yet? */
	CSS_OPT_FLAG_FULLY_BOOTED = (1 << 9),
	/*! Trascode via signed linear */
	CSS_OPT_FLAG_TRANSCODE_VIA_SLIN = (1 << 10),
	/*! Dump core on a seg fault */
	CSS_OPT_FLAG_DUMP_CORE = (1 << 12),
	/*! Cache sound files */
	CSS_OPT_FLAG_CACHE_RECORD_FILES = (1 << 13),
	/*! Display timestamp in CLI verbose output */
	CSS_OPT_FLAG_TIMESTAMP = (1 << 14),
	/*! Override config */
	CSS_OPT_FLAG_OVERRIDE_CONFIG = (1 << 15),
	/*! Reconnect */
	CSS_OPT_FLAG_RECONNECT = (1 << 16),
	/*! Transmit Silence during Record() and DTMF Generation */
	CSS_OPT_FLAG_TRANSMIT_SILENCE = (1 << 17),
	/*! Suppress some warnings */
	CSS_OPT_FLAG_DONT_WARN = (1 << 18),
	/*! End CDRs before the 'h' extension */
	CSS_OPT_FLAG_END_CDR_BEFORE_H_EXTEN = (1 << 19),
	/*! Use DAHDI Timing for generators if available */
	CSS_OPT_FLAG_INTERNAL_TIMING = (1 << 20),
	/*! Always fork, even if verbose or debug settings are non-zero */
	CSS_OPT_FLAG_ALWAYS_FORK = (1 << 21),
	/*! Disable log/verbose output to remote consoles */
	CSS_OPT_FLAG_MUTE = (1 << 22),
	/*! There is a per-module debug setting */
	CSS_OPT_FLAG_DEBUG_MODULE = (1 << 23),
	/*! There is a per-module verbose setting */
	CSS_OPT_FLAG_VERBOSE_MODULE = (1 << 24),
	/*! Terminal colors should be adjusted for a light-colored background */
	CSS_OPT_FLAG_LIGHT_BACKGROUND = (1 << 25),
	/*! Count Initiated seconds in CDR's */
	CSS_OPT_FLAG_INITIATED_SECONDS = (1 << 26),
	/*! Force black background */
	CSS_OPT_FLAG_FORCE_BLACK_BACKGROUND = (1 << 27),
	/*! Hide remote console connect messages on console */
	CSS_OPT_FLAG_HIDE_CONSOLE_CONNECT = (1 << 28),
	/*! Protect the configuration file path with a lock */
	CSS_OPT_FLAG_LOCK_CONFIG_DIR = (1 << 29),
	/*! Generic PLC */
	CSS_OPT_FLAG_GENERIC_PLC = (1 << 30),
};

/*! These are the options that set by default when Cssplayer starts */
#if (defined(HAVE_DAHDI_VERSION) && HAVE_DAHDI_VERSION >= 230)
#define CSS_DEFAULT_OPTIONS CSS_OPT_FLAG_TRANSCODE_VIA_SLIN | CSS_OPT_FLAG_INTERNAL_TIMING
#else
#define CSS_DEFAULT_OPTIONS CSS_OPT_FLAG_TRANSCODE_VIA_SLIN
#endif

#define css_opt_exec_includes		css_test_flag(&css_options, CSS_OPT_FLAG_EXEC_INCLUDES)
#define css_opt_no_fork			css_test_flag(&css_options, CSS_OPT_FLAG_NO_FORK)
#define css_opt_quiet			css_test_flag(&css_options, CSS_OPT_FLAG_QUIET)
#define css_opt_console			css_test_flag(&css_options, CSS_OPT_FLAG_CONSOLE)
#define css_opt_high_priority		css_test_flag(&css_options, CSS_OPT_FLAG_HIGH_PRIORITY)
#define css_opt_init_keys		css_test_flag(&css_options, CSS_OPT_FLAG_INIT_KEYS)
#define css_opt_remote			css_test_flag(&css_options, CSS_OPT_FLAG_REMOTE)
#define css_opt_exec			css_test_flag(&css_options, CSS_OPT_FLAG_EXEC)
#define css_opt_no_color		css_test_flag(&css_options, CSS_OPT_FLAG_NO_COLOR)
#define css_fully_booted		css_test_flag(&css_options, CSS_OPT_FLAG_FULLY_BOOTED)
#define css_opt_transcode_via_slin	css_test_flag(&css_options, CSS_OPT_FLAG_TRANSCODE_VIA_SLIN)
#define css_opt_dump_core		1//css_test_flag(&css_options, CSS_OPT_FLAG_DUMP_CORE)
#define css_opt_cache_record_files	css_test_flag(&css_options, CSS_OPT_FLAG_CACHE_RECORD_FILES)
#define css_opt_timestamp		1 //css_test_flag(&css_options, CSS_OPT_FLAG_TIMESTAMP)
#define css_opt_override_config		css_test_flag(&css_options, CSS_OPT_FLAG_OVERRIDE_CONFIG)
#define css_opt_reconnect		css_test_flag(&css_options, CSS_OPT_FLAG_RECONNECT)
#define css_opt_transmit_silence	css_test_flag(&css_options, CSS_OPT_FLAG_TRANSMIT_SILENCE)
#define css_opt_dont_warn		css_test_flag(&css_options, CSS_OPT_FLAG_DONT_WARN)
#define css_opt_end_cdr_before_h_exten	css_test_flag(&css_options, CSS_OPT_FLAG_END_CDR_BEFORE_H_EXTEN)
#define css_opt_internal_timing		css_test_flag(&css_options, CSS_OPT_FLAG_INTERNAL_TIMING)
#define css_opt_always_fork		css_test_flag(&css_options, CSS_OPT_FLAG_ALWAYS_FORK)
#define css_opt_mute			css_test_flag(&css_options, CSS_OPT_FLAG_MUTE)
#define css_opt_dbg_module		css_test_flag(&css_options, CSS_OPT_FLAG_DEBUG_MODULE)
#define css_opt_verb_module		css_test_flag(&css_options, CSS_OPT_FLAG_VERBOSE_MODULE)
#define css_opt_light_background	css_test_flag(&css_options, CSS_OPT_FLAG_LIGHT_BACKGROUND)
#define css_opt_force_black_background	css_test_flag(&css_options, CSS_OPT_FLAG_FORCE_BLACK_BACKGROUND)
#define css_opt_hide_connect		1//css_test_flag(&css_options, CSS_OPT_FLAG_HIDE_CONSOLE_CONNECT)
#define css_opt_lock_confdir		css_test_flag(&css_options, CSS_OPT_FLAG_LOCK_CONFIG_DIR)
#define css_opt_generic_plc         css_test_flag(&css_options, CSS_OPT_FLAG_GENERIC_PLC)

extern struct css_flags css_options;

enum css_compat_flags {
	CSS_COMPAT_DELIM_PBX_REALTIME = (1 << 0),
	CSS_COMPAT_DELIM_RES_AGI = (1 << 1),
	CSS_COMPAT_APP_SET = (1 << 2),
};

#define	css_compat_pbx_realtime	css_test_flag(&css_compat, CSS_COMPAT_DELIM_PBX_REALTIME)
#define css_compat_res_agi	css_test_flag(&css_compat, CSS_COMPAT_DELIM_RES_AGI)
#define	css_compat_app_set	css_test_flag(&css_compat, CSS_COMPAT_APP_SET)

extern struct css_flags css_compat;

extern int option_verbose;
extern int option_maxfiles;		/*!< Max number of open file handles (files, sockets) */
extern int option_debug;		/*!< Debugging */
extern int option_maxcalls;		/*!< Maximum number of simultaneous channels */
extern double option_maxload;
#if defined(HAVE_SYSINFO)
extern long option_minmemfree;		/*!< Minimum amount of free system memory - stop accepting calls if free memory falls below this watermark */
#endif
//extern char defaultlanguage[];

extern struct timeval css_startuptime;
extern struct timeval css_lcssreloadtime;
extern pid_t css_mainpid;

//extern char record_cache_dir[CSS_CACHE_DIR_LEN];
//extern char dahdi_chan_name[CSS_CHANNEL_NAME];
//extern int dahdi_chan_name_len;

//extern int css_language_is_prefix;


#ifdef	__cplusplus
}
#endif

#endif	/* OPTIONS_H */

