/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1997 Jon Nelson <jnelson@boa.org>
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

/* $Id: globals.h,v 1.65.2.3 2002/07/24 03:03:59 jnelson Exp $*/

#ifndef _GLOBALS_H
#define _GLOBALS_H

#ifdef DAVINCI_IPCAM

#include <linux/types.h> // __u8, __u16...
#include <netinet/in.h>	 // in_addr_t
#include <time.h> // time_t

#include <sysconf.h>
#include <Appro_interface.h>
#include <sysctrl_struct.h>
#include <userinfo.h>

#define USE_NEW_STREAM_API

#define MAX_SDLIST_LENGTH	(64*1024)

#define MAX_HTML_LENGTH	8192
#define MAX_CMD_LENGTH	2048
#define MAX_CMD_NUM		64
#define MAX_NIPCA_NOTIFY_LEN	512

typedef struct __COMMAND_ARGUMENT {
    char *name;
    char *value;
    int flags;
} COMMAND_ARGUMENT;

typedef struct __HTTP_CONNECTION {
	struct __HTTP_CONNECTION *last;
	struct __HTTP_CONNECTION *next;
	
	//USER_INFO *user;
	char * user_name;
	char * passwd;
	int * auth;
	unsigned int user_idx;
	in_addr_t ipaddr;
	time_t time_last;			/* time of last succ. op. */
	int counter;
	int revoke;
	
} HTTP_CONNECTION;

struct __HTTP_URI;

#define MFLAG_IS_MEMORY    0x0001
#define BUSY_FLAG_AUDIO	0x01
#define BUSY_FLAG_VIDEO	0x02
#define BUSY_FLAG_STREAM	0x04
#define AUDIO_SEND_SIZE      900

#endif

struct mmap_entry {
    dev_t dev;
    ino_t ino;
    char *mmap;
    int use_count;
    size_t len;
};

struct alias {
    char *fakename;             /* URI path to file */
    char *realname;             /* Actual path to file */
    int type;                   /* ALIAS, SCRIPTALIAS, REDIRECT */
    int fake_len;               /* strlen of fakename */
    int real_len;               /* strlen of realname */
    struct alias *next;
};

typedef struct alias alias;

struct request {                /* pending requests */
    int fd;                     /* client's socket fd */
    int status;                 /* see #defines.h */
    time_t time_last;           /* time of last succ. op. */
    char *pathname;             /* pathname of requested file */
    int simple;                 /* simple request? ( METHOD line without HTTP/1.x ?) */
    int keepalive;              /* keepalive status */
    int kacount;                /* keepalive count */

    int data_fd;                /* fd of data */
    unsigned long filesize;     /* filesize */
    unsigned long filepos;      /* position in file */
    char *data_mem;             /* mmapped/malloced char array */
    int method;                 /* M_GET, M_POST, etc. */

    char *logline;              /* line to log file */

    char *header_line;          /* beginning of un or incompletely processed header line */
    char *header_end;           /* last known end of header, or end of processed data */
    int parse_pos;              /* how much have we parsed */
    int client_stream_pos;      /* how much have we read... */

    int buffer_start;           /* where the buffer starts */
    int buffer_end;             /* where the buffer ends */

    char *http_version;         /* HTTP/?.? of req */
    int response_status;        /* R_NOT_FOUND etc. */

    char *if_modified_since;    /* If-Modified-Since */
    time_t last_modified;       /* Last-modified: */

    char local_ip_addr[NI_MAXHOST]; /* for virtualhost */

    /* CGI vars */

    int remote_port;            /* could be used for ident */

    char remote_ip_addr[NI_MAXHOST]; /* after inet_ntoa */

#ifdef DAVINCI_IPCAM
    struct __HTTP_URI *http_uri;
    int http_stream;
//    char user_id[USER_LEN];
    AUTHORITY authority;
    HTTP_CONNECTION *connect;
    in_addr_t remote_ip;
    int mem_flag;
    int busy_flag;
    char *mmap_ptr;
    unsigned long mmap_size;
#ifdef USE_NEW_STREAM_API
    STREAM_DATA video;
    STREAM_DATA audio;
#else
    AV_DATA av_data;
    int serial_book;
    int serial_lock;
//    int latest_frame;
    int audio_lock;
    int audio_book;
    int audio_sync;
#endif
	unsigned int audio_turns;
#ifdef CONFIG_STREAM_TIMEOUT
    time_t last_stream_time;
#endif
    char *cmd_string;
    int cmd_count;
    COMMAND_ARGUMENT cmd_arg[MAX_CMD_NUM];
    char *export_file;
    unsigned long extra_size;
    struct user_info curr_user;
    struct REQUEST_FLAG {
        unsigned int restart : 1;
        unsigned int audio_on : 1;
        unsigned int is_stream : 1;
        unsigned int set_mac : 1;
        unsigned int reboot : 1;
    } req_flag;
#endif
    int is_cgi;                 /* true if CGI/NPH */
    int cgi_status;
    int cgi_env_index;          /* index into array */

    /* Agent and referer for logfiles */
    char *header_user_agent;
    char *header_referer;

    int post_data_fd;           /* fd for post data tmpfile */

    char *path_info;            /* env variable */
    char *path_translated;      /* env variable */
    char *script_name;          /* env variable */
    char *query_string;         /* env variable */
    char *content_type;         /* env variable */
    char *content_length;       /* env variable */

    struct mmap_entry *mmap_entry_var;

    struct request *next;       /* next */
    struct request *prev;       /* previous */

#ifdef DAVINCI_IPCAM
    char *authorization;
#endif
	//NIPCA
	char nipca_notify_buf[MAX_NIPCA_NOTIFY_LEN];
	unsigned int nipca_notify_stream;
	time_t nipca_notify_time;

#ifdef SERVER_SSL
    void* ssl;
#endif /*SERVER_SSL*/

    char sessionid[28];
    /* everything below this line is kept regardless */
    char buffer[BUFFER_SIZE + 1]; /* generic I/O buffer */
    char request_uri[MAX_HEADER_LENGTH + 1]; /* uri */
    char client_stream[CLIENT_STREAM_SIZE]; /* data from client - fit or be hosed */
    char *cgi_env[CGI_ENV_MAX + 4];             /* CGI environment */

#ifdef ACCEPT_ON
    char accept[MAX_ACCEPT_LENGTH]; /* Accept: fields */
#endif
};

typedef struct request request;

struct status {
    long requests;
    long errors;
};


#ifdef DAVINCI_IPCAM

#define MAX_URI_HASH_SIZE		64

#define URI_FLAG_NEED_PARSE	(1 << 0)
#define URI_FLAG_VIRTUAL_PAGE  	(1 << 1)
#define URI_FLAG_CHECK_LOGIN  	(1 << 2)
#define URI_FLAG_VOLATILE_FILE	(1 << 3)
#define URI_FLAG_DIRECT_READ	(1 << 4)
#define URI_FLAG_DIRECT_WRITE	(1 << 5)
#define URI_FLAG_RTSP_O_HTTP	(1 << 6)
typedef struct __HTTP_URI
{
	char *name;
	int (*handler) (struct request *);
	AUTHORITY authority;
	int uri_flag;
  
	struct __HTTP_URI *next;
  
} HTTP_URI;

#define API_NONE		(0)
#define API_RESTART		(1<<0)
#define API_SET_MAC		(1<<1)
#define API_VIDEO		(1<<2)
typedef struct __HTTP_OPTION {
	char			*name;
	void			(*handler)(request *, COMMAND_ARGUMENT *);
	AUTHORITY	authority;
	int			now;
	int			flag;
	struct __HTTP_OPTION *next;
} HTTP_OPTION;

#define MAX_CMD_HASH_SIZE		64
typedef struct __CMD_HASH_TABLE {
	HTTP_OPTION *entry;
} CMD_HASH_TABLE;

struct JPEG_HEADER {
	__u8	jpeg_soi[2];
	__u8	jpeg_app0[2];
	__u16	length;		//  4
	__u16	csum;		//  6
	__u32	size;			//  8
	__u16	year;		// 12
	__u8	day;			// 14
	__u8	month;		// 15
	__u8	minute;		// 16
	__u8	hour;		// 17
	__u8	minisecond;	// 18
	__u8	second;		// 19
	__u32	serial;		// 20
	__u32	ID;			// 24
	__u16	device_code;	// 28
	__u8	quality;		// 30
	__u8	rate;		// 31
	__u8	resolution;	// 32
	__u8	scale_factor;	// 33
	__u8	flags;		// 34
	__u8	alarm;		// 35
	__u16	width;		// 36
	__u16	height;		// 38
	__u8	resv[24];		// 40
	
} __attribute__((packed));

#endif


extern struct status status;

extern char *optarg;            /* For getopt */
extern FILE *yyin;              /* yacc input */

extern request *request_ready;  /* first in ready list */
extern request *request_block;  /* first in blocked list */
extern request *request_free;   /* first in free list */

extern fd_set block_read_fdset; /* fds blocked on read */
extern fd_set block_write_fdset; /* fds blocked on write */

/* global server variables */

extern char *access_log_name;
extern char *error_log_name;
extern char *cgi_log_name;
extern int cgi_log_fd;
extern int use_localtime;

extern int server_port;
extern uid_t server_uid;
extern gid_t server_gid;
extern char *server_admin;
extern char *server_root;
extern char *server_name;
extern char *server_ip;
extern int max_fd;
extern int devnullfd;

extern char *document_root;
extern char *user_dir;
extern char *directory_index;
extern char *default_type;
extern char *dirmaker;
extern char *mime_types;
extern char *cachedir;

extern char *tempdir;

extern char *cgi_path;
extern int single_post_limit;

extern int ka_timeout;
extern int ka_max;

extern int sighup_flag;
extern int sigchld_flag;
extern int sigalrm_flag;
extern int sigterm_flag;
extern time_t start_time;

extern int pending_requests;
extern long int max_connections;

extern int verbose_cgi_logs;

extern int backlog;
extern time_t current_time;

extern int virtualhost;

extern int total_connections;

extern sigjmp_buf env;
extern int handle_sigbus;
extern int av_stream_pending;
extern time_t pending_time;
#ifdef DAVINCI_IPCAM
extern SysInfo *sys_info;
extern char http_stream[MAX_PROFILE_NUM][MAX_HTTPSTREAMNAME_LEN+2];
extern char rtsp_stream[MAX_PROFILE_NUM][MAX_HTTPSTREAMNAME_LEN+2];
#endif

#endif
