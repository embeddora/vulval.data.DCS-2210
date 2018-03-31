/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Authorization "module" (c) 1998,99 Martin Hinner <martin@tdp.cz>
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
#include <stdio.h>
#include <arpa/inet.h> // in_addr_t
#include <time.h> // time_t

//#define LOCAL_DEBUG
#include <debug.h>
#include <userinfo.h>
#include <sysconf.h>
#include <sysctrl.h>
#include "boa.h"
#include <config_save.h>
#include <sys_log.h>
#include <syslogno.h>
#include <syslogapi.h>

#ifdef DAVINCI_IPCAM

HTTP_CONNECTION *connect_guest = NULL;
HTTP_CONNECTION *connect_ready = NULL;
HTTP_CONNECTION *connect_free = NULL;
int connection_counter = 0;
USER_INFO user_guest = {
	"guest",
	"",
	AUTHORITY_VIEWER
};
	
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
HTTP_CONNECTION *alloc_connection(void)
{
	HTTP_CONNECTION *tmp;
	
	if (connect_free == NULL) {
		if ((connection_counter >= MAX_CONNECTION) || 
						(tmp = (HTTP_CONNECTION *) malloc(sizeof(HTTP_CONNECTION))) == NULL)
			return NULL;
		connection_counter++;
		dbg("New connection, connection counter = %d\n", connection_counter);
	}
	else {
		tmp = connect_free;
		connect_free = tmp->next;
	}

	tmp->revoke = 0;
	tmp->counter = 0;

	tmp->last = NULL;
	tmp->next = connect_ready;
	if (connect_ready)
		connect_ready->last = tmp;
	connect_ready = tmp;

	return tmp;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void free_connection(HTTP_CONNECTION *connect)
{
	if (connect_guest == connect)
		connect_guest = NULL;

	if (connect_ready == connect)
		connect_ready = connect->next;
	
	if (connect->last)
		connect->last->next = connect->next;
	if (connect->next)
		connect->next->last = connect->last;

	connect->next = connect_free;
	connect_free = connect;

	connection_counter--;
	dbg("Free connection, connection counter = %d\n", connection_counter);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int revoke_connection(char *name)
{
	HTTP_CONNECTION *connect;

	for (connect=connect_ready; connect; connect=connect->next) {
		if (strcasecmp(name, connect->user_name) == 0) {
			connect->revoke = 1;
			return 0;
		}
	}
	return -1;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
 * Name: base64decode
 *
 * Description: Decodes BASE-64 encoded string
 */
int base64decode( void *dst, char *src, int maxlen )
{
	int bitval,bits;
	int val;
	int len,x,y;

	len = strlen( src );
	bitval = 0;
	bits = 0;
	y = 0;
	for ( x = 0; x < len; x++ )
	{
		if ( ( src[x] >= 'A' ) && ( src[x] <= 'Z' ) )
			val = src[x] - 'A';
		else if ( ( src[x] >= 'a' ) && ( src[x] <= 'z' ) )
			val = src[x] - 'a' + 26;
		else if ( ( src[x] >= '0' ) && ( src[x] <= '9' ) )
			val = src[x] - '0' + 52;
		else if ( src[x] == '+' )
			val = 62;
		else if ( src[x] == '-' )
			val = 63;
		else if ( src[x] == '=' )
			val = 64;
		else
			val = -1;
		if ( val >= 0 )
		{
			if ( val & 0xC0 )
			{
				bitval = 0;
				bits = 0;
			}
			else
			{
				bitval <<= 6;
				bitval += ( val & 0x3F );
				bits += 6;
			}
			while ( bits >= 8 )
			{
				if ( y < maxlen )
					( (char *) dst )[y++] = ( bitval >> ( bits - 8 ) ) & 0xFF;
				bits -= 8;
				bitval &= ( 1 << bits ) - 1;
			}
		}
	}
	if ( y < maxlen )
		( (char *) dst )[y++] = 0;
	return y;
}

static char base64chars[64] = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

/*
 * Name: base64encode()
 *
 * Description: Encodes a buffer using BASE64.
 */
void base64encode(unsigned char *from, char *to, int len)
{
  while (len) {
    unsigned long k;
    int c;

    c = (len < 3) ? len : 3;
    k = 0;
    len -= c;
    while (c--)
      k = (k << 8) | *from++;
    *to++ = base64chars[ (k >> 18) & 0x3f ];
    *to++ = base64chars[ (k >> 12) & 0x3f ];
    *to++ = base64chars[ (k >> 6) & 0x3f ];
    *to++ = base64chars[ k & 0x3f ];
  }
  *to++ = 0;
}

int check_user(request *req, char *name, char *passwd)
{
	HTTP_CONNECTION *connect;
	int i;
#ifdef MSGLOGNEW
	char logstr[MSG_MAX_LEN];
#endif
	if (connect_guest == NULL && sys_info->user_auth_mode == 0) {
		dbg("guest alloc_connection\n");
		if ((connect_guest = alloc_connection()) == NULL)
			return ERROR_SERVICE_UNAVAILABLE;

		//connect_guest->user = &user_guest;
		connect_guest->user_name = user_guest.name;
		connect_guest->passwd = user_guest.passwd;
		connect_guest->auth = &user_guest.authority;
	}

	for (connect=connect_ready; connect; connect=connect->next) {
		if ( (connect->revoke == 0) && (connect==connect_guest || connect->ipaddr==req->remote_ip) ) {
			if ( (strcasecmp(name, connect->user_name) == 0) && (strcmp(passwd, connect->passwd) == 0) ) {
				connect->counter++;
				connect->time_last = req->time_last;
				req->connect = connect;
				req->authority = *connect->auth;
				// FIX_ME
				strcpy(req->curr_user.id, connect->user_name);
				req->curr_user.ip = connect->ipaddr;
//				dbg("connect->counter = %d, authority=%d\n", connect->counter, req->authority);
			
				return 0;
			}
		}
	}

	for (i=0; i<ACOUNT_NUM; i++) {

		if ((strcasecmp(name, conf_string_read(ACOUNTS0_NAME+i*3)) == 0) &&
						(strcmp(passwd, conf_string_read(ACOUNTS0_PASSWD+i*3)) == 0)) {

			dbg("%s alloc_connection\n", conf_string_read(ACOUNTS0_NAME+i*3));
			if ((connect = alloc_connection()) == NULL)
				return ERROR_SERVICE_UNAVAILABLE;

			//connect->user = &sys_info->config.acounts[i];
			connect->user_name = conf_string_read(ACOUNTS0_NAME+i*3);
			connect->passwd = conf_string_read(ACOUNTS0_PASSWD+i*3);
			connect->auth = (int *)&sys_info->ipcam[ACOUNTS0_AUTHORITY+i*3].config.u8;

			connect->user_idx = i;
			connect->ipaddr = req->remote_ip;
			connect->counter++;
			connect->time_last = req->time_last;
			req->connect = connect;
			req->authority = *connect->auth;
			// FIX_ME
			strcpy(req->curr_user.id, connect->user_name);
			req->curr_user.ip = connect->ipaddr;

			SYSLOG tLog;

			tLog.ip = req->curr_user.ip;
			tLog.opcode = SYSLOG_SET_LOGIN_OK;
			strcpy(tLog.name, req->curr_user.id);
			SetSysLog(&tLog);
#ifdef MSGLOGNEW
			snprintf( logstr, MSG_USE_LEN, "%s LOGIN OK FROM %s",req->curr_user.id, ipstr(req->curr_user.ip));
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
			//ControlSystemData(SFIELD_DO_LOGIN, connect->user, sizeof(USER_INFO), &req->curr_user);
			
			return 0;
		}
	}
	
	return ERROR_UNAUTHORIZED;
}

int auth_authorize(request *req)
{
	char *pwd;
	char buffer[256];

	if (req->http_uri->uri_flag & URI_FLAG_CHECK_LOGIN) {
		int idx;
		if (uri_decoding(req, req->query_string) < 0)
			return ERROR_BAD_REQUEST;

		for (idx=0; idx<req->cmd_count; idx++) {
			if (strcmp(req->cmd_arg[idx].name, "login") == 0) {
				char *user_name = req->cmd_arg[idx].value, *user_passwd;

				if ( (user_passwd = strchr(user_name, ':')) == NULL )
					return ERROR_UNAUTHORIZED;
				*user_passwd++ = 0;

				if ((strlen(user_name) > MAX_USER_SIZE) || (strlen(user_passwd) > MAX_PASS_SIZE))
					return ERROR_UNAUTHORIZED;

				return check_user(req, user_name, user_passwd);
				
			}
		}
	}

	if (req->authorization) {
		if (strncasecmp(req->authorization,"Basic ",6))
			return ERROR_UNAUTHORIZED;

		base64decode(buffer, req->authorization+6, 0x100);
		
		if ( (pwd = strchr(buffer, ':')) == 0 )
			return ERROR_UNAUTHORIZED;
		
		*pwd++ = 0;
		if ((strlen(buffer) > MAX_USER_SIZE) || (strlen(pwd) > MAX_PASS_SIZE))
			return ERROR_UNAUTHORIZED;

		return check_user(req, buffer, pwd);
	}
	else
		return check_user(req, "guest", "");
}

#endif

