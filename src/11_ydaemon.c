/*
 * Work inspired in daemon.c from NXWEB project.
 * https://bitbucket.org/yarosla/nxweb/overview
 * Copyright (c) 2011-2012 Yaroslav Stavnichiy <yarosla@gmail.com>
 */
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include "11_ydaemon.h"

/******************************************************
 *      Data
 ******************************************************/
PRIVATE volatile int relaunch_times = 0;
PRIVATE volatile int debug = 0;
PRIVATE volatile int exit_code;
PRIVATE volatile int signal_code;
PRIVATE volatile int watcher_pid = 0;

/***************************************************************************
 *  funcion like daemon() syscall
 ***************************************************************************/
PRIVATE void continue_as_daemon(const char *work_dir, const char *process_name)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        print_error(PEF_EXIT, "ERROR YUNETA", "fork() FAILED, errno %d %s", errno, strerror(errno));
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        _exit(EXIT_SUCCESS);
    }

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        print_error(PEF_EXIT, "ERROR YUNETA", "setsid() FAILED, errno %d %s", errno, strerror(errno));
    }

    /* Clear umask to enable explicit file modes. */
    umask(0);

    /* Change the current working directory */
    if (work_dir && chdir(work_dir) < 0) {
        print_error(PEF_EXIT, "ERROR YUNETA", "chdir() FAILED, errno %d %s", errno, strerror(errno));
    }

    /* Close out the standard file descriptors */
    // WARNING Removing this comments, --stop doesn't kill the daemon! why?
//     close(STDIN_FILENO);
//     close(STDOUT_FILENO);
//     close(STDERR_FILENO);
}

/***************************************************************************
 *
 ***************************************************************************/
static int relauncher(
    void (*process) (const char *process_name, const char *work_dir, const char *domain_dir),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*catch_signals)(void))
{
    if(debug) {
        print_info("INFO YUNETA", "Relauncher %d times, process %s, pid %d",
            relaunch_times,
            process_name,
            getpid()
        );
    }

    pid_t pid = fork();
    if(debug) {
        print_info("INFO YUNETA", "fork() return pid %d, process %s, pid %d",
            pid,
            process_name,
            getpid()
        );
    }

    if (pid < 0) {
        _exit(EXIT_FAILURE);
    } else if (pid > 0) {
        /*------------------------*
         *  we are the parent
         *------------------------*/
        catch_signals();
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            print_error(PEF_EXIT, "ERROR YUNETA", "waitpid() failed, errno %d %s", errno, strerror(errno));
        }
        exit_code = 0;
        signal_code = 0;

        if (WIFEXITED(status)) {
            exit_code = (int)(char)(WEXITSTATUS(status));
            if(debug) {
                print_info("INFO YUNETA", "Process child exiting with code %d, process %s, pid %d",
                    exit_code,
                    process_name,
                    getpid()
                );
            }
            if(exit_code < 0) {
                // relaunch the child
                return exit_code;
            }
            return 1;

        } else if (WIFSIGNALED(status)) {
            signal_code = (int)(char)(WTERMSIG(status));
            if(debug) {
                print_info("INFO YUNETA", "Process child signalized with signal %d, process %s, pid %d",
                    signal_code,
                    process_name,
                    getpid()
                );
            }
            if(signal_code == SIGKILL) {
                return 1; // Exit
            }
            return -1; // relaunch

        } else {
            print_error(0, "ERROR YUNETA", "Caso implementado, process %s, pid %d",
                process_name,
                getpid()
            );
        }
        return -1; // relaunch

    } else {
        /*------------------------*
         *  we are the child
         *  (return 0)
         *------------------------*/
        catch_signals();
        log_debug_printf(0, "\n"); // Blank line
        char temp[120];
        snprintf(temp, sizeof(temp), "\n=====> Starting yuno '%s', times: %d, pid: %d\n",
            process_name,
            relaunch_times,
            (int)getpid());
        log_debug_printf("", temp);
        if(relaunch_times > 0) {
            log_error(0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                "msg",              "%s", "Daemon relaunched",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                "signal_code",      "%d", signal_code,
                "exit_code",        "%d", exit_code,
                NULL
            );
        } else {
            log_debug(0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_DAEMON,
                "msg",              "%s", "Daemon started",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                NULL
            );
        }

        process(process_name, work_dir, domain_dir);
        if(debug) {
            log_debug(0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_DAEMON,
                "msg",              "%s", "*process* returned",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                NULL
            );
        }
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int daemon_run(
    void (*process)(const char *process_name, const char *work_dir, const char *domain_dir),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*catch_signals)(void))
{
    int ret;

    watcher_pid = getpid();

    continue_as_daemon(work_dir, process_name);
    while((ret=relauncher(process, process_name, work_dir, domain_dir, catch_signals))<0) {
        // sleep 2 sec and launch again while relauncher return negative
        relaunch_times++;
        sleep(2);
    }
    if(ret==1) { // the watcher returns 1
        if(debug) {
            print_info("INFO YUNETA", "Watcher exiting, process %s, pid %d",
                process_name,
                getpid()
            );
        }
        // parent doesn't return
        _exit(0);
    }
    if(debug) {
        print_info("INFO YUNETA", "daemon_run returning, process %s, pid %d",
            process_name,
            getpid()
        );
    }
    return 0;
}

/***************************************************************************
 *  Kill daemon
 ***************************************************************************/
PRIVATE void kill_proc(void *self, const char *name, pid_t pid)
{
    if(debug) {
        log_debug(0,
            "gobj",             "%s", __FILE__,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_DAEMON,
            "msg",              "%s", "kill proc",
            "process",          "%s", name,
            "pid",              "%d", (int)getpid(),
            "relaunch_times",   "%d", relaunch_times,
            NULL
        );
    }
    if(pid == getpid() || !pid) {
        if(debug) {
            log_debug(0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_DAEMON,
                "msg",              "%s", "Soy el Matador",
                "process",          "%s", name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                NULL
            );
        }
        return;
    }
    if(kill(pid, SIGKILL)<0) {
        int last_errno = errno;
        log_error(LOG_OPT_EXIT_NEGATIVE,
            "gobj",             "%s", __FILE__,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM_ERROR,
            "msg",              "%s", "kill() FAILED",
            "process",          "%s", name,
            "pid",              "%d", (int)getpid(),
            "relaunch_times",   "%d", relaunch_times,
            "error",            "%d", last_errno,
            "strerror",         "%s", strerror(last_errno),
            NULL
        );
    }
}

PUBLIC void daemon_shutdown(const char *process_name)
{
    search_process(process_name, kill_proc, 0);
}

/***************************************************************************
 *  Search process by his name and exec
 *  Return number of processes found.
 ***************************************************************************/
#if defined(__linux__)
PRIVATE int linux_search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self)
{
    int found = 0;
    pid_t pid;
    glob_t pglob;
    char *procname, *readbuf;
    int buflen = strlen(process_name) + 2;
    unsigned i;

    /* Get a list of all comm files. man 5 proc */
    if (glob("/proc/*/comm", 0, NULL, &pglob) != 0) {
        return 0;
    }

    /* The comm files include trailing newlines, so... */
    procname = malloc(buflen);
    if(!procname) {
        return 0;
    }
    strcpy(procname, process_name);
    procname[buflen - 2] = '\n';
    procname[buflen - 1] = 0;

    /* readbuff will hold the contents of the comm files. */
    readbuf = malloc(buflen);
    if(!readbuf) {
        free(procname);
        return 0;
    }

    for (i = 0; i < pglob.gl_pathc; ++i) {
        FILE *comm;
        char *ret;

        /* Read the contents of the file. */
        if ((comm = fopen(pglob.gl_pathv[i], "r")) == NULL)
            continue;
        ret = fgets(readbuf, buflen, comm);
        fclose(comm);
        if (ret == NULL)
            continue;

        /*
         *  If comm matches our process name, extract the process ID from the
         *  path, convert it to a pid_t, and call callback function.
        */
        int n = strlen(procname);
        if(n > 15)
            n = 15;
        if (strncmp(readbuf, procname, n) == 0) {
            pid = (pid_t)atoi(pglob.gl_pathv[i] + strlen("/proc/"));
            if(cb)
                cb(self, procname, pid);
            found ++;
        }
    }

    /* Clean up. */
    free(procname);
    free(readbuf);
    globfree(&pglob);

    return found;
}
#endif

/***************************************************************************
 *  Search process by his name and exec callback if not null.
 *  Return number of processes found.
 ***************************************************************************/
PUBLIC int search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self)
{
    return linux_search_process(process_name, cb, self);
}

/***************************************************************************
 *  Return number relaunch_times
 ***************************************************************************/
PUBLIC int get_relaunch_times(void)
{
    return relaunch_times;
}

/***************************************************************************
 *  Set debug mode
 ***************************************************************************/
PUBLIC int daemon_set_debug_mode(BOOL set)
{
    debug = set?1:0;
    return 0;
}

/***************************************************************************
 *  Get debug mode
 ***************************************************************************/
PUBLIC BOOL daemon_get_debug_mode(void)
{
    return debug;
}

/***************************************************************************
 *  Get debug mode
 ***************************************************************************/
PUBLIC int get_watcher_pid()
{
    return watcher_pid;
}
