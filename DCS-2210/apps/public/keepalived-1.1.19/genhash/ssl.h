/*
 * Soft:        Perform a GET query to a remote HTTP/HTTPS server.
 *              Set a timer to compute global remote server response
 *              time.
 *
 * Part:        ssl.c include file.
 *
 * Version:     $Id: ssl.h,v 1.1 2009/11/11 09:02:45 mikewang Exp $
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

#ifndef _SSL_H
#define _SSL_H

#include <openssl/ssl.h>

/* Prototypes */
extern void init_ssl(void);
extern int ssl_connect(thread * thread_obj);
extern int ssl_printerr(int err);
extern int ssl_send_request(SSL * ssl, char *str_request, int request_len);
extern int ssl_read_thread(thread * thread_obj);

#endif
