/* 
 * File: css_player_background.c
 * Author: root
 *
 * Created on April 16, 2014, 6:29 PM
 */
/* These includes are all about ordering */

#include "cssplayer.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/un.h>
#include <sys/prctl.h>

#include "css_monitor.h"
#include "_private.h"
#include "utils.h"
#include "cli.h"
#include "term.h"
#include "editline/histedit.h"
#include "strings.h"
#include "config.h"
#include "logger.h"
#include "io.h"

#define CSS_MAX_CONNECTS 128

#define CSSPLAYERSERVER_PROMPT "*CLI> "

#define CSSPLAYERSERVER_PROMPT2 "%s*CLI> "

#define WELCOME_MESSAGE \
    css_verbose("CSSPLAYERSERVER %s, Copyright (C) CSS.\n" \
                "=========================================================================\n", "C.1.0.0") \
                
#define MAX_HISTORY_COMMAND_LENGTH 256
#define DEFAULT_CONFIG_FILE "/etc/cssplayer/cssplayer.conf"

#define DEFAULT_CSS_SERVER_TYPE "Background"
#define DEFAULT_CSS_LISTEN_PORT "5060"
#define DEFAULT_CSS_LISTEN_TYPE "TCP"

static char *handle_show_settings(struct css_cli_entry *e, int cmd, struct css_cli_args *a);
static struct  css_cli_entry cli_cssplayer[] = {
    CSS_CLI_DEFINE(handle_show_settings, "Show some core settings"),

};

struct css_flags css_options = { CSS_DEFAULT_OPTIONS };
static const char *css_config_CSS_SOCKET = "/etc/cssplayer/cssplayerserver.ctl";
static const char *css_config_CSS_PID = "/etc/cssplayer/cssplayerserver.pid";
static char css_config_CSS_CTL_OWNER[PATH_MAX] = "\0";
static char css_config_CSS_CTL_GROUP[PATH_MAX] = "\0";
static char css_config_CSS_CTL[PATH_MAX] = "/etc/cssplayer";
static char css_config_CSS_CTL_PERMISSIONS[PATH_MAX] = "0660";
static char css_server_type[80];
static char css_listen_type[12];
static char css_listen_port[12];

static pthread_t lthread;
static char *_argv[256];
static int restartnow;
static pthread_t consolethread = CSS_PTHREADT_NULL;
static pthread_t css_player_background;
static pthread_t mon_sig_flags;
static pthread_t lthread;
static int css_consock = -1;
static int css_socket = -1;
static int shuttingdown;
static History *el_hist;
static EditLine *el;
static char *remotehostname;

pid_t css_mainpid;

static void quit_handler(int num, int niceness, int safeshutdown, int restart);

static int css_el_add_history(char *);
static int css_el_read_history(char *);
static int css_el_write_history(char *);
static int css_el_initialize(void);

struct console {
    int fd;				/*!< File descriptor */
    int p[2];			/*!< Pipe */
    pthread_t t;			/*!< Thread of handler */
    int mute;			/*!< Is the console muted for logs */
    int uid;			/*!< Remote user ID. */
    int gid;			/*!< Remote group ID. */
    int levels[NUMLOGLEVELS];	/*!< Which log levels are enabled for the console */
};

struct console consoles[CSS_MAX_CONNECTS];

static int sig_alert_pipe[2] = { -1, -1 };
static struct {
     unsigned int need_reload:1;
     unsigned int need_quit:1;
     unsigned int need_quit_handler:1;
} sig_flags;


static struct sigaction ignore_sig_handler = {
    .sa_handler = SIG_IGN,
};

static void _child_handler(int sig)
{
    /* Must not ever ast_log or ast_verbose within signal handler */
    int n, status, save_errno = errno;

    /*
     * Reap all dead children -- not just one
     */
    for (n = 0; wait4(-1, &status, WNOHANG, NULL) > 0; n++)
            ;
    if (n == 0 )	
        printf("Huh?  Child handler, but nobody there?\n");
    errno = save_errno;
}

static struct sigaction child_handler = {
    .sa_handler = _child_handler,
    .sa_flags = SA_RESTART,
};

/* Urgen handler
 * 
 * Called by soft_hanup to interrupt the poll read or other
 * system call. we done't actually need to do anything though
 * Remember: Cannot EVER log from within a signal handler
 */
static void _urg_handler(int num)
{
    return;
}

static struct sigaction urg_handler = {
    .sa_handler = _urg_handler,
    .sa_flags = SA_RESTART,
};

static void __quit_handler(int num)
{
    int a = 0;
    sig_flags.need_quit = 1;
    if (sig_alert_pipe[1] != -1) {
        if (write(sig_alert_pipe[1], &a, sizeof(a)) < 0) {
            fprintf(stderr, "quit_handler: write() failed: %s\n", strerror(errno));
        }
    }
    /* There is no need to restore the signal handler here, since the app
     * is going to exit */
}

static void _hup_handler(int num)
{
    int a = 0, save_errno = errno;
    if (option_verbose > 1)
        printf("Received HUP signal -- Reloading configs\n");
    if (restartnow)
        execvp(_argv[0], _argv);
    sig_flags.need_reload = 1;
    if (sig_alert_pipe[1] != -1) {
        if (write(sig_alert_pipe[1], &a, sizeof(a)) < 0) {
            fprintf(stderr, "hup_handler: write() failed: %s\n", strerror(errno));
        }
    }
    errno = save_errno;
}

static struct sigaction hup_handler = {
    .sa_handler = _hup_handler,
    .sa_flags = SA_RESTART,
};


/*! \brief Give an overview of core settings */
static char *handle_show_settings(struct css_cli_entry *e, int cmd, struct css_cli_args *a)
{
	char buf[BUFSIZ];
	char eid_str[128];

	switch (cmd) {
	case CLI_INIT:
		e->command = "core show settings";
		e->usage = "Usage: core show settings\n"
			   "       Show core misc settings";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
        
	css_cli(a->fd, "css_server_type:%s\n", S_OR(css_server_type, "Unknow"));
        css_cli(a->fd, "css_listen_port:%s\n", S_OR(css_listen_port, "Unknow"));
        css_cli(a->fd, "css_listen_type:%s\n", S_OR(css_listen_type, "Unknow"));
        
        css_log(LOG_NOTICE, "Type: %s, Port: %s, L_Type: %s\n", css_server_type, css_listen_port, css_listen_type);
        
	return CLI_SUCCESS;
}

static void css_readconfig(void) 
{
    struct css_config *cfg;
    struct css_variable *v;
    char *config = DEFAULT_CONFIG_FILE;
    char hostname[MAXHOSTNAMELEN] = "";
    struct css_flags config_flags = { CONFIG_FLAG_NOREALTIME };
    struct {
        unsigned int dbdir:1;
        unsigned int keydir:1;
    } found = { 0, 0 };


    cfg = css_config_load2(config, "" /* core, can't reload */, config_flags);

    css_copy_string(css_server_type, DEFAULT_CSS_SERVER_TYPE, sizeof(css_server_type));
    css_copy_string(css_listen_port, DEFAULT_CSS_LISTEN_PORT, sizeof(css_listen_port));
    css_copy_string(css_listen_type, DEFAULT_CSS_LISTEN_TYPE, sizeof(css_listen_type));

    /* init with buildtime config */

    //css_set_default_eid(&css_eid_default);

    /* no cssplayer.conf? no problem, use buildtime config! */
    if (cfg == CONFIG_STATUS_FILEMISSING || cfg == CONFIG_STATUS_FILEUNCHANGED || cfg == CONFIG_STATUS_FILEINVALID) {
        return;
    }

    for (v = css_variable_browse(cfg, "general"); v; v = v->next) {
        if (!strcasecmp(v->name, "cssserver_type"))
                css_copy_string(css_server_type, v->value, sizeof(css_server_type));
        else if (!strcasecmp(v->name, "listen_port"))
                css_copy_string(css_listen_port, v->value, sizeof(css_listen_port));
        else if (!strcasecmp(v->name, "listen_type"))
                css_copy_string(css_listen_type, v->value, sizeof(css_listen_type));
    }

    css_config_destroy(cfg);
}


static void *monitor_sig_flags(void *unused)
{
    for (;;) {
        struct pollfd p = { sig_alert_pipe[0], POLLIN, 0 };
        int a;
        poll(&p, 1, -1);
        if (sig_flags.need_reload) {
            sig_flags.need_reload = 0;
           // ast_module_reload(NULL);
        }
        if (sig_flags.need_quit) {
            sig_flags.need_quit = 0;
            if (consolethread != CSS_PTHREADT_NULL) {
                    sig_flags.need_quit_handler = 1;
                    pthread_kill(consolethread, SIGURG);
            } else {
                   quit_handler(0, 0, 1, 0);
            }
        }
        if (read(sig_alert_pipe[0], &a, sizeof(a)) != sizeof(a)) {
        }
    }

    return NULL;
}

static int css_el_add_history(char *buf)
{
    HistEvent ev;

    if (el_hist == NULL || el == NULL)
        css_el_initialize();
    if (strlen(buf) > (MAX_HISTORY_COMMAND_LENGTH - 1))
        return 0;
    return (history(el_hist, &ev, H_ENTER, css_strip(css_strdupa(buf))));
}

static int css_el_write_history(char *filename)
{
    HistEvent ev;

    if (el_hist == NULL || el == NULL)
        css_el_initialize();

    return (history(el_hist, &ev, H_SAVE, filename));
}


/* Sending commands from consoles back to the daemon requires a terminating NULL */
static int fdsend(int fd, const char *s)
{
    return write(fd, s, strlen(s) + 1);
}

/* Sending messages from the daemon back to the display requires _excluding_ the terminating NULL */
static int fdprint(int fd, const char *s)
{
    return write(fd, s, strlen(s));
}

/*!
 * \brief mute or unmute a console from logging
 */
void css_console_toggle_mute(int fd, int silent) 
{
    int x;
    for (x = 0;x < CSS_MAX_CONNECTS; x++) {
        if (fd == consoles[x].fd) {
            if (consoles[x].mute) {
                consoles[x].mute = 0;
                if (!silent)
                    css_cli(fd, "Console is not muted anymore.\n");
            } else {
                consoles[x].mute = 1;
                if (!silent)
                    css_cli(fd, "Console is muted.\n");
            }
            return;
        }
    }
    css_cli(fd, "Couldn't find remote console.\n");
}

static void css_network_puts_mutable(const char *string, int level)
{
    int x;
    for (x = 0;x < CSS_MAX_CONNECTS; x++) {
        if (consoles[x].mute)
            continue;
        if (consoles[x].fd > -1) {
            if (!consoles[x].levels[level]) 
                fdprint(consoles[x].p[1], string);
        }
    }
}

void css_console_puts_mutable(const char *string, int level)
{
    fputs(string, stdout);
    fflush(stdout);
    css_network_puts_mutable(string, level);
}

/*!
 * \brief enable or disable a logging level to a specified console
 */
void css_console_toggle_loglevel(int fd, int level, int state)
{
    int x;
    for (x = 0;x < CSS_MAX_CONNECTS; x++) {
        if (fd == consoles[x].fd) {
            /*
             * Since the logging occurs when levels are false, set to
             * flipped iinput because this function accepts 0 as off and 1 as on
             */
            consoles[x].levels[level] = state ? 0 : 1;
            return;
        }
    }
}

static void quit_handler(int num, int niceness, int safeshutdown, int restart)
{
    char filename[80] = "";
    time_t s,e;
    int x;
    /* Try to get as many CDRs as possible submitted to the backend engines (if in batch mode) */
    //ast_cdr_engine_term();
    if (safeshutdown) {
        shuttingdown = 1;
        if (!niceness) {
            /* Begin shutdown routine, hanging up active channels */
            //ast_begin_shutdown(1);
            if (option_verbose && css_opt_console)
                css_verbose("Beginning cssser %s....\n", restart ? "restart" : "shutdown");
            time(&s);
            for (;;) {
                time(&e);
                /* Wait up to 15 seconds for all channels to go away */
                if ((e - s) > 15)
                        break;
                //if (!ast_active_channels())
                //	break;
                if (!shuttingdown)
                        break;
                /* Sleep 1/10 of a second */
                usleep(100000);
            }
        } else {
            if (niceness < 2)
                //ast_begin_shutdown(0);
            if (option_verbose && css_opt_console)
                css_verbose("Waiting for inactivity to perform %s...\n", restart ? "restart" : "halt");
            for (;;) {
        //	if (!ast_active_channels())
                //	break;
                if (!shuttingdown)
                        break;
                sleep(1);
            }
        }

        if (!shuttingdown) {
            if (option_verbose && css_opt_console)
                css_verbose("Css %s cancelled.\n", restart ? "restart" : "shutdown");
            return;
        }

        //if (niceness)
            //ast_module_shutdown();
    }
    if (css_opt_console || (css_opt_remote && !css_opt_exec)) {
        if (getenv("HOME")) {
            snprintf(filename, sizeof(filename), "%s/.cssplayerserver_history", getenv("HOME"));
        }
        if (!css_strlen_zero(filename)) {
            css_el_write_history(filename);
        }
        if (consolethread == CSS_PTHREADT_NULL || consolethread == pthread_self()) {
            /* Only end if we are the consolethread, otherwise there's a race with that thread. */
            if (el != NULL) {
                el_end(el);
            }
            if (el_hist != NULL) {
                history_end(el_hist);
            }
        } else if (mon_sig_flags == pthread_self()) {
            if (consolethread != CSS_PTHREADT_NULL) {
                pthread_kill(consolethread, SIGURG);
            }
        }
    }
    if (option_verbose)
        css_verbose("Executing last minute cleanups\n");
    //ast_run_atexits();
    /* Called on exit */
    //if (option_verbose && css_opt_console)
        //css_verbose("CSS %s ending (%d).\n", ast_active_channels() ? "uncleanly" : "cleanly", num);
    css_debug(1, "CSS ending (%d).\n", num);
    //manager_event(EVENT_FLAG_SYSTEM, "Shutdown", "Shutdown: %s\r\nRestart: %s\r\n", ast_active_channels() ? "Uncleanly" : "Cleanly", restart ? "True" : "False");
    if (css_socket > -1) {
        pthread_cancel(lthread);
        close(css_socket);
        css_socket = -1;
        unlink(css_config_CSS_SOCKET);
    }
    if (css_consock > -1)
        close(css_consock);
    if (!css_opt_remote)
        unlink(css_config_CSS_PID);
    printf("%s", term_quit());
    if (restart) {
        if (option_verbose || css_opt_console)
            css_verbose("Preparing for cssplayer restart...\n");
        /* Mark all FD's for closing on exec */
        for (x=3; x < 32768; x++) {
            fcntl(x, F_SETFD, FD_CLOEXEC);
        }
        if (option_verbose || css_opt_console)
            css_verbose("cssplayer is now restarting...\n");
        restartnow = 1;

        /* close logger */
        close_logger();

        /* If there is a consolethread running send it a SIGHUP 
           so it can execvp, otherwise we can do it ourselves */
        if ((consolethread != CSS_PTHREADT_NULL) && (consolethread != pthread_self())) {
            pthread_kill(consolethread, SIGHUP);
            /* Give the signal handler some time to complete */
            sleep(2);
        } else
            execvp(_argv[0], _argv);
    } else {
        /* close logger */
        close_logger();
    }
    exit(0);
}

static void network_verboser(const char *s)
{
    css_network_puts_mutable(s, __LOG_VERBOSE);
}

#undef SO_PEERCRED
static int read_credentials(int fd, char *buffer, size_t size, struct console *con)
{
#if defined(SO_PEERCRED)
    struct ucred cred;
    socklen_t len = sizeof(cred);
#endif
#if defined(HAVE_GETPEEREID)
    uid_t uid;
    gid_t gid;
#else
    int uid, gid;
#endif
    int result;

    result = read(fd, buffer, size);
    if (result < 0) {
            return result;
    }

#if defined(SO_PEERCRED) && (defined(HAVE_STRUCT_UCRED_UID) || defined(HAVE_STRUCT_UCRED_CR_UID))
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len)) {
            return result;
    }
#if defined(HAVE_STRUCT_UCRED_UID)
    uid = cred.uid;
    gid = cred.gid;
#else /* defined(HAVE_STRUCT_UCRED_CR_UID) */
    uid = cred.cr_uid;
    gid = cred.cr_gid;
#endif /* defined(HAVE_STRUCT_UCRED_UID) */

#elif defined(HAVE_GETPEEREID)
    if (getpeereid(fd, &uid, &gid)) {
            return result;
    }
#else
    return result;
#endif
    con->uid = uid;
    con->gid = gid;

    return result;
}

static void *netconsole(void *vconsole)
{
    struct console *con = vconsole;
    char hostname[MAXHOSTNAMELEN] = "";
    char tmp[512];
    int res;
    struct pollfd fds[2];

    if (gethostname(hostname, sizeof(hostname)-1))
        css_copy_string(hostname, "<Unknown>", sizeof(hostname));
    snprintf(tmp, sizeof(tmp), "%s/%ld/%s\n", hostname, (long)css_mainpid, "1.0.0");
    fdprint(con->fd, tmp);
    for (;;) {
        fds[0].fd = con->fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = con->p[0];
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        res = poll(fds, 2, -1);
        if (res < 0) {
            if (errno != EINTR)
                css_log(LOG_WARNING, "poll returned < 0: %s\n", strerror(errno));
            continue;
        }
        if (fds[0].revents) {
            res = read_credentials(con->fd, tmp, sizeof(tmp) - 1, con);
            if (res < 1) {
                break;
            }
            tmp[res] = 0;
            if (strncmp(tmp, "cli quit after ", 15) == 0) {
                css_cli_command_multiple_full(con->uid, con->gid, con->fd, res - 15, tmp + 15);
                break;
            }
            css_cli_command_multiple_full(con->uid, con->gid, con->fd, res, tmp);
        }
        if (fds[1].revents) {
            res = read_credentials(con->p[0], tmp, sizeof(tmp), con);
            if (res < 1) {
                css_log(LOG_ERROR, "read returned %d\n", res);
                break;
            }
            res = write(con->fd, tmp, res);
            if (res < 1)
                break;
        }
    }
	if (!css_opt_hide_connect) {
            css_verb(3, "Remote UNIX connection disconnected\n");
	}
	close(con->fd);
	close(con->p[0]);
	close(con->p[1]);
	con->fd = -1;
	
	return NULL;
}

static void *listener(void *unused)
{
    struct sockaddr_un sunaddr;
    int s;
    socklen_t len;
    int x;
    int flags;
    struct pollfd fds[1];
    for (;;) {
        if (css_socket < 0)
            return NULL;
        fds[0].fd = css_socket;
        fds[0].events = POLLIN;
        s = poll(fds, 1, -1);
        pthread_testcancel();
        if (s < 0) {
            if (errno != EINTR)
                css_log(LOG_WARNING, "poll returned error: %s\n", strerror(errno));
            continue;
        }
        len = sizeof(sunaddr);
        s = accept(css_socket, (struct sockaddr *)&sunaddr, &len);
        if (s < 0) {
            if (errno != EINTR)
                css_log(LOG_WARNING, "Accept returned %d: %s\n", s, strerror(errno));
        } else {
#if !defined(SO_PASSCRED)
                {
#else
                int sckopt = 1;
                /* turn on socket credentials passing. */
                if (setsockopt(s, SOL_SOCKET, SO_PASSCRED, &sckopt, sizeof(sckopt)) < 0) {
                    css_log(LOG_WARNING, "Unable to turn on socket credentials passing\n");
                } else {
#endif
                    for (x = 0; x < CSS_MAX_CONNECTS; x++) {
                        if (consoles[x].fd >= 0) {
                                continue;
                        }
                        if (socketpair(AF_LOCAL, SOCK_STREAM, 0, consoles[x].p)) {
                                css_log(LOG_ERROR, "Unable to create pipe: %s\n", strerror(errno));
                                consoles[x].fd = -1;
                                fdprint(s, "Server failed to create pipe\n");
                                close(s);
                                break;
                        }
                        flags = fcntl(consoles[x].p[1], F_GETFL);
                        fcntl(consoles[x].p[1], F_SETFL, flags | O_NONBLOCK);
                        consoles[x].fd = s;
                        consoles[x].mute = 1; /* Default is muted, we will un-mute if necessary */
                        /* Default uid and gid to -2, so then in cli.c/cli_has_permissions() we will be able
                           to know if the user didn't send the credentials. */
                        consoles[x].uid = -2;
                        consoles[x].gid = -2;
                        if (css_pthread_create_detached_background(&consoles[x].t, NULL, netconsole, &consoles[x])) {
                            css_log(LOG_ERROR, "Unable to spawn thread to handle connection: %s\n", strerror(errno));
                            close(consoles[x].p[0]);
                            close(consoles[x].p[1]);
                            consoles[x].fd = -1;
                            fdprint(s, "Server failed to spawn thread\n");
                            close(s);
                        }
                        break;
                    }
                    if (x >= CSS_MAX_CONNECTS) {
                        fdprint(s, "No more connections allowed\n");
                        css_log(LOG_WARNING, "No more connections allowed\n");
                        close(s);
                    } else if ((consoles[x].fd > -1) && (!css_opt_hide_connect)) {
                        css_verb(3, "Remote UNIX connection\n");
                    }
                }
            }
    }
    return NULL;
}

static int css_makesocket(void)
{
	struct sockaddr_un sunaddr;
	int res;
	int x;
	uid_t uid = -1;
	gid_t gid = -1;

	for (x = 0; x < CSS_MAX_CONNECTS; x++)	
		consoles[x].fd = -1;
	unlink(css_config_CSS_SOCKET);
	css_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (css_socket < 0) {
		css_log(LOG_WARNING, "Unable to create control socket: %s\n", strerror(errno));
		return -1;
	}		
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_LOCAL;
	css_copy_string(sunaddr.sun_path, css_config_CSS_SOCKET, sizeof(sunaddr.sun_path));
	res = bind(css_socket, (struct sockaddr *)&sunaddr, sizeof(sunaddr));
	if (res) {
		css_log(LOG_WARNING, "Unable to bind socket to %s: %s\n", css_config_CSS_SOCKET, strerror(errno));
		close(css_socket);
		css_socket = -1;
		return -1;
	}
	res = listen(css_socket, 2);
	if (res < 0) {
		css_log(LOG_WARNING, "Unable to listen on socket %s: %s\n", css_config_CSS_SOCKET, strerror(errno));
		close(css_socket);
		css_socket = -1;
		return -1;
	}

	if (css_register_verbose(network_verboser)) {
		css_log(LOG_WARNING, "Unable to register network verboser?\n");
	}

	css_pthread_create_background(&lthread, NULL, listener, NULL);

	if (!css_strlen_zero(css_config_CSS_CTL_OWNER)) {
		struct passwd *pw;
		if ((pw = getpwnam(css_config_CSS_CTL_OWNER)) == NULL)
			css_log(LOG_WARNING, "Unable to find uid of user %s\n", css_config_CSS_CTL_OWNER);
		else
			uid = pw->pw_uid;
	}
		
	if (!css_strlen_zero(css_config_CSS_CTL_GROUP)) {
		struct group *grp;
		if ((grp = getgrnam(css_config_CSS_CTL_GROUP)) == NULL)
			css_log(LOG_WARNING, "Unable to find gid of group %s\n", css_config_CSS_CTL_GROUP);
		else
			gid = grp->gr_gid;
	}

	if (chown(css_config_CSS_SOCKET, uid, gid) < 0)
		css_log(LOG_WARNING, "Unable to change ownership of %s: %s\n", css_config_CSS_SOCKET, strerror(errno));

	if (!css_strlen_zero(css_config_CSS_CTL_PERMISSIONS)) {
		int p1;
		mode_t p;
		sscanf(css_config_CSS_CTL_PERMISSIONS, "%30o", &p1);
		p = p1;
		if ((chmod(css_config_CSS_SOCKET, p)) < 0)
			css_log(LOG_WARNING, "Unable to change file permissions of %s: %s\n", css_config_CSS_SOCKET, strerror(errno));
	}

	return 0;
}

static int css_tryconnect(void)
{
    struct sockaddr_un sunaddr;
    int res;
    css_consock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (css_consock < 0) {
        css_log(LOG_WARNING, "Unable to create socket: %s\n", strerror(errno));
        return 0;
    }
    memset(&sunaddr, 0, sizeof(sunaddr));
    sunaddr.sun_family = AF_LOCAL;
    css_copy_string(sunaddr.sun_path, css_config_CSS_SOCKET, sizeof(sunaddr.sun_path));
    res = connect(css_consock, (struct sockaddr *)&sunaddr, sizeof(sunaddr));
    if (res) {
        close(css_consock);
        css_consock = -1;
        return 0;
    } else
        return 1;
}

static int css_el_read_char(EditLine *editline, char *cp)
{
	int num_read = 0;
	int lastpos = 0;
	struct pollfd fds[2];
	int res;
	int max;
#define EL_BUF_SIZE 512
	char buf[EL_BUF_SIZE];

	for (;;) {
		max = 1;
		fds[0].fd = css_consock;
		fds[0].events = POLLIN;
		if (!css_opt_exec) {
			fds[1].fd = STDIN_FILENO;
			fds[1].events = POLLIN;
			max++;
		}
		res = poll(fds, max, -1);
		if (res < 0) {
			if (sig_flags.need_quit || sig_flags.need_quit_handler)
				break;
			if (errno == EINTR)
				continue;
			css_log(LOG_ERROR, "poll failed: %s\n", strerror(errno));
			break;
		}

		if (!css_opt_exec && fds[1].revents) {
			num_read = read(STDIN_FILENO, cp, 1);
			if (num_read < 1) {
				break;
			} else 
				return (num_read);
		}
		if (fds[0].revents) {
			char *tmp;
			res = read(css_consock, buf, sizeof(buf) - 1);
			/* if the remote side disappears exit */
			if (res < 1) {
				fprintf(stderr, "\nDisconnected from cssplayer server\n");
				if (!css_opt_reconnect) {
					quit_handler(0, 0, 0, 0);
				} else {
					int tries;
					int reconnects_per_second = 20;
					fprintf(stderr, "Attempting to reconnect for 30 seconds\n");
					for (tries = 0; tries < 30 * reconnects_per_second; tries++) {
						if (css_tryconnect()) {
							fprintf(stderr, "Reconnect succeeded after %.3f seconds\n", 1.0 / reconnects_per_second * tries);
							printf("%s", term_quit());
							WELCOME_MESSAGE;
							if (!css_opt_mute)
								fdsend(css_consock, "logger mute silent");
							else 
								printf("log and verbose output currently muted ('logger mute' to unmute)\n");
							break;
						} else
							usleep(1000000 / reconnects_per_second);
					}
					if (tries >= 30 * reconnects_per_second) {
						fprintf(stderr, "Failed to reconnect for 30 seconds.  Quitting.\n");
						quit_handler(0, 0, 0, 0);
					}
				}
			}

			buf[res] = '\0';

			/* Strip preamble from asynchronous events, too */
			for (tmp = buf; *tmp; tmp++) {
				if (*tmp == 127) {
					memmove(tmp, tmp + 1, strlen(tmp));
					tmp--;
					res--;
				}
			}

			/* Write over the CLI prompt */
			if (!css_opt_exec && !lastpos) {
				if (write(STDOUT_FILENO, "\r[0K", 5) < 0) {
				}
			}
			if (write(STDOUT_FILENO, buf, res) < 0) {
			}
			if ((res < EL_BUF_SIZE - 1) && ((buf[res-1] == '\n') || (buf[res-2] == '\n'))) {
				*cp = CC_REFRESH;
				return(1);
			} else
				lastpos = 1;
		}
	}

	*cp = '\0';
	return (0);
}

static struct css_str *prompt = NULL;

static char *cli_prompt(EditLine *editline)
{
	char tmp[100];
	char *pfmt;
	int color_used = 0;
	static int cli_prompt_changes = 0;
	char term_code[20];
	struct passwd *pw;
	struct group *gr;

	if (prompt == NULL) {
		prompt = css_str_create(100);
	} else if (!cli_prompt_changes) {
		return css_str_buffer(prompt);
	} else {
		css_str_reset(prompt);
	}

	if ((pfmt = getenv("CSSSER_PROMPT"))) {
		char *t = pfmt;
		struct timeval ts = css_tvnow();
		while (*t != '\0') {
			if (*t == '%') {
				char hostname[MAXHOSTNAMELEN] = "";
				int i, which;
				struct css_tm tm = { 0, };
				int fgcolor = COLOR_WHITE, bgcolor = COLOR_BLACK;

				t++;
				switch (*t) {
				case 'C': /* color */
					t++;
					if (sscanf(t, "%30d;%30d%n", &fgcolor, &bgcolor, &i) == 2) {
                                                css_str_append(&prompt, 0, "%s", term_color_code(term_code, fgcolor, bgcolor, sizeof(term_code)));
						t += i - 1;
					} else if (sscanf(t, "%30d%n", &fgcolor, &i) == 1) {
						css_str_append(&prompt, 0, "%s", term_color_code(term_code, fgcolor, 0, sizeof(term_code)));
						t += i - 1;
					}

					/* If the color has been reset correctly, then there's no need to reset it later */
					color_used = ((fgcolor == COLOR_WHITE) && (bgcolor == COLOR_BLACK)) ? 0 : 1;
					break;
				case 'd': /* date */
					if (css_localtime(&ts, &tm, NULL)) {
						css_strftime(tmp, sizeof(tmp), "%Y-%m-%d", &tm);
						css_str_append(&prompt, 0, "%s", tmp);
						cli_prompt_changes++;
					}
					break;
				case 'g': /* group */
					if ((gr = getgrgid(getgid()))) {
						css_str_append(&prompt, 0, "%s", gr->gr_name);
					}
					break;
				case 'h': /* hostname */
					if (!gethostname(hostname, sizeof(hostname) - 1)) {
						css_str_append(&prompt, 0, "%s", hostname);
					} else {
						css_str_append(&prompt, 0, "%s", "localhost");
					}
					break;
				case 'H': /* short hostname */
					if (!gethostname(hostname, sizeof(hostname) - 1)) {
						char *dotptr;
						if ((dotptr = strchr(hostname, '.'))) {
							*dotptr = '\0';
						}
						css_str_append(&prompt, 0, "%s", hostname);
					} else {
						css_str_append(&prompt, 0, "%s", "localhost");
					}
					break;
#ifdef HAVE_GETLOADAVG
				case 'l': /* load avg */
					t++;
					if (sscanf(t, "%30d", &which) == 1 && which > 0 && which <= 3) {
						double list[3];
						getloadavg(list, 3);
						css_str_append(&prompt, 0, "%.2f", list[which - 1]);
						cli_prompt_changes++;
					}
					break;
#endif
				case 's': /* cssser system name (from cssser.conf) */
					css_str_append(&prompt, 0, "%s", "CSSPLAYSERVER");
					break;
				case 't': /* time */
					if (css_localtime(&ts, &tm, NULL)) {
						css_strftime(tmp, sizeof(tmp), "%H:%M:%S", &tm);
						css_str_append(&prompt, 0, "%s", tmp);
						cli_prompt_changes++;
					}
					break;
				case 'u': /* username */
					if ((pw = getpwuid(getuid()))) {
						css_str_append(&prompt, 0, "%s", pw->pw_name);
					}
					break;
				case '#': /* process console or remote? */
					css_str_append(&prompt, 0, "%c", css_opt_remote ? '>' : '#');
					break;
				case '%': /* literal % */
					css_str_append(&prompt, 0, "%c", '%');
					break;
				case '\0': /* % is last character - prevent bug */
					t--;
					break;
				}
			} else {
				css_str_append(&prompt, 0, "%c", *t);
			}
			t++;
		}
		if (color_used) {
			/* Force colors back to normal at end */
			css_str_append(&prompt, 0, "%s", term_color_code(term_code, 0, 0, sizeof(term_code)));
		}
	} else if (remotehostname) {
		css_str_set(&prompt, 0, CSSPLAYERSERVER_PROMPT2, remotehostname);
	} else {
		css_str_set(&prompt, 0, "%s", CSSPLAYERSERVER_PROMPT);
	}

	return css_str_buffer(prompt);	
}


static void __remote_quit_handler(int num)
{
	sig_flags.need_quit = 1;
}

static char **css_el_strtoarr(char *buf)
{
	char **match_list = NULL, **match_list_tmp, *retstr;
	size_t match_list_len;
	int matches = 0;

	match_list_len = 1;
	while ( (retstr = strsep(&buf, " ")) != NULL) {

		if (!strcmp(retstr, CSS_CLI_COMPLETE_EOF))
			break;
		if (matches + 1 >= match_list_len) {
			match_list_len <<= 1;
			if ((match_list_tmp = css_realloc(match_list, match_list_len * sizeof(char *)))) {
				match_list = match_list_tmp;
			} else {
				if (match_list)
					css_free(match_list);
				return (char **) NULL;
			}
		}

		match_list[matches++] = css_strdup(retstr);
	}

	if (!match_list)
		return (char **) NULL;

	if (matches >= match_list_len) {
		if ((match_list_tmp = css_realloc(match_list, (match_list_len + 1) * sizeof(char *)))) {
			match_list = match_list_tmp;
		} else {
			if (match_list)
				css_free(match_list);
			return (char **) NULL;
		}
	}

	match_list[matches] = (char *) NULL;

	return match_list;
}


static int css_el_sort_compare(const void *i1, const void *i2)
{
	char *s1, *s2;

	s1 = ((char **)i1)[0];
	s2 = ((char **)i2)[0];

	return strcasecmp(s1, s2);
}

static int css_cli_display_match_list(char **matches, int len, int max)
{
	int i, idx, limit, count;
	int screenwidth = 0;
	int numoutput = 0, numoutputline = 0;

	screenwidth = css_get_termcols(STDOUT_FILENO);

	/* find out how many entries can be put on one line, with two spaces between strings */
	limit = screenwidth / (max + 2);
	if (limit == 0)
		limit = 1;

	/* how many lines of output */
	count = len / limit;
	if (count * limit < len)
		count++;

	idx = 1;

	qsort(&matches[0], (size_t)(len), sizeof(char *), css_el_sort_compare);

	for (; count > 0; count--) {
		numoutputline = 0;
		for (i = 0; i < limit && matches[idx]; i++, idx++) {

			/* Don't print dupes */
			if ( (matches[idx+1] != NULL && strcmp(matches[idx], matches[idx+1]) == 0 ) ) {
				i--;
				css_free(matches[idx]);
				matches[idx] = NULL;
				continue;
			}

			numoutput++;
			numoutputline++;
			fprintf(stdout, "%-*s  ", max, matches[idx]);
			css_free(matches[idx]);
			matches[idx] = NULL;
		}
		if (numoutputline > 0)
			fprintf(stdout, "\n");
	}

	return numoutput;
}

static char *cli_complete(EditLine *editline, int ch)
{
	int len = 0;
	char *ptr;
	int nummatches = 0;
	char **matches;
	int retval = CC_ERROR;
	char buf[2048], savechr;
	int res;

	LineInfo *lf = (LineInfo *)el_line(editline);

	savechr = *(char *)lf->cursor;
	*(char *)lf->cursor = '\0';
	ptr = (char *)lf->cursor;
	if (ptr) {
		while (ptr > lf->buffer) {
			if (isspace(*ptr)) {
				ptr++;
				break;
			}
			ptr--;
		}
	}

	len = lf->cursor - ptr;

	if (css_opt_remote) {
		snprintf(buf, sizeof(buf), "_COMMAND NUMMATCHES \"%s\" \"%s\"", lf->buffer, ptr); 
		fdsend(css_consock, buf);
		res = read(css_consock, buf, sizeof(buf) - 1);
		buf[res] = '\0';
		nummatches = atoi(buf);

		if (nummatches > 0) {
			char *mbuf;
			int mlen = 0, maxmbuf = 2048;
			/* Start with a 2048 byte buffer */			
			if (!(mbuf = css_malloc(maxmbuf))) {
				lf->cursor[0] = savechr;
				return (char *)(CC_ERROR);
			}
			snprintf(buf, sizeof(buf), "_COMMAND MATCHESARRAY \"%s\" \"%s\"", lf->buffer, ptr); 
			fdsend(css_consock, buf);
			res = 0;
			mbuf[0] = '\0';
			while (!strstr(mbuf, CSS_CLI_COMPLETE_EOF) && res != -1) {
				if (mlen + 1024 > maxmbuf) {
					/* Every step increment buffer 1024 bytes */
					maxmbuf += 1024;					
					if (!(mbuf = css_realloc(mbuf, maxmbuf))) {
						lf->cursor[0] = savechr;
						return (char *)(CC_ERROR);
					}
				}
				/* Only read 1024 bytes at a time */
				res = read(css_consock, mbuf + mlen, 1024);
				if (res > 0)
					mlen += res;
			}
			mbuf[mlen] = '\0';

			matches = css_el_strtoarr(mbuf);
			css_free(mbuf);
		} else
			matches = (char **) NULL;
	} else {
		char **p, *oldbuf=NULL;
		nummatches = 0;
		matches = css_cli_completion_matches((char *)lf->buffer,ptr);
		for (p = matches; p && *p; p++) {
			if (!oldbuf || strcmp(*p,oldbuf))
				nummatches++;
			oldbuf = *p;
		}
	}

	if (matches) {
		int i;
		int matches_num, maxlen, match_len;

		if (matches[0][0] != '\0') {
			el_deletestr(editline, (int) len);
			el_insertstr(editline, matches[0]);
			retval = CC_REFRESH;
		}

		if (nummatches == 1) {
			/* Found an exact match */
			el_insertstr(editline, " ");
			retval = CC_REFRESH;
		} else {
			/* Must be more than one match */
			for (i = 1, maxlen = 0; matches[i]; i++) {
				match_len = strlen(matches[i]);
				if (match_len > maxlen)
					maxlen = match_len;
			}
			matches_num = i - 1;
			if (matches_num >1) {
				fprintf(stdout, "\n");
				css_cli_display_match_list(matches, nummatches, maxlen);
				retval = CC_REDISPLAY;
			} else { 
				el_insertstr(editline," ");
				retval = CC_REFRESH;
			}
		}
		for (i = 0; matches[i]; i++)
			css_free(matches[i]);
		css_free(matches);
	}

	lf->cursor[0] = savechr;

	return (char *)(long)retval;
}

static int css_el_initialize(void)
{
	HistEvent ev;
	char *editor = getenv("AST_EDITOR");

	if (el != NULL)
		el_end(el);
	if (el_hist != NULL)
		history_end(el_hist);

	el = el_init("cssser", stdin, stdout, stderr);
	el_set(el, EL_PROMPT, cli_prompt);

	el_set(el, EL_EDITMODE, 1);		
	el_set(el, EL_EDITOR, editor ? editor : "emacs");		
	el_hist = history_init();
	if (!el || !el_hist)
		return -1;

	/* setup history with 100 entries */
	history(el_hist, &ev, H_SETSIZE, 100);

	el_set(el, EL_HIST, history, el_hist);

	el_set(el, EL_ADDFN, "ed-complete", "Complete argument", cli_complete);
	/* Bind <tab> to command completion */
	el_set(el, EL_BIND, "^I", "ed-complete", NULL);
	/* Bind ? to command completion */
	el_set(el, EL_BIND, "?", "ed-complete", NULL);
	/* Bind ^D to redisplay */
	el_set(el, EL_BIND, "^D", "ed-redisplay", NULL);

	return 0;
}


int css_safe_system(const char *s)
{
	pid_t pid;
	int res;
	struct rusage rusage;
	int status;

#if defined(HAVE_WORKING_FORK) || defined(HAVE_WORKING_VFORK)
	ast_replace_sigchld();

#ifdef HAVE_WORKING_FORK
	pid = fork();
#else
	pid = vfork();
#endif	

	if (pid == 0) {
#ifdef HAVE_CAP
		cap_t cap = cap_from_text("cap_net_admin-eip");

		if (cap_set_proc(cap)) {
			/* Careful with order! Logging cannot happen after we close FDs */
			ast_log(LOG_WARNING, "Unable to remove capabilities.\n");
		}
		cap_free(cap);
#endif
#ifdef HAVE_WORKING_FORK
		if (ast_opt_high_priority)
			ast_set_priority(0);
		/* Close file descriptors and launch system command */
		ast_close_fds_above_n(STDERR_FILENO);
#endif
		execl("/bin/sh", "/bin/sh", "-c", s, (char *) NULL);
		_exit(1);
	} else if (pid > 0) {
		for (;;) {
			res = wait4(pid, &status, 0, &rusage);
			if (res > -1) {
				res = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
				break;
			} else if (errno != EINTR) 
				break;
		}
	} else {
		ast_log(LOG_WARNING, "Fork failed: %s\n", strerror(errno));
		res = -1;
	}

	ast_unreplace_sigchld();
#else /* !defined(HAVE_WORKING_FORK) && !defined(HAVE_WORKING_VFORK) */
	res = -1;
#endif

	return res;
}

static const char *fix_header(char *outbuf, int maxout, const char *s, char *cmp)
{
	const char *c;

	/* Check for verboser preamble */
	if (*s == 127) {
		s++;
	}

	if (!strncmp(s, cmp, strlen(cmp))) {
		c = s + strlen(cmp);
		term_color(outbuf, cmp, COLOR_GRAY, 0, maxout);
		return c;
	}
	return NULL;
}

static void console_verboser(const char *s)
{
	char tmp[80];
	const char *c = NULL;

	if ((c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_4)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_3)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_2)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_1))) {
		fputs(tmp, stdout);
		fputs(c, stdout);
	} else {
		if (*s == 127) {
			s++;
		}
		fputs(s, stdout);
	}

	fflush(stdout);
	
	/* Wake up a poll()ing console */
	if (css_opt_console && consolethread != CSS_PTHREADT_NULL)
		pthread_kill(consolethread, SIGURG);
}

static int css_all_zeros(char *s)
{
	while (*s) {
		if (*s > 32)
			return 0;
		s++;  
	}
	return 1;
}

static void consolehandler(char *s)
{
	printf("%s", term_end());
	fflush(stdout);

	/* Called when readline data is available */
	if (!css_all_zeros(s))
		css_el_add_history(s);
	/* The real handler for bang */
	if (s[0] == '!') {
		if (s[1])
			css_safe_system(s+1);
		else
			css_safe_system(getenv("SHELL") ? getenv("SHELL") : "/bin/sh");
	} else 
		css_cli_command(STDOUT_FILENO, s);
}

static int remoteconsolehandler(char *s)
{
	int ret = 0;

	/* Called when readline data is available */
	if (!css_all_zeros(s))
		css_el_add_history(s);
	/* The real handler for bang */
	if (s[0] == '!') {
		if (s[1])
			css_safe_system(s+1);
		else
			css_safe_system(getenv("SHELL") ? getenv("SHELL") : "/bin/sh");
		ret = 1;
	}
	if ((strncasecmp(s, "quit", 4) == 0 || strncasecmp(s, "exit", 4) == 0) &&
	    (s[4] == '\0' || isspace(s[4]))) {
		quit_handler(0, 0, 0, 0);
		ret = 1;
	}

	return ret;
}


static int  css_el_read_history(char *filename)
{
	char buf[MAX_HISTORY_COMMAND_LENGTH];
	FILE *f;
	int ret = -1;

	if (el_hist == NULL || el == NULL)
		css_el_initialize();

	if ((f = fopen(filename, "r")) == NULL)
		return ret;

	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if (!strcmp(buf, "_HiStOrY_V2_\n"))
			continue;
		if (css_all_zeros(buf))
			continue;
		if ((ret = css_el_add_history(buf)) == -1)
			break;
	}
	fclose(f);

	return ret;
}

static void css_remotecontrol(char *data)
{
    char buf[80];
    int res;
    char filename[80] = "";
    char *hostname;
    char *cpid;
    char *version;
    int pid;
    char *stringp = NULL;
    char *ebuf;
    int num = 0;

    memset(&sig_flags, 0, sizeof(sig_flags));
    signal(SIGINT, __remote_quit_handler);
    signal(SIGTERM, __remote_quit_handler);
    signal(SIGHUP, __remote_quit_handler);

    if (read(css_consock, buf, sizeof(buf)) < 0) {
        css_log(LOG_ERROR, "read() failed: %s\n", strerror(errno));
        return;
    }
    if (data) {
        char prefix[] = "cli quit after ";
        char *tmp = alloca(strlen(data) + strlen(prefix) + 1);
        sprintf(tmp, "%s%s", prefix, data);
        if (write(css_consock, tmp, strlen(tmp) + 1) < 0) {
            css_log(LOG_ERROR, "write() failed: %s\n", strerror(errno));
            if (sig_flags.need_quit || sig_flags.need_quit_handler) {
                return;
            }
        }
    }
    stringp = buf;
    hostname = strsep(&stringp, "/");
    cpid = strsep(&stringp, "/");
    version = strsep(&stringp, "\n");
    if (!version)
            version = "<Version Unknown>";
    stringp = hostname;
    strsep(&stringp, ".");
    if (cpid)
            pid = atoi(cpid);
    else
            pid = -1;
    if (!data) {
        char tmp[80];
        snprintf(tmp, sizeof(tmp), "core set verbose atleast %d", option_verbose);
        fdsend(css_consock, tmp);
        snprintf(tmp, sizeof(tmp), "core set debug atleast %d", option_debug);
        fdsend(css_consock, tmp);
        if (!css_opt_mute)
            fdsend(css_consock, "logger mute silent");
        else 
            printf("log and verbose output currently muted ('logger mute' to unmute)\n");
    }

    if (css_opt_exec && data) {  /* hack to print output then exit if cssplayer -rx is used */
        struct pollfd fds;
        fds.fd = css_consock;
        fds.events = POLLIN;
        fds.revents = 0;
        while (poll(&fds, 1, 60000) > 0) {
            char buffer[512] = "", *curline = buffer, *nextline;
            int not_written = 1;

            if (sig_flags.need_quit || sig_flags.need_quit_handler) {
                break;
            }

            if (read(css_consock, buffer, sizeof(buffer) - 1) <= 0) {
                break;
            }

            do {
                if ((nextline = strchr(curline, '\n'))) {
                    nextline++;
                } else {
                    nextline = strchr(curline, '\0');
                }

                /* Skip verbose lines */
                if (*curline != 127) {
                    not_written = 0;
                    if (write(STDOUT_FILENO, curline, nextline - curline) < 0) {
                        css_log(LOG_WARNING, "write() failed: %s\n", strerror(errno));
                    }
                }
                curline = nextline;
            } while (!css_strlen_zero(curline));

            /* No non-verbose output in 60 seconds. */
            if (not_written) {
                break;
            }
        }
        return;
    }

    css_verbose("Connected to cssplayer %s currently running on %s (pid = %d)\n", version, hostname, pid);
    remotehostname = hostname;
    if (getenv("HOME")) 
        snprintf(filename, sizeof(filename), "%s/.cssplayer_history", getenv("HOME"));
    if (el_hist == NULL || el == NULL)
        css_el_initialize();

    el_set(el, EL_GETCFN, css_el_read_char);

    if (!css_strlen_zero(filename))
            css_el_read_history(filename);

    for (;;) {
        ebuf = (char *)el_gets(el, &num);

        if (sig_flags.need_quit || sig_flags.need_quit_handler) {
            break;
        }

        if (!ebuf && write(1, "", 1) < 0)
            break;

        if (!css_strlen_zero(ebuf)) {
            if (ebuf[strlen(ebuf)-1] == '\n')
                    ebuf[strlen(ebuf)-1] = '\0';
            if (!remoteconsolehandler(ebuf)) {
                /* Strip preamble from output */
                char *temp;
                for (temp = ebuf; *temp; temp++) {
                    if (*temp == 127) {
                            memmove(temp, temp + 1, strlen(temp));
                            temp--;
                    }
                }
                res = write(css_consock, ebuf, strlen(ebuf) + 1);
                if (res < 1) {
                    css_log(LOG_WARNING, "Unable to write: %s\n", strerror(errno));
                    break;
                }
            }
        }
    }
    printf("\nDisconnected from Cssplayer server\n");
}


/*! \brief Set an X-term or screen title */
static void set_title(char *text)
{
	if (getenv("TERM") && strstr(getenv("TERM"), "xterm"))
		fprintf(stdout, "\033]2;%s\007", text);
}

static void set_icon(char *text)
{
	if (getenv("TERM") && strstr(getenv("TERM"), "xterm"))
		fprintf(stdout, "\033]1;%s\007", text);
}

/*
 *服务器主函数 
 */
int main(int argc, char** argv) 
{   
    int x,c;
    int isroot = 1;
    sigset_t sigs;
    struct rlimit l;
    char * xarg = NULL;
    FILE *f;
    char filename[80] = "";
#define EL_BUF_SIZE 512
    char *buf;
    int num;
    char hostname[MAXHOSTNAMELEN];
    
    if (gethostname(hostname, sizeof(hostname)-1))
        css_copy_string(hostname, "<Unknown>", sizeof(hostname));
    
    css_mainpid = getpid();

    /* Remember original args for restart */
    if (argc > ARRAY_LEN(_argv) - 1) {
        fprintf(stderr, "Truncating argument size to %d\n", (int)ARRAY_LEN(_argv) - 1);
        argc = ARRAY_LEN(_argv) - 1;
    }

    for (x = 0; x < argc; x++)
        _argv[x] = argv[x];
    _argv[x] = NULL;
    
    if (geteuid() != 0)
        isroot = 0;
    
    css_builtins_init();

    if (getenv("HOME")) 
        snprintf(filename, sizeof(filename), "%s/.cssplayerserver_history", getenv("HOME"));

    while ((c = getopt(argc, argv, "BC:cde:FfG:ghIiL:M:mnpqRrs:TtU:VvWXx:")) != -1) {
        switch (c) {
            case 'V':
                printf("css player server version 1.0.0\n");
                break;
            case 'c':
                css_set_flag(&css_options, CSS_OPT_FLAG_NO_FORK | CSS_OPT_FLAG_CONSOLE);
                break;
            case 'r':
                css_set_flag(&css_options, CSS_OPT_FLAG_NO_FORK | CSS_OPT_FLAG_REMOTE);
                break;
            case '?':
                exit(1);
        }
    }
    
    if (css_opt_console || option_verbose || (css_opt_remote && !css_opt_exec)) {
        if (css_register_verbose(console_verboser)) {
            css_log(LOG_WARNING, "Unable to register console verboser?\n");
        }
        WELCOME_MESSAGE;
    }
    
    if (css_opt_console && !option_verbose) 
        css_verbose("[ Booting...\n");

    /* For remote connections, change the name of the remote connection.
     * We do this for the benefit of init scripts (which need to know if/when
     * the main cssplayer process has died yet). */
    if (css_opt_remote) {
        strcpy(argv[0], "rcssplayer");
        for (x = 1; x < argc; x++) {
                argv[x] = argv[0] + 10;
        }
    }

    if (css_opt_console && !option_verbose) {
        css_verbose("[ Reading Master Configuration ]\n");
    }
    
    css_readconfig();

    if (css_opt_dump_core) {
        memset(&l, 0, sizeof(l));
        l.rlim_cur = RLIM_INFINITY;
        l.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &l)) {
            css_log(LOG_WARNING, "Unable to disable core size resource limit: %s\n", strerror(errno));
        }
    }
    
    /*
     *检查系统文件数量限制
     */
    if (getrlimit(RLIMIT_NOFILE, &l)) {
        css_log(LOG_WARNING, "Unable to check file descriptor limit: %s\n", strerror(errno));
    }
        
    /*
     *设置信号量，防止子线程终止不会影响到主线程 .
     */
    sigaction(SIGCHLD, &child_handler, NULL);
    
    //设置core文件产生
#ifdef linux
    if (geteuid() && css_opt_dump_core) {
        if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) < 0) {
            css_log(LOG_WARNING, "Unable to set the process for core dumps after changing to a non-root user. %s\n", strerror(errno));
        }
    }
#endif
    
    //初始化CLI
    css_term_init();
    printf("%s", term_end());
    fflush(stdout);  
    
    if (css_opt_console && !option_verbose) 
        css_verbose("[ Initializing Custom Configuration Options ]\n");
    /* custom config setup */
    register_config_cli();
    //read_config_maps();
    
    if (css_opt_console) {
        if (el_hist == NULL || el == NULL)
            css_el_initialize();

        if (!css_strlen_zero(filename))
            css_el_read_history(filename);
    }
    
    if (css_tryconnect()) {
        /* One is already running */
        if (css_opt_remote) {
            if (css_opt_exec) {
                css_remotecontrol(xarg);
                quit_handler(0, 0, 0, 0);
                exit(0);
            }
            printf("%s", term_quit());
            css_remotecontrol(NULL);
            quit_handler(0, 0, 0, 0);
            exit(0);
        } else {
            css_log(LOG_ERROR, "CSS already running on %s.  Use 'cssplayerserver -r' to connect.\n", "/etc/cssplayerserver.ctl");
            printf("%s", term_quit());
            exit(1);
        }
    } else if (css_opt_remote || css_opt_exec) {
        css_log(LOG_ERROR, "Unable to connect to remote css (does %s exist?)\n", css_config_CSS_SOCKET);
        printf("%s", term_quit());
        exit(1);
    }

    /* Blindly write pid file since we couldn't connect */
    unlink(css_config_CSS_PID);
    f = fopen(css_config_CSS_PID, "w");
    if (f) {
        fprintf(f, "%ld\n", (long)getpid());
        fclose(f);
    } else
        css_log(LOG_WARNING, "Unable to open pid file '%s': %s\n", css_config_CSS_PID, strerror(errno));

    css_makesocket();
    /*
     *屏蔽进程相关信号
     */
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGPIPE);
    sigaddset(&sigs, SIGWINCH);
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);
    sigaction(SIGURG, &urg_handler, NULL);
    signal(SIGINT, __quit_handler);
    signal(SIGTERM, __quit_handler);
    sigaction(SIGHUP, &hup_handler, NULL);
    sigaction(SIGPIPE, &ignore_sig_handler, NULL);

    threadstorage_init();

    cssobj2_init();
    
    //初始化日志模块
    if (init_logger()) {		/* Start logging subsystem */
        printf("%s", term_quit());
        exit(1);
    }

    //加载配置文件，读取本地端口、设备信息和中心服务器地址
    css_log(LOG_NOTICE,"读取服务器设备文件!\n");

    //注册到中心服务器
    css_log(LOG_NOTICE,"设备注册中心!\n");   
   // css_register_init();

    //开启心跳线程
   // css_heartbeat_init();
    css_log(LOG_NOTICE,"初始化心跳模块!\n");

    //初始化GMP信令监听模块 
    css_pthread_create_background(&css_player_background, NULL, css_monitor_udp_init, NULL);
   
    if (pipe(sig_alert_pipe))
        sig_alert_pipe[0] = sig_alert_pipe[1] = -1;
    
    pthread_sigmask(SIG_UNBLOCK, &sigs, NULL);
    
    css_cli_register_multiple(cli_cssplayer, ARRAY_LEN(cli_cssplayer));
    
    if (css_opt_console) {
        /* Console stuff now... */
        /* Register our quit function */
        char title[256];

        css_pthread_create_detached(&mon_sig_flags, NULL, monitor_sig_flags, NULL);

        //set_icon("cssplayerserver");
        snprintf(title, sizeof(title), "Cssplayer Console on '%s' (pid %ld)", hostname, (long)css_mainpid);
        set_title(title);

        el_set(el, EL_GETCFN, css_el_read_char);

        for (;;) {
            if (sig_flags.need_quit || sig_flags.need_quit_handler) {
                    quit_handler(0, 0, 0, 0);
                    break;
            }
            buf = (char *) el_gets(el, &num);

            if (!buf && write(1, "", 1) < 0)
                    goto lostterm;

            if (buf) {
                    if (buf[strlen(buf)-1] == '\n')
                            buf[strlen(buf)-1] = '\0';

                    consolehandler((char *)buf);
            } else if (css_opt_remote && (write(STDOUT_FILENO, "\nUse EXIT or QUIT to exit the CSSPLAYERSERVER console\n",
                       strlen("\nUse EXIT or QUIT to exit the CSSPLAYERSERVER console\n")) < 0)) {
                    /* Whoa, stdout disappeared from under us... Make /dev/null's */
                    int fd;
                    fd = open("/dev/null", O_RDWR);
                    if (fd > -1) {
                            dup2(fd, STDOUT_FILENO);
                            dup2(fd, STDIN_FILENO);
                    } else
                            css_log(LOG_WARNING, "Failed to open /dev/null to recover from dead console. Bad things will happen!\n");
                    break;
            }
        }
    }

    /*等待信号*/
    monitor_sig_flags(NULL);

lostterm:
    return (EXIT_SUCCESS);
}

