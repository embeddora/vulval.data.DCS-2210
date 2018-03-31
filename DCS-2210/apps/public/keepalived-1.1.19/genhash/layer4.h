/*
 * Soft:        Perform a GET query to a remote HTTP/HTTPS server.
 *              Set a timer to compute global remote server response
 *              time.
 *
 * Part:        layer4.c include file.
 *
 * Version:     $Id: layer4.h,v 1.1 2009/11/11 09:02:45 mikewang Exp $
 *
 * Authors:     Alexandre Cassen, <acassen@linux-vs.org>
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2001-2009 Alexandre Cassen, <acassen@freebox.fr>
 */

#ifndef _LAYER4_H
#define _LAYER4_H

/* system includes */
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

/* local includes */
#include "scheduler.h"

enum connect_result {
	connect_error,
	connect_in_progress,
	connect_timeout,
	connect_success
};

/* Prototypes defs */
extern enum connect_result
 tcp_connect(int fd, uint32_t, uint16_t);

extern enum connect_result
 tcp_socket_state(int, thread *, uint32_t, uint16_t,
		  int (*func) (struct _thread *));

extern void
 tcp_connection_state(int, enum connect_result
		      , thread *, int (*func) (struct _thread *)
		      , long);

extern int
 tcp_connect_thread(thread *);

#endif
