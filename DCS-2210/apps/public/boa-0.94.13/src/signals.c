/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@boa.org>
 *  Some changes Copyright (C) 1996-99 Jon Nelson <jnelson@boa.org>
 *  Some changes Copyright (C) 1997 Alain Magloire <alain.magloire@rcsm.ee.mcgill.ca>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* $Id: signals.c,v 1.37.2.2 2002/07/23 16:03:41 jnelson Exp $*/

#include "boa.h"
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>           /* wait */
#endif
#include <signal.h>             /* signal */

#include <utility.h>

sigjmp_buf env;
int handle_sigbus;

void sigsegv(int);
void sigbus(int);
void sigterm(int);
void sighup(int);
void sigint(int);
void sigchld(int);
void sigalrm(int);
void sigio_handler(int);

#ifdef SERVER_SSL
    extern int server_ssl;
#endif

static int server_sock;

/*
 * Name: init_signals
 * Description: Sets up signal handlers for all our friends.
 */

void set_server_socket(int sock)
{
	server_sock = sock;
}

void boa_release(void)
{
    void free_client_socket(void);

    free_client_socket();
#ifdef SERVER_SSL
    if (server_ssl > 0)
        close(server_ssl);
#endif
    if (server_sock)
        close(server_sock);
}

void sigusr1(int dummy)
{
	time(&pending_time);
    av_stream_pending = 1;
}

void sigusr2(int dummy)
{
    av_stream_pending = -1;
}

void signal_handler(int signo)
{
    fprintf(stderr, "Unexpected signal %d\n", signo);
    exit(0);
}

void init_signals(void)
{
    struct sigaction sa;

    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSEGV);
    sigaddset(&sa.sa_mask, SIGBUS);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigaddset(&sa.sa_mask, SIGALRM);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sigaddset(&sa.sa_mask, SIGIO);

    sa.sa_handler = sigsegv;
    sigaction(SIGSEGV, &sa, NULL);

    sa.sa_handler = sigbus;
    sigaction(SIGBUS, &sa, NULL);

    sa.sa_handler = sigterm;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighup;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigalrm;
    sigaction(SIGALRM, &sa, NULL);

    sa.sa_handler = sigusr1;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigusr2;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = sigio_handler;
    sigaction(SIGIO, &sa, NULL);	/* add by Alex, 2009.01.09	*/	

#if 1
    sa.sa_handler = signal_handler;
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGPROF, &sa, NULL);
    sigaction(SIGPWR, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGSTKFLT, &sa, NULL);
    sigaction(SIGSYS, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
#endif

}

void sigsegv(int dummy)
{
    time(&current_time);
    log_error_time();
    boa_release();
    fprintf(stderr, "caught SIGSEGV, dumping core in %s\n", tempdir);
    fclose(stderr);
    chdir(tempdir);
    abort();
}

extern sigjmp_buf env;
extern int handle_sigbus;

void sigbus(int dummy)
{
    if (handle_sigbus) {
        longjmp(env, dummy);
    }
    time(&current_time);
    log_error_time();
    boa_release();
    fprintf(stderr, "caught SIGBUS, dumping core in %s\n", tempdir);
    fclose(stderr);
    chdir(tempdir);
    abort();
}

void sigterm(int dummy)
{
    sigterm_flag = 1;
}

void sigterm_stage1_run(int server_s) /* lame duck mode */
{
    time(&current_time);
    log_error_time();
    fputs("caught SIGTERM, starting shutdown\n", stderr);
    FD_CLR(server_s, &block_read_fdset);
    close(server_s);
    boa_release();
    sigterm_flag = 2;
}

void sigterm_stage2_run() /* lame duck mode */
{
    log_error_time();
    fprintf(stderr,
            "exiting Boa normally (uptime %d seconds)\n",
            (int) (current_time - start_time));
    chdir(tempdir);
    clear_common_env();
    dump_mime();
    dump_passwd();
    dump_alias();
    free_requests();
    exit(0);
}


void sighup(int dummy)
{
    execl("/opt/ipnc/boa", "boa", "-c", "/etc", NULL);
    sighup_flag = 1;
}

void sighup_run(void)
{
    sighup_flag = 0;
    time(&current_time);
    log_error_time();
    fputs("caught SIGHUP, restarting\n", stderr);

    /* Philosophy change for 0.92: don't close and attempt reopen of logfiles,
     * since usual permission structure prevents such reopening.
     */

    FD_ZERO(&block_read_fdset);
    FD_ZERO(&block_write_fdset);
    /* clear_common_env(); NEVER DO THIS */
    dump_mime();
    dump_passwd();
    dump_alias();
    free_requests();

    log_error_time();
    fputs("re-reading configuration files\n", stderr);
    read_config_files();

    log_error_time();
    fputs("successful restart\n", stderr);

}

void sigint(int dummy)
{
    time(&current_time);
    log_error_time();
    boa_release();
    fputs("caught SIGINT: shutting down\n", stderr);
    fclose(stderr);
    chdir(tempdir);
    exit(1);
}

void sigchld(int dummy)
{
    sigchld_flag = 1;
}

void sigchld_run(void)
{
    int status;
    pid_t pid;

    sigchld_flag = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        if (verbose_cgi_logs) {
            time(&current_time);
            log_error_time();
            fprintf(stderr, "reaping child %d: status %d\n", (int) pid, status);
        }
    return;
}

void sigalrm(int dummy)
{
    sigalrm_flag = 1;
}

void sigalrm_run(void)
{
    time(&current_time);
    log_error_time();
    fprintf(stderr, "%ld requests, %ld errors\n",
            status.requests, status.errors);
    show_hash_stats();
    sigalrm_flag = 0;
}


