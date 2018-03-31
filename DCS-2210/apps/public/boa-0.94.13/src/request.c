/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@boa.org>
 *  Some changes Copyright (C) 1996-2002 Jon Nelson <jnelson@boa.org>
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

/* $Id: request.c,v 1.112.2.3 2002/07/24 03:03:59 jnelson Exp $*/

#include <arpa/inet.h> // inet_ntoa()
#include <stddef.h> /* for offsetof */

//#define LOCAL_DEBUG
#include <storage.h>
#include <debug.h>
#include <sysctrl.h>
#include <sysinfo.h>
#include <utility.h>
#include <gio_util.h>
#include <pathname.h>
#include <bios_data.h>
#include <net_config.h>
#include <parser_name.h>
#include <config_range.h>
#include <system_default.h>	//(sdlist) by jiahung, 20090522

#include "boa.h"
#include "acs.h"

#include "appro_api.h"
#include "file_list.h"

#ifdef SERVER_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <bios_data.h>
#include <gio_util.h>
#include <ApproDrvMsg.h>
#include "appro_api.h"
#include "file_list.h"
#include <parser_name.h>
#include <libvisca.h>		//added by ryan 2010.09.29
#include <syslogapi.h>		//added by ryan 2010.07.12
#include <sys_log.h>		//added by ryan 2010.07.12

#if SUPPORT_DCPOWER
#include <dc_power.h>
#endif
#ifdef SERVER_SSL
extern int server_ssl;			/*the socket to listen for ssl requests*/
extern SSL_CTX *ctx;			/*The global connection context*/
extern int do_ssl;				/*do ssl sockets??*/
extern int do_sock;
#endif /*SERVER_SSL*/
int viscapreset_add(VISCAPRESET viscapreset);
int viscapreset_del(VISCAPRESET viscapreset);
int viscapreset_goto(VISCAPRESET viscapreset, char* name, request * req);
int viscaicr_auto(int value);
int viscaicr_day(int value);
int viscaicr_night(int value);
int viscaicr_schedule(int value, char* sch_time);

/*
int net_get_hwaddr(char *ifname, unsigned char *mac);
in_addr_t net_get_gateway(void);
in_addr_t net_get_ifaddr(char *ifname);
in_addr_t net_get_netmask(char *ifname);
in_addr_t net_get_dns(void);
*/
char *req_bufptr(request * req);

#define CHUNKED_MAXLEN	1024*5
// disable by Alex, 2009.04.13
//#define alarm_in_num		1
//#define alarm_out_num		1

int total_connections;
struct status status;

static int sockbufsize = SOCKETBUF_SIZE;

/* function prototypes located in this file only */
static void free_request(request ** list_head_addr, request * req);
void disable_sequence_autopan(int streamid);

static int rtsp_over_http( request *req );

#ifdef DAVINCI_IPCAM

#ifdef CONFIG_STREAM_LIMIT
static int stream_count;
int check_stream_count(request * req)
{
	if (stream_count >= MAX_STREAM_LIMIT) {
		send_r_service_unavailable(req);
		return 0;
	}
	req->req_flag.is_stream = 1;
	stream_count++;
#ifdef BOA_OSD_DEBUG
	{
		char buffer[50];
		sprintf(buffer, "stream count=%d ", stream_count);
		osd_print(5, 3, buffer);
	}
#endif
	return 1;
}

#endif

#define HTTP_CONNECTION_TIMEOUT		(5*60)	// by seconds

typedef struct __URI_HASH_TABLE {
	HTTP_URI *entry;
	
} URI_HASH_TABLE;

URI_HASH_TABLE *uri_hash;
extern HTTP_CONNECTION *connect_ready, *connect_guest;

int get_jpeg_image(request * req, int index)
{
	req->http_stream = URI_STREAM_JPEG + index;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_mjpeg_data(&req->video, index,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}
	
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = sizeof(struct JPEG_HEADER);
	send_request_ok_jpeg(req);
	add_jpeg_header(req);
	return 1;
}

int uri_jpeg(request * req)
{
	unsigned int i, profile_id=1;
	//int jpeg_num = 0;
	//int jpeg_idx[MAX_PROFILE_NUM];

	uri_decoding(req, req->query_string);
	for (i=0; i<req->cmd_count; i++) {
		if (strcmp(req->cmd_arg[i].name, "nowprofileid") == 0) {
			profile_id = atoi(req->cmd_arg[i].value) - 1;
#if 0
#if defined SENSOR_USE_IMX035	//mikewang , for 1280x720 croped 1 fps jpeg
			if( profile_id == 4 )
				return get_jpeg_image(req, 4);
#endif
		for (i=0; i<MAX_PROFILE_NUM; i++){
			dbg("sys_info->profile_config.profile_codec_fmt[%d] = %d", i, sys_info->profile_config.profile_codec_fmt[i]);
			if(sys_info->profile_config.profile_codec_fmt[i] == SEND_FMT_JPEG){
					jpeg_idx[i] = jpeg_num++;
				dbg("jpeg_idx[%d] = %d", i, jpeg_idx[i]);
			}
		}

		#ifdef SUPPORT_PROFILE_NUMBER
			if(sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value > MAX_PROFILE_NUM)
				break;
		#else
			if (profile_id >= MAX_PROFILE_NUM)
				break;
		#endif

			dbg("jpeg_idx[%d]=%d", profile_id, jpeg_idx[profile_id]);
			return get_jpeg_image(req, jpeg_idx[profile_id]);
#endif
		}
	}

	return get_jpeg_image(req, sys_info->profile_config.profile_jpeg_idx[profile_id]);
	//return get_jpeg_image(req, 0);
}

int get_mjpeg_stream(request *req, int index)
{
	req->http_stream = URI_STREAM_MJPEG + index;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
#ifdef CONFIG_STREAM_TIMEOUT
	req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
	if (!check_stream_count(req))
		return 0;
#endif

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_mjpeg_data(&req->video, index,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = sizeof(struct JPEG_HEADER);
	send_request_ok_mjpeg(req);
	add_jpeg_header(req);
	return 1;
}

int get_mpeg4_stream(request *req, int index)
{
	AV_DATA vol_data;

	req->http_stream = URI_STREAM_MPEG4 + index;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
#ifdef CONFIG_STREAM_TIMEOUT
	req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
	if (!check_stream_count(req))
		return 0;
#endif
	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if(GetAVData(index|AV_FMT_MPEG4|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_MP4_VOL\n");
		return 0;
	}
	
	if (get_mpeg4_data(&req->video, index,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = vol_data.size;

	send_request_ok_mpeg4(req);
	req_copy(req, vol_data.ptr, vol_data.size);
//	GetAVData(AV_OP_UNLOCK_MP4_VOL, -1, NULL);
	return 1;
}

int get_h264_stream(request *req, int index)
{
	//AV_DATA vol_data;

	req->http_stream = URI_STREAM_H264 + index;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
#ifdef CONFIG_STREAM_TIMEOUT
	req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
	if (!check_stream_count(req))
		return 0;
#endif
	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
/*	if(GetAVData(index|AV_FMT_H264|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_H264_VOL\n");
		return 0;
	}
*/
	if (get_h264_data(&req->video, index,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}
	
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
//	req->extra_size = vol_data.size;
	
	send_request_ok_H264(req);
//	req_copy(req, vol_data.ptr, vol_data.size);
//	GetAVData(AV_OP_UNLOCK_MP4_VOL, -1, NULL);
	return 1;
}

int get_stream_avc( request *req )
{
	AV_DATA vol_data;

	req->http_stream = URI_STREAM_AVC;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
	if ( Sharedmemory_init() == -1 )
	{
		dbg( "AVC Sharedmemory_init failed" );
		return 0;
	}
	else
		dbg( "AVC Sharedmemory_init complete\n" );
	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if ( GetAVData( AV_OP_LOCK_AVC_VOL, -1, &vol_data ) != RET_SUCCESS )
	{
		send_r_error( req );
		dbg( "Error on AV_OP_LOCK_AVC_VOL\n" );
		return 0;
	}
	else 
		dbg( "AV_OP_LOCK_AVC_VOL complete" );
	if ( GetAVData( AV_OP_LOCK_AVC_IFRAME, -1, &req->video.av_data ) != RET_SUCCESS )
	{
		send_r_error( req );
		dbg( "Error on AV_OP_LOCK_AVC_IFRAME\n" );
		return 0;
	}
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = vol_data.size;
	req->video.serial_lock = req->video.av_data.serial;
//	fprintf( stderr, "lock serial %d\n",req->serial_lock );
	req->video.serial_book = req->video.serial_lock + 1;
	send_request_ok_H264( req );
	req_copy( req, vol_data.ptr, vol_data.size );
	GetAVData( AV_OP_UNLOCK_AVC_VOL, -1, NULL );
	return 1;
}

int get_http_mjpeg(request *req, int index)
{
	req->http_stream = URI_HTTP_MJPEG + index;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
#ifdef CONFIG_STREAM_TIMEOUT
	req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
	if (!check_stream_count(req))
		return 0;
#endif

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_mjpeg_data(&req->video, index,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	send_request_ok_mjpeg(req);
	return 1;
}

int uri_http_stream1(request * req)
{
	return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[0]);
}

int uri_http_stream2(request * req)
{
#ifdef SUPPORT_PROFILE_NUMBER

	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 2)
		return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[1]);
	else
		return 0;
#else
	if(MAX_WEB_PROFILE_NUM >= 2)
		return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[1]);
	else
		return 0;
#endif
}

int uri_http_stream3(request * req)
{
#ifdef SUPPORT_PROFILE_NUMBER

	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 3)
		return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[2]);
	else
		return 0;
#else
	if(MAX_WEB_PROFILE_NUM >= 3)
		return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[2]);	
	else
		return 0;
#endif
}

#if MAX_PROFILE_NUM >= 4
int uri_http_stream4( request *req )
{
	return get_http_mjpeg(req, sys_info->profile_config.profile_jpeg_idx[3]);
}
#endif

int uri_rtsp_stream1( request *req )
{
	return rtsp_over_http( req );
}

int uri_rtsp_stream2( request *req )
{
#ifdef SUPPORT_PROFILE_NUMBER
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 2)
		return rtsp_over_http( req );
	else
		return 0;
#else
	if(MAX_WEB_PROFILE_NUM >= 2)
		return rtsp_over_http( req );
	else
		return 0;
#endif

}

int uri_rtsp_stream3( request *req )
{
#ifdef SUPPORT_PROFILE_NUMBER
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 3)
		return rtsp_over_http( req );
	else
		return 0;
#else
	if(MAX_WEB_PROFILE_NUM >= 3)
		return rtsp_over_http( req );
	else
		return 0;
#endif

}

int uri_rtsp_stream4( request *req )
{
#ifdef SUPPORT_PROFILE_NUMBER
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 4)
		return rtsp_over_http( req );
	else
		return 0;
#else
	if(MAX_WEB_PROFILE_NUM >= 4)
		return rtsp_over_http( req );
	else
		return 0;
#endif
}

/* multi-profile */
int get_av_stream(request * req , unsigned int profileID)
{
	//ProfileID range : 0 ~ (MAX_PROFILE_NUM-1)
	
#if SUPPORT_MG1264
	if (profileID == 3)
		return get_stream_avc(req);
#endif
	if (profileID > (MAX_PROFILE_NUM-1))
		return -1;//profileID = 0;
	
#ifdef SUPPORT_PROFILE_NUMBER
	if(profileID > (sys_info->ipcam[MAX_PROFILE_NUM].config.value-1))
		return -1;//profileID = 0;
#endif

	if ( sys_info->profile_config.profile_codec_fmt[profileID] == SEND_FMT_JPEG )
		return get_mjpeg_stream(req, sys_info->profile_config.profile_codec_idx[profileID]-1);
	else if ( sys_info->profile_config.profile_codec_fmt[profileID] == SEND_FMT_MPEG4 )
		return get_mpeg4_stream(req, sys_info->profile_config.profile_codec_idx[profileID]-1);
	else
		 return get_h264_stream(req, sys_info->profile_config.profile_codec_idx[profileID]-1);	
}

int uri_av_stream(request * req)
{
	int i,profile_id = 0;

	uri_decoding(req, req->query_string);
	for (i = 0; i < req->cmd_count; i++) {
		if(strcmp(req->cmd_arg[i].name, "nowprofileid") == 0) {
			profile_id = atoi(req->cmd_arg[i].value);
		}
		else if(strcmp(req->cmd_arg[i].name, "audiostream") == 0) {
			if (req->cmd_arg[i].value[0] == '1')
				req->req_flag.audio_on = 1;
		}
	}
	return get_av_stream(req , profile_id-1);
}
int uri_getstreaminfo(request * req)
{
	//ProfileID range : 0 ~ (MAX_PROFILE_NUM-1)
	char buf[256];
	int i,j,profile_id = 0;
	req->is_cgi = CGI;
	SQUASH_KA(req);
	uri_decoding(req, req->query_string);
	for(i = req->cmd_count-1 ; i >= 0 ;i--){
		for( j = req->cmd_count-1 ; j >= 0 ; j--){
			if(strcmp(req->cmd_arg[j].name, "nowprofileid") == 0){
	  		profile_id = atoi(req->cmd_arg[j].value);
			profile_id--;
			}else if(strcmp(req->cmd_arg[j].name, "getformat") == 0){
#ifdef SUPPORT_PROFILE_NUMBER
				if(profile_id > (sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value-1)){
					sprintf(buf, "NG " "%s\r\n",req->cmd_arg[j].name);
					continue;
				}
				
#endif
				//if( strcmp( sys_info->config.profile_config[profile_id].codecname , JPEG_PARSER_NAME ) == 0 )
				if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*profile_id) , JPEG_PARSER_NAME ) == 0 )
					sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,0);
				//else if( strcmp( sys_info->config.profile_config[profile_id].codecname , MPEG4_PARSER_NAME ) == 0 )
				else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*profile_id) , MPEG4_PARSER_NAME ) == 0 )
					sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,1);
				//else if( strcmp( sys_info->config.profile_config[profile_id].codecname , H264_PARSER_NAME ) == 0 )
				else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*profile_id) , H264_PARSER_NAME ) == 0 )
					sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,2);
				else
					sprintf(buf, "NG " "%s\r\n",req->cmd_arg[j].name);
			}else if(strcmp(req->cmd_arg[j].name, "streamvalid") == 0){
				//sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name, sys_info->config.lan_config.supportstream[profile_id] );	//profile always enable

#ifdef SUPPORT_PROFILE_NUMBER
				if(profile_id > (sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value-1)){
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
				}else
					sprintf(buf, "OK " "%s=1\r\n",req->cmd_arg[j].name);
#else
				if(profile_id == 0){
					#if (MAX_PROFILE_NUM >= 1)
					sprintf(buf, "OK " "%s=1\r\n",req->cmd_arg[j].name);
					#else
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
					#endif
				}else if(profile_id == 1){
					#if (MAX_PROFILE_NUM >= 2)
					sprintf(buf, "OK " "%s=1\r\n",req->cmd_arg[j].name);
					#else
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
					#endif
				}else if(profile_id == 2){
					#if (MAX_PROFILE_NUM >= 3)
					sprintf(buf, "OK " "%s=1\r\n",req->cmd_arg[j].name);
					#else
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
					#endif
				}else if(profile_id == 3){
					#if (MAX_PROFILE_NUM >= 4)
					sprintf(buf, "OK " "%s=1\r\n",req->cmd_arg[j].name);
					#else
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
					#endif
				}else
					sprintf(buf, "OK " "%s=0\r\n",req->cmd_arg[j].name);
#endif
				//sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name, sys_info->ipcam[SUPPORTSTREAM0+profile_id].config.value);	//profile always enable
			}else if(strcmp(req->cmd_arg[j].name, "getxsize") == 0){
				//sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,sys_info->config.lan_config.profilesize[profile_id].xsize);
				sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,sys_info->ipcam[PROFILESIZE0_XSIZE+profile_id*2].config.value);
			}else if(strcmp(req->cmd_arg[j].name, "getysize") == 0){
				//sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,sys_info->config.lan_config.profilesize[profile_id].ysize);
				sprintf(buf, "OK " "%s=%d\r\n",req->cmd_arg[j].name,sys_info->ipcam[PROFILESIZE0_YSIZE+profile_id*2].config.value);
			}
		}
	}
	
	send_request_ok_api(req);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	req->status = DONE;

  return 1;
}

int uri_vb_htm(request * req)
{
	req->is_cgi = CGI;
        if (!req->cmd_count && (uri_decoding(req, req->query_string) < 0)) {
            send_r_bad_request(req);
            return 0;
        }
        SQUASH_KA(req);
        http_run_command(req, req->cmd_arg, req->cmd_count);
        req->status = DONE;
        return 1;
}

int uri_aevt_htm(request * req)
{
        if (uri_decoding(req, req->query_string) >= 0) {
	        http_run_command(req, req->cmd_arg, req->cmd_count);
              reset_output_buffer(req);
        }
        return -1;
}

int uri_status_log_htm(request * req)
{
        if (uri_decoding(req, req->query_string) >= 0) {
	        http_run_command(req, req->cmd_arg, req->cmd_count);
              //dbg("req->cmd_arg[0].name=%s\n", req->cmd_arg[0].name); 
              reset_output_buffer(req);
        }
        return -1;
}


int uri_ini_htm(request * req)
{
#define MAX_INT_SIZE	(32*1024)
        char *addr = (char *)malloc(MAX_INT_SIZE);
        if (addr == NULL) {
            send_r_error(req);
            return 0;
        }
        SQUASH_KA(req);
        req->mem_flag |= MFLAG_IS_MEMORY;
        req->data_mem = addr;
        req->filesize = html_ini_cmd(req, addr, MAX_INT_SIZE);
        send_request_ok_text(req);     /* All's well */
        req->status = WRITE;
        return 1;
}

int uri_check_sum(request * req)
{
	unsigned int sum;
	char buffer[32];
	
	sum = get_firmware_checksum();
	
	SQUASH_KA(req);
	send_request_ok_api(req);     /* All's well */
	sprintf(buffer, "checksum(dec) = %05u<br>", sum);
	req_write(req, buffer);
	sprintf(buffer, "checksum(hex) = 0x%04X<br>", sum);
	req_write(req, buffer);
	req->status = DONE;
	return 1;
}

int uri_export_conf(request * req)
{
	req->is_cgi = 0;
	SQUASH_KA(req);
	req->export_file = strdup("sysenv.dat");
	if (!req->export_file) {
		send_r_error(req);
		WARN("unable to strdup buffer onto req->export_file");
		return 0;
	}

	if (req->pathname)
		free(req->pathname);
	req->pathname = strdup(SYS_ENV_PATH);
	if (!req->pathname) {
		send_r_error(req);
		WARN("unable to strdup buffer onto req->pathname");
		return 0;
	}
	strncpy(req->request_uri, "/sysenv.dat", MAX_HEADER_LENGTH);
#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	char vistor[sizeof( loginip ) + MAX_USERNAME_LEN + 6];
	strcpy(loginip, ipstr(req->curr_user.ip));
	sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
	snprintf(logstr, MSG_USE_LEN, "%s SAVE CONFIGURATION sysenv.dat", vistor);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#endif
	return -1;
}

int uri_keyword_htm(request * req)
{
#define MAX_KEYWORD_SIZE	(32*1024)
        char *addr = (char *)malloc(MAX_KEYWORD_SIZE);
        if (addr == NULL) {
            send_r_error(req);
            return 0;
        }
        SQUASH_KA(req);
        req->mem_flag |= MFLAG_IS_MEMORY;
        req->data_mem = addr;
        req->filesize = html_keyword_cmd(req, addr, MAX_KEYWORD_SIZE);
        send_request_ok_text(req);     /* All's well */
        req->status = WRITE;
        return 1;
}

int uri_cmdlist_htm(request * req)
{
#define MAX_CMDLIST_SIZE	(32*1024)
        char *addr = (char *)malloc(MAX_CMDLIST_SIZE);
        if (addr == NULL) {
            send_r_error(req);
            return 0;
        }
        SQUASH_KA(req);
        req->mem_flag |= MFLAG_IS_MEMORY;
        req->data_mem = addr;
        req->filesize = html_cmdlist_cmd(req, addr, MAX_CMDLIST_SIZE);
        send_request_ok_text(req);     /* All's well */
        req->status = WRITE;
        return 1;
}

// Disable by Alex. 2010.04.12, NO MORE USED
/*
int uri_sdget_htm(request * req)
{
        char *addr, sdpath[128] = {0};

        if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
        }

        addr = (char *)malloc(MAX_SDLIST_LENGTH);
        if (addr == NULL) {
            send_r_error(req);
            return 0;
        }
        req->filesize = http_sdget_cmd(req, req->cmd_arg, addr, MAX_SDLIST_LENGTH, sdpath);
        if (req->filesize > 0) {
            SQUASH_KA(req);
            req->mem_flag |= MFLAG_IS_MEMORY;
            req->data_mem = addr;
            send_request_ok_sdget(req);
            req->status = WRITE;
            return 1;
        }
        free(addr);
        if (req->pathname)
            free(req->pathname);
        strcat(sdpath, "/");
        strcat(sdpath, req->cmd_arg[0].value);
        req->pathname = strdup(sdpath);
        if (!req->pathname) {
            send_r_error(req);
            WARN("unable to strdup buffer onto req->pathname");
            return 0;
        }
        strncpy(req->request_uri, req->cmd_arg[0].value, MAX_HEADER_LENGTH);
        return -1;

}

int uri_sddel_htm(request * req)
{
        SQUASH_KA(req);
        if (uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
        }
        http_sddel_cmd(req, req->cmd_arg);
        req->status = DONE;
        return 1;
}
*/

//uri_sdlist_htm add by jiahung, 20090522
FILELIST *pFilelist = NULL;
FILELIST *pFloderlist = NULL;
int filenum = 0, foldernum = 0 ;
char indir[256] = "";
int pageindex = 1;
int totalpage = 0 ;
int sdfpp;
/*****/
int totalfolderpage = 0;
int folderpage = 0;
int foldernum_lastpage = 0;

/*****/
int totalfilepage = 0;
int numoffirstpage = 0 ;
int filepage = 0;
int numoflastpage = 0 ;
int filestart = 0;
/*****/
void cal_pagestaus(void)
{
	switch(sys_info->ipcam[SDCARD_SDFPP].config.u8){
	//switch(sys_info->config.sdcard.sdfpp){
			case 0:	sdfpp = 10; break;
			case 1:	sdfpp = 20;	break;
			case 2:	sdfpp = 50;	break;
			default: sdfpp = 10;	break;
	}
	/*****cal total*****/
	if((filenum + foldernum )!= 0){			
       		totalpage = (filenum + foldernum ) / sdfpp ;
       		numoflastpage = filenum % sdfpp ;
       		if(((filenum + foldernum ) % sdfpp) != 0){
       			totalpage += 1;
       		}
       	}else
       		totalpage = 0;
	/*****cal folder*****/
	if(foldernum != 0){
		folderpage = foldernum / sdfpp ;
		foldernum_lastpage = foldernum % sdfpp ;
	}else{
		folderpage = 0;
		foldernum_lastpage = 0;
		totalfolderpage = 0;
	}
	if(foldernum_lastpage != 0 )
		totalfolderpage = folderpage + 1;
	else
		totalfolderpage = folderpage;
	/*****cal filenum*****/	
	if(filenum != 0){
		if(foldernum != 0){
			numoffirstpage = sdfpp - foldernum_lastpage;
			dbg("numoffirstpage=%d\n", numoffirstpage);
			filepage = (filenum - numoffirstpage) / sdfpp ;
			numoflastpage = (filenum - numoffirstpage) % sdfpp;
			if(numoffirstpage > filenum){
				dbg("frist filepage no full\n");
				numoffirstpage = filenum;
				numoflastpage = 0;
			}
		}else{
			if(filenum > sdfpp)
				numoffirstpage = sdfpp;
			else
				numoffirstpage = filenum;
			filepage = (filenum - numoffirstpage) / sdfpp;
			numoflastpage = (filenum - numoffirstpage) % sdfpp;
			if(numoffirstpage > filenum){
				numoffirstpage = filenum;
				numoflastpage = 0;
			}
		}
	}else{
		numoffirstpage = 0;
		filepage = 0;
		numoflastpage = 0;
		totalfilepage = 0;
	}
	/*****cal totalfilepage*****/
	int ext = 0;
	if(numoffirstpage != 0)
		ext++;
	if(numoflastpage != 0 )
		ext++;
	totalfilepage = ext + filepage;
	/*****cal filestart*****/
	if(foldernum_lastpage == 0 && numoffirstpage != 0)
		filestart = folderpage + 1;
	else
		filestart = totalfolderpage;
	if(foldernum == 0)
		filestart = 1;
	/*****comp pageindex*****/
	if(pageindex > totalpage)
		pageindex = 1;
}


int getsdlist(char* path)
{	
		//dbg("path%s\n", path);	
		DIR* dir;
	    char workdir[256] = "";
	    struct dirent *entry;
	    struct stat finfo;	    
		char cwd[256] = "";	 	
	 	strcpy(workdir,path);

		if(pFilelist != NULL || pFloderlist != NULL){ 
	     	 filenum = 0;
	     	 foldernum = 0;
	     	 free(pFilelist);
	     	 free(pFloderlist);
	     } 
	     pFilelist = (FILELIST *)malloc(MAX_SDLIST_LENGTH);
	     if (pFilelist == NULL) {
	         DBG("malloc pFilelist error\n");
	         return -1;
	      }        
	     pFloderlist = (FILELIST *)malloc(MAX_SDLIST_LENGTH);
	     if (pFloderlist == NULL) {
			DBG("malloc pFloderlist error\n");
			return -1;
	     }

	     FILELIST *ptr = pFilelist;
	     FILELIST *ptrdir = pFloderlist;
       
	    if(CheckDlinkFileExist(workdir) < 0)
	       	DBGERR("files is not exist\n");
  		else{
  			//dbg("files is exist\n");       				
  			dir = opendir(workdir);      				
			chdir(workdir);

			//dbg("cwd = %s\n", GetWorkDir(cwd));
  			while( (entry = readdir(dir)) != NULL){
  				//dbg("entry->d_name = %s\n", entry->d_name);
  				if(stat(entry->d_name, &finfo) == 0){
  					if(S_ISDIR(finfo.st_mode) == 0 ){
  						if( !(( strchr(entry->d_name, '.') - entry->d_name + 1 ) == 1) ){	
  						//if(fileformat_select(entry)){
  							strncpy(ptr->name, entry->d_name, sizeof(ptr->name));
  							ptr->filetype = S_ISDIR(finfo.st_mode);
  							ptr->eventtype = 0;
  							ptr->size = finfo.st_size;
  							ptr->numoffiles = 0;
  							ptr->property = 0;
  							ptr->authority = 0;      				
  							ptr->time = finfo.st_mtime;
  							ptr->reserve = 0;
  							dbg("pFilelist->name = %s - %d - %d - %ld\n", ptr->name, ptr->filetype, ptr->size, ptr->time);    						

  							ptr++;	
  							filenum++;  
  						//} 
  						}
  					}
  					else{
  						//if(!( (strcmp(entry->d_name, ".")==0 ) || (strcmp(entry->d_name, "..")==0) ) && 
 						//		( (strcmp(entry->d_name, "Video")==0 ) || (strcmp(entry->d_name, "Picture")==0) )){
 						if(!( (strcmp(entry->d_name, ".")==0 ) || (strcmp(entry->d_name, "..")==0) ) &&
 							( !(( strchr(entry->d_name, '.') - entry->d_name + 1 ) == 1) ) ){	
  							strncpy(ptrdir->name, entry->d_name, sizeof(ptr->name));     							
  							ptrdir->filetype = S_ISDIR(finfo.st_mode);
  							ptrdir->eventtype = 0;
  							ptrdir->size = finfo.st_size;
  							ptrdir->numoffiles = 0;
  							ptrdir->property = 0;
  							ptrdir->authority = 0;				
  							ptrdir->time = finfo.st_mtime;
  							ptrdir->reserve = 0;
  							dbg("pFloderlist->name = %s - %d - %d - %ld\n", ptrdir->name, ptrdir->filetype, ptrdir->size, ptrdir->time);    						

							ptrdir++;
  							foldernum++;
  						}
  					}
 						 			
  				}else{
  					perror("stat");
  					DBGERR("stat(%s) = error\n", entry->d_name);
  				}

  			}
  			ptr = pFilelist;
			ptrdir = pFloderlist;
  			closedir(dir);
  			dbg("filenum = %d\n", filenum); 
  			dbg("foldernum = %d\n", foldernum);    				
  		} 
  		char olddir[256] = "";
  		strcat(olddir, GetWorkDir(cwd));
  		//dbg("olddir=%s\n", olddir);
  		int j;
		//dbg("##########################\n");
		for( j = 1 ; j <= foldernum ; j++){
  			if(foldernum== 0)	break;
  			int cnt = 0, ret;
  			struct dirent *entrycnt;
  			DIR*	dircnt;
  			bzero(workdir, sizeof(workdir));
  		 
  			strcat(workdir, olddir); 
  			if(strcmp(workdir, olddir)==0)	strcat(workdir, "/"); 
  			strcat(workdir, ptrdir->name);
  		
  			//dbg("workdir = %s<%d>\n", workdir, __LINE__); 
  			ret = chdir(workdir);
  			if(ret != 0)  perror("chdir");
  		
  			dircnt = opendir(workdir); 

  			//dbg("workdir = %s<%d>\n", workdir, __LINE__); 
  			ret = chdir(workdir);
  			if(ret != 0)  perror("chdir");	 	
	    	//dbg("cwd = %s<%d>\n", GetWorkDir(cwd), __LINE__);
  			while( (entrycnt = readdir(dircnt)) != NULL){
  				if(!( (strcmp(entrycnt->d_name, ".") == 0) || (strcmp(entrycnt->d_name, "..") == 0) ) ){			
  					//dbg("entrycnt->d_name=%s\n", entrycnt->d_name);
  					//if(fileformat_select(entrycnt))
  						cnt++;
  				}
  			}
  			//dbg("cnt = %d\n", cnt);
  			ptrdir->numoffiles = cnt;
  			ptrdir++;
  			bzero(workdir, sizeof(workdir));
  			//dbg("cwd = %s<%d>\n", GetWorkDir(cwd), __LINE__);
  			//dbg("##########################\n");
  			closedir(dircnt);

  		}
  		
		cal_pagestaus();

		ptrdir = pFloderlist;
  		chdir("/opt/ipnc");
        return 1;
}

int sdfile_download(request * req, char* path)
{
		char* filename; 
		char workdir[256] = ""; 
		getsdlist("/mnt/mmc/");
		dbg("path=%s\n", path);
		filename = path;
		filename = strrchr(filename, '/');
		if(filename == NULL){
			DBGERR("get download filename error\n");
			return 0;
		}
		dbg("filename = %s\n", filename + 1);
		req->export_file = strdup(filename + 1);

		if (!req->export_file) {
			send_r_error(req);
			WARN("unable to strdup buffer onto req->export_file");
			return 0;
		}
		if (req->pathname)
      		free(req->pathname); 

		strcat(workdir, "/mnt/mmc");
		strcat(workdir, path);             		      		
  		dbg("workdir=%s\n", workdir);
  		req->pathname = strdup(workdir);
 		if (!req->pathname) {
      		send_r_error(req);
      		WARN("unable to strdup buffer onto req->pathname");
      		return 0;
  		}

  		strncpy(req->request_uri, workdir, MAX_HEADER_LENGTH);
  		dbg("req->request_uri=%s\n", req->request_uri);
 		return -1;
 		
}
int uri_sdcard(request * req)
{
	SQUASH_KA(req);
	char card[256];
	long long usedspace;
	long long freespace;
	send_request_ok_api(req);
	usedspace = GetDiskusedSpace(SD_MOUNT_PATH);
	freespace = GetDiskfreeSpace(SD_MOUNT_PATH);
	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
    
	//fprintf(stderr, "sys_info->status.mmc=%d\n", sys_info->status.mmc);
	
	if( req->cmd_count != 0 ) {
		if(strcmp(req->cmd_arg[0].name, "type") == 0){	
			bzero(card, sizeof(card));
			strcat(card, SD_MOUNT_PATH);
			strcat(card, "/");
			strcat(card, req->cmd_arg[0].value);	
			getsdlist(card);
			
			if(sys_info->status.mmc & SD_LOCK)
		    	req->buffer_end += sprintf((void *)req_bufptr(req), "status=write_protected\r\n");
		    else if(!(sys_info->status.mmc & SD_MOUNTED))
		    	req->buffer_end += sprintf((void *)req_bufptr(req), "status=invaild\r\n");
		    else 
		   		req->buffer_end += sprintf((void *)req_bufptr(req), "status=ready\r\n");
			
			req->buffer_end += sprintf((void *)req_bufptr(req), "num=%d\r\n", filenum);
			req->buffer_end += sprintf((void *)req_bufptr(req), "filenum of folder=%d\r\n", foldernum);
		}
	}else{
		
	if((sys_info->status.mmc & SD_MOUNTED) && !(sys_info->status.mmc & SD_LOCK))
		req->buffer_end += sprintf((void *)req_bufptr(req), "status=available\r\n");
	else
		req->buffer_end += sprintf((void *)req_bufptr(req), "status=unavailable\r\n");	
	req->buffer_end += sprintf((void *)req_bufptr(req), "total=%lldKB\r\n", usedspace + freespace);
	req->buffer_end += sprintf((void *)req_bufptr(req), "used=%lldKB\r\n", usedspace);
	req->buffer_end += sprintf((void *)req_bufptr(req), "free=%lldKB\r\n", freespace);
    //req->buffer_end += sprintf((void *)req_bufptr(req), "picture=-----\r\n");
    //req->buffer_end += sprintf((void *)req_bufptr(req), "video=-----\r\n");
	}
	
  	req->status = WRITE;
    return 1;
}
int uri_sdcard_list(request * req)
{
	int i = 0;
	SQUASH_KA(req);
	send_request_ok_api(req);

	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
    }
	if(req->cmd_count == 0 || req->cmd_count != 4){
		dbg("req->cmd_count error\n");
		 send_r_bad_request(req);
        return 0;
	}
    fprintf(stderr, "sys_info->status.mmc=%d\n", sys_info->status.mmc);
    if(sys_info->status.mmc & SD_LOCK)
    	req->buffer_end += sprintf((void *)req_bufptr(req), "status=write_protected\r\n");
    else if(!(sys_info->status.mmc & SD_MOUNTED))
    	req->buffer_end += sprintf((void *)req_bufptr(req), "status=invaild\r\n");
    else 
   		req->buffer_end += sprintf((void *)req_bufptr(req), "status=ready\r\n");
	if(strcmp(req->cmd_arg[0].name, "type") == 0)
    	req->buffer_end += sprintf((void *)req_bufptr(req), "type=%s\r\n", req->cmd_arg[0].value);
    if(strcmp(req->cmd_arg[1].name, "date") == 0)
    	req->buffer_end += sprintf((void *)req_bufptr(req), "date=%s\r\n", req->cmd_arg[1].value);
    if(strcmp(req->cmd_arg[2].name, "page") == 0){
    	//req->buffer_end += sprintf((void *)req_bufptr(req), "page=%s\r\n",  req->cmd_arg[2].value);
    	pageindex = atoi(req->cmd_arg[2].value);
    }
    if(strcmp(req->cmd_arg[3].name, "pagesize") == 0){
    		    switch(atoi(req->cmd_arg[3].value)){
					default:
					case 10:	sdfpp = 10; 
								sys_info->ipcam[SDCARD_SDFPP].config.u8 = 0;
								//sys_info->config.sdcard.sdfpp = 0;
								break;
					case 20:	sdfpp = 20;	
								sys_info->ipcam[SDCARD_SDFPP].config.u8 = 1;
								//sys_info->config.sdcard.sdfpp = 1;
								break;
					case 50:	sdfpp = 50;	
								sys_info->ipcam[SDCARD_SDFPP].config.u8 = 2;
								//sys_info->config.sdcard.sdfpp = 2;
								break;
					
				}
    	
    }

	int displaynum = 0;
	char card[256];
	memset(card, 0, sizeof(card));
	strcat(card, SD_MOUNT_PATH);
	strcat(card, "/");
	strcat(card, req->cmd_arg[0].value);
	strcat(card, "/");
	strcat(card, req->cmd_arg[1].value);
	if(CheckDlinkFileExist(card) < 0){
		 send_r_bad_request(req);
        return 0;
     }
	getsdlist(card);
	FILELIST *ptr = pFilelist;
	dbg("card=%s\n", card);
	
	//FILELIST *ptrdir = pFloderlist;
	if((filenum + foldernum )!= 0){			
       		totalpage = (filenum + foldernum ) / sdfpp ;
       		numoflastpage = (filenum + foldernum) % sdfpp;	
       		if(((filenum + foldernum ) % sdfpp) != 0){
       			totalpage += 1;
       		}
     }else
       		totalpage = 0;
	dbg("totalpage = %d\n", totalpage);
	dbg("totalfilepage = %d\n", totalfilepage);
	dbg("numoffirstpage = %d\n", numoffirstpage);
	dbg("filepage = %d\n", filepage);
	dbg("numoflastpage = %d\n", numoflastpage);
	dbg("filestart = %d\n", filestart);
	dbg("displaynum = %d\n", displaynum);
	if(pageindex ==  filestart){
		dbg("frist\n");	
		if(sdfpp > numoffirstpage )
			displaynum = numoffirstpage;
		else
			displaynum = sdfpp;
	}else if(pageindex ==  totalpage){
		dbg("last\n");
		//displaynum = numoflastpage;
		if(numoflastpage == 0){
			displaynum = sdfpp;
			ptr =  ptr + numoffirstpage + (filepage-1) * sdfpp;
		}else{
			displaynum = numoflastpage;
			ptr =  ptr + numoffirstpage + filepage * sdfpp;
		}
	}else if(pageindex < filestart || ((numoffirstpage == sdfpp) && (filestart == pageindex))){
		dbg("no page\n");
		displaynum = 0;
	}else{
		dbg("other\n");
		ptr =  ptr + numoffirstpage + (pageindex - filestart - 1) * sdfpp;
		displaynum = sdfpp;
	}

	
/*	if(filenum != 0){			
		totalpage = (foldernum + filenum) / sdfpp ;
		numoflastpage = filenum % sdfpp ;
	}else
		totalpage = 1;

	if(numoflastpage != 0){
		totalpage += 1;
	}else if(numoflastpage == 0 ){ 
		numoflastpage = sdfpp;
	}
	if(pageindex > totalpage)
		pageindex = totalpage;
	int i=0;
		ptr += ( (pageindex-1) * sdfpp);

	if(pageindex == totalpage)
		displaynum = numoflastpage;
	else
		displaynum = sdfpp;

	if(filenum == 0)
		displaynum = 0;
*/		
	
	
	
	req->buffer_end += sprintf((void *)req_bufptr(req), "page=%d\r\n",  pageindex);
	req->buffer_end += sprintf((void *)req_bufptr(req), "pagesize=%d\r\n",  sdfpp);
	req->buffer_end += sprintf((void *)req_bufptr(req), "total_file=%d\r\n", filenum);
    req->buffer_end += sprintf((void *)req_bufptr(req), "total_page=%d\r\n", totalpage);
    req->buffer_end += sprintf((void *)req_bufptr(req), "num=%d\r\n", displaynum);
	
	dbg("displaynum=%d\n", displaynum);
	for(i=0;i <= (displaynum - 1);i++){
		req->buffer_end += sprintf((void *)req_bufptr(req), "file_%s=<%d>\r\n", ptr->name, ptr->size);
		//dbg("file=<%s><%d>\n", ptr->name, ptr->size);
		ptr++;
	}
	req->status = WRITE; 
    return 1;
}
int uri_sdcard_format(request * req)
{
	//int value = 1;
	char workdir[256] = "";
	SQUASH_KA(req);
	send_request_ok_api(req);

	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
    }
	if(req->cmd_count == 0 )
		return 1;
	getsdlist("/mnt/mmc/");
  	if(strcmp(req->cmd_arg[0].name, "format") == 0){	
		if(strcmp(req->cmd_arg[0].value, "go") == 0){
			/*
			if (ControlSystemData(SFIELD_SD_FORMAT, &value, sizeof(value), NULL) < 0){
				req->buffer_end += sprintf((void *)req_bufptr(req), "unformatted\r\n");
				return 0;
			}
			system("/opt/ipnc/cgi-bin/format.cgi sd\n");
			*/
			pid_t pid;
			if( !(sys_info->status.mmc & SD_INSERTED) )
				req->buffer_end += sprintf((void *)req_bufptr(req), "format fail\r\n");
			
			if( (pid = fork()) == 0 ) {
				execl("/opt/ipnc/cgi-bin/format.cgi", "format.cgi", "-d", "sd", NULL);
				DBGERR("exec format.cgi failed\n");
				_exit(0);
			}
			else if(pid < 0) {
				DBGERR("fork format.cgi failed\n");
				return -1;
			}

			req->buffer_end += sprintf((void *)req_bufptr(req), "ready\r\n");
		}
	}else if(strcmp(req->cmd_arg[0].name, "type") == 0){
		if(strcmp(req->cmd_arg[1].name, "file") == 0){
			//dbg("req->cmd_arg[1].value = %s\n", req->cmd_arg[1].value);
			strcat(workdir, "/mnt/mmc/");
			strcat(workdir, req->cmd_arg[0].value);
			strcat(workdir, "/");
			strcat(workdir, req->cmd_arg[1].value);
			//dbg("workdir = %s\n", workdir);
			if(DeleteDlinkFile(workdir) == 0){
				req->buffer_end += sprintf((void *)req_bufptr(req), "File is successfully deleted\r\n");
			}else{
				req->buffer_end += sprintf((void *)req_bufptr(req), "File is not exists\r\n");
			}
		}
	}	
	else
		req->buffer_end += sprintf((void *)req_bufptr(req), "format fail\r\n");
	
	req->status = WRITE; 
    return 1;
}
int uri_sdcard_delete(request * req)
{
	//int value = 1;
	char workdir[256] = "";
	SQUASH_KA(req);
	send_request_ok_api(req);

	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
    }
	if(req->cmd_count == 0 )
		return 1;
	getsdlist("/mnt/mmc/");

	if(strcmp(req->cmd_arg[0].name, "type") == 0){
		if(strcmp(req->cmd_arg[1].name, "file") == 0){
			//dbg("req->cmd_arg[1].value = %s\n", req->cmd_arg[1].value);
			strcat(workdir, "/mnt/mmc/");
			strcat(workdir, req->cmd_arg[0].value);
			strcat(workdir, "/");
			strcat(workdir, req->cmd_arg[1].value);
			//dbg("workdir = %s\n", workdir);
			if(DeleteDlinkFile(workdir) == 0){
				req->buffer_end += sprintf((void *)req_bufptr(req), "File is successfully deleted\r\n");
			}else{
				req->buffer_end += sprintf((void *)req_bufptr(req), "File is not exists\r\n");
			}
		}
	}	
	
	
	req->status = WRITE; 
    return 1;
}

int uri_sdcard_download(request * req)
{
	char workdir[256] = "/";

	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
    }
	if(req->cmd_count == 0 )
		return 1;
			
	if(strcmp(req->cmd_arg[0].name , "type") == 0){	
		//dbg("req->cmd_arg[0].value=%s\n", req->cmd_arg[0].value);
		strcat(workdir, req->cmd_arg[0].value);
		if(strcmp(req->cmd_arg[1].name , "file") == 0){	
			//dbg("req->cmd_arg[1].value=%s\n", req->cmd_arg[1].value);
			strcat(workdir, "/");
			strcat(workdir, req->cmd_arg[1].value);
			//dbg("%s\n", workdir);
			if(sdfile_download(req, workdir) < 0)
				return -1;
		}		
	}
	return 1;
}
int uri_sddownload(request * req)
{
	char workdir[256] = "";
	if (req->query_string && uri_decoding(req, req->query_string) < 0) {
            send_r_bad_request(req);
            return 0;
    }
    if(req->cmd_count == 0 )
		return 1;
	
	if(strcmp(req->cmd_arg[0].name , "file") == 0){	
		strcat(workdir, req->cmd_arg[0].value);
		dbg("%s\n", workdir);
		if(sdfile_download(req, workdir) < 0)
			return -1;
	}
	return 1;	
}

#ifdef CONFIG_BRAND_DLINK //tmp jiahung NIPCA
int uri_info(request * req)
{
	__u8 mac[MAC_LENGTH];
	struct in_addr ip;
	struct in_addr netmask;
	struct in_addr gateway;
	char buf[512];
	size_t size = 0;
	//req->is_cgi = CGI;

	size += sprintf(buf+size, "model=%s\r\n", sys_info->oem_data.model_name);
	size += sprintf(buf+size, "product=%s\r\n", conf_string_read(TITLE));

	size += sprintf(buf+size, "version=%d.%02d\r\n", APP_VERSION_MAJOR, APP_VERSION_MINOR);
	size += sprintf(buf+size, "build=0\r\n");

	size += sprintf(buf+size, "nipca=1.9\r\n");
	size += sprintf(buf+size, "name=%s\r\n", conf_string_read(TITLE));

	size += sprintf(buf+size, "location=%s\r\n", "");
	net_get_hwaddr(sys_info->ifname, mac);
	size += sprintf(buf+size, "macaddr=%02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5] );	
	ip.s_addr = net_get_ifaddr(sys_info->ifname);
	size += sprintf(buf+size, "ipaddr=%s\r\n", (char*)inet_ntoa(ip));
	netmask.s_addr = net_get_netmask(sys_info->ifname);
	size += sprintf(buf+size, "netmask=%s\r\n", (char*)inet_ntoa(netmask));
	gateway.s_addr = net_get_gateway(sys_info->ifname);
	size += sprintf(buf+size, "gateway=%s\r\n", (char*)inet_ntoa(gateway));

#ifdef SUPPORT_WIRELESS
	size += sprintf(buf+size, "wireless=yes\r\n"); 
#else
	size += sprintf(buf+size, "wireless=no\r\n");
#endif
#ifdef SUPPORT_VISCA
	size += sprintf(buf+size, "ptz=P,T,Z\r\n");
#else
	size += sprintf(buf+size, "ptz=\r\n");
#endif
	size += sprintf(buf+size, "inputs=%d\r\n", GIOIN_NUM);
	size += sprintf(buf+size, "outputs=%d\r\n", GIOOUT_NUM);
	//size += sprintf(buf+size, "speaker=%s\r\n", sys_info->config.support_config.supporttwowayaudio ? "yes" : "no" ); 
	size += sprintf(buf+size, "speaker=yes\r\n" );
	//size += sprintf(buf+size, "videoout=%s \r\n","");
	SQUASH_KA(req);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		
  	req->status = WRITE;
  	return 1;
}
int uri_camera_info(request * req)
{
	char buf[256];
	size_t size = 0;
	int i = 0;
	
	uri_decoding(req, req->query_string);
	
	 for(i = 0 ; i <= (req->cmd_count-1) ; i++)
  	{
		if(strcmp(req->cmd_arg[i].name, "name") == 0){			
				if(strcmp(conf_string_read(TITLE), req->cmd_arg[i].value))
					ControlSystemData(SFIELD_SET_TITLE, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);		
		}//else if(strcmp(req->cmd_arg[i].name, "location") == 0){
			//	
		//}
	}

	size += sprintf(buf+size, "name=%s\r\n", conf_string_read(TITLE)); 
	size += sprintf(buf+size, "location=%s\r\n", "");
	SQUASH_KA(req);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
		
	req->status = WRITE;
	return 1;
}
int uri_datetime(request * req)
{
	char buf[256];
	size_t size = 0;
	int value;
	int i;
	int year, month, day, hour, min, sec;
	int ret;
	int dst_month, dst_week, dst_day, dst_hour, dst_minute, start_sec=0, stop_sec=0;
	
	uri_decoding(req, req->query_string);
	//set_time_frequency
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "method") == 0){
			value = atoi(req->cmd_arg[i].value);

			if(sys_info->ipcam[NTP_FREQUENCY].config.s8 != value) {

					if (value == 0)
						kill_ntpd();
					else if (value == 1) {
						if (exec_ntpd(conf_string_read(NTP_SERVER)) < 0)
							break;
					}else if (value <= MAX_NTP_FREQUENCY) {
						if (exec_ntpdate(conf_string_read( NTP_SERVER)) < 0)
							break;
					}else
						break;

					conf_lock();
					sys_info->ipcam[NTP_FREQUENCY].config.s8 = value;
					sys_info->osd_setup = 1;
					conf_unlock();
					info_set_flag(CONF_FLAG_CHANGE);
					ControlSystemData(SFIELD_SET_SNTP_FREQUENCY, (void *)&value, sizeof(value), &req->curr_user);
					
			}	
		}
		
		if(strcmp(req->cmd_arg[i].name, "timeserver") == 0){
			if (strcmp(conf_string_read(NTP_SERVER), req->cmd_arg[i].value)) {
				if (sys_info->ipcam[NTP_FREQUENCY].config.s8 == 1)
					exec_ntpd(req->cmd_arg[i].value);

				ControlSystemData(SFIELD_SET_SNTP_SERVER, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
			}
		}

		if(strcmp(req->cmd_arg[i].name, "timezone") == 0){	
			value = atoi(req->cmd_arg[i].value);
			if(sys_info->ipcam[NTP_TIMEZONE].config.u8 != value)
			  ControlSystemData(SFIELD_SET_TIMEZONE, (void *)&value, sizeof(value), &req->curr_user);
		}
		
		if(strcmp(req->cmd_arg[i].name, "date") == 0){	
			if (sscanf(req->cmd_arg[i].value, "%d%*c%d%*c%d", &year, &month, &day) != 3)
				break;
			ret = getmaxmday(year, month);
    	
			if (ret < 0 || ret < day)
				break;	
			if (sys_set_date(year, month, day) < 0)
				break;
    	
			ControlSystemData(SFIELD_SET_DATE, req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}	
		if(strcmp(req->cmd_arg[i].name, "time") == 0){	
			if (sscanf(req->cmd_arg[i].value, "%d:%d:%d", &hour, &min, &sec) != 3)
				break;
			dbg("time = %d:%d:%d\n", hour, min ,sec);
			if (sys_set_time(hour, min, sec) < 0)
				break;
        	
			ControlSystemData(SFIELD_SET_TIME, req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "dstenable") == 0){
			if(strcmp(req->cmd_arg[i].value, "yes") == 0)
				value = 1;
			else if(strcmp(req->cmd_arg[i].value, "no") == 0)
				value = 0;
			else
				value = -1;

			if(sys_info->ipcam[DST_ENABLE].config.u8 != value && value >= 0){
				ControlSystemData(SFIELD_SET_DAYLIGHT, (void *)&value, sizeof(value), &req->curr_user);
				setenv("TZ", sys_info->tzenv, 1);
            	tzset();
			}
		}
		if(strcmp(req->cmd_arg[i].name, "dstauto") == 0){
			if(strcmp(req->cmd_arg[i].value, "yes") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "no") == 0)
				value = 1;
			else
				value = -1;

			if(sys_info->ipcam[DST_MODE].config.u8 != value && value >= 0){//&& sys_info->config.lan_config.net.dst_enable == 1)
				ControlSystemData(SFIELD_SET_DST_MODE, (void *)&value, sizeof(value), &req->curr_user);
            	setenv("TZ", sys_info->tzenv, 1);
            	tzset();
			}
		}
		if(strcmp(req->cmd_arg[i].name, "offset") == 0){	
			value = atoi(req->cmd_arg[i].value);
			if(sys_info->ipcam[DST_SHIFT].config.value != value ){//&& sys_info->config.lan_config.net.dst_enable == 1){
				//dbg("offset=%d\n", value);
				ControlSystemData(SFIELD_SET_DST_SHIFT, (void *)&value, sizeof(value), &req->curr_user);
            	setenv("TZ", sys_info->tzenv, 1);
            	tzset();
			}
		}
		if(strcmp(req->cmd_arg[i].name, "starttime") == 0){
			if(sys_info->ipcam[DST_ENABLE].config.u8 == 0)
				break;
			if( sscanf(req->cmd_arg[i].value, "%d.%d.%d/%d:%d:%d", &dst_month, &dst_week, &dst_day, &dst_hour, &dst_minute, &start_sec) != 6)
				break;
			
			conf_lock();
			sys_info->ipcam[DSTSTART_MONTH].config.u8 = dst_month;
			sys_info->ipcam[DSTSTART_WEEK].config.u8 = dst_week;
			sys_info->ipcam[DSTSTART_DAY].config.u8 = dst_day;
			sys_info->ipcam[DSTSTART_HOUR].config.u8 = dst_hour;
			sys_info->ipcam[DSTSTART_MINUTE].config.u8 = dst_minute;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			setenv("TZ", sys_info->tzenv, 1);
       		tzset();
		}
		if(strcmp(req->cmd_arg[i].name, "stoptime") == 0){
			if(sys_info->ipcam[DST_ENABLE].config.u8 == 0)
				break;
			if( sscanf(req->cmd_arg[i].value, "%d.%d.%d/%d:%d:%d", &dst_month, &dst_week, &dst_day, &dst_hour, &dst_minute, &stop_sec) != 6)
				break;

			conf_lock();
			sys_info->ipcam[DSTEND_MONTH].config.u8 = dst_month;
			sys_info->ipcam[DSTEND_WEEK].config.u8 = dst_week;
			sys_info->ipcam[DSTEND_DAY].config.u8 = dst_day;
			sys_info->ipcam[DSTEND_HOUR].config.u8 = dst_hour;
			sys_info->ipcam[DSTEND_MINUTE].config.u8 = dst_minute;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			setenv("TZ", sys_info->tzenv, 1);
        	tzset();
		}
	}
	
	size += sprintf(buf+size, "method=%d\r\n", sys_info->ipcam[NTP_FREQUENCY].config.s8);
	size += sprintf(buf+size, "timeserver=%s\r\n", conf_string_read(NTP_SERVER));
	size += sprintf(buf+size, "timezone=%d\r\n", sys_info->ipcam[NTP_TIMEZONE].config.u8);
	size += sprintf(buf+size, "date=%d-%d-%d\r\n", sys_info->tm_now.tm_year+1900, sys_info->tm_now.tm_mon+1, sys_info->tm_now.tm_mday);
	size += sprintf(buf+size, "time=%d:%d:%d\r\n", sys_info->tm_now.tm_hour, sys_info->tm_now.tm_min, sys_info->tm_now.tm_sec);
	size += sprintf(buf+size, "dstenable=%s\r\n", sys_info->ipcam[DST_ENABLE].config.u8 ?"yes":"no");

	//if(sys_info->config.lan_config.net.dst_enable == 1){

		size += sprintf(buf+size, "dstauto=%s\r\n", sys_info->ipcam[DST_MODE].config.u8 ?"no":"yes"); 
		//if(sys_info->config.lan_config.net.dst_mode == 1){
			size += sprintf(buf+size, "offset=%d\r\n", sys_info->ipcam[DST_SHIFT].config.value);
			size += sprintf(buf+size, "starttime=%d.%d.%d/%d:%02d:%02d\r\n", sys_info->ipcam[DSTSTART_MONTH].config.u8, sys_info->ipcam[DSTSTART_WEEK].config.u8, sys_info->ipcam[DSTSTART_DAY].config.u8, 
						sys_info->ipcam[DSTSTART_HOUR].config.u8, sys_info->ipcam[DSTSTART_MINUTE].config.u8, start_sec);
			size += sprintf(buf+size, "stoptime=%d.%d.%d/%d:%02d:%02d\r\n", sys_info->ipcam[DSTEND_MONTH].config.u8, sys_info->ipcam[DSTEND_WEEK].config.u8, sys_info->ipcam[DSTEND_DAY].config.u8, 
						sys_info->ipcam[DSTEND_HOUR].config.u8, sys_info->ipcam[DSTEND_MINUTE].config.u8, stop_sec);

		//}
	//}
	SQUASH_KA(req);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
		
	req->status = WRITE;
	return 1;
}
int nipca_check_codec(NIPCA_VIDEO_CFG video)
{
	//dbg("codec = %s\n", video.codec);

	if((strcmp(video.codec, JPEG_PARSER_NAME) != 0)
			&&(strcmp(video.codec, MPEG4_PARSER_NAME) != 0)
			&&(strcmp(video.codec, H264_PARSER_NAME) != 0)){
					dbg("codec error");
					return -1;	
	}

	return 0;
}
int nipca_check_resolution(int profileid, NIPCA_VIDEO_CFG video)
{
	int i;

//dbg("resolution = %s\n", video.resolution);

#ifdef RESOLUTION_IMX036_3M
	int idx , cnt_3m=0, cnt_2m=0;
	char* res_tmp=NULL;

	//MPEG4 3M is not allowed
	if( !strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+(profileid*PROFILE_STRUCT_SIZE)), MPEG4_PARSER_NAME) )
		if( !strcmp(video.resolution, IMG_SIZE_2048x1536) )
			return INVALID_FIELD_MPEG4_RESOLUTION;
		
	//3M+3M+3M, 3M+3M+2M and 3M+2M+2M, is not allowed
	for(idx=0; idx<MAX_PROFILE_NUM;idx++){
		if(idx == profileid)
			res_tmp = video.resolution;
		else
			res_tmp = conf_string_read(PROFILE_CONFIG0_RESOLUTION+(idx*PROFILE_STRUCT_SIZE));
		
		//dbg("res_tmp=%s", res_tmp);	
		if(!strcmp(res_tmp, IMG_SIZE_2048x1536))	cnt_3m++;
		else if(!strcmp(res_tmp, IMG_SIZE_1600x1200))	cnt_2m++;
	}
	dbg("(3m, 2m)cnt = (%d, %d)", cnt_3m, cnt_2m);
	
	if( (cnt_3m + cnt_2m) > 2 )
		if(( cnt_3m >= 3) || (cnt_3m >= 2 && cnt_2m >= 1) || (cnt_3m >= 1 && cnt_2m >= 2)){
			DBG("Over range");
			return INVALID_FIELD_MEMORY_ALLOC;
		}
#endif		
	for(i=0;i<MAX_RESOLUTIONS;i++){
		if(!strcmp(sys_info->api_string.resolution_list[i], video.resolution) )
			return 0;
	}

	return INVALID_FIELD_NOT_FOUND_RESOLUTION;

}
int nipca_check_framerate(NIPCA_VIDEO_CFG video)
{
	//dbg("framerate = %s\n", video.framerate);

	if((strcmp(video.framerate, FRAMERATE_15FPS) != 0)
		&&(strcmp(video.framerate, FRAMERATE_7FPS) != 0)
		&&(strcmp(video.framerate, FRAMERATE_4FPS) != 0)
		&&(strcmp(video.framerate, FRAMERATE_1FPS) != 0)){
		if(sys_info->video_type == 0){	//NTSC
			if(strcmp(video.framerate, FRAMERATE_30FPS) != 0){
				DBG("framerate error");
				return INVALID_FIELD_NOT_FOUND_FRAMERATE;
			}else{
				#ifdef RESOLUTION_IMX036_3M
				if( !strcmp(video.resolution, IMG_SIZE_2048x1536) )
					return INVALID_FIELD_FRAMERATE;
				#else
					return 0;
				#endif
			}
		}else{
			if(strcmp(video.framerate, FRAMERATE_25FPS) != 0){
				DBG("framerate error");
				return INVALID_FIELD_NOT_FOUND_FRAMERATE;
			}else{
				#ifdef RESOLUTION_IMX036_3M
				if( !strcmp(video.resolution, IMG_SIZE_2048x1536) )
					return INVALID_FIELD_FRAMERATE;
				#else
					return 0;
				#endif
			}
		}	
	}

	return 0;
}
int nipca_check_bitrate(NIPCA_VIDEO_CFG video)
{
	//dbg("bitrate = %s\n", video.bitrate);
	int list_id;

	if( !strcmp(video.codec, JPEG_PARSER_NAME))
		return -1;
	
	if(video.qmode != VIDEO_CBR)
		return -1;
	
	for(list_id=0;list_id<MAX_MP4_BITRATE;list_id++){
		if( strcmp( video.bitrate , sys_info->api_string.mp4_bitrate[list_id] ) == 0 ){
			return 0;
		}
	}

	dbg("bitrate error");
	return -1;
}
int nipca_check_quality(NIPCA_VIDEO_CFG video)
{
	//dbg("quality = %d\n", video.quality);
	if( !strcmp( video.codec , MPEG4_PARSER_NAME )  || !strcmp( video.codec , H264_PARSER_NAME ))
		if(video.qmode != VIDEO_FIXQUALITY)
			return -1;
		
	#if 0	
		if(value >= 70)	quality = 0;
		else if (value < 70 && value > 35 )	quality = 1;
		else	quality = 2;
	#else
		if(video.quality > 2){
			dbg("quality error");
			return -1;
		}
	#endif

	return 0;
}

int uri_video(request * req)
{
	char buf[512];
	size_t size = 0;
	__u8 change = 0;
	//req->is_cgi = CGI;
	int profile_id = -1;
	int idx = 0;
	int i, ret=0;

	NIPCA_VIDEO_CFG video_cfg[MAX_WEB_PROFILE_NUM];

  	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "profileid") == 0){
			profile_id = atoi(req->cmd_arg[i].value) - 1;
			idx = profile_id*PROFILE_STRUCT_SIZE;
		}
	}
#ifdef SUPPORT_PROFILE_NUMBER
	if(profile_id < 0 || profile_id >= sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value){
#else
	if(profile_id < 0 || profile_id >= MAX_WEB_PROFILE_NUM){
#endif
		dbg("no profileid");
		size += sprintf(buf+size, "error!\r\nno profileid!\r\n");
		send_request_dlink_ok(req, size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		req->status = WRITE;
		return 1;
	}

	//video init
	for(i = 0 ; i < MAX_WEB_PROFILE_NUM ; i++){
		idx = i*PROFILE_STRUCT_SIZE;
		strcpy(video_cfg[i].codec, conf_string_read(PROFILE_CONFIG0_CODECNAME+idx));
		strcpy(video_cfg[i].resolution, conf_string_read(PROFILE_CONFIG0_RESOLUTION+idx));
		strcpy(video_cfg[i].bitrate, conf_string_read(PROFILE_CONFIG0_CBR_QUALITY+idx));
		video_cfg[i].qmode = sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8;
		strcpy(video_cfg[i].framerate, conf_string_read(PROFILE_CONFIG0_FRAMERATE+idx));
		video_cfg[i].quality = sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY+idx].config.u8;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "codec") == 0){
			if(strcmp(req->cmd_arg[i].value, "MJPEG")==0)
				strcpy(video_cfg[profile_id].codec, JPEG_PARSER_NAME);
			else
				strncpy(video_cfg[profile_id].codec, req->cmd_arg[i].value, sizeof(video_cfg[profile_id].codec));
		}
		if(strcmp(req->cmd_arg[i].name, "resolution") == 0){
			strncpy(video_cfg[profile_id].resolution, req->cmd_arg[i].value, sizeof(video_cfg[profile_id].resolution));
		}
		if(strcmp(req->cmd_arg[i].name, "framerate") == 0){
			strncpy(video_cfg[profile_id].framerate, req->cmd_arg[i].value, sizeof(video_cfg[profile_id].framerate));
		}
		if(strcmp(req->cmd_arg[i].name, "bitrate") == 0){
			strncpy(video_cfg[profile_id].bitrate, req->cmd_arg[i].value, sizeof(video_cfg[profile_id].bitrate));
		}
		if(strcmp(req->cmd_arg[i].name, "quality") == 0){
			video_cfg[profile_id].quality= atoi(req->cmd_arg[i].value);
		}
		if(strcmp(req->cmd_arg[i].name, "qualitymode") == 0){
			dbg("req->cmd_arg[i].value=%s", req->cmd_arg[i].value);
			if(!strcmp(req->cmd_arg[i].value, "CBR"))
				video_cfg[profile_id].qmode = VIDEO_CBR;
			else if(!strcmp(req->cmd_arg[i].value, "Fixquality"))
				video_cfg[profile_id].qmode = VIDEO_FIXQUALITY;
		}

	}
	dbg("video_cfg[profile_id].codec = %s\n", video_cfg[profile_id].codec);
	dbg("video_cfg[profile_id].resolution = %s\n", video_cfg[profile_id].resolution);
	dbg("video_cfg[profile_id].framerate = %s\n", video_cfg[profile_id].framerate);
	dbg("video_cfg[profile_id].bitrate = %s\n", video_cfg[profile_id].bitrate);
	dbg("video_cfg[profile_id].quality = %d\n", video_cfg[profile_id].quality);
	dbg("video_cfg[profile_id].qmode = %d\n", video_cfg[profile_id].qmode);

	do{
		idx = profile_id*PROFILE_STRUCT_SIZE;

		dbg("=====setup start=====");
		/*setup codec*/
		if(nipca_check_codec(video_cfg[profile_id]) < 0)	
			strcpy(video_cfg[profile_id].codec, conf_string_read(PROFILE_CONFIG0_CODECNAME+idx));
		else{
			if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+idx), video_cfg[profile_id].codec ) != 0 ){
				change = NEED_RESET;
				dbg("setup codec");
				info_write(video_cfg[profile_id].codec, PROFILE_CONFIG0_CODECNAME+idx, HTTP_OPTION_LEN, 0);
			}
		}

		/*setup resolution*/
		ret = nipca_check_resolution(profile_id, video_cfg[profile_id]);
		if( ret == INVALID_FIELD_MPEG4_RESOLUTION)	
			strcpy(video_cfg[profile_id].resolution, IMG_SIZE_1600x1200);
		else if(ret == INVALID_FIELD_MEMORY_ALLOC)
			strcpy(video_cfg[profile_id].resolution, IMG_SIZE_1024x768);
		else if(ret == INVALID_FIELD_NOT_FOUND_RESOLUTION){
			DBG("INVALID_FIELD_NOT_FOUND_RESOLUTION");
			break;
		}
#ifdef SUPPORT_EPTZ
		if( (strcmp(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE+idx), video_cfg[profile_id].resolution) != 0))
#else
		if( (strcmp(conf_string_read(PROFILE_CONFIG0_RESOLUTION+idx), video_cfg[profile_id].resolution) != 0))
#endif
		{
			change = NEED_RESET;
			dbg("setup resolution");
            sys_info->ipcam[OLD_PROFILE_SIZE0_XSIZE+profile_id*2].config.value = sys_info->ipcam[PROFILESIZE0_XSIZE+profile_id*2].config.value;
            sys_info->ipcam[OLD_PROFILE_SIZE0_YSIZE+profile_id*2].config.value = sys_info->ipcam[PROFILESIZE0_YSIZE+profile_id*2].config.value;

			info_write(video_cfg[profile_id].resolution, PROFILE_CONFIG0_RESOLUTION+idx, HTTP_OPTION_LEN, 0);
#ifdef SUPPORT_EPTZ
			info_write(video_cfg[profile_id].resolution, PROFILE_CONFIG0_EPTZWINDOWSIZE+idx, HTTP_OPTION_LEN, 0);
#endif
		}

		/*setup framerate*/
		ret = nipca_check_framerate(video_cfg[profile_id]);
		if( ret < 0){	
			if(ret == INVALID_FIELD_FRAMERATE)
				strcpy(video_cfg[profile_id].framerate, FRAMERATE_15FPS);
		}
		if(strcmp(conf_string_read(PROFILE_CONFIG0_FRAMERATE+idx), video_cfg[profile_id].framerate) != 0){
			dbg("setup framerate");
			switch(profile_id)
			{
				default:
				case 0:	ControlSystemData(SFIELD_SET_PROFILE1_FRAMERATE, video_cfg[profile_id].framerate, strlen(video_cfg[profile_id].framerate)+1, &req->curr_user);
						break;
				case 1:	ControlSystemData(SFIELD_SET_PROFILE2_FRAMERATE, video_cfg[profile_id].framerate, strlen(video_cfg[profile_id].framerate)+1, &req->curr_user);
						break;
				case 2:	ControlSystemData(SFIELD_SET_PROFILE3_FRAMERATE, video_cfg[profile_id].framerate, strlen(video_cfg[profile_id].framerate)+1, &req->curr_user);
						break;
			}
		}

		/*setup qualitymode*/
		if( (!strcmp(video_cfg[profile_id].codec, MPEG4_PARSER_NAME)) || (!strcmp(video_cfg[profile_id].codec, H264_PARSER_NAME)) ){
			if(video_cfg[profile_id].qmode > VIDEO_FIXQUALITY)	
				video_cfg[profile_id].qmode = sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8;
			else{
				if( video_cfg[profile_id].qmode !=  sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8){
					dbg("setup qualitymode");
					change = NEED_RESET;
					info_write(&video_cfg[profile_id].qmode, PROFILE_CONFIG0_QMODE+idx, 0, 0);
				}
			}
		}
		
		/*setup bitrate*/
		if(nipca_check_bitrate(video_cfg[profile_id]) < 0)	
			strcpy(video_cfg[profile_id].bitrate, conf_string_read(PROFILE_CONFIG0_CBR_QUALITY+idx));
		else{
			if( video_cfg[profile_id].qmode !=  sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8){
				change = NEED_RESET;
				info_write(&video_cfg[profile_id].qmode, PROFILE_CONFIG0_QMODE+idx, 0, 0);
			}
			if(strcmp(conf_string_read(PROFILE_CONFIG0_CBR_QUALITY+idx), video_cfg[profile_id].bitrate) != 0){
				dbg("setup bitrate");

				switch(profile_id)
				{
					default:
					case 0:	ControlSystemData(SFIELD_SET_PROFILE1_CBR, video_cfg[profile_id].bitrate, strlen(video_cfg[profile_id].bitrate)+1, &req->curr_user);
							break;
					case 1:	ControlSystemData(SFIELD_SET_PROFILE2_CBR, video_cfg[profile_id].bitrate, strlen(video_cfg[profile_id].bitrate)+1, &req->curr_user);
							break;
					case 2:	ControlSystemData(SFIELD_SET_PROFILE3_CBR, video_cfg[profile_id].bitrate, strlen(video_cfg[profile_id].bitrate)+1, &req->curr_user);
							break;
				}
			}
			
		}
		
		/*setup quality*/
		if(nipca_check_quality(video_cfg[profile_id]) < 0){
			if( !strcmp( video_cfg[profile_id].codec , MPEG4_PARSER_NAME )  || !strcmp( video_cfg[profile_id].codec , H264_PARSER_NAME ))
				video_cfg[profile_id].quality = sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY+idx].config.u8;
			else if( !strcmp( video_cfg[profile_id].codec , JPEG_PARSER_NAME ) )
				video_cfg[profile_id].quality = sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY+idx].config.u8;
			else{
				DBG("ENCODER(%s) ?", conf_string_read(PROFILE_CONFIG0_CODECNAME+idx));
				break;
			}
		}else{
			if( video_cfg[profile_id].qmode !=  sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8){
				change = NEED_RESET;
				info_write(&video_cfg[profile_id].qmode, PROFILE_CONFIG0_QMODE+idx, 0, 0);
			}
			
			switch(profile_id)
			{
				default:
				case 0:	ControlSystemData(SFIELD_SET_PROFILE1_QUALITY, &video_cfg[profile_id].quality, sizeof(video_cfg[profile_id].quality), &req->curr_user);
						break;
				case 1:	ControlSystemData(SFIELD_SET_PROFILE2_QUALITY, &video_cfg[profile_id].quality, sizeof(video_cfg[profile_id].quality), &req->curr_user);
						break;
				case 2:	ControlSystemData(SFIELD_SET_PROFILE3_QUALITY, &video_cfg[profile_id].quality, sizeof(video_cfg[profile_id].quality), &req->curr_user);
						break;
			}
		}
		
	}while(0);
		
		dbg("==========================================\n");
		dbg("video_cfg[profile_id].codec = %s\n", video_cfg[profile_id].codec);
		dbg("video_cfg[profile_id].resolution = %s\n", video_cfg[profile_id].resolution);
		dbg("video_cfg[profile_id].framerate = %s\n", video_cfg[profile_id].framerate);
		dbg("video_cfg[profile_id].bitrate = %s\n", video_cfg[profile_id].bitrate);
		dbg("video_cfg[profile_id].quality = %d\n", video_cfg[profile_id].quality);
		dbg("video_cfg[profile_id].qmode = %s\n", video_cfg[profile_id].qmode?"FIXQUALITY":"CBR");

	//dbg("restart = %d\n", change);

	//restart
	if(change == NEED_RESET){
		change = 1;
		sys_info->procs_status.reset_status = NEED_RESET;
		ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&change, sizeof(change), &req->curr_user);
		sys_info->procs_status.avserver = AV_SERVER_RESTARTING;
		change = DONT_RESET;
	}

	//display
  	if(strcmp(req->cmd_arg[0].name, "profileid") == 0){	
		//profile_id = atoi(req->cmd_arg[0].value) - 1;
		//fprintf(stderr, "profileid = %d\n", profile_id);
		SQUASH_KA(req);
		size += sprintf(buf+size, "profileid=%d\r\n", profile_id+1);

		#ifdef SUPPORT_EPTZ
		size += sprintf(buf+size, "resolution=%s\r\n",conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE+idx));
		#else
		size += sprintf(buf+size, "resolution=%s\r\n",conf_string_read(PROFILE_CONFIG0_RESOLUTION+idx));
		#endif

		if(strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+idx), JPEG_PARSER_NAME)==0)
			size += sprintf(buf+size, "codec=MJPEG\r\n");
		else
			size += sprintf(buf+size, "codec=%s\r\n", conf_string_read(PROFILE_CONFIG0_CODECNAME+idx));
		
		if(strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+idx) , MPEG4_PARSER_NAME ) == 0 )
			size += sprintf(buf+size, "goplength=10\r\n");
			
		size += sprintf(buf+size, "framerate=%s\r\n", conf_string_read(PROFILE_CONFIG0_FRAMERATE+idx));
		
		if( (!strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+idx) , MPEG4_PARSER_NAME)) || (!strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+idx) , H264_PARSER_NAME)) )
		{
			size += sprintf(buf+size, "qualitymode=%s\r\n", sys_info->ipcam[PROFILE_CONFIG0_QMODE+idx].config.u8?"Fixquality":"CBR");
			size += sprintf(buf+size, "bitrate=%s\r\n", conf_string_read(PROFILE_CONFIG0_CBR_QUALITY+idx));
			switch(sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY+idx].config.u8){
				default:
				case 0:size += sprintf(buf+size, "quality=Excellent\r\n");
					break;
				case 1:size += sprintf(buf+size, "quality=Good\r\n");
					break;
				case 2:size += sprintf(buf+size, "quality=Standard\r\n");
					break;
			}
		}else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+idx) , JPEG_PARSER_NAME ) == 0 ){
			switch(sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY+idx].config.u8){
				default:
				case 0:size += sprintf(buf+size, "quality=Excellent\r\n");
					break;
				case 1:size += sprintf(buf+size, "quality=Good\r\n");
					break;
				case 2:size += sprintf(buf+size, "quality=Standard\r\n");
					break;
			}
		}else 
			dbg("display bitrate and quality error (codecname?)");
		
		send_request_dlink_ok(req, size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	}else
		return 1;

	req->status = WRITE;
  	return 1;
}

int uri_motion(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	__u8 en_value;
	__u16 value;
	extern int para_motionblock(request *req, char *data, char *arg);
	static char tempBuff[MAX_MOTION_BLK + 1] = "";
	
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "enable") == 0){
			en_value = atoi(req->cmd_arg[i].value);
			if(strcmp(req->cmd_arg[i].value, "yes") == 0){
				en_value = 1;
			}
			if(strcmp(req->cmd_arg[i].value, "no") == 0){
				en_value = 0;
			}
			//if(sys_info->config.motion_config.motionenable != en_value)
			if(sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8 != en_value)
				ControlSystemData(SFIELD_SET_MOTIONENABLE, &en_value, sizeof(en_value), &req->curr_user);
		}
		
		if(strcmp(req->cmd_arg[i].name, "mbmask") == 0 /*&& strlen(req->cmd_arg[i].value) == 24*/){		

				if( strlen(req->cmd_arg[i].value) > MAX_MOTION_BLK )
				{
					DBG("mbmask len error!\n");
					break;
				}
    		
				if(ControlSystemData(SFIELD_SET_MOTIONBLOCK, req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user) < 0)
					break;
				
		}/*else{
				DBG("no mbmask\n");
				para_motionblock(req, tempBuff,NULL);

		}*/
		if(strcmp(req->cmd_arg[i].name, "sensitivity") == 0){
			//value = atoi(req->cmd_arg[i].value)*5+12;
			value = atoi(req->cmd_arg[i].value);
			//dbg("sensitivity value = %d\n", value);

			if(sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16 != value)
				ControlSystemData(SFIELD_SET_MOTIONCVALUE, &value, sizeof(value), &req->curr_user);
		}
	}
	
	para_motionblock(req, tempBuff,NULL);

	size += sprintf(buf+size, "enable=%s\r\n", sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8 ? "yes" : "no");
	size += sprintf(buf+size, "mbmask=%s\r\n", tempBuff);	

	//size += sprintf(buf+size, "motioncvalue=%d\r\n", (sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16 -12)/5);
	size += sprintf(buf+size, "motioncvalue=%d\r\n", sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
  	return 1;
}

int uri_sensor_info(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	//req->is_cgi = CGI;
	
	SQUASH_KA(req);

#ifdef SUPPORT_WDRLEVEL
		size += sprintf(buf+size, "wdrlevel=0...%d\n", MAX_WDRLEVEL);
#else
		dbg("no supportwdrlevel");
#endif
#ifdef SUPPORT_BRIGHTNESS
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "brightness=0...8\r\n");
		#else
			size += sprintf(buf+size, "brightness=%d...%d\r\n", MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		#endif
#else
		size += sprintf(buf+size, "brightness=\r\n");
#endif
#ifdef SUPPORT_CONTRAST
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "contrast=0...8\r\n");
		#else
			size += sprintf(buf+size, "contrast=%d...%d\r\n", MIN_CONTRAST, MAX_CONTRAST);
		#endif
#else
		size += sprintf(buf+size, "contrast=\r\n");
#endif
#ifdef SUPPORT_SATURATION
		size += sprintf(buf+size, "saturation=%d...%d\r\n", MIN_SATURATION, MAX_SATURATION);
#else
		size += sprintf(buf+size, "saturation=\r\n");
#endif
#ifdef SUPPORT_SHARPNESS
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "sharpness=0...8\r\n");
		#else
			size += sprintf(buf+size, "sharpness=%d...%d\r\n", MIN_SHARPNESS, MAX_SHARPNESS);
		#endif
#else
		#ifdef SUPPORT_VISCA
			size += sprintf(buf+size, "sharpness=%d...%d\r\n", MIN_SHARPNESS, MAX_SHARPNESS);
		#else
			size += sprintf(buf+size, "sharpness=\r\n");
		#endif
#endif
#ifdef SUPPORT_AWB
		size += sprintf(buf+size, "whitebalance=%s", sys_info->api_string.awb[0]);
		for(i=1;i<MAX_AWB;i++){
			if(!strcmp(sys_info->api_string.awb[i], ""))
				break;
			size += sprintf(buf+size, ",%s", sys_info->api_string.awb[i]);
		}
		size += sprintf(buf+size, "\r\n");
#else
		#ifdef SUPPORT_VISCA
		size += sprintf(buf+size, "whitebalance=Auto,Indoor,Outdoor,ATW\r\n");
		#else
		size += sprintf(buf+size, "whitebalance=\r\n");
		#endif
		dbg("UNKNOWN SENSOR SUPPORT AWB");
#endif

#ifdef SUPPORT_EXPOSURETIME
	#if defined (SENSOR_USE_IMX035) || defined (SENSOR_USE_IMX076)
		//size += sprintf(buf+size, "maxexposuretime=1000,750,500,120,30\r\n");
		size += sprintf(buf+size, "maxexposuretime=\r\n");
		size += sprintf(buf+size, "autoexposure=\r\n");
	#elif defined (SENSOR_USE_MT9V136)
		size += sprintf(buf+size, "maxexposuretime=2000,1000,500,250,120,100\r\n");
		size += sprintf(buf+size, "autoexposure=yes,no\r\n");
	#else
		dbg("UNKNOWN SENSOR SUPPORT EXPOSURE");
	#endif
#else
		size += sprintf(buf+size, "maxexposuretime=\r\n");
		size += sprintf(buf+size, "autoexposure=\r\n");
		dbg("no supportexposuretime");
		
#endif

#ifdef SUPPORT_AGC
#if 0
	if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
		if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_NIGHT)){
			if(!strcmp(conf_string_read(EXPOSURE_NIGHT_GAIN), EXPOSURE_MODE_NIGHT_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else
				size += sprintf(buf+size, "autogainctrl=no\r\n");
		}else{
			if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_INDOOR_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_OUTDOOR_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_MOVING_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_LOWNOISE_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_CUSTOMIZE1_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_CUSTOMIZE2_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR) && 
					!strcmp(conf_string_read(EXPOSURE_CUSTOMIZE3_GAIN), EXPOSURE_MODE_GAIN_DEF))
				size += sprintf(buf+size, "autogainctrl=yes\r\n");
			else
				size += sprintf(buf+size, "autogainctrl=no\r\n");
		}
	}else if(EXPOSURETYPE == EXPOSURETIME_SELECT){
		size += sprintf(buf+size, "autogainctrl=%s", sys_info->api_string.agc[0]);
		for(i=1;i<MAX_AGC;i++){
			if(!strcmp(sys_info->api_string.agc[i], ""))
				break;
			size += sprintf(buf+size, ",%s", sys_info->api_string.agc[i]);

		}
		size += sprintf(buf+size, "\r\n");
	}else
		DBG("UNKNOWN EXPOSURETYPE(%d)", EXPOSURETYPE);
#endif
	size += sprintf(buf+size, "autogainctrl=no\r\n");
#else
	size += sprintf(buf+size, "autogainctrl=\r\n");
	dbg("UNKNOWN SENSOR SUPPORT AGC");
#endif
	
	size += sprintf(buf+size, "videooutformat=pal,ntsc\r\n");
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
  	return 1;
}
int uri_sensor(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	char exposuretime_buf[12]="";
	int agcctrl_value = 0;
	int flicker = 0;
	int setmode;
#ifdef SUPPORT_VISCA
	pid_t pid;
	char ptz_mode[8];
	char ptz_arg[64];
#endif
	//req->is_cgi = CGI;
	uri_decoding(req, req->query_string);

  for(i = 0 ; i <= (req->cmd_count-1) ; i++)
  {
  		__u8 value;
  		setmode = 1;
		
  			if(strcmp(req->cmd_arg[i].name, "brightness") == 0){
#ifdef SUPPORT_BRIGHTNESS
  				value = atoi(req->cmd_arg[i].value);
				#ifdef IMAGE_8_ORDER
					value *=  29;
				#endif
  				if(sys_info->ipcam[NBRIGHTNESS].config.u8 != value)
						ControlSystemData(SFIELD_SET_BRIGHTNESS, (void *)&value, sizeof(value), &req->curr_user);		
#endif		
			}else if(strcmp(req->cmd_arg[i].name, "contrast") == 0){
#ifdef SUPPORT_CONTRAST
				value = atoi(req->cmd_arg[i].value);
				#ifdef IMAGE_8_ORDER
					value *=  29;
				#endif
  				if(sys_info->ipcam[NCONTRAST].config.u8 != value)
						ControlSystemData(SFIELD_SET_CONTRAST, (void *)&value, sizeof(value), &req->curr_user);	
#endif		
			}else if(strcmp(req->cmd_arg[i].name, "saturation") == 0){
#ifdef SUPPORT_SATURATION
				value = atoi(req->cmd_arg[i].value);
  				if(sys_info->ipcam[NSATURATION].config.u8 != value)
						ControlSystemData(SFIELD_SET_SATURATION, (void *)&value, sizeof(value), &req->curr_user);	
#endif
			}else if(strcmp(req->cmd_arg[i].name, "sharpness") == 0){
#ifdef SUPPORT_SHARPNESS
				value = atoi(req->cmd_arg[i].value);
				#ifdef IMAGE_8_ORDER
					value *=  29;
				#endif
				
				if(sys_info->ipcam[NSHARPNESS].config.u8 != value)
						ControlSystemData(SFIELD_SET_SHARPNESS, (void *)&value, sizeof(value), &req->curr_user);
#else
				#ifdef SUPPORT_VISCA
				value = atoi(req->cmd_arg[i].value);
				dbg("value = %d", value);
				sprintf(ptz_mode, "%d", CAM_APETURE_DIRECT);
				sprintf(ptz_arg, "%d", value-1);
				dbg("ptz_mode = %s", ptz_mode);
				dbg("ptz_arg = %s", ptz_arg);
				if ( (pid = fork()) == 0 ) {
					/* the child */

					execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
					DBGERR("exec "VISCA_EXEC_PATH" failed\n");
					_exit(0);
				}
				else if (pid < 0) {
					DBGERR("fork "VISCA_EXEC_PATH" failed\n");
					continue;
				}
				sys_info->vsptz.aperture = value;
				#endif
#endif
			}else if(strcmp(req->cmd_arg[i].name, "flicker") == 0){
#ifdef SUPPORT_FLICKLESS
				flicker = atoi(req->cmd_arg[i].value);
				
  				if(flicker == 50) value = 1;
				else if (flicker == 60) value = 0;
				else value = 9;

				dbg("value = %d\n", value);
				if((value != 9)&& (value <= VIDEO_SYSTEM_VGA))
					ControlSystemData(SFIELD_SET_FLICKLESS, (void *)&value, sizeof(value), &req->curr_user);
#endif				
			}else if(strcmp(req->cmd_arg[i].name, "whitebalance") == 0 ){
				if(strcmp(sys_info->api_string.awb[i], req->cmd_arg[i].value) == 0){
	  				if( strcmp( conf_string_read(NWHITEBALANCE), req->cmd_arg[i].value ) != 0)
						ControlSystemData(SFIELD_SET_WHITE_BALANCE, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value) + 1, &req->curr_user);
				}
			}else if(strcmp(req->cmd_arg[i].name, "autoexposure") == 0){
				if(EXPOSURETYPE == EXPOSURETIME_SELECT){
	  				if(strcmp(req->cmd_arg[i].value, "yes") == 0)	strcpy(exposuretime_buf, "Auto");
	  				else if(strcmp(req->cmd_arg[i].value, "no") == 0)	strcpy(exposuretime_buf, "1/120");
	  				else	setmode = 0;
					
	  				if( setmode == 1){
						agcctrl_value = 1;
						info_write(&agcctrl_value, AGCCTRL, sizeof(agcctrl_value), 0);
						ControlSystemData(SFIELD_SET_EXPOSURE_TIME, (void *)exposuretime_buf, strlen(exposuretime_buf)+1, &req->curr_user);
	  				}
				}
			}else if(strcmp(req->cmd_arg[i].name, "maxexposuretime") == 0){
				if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
					if(strcmp(req->cmd_arg[i].value, "120") == 0)	strcpy(exposuretime_buf, EXPOSURETIME_INDOOR);
	  				else if(strcmp(req->cmd_arg[i].value, "750") == 0)	strcpy(exposuretime_buf, EXPOSURETIME_OUTDOOR);
					else if(strcmp(req->cmd_arg[i].value, "500") == 0)	strcpy(exposuretime_buf, EXPOSURETIME_NIGHT);
					else if(strcmp(req->cmd_arg[i].value, "1000") == 0)	strcpy(exposuretime_buf, EXPOSURETIME_MOVING);
					else if(strcmp(req->cmd_arg[i].value, "30") == 0)	strcpy(exposuretime_buf, EXPOSURETIME_LOW_NOISE);
	  				else	setmode = 0;
					
	  				if( setmode == 1){
						ControlSystemData(SFIELD_SET_EXPOSURE_TIME, (void *)exposuretime_buf, strlen(exposuretime_buf)+1, &req->curr_user);
	  				}
				}else{
#ifdef SENSOR_USE_IMX035
	  				if(strcmp(req->cmd_arg[i].value, "2000") == 0)	strcpy(exposuretime_buf, "1/2000");
	  				else if(strcmp(req->cmd_arg[i].value, "1500") == 0)	strcpy(exposuretime_buf, "1/1500");
	  				else if(strcmp(req->cmd_arg[i].value, "1000") == 0)	strcpy(exposuretime_buf, "1/1000");
	  				else if(strcmp(req->cmd_arg[i].value, "750") == 0)	strcpy(exposuretime_buf, "1/750");
	  				else if(strcmp(req->cmd_arg[i].value, "500") == 0)	strcpy(exposuretime_buf, "1/500");
	  				else if(strcmp(req->cmd_arg[i].value, "250") == 0)	strcpy(exposuretime_buf, "1/250");
	  				else if(strcmp(req->cmd_arg[i].value, "120") == 0)	strcpy(exposuretime_buf, "1/120");
	  				else if(strcmp(req->cmd_arg[i].value, "60") == 0)	strcpy(exposuretime_buf, "1/60");
	  				else if(strcmp(req->cmd_arg[i].value, "30") == 0)	strcpy(exposuretime_buf, "1/30");
	  				else if(strcmp(req->cmd_arg[i].value, "15") == 0)	strcpy(exposuretime_buf, "1/15");
	  				else if(strcmp(req->cmd_arg[i].value, "7.5") == 0)	strcpy(exposuretime_buf, "1/7.5");
	  				else if(strcmp(req->cmd_arg[i].value, "4") == 0)	strcpy(exposuretime_buf, "1/4");
	  				else if(strcmp(req->cmd_arg[i].value, "2") == 0)	strcpy(exposuretime_buf, "1/2");
	 				else	setmode = 0;
#elif defined(SENSOR_USE_MT9V136)
					if(strcmp(req->cmd_arg[i].value, "2000") == 0)	strcpy(exposuretime_buf, "1/2000");
	  				else if(strcmp(req->cmd_arg[i].value, "1000") == 0)	strcpy(exposuretime_buf, "1/1000");
	  				else if(strcmp(req->cmd_arg[i].value, "500") == 0)	strcpy(exposuretime_buf, "1/500");
	  				else if(strcmp(req->cmd_arg[i].value, "250") == 0)	strcpy(exposuretime_buf, "1/250");
	  				else if(strcmp(req->cmd_arg[i].value, "120") == 0)	strcpy(exposuretime_buf, "1/120");
					else if(strcmp(req->cmd_arg[i].value, "100") == 0)	strcpy(exposuretime_buf, "1/100");
					//else if(strcmp(req->cmd_arg[i].value, "50") == 0)	strcpy(exposuretime_buf, "1/60");
	  				//else if(strcmp(req->cmd_arg[i].value, "60") == 0)	strcpy(exposuretime_buf, "1/60");
	 				else	setmode = 0;
#endif

	  				if( strcmp( conf_string_read(A_EXPOSURE_TIME) , exposuretime_buf ) != 0 && setmode == 1){
						agcctrl_value = 1;
						info_write(&agcctrl_value, AGCCTRL, sizeof(agcctrl_value), 0);
						ControlSystemData(SFIELD_SET_EXPOSURE_TIME, (void *)exposuretime_buf, strlen(exposuretime_buf)+1, &req->curr_user);
					}
				}
			}else if (strcmp(req->cmd_arg[i].name, "backlightcomp") == 0 && strcmp(conf_string_read(A_EXPOSURE_TIME), EXPOSURETIME_AUTO) == 0){
#ifdef SUPPORT_BLCMODE
				if(strcmp(req->cmd_arg[i].value, "yes") == 0)	value = 1;
  				else if(strcmp(req->cmd_arg[i].value, "no") == 0)	value = 0;
				else value = 0;

				if(sys_info->ipcam[NBACKLIGHTCONTROL].config.u8 != value)
					ControlSystemData(SFIELD_SET_BLCMODE, (void *)&value, sizeof(value), &req->curr_user);
#endif
			}else if(strcmp(req->cmd_arg[i].name, "mirror") == 0){ 				 				
  				if(strcmp(req->cmd_arg[i].value, "on") == 0)
					#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136))
  					value = 0;
					#else
					value = 1;
					#endif
  				else if(strcmp(req->cmd_arg[i].value, "off") == 0)
					#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136))
  					value = 1;
					#else
					value = 0;
					#endif	
  				else
					setmode = 0;
				
  				if(sys_info->ipcam[NMIRROR].config.u8 != value && setmode == 1)
						ControlSystemData(SFIELD_SET_MIRROR, (void *)&value, sizeof(value), &req->curr_user);			
			}else if(strcmp(req->cmd_arg[i].name, "flip") == 0){ 				 				
  				if(strcmp(req->cmd_arg[i].value, "on") == 0)
  					#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136)) 
  					value = 0;
					#else
					value = 1;
					#endif
  				else if(strcmp(req->cmd_arg[i].value, "off") == 0)
  					#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136))
  					value = 1;
					#else
					value = 0;
					#endif	
  				else
					setmode = 0;
				
  				if(sys_info->ipcam[NFLIP].config.u8 != value && setmode == 1)
						ControlSystemData(SFIELD_SET_FLIP, (void *)&value, sizeof(value), &req->curr_user);	
			}else if(strcmp(req->cmd_arg[i].name, "autogainctrl") == 0){
				if(EXPOSURETYPE == EXPOSURETIME_SELECT){
#ifdef SENSOR_USE_IMX035
					dbg("nMaxAGCGain=%s\n", req->cmd_arg[i].value);
					if( strcmp( req->cmd_arg[i].value , sys_info->api_string.agc[i] ) == 0 ){
		  				if(strcmp( conf_string_read(NMAXAGCGAIN), req->cmd_arg[i].value ) != 0 ){
							agcctrl_value = 0;
							info_write(&agcctrl_value, AGCCTRL, sizeof(agcctrl_value), 0);
							ControlSystemData(SFIELD_SET_AEGAIN, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value)+1 , &req->curr_user);
						}
					}
#endif
				}
			}	
			
  }

  	SQUASH_KA(req);
#ifdef SUPPORT_BRIGHTNESS
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "brightness=%d\r\n",sys_info->ipcam[NBRIGHTNESS].config.u8/29);
		#else
			size += sprintf(buf+size, "brightness=%d\r\n",sys_info->ipcam[NBRIGHTNESS].config.u8);
		#endif
		
#endif
#ifdef SUPPORT_CONTRAST
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "contrast=%d\r\n",sys_info->ipcam[NCONTRAST].config.u8/29);
		#else
			size += sprintf(buf+size, "contrast=%d\r\n",sys_info->ipcam[NCONTRAST].config.u8);
		#endif
#endif
#ifdef SUPPORT_SATURATION
		size += sprintf(buf+size, "saturation=%d\r\n",sys_info->ipcam[NSATURATION].config.u8);
#endif
#ifdef SUPPORT_SHARPNESS
		#ifdef IMAGE_8_ORDER
			size += sprintf(buf+size, "sharpness=%d\r\n",sys_info->ipcam[NSHARPNESS].config.u8/29);
		#else
			size += sprintf(buf+size, "sharpness=%d\r\n",sys_info->ipcam[NSHARPNESS].config.u8);
		#endif
#else
		#ifdef SUPPORT_VISCA
			size += sprintf(buf+size, "sharpness=%d\r\n",sys_info->vsptz.aperture);
		#endif
#endif
#ifdef SUPPORT_AWB
		size += sprintf(buf+size, "whitebalance=%s\r\n",conf_string_read(NWHITEBALANCE));
#endif
#ifdef SUPPORT_FLICKLESS
		size += sprintf(buf+size, "flicker=%s\r\n", sys_info->video_type?"50":"60");
#endif

#ifdef SUPPORT_EXPOSURETIME
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
			#if 0
			size += sprintf(buf+size, "autoexposure=%s\r\n", conf_string_read(EXPOSURE_MODE));
			size += sprintf(buf+size, "maxexposuretime=");
			if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR))	size += sprintf(buf+size, "120\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_OUTDOOR))	size += sprintf(buf+size, "750\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_NIGHT))	size += sprintf(buf+size, "500\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_MOVING))	size += sprintf(buf+size, "1000\r\n");
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_LOW_NOISE))	size += sprintf(buf+size, "30\r\n");
			else{
				size += sprintf(buf+size, "#\r\n");
				DBG("UNKNOWN EXPOSURE_MODE(%s)", conf_string_read(EXPOSURE_MODE));
			}
			#else
			size += sprintf(buf+size, "autoexposure=no\r\n");
			size += sprintf(buf+size, "maxexposuretime=#\r\n");
			#endif
		}else{
			if(sys_info->ipcam[AGCCTRL].config.u8 == 0){ //agc mode
				size += sprintf(buf+size, "autoexposure=no\r\n");
				size += sprintf(buf+size, "maxexposuretime=#\r\n");
			}else if(strcmp( conf_string_read(A_EXPOSURE_TIME), "Auto" ) == 0){
				size += sprintf(buf+size, "autoexposure=yes\r\n");
				size += sprintf(buf+size, "maxexposuretime=Auto\r\n");
			}else{
				size += sprintf(buf+size, "autoexposure=no\r\n");	
				//dbg("exposuretime_buf=%s\n", (strchr(sys_info->config.lan_config.exposure_time, '/') + 1));
				if(strchr(conf_string_read(A_EXPOSURE_TIME), '/') != NULL)
					size += sprintf(buf+size, "maxexposuretime=%s\r\n", strchr(conf_string_read(A_EXPOSURE_TIME), '/') + 1);
			}
		}
#endif
#ifdef SUPPORT_AGC
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
			#if 0	//tmp, jiahung
			if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_INDOOR_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_OUTDOOR))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_OUTDOOR_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_NIGHT))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_NIGHT_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_MOVING)))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_MOVING_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_LOW_NOISE))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_LOWNOISE_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_CUSTOMIZE1_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_CUSTOMIZE2_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3))
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(EXPOSURE_CUSTOMIZE3_GAIN));
			else 
				DBG("UNKNOWN EXPOSURE_MODE(%s)", conf_string_read(EXPOSURE_MODE));
			#else
			size += sprintf(buf+size, "autogainctrl=no\r\n");
			#endif
		}else{	
			if(sys_info->ipcam[AGCCTRL].config.u8 == 0)
				size += sprintf(buf+size, "autogainctrl=%s\r\n", conf_string_read(NMAXAGCGAIN));
			else
				size += sprintf(buf+size, "autogainctrl=no\r\n");
		}
#endif
#ifdef SUPPORT_BLCMODE
		size += sprintf(buf+size, "backlightcomp=%s\r\n", sys_info->ipcam[NBACKLIGHTCONTROL].config.u8 ? "yes" : "no");
#endif
	
#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136))
	#ifdef SUPPORT_MIRROR
		size += sprintf(buf+size, "mirror=%s\r\n", sys_info->ipcam[NMIRROR].config.u8 ? "off" : "on");
	#endif
	#ifdef SUPPORT_FLIP
		size += sprintf(buf+size, "flip=%s\r\n", sys_info->ipcam[NFLIP].config.u8 ? "off" : "on");
	#endif
#else
	#ifdef SUPPORT_MIRROR
		size += sprintf(buf+size, "mirror=%s\r\n", sys_info->ipcam[NMIRROR].config.u8 ? "on" : "off");
	#endif
	#ifdef SUPPORT_FLIP
		size += sprintf(buf+size, "flip=%s\r\n", sys_info->ipcam[NFLIP].config.u8 ? "on" : "off");
	#endif
#endif

	size += sprintf(buf+size, "videoinformat=%s\r\n", sys_info->video_type ? "PAL" : "NTSC");
		 
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;

  return 1;
}
int uri_mic(request * req)
{
	char buf[512];
	//char vol_buf[12];
	size_t size = 0;
	int i;
	__u8 enable, volume;
	char volume_buf[16];
	
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "enable") == 0){	
			if(strcmp(req->cmd_arg[i].value, "yes") == 0)	enable = 1;
			else if(strcmp(req->cmd_arg[i].value, "no") == 0)	enable = 0;
			else enable = sys_info->ipcam[AUDIOINENABLE].config.u8;

			if(sys_info->ipcam[AUDIOINENABLE].config.u8 != enable )
				ControlSystemData(SFIELD_SET_AUDIOENABLE, (void *)&enable, sizeof(enable), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "volume") == 0){
			volume = atoi(req->cmd_arg[i].value);
			dbg("volume = %d\n", volume);
			
			#if defined PLAT_DM355
			if( sys_info->ipcam[MICININPUTGAIN].config.u8 != volume)
				ControlSystemData(SFIELD_SET_MICINGAIN, (void *)&volume, sizeof(volume), &req->curr_user);
			#elif defined PLAT_DM365
				if(volume == 0)	strcpy(volume_buf, AUDIOINGAIN_20_DB);
				else if(volume == 1)	strcpy(volume_buf, AUDIOINGAIN_26_DB);
				else strcpy(volume_buf, conf_string_read( EXTERNALMICGAIN));

				if( strcmp( conf_string_read( EXTERNALMICGAIN) , volume_buf ) != 0 )
					ControlSystemData(SFIELD_SET_EXTERNAL_MICINGAIN, (void *)volume_buf, strlen(volume_buf) + 1, &req->curr_user);
			#else
			#error Platform
			#endif
		}
	}
	//dbg( "sys_info->config.lan_config.audioinenable=%d\n",  sys_info->config.lan_config.audioinenable);
	//dbg( "sys_info->config.lan_config.micininputgain=%d\n",  sys_info->config.lan_config.micininputgain);
	SQUASH_KA(req);

	size += sprintf(buf+size,"enable=%s\r\n", sys_info->ipcam[AUDIOINENABLE].config.u8? "yes" : "no" );
	#if defined PLAT_DM355
	size += sprintf(buf+size,"volume=%d\r\n", sys_info->ipcam[MICININPUTGAIN].config.u8);
	#elif defined PLAT_DM365
	if(!strcmp(conf_string_read( EXTERNALMICGAIN), AUDIOINGAIN_20_DB))
		size += sprintf(buf+size,"volume=0\r\n");
	else if(!strcmp(conf_string_read( EXTERNALMICGAIN), AUDIOINGAIN_26_DB))
		size += sprintf(buf+size,"volume=1\r\n");
	else
		size += sprintf(buf+size,"volume=Nan\r\n");
	#else
	#error Platform
	#endif
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	req->status = DONE;

 	return 1;
}
int uri_speaker(request * req)
{
	char buf[512];
	size_t size = 0;
	int i; 
	__u8 value;

	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		value = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "enable") == 0){	
			if(strcmp(req->cmd_arg[i].value, "yes") == 0)	value = 1;
			else if(strcmp(req->cmd_arg[i].value, "no") == 0)	value = 0;
			else value = sys_info->ipcam[AUDIOOUTENABLE].config.u8;
				
			if(sys_info->ipcam[AUDIOOUTENABLE].config.u8 != value )
				ControlSystemData(SFIELD_SET_AUDIOOUTENABLE, &value, sizeof(value), &req->curr_user);
					
		}
		if(strcmp(req->cmd_arg[i].name, "volume") == 0){
			if(value > 10) value = 10;
			else if(value < 1) value = 1;
			
			if(sys_info->ipcam[AUDIOOUTVOLUME].config.u8 != value)
				ControlSystemData(SFIELD_SET_AUDIOOUTVOLUME, &value, sizeof(value), &req->curr_user);
		}
		
	}
	
	SQUASH_KA(req);

	size += sprintf(buf+size,"enable=%s\r\n", sys_info->ipcam[AUDIOOUTENABLE].config.u8? "yes" : "no" );
	size += sprintf(buf+size,"volume=%d\r\n", sys_info->ipcam[AUDIOOUTVOLUME].config.u8);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	req->status = DONE;

  	return 1;
}
int uri_led(request * req)
{
#if (SUPPORT_LED == 1)
	char buf[512];
	size_t size = 0;
	int i, value = -1;
	
	uri_decoding(req, req->query_string);	
	value = sys_info->ipcam[LED_STATUS].config.u8;
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "led") == 0){	
			if(strcmp(req->cmd_arg[i].value, "on") == 0){
				value = 1;
			}
			if(strcmp(req->cmd_arg[i].value, "off") == 0){
				value = 0;
			}
		}
		
	}
	
	if(sys_info->ipcam[LED_STATUS].config.u8 != value)
		info_write(&value, LED_STATUS, sizeof(value), 0);

	size += sprintf(buf+size,"led=%s\r\n", sys_info->ipcam[LED_STATUS].config.u8 ? "on" : "off" );

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
#else
	return -1;
#endif
}
int uri_reboot(request * req)
{
	//req->is_cgi = CGI;	

 	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }      
  SQUASH_KA(req);
  int cmp = strcmp(req->cmd_arg[0].value, "go");
	
	if(strcmp(req->cmd_arg[0].name, "reboot") == 0)
		req->buffer_end += sprintf(req_bufptr(req),"reboot=%s<br>",cmp ? "fail" : "yes" );
	
	if(cmp == 0){
		//system("reboot -f");
		#ifdef SUPPORT_AF
		SetMCUWDT(0);
		#endif
		softboot();
	}
	
	req->status = DONE;
  return 1;
}
int uri_reset(request * req)
{
	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }   
    SQUASH_KA(req);
    int cmp = strcmp(req->cmd_arg[0].value, "go");
	
	if(strcmp(req->cmd_arg[0].name, "reset") == 0)
		req->buffer_end += sprintf(req_bufptr(req),"reset=%s<br>",cmp ? "fail" : "yes" );
	
	if(cmp == 0)
		ControlSystemData(SFIELD_FACTORY_DEFAULT, sys_info, sizeof(sys_info), &req->curr_user);
			
    req->status = DONE;
 	return 1;
}
int uri_io(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	int id, enable = 0;
	__u32 value = 0;
	//req->is_cgi = CGI;
   	uri_decoding(req, req->query_string);

  	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
  		if(strcmp(req->cmd_arg[i].name, "out1") == 0){
  				id = 1;
  				if(strcmp(req->cmd_arg[i].value, "on") == 0){
  					enable = 1;
  					sys_info->status.alarm_out1 = 1;
  				}else if(strcmp(req->cmd_arg[i].value, "off") == 0){
  					enable = 0;
  					sys_info->status.alarm_out1 = 0;
  				}else
  					break;
  				
  				//if(sys_info->config.lan_config.gioout_alwayson[id] != enable) {
  				if(sys_info->ipcam[GIOOUT_ALWAYSON0+id].config.u16!= enable) {
					value = (id << 16) | enable;
					if(ControlSystemData(SFIELD_SET_GIOOUT_ALWAYSON, &value, sizeof(value), &req->curr_user) < 0)
					break;
				}
  		}
  	}

  	SQUASH_KA(req);
	#if (GIOIN_NUM >= 1)
	size += sprintf(buf+size,"in1=%s\r\n", sys_info->status.alarm1 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 2)
	size += sprintf(buf+size,"in2=%s\r\n", sys_info->status.alarm2 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 3)
	size += sprintf(buf+size,"in3=%s\r\n", sys_info->status.alarm3 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 4)
	size += sprintf(buf+size,"in4=%s\r\n", sys_info->status.alarm4 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 5)
	size += sprintf(buf+size,"in5=%s\r\n", sys_info->status.alarm5 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 6)
	size += sprintf(buf+size,"in6=%s\r\n", sys_info->status.alarm6 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 7)
	size += sprintf(buf+size,"in7=%s\r\n", sys_info->status.alarm7 ? "on" : "off" );
	#endif
	#if (GIOIN_NUM >= 8)
	size += sprintf(buf+size,"in8=%s\r\n", sys_info->status.alarm8 ? "on" : "off" );
	#endif

	#if (GIOOUT_NUM >= 1)
	size += sprintf(buf+size,"out1=%s\r\n", sys_info->status.alarm_out1  ? "on" : "off" );
	#endif
	#if (GIOOUT_NUM >= 2)
	size += sprintf(buf+size,"out2=%s\r\n", sys_info->status.alarm_out2  ? "on" : "off" );
	#endif
	#if (GIOOUT_NUM >= 3)
	size += sprintf(buf+size,"out3=%s\r\n", sys_info->status.alarm_out3  ? "on" : "off" );
	#endif
	#if (GIOOUT_NUM >= 4)
	size += sprintf(buf+size,"out4=%s\r\n", sys_info->status.alarm_out4  ? "on" : "off" );
	#endif
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
 	return 1;
}
int uri_rs485(request * req)
{
#ifdef SUPPORT_RS485
	char buf[512];
	int i = 0;
	size_t size = 0;
	//req->is_cgi = CGI;

	uri_decoding(req, req->query_string);
       
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
  {
  		__u8 value = atoi(req->cmd_arg[i].value);
  		if(strcmp(req->cmd_arg[i].name, "enable") == 0){
  			if(strcmp(req->cmd_arg[i].value, "no") == 0)
  				value = 0;
  			else if(strcmp(req->cmd_arg[i].value, "yes") == 0)
  				value = 1;
  			else
  				break;

  			if(sys_info->ipcam[RS485_ENABLE].config.u8 != value) {
				/*if(value == 1) {
					if(rs485_init(sys_info) < 0)
						break;
				}
				else
					rs485_free();*/
  				ControlSystemData(SFIELD_SET_485_ENABLE, &value, sizeof(value), &req->curr_user);
  			}
  		}
  		if(strcmp(req->cmd_arg[i].name, "proto") == 0){
  			if(strcmp(req->cmd_arg[i].value, "PelcoD") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "PelcoP") == 0)
				value = 1;
			else
				break;

			if(sys_info->ipcam[RS485_PROTOCOL].config.u8 != value)
  				ControlSystemData(SFIELD_SET_485_PROTOCOL, &value, sizeof(value), &req->curr_user);
  		}		
  		if(strcmp(req->cmd_arg[i].name, "devid") == 0){
//  			if(value <= 0 || value >= 256)  // fixed by javier -> always false
//  				break;

  			if(sys_info->ipcam[RS485_ID].config.u8 != value )
  				ControlSystemData(SFIELD_SET_485_ID, &value, sizeof(value), &req->curr_user);
  		}
  		if(strcmp(req->cmd_arg[i].name, "baudrate") == 0){
  			if(strcmp(req->cmd_arg[i].value, "2400") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "4800") == 0)
				value = 1;
			else if(strcmp(req->cmd_arg[i].value, "9600") == 0)
				value = 2;
			else if(strcmp(req->cmd_arg[i].value, "19200") == 0)
				value = 3;
			else
				break;

			if(sys_info->ipcam[RS485_SPEED].config.u8 != value)
  				ControlSystemData(SFIELD_SET_485_SPEED, &value, sizeof(value), &req->curr_user);  	
  		}	
  		if(strcmp(req->cmd_arg[i].name, "databits") == 0){
  			if(strcmp(req->cmd_arg[i].value, "7") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "8") == 0)
				value = 1;
			else
				break;

			if(sys_info->ipcam[RS485_DATA].config.u8 != value)
  				ControlSystemData(SFIELD_SET_485_DATA, &value, sizeof(value), &req->curr_user);  
  		}		
  		if(strcmp(req->cmd_arg[i].name, "parity") == 0){
  			if(strcmp(req->cmd_arg[i].value, "none") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "even") == 0)
				value = 1;
			else if(strcmp(req->cmd_arg[i].value, "odd") == 0)
				value = 2;
			else
				break;

			if(sys_info->ipcam[RS485_PARITY].config.u8 != value)
  				ControlSystemData(SFIELD_SET_485_PARITY, &value, sizeof(value), &req->curr_user); 
  		} 		
  		if(strcmp(req->cmd_arg[i].name, "stopbits") == 0){
  			if(strcmp(req->cmd_arg[i].value, "1") == 0)
				value = 0;
			else if(strcmp(req->cmd_arg[i].value, "2") == 0)
				value = 1;
			else
				break;

			if(sys_info->ipcam[RS485_STOP].config.u8 != value)
  				ControlSystemData(SFIELD_SET_485_STOP, &value, sizeof(value), &req->curr_user);  	
  		}	
  }
	SQUASH_KA(req);
	size += sprintf(buf+size, "enable=%s\r\n", sys_info->ipcam[RS485_ENABLE].config.u8 ?"yes":"no");
	size += sprintf(buf+size, "proto=%s\r\n", sys_info->ipcam[RS485_PROTOCOL].config.u8 ?"PelcoP":"PelcoD");
	size += sprintf(buf+size, "devid=%d\r\n", sys_info->ipcam[RS485_ID].config.u8 );
	size += sprintf(buf+size, "baudrate=%d\r\n", 2400*(1<<sys_info->ipcam[RS485_SPEED].config.u8));
	size += sprintf(buf+size, "databits=%d\r\n", sys_info->ipcam[RS485_DATA].config.u8? 8 : 7);

	switch(sys_info->ipcam[RS485_PARITY].config.u8){
	
		default:
		case 0:	size += sprintf(buf+size, "parity=none\r\n"); 
					break;
		case 1:	size += sprintf(buf+size, "parity=even\r\n"); 
					break;
		case 2:	size += sprintf(buf+size, "parity=odd\r\n"); 
					break;
	}	

	size += sprintf(buf+size, "stopbits=%d\r\n", sys_info->ipcam[RS485_STOP].config.u8? 2 : 1);
	size += sprintf(buf+size, "home=\r\n");
	size += sprintf(buf+size, "up=\r\n");
	size += sprintf(buf+size, "down=\r\n");
	size += sprintf(buf+size, "left=\r\n");
	size += sprintf(buf+size, "right=\r\n");
	size += sprintf(buf+size, "stop=\r\n");
	size += sprintf(buf+size, "wide=\r\n");
	size += sprintf(buf+size, "tele=\r\n");
	size += sprintf(buf+size, "focusfar=\r\n");
	size += sprintf(buf+size, "focusnear=\r\n");
	//size += sprintf(buf+size, "stoppattern=0000\r\n");
	//size += sprintf(buf+size, "cmdname1=\r\n");
	//size += sprintf(buf+size, "cmdname2=\r\n");
	//size += sprintf(buf+size, "cmdname3=\r\n");
	//size += sprintf(buf+size, "cmdname4=\r\n");
	//size += sprintf(buf+size, "cmdstr1=\r\n");
	//size += sprintf(buf+size, "cmdstr2=\r\n");
	//size += sprintf(buf+size, "cmdstr3=\r\n");
	//size += sprintf(buf+size, "cmdstr4=\r\n");
	//size += sprintf(buf+size, "delaytime=2000\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
  return 1;
#else
	return -1;
#endif
}

int uri_user_mod(request * req)
{	
	int i, blank_idx = -1;
	int name_len, pass_len;
	char *user_name=0, *user_pass=0, *auth;
	AUTHORITY authority;
	USER_INFO user;//, *account = sys_info->config.acounts;
	char* groupnum[] = {"admin", "user", "guest"};
	char buf[512];
	size_t size = 0;
	//req->is_cgi = CGI;
	SQUASH_KA(req);
	auth = "1";

	uri_decoding(req, req->query_string );
	//dbg("req->cmd_arg[0].name=%s\n", req->cmd_arg[0].name);
	//dbg("req->cmd_arg[0].value=%s\n", req->cmd_arg[0].value);
	if(req->cmd_count <= 1 || req->cmd_count >= 4)
		return 1;
 	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "name") == 0)
			user_name = req->cmd_arg[i].value;
		else if(strcmp(req->cmd_arg[i].name, "password") == 0)
			user_pass = req->cmd_arg[i].value;
		else if(strcmp(req->cmd_arg[i].name, "group") == 0)
			auth = req->cmd_arg[i].value;
		//dbg("#######\n");
	}	
	//dbg("user_name=%s\n", user_name);
	
	do {
		name_len = strlen(user_name);
		if ((name_len > MAX_USER_SIZE) || (name_len < MIN_USER_SIZE))
			break;
		
		pass_len = strlen(user_pass);
		if (pass_len > MAX_PASS_SIZE)
			break;
		
		authority = atoi(auth);
		if ((authority > AUTHORITY_VIEWER) || (authority < AUTHORITY_ADMIN))
			break;

		if (strcasecmp(user_name, req->connect->user_name) == 0) {
			//sysinfo_write(user_pass, offsetof(SysInfo, config.acounts[req->connect->user_idx].passwd), pass_len+1, 0);
			info_write(user_pass, ACOUNTS0_PASSWD+req->connect->user_idx*3, pass_len, 0);
			//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);
			sys_info->user_auth_mode = 1;
			// FIX_ME : SFIELD_ADD_USER
			ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user);
			size += sprintf(buf+size, "user=%s\r\n", user_name);
			return 1;
		}
		else if (req->authority != AUTHORITY_ADMIN) {
			req->buffer_end += sprintf(req_bufptr(req), "You must have administrator authority!\r\n");
			dbg("You must have administrator authority!\n");
			return 1;
		}

		for (i=0; i<ACOUNT_NUM; i++) {
			//if (blank_idx<0 && !account[i].name[0])
			if (blank_idx<0 && (strcmp(conf_string_read( ACOUNTS0_NAME+i*3), "") == 0))
				blank_idx = i;
			//if (strcasecmp(user_name, account[i].name) == 0) {
			if (strcasecmp(user_name, conf_string_read( ACOUNTS0_NAME+i*3)) == 0) {
				req->buffer_end += sprintf(req_bufptr(req), "The specific user was existed!\r\n");
				dbg("The specific user was existed!\n");
			}
		}
		if (blank_idx > 0) {
			memset(&user, 0, sizeof(USER_INFO));
			strcpy(user.name, user_name);
			strcpy(user.passwd, user_pass);
			user.authority = authority;
			info_write(user_name, ACOUNTS0_NAME+blank_idx*3, strlen(user_name), 0);
			info_write(user_pass, ACOUNTS0_PASSWD+blank_idx*3, strlen(user_pass), 0);
			info_write(&authority, ACOUNTS0_AUTHORITY+blank_idx*3, 0, 0);
			//sysinfo_write(&user, offsetof(SysInfo, config.acounts[blank_idx]), sizeof(USER_INFO), 0);
//			ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user)
		}
		else {
			req->buffer_end += sprintf(req_bufptr(req), "The user list was full!\r\n");
			dbg("The user list was full!\n");
			break;
		}

		//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);
		sys_info->user_auth_mode = 1;

		ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user);
		
		size += sprintf(buf+size, "user=%s\r\n", user_name);
		size += sprintf(buf+size, "password=********\r\n");		
		size += sprintf(buf+size, "group=%s\r\n", groupnum[authority]);
		send_request_dlink_ok(req, size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		req->status = DONE;
  	return 1;
	} while (0);
	return 1;
}
int uri_user_list(request * req)
{
	char buf[512];
	size_t size = 0;
	int i = 0;
	int user_cnt = 0;
	uri_decoding(req, req->query_string);
	
	for(i=0;i<ACOUNT_NUM;i++){
				//dbg("sys_info->config.acounts[%d].name = %s\n", i, sys_info->config.acounts[i].name);
				//if(!strcmp(sys_info->config.acounts[i].name, "") == 0){
				if(!strcmp(conf_string_read(ACOUNTS0_NAME+3*i), "") == 0){
					user_cnt ++;
				}/*else{
					size += sprintf(buf+size, "%s not exist!\r\n", req->cmd_arg[0].name);
				}*/
	}
	
	if(req->cmd_count != 0){
		if(strcmp(req->cmd_arg[0].name, "name") == 0){
			for(i = 0 ; i < user_cnt ; i++){
				//if(strcmp(sys_info->config.acounts[i].name, req->cmd_arg[0].value) == 0){
				if(strcmp(conf_string_read(ACOUNTS0_NAME+3*i), req->cmd_arg[0].value) == 0){
					size += sprintf(buf+size, "password=********\r\n");		
					//size += sprintf(buf+size, "group=%d\r\n", sys_info->config.acounts[i].authority);
					size += sprintf(buf+size, "group=%d\r\n", sys_info->ipcam[ACOUNTS0_AUTHORITY+3*i].config.u8);
					break;
				}
			}
		}
	}else{
		size += sprintf(buf+size, "users=%d\r\n", user_cnt);
		for(i=0 ; i <= user_cnt ; i++){
			//if(!strcmp(sys_info->config.acounts[i].name, "") == 0)
			if(!strcmp(conf_string_read(ACOUNTS0_NAME+3*i), "") == 0)
				//size += sprintf(buf+size, "%s=%d\r\n", sys_info->config.acounts[i].name, sys_info->config.acounts[i].authority);
				size += sprintf(buf+size, "%s=%d\r\n", conf_string_read(ACOUNTS0_NAME+3*i), sys_info->ipcam[ACOUNTS0_AUTHORITY+3*i].config.u8);
		}
	}
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
	
	return 1;
}
int uri_user_del(request * req)
{
	char buf[512];
	size_t size = 0;
	int i;
	int user_cnt = -1;
	//USER_INFO *account = sys_info->config.acounts;
	
	uri_decoding(req, req->query_string);
	
	if (req->authority == AUTHORITY_ADMIN) {
		for (i=0; i<ACOUNT_NUM; i++) {
			//if(!strcmp(sys_info->config.acounts[i].name, "") == 0){
			if(!strcmp(conf_string_read(ACOUNTS0_NAME+3*i), "") == 0){
				user_cnt ++;
				dbg("user_cnt = %d\n", user_cnt);
			}
		}
		for (i=0; i<=user_cnt; i++) {	
			//if (strcasecmp(req->cmd_arg[0].value, account[i].name) == 0) {
			if (strcasecmp(req->cmd_arg[0].value, conf_string_read( ACOUNTS0_NAME+3*i)) == 0) {
				dbg("match <%d>\n", __LINE__);
				USER_INFO user;
				if (i == 0) {
					DBG("You can not delete the administrator account!\n");
					break;;
				}
#ifdef CONFIG_BRAND_DLINK
				else if(i == 1){
					DBG("You can not delete the guest accunt!\n");
					break;
				}
#endif
				memset(&user, 0, sizeof(USER_INFO));
				user.authority = AUTHORITY_NONE;
				//sysinfo_write(&user, offsetof(SysInfo, config.acounts[i]), sizeof(USER_INFO), 0);
				info_write(user.name, ACOUNTS0_NAME+i*3, strlen(user.name), 0);
				info_write(user.passwd, ACOUNTS0_PASSWD+i*3, strlen(user.passwd), 0);
				info_write(&user.authority, ACOUNTS0_AUTHORITY+i*3, 0, 0);
				revoke_connection(req->cmd_arg[0].value);
				ControlSystemData(SFIELD_DEL_USER, (void *)req->cmd_arg[0].value, strlen(req->cmd_arg[0].value)+1, &req->curr_user);
				dbg("del ok<%d>\n", __LINE__);
				size += sprintf(buf+size, "username=%s\r\n", req->cmd_arg[0].value);
				//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);
				sys_info->user_auth_mode = 1;
				send_request_dlink_ok(req, size);
				req->buffer_end += sprintf((void *)req_bufptr(req), buf);
				req->status = DONE;
				return 1;
			}else
				dbg("cont <%d>\n", __LINE__);
			
		}
	}
	else
		DBG("You must have administrator authority!\n");
		
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_stream_info(request * req)
{
	char buf[1024];
	size_t size = 0;
	int i = 0, idx = 0;
	
	SQUASH_KA(req);

#if defined( PLAT_DM355 )
	size += sprintf(buf+size, "videos=%s,%s\r\n", MPEG4_PARSER_NAME, JPEG_PARSER_NAME);
#else
//	size += sprintf(buf+size, "videos=%s,%s,%s\r\n", MPEG4_PARSER_NAME, JPEG_PARSER_NAME, H264_PARSER_NAME);
	size += sprintf(buf+size, "videos=MJPEG,%s,%s\r\n", MPEG4_PARSER_NAME, H264_PARSER_NAME);

#endif
	size += sprintf(buf+size, "audios=%s\r\n", conf_string_read(AUDIO_INFO_CODEC_NAME));

	size += sprintf(buf+size, "resolutions=");
	for(i=0;i<MAX_RESOLUTIONS;i++){
		if(strlen(sys_info->api_string.resolution_list[i]))
			size += sprintf(buf+size, "%s", sys_info->api_string.resolution_list[i]);
		if(strlen(sys_info->api_string.resolution_list[++i])){
			i--;
			size += sprintf(buf+size, ",");
		}else{
			size += sprintf(buf+size, "\r\n");
			break;
		}
	}
	
#if 0
#if defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_IMX036_2M)
	size += sprintf(buf+size, "resolutions=%s,%s,%s,%s,%s\r\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5);
#elif defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_IMX036_3M)

	size += sprintf(buf+size, "resolutions=");
	for(i=0;i<10;i++){
		if(strlen(sys_info->api_string.resolution_list[i]))
			size += sprintf(buf+size, "%s", sys_info->api_string.resolution_list[i]);
		if(strlen(sys_info->api_string.resolution_list[++i])){
			i--;
			size += sprintf(buf+size, ",");
		}else
			break;
	}
	size += sprintf(buf+size, "\r\n");
	/*if(sys_info->video_type == 0) //NTSC
		size += sprintf(buf+size, "resolutions=%s,%s,%s,%s,%s,%s\r\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
	else
		size += sprintf(buf+size, "resolutions=%s,%s,%s,%s,%s,%s\r\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_7, RESOLUTION_8, RESOLUTION_9);*/
#elif defined ( SENSOR_USE_IMX035 )
	#if defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
		if(sys_info->video_type == 0) //NTSC
			size += sprintf(buf+size, "resolutions=%s,%s,%s,%s\r\n", RESOLUTION_7, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
		else
			size += sprintf(buf+size, "resolutions=%s,%s,%s,%s\r\n", RESOLUTION_7, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
	#else
		size += sprintf(buf+size, "resolutions=%s,%s,%s,%s,%s\r\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5);

	#endif

#elif defined(SENSOR_USE_MT9V136)
	size += sprintf(buf+size, "resolutions=%s,%s,%s\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);	

#elif defined(SENSOR_USE_TVP5150) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined(SENSOR_USE_TVP5150_MDIN270)
	if(sys_info->video_type == 0) //NTSC
		size += sprintf(buf+size, "resolutions=%s,%s,%s\r\n", RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
	else
		size += sprintf(buf+size, "resolutions=%s,%s,%s\r\n", RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);

#else
	dbg("UNKNOWN SENSOR");
#endif
#endif
	/*vbitrates*/
	size += sprintf(buf+size, "vbitrates=");
#if defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 ) //|| defined ( SENSOR_USE_MT9V136)
	idx = 2;
#else
	idx = 0;
#endif
	for(i=idx;i<MAX_MP4_BITRATE;i++){
		size += sprintf(buf+size, "%s", sys_info->api_string.mp4_bitrate[i]);
			
		if(i < MAX_MP4_BITRATE-1)
			size += sprintf(buf+size, ",");
	}
	size += sprintf(buf+size, "\r\n");
	
	size += sprintf(buf+size, "goplengths=10\r\n");
	size += sprintf(buf+size, "framerates=%s,%s,%s,%s,%s\r\n", (( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1), FRAMERATE_2, FRAMERATE_3 ,FRAMERATE_4 ,FRAMERATE_5);

	size += sprintf(buf+size, "qualities=Excellent,Good,Standard\r\n");

	size += sprintf(buf+size, "asamplerates=8\r\n");
	size += sprintf(buf+size, "abitrates=32\r\n");
	size += sprintf(buf+size, "micvol=0...%d\r\n", MAX_MICGAIN-1);
	size += sprintf(buf+size, "speakervol=1...10\r\n");
#ifdef SUPPORT_PROFILE_NUMBER
	size += sprintf(buf+size, "vprofilenum=%d\r\n", sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
#else
	size += sprintf(buf+size, "vprofilenum=%d\r\n", MAX_WEB_PROFILE_NUM);
#endif


#ifdef SUPPORT_PROFILE_NUMBER
	for(i=0;i<sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value;i++){
#else
	for(i=0;i<MAX_WEB_PROFILE_NUM;i++){
#endif
		if(strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*i), JPEG_PARSER_NAME) == 0)
			size += sprintf(buf+size, "vprofile%d=MJPEG\r\n", i+1);
		else
			size += sprintf(buf+size, "vprofile%d=%s\r\n", i+1, conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*i));

	//for(i=0;i<MAX_WEB_PROFILE_NUM;i++)
		if(strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*i), MPEG4_PARSER_NAME) == 0)
			size += sprintf(buf+size, "vprofileurl%d=/video/ACVS.cgi?profileid=%d\r\n", i+1, i+1);
		else if(strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*i), JPEG_PARSER_NAME) == 0)
			size += sprintf(buf+size, "vprofileurl%d=/video/mjpg.cgi?profileid=%d\r\n", i+1, i+1);
		else
			size += sprintf(buf+size, "vprofileurl%d=/video/ACVS-H264.cgi?profileid=%d\r\n", i+1, i+1);
		size += sprintf(buf+size, "vprofileres%d=%s\r\n", i+1,conf_string_read(PROFILE_CONFIG0_RESOLUTION+PROFILE_STRUCT_SIZE*i));
		
	}
	size += sprintf(buf+size, "aprofilenum=1\r\n");
	size += sprintf(buf+size, "aprofile11=%s\r\n", conf_string_read(AUDIO_INFO_CODEC_NAME));
	size += sprintf(buf+size, "aprofileurl1=/audio/ACAS.cgi\r\n");
		
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int select_decoding(char* name[], char* str, int len)
{
	
	dbg("str = %s\n", str);

	int i = 0, ret = 0;
	
	for(i = 0 ; i < len ; i++){
		//dbg("name = %s\n", name[i]);
		
		if(strcmp(name[i], str) == 0){
			 dbg("i = %d\n", i);
			 ret = i;
			 break;
		}
		
	}
	//"0" reserve
	return (ret+1);
}

int uri_sensor_reset(request * req)
{
	char buf[256];
	size_t size = 0;
	__u8 value = 1 ;
	
	uri_decoding(req, req->query_string);
    if(req->cmd_count <= 0){
    	size += sprintf(buf+size,"reset=fail\r\n");
    	send_request_dlink_ok(req, size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		req->status = DONE;
		return 1;
    }
    SQUASH_KA(req);
   
	if(strcmp(req->cmd_arg[0].name, "reset") == 0){
		if(strcmp(req->cmd_arg[0].value, "go") == 0){
			ControlSystemData(SFIELD_SET_IMAGEDEFAULT, (void *)&value, sizeof(value), &req->curr_user);
			size += sprintf(buf+size,"reset=yes\r\n");
		}else
			size += sprintf(buf+size,"reset=fail\r\n");
	}
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_icr(request * req)
{
#ifdef SUPPORT_DNCONTROL
	char buf[256];
	size_t size = 0;
	int i, value;
	int start_tmp1 = 0, end_tmp1 = 0, start_tmp2 = 0, end_tmp2 = 0;
	int starttime, endtime;
	int schedule_hour, schedule_min;
	
	uri_decoding(req, req->query_string);
	
	//value = sys_info->config.lan_config.dnc_mode;
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "mode") == 0){
			if(strcmp(req->cmd_arg[i].value, "on") == 0){		
				value = DNC_MODE_DAY;
				sys_info->cmd.dn_control |= DNC_CMD_MODE;
				dbg("day_mode\n");
			}	
			else if(strcmp(req->cmd_arg[i].value, "off") == 0){		
				value = DNC_MODE_NIGHT;
				sys_info->cmd.dn_control |= DNC_CMD_MODE;
				dbg("night_mode\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "auto") == 0){		
				value = DNC_MODE_AUTO;
				sys_info->cmd.dn_control |= DNC_CMD_MODE;
				dbg("auto\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "schedule") == 0){		
				value = DNC_MODE_SCHEDULE;
				sys_info->cmd.dn_control |= DNC_CMD_SCHEDULE;
				dbg("schedule\n");
			}
			else 
				break;
			
			if(sys_info->ipcam[DNC_MODE].config.u8 != value){
				dbg("change mode\n");
				info_write(&value, DNC_MODE, sizeof(value), 0);
			}
		}
		if((strcmp(req->cmd_arg[i].name, "starttime") == 0) && (value == DNC_MODE_SCHEDULE)){	
				sscanf(req->cmd_arg[i].value, "%02d:%02d", &start_tmp1, &start_tmp2);
				if(start_tmp1 > 23 || start_tmp2 > 59)
					break;
				starttime = start_tmp1 *10000 + start_tmp2*100;
				dbg("%d-%d-%d\n", start_tmp1, start_tmp2, starttime);
				info_write(&starttime, DNC_SCHEDULE_START, sizeof(starttime), 0);	
		}
		if((strcmp(req->cmd_arg[i].name, "endtime") == 0) && (value == DNC_MODE_SCHEDULE)){
				sscanf(req->cmd_arg[i].value, "%02d:%02d", &end_tmp1, &end_tmp2);
				if(end_tmp1 > 23 || end_tmp2 > 59)
					break;
				endtime = end_tmp1 *10000 + end_tmp2*100;
				info_write(&endtime, DNC_SCHEDULE_END, sizeof(endtime), 0);
		}
		
	}	

	switch(sys_info->ipcam[DNC_MODE].config.u8){
		case DNC_MODE_DAY:
			size += sprintf(buf+size,"mode=on\r\n" );
			break;
		case DNC_MODE_NIGHT:
			size += sprintf(buf+size,"mode=off\r\n" );
			break;
		case DNC_MODE_AUTO:
			size += sprintf(buf+size,"mode=auto\r\n" );
			break;
		case DNC_MODE_SCHEDULE:
			size += sprintf(buf+size,"mode=schedule\r\n" );
			break;
	}


	//schedule_hour = sys_info->config.lan_config.dnc_schedule_start/10000;
	//schedule_min = (sys_info->config.lan_config.dnc_schedule_start - schedule_hour*10000)/100;
	if(value == DNC_MODE_SCHEDULE){
		schedule_hour = sys_info->ipcam[DNC_SCHEDULE_START].config.u32/10000;
		schedule_min = (sys_info->ipcam[DNC_SCHEDULE_START].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"starttime=%02d:%02d\r\n", schedule_hour, schedule_min);
		
		//schedule_hour = sys_info->config.lan_config.dnc_schedule_end/10000;
		//schedule_min = (sys_info->config.lan_config.dnc_schedule_end - schedule_hour*10000)/100;
		schedule_hour = sys_info->ipcam[DNC_SCHEDULE_END].config.u32/10000;
		schedule_min = (sys_info->ipcam[DNC_SCHEDULE_END].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"endtime=%02d:%02d\r\n", schedule_hour, schedule_min);
	}

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
#elif defined(MODEL_VS2415)
	char buf[256];
	size_t size = 0;
	int i, mode;
	int start_tmp1 = 0, end_tmp1 = 0, start_tmp2 = 0, end_tmp2 = 0;
	int schedule_hour, schedule_min;
	char sch_time[16]; 

	uri_decoding(req, req->query_string);

	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "mode") == 0){
			if(strcmp(req->cmd_arg[i].value, "on") == 0){		
				mode = DNC_MODE_DAY;
				viscaicr_day(mode);
				dbg("day_mode\n");
			}	
			else if(strcmp(req->cmd_arg[i].value, "off") == 0){		
				mode = DNC_MODE_NIGHT;
				viscaicr_night(mode);
				dbg("night_mode\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "auto") == 0){		
				mode = DNC_MODE_AUTO;
				viscaicr_auto(mode);
				dbg("auto\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "schedule") == 0){		
				mode = DNC_MODE_SCHEDULE;
				dbg("schedule\n");
			}
			else 
				break;
			
			if(sys_info->ipcam[DNC_MODE].config.u8 != mode){
				dbg("change mode\n");
				info_write(&mode, DNC_MODE, sizeof(mode), 0);
			}
		}
		
		if(mode == DNC_MODE_SCHEDULE){
			if((strcmp(req->cmd_arg[i].name, "starttime") == 0)){	
					sscanf(req->cmd_arg[i].value, "%02d:%02d", &start_tmp1, &start_tmp2);
					if(start_tmp1 > 23 || start_tmp2 > 59)
						break;
			}
			if((strcmp(req->cmd_arg[i].name, "endtime") == 0)){
					sscanf(req->cmd_arg[i].value, "%02d:%02d", &end_tmp1, &end_tmp2);
					if(end_tmp1 > 23 || end_tmp2 > 59)
						break;
			}
			
			sprintf(sch_time, "%02d%02d00%02d%02d00", start_tmp1, start_tmp2, end_tmp1, end_tmp2);
			dbg("sch_time = %s", sch_time);
			viscaicr_schedule(mode, sch_time);
		}
	}	

	dbg("sys_info->ipcam[DNC_MODE].config.u8 = %d", sys_info->ipcam[DNC_MODE].config.u8);
	switch(sys_info->ipcam[DNC_MODE].config.u8){
		case DNC_MODE_DAY:
			size += sprintf(buf+size,"mode=on\r\n" );
			break;
		case DNC_MODE_NIGHT:
			size += sprintf(buf+size,"mode=off\r\n" );
			break;
		case DNC_MODE_AUTO:
			size += sprintf(buf+size,"mode=auto\r\n" );
			break;
		case DNC_MODE_SCHEDULE:
			size += sprintf(buf+size,"mode=schedule\r\n" );
			break;
	}


	if(mode == DNC_MODE_SCHEDULE){
		schedule_hour = sys_info->ipcam[DNC_SCHEDULE_START].config.u32/10000;
		schedule_min = (sys_info->ipcam[DNC_SCHEDULE_START].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"starttime=%02d:%02d\r\n", schedule_hour, schedule_min);

		schedule_hour = sys_info->ipcam[DNC_SCHEDULE_END].config.u32/10000;
		schedule_min = (sys_info->ipcam[DNC_SCHEDULE_END].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"endtime=%02d:%02d\r\n", schedule_hour, schedule_min);
	}

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
#else
	return -1;
#endif
}  
int uri_irled(request * req)
{
#if SUPPORT_DCPOWER
	char buf[256];
	size_t size = 0;
	int i, value;
	int start_tmp1 = 0, end_tmp1 = 0, start_tmp2 = 0, end_tmp2 = 0;
	int starttime, endtime;
	int schedule_hour, schedule_min;
	
	uri_decoding(req, req->query_string);
	
	//value = sys_info->config.lan_config.powerout;
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "mode") == 0){
			if(strcmp(req->cmd_arg[i].value, "on") == 0){		
				value = DC_POWER_ON;
				dbg("day_mode\n");
			}	
			else if(strcmp(req->cmd_arg[i].value, "off") == 0){		
				value = DC_POWER_OFF;
				dbg("night_mode\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "auto") == 0){		
				value = DC_POWER_SYNC;
				dbg("auto\n");
			}
			else if(strcmp(req->cmd_arg[i].value, "schedule") == 0){		
				value = DC_POWER_SCHEDULE;
				dbg("schedule\n");
			}
			else
				break;
	
		}
		if((strcmp(req->cmd_arg[i].name, "starttime") == 0) && (value == DC_POWER_SCHEDULE)){	
				sscanf(req->cmd_arg[i].value, "%02d:%02d", &start_tmp1, &start_tmp2);
				if(start_tmp1 > 23 || start_tmp2 > 59)
					break;
				starttime = start_tmp1 *10000 + start_tmp2*100;
				dbg("%d-%d-%d\n", start_tmp1, start_tmp2, starttime);
				info_write(&starttime, POWERSCHEDULE_START, sizeof(starttime), 0);	
		}
		if((strcmp(req->cmd_arg[i].name, "endtime") == 0) && (value == DC_POWER_SCHEDULE)){
				sscanf(req->cmd_arg[i].value, "%02d:%02d", &end_tmp1, &end_tmp2);
				if(end_tmp1 > 23 || end_tmp1 > 59)
					break;
				endtime = end_tmp1 *10000 + end_tmp2*100;
				info_write(&endtime, POWERSCHEDULE_END, sizeof(endtime), 0);
		}

		if(sys_info->ipcam[POWEROUT].config.u8 != value){
			dbg("change mode\n");
			info_write(&value, POWEROUT, sizeof(value), 0);
		}
	}	
	
	

	switch(sys_info->ipcam[POWEROUT].config.u8){
		case DC_POWER_ON:
			size += sprintf(buf+size,"mode=on\r\n" );
			break;
		case DC_POWER_OFF:
			size += sprintf(buf+size,"mode=off\r\n" );
			break;
		case DC_POWER_SYNC:
			size += sprintf(buf+size,"mode=auto\r\n" );
			break;
		case DC_POWER_SCHEDULE:
			size += sprintf(buf+size,"mode=schedule\r\n" );
			break;
	}
	
	if(value == DC_POWER_SCHEDULE){

		schedule_hour = sys_info->ipcam[POWERSCHEDULE_START].config.u32/10000;
		schedule_min = (sys_info->ipcam[POWERSCHEDULE_START].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"starttime=%02d:%02d\r\n", schedule_hour, schedule_min);

		schedule_hour = sys_info->ipcam[POWERSCHEDULE_END].config.u32/10000;
		schedule_min = (sys_info->ipcam[POWERSCHEDULE_END].config.u32 - schedule_hour*10000)/100;
		size += sprintf(buf+size,"endtime=%02d:%02d\r\n", schedule_hour, schedule_min);
	}
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
#else
		
	return -1;
#endif

}  
int uri_network(request * req)
{
	char buf[512];
	size_t size = 0;
	int i, value = 0;
	//char *name[] = {"dhcp", "ip", "netmask", "schedule"};
	struct in_addr ipaddr, sys_ip, ip, dns;
	struct in_addr netmask;
	struct in_addr gateway;
	
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
  {
  		//__u8 value = atoi(req->cmd_arg[i].value);
  		if(strcmp(req->cmd_arg[i].name, "dhcp") == 0){
  			if(strcmp(req->cmd_arg[i].value , "on") == 0)	value = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0)	value = 0;
  			else break;

  			if (sys_info->pppoe_active == 1) {	
				dbg("pppoe already action\n");
				break;
			}

			#ifndef DHCP_CONFIG
				dbg("dhcp_config value is zero\n");
				break;
			#endif

			//dbg("value = %d\n", value);
			if(sys_info->ipcam[NET_DHCP_ENABLE].config.u8 != value){
				if(ControlSystemData(SFIELD_SET_DHCPC_ENABLE, (void *)&value, sizeof(value), &req->curr_user) < 0)
					break;
			}
  		}
  		if(strcmp(req->cmd_arg[i].name, "ip") == 0){

  			if(sys_info->ipcam[NET_DHCP_ENABLE].config.u8 == 1){
  				dbg("DHCP already used!\n");
  				break;
  			}

  			if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1){
				dbg("PPPoe already used!\n");
				break;
			}
			if (ipv4_str_to_num(req->cmd_arg[i].value, &ipaddr) == 0){
				dbg("ipv4_str_to_num error!");
				break;
			}
			if ( (sys_ip.s_addr = net_get_ifaddr(sys_info->ifether)) == -1){
				dbg("net_get_ifaddr error!");
				break;
			}

			if (sys_ip.s_addr != ipaddr.s_addr) {
				if (net_set_ifaddr(sys_info->ifether, ipaddr.s_addr) < 0){
					dbg("net_set_ifaddr error");
					break;
				}
				ControlSystemData(SFIELD_SET_IP, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
			}
			
  		}	
  		if(strcmp(req->cmd_arg[i].name, "netmask") == 0){
  			if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
				break;
		
			if (ipv4_str_to_num(req->cmd_arg[i].value, &ipaddr) == 0)
				break;

			if ( (sys_ip.s_addr = net_get_netmask(sys_info->ifether)) == -1)  
				break;

			if (sys_ip.s_addr != ipaddr.s_addr) {
				if (net_set_netmask(sys_info->ifether, ipaddr.s_addr) < 0) 
					break;

				ControlSystemData(SFIELD_SET_NETMASK, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
			}
  		}
  		if(strcmp(req->cmd_arg[i].name, "gateway") == 0){

  			if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
				break;
		
			if (ipv4_str_to_num(req->cmd_arg[i].value, &ipaddr) == 0)
				break;

			sys_ip.s_addr = net_get_gateway(sys_info->ifether);

			if (sys_ip.s_addr != ipaddr.s_addr) {
				net_set_gateway(ipaddr.s_addr);
				ControlSystemData(SFIELD_SET_GATEWAY, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
			}
  		} 		
  		if(strcmp(req->cmd_arg[i].name, "dns1") == 0){
			if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
				break;
		
			if (ipv4_str_to_num(req->cmd_arg[i].value, &ipaddr) == 0)
				break;
		
			sys_ip.s_addr = net_get_dns();

			if (sys_ip.s_addr != ipaddr.s_addr) {
				if (net_set_dns(inet_ntoa(ipaddr)) < 0)
					break;

				ControlSystemData(SFIELD_SET_DNS, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
			}
  		}	
  		if(strcmp(req->cmd_arg[i].name, "dns2") == 0){
  			dbg("no ddns2 setting\n");
  		}
  		if(strcmp(req->cmd_arg[i].name, "pppoe") == 0){
  			__u8 pppoe_enable;
  			if(strcmp(req->cmd_arg[i].value , "on") == 0)	pppoe_enable = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0)	pppoe_enable = 0;
  			else break;
  			dbg("pppoe enable set value = %d\n", pppoe_enable);

  			if((sys_info->ipcam[PPPOEENABLE].config.u8 != pppoe_enable) ){
				ControlSystemData(SFIELD_SET_PPPOE_ENABLE, (void *)&pppoe_enable, sizeof(pppoe_enable), &req->curr_user);
				if ( pppoe_enable == 0 )
					ControlSystemData(SFIELD_SET_PPPOE_ENABLE, (void *)&pppoe_enable, sizeof(pppoe_enable), &req->curr_user);
			}
  		}	
  		if(strcmp(req->cmd_arg[i].name, "pppoeuser") == 0){
  			if ( (strcmp(conf_string_read(PPPOEACCOUNT), req->cmd_arg[i].value)) || (sys_info->pppoe_active == 0)) 
				ControlSystemData(SFIELD_SET_PPPOE_ACCOUNT, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
  		}
  		if(strcmp(req->cmd_arg[i].name, "pppoepass") == 0){
  			if ( (strcmp(conf_string_read(PPPOEPASSWORD), req->cmd_arg[i].value)) || (sys_info->pppoe_active == 0) )
				ControlSystemData(SFIELD_SET_PPPOE_PASSWORD, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
  		}
  		if(strcmp(req->cmd_arg[i].name, "ddns") == 0){
  			__u8 ddns_enable;
  			if(strcmp(req->cmd_arg[i].value , "on") == 0)	ddns_enable = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0)	ddns_enable = 0;
  			else break;
  			dbg("ddns enable set value = %d\n", ddns_enable);

  			if(sys_info->ipcam[DDNS_ENABLE].config.u8 != ddns_enable)
				ControlSystemData(SFIELD_SET_DDNS_ENABLE, (void *)&ddns_enable, sizeof(ddns_enable), &req->curr_user);
		}
  		if(strcmp(req->cmd_arg[i].name, "ddnsprovider") == 0){
  			__u8 ddnsprovider;
  			ddnsprovider = atoi(req->cmd_arg[i].value);
  			if(ddnsprovider > 0) 
  				ddnsprovider -- ;
  			else
  				break;
  			dbg("ddnsprovider set value = %d\n", ddnsprovider);

  			if(sys_info->ipcam[DDNS_TYPE].config.u8 != ddnsprovider && value >= 0 && value <= 1)
				ControlSystemData(SFIELD_SET_DDNS_TYPE, (void *)&ddnsprovider, sizeof(ddnsprovider), &req->curr_user);
		}
  		if(strcmp(req->cmd_arg[i].name, "ddnshost") == 0){
  			//if (strcmp(sys_value, req->cmd_arg[i].value)) 
				ControlSystemData(SFIELD_SET_DDNS_HOST, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
  		if(strcmp(req->cmd_arg[i].name, "ddnsuser") == 0){
  			//if (strcmp(sys_value, req->cmd_arg[i].value)) 
				ControlSystemData(SFIELD_SET_DDNS_ACCOUNT, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
			
		}
  		if(strcmp(req->cmd_arg[i].name, "ddnspass") == 0){
  			//if (strcmp(sys_value, req->cmd_arg[i].value)) 
				ControlSystemData(SFIELD_SET_DDNS_PASSWORD, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
			
  		}
  		if(strcmp(req->cmd_arg[i].name, "upnp") == 0){
  			__u8 upnp_enable;
  			if(strcmp(req->cmd_arg[i].value , "on") == 0) upnp_enable = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0) upnp_enable = 0;
  			else break;
  			dbg("upnp enable set value = %d\n", upnp_enable);

  			if(sys_info->ipcam[UPNPENABLE].config.u8 != upnp_enable)
				ControlSystemData(SFIELD_SET_UPNP_ENABLE, (void *)&upnp_enable, sizeof(upnp_enable), &req->curr_user);
		}
  		if(strcmp(req->cmd_arg[i].name, "httpport") == 0){
  			unsigned short httpport_value;
  			httpport_value = atoi(req->cmd_arg[i].value);
  			if(httpport_value < MIN_HTTP_PORT && httpport_value != HTTP_PORT_DEFAULT)
				break;

			if (sys_info->ipcam[HTTP_PORT].config.u16 != httpport_value) 
				ControlSystemData(SFIELD_SET_HTTPPORT, (void *)&httpport_value, sizeof(httpport_value), &req->curr_user);
			
  		}
  		//if(strcmp(req->cmd_arg[i].name, "httpexternalport") == 0)
  		if(strcmp(req->cmd_arg[i].name, "rtspport") == 0){
  			__u8 rtspport_value;
  			rtspport_value = atoi(req->cmd_arg[i].value);

  			if(sys_info->ipcam[RTSP_PORT].config.u16 != rtspport_value)
				//sysinfo_write(&rtspport_value, offsetof(SysInfo, config.lan_config.net.rtsp_port), sizeof(rtspport_value), 0);
				info_write(&rtspport_value, RTSP_PORT, sizeof(rtspport_value), 0);
		  
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
  		}
  		//if(strcmp(req->cmd_arg[i].name, "rtspexternalport") == 0)
  }
 	SQUASH_KA(req);

	size += sprintf(buf+size, "dhcp=%s\r\n", sys_info->ipcam[NET_DHCP_ENABLE].config.u8 ? "on" : "off" );
	ip.s_addr = net_get_ifaddr(sys_info->ifname);
	size += sprintf(buf+size, "ip=%s\r\n", (char*)inet_ntoa(ip));
	netmask.s_addr = net_get_netmask(sys_info->ifname);
	size += sprintf(buf+size, "netmask=%s\r\n", (char*)inet_ntoa(netmask));
	gateway.s_addr = net_get_gateway(sys_info->ifname);	
	size += sprintf(buf+size, "gateway=%s\r\n", (char*)inet_ntoa(gateway));	
	dns.s_addr = net_get_dns();
	size += sprintf(buf+size, "dns1=%s\n", (char*)inet_ntoa(dns));
	size += sprintf(buf+size, "dns2=192.95.1.1\n");
	/*pppoe*/

	size += sprintf(buf+size,"pppoe=%s\r\n", sys_info->ipcam[PPPOEENABLE].config.u8 ? "on" : "off" );
	size += sprintf(buf+size,"pppoeuser=%s\r\n", conf_string_read(PPPOEACCOUNT));
	size += sprintf(buf+size,"pppoepass=********\r\n");
	/*ddns*/
	size += sprintf(buf+size,"ddns=%s\r\n", sys_info->ipcam[DDNS_ENABLE].config.u8 ?"on":"off");

	if(sys_info->ipcam[DDNS_TYPE].config.u8 == 0)
		size += sprintf(buf+size,"providers=www.dlink.com\r\n");
	else if(sys_info->ipcam[DDNS_TYPE].config.u8 == 1)
		size += sprintf(buf+size,"providers=www.DynDns.org\r\n");
	else
		size += sprintf(buf+size,"providers=select Dynamic DNS Server\r\n");
	/*ddns,upnp,http,rtst*/

	size += sprintf(buf+size,"ddnshost=%s\r\n", conf_string_read(DDNS_HOSTNAME));
	size += sprintf(buf+size,"ddnsuser=%s\r\n", conf_string_read(DDNS_ACCOUNT));
	size += sprintf(buf+size,"ddnspass=********\r\n");
	size += sprintf(buf+size,"upnp=%s\r\n", sys_info->ipcam[UPNPENABLE].config.u8 ? "on" : "off");
	size += sprintf(buf+size,"httpport=%d\r\n", sys_info->ipcam[HTTP_PORT].config.u16);
	//size += sprintf(buf+size,"httpexternalport=%s\r\n", sys_info->config.lan_config.net.upnpcextport);
	size += sprintf(buf+size,"rtspport=%d\r\n", sys_info->ipcam[RTSP_PORT].config.u16);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	
	return 1;
}
int uri_pppoe(request * req)
{
	char buf[512];
	size_t size = 0;
	int i;
	__u8 value;
	
	uri_decoding(req, req->query_string);

	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "pppoe") == 0){
		  	if(strcmp(req->cmd_arg[i].value , "on") == 0)	value = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0)	value = 0;
  			else break;
			
			//if((sys_info->config.lan_config.net.pppoeenable != value) )
			if((sys_info->ipcam[PPPOEENABLE].config.u8 != value) )
				ControlSystemData(SFIELD_SET_PPPOE_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "user") == 0){
			//if ( (strcmp(sys_info->config.lan_config.net.pppoeaccount, req->cmd_arg[i].value)) || (sys_info->config.lan_config.net. pppoeactive == 0))
			if ( (strcmp(conf_string_read(PPPOEACCOUNT), req->cmd_arg[i].value)) || (sys_info->pppoe_active == 0))
				ControlSystemData(SFIELD_SET_PPPOE_ACCOUNT, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "pass") == 0){
			//if ( (strcmp(sys_info->config.lan_config.net.pppoepassword, req->cmd_arg[i].value)) || (sys_info->config.lan_config.net. pppoeactive == 0) )
			if ( (strcmp(conf_string_read(PPPOEPASSWORD), req->cmd_arg[i].value)) || (sys_info->pppoe_active == 0) )
				ControlSystemData(SFIELD_SET_PPPOE_PASSWORD, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
	}	

	//size += sprintf(buf+size,"pppoe=%s\r\n", sys_info->config.lan_config.net.pppoeenable? "on" : "off" );
	size += sprintf(buf+size,"pppoe=%s\r\n", sys_info->ipcam[PPPOEENABLE].config.u8? "on" : "off" );
	//size += sprintf(buf+size,"user=%s\r\n", sys_info->config.lan_config.net.pppoeaccount);
	size += sprintf(buf+size,"user=%s\r\n", conf_string_read(PPPOEACCOUNT));
	size += sprintf(buf+size,"pass=********\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_ddnsproviders(request * req)
{
	char buf[512];
	size_t size = 0;
	
	size += sprintf(buf+size,"num=2\r\n");
	size += sprintf(buf+size,"providers=www.dlink.com, www.DynDns.org\r\n" );
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_ddns(request * req)
{
	char buf[512];
	size_t size = 0;
	int i;
	__u8 value = 0;

	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){	
		if(strcmp(req->cmd_arg[i].name, "ddns") == 0){
		  	if(strcmp(req->cmd_arg[i].value , "on") == 0)	value = 1;
  			else if(strcmp(req->cmd_arg[i].value , "off") == 0)	value = 0;
  			else break;
			//if(sys_info->config.ddns.enable != value)
			if(sys_info->ipcam[DDNS_ENABLE].config.u8 != value)
				ControlSystemData(SFIELD_SET_DDNS_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "provider") == 0){
			__u8 value = atoi(req->cmd_arg[i].value);
			if(value > 0) value--;
			else break;
			
			//if(sys_info->config.ddns.type != value && /*value >= 0 &&*/ value <= 1)
			if(sys_info->ipcam[DDNS_TYPE].config.u8 != value && /*value >= 0 &&*/ value <= 1)
				ControlSystemData(SFIELD_SET_DDNS_TYPE, (void *)&value, sizeof(value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "host") == 0){
			//if ( strcmp(sys_info->config.ddns.hostname, req->cmd_arg[i].value) != 0)
			if ( strcmp(conf_string_read(DDNS_HOSTNAME), req->cmd_arg[i].value) != 0)
				ControlSystemData(SFIELD_SET_DDNS_HOST, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "user") == 0){
			//if ( strcmp(sys_info->config.ddns.account, req->cmd_arg[i].value) != 0)
			if ( strcmp(conf_string_read(DDNS_ACCOUNT), req->cmd_arg[i].value) != 0)
				ControlSystemData(SFIELD_SET_DDNS_ACCOUNT, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "pass") == 0){
			//if ( strcmp(sys_info->config.ddns.password, req->cmd_arg[i].value) != 0 )
			if ( strcmp(conf_string_read(DDNS_PASSWORD), req->cmd_arg[i].value) != 0 )
				ControlSystemData(SFIELD_SET_DDNS_PASSWORD, (void *)req->cmd_arg[i].value, strlen(req->cmd_arg[i].value), &req->curr_user);
		}
	}
	
	//size += sprintf(buf+size,"ddns=%s\r\n", sys_info->config.ddns.enable?"on":"off");
	size += sprintf(buf+size,"ddns=%s\r\n", sys_info->ipcam[DDNS_ENABLE].config.u8 ?"on":"off");
	size += sprintf(buf+size,"provider=");
	//if(sys_info->config.ddns.type == 0)
	if(sys_info->ipcam[DDNS_TYPE].config.u8 == 0)
		size += sprintf(buf+size,"www.dlink.com\r\n");
	//else if(sys_info->config.ddns.type == 1)
	else if(sys_info->ipcam[DDNS_TYPE].config.u8 == 1)
		size += sprintf(buf+size,"www.DynDns.org\r\n");
	else
		size += sprintf(buf+size,"select Dynamic DNS Server\r\n");
	//size += sprintf(buf+size,"host=%s\r\n", sys_info->config.ddns.hostname);
	size += sprintf(buf+size,"host=%s\r\n", conf_string_read(DDNS_HOSTNAME));
	//size += sprintf(buf+size,"user=%s\r\n", sys_info->config.ddns.account);
	size += sprintf(buf+size,"user=%s\r\n", conf_string_read(DDNS_ACCOUNT));
	size += sprintf(buf+size,"pass=********\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_upnp(request * req)
{
	char buf[512];
	size_t size = 0;
	int i;
	__u8 value = 0;
	
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].value , "on") == 0)	value = 1;
  		else if(strcmp(req->cmd_arg[i].value , "off") == 0)	value = 0;
  		else break;
		
		if(strcmp(req->cmd_arg[i].name, "upnpav") == 0){
			//if(sys_info->config.lan_config.net.upnpenable != value)
			if(sys_info->ipcam[UPNPENABLE].config.u8 != value)
				ControlSystemData(SFIELD_SET_UPNP_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		}
		if(strcmp(req->cmd_arg[i].name, "upnpcp") == 0){
			//if(sys_info->config.lan_config.net.upnpcenable != value)
			if(sys_info->ipcam[UPNPCENABLE].config.u8 != value)
				ControlSystemData(SFIELD_SET_UPNP_CLIENT_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		}
	}
	
	//size += sprintf(buf+size,"upnpav=%s\r\n", sys_info->config.lan_config.net.upnpenable? "on" : "off");
	size += sprintf(buf+size,"upnpav=%s\r\n", sys_info->ipcam[UPNPENABLE].config.u8? "on" : "off");
	//size += sprintf(buf+size,"upnpcp=%s\r\n", sys_info->config.lan_config.net.upnpcenable? "on" : "off");
	size += sprintf(buf+size,"upnpcp=%s\r\n", sys_info->ipcam[UPNPCENABLE].config.u8? "on" : "off");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_httpport(request * req)
{
	char buf[512];
	size_t size = 0;
	int i;
	unsigned short value = 80;
	
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		value = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "httpport") == 0){
			if(value < MIN_HTTP_PORT && value != HTTP_PORT_DEFAULT)
				break;
			//if (sys_info->config.lan_config.net.http_port != value) {
			if (sys_info->ipcam[HTTP_PORT].config.u16 != value) {
				ControlSystemData(SFIELD_SET_HTTPPORT, (void *)&value, sizeof(value), &req->curr_user);
				req->req_flag.restart = 1;
			}
		}
	}
	
	SQUASH_KA(req);
	//size += sprintf(buf+size,"httpport=%d\r\n", sys_info->config.lan_config.net.http_port);
	size += sprintf(buf+size,"httpport=%d\r\n", sys_info->ipcam[HTTP_PORT].config.u16);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}
int uri_dlink_jpeg(request * req)
{
	req->http_stream = URI_STREAM_JPEG;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_mjpeg_data(&req->video, AV_OP_SUB_STREAM_1,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;	
	
	send_request_ok_jpeg(req);
	return 1;
}

int get_dlink_mjpeg_stream(request * req, int idx)
{
	if(idx < 0 || idx > 2) {
		send_r_not_found(req);
		return 0;
	}
	
	req->http_stream = URI_STREAM_MJPEG + idx;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_mjpeg_data(&req->video, idx,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;	

 	req->req_flag.audio_on = 0;

	send_request_dlink_ok_mjpg(req);
	return 1;
}
int get_dlink_mpeg4_stream(request * req, int idx)
{
	AV_DATA vol_data;

	send_request_dlink_ok_mpeg4(req);
	
	if(idx < 0 || idx > 2) {
		send_r_not_found(req);
		return 0;
	}
	dbg("idx = %d\n", idx);
	req->http_stream = URI_DLINK_STREAM_MPEG4 + idx;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
	
#ifdef CONFIG_STREAM_TIMEOUT
	req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
	if (!check_stream_count(req))
		return 0;
#endif

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if(GetAVData(idx|AV_FMT_MPEG4|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_MP4_VOL\n");
		return 0;
	}

	if (get_mpeg4_data(&req->video, idx,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = vol_data.size;
	print_dlink_mpeg4_headers(req);
	req_copy(req, vol_data.ptr, vol_data.size);

	return 1;
}
int get_dlink_h264_stream(request * req, int idx)
{
	//AV_DATA vol_data;
	send_request_dlink_ok_mpeg4(req);
	
	if(idx < 0 || idx > 2) {
		send_r_not_found(req);
		return 0;
	}

	req->http_stream = URI_DLINK_STREAM_H264 + idx;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
#ifdef CONFIG_STREAM_TIMEOUT
		req->last_stream_time = sys_info->second;
#endif
#ifdef CONFIG_STREAM_LIMIT
		if (!check_stream_count(req))
			return 0;
#endif

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
/*	if(GetAVData(idx|AV_FMT_H264|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_H264_VOL\n");
		return 0;
	}
*/
	if (get_h264_data(&req->video, idx,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
//	req->extra_size = vol_data.size;
	print_dlink_mpeg4_headers(req);
//	req_copy(req, vol_data.ptr, vol_data.size);

	return 1;
}


int uri_verify(request * req)
{
	char buf[256];
	size_t size = 0;	
	//req->is_cgi = CGI;

	SQUASH_KA(req);
	
	size += sprintf(buf+size, "login=1\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = WRITE;
  return 1;
}
int chunked_start(request * req, size_t size)
{
	
	char buf[]={"\r\n"};
	fprintf(stderr, "size = %d(%x) \n", size, size);
	if(size >= CHUNKED_MAXLEN)
			DBGERR("chunked buf not enough!");
	req->buffer_end += sprintf((void *)req_bufptr(req), "%x", size-2);		
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	return 0;
}

int end_line(request * req)
{
	
	char end_buf[]={"0\r\n\r\n"};
	req->buffer_end += sprintf((void *)req_bufptr(req), end_buf);
	return 0;
}

int uri_st_device(request * req)
{
		char buf[CHUNKED_MAXLEN];
		//char br[]={"\r\n"};
		size_t size = 0;	
		__u8 mac[MAC_LENGTH];
		struct in_addr ip;
		struct in_addr netmask;
		struct in_addr gateway;
		struct in_addr dns;
		//req->is_cgi = CGI;
		size += sprintf(buf+size, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
		size += sprintf(buf+size, "<?xml-stylesheet type=\"text/xsl\" href=\"st_device.xsl\"?>\n");
		size += sprintf(buf+size, "<root>\n");
		
		/*
		SQUASH_KA(req);
		send_request_uni_xml_ok(req);
		chunked_start(req,size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		size = 0;
		bzero(buf, CHUNKED_MAXLEN);
		*/
		size += sprintf(buf+size, "<common>\n");
		size += sprintf(buf+size, "<version>%d.%02d</version>\n",APP_VERSION_MAJOR,APP_VERSION_MINOR);
		
		//size += sprintf(buf+size, "<product>%s</product>\n", sys_info->config.lan_config.title);
		size += sprintf(buf+size, "<product>%s</product>\n", conf_string_read(TITLE));
		//size += sprintf(buf+size, "<serial>%s</serial>\n", sys_info->config.lan_config.title);
		size += sprintf(buf+size, "<serial>%s</serial>\n", conf_string_read(TITLE));
		
		size += sprintf(buf+size, "<oem>D-Link</oem>\n");
		//size += sprintf(buf+size, "<cameraName>%s</cameraName>\n", sys_info->config.lan_config.title);
		size += sprintf(buf+size, "<cameraName>%s</cameraName>\n", conf_string_read(TITLE));
		size += sprintf(buf+size, "<peripheral>\n");
		size += sprintf(buf+size, "<GPINs>%d</GPINs>\n", GIOIN_NUM);
		size += sprintf(buf+size, "<GPOUTs>%d</GPOUTs>\n", GIOOUT_NUM);
		size += sprintf(buf+size, "<speaker>0</speaker>\n");
		size += sprintf(buf+size, "<microphone>0</microphone>\n");
		size += sprintf(buf+size, "<PT>0</PT>\n");
		size += sprintf(buf+size, "<Z>0</Z>\n");
		size += sprintf(buf+size, "<RS485>0</RS485>\n");
		size += sprintf(buf+size, "<localStorage>1</localStorage>\n");
		size += sprintf(buf+size, "<frontLED>0</frontLED>\n");
		size += sprintf(buf+size, "<PIR>0</PIR>\n");
		size += sprintf(buf+size, "<wireless>0</wireless>\n");				
		size += sprintf(buf+size, "</peripheral>\n");
		
		size += sprintf(buf+size, "</common>\n");

		size += sprintf(buf+size, "<lang>\n");
			
		size += sprintf(buf+size, "<frame>\n");
		size += sprintf(buf+size, "<productpage>Product: </productpage>\n");
		size += sprintf(buf+size, "<version>Firmware Version: </version>\n");
		size += sprintf(buf+size, "<home>LIVE VIDEO</home>\n");
		size += sprintf(buf+size, "<advanced>SETUP</advanced>\n");
		size += sprintf(buf+size, "<tools>MAINTENANCE</tools>\n");
		size += sprintf(buf+size, "<status>STATUS</status>\n");
		size += sprintf(buf+size, "<support>HELP</support>\n");
		size += sprintf(buf+size, "<title1>D-LINK CORPORATION | INTERNET CAMERA | </title1>\n");
		size += sprintf(buf+size, "<title2>D-LINK CORPORATION | WIRELESS INTERNET CAMERA | </title2>\n");
		size += sprintf(buf+size, "<copyright1>Copyright </copyright1>\n");
		size += sprintf(buf+size, "<copyright2> 2007-2008 D-Link Corporation.</copyright2>\n");
		size += sprintf(buf+size, "</frame>\n");
		
		size += sprintf(buf+size, "<leftPanel>\n");
		size += sprintf(buf+size, "<deviceInfo>Device Info</deviceInfo>\n");
		size += sprintf(buf+size, "<log>Log</log>\n");
		size += sprintf(buf+size, "<logout>Logout</logout>\n");
		size += sprintf(buf+size, "<titleStatus>STATUS</titleStatus>\n");
		size += sprintf(buf+size, "</leftPanel>\n");
		
		size += sprintf(buf+size, "<body>\n");
		size += sprintf(buf+size, "<title>D-LINK SYSTEMS, INC | WIRELESS INTERNET CAMERA | HOME</title>\n");
		size += sprintf(buf+size, "<deviceInformation>Device Information</deviceInformation>\n");
		size += sprintf(buf+size, "<description1>All of your network connection details are displayed on this page. The firmware version is also displayed here.</description1>\n");
		size += sprintf(buf+size, "<Information>Information</Information>\n");
		size += sprintf(buf+size, "<hwBoard>Hardware Board</hwBoard>\n");
		size += sprintf(buf+size, "<hwVersion>Hardware Version</hwVersion>\n");
		size += sprintf(buf+size, "<cameraName>Camera Name</cameraName>\n");
		size += sprintf(buf+size, "<timeAndDate>Time &amp; Date</timeAndDate>\n");
		size += sprintf(buf+size, "<firmwareVersion>Firmware Version</firmwareVersion>\n");
		size += sprintf(buf+size, "<macAddress>MAC Address</macAddress>\n");
		size += sprintf(buf+size, "<ipAddress>IP Address</ipAddress>\n");
		size += sprintf(buf+size, "<subnetMask>IP Subnet Mask</subnetMask>\n");
		size += sprintf(buf+size, "<defaultRouter>Default Gateway</defaultRouter>\n");
		size += sprintf(buf+size, "<primaryDns>Primary DNS</primaryDns>\n");
		size += sprintf(buf+size, "<secondaryDns>Secondary DNS</secondaryDns>\n");
		size += sprintf(buf+size, "<pppoe>PPPoE</pppoe>\n");
		size += sprintf(buf+size, "<ddns>DDNS</ddns>\n");
		size += sprintf(buf+size, "</body>\n");
		
		size += sprintf(buf+size, "<hint>\n");
		size += sprintf(buf+size, "<helpfulHints>Helpful Hints..</helpfulHints>\n");
		size += sprintf(buf+size, "<description1>This page displays all the information about the camera and network settings.</description1>\n");
		size += sprintf(buf+size, "</hint>\n");
		
		size += sprintf(buf+size, "<message>\n");
		size += sprintf(buf+size, "<success>Changes saved.</success>\n");
		size += sprintf(buf+size, "</message>\n");
		
		size += sprintf(buf+size, "</lang>\n");
		
		size += sprintf(buf+size, "<config>\n");
		//size += sprintf(buf+size, "<hwBoard>%s</hwBoard>\n", sys_info->config.lan_config.title);
		size += sprintf(buf+size, "<hwBoard>%s</hwBoard>\n", conf_string_read(TITLE));
		size += sprintf(buf+size, "<hwVersion>%d</hwVersion>\n",APP_VERSION_MAJOR);
		size += sprintf(buf+size, "<build>%02d</build>\n",APP_VERSION_MINOR);	
		net_get_hwaddr(sys_info->ifname, mac);
		size += sprintf(buf+size, "<mac>%02X:%02X:%02X:%02X:%02X:%02X</mac>\n",mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5]);
		ip.s_addr = net_get_ifaddr(sys_info->ifname);
		size += sprintf(buf+size, "<address>%s</address>\n", (char*)inet_ntoa(ip));
		netmask.s_addr = net_get_netmask(sys_info->ifname);
		size += sprintf(buf+size, "<netmask>%s</netmask>\n", (char*)inet_ntoa(netmask));
		gateway.s_addr = net_get_gateway(sys_info->ifname);
		size += sprintf(buf+size, "<gateway>%s</gateway>\n", (char*)inet_ntoa(gateway));
		dns.s_addr = net_get_dns();
		size += sprintf(buf+size, "<dns1>%s</dns1>\n", (char*)inet_ntoa(dns));
		size += sprintf(buf+size, "<date>Sun Jan  4 01:27:27 1970</date>\n");	
		
		size += sprintf(buf+size, "<pppoe>\n");
		size += sprintf(buf+size, "<status>Disabled</status>\n");
		size += sprintf(buf+size, "</pppoe>\n");
		
		size += sprintf(buf+size, "<ddns>\n");	
		size += sprintf(buf+size, "<status>Disabled</status>\n");
		size += sprintf(buf+size, "</ddns>\n");
		
		size += sprintf(buf+size, "</config>\n");
		
		size += sprintf(buf+size, "</root>\n");	
		size += sprintf(buf+size, "\r\n");
		
		SQUASH_KA(req);
		send_request_uni_xml_ok(req);
		chunked_start(req,size);
		req->buffer_end += sprintf((void *)req_bufptr(req), buf);
		end_line(req);
		
		req->status = WRITE;
		return 1;
}
int uri_dlink_mjpeg(request * req)
{
	int idx = 0;

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	if(strcmp(req->cmd_arg[0].name, "profileid") == 0)
			idx = atoi(req->cmd_arg[0].value)-1;
	
	/*if(idx < 0 || idx > 2) {
		send_r_not_found(req);
		return 0;
	}*/

	return get_dlink_mjpeg_stream(req, sys_info->profile_config.profile_codec_idx[idx]-1);
		
}

int uri_dlink_mpeg4(request * req)
{
	int idx = 0;
	
	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	if(strcmp(req->cmd_arg[0].name, "profileid") == 0)
		idx = atoi(req->cmd_arg[0].value)-1;
	
	return get_dlink_mpeg4_stream(req, sys_info->profile_config.profile_codec_idx[idx]-1);
#if 0
	AV_DATA vol_data;
//	ACS_VideoHeader MPEG4HEADER;	
//	time_t dlinktime = time(NULL);
	//req->is_cgi = CGI;

	send_request_dlink_ok_mpeg4(req);

	req->http_stream = URI_DLINK_STREAM_MPEG4;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;

	if(GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_MPEG4|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_MP4_VOL\n");
		return 0;
	}

	if (get_mpeg4_data(&req->video, AV_OP_SUB_STREAM_1,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	/*if ( sys_info->profile_codec_fmt[0] == SEND_FMT_MPEG4 ){
		return get_mpeg4_stream(req, sys_info->profile_codec_idx[0]);
	}*/

	//print_dlink_mpeg4_headers(req);
	
	/*DLINK_HEADE VOL PART*/
/*	req->filesize = 18;
	memcpy(&MPEG4HEADER.ulDataCheckSum, vol_data.ptr + vol_data.size -4, 4);
	MPEG4HEADER.ulHdrID 					= 0xF5010000;
	MPEG4HEADER.ulHdrLength 			= sizeof(ACS_VideoHeader);
	MPEG4HEADER.ulDataLength 			=	req->filesize;
	MPEG4HEADER.ulSequenceNumber	=	req->av_data.serial;
	MPEG4HEADER.ulTimeSec					= (unsigned long)dlinktime;	
	MPEG4HEADER.ulTimeUSec				= 0;	
	if(req->av_data.frameType == AV_FRAME_TYPE_I_FRAME)
		MPEG4HEADER.usCodingType			= 0;
	else 
		MPEG4HEADER.usCodingType			= 1;
	MPEG4HEADER.usFrameRate				= 15;
	MPEG4HEADER.usWidth						= sys_info->config.lan_config.profilesize[0].xsize;
	MPEG4HEADER.usHeight					=	sys_info->config.lan_config.profilesize[0].ysize;
	MPEG4HEADER.ucMDBitmap				= 0;
	bzero(MPEG4HEADER.ucMDPowers, sizeof(MPEG4HEADER.ucMDPowers));	
	memcpy(req->buffer + req->buffer_end, &MPEG4HEADER, sizeof(MPEG4HEADER));	
	req->buffer_end += sizeof(MPEG4HEADER);*/
	
	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = vol_data.size;
	print_dlink_mpeg4_headers(req);
	req_copy(req, vol_data.ptr, vol_data.size);
//	GetAVData(AV_OP_UNLOCK_MP4_VOL, -1, NULL);
	return 1;
#endif
}
int uri_dlink_h264(request * req)
{
	int idx = 0;
	
	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	if(strcmp(req->cmd_arg[0].name, "profileid") == 0)
			idx = atoi(req->cmd_arg[0].value)-1;
	
	return get_dlink_h264_stream(req, sys_info->profile_config.profile_codec_idx[idx]-1);
#if 0
	AV_DATA vol_data;
	send_request_dlink_ok_mpeg4(req);

	req->http_stream = URI_DLINK_STREAM_H264;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;

	if(GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_H264|AV_OP_LOCK_VOL, -1, &vol_data) != RET_SUCCESS) {
		send_r_error(req);
		dbg("Error on AV_OP_LOCK_H264_VOL\n");
		return 0;
	}

	if (get_h264_data(&req->video, AV_OP_SUB_STREAM_1,0) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}

	req->data_mem = req->video.av_data.ptr;
	req->filesize = req->video.av_data.size;
	req->extra_size = vol_data.size;
	print_dlink_mpeg4_headers(req);
	req_copy(req, vol_data.ptr, vol_data.size);

	return 1;
#endif
}

int uri_dlink_audio(request * req)
{

	send_request_dlink_ok_audio(req);
	
	req->http_stream = URI_AUDIO_G726;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
	//if(sys_info->config.lan_config.audiooutenable == 0)
	if(sys_info->ipcam[AUDIOINENABLE].config.u8 == 0)
		return 0;

	if (av_stream_pending) {
		send_r_service_unavailable(req);
		return 0;
	}
	if (get_audio_data(&req->audio, AV_OP_SUB_STREAM_1) != RET_STREAM_SUCCESS)
		return 0;

	req->data_mem = req->audio.av_data.ptr;
	req->filesize = req->audio.av_data.size;
	req->filepos = 0;
	print_dlink_audio_headers(req);
	return 1;
}

int uri_dlink_video(request * req)
{
	int idx = 0;

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }

	if(strcmp(req->cmd_arg[0].name, "profileid") == 0)
		idx = atoi(req->cmd_arg[0].value)-1;

	if(idx < 0 || idx > 2) {
		send_r_not_found(req);
		return 0;
	}
    
    //if( strcmp( sys_info->config.profile_config[value-1].codecname , JPEG_PARSER_NAME ) == 0 ){      // MJPEG
    if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*idx) , JPEG_PARSER_NAME ) == 0 ){      // MJPEG
    	get_dlink_mjpeg_stream(req, sys_info->profile_config.profile_codec_idx[idx]-1);
    }
    //else if( strcmp( sys_info->config.profile_config[value-1].codecname , MPEG4_PARSER_NAME ) == 0 ){ // MPEG4
    else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*idx) , MPEG4_PARSER_NAME ) == 0 ){ // MPEG4
		get_dlink_mpeg4_stream(req,sys_info->profile_config.profile_codec_idx[idx]-1);
    }
    //else if( strcmp( sys_info->config.profile_config[value-1].codecname , H264_PARSER_NAME ) == 0 ){ // H.264
    else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*idx) , H264_PARSER_NAME ) == 0 ){ // H.264
		get_dlink_h264_stream(req, sys_info->profile_config.profile_codec_idx[idx]-1);
    }

	SQUASH_KA(req);
	return 1;
}

int uri_rtspurl(request * req)
{
	__u8 id = 0;
	int i = 0;
	char buf[256];
	size_t size = 0;

	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "profileid") == 0){
			id = atoi(req->cmd_arg[i].value) - 1;
#ifdef SUPPORT_PROFILE_NUMBER 
			if(id > sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value){
#else
			if(id > MAX_WEB_PROFILE_NUM){
#endif
				send_r_bad_request(req);
				return 0;
			}
		}
		if(strcmp(req->cmd_arg[i].name, "urlentry") == 0){
			if (strlen(req->cmd_arg[i].value) > MAX_HTTPSTREAMNAME_LEN){
				send_r_bad_request(req);
				return 0;
			}
			
			if (strcmp(conf_string_read(STREAM0_RTSPSTREAMNAME+2*id), req->cmd_arg[i].value)) {
				conf_lock();
				strcpy(conf_string_read(STREAM0_RTSPSTREAMNAME+2*id), req->cmd_arg[i].value);
				sys_info->osd_setup = 1;
				conf_unlock();
				info_set_flag(CONF_FLAG_CHANGE);
			/*
			if (strcmp(sys_info->config.lan_config.stream[id-1].rtspstreamname, req->cmd_arg[1].value)) {
				sysconf_lock();
				strcpy(sys_info->config.lan_config.stream[id-1].rtspstreamname, req->cmd_arg[1].value);
				sys_info->osd_setup = 1;
				sysconf_unlock();
				sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
				*/
				system("killall -SIGTERM rtsp-streamer\n");
				system("/opt/ipnc/rtsp-streamer &\n");
			}
		}
	}

	
	SQUASH_KA(req);
#ifdef SUPPORT_PROFILE_NUMBER
	for(i=0;i<sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value;i++){
#else
	for(i=0;i<MAX_WEB_PROFILE_NUM;i++){
#endif
		size += sprintf(buf+size,"profileid=%d\r\n", i+1);
		size += sprintf(buf+size,"urlentry=%s\r\n", conf_string_read(STREAM0_RTSPSTREAMNAME+2*i));
	}
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
	return 1;
}
int uri_audio(request * req)
{
	char buf[256];
	size_t size = 0;
	int id = 0;
	
	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	if(strcmp(req->cmd_arg[0].name, "profileid") == 0)
  		id = atoi(req->cmd_arg[0].value);
	
	size += sprintf(buf+size, "profileid=%d\r\n", id); 
	size += sprintf(buf+size, "codec=%s\r\n", conf_string_read(AUDIO_INFO_CODEC_NAME));
	size += sprintf(buf+size, "samplerate=8\r\n");
	size += sprintf(buf+size, "channel=1\r\n");
	size += sprintf(buf+size, "bitrate=32\r\n");
	
	SQUASH_KA(req);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
		
	req->status = WRITE;
	return 1;
}

int uri_ptz_info(request * req)
{
#ifndef SUPPORT_VISCA
		return -1;
#endif
#ifndef SUPPORT_AF
		return -1;
#endif
	char buf[256];
	size_t size = 0;

	size += sprintf(buf+size,"pmax=%d\r\n", sys_info->vsptz.nipca_ptz.pan.max);
	size += sprintf(buf+size,"pmin=%d\r\n", sys_info->vsptz.nipca_ptz.pan.min);
	size += sprintf(buf+size,"tmax=%d\r\n", sys_info->vsptz.nipca_ptz.tilt.max);
	size += sprintf(buf+size,"tmin=%d\r\n", sys_info->vsptz.nipca_ptz.tilt.min);
	size += sprintf(buf+size,"zmax=%d\r\n", sys_info->vsptz.nipca_ptz.zoom.max);
	size += sprintf(buf+size,"zmin=%d\r\n", sys_info->vsptz.nipca_ptz.zoom.min);
	size += sprintf(buf+size,"customizedhome=no\r\n");

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
	return 1;
}

int uri_ptz_pos(request * req)
{
	char buf[256];
	size_t size = 0;
#ifdef SUPPORT_VISCA

	VISCACamera_t    camera;
	VISCAInterface_t interface;
	unsigned int  arg = 0, option = 0;
	unsigned int info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;
	camera.address   = 1;

	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	
	option = CAM_PAN_TILT_POS_INQ;
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);
	dbg("info_value = %08X", info_value);
	dbg("info_extend = %08X", info_extend);

	int pos1 = (info_value & 0x0F000000)>>12;
	int pos2 = (info_value & 0x000F0000)>>8;	
	int pos3 = (info_value & 0x00000F00)>>4;
	int pos4 = (info_value & 0x0000000F);	
	sys_info->vsptz.nipca_ptz.pan.position = pos1+pos2+pos3+pos4;
	
	pos1 = (info_extend & 0x0F000000)>>12;
	pos2 = (info_extend & 0x000F0000)>>8;	
	pos3 = (info_extend & 0x00000F00)>>4;
	pos4 = (info_extend & 0x0000000F);
	sys_info->vsptz.nipca_ptz.tilt.position = pos1+pos2+pos3+pos4;
	
	option = CAM_ZOOM_POS_INQ;
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);
	dbg("info_value = %08X", info_value);
	dbg("info_extend = %08X", info_extend);
	pos1 = (info_value  & 0x00000F00)<<4;
	pos2 = (info_value  & 0x0000000F)<<8;	
	pos3 = (info_extend & 0x0F000000)>>20;
	pos4 = (info_extend & 0x000F0000)>>16;
	sys_info->vsptz.nipca_ptz.zoom.position = pos1+pos2+pos3+pos4;
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		return 1;

	//dbg("info_value = %08X", info_value);
	//dbg("info_extend = %08X", info_extend);
#elif defined (SUPPORT_AF)
	sys_info->vsptz.nipca_ptz.pan.position = 0;
	sys_info->vsptz.nipca_ptz.tilt.position = 0;
	sys_info->vsptz.nipca_ptz.zoom.position = sys_info->af_status.zoom;
#else
	return -1;
#endif
	size += sprintf(buf+size,"p=%d\r\n", sys_info->vsptz.nipca_ptz.pan.position);
	size += sprintf(buf+size,"t=%d\r\n", sys_info->vsptz.nipca_ptz.tilt.position);
	size += sprintf(buf+size,"z=%d\r\n", sys_info->vsptz.nipca_ptz.zoom.position);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	
	req->status = DONE;
	return 1;
	
}

int uri_ptz_step(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	#ifdef SUPPORT_AF
	sys_info->vsptz.nipca_ptz.zoom.step = sys_info->ipcam[LENS_SPEED].config.value;
	#endif
	uri_decoding(req, req->query_string);
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(strcmp(req->cmd_arg[i].name, "pstep") == 0)
			sys_info->vsptz.nipca_ptz.pan.step = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "tstep") == 0)
			sys_info->vsptz.nipca_ptz.tilt.step = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "zstep") == 0)
			sys_info->vsptz.nipca_ptz.zoom.step = atoi(req->cmd_arg[i].value);		 		
	}
#ifdef SUPPORT_VISCA
	if(sys_info->vsptz.nipca_ptz.pan.step > 8)	sys_info->vsptz.nipca_ptz.pan.step = 8;
	else if(sys_info->vsptz.nipca_ptz.pan.step < 1)	sys_info->vsptz.nipca_ptz.pan.step = 1;

	if(sys_info->vsptz.nipca_ptz.tilt.step > 8)	sys_info->vsptz.nipca_ptz.tilt.step = 8;
	else if(sys_info->vsptz.nipca_ptz.tilt.step < 1)	sys_info->vsptz.nipca_ptz.tilt.step = 1;
	
	if(sys_info->vsptz.nipca_ptz.zoom.step > 8)	sys_info->vsptz.nipca_ptz.zoom.step = 8;
	else if(sys_info->vsptz.nipca_ptz.zoom.step < 1)	sys_info->vsptz.nipca_ptz.zoom.step = 1;
#elif defined (SUPPORT_AF)
	sys_info->vsptz.nipca_ptz.pan.step = sys_info->vsptz.nipca_ptz.tilt.step = 0;
	if(sys_info->vsptz.nipca_ptz.zoom.step > MAX_LENS_SPEED) sys_info->ipcam[LENS_SPEED].config.value = MAX_LENS_SPEED;
	else if(sys_info->vsptz.nipca_ptz.zoom.step < 1)	sys_info->vsptz.nipca_ptz.zoom.step = 1;
	else	{
		info_write(&sys_info->vsptz.nipca_ptz.zoom.step, LENS_SPEED, sizeof(sys_info->vsptz.nipca_ptz.zoom.step), 0);
		sys_info->vsptz.nipca_ptz.zoom.step = sys_info->ipcam[LENS_SPEED].config.value;
	}
#else
	return -1;
#endif
	SQUASH_KA(req);
	size += sprintf(buf+size,"pstep=%d\r\n", sys_info->vsptz.nipca_ptz.pan.step);
	size += sprintf(buf+size,"tstep=%d\r\n", sys_info->vsptz.nipca_ptz.tilt.step);
	size += sprintf(buf+size,"zstep=%d\r\n", sys_info->vsptz.nipca_ptz.zoom.step);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}

int uri_ptz_preset_list(request * req)
{
#if defined (SUPPORT_RS485)
	char buf[MAX_PTZPRESET_NUM*MAX_PTZPRESETNAME_LEN+64];
	size_t size = 0;
	int i;
	int dot = 0;
	
	size += sprintf(buf+size,"presets=");
	for(i =0 ; i < MAX_PTZPRESET_NUM ; i++){
		//if(strlen(sys_info->config.ptz_config.presetposition[i].name) > 0){
		if(strlen(conf_string_read(PRESETPOSITION0_NAME+i)) > 0){

			//dbg("presetposition[%d].name = %s\n", i, sys_info->config.ptz_config.presetposition[i].name);
			if(dot == 0)
				dot = 1;
			else
				size += sprintf(buf+size,",");				

			//size += sprintf(buf+size,"%s", sys_info->config.ptz_config.presetposition[i].name);
			size += sprintf(buf+size,"%s", conf_string_read(PRESETPOSITION0_NAME+i));
		}
	}

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
#elif defined(MODEL_VS2415)
	char buf[MAX_VISCAPRESET*MAX_VISCAPRESETNAME_LEN+512];
	size_t size = 0;
	int i;
	int dot = 0;
	int tmp;
	char string_cmp[32];
	
	size += sprintf(buf+size,"presets=");
	for(i =0 ; i < MAX_VISCAPRESET ; i++){
	
		if(strlen(conf_string_read(VISCA_PRESET_ID01+i)) > 0){

			if(dot == 0)
				dot = 1;
			else
				size += sprintf(buf+size,",");				
			
			sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp,  string_cmp);
			size += sprintf(buf+size,"%s", string_cmp);
		}
	}

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;

#else
	return -1;
#endif
}
int serach_visca_empty_presetid(void)
{
	int i;
	int tmp;
	char string_cmp[32];
	
	for(i=0;i<MAX_VISCAPRESET;i++){
		sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp,  string_cmp);
		if(!strcmp(string_cmp, "nonsetting")){
			//dbg("tmp = %d", tmp);
			return tmp;
		}
	}
	
	return -1;
}
int serach_visca_presetid(char* name)
{
	int i;
	int tmp;
	char string_cmp[32];
	
	for(i=0;i<MAX_VISCAPRESET;i++){
		sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp,  string_cmp);
		if(!strcmp(string_cmp, name)){
			return tmp;
		}
	}
	
	return -1;
}

int uri_ptz_preset(request * req)
{
#if defined (SUPPORT_RS485)
	
	char buf[256];
	size_t size = 0;
	int i;
	char* name = NULL;
	char* act = NULL;
	int addr = 0;
	int cmd = 0;
	int clr = 0;
	int preset_point = 0;
	char name_tmp[MAX_PTZPRESETNAME_LEN];
	char name_clr[MAX_PTZPRESETNAME_LEN]="";
	char ptz_cmd[128];

	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "name") == 0){			
			name = req->cmd_arg[i].value;
			strcpy(name_tmp, name); 						
		}else if(strcmp(req->cmd_arg[i].name, "act") == 0){ 				
			if(strcmp(req->cmd_arg[i].value, "add") == 0){
				act = req->cmd_arg[i].value;
				cmd = SETPRESET_MODE;
			}else if(strcmp(req->cmd_arg[i].value, "del") == 0){
				act = req->cmd_arg[i].value;
				cmd = CLRPRESET_MODE;
			}else if(strcmp(req->cmd_arg[i].value, "go") == 0){
				act = req->cmd_arg[i].value;
				cmd = GOTOPRESET_MODE;
			}
		}
	}
	dbg("cmd = %02x\n", cmd);

	if(cmd == CLRPRESET_MODE){
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){
			if(strcmp(conf_string_read(PRESETPOSITION0_NAME+i), name) == 0 ){
				dbg("addr=%d",	i);
				addr = i;
				preset_point = sys_info->ipcam[PRESETPOSITION0_INDEX+addr].config.value;
				break;
			}
		}
		if((addr <= MAX_PTZPRESET_NUM)	){
			info_write(&clr, PRESETPOSITION0_INDEX+addr, sizeof(clr), 0);
			info_write(&name_clr, PRESETPOSITION0_NAME+addr, sizeof(name_clr), 0);
		}else
			return 0;
	}else if(cmd == SETPRESET_MODE){
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){
			if(strlen(conf_string_read(PRESETPOSITION0_NAME+i)) <= 0 ){
				dbg("addr=%d",	i);
				addr = i;
				break;
			}
		}
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){
			if(sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value == preset_point){
				i = 0;
				preset_point++;
			}
		}
		dbg("preset_point=%d\n", preset_point);
		if((addr  <= MAX_PTZPRESET_NUM)  && strlen(name_tmp) < MAX_PTZPRESETNAME_LEN){
			info_write(&preset_point, PRESETPOSITION0_INDEX+addr, sizeof(addr), 0);
			info_write(&name_tmp, PRESETPOSITION0_NAME+addr, sizeof(name_tmp), 0);
		}else
			return 0;
	}else if(cmd == GOTOPRESET_MODE){
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){
			if( strcmp(conf_string_read(PRESETPOSITION0_NAME+i), name) == 0){
				addr = sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value;
				preset_point = addr;
				dbg("match.....addr = %d\n", addr);
			}
		}
	}else
		return 0;

	dbg("addr=%d\n", addr);
	for(i=0;i<MAX_PTZPRESET_NUM;i++)
		dbg("%d->%d:%s\n", i, sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value, conf_string_read(PRESETPOSITION0_NAME+i));

	if(preset_point > 0){
		sprintf(ptz_cmd, "/opt/ipnc/ptzctrl -m %d -n %d &", cmd, preset_point);
		dbg("ptz_cmd = %s", ptz_cmd);
		system(ptz_cmd);
	}
	
	SQUASH_KA(req);

	size += sprintf(buf+size,"name=%s\r\n", name);
	size += sprintf(buf+size,"act=%s\r\n", act);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
#elif defined(MODEL_VS2415)
	
	size_t size = 0;
	int presetid;
	char* presetname;
	VISCAPRESET viscapreset;

	int i, ret;
	char* action = NULL;
	char buf[256];

	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "name") == 0){			
				strcpy(viscapreset.name, req->cmd_arg[i].value);							
		}else if(strcmp(req->cmd_arg[i].name, "act") == 0){ 				
				if(strcmp(req->cmd_arg[i].value, "add") == 0){
					action = req->cmd_arg[i].value;
				}else if(strcmp(req->cmd_arg[i].value, "del") == 0){
					action = req->cmd_arg[i].value;
				}else if(strcmp(req->cmd_arg[i].value, "go") == 0){
					action = req->cmd_arg[i].value;
				}
		}
	}
	//viscapreset.index = -1;
	
	if(strcmp(action, "add") == 0){
		viscapreset.index = serach_visca_empty_presetid() - 1;
		//dbg("presetid = %d", viscapreset.index);
		if(viscapreset_add(viscapreset) == 2){
			DBG("Preset name already exist");

		}
	}else if(strcmp(action, "del") == 0){
		viscapreset.index = serach_visca_presetid(viscapreset.name) - 1;
		ret = viscapreset_del(viscapreset);
		if(ret == 1){
			dbg("ret = %d", ret);
		}	
	}else if(strcmp(action, "go") == 0){
		viscapreset.index = serach_visca_presetid(viscapreset.name) - 1;
		//dbg("presetid = %d", viscapreset.index);
		if(viscapreset_goto(viscapreset, viscapreset.name, req) < 0)
			DBG("viscapreset_goto error");
	}
	
	SQUASH_KA(req);

	size += sprintf(buf+size,"name=%s\r\n", viscapreset.name);
	size += sprintf(buf+size,"act=%s\r\n", action);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
#else
	return -1;
#endif

}

int uri_ptz_move(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	int pmove=0,tmove=0,zmove=0;
	NIPCA_PTZ ptz_pos;
	
#ifdef SUPPORT_AF
	pmove = 0;
	tmove = 0;
	zmove = sys_info->ipcam[AUTOFOCUS_ZOOM_POSTITION].config.value;
#elif defined (SUPPORT_VISCA)
	pmove = 0;
	tmove = 0;
	zmove = 0;
#else
	return -1;
#endif

	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(strcmp(req->cmd_arg[i].name, "p") == 0)
			pmove = atoi(req->cmd_arg[i].value);
		else if(strcmp(req->cmd_arg[i].name, "t") == 0)
			tmove = atoi(req->cmd_arg[i].value);
		else if(strcmp(req->cmd_arg[i].name, "z") == 0)
			zmove = atoi(req->cmd_arg[i].value);		  		
	}
	if(pmove > sys_info->vsptz.nipca_ptz.pan.max)	pmove = sys_info->vsptz.nipca_ptz.pan.max;
	else if(pmove < sys_info->vsptz.nipca_ptz.pan.min)	pmove = sys_info->vsptz.nipca_ptz.pan.min;

	if(tmove > sys_info->vsptz.nipca_ptz.tilt.max)	tmove = sys_info->vsptz.nipca_ptz.tilt.max;
	else if(tmove < sys_info->vsptz.nipca_ptz.tilt.min) tmove = sys_info->vsptz.nipca_ptz.tilt.min;
	
	if(zmove > sys_info->vsptz.nipca_ptz.zoom.max)	zmove = sys_info->vsptz.nipca_ptz.zoom.max;
	else if(zmove < sys_info->vsptz.nipca_ptz.zoom.min) zmove = sys_info->vsptz.nipca_ptz.zoom.min;

	ptz_pos.pan.position = pmove;
	ptz_pos.tilt.position = tmove;
	ptz_pos.zoom.position = zmove;

	ControlSystemData(SFIELD_SET_PTZ_ABSOLUTE_POSITION, &ptz_pos, sizeof(ptz_pos), &req->curr_user);

	SQUASH_KA(req);

	size += sprintf(buf+size,"p=%d\r\n", pmove);
	size += sprintf(buf+size,"t=%d\r\n", tmove);
	size += sprintf(buf+size,"z=%d\r\n", zmove);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}

int uri_ptz_direction(request * req)
{
#ifndef SUPPORT_VISCA
	return -1;
#endif

	char buf[256];
	size_t size = 0;
	int i;
	int speed = 5;
	char *direction=NULL;
	unsigned int option = 0;
	pid_t pid;
	char ptz_mode[8];
	char ptz_arg[64];

	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(strcmp(req->cmd_arg[i].name, "direction") == 0)
			direction = req->cmd_arg[i].value;
		else if(strcmp(req->cmd_arg[i].name, "speed") == 0)
			speed = atoi(req->cmd_arg[i].value);			
	}

	if(!direction)	return -1;
	if(speed > 10 || speed < 0)	speed = 5;

	if(!strcmp(direction, "up"))
		option = CAM_PANTILT_UP;
	else if(!strcmp(direction, "down"))
		option = CAM_PANTILT_DOWN;
	else if(!strcmp(direction, "left"))
		option = CAM_PANTILT_LEFT;
	else if(!strcmp(direction, "right"))
		option = CAM_PANTILT_RIGHT;
	else if(!strcmp(direction, "upleft"))
		option = CAM_PANTILT_UPLEFT;
	else if(!strcmp(direction, "upright"))
		option = CAM_PANTILT_UPRIGHT;
	else if(!strcmp(direction, "downleft"))
		option = CAM_PANTILT_DOWNLEFT;
	else if(!strcmp(direction, "downright"))
		option = CAM_PANTILT_DOWNRIGHT;
	else if(!strcmp(direction, "zoomwide"))
		option = CAM_ZOOM_WIDE;
	else if(!strcmp(direction, "zoomtele"))
		option = CAM_ZOOM_TELE;
	else if(!strcmp(direction, "zoomstop"))
		option = CAM_ZOOM_STOP;
	else if(!strcmp(direction, "stop"))
		option = CAM_PANTILT_STOP;
	else
		return -1;
	
	sprintf(ptz_mode, "%d", option);
	sprintf(ptz_arg, "%d", speed);
	dbg("ptz_mode = %s", ptz_mode);
	dbg("ptz_arg = %s", ptz_arg);
	if ( (pid = fork()) == 0 ) {
		/* the child */
		
		execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
		DBGERR("exec "VISCA_EXEC_PATH" failed\n");
		_exit(0);
	}
	else if (pid < 0) {
		DBGERR("fork "VISCA_EXEC_PATH" failed\n");
		return -1;
	}
	SQUASH_KA(req);

	//size += sprintf(buf+size,"direction=%s\r\n", direction);
	//size += sprintf(buf+size,"speed=%d\r\n", speed);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}

int uri_ptz_move_rel(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	int pmove_rel=0,tmove_rel=0,zmove_rel=0;
	
	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(strcmp(req->cmd_arg[i].name, "p") == 0)
			pmove_rel = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "t") == 0)
			tmove_rel = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "z") == 0)
			zmove_rel = atoi(req->cmd_arg[i].value);		 		
	}

	pmove_rel *= sys_info->vsptz.nipca_ptz.pan.step;
	tmove_rel *= sys_info->vsptz.nipca_ptz.tilt.step;	
	// zmove_rel < 0, is zoom out
	zmove_rel *= sys_info->vsptz.nipca_ptz.zoom.step;
	
#ifdef SUPPORT_VISCA	
	VISCACamera_t    camera;
	VISCAInterface_t interface;
	unsigned int  arg = 0, option = 0;
	unsigned int info_value, info_extend;
	int pos1, pos2, pos3, pos4;
	interface.broadcast = 0;
	camera.socket_num	= 2;
	camera.address   = 1;
	int pan_dir=0, tilt_dir=0;
	
	if(pmove_rel < 0)	pan_dir = 1; //left
	else	pan_dir = 0; //right

	if(tmove_rel < 0)	tilt_dir = 0; //up
	else	tilt_dir = 1; //down
		
	if(pmove_rel > 32)	pmove_rel = 32;
	else if(pmove_rel < -32)	pmove_rel = -32;
	
	if(tmove_rel > 32)	tmove_rel = 32;
	else if(tmove_rel < -32)	tmove_rel = -32;
	
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
			return 1;

	// pan and tilt
	option = CAM_PAN_TILT_POS_INQ;
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);
	//dbg("info_value = %08X", info_value);
	//dbg("info_extend = %08X", info_extend);

	pos1 = (info_value & 0x0F000000)>>12;
	pos2 = (info_value & 0x000F0000)>>8;
	pos3 = (info_value & 0x00000F00)>>4;
	pos4 = (info_value & 0x0000000F);	
	sys_info->vsptz.nipca_ptz.pan.position = pos1+pos2+pos3+pos4;
	
	pos1 = (info_extend & 0x0F000000)>>12;
	pos2 = (info_extend & 0x000F0000)>>8;
	pos3 = (info_extend & 0x00000F00)>>4;
	pos4 = (info_extend & 0x0000000F);
	sys_info->vsptz.nipca_ptz.tilt.position = pos1+pos2+pos3+pos4;

	// zoom
	option = CAM_ZOOM_POS_INQ;
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);
	//dbg("info_value = %08X", info_value);
	//dbg("info_extend = %08X", info_extend);
	pos1 = (info_value  & 0x00000F00)<<4;
	pos2 = (info_value  & 0x0000000F)<<8;	
	pos3 = (info_extend & 0x0F000000)>>20;
	pos4 = (info_extend & 0x000F0000)>>16;
	sys_info->vsptz.nipca_ptz.zoom.position = pos1+pos2+pos3+pos4;

	/*
	dbg("ptz.pan.position = %d", sys_info->vsptz.nipca_ptz.pan.position);
	dbg("ptz.tilt.position = %d", sys_info->vsptz.nipca_ptz.tilt.position);
	dbg("ptz.zoom.position = %d", sys_info->vsptz.nipca_ptz.zoom.position);
	dbg("pmove_rel = %d", pmove_rel);
	dbg("tmove_rel = %d", tmove_rel);
	dbg("zmove_rel = %d", zmove_rel);
*/
	if(sys_info->vsptz.nipca_ptz.tilt.position+tmove_rel > sys_info->vsptz.nipca_ptz.tilt.max){
		tmove_rel = sys_info->vsptz.nipca_ptz.tilt.max - sys_info->vsptz.nipca_ptz.tilt.position;
		sys_info->vsptz.nipca_ptz.tilt.position = sys_info->vsptz.nipca_ptz.tilt.max;
	}else if(sys_info->vsptz.nipca_ptz.tilt.position+tmove_rel < sys_info->vsptz.nipca_ptz.tilt.min){
		tmove_rel = sys_info->vsptz.nipca_ptz.tilt.min - sys_info->vsptz.nipca_ptz.tilt.position;
		sys_info->vsptz.nipca_ptz.tilt.position = sys_info->vsptz.nipca_ptz.tilt.min;
	}else
		sys_info->vsptz.nipca_ptz.tilt.position += tmove_rel;
	
	if(sys_info->vsptz.nipca_ptz.zoom.position+zmove_rel > sys_info->vsptz.nipca_ptz.zoom.max){
		zmove_rel = sys_info->vsptz.nipca_ptz.zoom.max - zmove_rel;
		sys_info->vsptz.nipca_ptz.zoom.position = sys_info->vsptz.nipca_ptz.zoom.max;
	}else if(sys_info->vsptz.nipca_ptz.zoom.position+zmove_rel < sys_info->vsptz.nipca_ptz.zoom.min){
		zmove_rel = sys_info->vsptz.nipca_ptz.zoom.min - sys_info->vsptz.nipca_ptz.zoom.position;
		sys_info->vsptz.nipca_ptz.zoom.position = sys_info->vsptz.nipca_ptz.zoom.min;
	}else
		sys_info->vsptz.nipca_ptz.zoom.position += zmove_rel;

	option = CAM_PANTILT_RELATIVE_POSITION;
	arg = ((abs(pmove_rel) & 0x000000FF)<<16) + (abs(tmove_rel) & 0x000000FF);
	
	if(pan_dir == 1)  arg |= 0x10000000;
	if(tilt_dir == 1) arg |= 0x00001000;
	
	dbg("CAM_PANTILT_RELATIVE_POSITION arg = %08x\n", arg);
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);
	dbg("info_value = %08X", info_value);
	dbg("info_extend = %08X", info_extend);
	
	option = CAM_ZOOM_DIRECT;
	arg = (sys_info->vsptz.nipca_ptz.zoom.position & 0x0000FFFF)<<8;
	dbg("CAM_ZOOM_DIRECT arg = %08x\n", arg);
	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);

	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		return 1;
#elif defined (SUPPORT_AF)
	int zoom_pos;
	pmove_rel = tmove_rel = 0;

	ControlSystemData(SFIELD_GET_AF_ZOOM_POSITION, &zoom_pos, sizeof(zoom_pos), &req->curr_user);
	dbg("zoom_pos=%d", zoom_pos);
	
	zoom_pos += zmove_rel;

	if(zoom_pos < 0){
		zmove_rel = (-1)*(abs(zmove_rel) - abs(zoom_pos));
		zoom_pos = 0;
	}else if(zoom_pos > MAX_AF_ZOOM){
		zmove_rel = (1)*(abs(zmove_rel) - (zoom_pos - MAX_AF_ZOOM));
		zoom_pos = MAX_AF_ZOOM;
	}
	ControlSystemData(SFIELD_SET_AF_ZOOM_POSITION, &zoom_pos, sizeof(zoom_pos), &req->curr_user);
	info_write(&zoom_pos, AUTOFOCUS_ZOOM_POSTITION, sizeof(zoom_pos), 0);
	ControlSystemData(SFIELD_GET_AF_ZOOM_POSITION, &zoom_pos, sizeof(zoom_pos), &req->curr_user);
#else
	return -1;
#endif
	//dbg("pmove_rel = %d", sys_info->vsptz.nipca_ptz.pan.step);
	//dbg("tmove_rel = %d", sys_info->vsptz.nipca_ptz.tilt.step);
	//dbg("zmove_rel = %d", sys_info->vsptz.nipca_ptz.zoom.step);

	SQUASH_KA(req);

	size += sprintf(buf+size,"p=%d\r\n", pmove_rel/sys_info->vsptz.nipca_ptz.pan.step);
	size += sprintf(buf+size,"t=%d\r\n", tmove_rel/sys_info->vsptz.nipca_ptz.tilt.step);
	size += sprintf(buf+size,"z=%d\r\n", zmove_rel/sys_info->vsptz.nipca_ptz.zoom.step);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}
int uri_ptz_home(request * req)
{
#ifndef SUPPORT_VISCA
	return -1;
#endif
	char buf[256];
	size_t size = 0;	
	char act[16];
	NIPCA_PTZ ptz_home;
	int i, ret=0;
	VISCAPRESET viscapreset;
	
	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(strcmp(req->cmd_arg[i].name, "act") == 0)
			strcpy(act, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "p") == 0)
			ptz_home.pan.position= atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "t") == 0)
			ptz_home.tilt.position = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "z") == 0)
			ptz_home.zoom.position = atoi(req->cmd_arg[i].value);		 		
	}

	//dbg("act = %s", act);
	//dbg("home_p = %d", ptz_home.pan.position);
	//dbg("home_t = %d", ptz_home.tilt.position);
	//dbg("home_z = %d", ptz_home.zoom.position);

	if(!strcmp(act, "get")){
		//dbg("get homeposition");
		if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8 == 0){
			DBG("NO Setting");
			SQUASH_KA(req);
			size += sprintf(buf+size,"p=0\r\n");
			size += sprintf(buf+size,"t=0\r\n");
			size += sprintf(buf+size,"z=0\r\n");
			send_request_dlink_ok(req, size);
			req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
			req->status = DONE;
			return 1;
		}

	}else if(!strcmp(act, "set")){
		if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8 == 1){
			viscahome_reset();
			viscapreset.index = sys_info->ipcam[NIPCA_HOME_ID].config.value;
			viscapreset_del(viscapreset);
		}
		strcpy(viscapreset.name, "Home_Position");
		viscapreset.index = serach_visca_empty_presetid()-1;
		//dbg("presetid = %d", viscapreset.index);
		ret = viscapreset_add(viscapreset);
		if(ret == 2)
			DBG("Preset name already exist");
		else if (ret != 0)
			return -1;

		ControlSystemData(SFIELD_SET_PTZ_ABSOLUTE_POSITION, &ptz_home , sizeof(ptz_home), &req->curr_user);
		viscahome_set(simple_itoa(viscapreset.index+1));
		info_write(&viscapreset.index, NIPCA_HOME_ID, sizeof(viscapreset.index), 0);
		info_write(&ptz_home.pan.position, NIPCA_HOME_P, sizeof(ptz_home.pan.position), 0);
		info_write(&ptz_home.tilt.position, NIPCA_HOME_T, sizeof(ptz_home.pan.position), 0);
		info_write(&ptz_home.zoom.position, NIPCA_HOME_Z, sizeof(ptz_home.pan.position), 0);
	}else if(!strcmp(act, "go")){
		viscahome_go();
	}else if(!strcmp(act, "reset")){
		viscahome_reset();
		viscapreset.index = sys_info->ipcam[NIPCA_HOME_ID].config.value;
		viscapreset_del(viscapreset);
		info_write(&sys_info->ipcam[NIPCA_HOME_P].def_value.value, NIPCA_HOME_P, sizeof(ptz_home.pan.position), 0);
		info_write(&sys_info->ipcam[NIPCA_HOME_T].def_value.value, NIPCA_HOME_T, sizeof(ptz_home.pan.position), 0);
		info_write(&sys_info->ipcam[NIPCA_HOME_Z].def_value.value, NIPCA_HOME_Z, sizeof(ptz_home.pan.position), 0);
	}else{
		DBG("Unknown act(%s)", act);
		return -1;
	}

	SQUASH_KA(req);
	size += sprintf(buf+size,"p=%d\r\n", sys_info->ipcam[NIPCA_HOME_P].config.value);
	size += sprintf(buf+size,"t=%d\r\n", sys_info->ipcam[NIPCA_HOME_T].config.value);
	size += sprintf(buf+size,"z=%d\r\n", sys_info->ipcam[NIPCA_HOME_Z].config.value);
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}
int uri_auto_partol(request * req)
{
#ifndef SUPPORT_VISCA
	return -1;
#endif

	char buf[256];
	size_t size = 0;
	int i, act=0;
	int mode[3]={CAM_SEQUENCE_GO, -1, CAM_STOP_TOUR};
	pid_t pid;
	char ptz_mode[8];
	char ptz_arg[64];
	
	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(!strcmp(req->cmd_arg[i].name, "Act")){
			if(!strcmp(req->cmd_arg[i].value, "go"))
				act = mode[0];
			else if(!strcmp(req->cmd_arg[i].value, "stop"))
				act = mode[2];
			else
				DBG("Unknown Act=%s cmd", req->cmd_arg[i].value);
		}
	}
	
	sprintf(ptz_mode, "%d", act);
	sprintf(ptz_arg, "0");
	dbg("ptz_mode = %s", ptz_mode);
	dbg("ptz_arg = %s", ptz_arg);
	if ( (pid = fork()) == 0 ) {
		/* the child */
		
		execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
		DBGERR("exec "VISCA_EXEC_PATH" failed\n");
		_exit(0);
	}
	else if (pid < 0) {
		DBGERR("fork "VISCA_EXEC_PATH" failed\n");
		return -1;
	}

	SQUASH_KA(req);
	if(act == CAM_SEQUENCE_GO)
		size += sprintf(buf+size,"Act=go\r\n");
	else
		size += sprintf(buf+size,"Act=stop\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}

int uri_auto_pan(request * req)
{
#ifndef SUPPORT_VISCA
	return -1;
#endif

	char buf[256];
	size_t size = 0;
	int i, act=0;
	int mode[3]={CAM_AUTOPAN_GO, -1, CAM_STOP_TOUR};
	pid_t pid;
	char ptz_mode[8];
	char ptz_arg[64];
	
	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(!strcmp(req->cmd_arg[i].name, "Act")){
			if(!strcmp(req->cmd_arg[i].value, "go")){
				act = mode[0];
			}else if(!strcmp(req->cmd_arg[i].value, "stop")){
				act = mode[2];
			}else
				DBG("Unknown Act=%s cmd", req->cmd_arg[i].value);
		}			
	}
	
	sprintf(ptz_mode, "%d", act);
	sprintf(ptz_arg, "0");
	dbg("ptz_mode = %s", ptz_mode);
	dbg("ptz_arg = %s", ptz_arg);
	if ( (pid = fork()) == 0 ) {
		/* the child */
		
		execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
		DBGERR("exec "VISCA_EXEC_PATH" failed\n");
		_exit(0);
	}
	else if (pid < 0) {
		DBGERR("fork "VISCA_EXEC_PATH" failed\n");
		return -1;
	}

	SQUASH_KA(req);

	if(act == CAM_AUTOPAN_GO)
		size += sprintf(buf+size,"Act=go\r\n");
	else
		size += sprintf(buf+size,"Act=stop\r\n");
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}
int uri_config_auto_patrol(request * req)
{
#ifndef SUPPORT_VISCA
	return -1;
#endif
	char buf[MAX_VISCAPRESET*MAX_VISCAPRESETNAME_LEN+512];
	size_t size = 0;
	int i, j, Fd, num=0, total=0, match=0;
	int tmp, max=0;
	char string_cmp[32];
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	unsigned short int pp, qq;
	char *ptr[MAX_PTZPRESET_NUM];
	char *str=NULL, *src=NULL;
	pid_t pid;
	char ptz_mode[8];
	char ptz_arg[64];
	NIPCA_SEQUENCE auto_patrol[MAX_PTZPRESET_NUM];
		
	memset(ptr, 0, sizeof(char*) * MAX_PTZPRESET_NUM);

	if(!req->query_string)
		dbg("req->query_string NULL");
	
	if ( uri_decoding(req, req->query_string) < 0) {
		if(req->cmd_count <= 0)
			goto END_AUTO_PATROL;
		send_r_bad_request(req);
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(!strcmp(req->cmd_arg[i].name, "presets")){
			max = strlen(req->cmd_arg[i].value);
			str = req->cmd_arg[i].value;
			src = req->cmd_arg[i].value;
			size += sprintf(buf+size,"presets=%s\r\n", req->cmd_arg[i].value);
			dbg("req->cmd_arg[i].value = %s len =%d", req->cmd_arg[i].value, max);
			
			for(j = 0; j < MAX_PTZPRESET_NUM ; j++) {
				
				str = strtok(str, ",");
				if(str == NULL)
				    break;

				ptr[j] = str;
				dbg("ptr[%d] = %s", j, ptr[j]);

				if((str + strlen(ptr[j]) + 1) > (src + max)){
					ptr[j++] = str;
					total++;
					dbg("end");
					break;
				}else{
					str += (strlen(ptr[j])+1);
				}

				total++;
			}
		}
	}
	
	dbg("total=%d", total);
	
	for(i =0 ; i < total ; i++){
		//dbg("i=%d", i);
			
		for(j = 0; j < MAX_VISCAPRESET ; j++){
			sscanf(conf_string_read(VISCA_PRESET_ID01+j), "%d-%s", &tmp,  string_cmp);
			if(!strcmp(string_cmp, "nonsetting")) continue;

			//dbg("ptr[%d]=%s", i, ptr[i]);
			//dbg("string_cmp=%s", string_cmp);
			if(!strcmp(ptr[i], string_cmp)){
				//dbg("match -> %d", tmp);
				auto_patrol[i].line = 0;
				auto_patrol[i].seq_idx = i;
				auto_patrol[i].preset_idx = tmp;
				auto_patrol[i].dwell_time = 6;
				auto_patrol[i].speed = 7;
				auto_patrol[i].isendseq = -1;
				match++;
				break;
			}
		}
	}

	if(total != match) {
		DBG("Enter the value contains an invalid value");
		return -1;
	}

	memset(sys_info->auto_patrol, 0, sizeof(sys_info->auto_patrol));
	memcpy(sys_info->auto_patrol, auto_patrol, sizeof(sys_info->auto_patrol));
	
	sys_info->auto_patrol[total-1].isendseq = total;
	//dbg("auto patrol end point = %d", sys_info->auto_patrol[total-1].isendseq);

	sprintf(ptz_mode, "%d", CAM_SEQUENCE_SET);
	sprintf(ptz_arg, "%d", total);
	//dbg("ptz_mode = %s", ptz_mode);
	//dbg("ptz_arg = %s", ptz_arg);

	if ( (pid = fork()) == 0 ) {
		/* the child */
		
		execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
		DBGERR("exec "VISCA_EXEC_PATH" failed\n");
		_exit(0);
	}
	else if (pid < 0) {
		DBGERR("fork "VISCA_EXEC_PATH" failed\n");
		return -1;
	}

/*
	for(i =0 ; i < total ; i++){
		dbg("===== i = %d =====", i);
		dbg("line       = %d", auto_patrol[i].line);
		dbg("seq_idx    = %d", auto_patrol[i].seq_idx);
		dbg("preset_idx = %d", auto_patrol[i].preset_idx);
		dbg("dwell_time = %d", auto_patrol[i].dwell_time);
		dbg("speed      = %d", auto_patrol[i].speed);
		dbg("isendseq   = %d", auto_patrol[i].isendseq);
	}
*/

	dbg("nipca_sequence_config ok");

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
	
END_AUTO_PATROL:

	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
     	DBGERR("open sequence.vs failed! %d\n",__LINE__); 
	
	size += sprintf(buf+size,"presets=");
	
	//for(i=0;i<sys_info->vsptz.sequencepoint;i++)
	for(i=0;i<MAX_PTZPRESET_NUM;i++)
	{
		lseek(Fd, i*4, SEEK_SET);
		read(Fd, &pp, sizeof(pp)); 
		read(Fd, &qq, sizeof(qq));
		preset_point[i] = pp;
		dwell_time[i] = (qq >> 8) & 0x00FF;
		speed[i] = qq & 0x00FF;

		if(pp == 0)
			break;

		num++;
		if(num == 2)
		{
			num -= 1;
			size += sprintf(buf+size,",");
		}
		sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
		if(strcmp (string_cmp, "") != 0){
			size += sprintf(buf+size,"%s", string_cmp);
		}		
	}

	size += sprintf(buf+size,"\r\n");
	if(close(Fd) != 0)
		DBGERR("close sequence.vs failed! %d\n",__LINE__);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf);
	req->status = DONE;
	return 1;
}	
int uri_focus_type(request * req)
{
	char buf[256];
	size_t size = 0;
	int i;
	char* action=NULL;
	int type = AF_MANUAL;

	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(!strcmp(req->cmd_arg[i].name, "act")){
			action = req->cmd_arg[i].value;
		}
		if(!strcmp(req->cmd_arg[i].name, "type")){
			type = atoi(req->cmd_arg[i].value);
			dbg("type =%d", type);
			if((type != AF_MANUAL) && (type != AF_AUTO))
				return -1;
		}
	}
	dbg("action = %s", action);
	dbg("type = %d", type);
	
	if(!action) return -1;
	
	if(!strcmp(action, "get")){
		ControlSystemData(SFIELD_GET_AF_ENABLE, &type, sizeof(type), &req->curr_user);
	}else if(!strcmp(action, "set")){
#ifdef SUPPORT_VISCA
		ControlSystemData(SFIELD_SET_AF_ENABLE, &type, sizeof(type), &req->curr_user);
#elif defined SUPPORT_AF
		if(ControlSystemData(SFIELD_SET_AF_ENABLE, &type, sizeof(type), &req->curr_user) > 0){
			info_write(&type, AUTOFOCUS_MODE, sizeof(type), 0);
			type = sys_info->ipcam[AUTOFOCUS_MODE].config.value;
		}
#else
		return -1;
#endif

	}else
		return -1;

	dbg("dispaly");
	SQUASH_KA(req);

	size += sprintf(buf+size,"type=%d\r\n", type);
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}
int uri_manual_focus(request * req)
{
	char buf[256];
	size_t size = 0;
	int i, focus_move=0;
	int type = AF_MANUAL, focus_pos;
	
	if ( uri_decoding(req, req->query_string) < 0) {
		send_r_bad_request(req);
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++)
	{
		if(!strcmp(req->cmd_arg[i].name, "step")){
			focus_move = atoi(req->cmd_arg[i].value);
			if(abs(focus_move)>32) return -1;
		}
	}
#ifdef SUPPORT_AF
	type = sys_info->ipcam[AUTOFOCUS_MODE].config.value;
#elif defined(SUPPORT_VISCA)
	ControlSystemData(SFIELD_GET_AF_ENABLE, &type, sizeof(type), &req->curr_user);
#else
	return -1;
#endif
	
	dbg("type = %s", type?"AF_AUTO":"AF_MANUAL");

	if(type == AF_AUTO){
		focus_move = 0;
	}else if(type == AF_MANUAL){
		ControlSystemData(SFIELD_GET_AF_FOCUS_POSITION, &focus_pos, sizeof(focus_pos), &req->curr_user);
		dbg("focus_pos=%x", focus_pos);
		
		focus_pos -= focus_move;

		if(focus_pos < 0){
			focus_move = (1)*(abs(focus_move) - abs(focus_pos));
			focus_pos = 0;
		}else if(focus_pos > MAX_AF_FOCUS){
			focus_move = (-1)*(abs(focus_move) - (focus_pos - MAX_AF_FOCUS));
			focus_pos = MAX_AF_FOCUS;
		}
		ControlSystemData(SFIELD_SET_AF_FOCUS_POSITION, &focus_pos, sizeof(focus_pos), &req->curr_user);
		#ifdef SUPPORT_AF
		info_write(&focus_pos, AUTOFOCUS_FOCUS_POSTITION, sizeof(focus_pos), 0);
		#endif
	}else
		return -1;
	
	dbg("focus_pos = %d, focus_move = %d", focus_pos,focus_move );

	SQUASH_KA(req);
	
	size += sprintf(buf+size,"step=%d\r\n", focus_move);
	
	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 

	req->status = DONE;
	return 1;
}

int uri_notify(request * req)
{
	char buf[256];
	size_t size = 0;
	//int i;
	
	SQUASH_KA(req);
	size += sprintf(buf+size,"md1=%s\r\n", sys_info->status.motion? "on":"off");
	//size += sprintf(buf+size,"mdv1=%d\r\n", sys_info->status.motion_cpercentage);
#if GIOIN_NUM >= 1
	size += sprintf(buf+size,"input1=%s\r\n", sys_info->status.alarm1? "on":"off");
#endif
#if GIOIN_NUM >= 2
	size += sprintf(buf+size,"input2=%s\r\n", sys_info->status.alarm2? "on":"off");
#endif
#if GIOIN_NUM >= 3
	size += sprintf(buf+size,"input3=%s\r\n", sys_info->status.alarm3? "on":"off");
#endif
#if GIOIN_NUM >= 4
	size += sprintf(buf+size,"input4=%s\r\n", sys_info->status.alarm4? "on":"off");
#endif
#if GIOIN_NUM >= 5
	size += sprintf(buf+size,"input5=%s\r\n", sys_info->status.alarm5? "on":"off");
#endif
#if GIOIN_NUM >= 6
	size += sprintf(buf+size,"input6=%s\r\n", sys_info->status.alarm6? "on":"off");
#endif
#if GIOIN_NUM >= 7
	size += sprintf(buf+size,"input7=%s\r\n", sys_info->status.alarm7? "on":"off");
#endif
#if GIOIN_NUM >= 8
	size += sprintf(buf+size,"input8=%s\r\n", sys_info->status.alarm8? "on":"off");
#endif

	//size += sprintf(buf+size,"storagefull=%s\r\n", str[0]);
	//size += sprintf(buf+size,"storagefail=%s\r\n", str[0]);
	size += sprintf(buf+size,"recording=%s\r\n", sys_info->status.on_record? "on":"off");
	//size += sprintf(newbuf+size,"snapshooting=%s\r\n", str[0]);
	//size += sprintf(buf+size,"mdetecting=%s\r\n", sys_info->config.motion_config.motionenable? "on":"off");
	size += sprintf(buf+size,"mdetecting=%s\r\n", sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8 ? "on":"off");
	size += sprintf(buf+size,"output1=%s\r\n", sys_info->status.alarm_out1? "on":"off");
	size += sprintf(buf+size,"vsignal=on\r\n");
	//size += sprintf(buf+size,"speaker=%s\r\n", str[0]);
	//size += sprintf(buf+size,"mic=%s\r\n", str[0]);

	send_request_dlink_ok(req, size);
	req->buffer_end += sprintf((void *)req_bufptr(req), buf); 
	
	req->status = DONE;
	return 1;
}
int uri_notify_stream(request * req)
{
	req->http_stream = URI_NOTIFY_STREAM;
	req->status = WRITE;
	
	req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\n");
    req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
    req_write(req, "Pragma: no-cache\r\n");
	req_write(req, "Content-type: text/plain\r\n");
  	req_write(req, "\r\n");
	

	//strcpy(req->nipca_notify_buf, buf);
	req->nipca_notify_time = sys_info->second;
	req->nipca_notify_stream = 1;	
	return 1;
}
#endif//tmp jiahung NIPCA

int uri_para_info(request * req)
{

	int i = 0, j = 0, idx = 0;
	send_request_ok_no_length(req);
	SQUASH_KA(req);
	
#ifdef SUPPORT_AGC
		req->buffer_end += sprintf((void *)req_bufptr(req), "agc=");
		for(i=0;i<MAX_AGC;i++){
			req->buffer_end += sprintf((void *)req_bufptr(req), "%s", sys_info->api_string.agc[i]);
			
			if(i < MAX_AGC-1)
				req->buffer_end += sprintf((void *)req_bufptr(req), ",");
		}
		req->buffer_end += sprintf((void *)req_bufptr(req), "\r\n");
#else
		dbg("no supportagc");
#endif
	//if(sys_info->config.support_config.supportawb){
	/*if(sys_info->ipcam[SUPPORTBLCMODE].config.u8){
		req->buffer_end += sprintf((void *)req_bufptr(req), "blcmode=%d\r\n", sys_info->ipcam[NBACKLIGHTCONTROL].config.u8);
	}else
		dbg("no supportblc");*/

#ifdef SUPPORT_AWB
		req->buffer_end += sprintf((void *)req_bufptr(req), "whitebalance=");
		for(i=0;i<MAX_AWB;i++){
			req->buffer_end += sprintf((void *)req_bufptr(req), "%s", sys_info->api_string.awb[i]);
			
			if(i < MAX_AWB-1)
				req->buffer_end += sprintf((void *)req_bufptr(req), ",");
		}
		req->buffer_end += sprintf((void *)req_bufptr(req), "\r\n");
#else
		dbg("no supportawb");
#endif
#ifdef SUPPORT_BRIGHTNESS	
		#if defined(CONFIG_BRAND_DLINK) && defined(SENSOR_USE_IMX035)
			req->buffer_end += sprintf((void *)req_bufptr(req), "brightness=0...8\n");
		#else
			req->buffer_end += sprintf((void *)req_bufptr(req), "brightness=0...%d\n", MAX_BRIGHTNESS);
		#endif
#else 
		dbg("no supportbrightness");
#endif	
#ifdef SUPPORT_CONTRAST	
		#if defined(CONFIG_BRAND_DLINK) && defined(SENSOR_USE_IMX035)
			req->buffer_end += sprintf((void *)req_bufptr(req), "contrast=0...8\n");
		#else
			req->buffer_end += sprintf((void *)req_bufptr(req), "contrast=0...%d\n", MAX_CONTRAST);
		#endif
#else 
		dbg("no supportcontrast");
#endif	
#ifdef SUPPORT_SATURATION	
		req->buffer_end += sprintf((void *)req_bufptr(req), "saturation=0...%d\n", MAX_SATURATION);
#else 
		dbg("no supportsaturation");
#endif
#ifdef SUPPORT_SHARPNESS
		#if defined(CONFIG_BRAND_DLINK) && defined(SENSOR_USE_IMX035)
			req->buffer_end += sprintf((void *)req_bufptr(req), "sharpness=0...8\n");
		#else
			req->buffer_end += sprintf((void *)req_bufptr(req), "sharpness=0...%d\n", MAX_SHARPNESS);
		#endif
#else 
		dbg("no supportsharpness");
#endif
/*
	if(sys_info->ipcam[SUPPORTHUE].config.u8)
		req->buffer_end += sprintf((void *)req_bufptr(req), "hue=0...%d\n", MAX_HUE);
	else 
		dbg("no supporthue");
	*/
#ifdef SUPPORT_WDRLEVEL
		req->buffer_end += sprintf((void *)req_bufptr(req), "wdrlevel=%d...%d\n", MIN_WDRLEVEL, MAX_WDRLEVEL);
#else 
		dbg("no supportwdrlevel");
#endif	
#if defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 ) //|| defined ( SENSOR_USE_MT9V136)
			idx = 2;
#else
			idx = 0;
#endif

#if defined (SENSOR_USE_IMX035) || defined(SENSOR_USE_OV2715) || defined(SENSOR_USE_IMX036)
	for(i=1; i<=MAX_WEB_PROFILE_NUM;i++){
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dformat=%s,%s,%s\n", i, JPEG_PARSER_NAME, MPEG4_PARSER_NAME, H264_PARSER_NAME);
	
#if 0	//tmp jiahung
	#if defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
		if (sys_info->video_type == 0)
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s\n", i, RESOLUTION_7, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
		else
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s\n", i, RESOLUTION_7, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);		
	#elif defined(RESOLUTION_IMX036_3M)
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
	#elif defined(RESOLUTION_TYCO_3M)
		if (sys_info->video_type == 0)
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
		else
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_7, RESOLUTION_8, RESOLUTION_9);
	#else
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5);
	#endif
#endif
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dbps=", i);
				for(j=idx;j<MAX_MP4_BITRATE;j++){
					req->buffer_end += sprintf((void *)req_bufptr(req), "%s", sys_info->api_string.mp4_bitrate[j]);
					
					if(j < MAX_MP4_BITRATE-1)
						req->buffer_end += sprintf((void *)req_bufptr(req), ",");
				}
				req->buffer_end += sprintf((void *)req_bufptr(req), "\r\n");

		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dquality=0...4\r\n", i);
	}
#elif defined(SENSOR_USE_MT9V136)

	for(i=1; i<=MAX_WEB_PROFILE_NUM;i++){
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dformat=%s,%s,%s\n", i, JPEG_PARSER_NAME, MPEG4_PARSER_NAME, H264_PARSER_NAME);
		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
		
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dbps=", i);
				for(j=idx;j<MAX_MP4_BITRATE;j++){
					req->buffer_end += sprintf((void *)req_bufptr(req), "%s", sys_info->api_string.mp4_bitrate[j]);
					
					if(j < MAX_MP4_BITRATE-1)
						req->buffer_end += sprintf((void *)req_bufptr(req), ",");
				}
				req->buffer_end += sprintf((void *)req_bufptr(req), "\r\n");
		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dquality=0...4\r\n", i);
	}	
#if 0//SUPPORT_MG1264
	//if(sys_info->config.lan_config.net.supportavc == 1){
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dformat=%s\n", i,H264_PARSER_NAME);
		if(sys_info->video_type == 0)
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s\n", i,RESOLUTION_4, RESOLUTION_7);
		else
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s\n", i, RESOLUTION_5, RESOLUTION_6);
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dbps=%s,%s,%s,%s\n", 
			i, AVC_FRAMERATE_1, AVC_FRAMERATE_2 ,AVC_FRAMERATE_3 ,AVC_FRAMERATE_4);
		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile4quality=0...5\r\n");
	//}
#endif
#elif defined (SENSOR_USE_TVP5150) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined (SENSOR_USE_TVP5150_MDIN270)

	for(i=1; i<=MAX_WEB_PROFILE_NUM;i++){
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dformat=%s,%s,%s\n", i, JPEG_PARSER_NAME, MPEG4_PARSER_NAME, H264_PARSER_NAME);
		/*if(sys_info->video_type == 0)
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s\n", i, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
		else
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s,%s\n", i, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
		*/	
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dbps=", i);
				for(j=idx;j<MAX_MP4_BITRATE;j++){
					req->buffer_end += sprintf((void *)req_bufptr(req), "%s", sys_info->api_string.mp4_bitrate[j]);
					
					if(j < MAX_MP4_BITRATE-1)
						req->buffer_end += sprintf((void *)req_bufptr(req), ",");
				}
				req->buffer_end += sprintf((void *)req_bufptr(req), "\r\n");
		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dquality=0...4\r\n", i);
	}
#if 0//SUPPORT_MG1264
	//if(sys_info->config.lan_config.net.supportavc == 1){
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dformat=%s\n", i,H264_PARSER_NAME);
		if(sys_info->video_type == 0)
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s\n", i,RESOLUTION_1, RESOLUTION_2);
		else
			req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dresolution=%s,%s\n", i, RESOLUTION_4, RESOLUTION_5);
		req->buffer_end += sprintf((void *)req_bufptr(req), "profile%dbps=%s,%s,%s,%s\n", 
			i, AVC_FRAMERATE_1, AVC_FRAMERATE_2 ,AVC_FRAMERATE_3 ,AVC_FRAMERATE_4);
		//req->buffer_end += sprintf((void *)req_bufptr(req), "profile4quality=0...5\r\n");
	//}
#endif

#else
	dbg("UNKNOWN SENSOR");
#endif

	req->status = DONE;
	return 1;
}
int uri_api_string(request * req)
{
	//debug api_string
	int i;
	send_request_ok_no_length(req);
	SQUASH_KA(req);
	
	req->buffer_end += sprintf( req_bufptr( req ),  "resolution ");
	for(i=0;i<10;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.resolution_list[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "mp4_gov ");
	for(i=0;i<MAX_MP4_GOV;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.mp4_gov[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "mp4_bitrate ");
	for(i=0;i<MAX_MP4_BITRATE;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.mp4_bitrate[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "awb ");
	for(i=0;i<MAX_AWB;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.awb[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "agc ");
	for(i=0;i<MAX_AGC;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.agc[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "exposure ");
	for(i=0;i<20;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.exposure_time[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");
	
	req->buffer_end += sprintf( req_bufptr( req ),  "minshutter ");
	for(i=0;i<20;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.minshutter[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");
	
	req->buffer_end += sprintf( req_bufptr( req ),	"maxshutter ");
		for(i=0;i<20;i++)
			req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.maxshutter[i]);
		req->buffer_end += sprintf( req_bufptr( req ),	"\n");

	req->buffer_end += sprintf( req_bufptr( req ),  "audioingain ");
	for(i=0;i<MAX_AUDIOINGAIN;i++)
		req->buffer_end += sprintf( req_bufptr( req ), "[%d]=%s ", i , sys_info->api_string.audioingain[i]);
	req->buffer_end += sprintf( req_bufptr( req ),  "\n");
	
	req->status = DONE;
	return 1;
}
int uri_shutter(request * req)
{
#ifndef SUPPORT_EXPOSURETIME
	//OPTION_NS
	return 0;
#endif
	int i, id=0, max_idx=-1, min_idx=-1;
	char* max=NULL;
	char* min=NULL;
	
	req->is_cgi = CGI;
	SQUASH_KA(req);

	if(EXPOSURETYPE != EXPOSUREMODE_SELECT){
		//OPTION_NS
		return 0;
	}
	
	if ( uri_decoding(req, req->query_string) < 0) {
		if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1)){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "min=%s\r\n", conf_string_read(MINSHUTTER1));
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "max=%s\r\n", conf_string_read(MAXSHUTTER1));
		}else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2)){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "min=%s\r\n", conf_string_read(MINSHUTTER2));
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "max=%s\r\n", conf_string_read(MAXSHUTTER2));
		}else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3)){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "min=%s\r\n", conf_string_read(MINSHUTTER3));
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "max=%s\r\n", conf_string_read(MAXSHUTTER3));
		}else
			DBG("UNKNOWN EXPOSURE_MODE(%s)", conf_string_read(EXPOSURE_MODE));

		send_request_ok_api(req);
		req->status = DONE;
		return 1;
	}

	if(strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1) && 
		strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2)&&
		strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3)){
		dbg("EXPOSURE_MODE = %s", conf_string_read(EXPOSURE_MODE));
		return 0;
	}

	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "max") == 0){
			max = req->cmd_arg[i].value;
		}
		if(strcmp(req->cmd_arg[i].name, "min") == 0){
			min = req->cmd_arg[i].value;
		}
	}

	if( (!max) || (!min) ) return 0;

	if( !strcmp( conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1 ) )
		id = 0;
	else if( !strcmp( conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2 ) )
		id = 1;
	else if( !strcmp( conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3 ) )
		id = 2;
	else{
		dbg("id error");
		return 0;
	}

	for(i=0;i<MAX_EXPOSURETIME;i++){
		if( !strcmp( sys_info->api_string.maxshutter[i], max ) )
			max_idx = i;
		if( !strcmp( sys_info->api_string.minshutter[i], min ) )
			min_idx = i;
	}

	if( (max_idx==-1) || (min_idx==-1) )	return -1;
			
	dbg("max=%d, min=%d", max_idx, min_idx);

	if(max_idx < min_idx){
		dbg("max < min");
		send_request_ok_api(req);
		req->buffer_end += sprintf( req_bufptr( req ),  "NG shutter=1\r\n");
		req->status = DONE;
		return 1;
	}

	ControlSystemData(SFIELD_SET_MAXSHUTTER, (void *)max, strlen(max)+1, &req->curr_user);
	ControlSystemData(SFIELD_SET_MINSHUTTER, (void *)min, strlen(min)+1, &req->curr_user);
	
	send_request_ok_api(req);
	req->status = DONE;
	return 1;
}
int check_exposure_schedule_time(EXPOSURE_TIME_EX set, EXPOSURE_TIME_EX base)
{
	int set_strart = (set.start.nHour*60)+set.start.nMin;
	int set_end = (set.end.nHour*60)+set.end.nMin;
	int base_strart = (base.start.nHour*60)+base.start.nMin;
	int base_end = (base.end.nHour*60)+base.end.nMin; 
	int i, idx;

	if((set.enable == 0) || (base.enable == 0)) 
		return 0;
	
	if(set_strart == set_end)
		return -1;
	
	for(i=0;i<(EXPOSURE_EX_MAX_NUM-1);i++){
		
		idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
		
		if( (base_strart < base_end) ){
			if((set_strart >= base_strart) && (set_strart <= base_end) ){
				dbg("strart_time over");
				return -1;
			}
			if((set_end >= base_strart) && (set_strart <= base_end) ){
				dbg("end_time over");
				return -1;
			}
		}else if((base_strart > base_end) && (set_strart < set_end)){
			if( (set_strart >= base_strart) || (set_strart <= base_end)){
				dbg("===strart_time over===");
				return -1;
			}
			
			if( (set_end >= base_strart) || (set_end <= base_end)){
				dbg("===end_time over===");
				return -1;
			}
		}else{
			dbg("check_exposure_schedule_time overrange");
			return -1;
		}
	}

	return 0;
}
int load_exposure_schedule(char* schedule, EXPOSURE_TIME_EX* setting)
{
	char *ptr = NULL;
	int i, id;
	
	ptr = schedule;
	for(i = 0; i < 3 ; i++) {
		if( (ptr = strchr(ptr+1, ':')) == NULL ) {
			dbg("Not found ':'");
			return -1;
		}
	}
	
	*ptr = ' ';
	dbg("schedule = %s", schedule);
	
	if(sscanf(schedule, "%d:%d:%s %02d%02d:%02d%02d", &id,
			&setting->enable, setting->value,
			&setting->start.nHour, &setting->start.nMin,
			&setting->end.nHour, &setting->end.nMin) != 7) {
		dbg("schedule format not correct!! ");
		return -1;
	}

	if(id < 0 || id > EXPOSURE_EX_MAX_NUM) {
		dbg("schedule id(%d) is over range !!", id);
		return -1;
	}
#if 0
	dbg("setting.enable = %d", setting->enable);
	dbg("setting.value = %s", setting->value);
	dbg("setting.start.nHour = %d", setting->start.nHour);
	dbg("setting.start.nMin = %d", setting->start.nMin);
	dbg("setting.end.nHour = %d", setting->end.nHour);
	dbg("setting.end.nMin = %d", setting->end.nMin);
#endif		
	return 0;
}
int uri_expoure(request * req)
{
#ifndef SUPPORT_EXPOSURETIME
		//OPTION_NS
		return 0;
#endif
		int i, j, idx=0;
		char* mode= NULL;
		char* schedule= NULL;
		EXPOSURE_TIME_EX setting[EXPOSURE_EX_MAX_NUM];
		
		req->is_cgi = CGI;
		SQUASH_KA(req);
	
		if(EXPOSURETYPE != EXPOSUREMODE_SELECT){
			//OPTION_NS
			return 0;
		}
#if 1		
		uri_decoding(req, req->query_string);
#else		
		if ( uri_decoding(req, req->query_string) < 0) {
			for(i = 0; i < EXPOSURE_EX_MAX_NUM; i++) {
				idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "schedule=%d:%d:%s:%02d%02d:%02d%02d\n", 
					 i, 
					sys_info->ipcam[EXPOSURE_SCHEDULE0_EN+idx].config.value,
					conf_string_read(EXPOSURE_SCHEDULE0_MODE+idx),
					sys_info->ipcam[EXPOSURE_SCHEDULE0_START_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_START_MIN+idx].config.value,
					sys_info->ipcam[EXPOSURE_SCHEDULE0_END_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_END_MIN+idx].config.value);
			}
	
			send_request_ok_api(req);
			req->status = DONE;
			return 1;
		}
#endif	
		if(strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_SCHEDULE) != 0)
			return 0;

		for(i = 0 ; i <= (req->cmd_count-1) ; i++){
			if(strcmp(req->cmd_arg[i].name, "mode") == 0){
				mode = req->cmd_arg[i].value;
			}
			if(strcmp(req->cmd_arg[i].name, "schedule") == 0){
				schedule = req->cmd_arg[i].value;
				idx = atoi(schedule);
				dbg("idx=%d", idx);
				if(load_exposure_schedule(schedule, &setting[idx]) >= 0)
					setting[idx].valid = 1;
			}
		}
		//dbg("mode = %s", mode);
		//dbg("schedule = %s", schedule);

#if 0 //debug
	for(i=0;i<EXPOSURE_EX_MAX_NUM;i++){
		if(setting[i].valid == 1){
			dbg("setting[%d].enable = %d", i, setting[i].enable);
			dbg("setting[%d].value = %s", i, setting[i].value);
			dbg("setting[%d].start.nHour = %d", i, setting[i].start.nHour);
			dbg("setting[%d].start.nMin = %d", i, setting[i].start.nMin);
			dbg("setting[%d].end.nHour = %d", i, setting[i].end.nHour);
			dbg("setting[%d].end.nMin = %d", i, setting[i].end.nMin);
		}
	}
#endif
	for(i=0;i<EXPOSURE_EX_MAX_NUM;i++){
		for(j=0;j<EXPOSURE_EX_MAX_NUM;j++){
			
			if(i == (EXPOSURE_EX_MAX_NUM-1) || j == (EXPOSURE_EX_MAX_NUM-1) || i == j)	continue;
			
			if(setting[i].valid == 1){
				if(check_exposure_schedule_time(setting[i], setting[j]) < 0){
					dbg("NG Schedule=1");
					send_request_ok_api(req);
					req->buffer_end += sprintf( req_bufptr( req ),	"NG Schedule=1\r\n");
					req->status = DONE;
					return 1;
				}
			}
		}
	}

	for(i=0;i<EXPOSURE_EX_MAX_NUM;i++){
		if(setting[i].valid == 1){
			
			dbg("save exposure schedule");
			idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
			info_write(&setting[i].enable, EXPOSURE_SCHEDULE0_EN+idx, 0, 0);
			info_write(setting[i].value, EXPOSURE_SCHEDULE0_MODE+idx, HTTP_OPTION_LEN, 0);
			info_write(&setting[i].start.nHour, EXPOSURE_SCHEDULE0_START_HOUR+idx, 0, 0);
			info_write(&setting[i].start.nMin, EXPOSURE_SCHEDULE0_START_MIN+idx, 0, 0);
			info_write(&setting[i].end.nHour, EXPOSURE_SCHEDULE0_END_HOUR+idx, 0, 0);
			info_write(&setting[i].end.nMin, EXPOSURE_SCHEDULE0_END_MIN+idx, 0, 0);
		}
		setting[i].valid = 0;
	}

	send_request_ok_api(req);
	req->status = DONE;
	return 1;

}

int uri_appro_audio(request * req)
{
	req->http_stream = URI_AUDIO_STREAM;
	req->status = WRITE;
	req->busy_flag |= BUSY_FLAG_STREAM;
	//if(sys_info->config.lan_config.audiooutenable == 0) {
	if(sys_info->ipcam[AUDIOOUTENABLE].config.u8 == 0) {
		send_r_error(req);
		return 0;
	}
			
	if (get_audio_data(&req->audio, AV_OP_SUB_STREAM_1) != RET_STREAM_SUCCESS) {
		send_r_error(req);
		return 0;
	}
	
	req->data_mem = req->audio.av_data.ptr;
	req->filesize = req->audio.av_data.size;
	req->filepos = 0;
	send_request_ok_audio(req);
	print_audio_headers(req);

	return 1;
}

int eptzpreset_add(EPTZPRESET eptzpreset)
{
	int i, tmp;
	int page = 0, add_idx = 0;
	char string_cmp[32];
	char buf[256];

	dbg("streamid = %d", eptzpreset.streamid);
	dbg(" seq_idx = %d", eptzpreset.index);
	dbg("seq_time = %d", eptzpreset.seq_time);
	dbg("    zoom = %d", eptzpreset.zoom);
	dbg("   focus = %d", eptzpreset.focus);
	dbg("position = %d", eptzpreset.position);
	dbg("    name = %s", eptzpreset.name);

	page = (eptzpreset.streamid-1)*20;
	
	dbg("check space");
	
	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*string_cmp = '\0';
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, string_cmp);
			
		//dbg("string_cmp=%s.", string_cmp);
		if(strcmp(string_cmp, eptzpreset.name) == 0){
			return 1;
		}
	}
	dbg("add preset");
	
	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*string_cmp = '\0';

		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, string_cmp);
		//dbg("string_cmp=%s.", string_cmp);
		if(strcmp(string_cmp, "") == 0){
			disable_sequence_autopan(eptzpreset.streamid);
			add_idx = i+1;
			//dbg("index = %d", index);
			sprintf(buf, "%d:%d:%d:%d:%d:%08d:%s", eptzpreset.streamid, add_idx, eptzpreset.seq_time, eptzpreset.zoom, eptzpreset.focus, eptzpreset.position, eptzpreset.name);
			dbg("buf = %s", buf);
			dbg("write stream%d", eptzpreset.streamid);
			info_write(buf, (EPTZPRESET_STREAM1_ID01+page)+i, strlen(buf), 0);
			break;
		}
	}
	return 0;
}
int eptzpreset_del(EPTZPRESET eptzpreset)
{
	int i, j, tmp;
	int page = 0;
	char string_cmp[32];
	char buf[256];
	int del_buf_idx = -1;
	int streamid, index;
	
	dbg("streamid = %d", eptzpreset.streamid);
	dbg(" seq_idx = %d", eptzpreset.index);
	dbg("seq_time = %d", eptzpreset.seq_time);
	dbg("    zoom = %d", eptzpreset.zoom);
	dbg("   focus = %d", eptzpreset.focus);
	dbg("position = %d", eptzpreset.position);
	dbg("    name = %s", eptzpreset.name);
	
	dbg("del");
	page = (eptzpreset.streamid-1)*20;
	
	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*string_cmp = '\0';
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &streamid, &index, &tmp, &tmp, &tmp, &tmp, string_cmp);
		dbg("<%d>%s,%s.", i, string_cmp,  eptzpreset.name);
		if(strcmp(string_cmp,  eptzpreset.name) == 0){
			dbg("Delete %d", i);
			del_buf_idx = i;
			
			eptzpreset.streamid = 0;
			eptzpreset.index = 0;
			eptzpreset.seq_time = 10;
			eptzpreset.zoom = 0;
			eptzpreset.focus = 0;
			eptzpreset.position = 0;
			strcpy(eptzpreset.name, "");
				
			sprintf(buf, "%d:%d:%d:%d:%d:%08d:%s", eptzpreset.streamid, eptzpreset.index, eptzpreset.seq_time, eptzpreset.zoom, eptzpreset.focus, eptzpreset.position, eptzpreset.name);
			//strcpy(buf, sys_info->ipcam[EPTZPRESET_STREAM1_ID01].def_value.string);
			
			dbg("buf = %s", buf);
			dbg("write stream%d", eptzpreset.streamid);
			info_write(buf, (EPTZPRESET_STREAM1_ID01+page)+i, strlen(buf), 0);
			break;
		}

	}
	
	if(del_buf_idx == -1)	return 0;
	
	#if 1
	for(i=0;i<MAX_SEQUENCE;i++){
		if(index == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
			disable_sequence_autopan(streamid);
			
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value = 0;
			sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value = EPTZPRESET_SEQ_TIME_DEF;
			for(j=i;j<MAX_SEQUENCE;j++){
				sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+j].config.value = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(j+1)].config.value;
				sys_info->ipcam[(SEQUENCE1_TIME01+page)+j].config.value = sys_info->ipcam[(SEQUENCE1_TIME01+page)+(j+1)].config.value;
			}
		}
		
	}
	/*
	for(i=0;i<MAX_SEQUENCE;i++){
		dbg("(%d)<%d, %d>", i, sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value, sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(i+1)].config.value);
		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value == 0){
			for(j=i;j<MAX_SEQUENCE;j++){
				if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+j].config.value != 0){
					sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(i+1)].config.value;
					sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value = sys_info->ipcam[(SEQUENCE1_TIME01+page)+(i+1)].config.value;
					sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(i+1)].config.value = 0;
					sys_info->ipcam[(SEQUENCE1_TIME01+page)+(i+1)].config.value = EPTZPRESET_SEQ_TIME_DEF;
					break;
				}
			}
		}
	}*/
	for(i=0;i<MAX_SEQUENCE;i++){
		if(index < sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value --;
			
		}
	}
	#else //clean all
	for(i=0;i<MAX_SEQUENCE;i++){
		if(index == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
			disable_sequence_autopan(streamid);
			for(i=0;i<MAX_SEQUENCE;i++){
				sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value = 0;
				sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value = EPTZPRESET_SEQ_TIME_DEF;
			}
			break;
		}
	}
	#endif
	
	for(i=(del_buf_idx+1);i<MAX_EPTZPRESET;i++)
	{
		*eptzpreset.name = '\0';
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &eptzpreset.streamid, &eptzpreset.index, &eptzpreset.seq_time, &eptzpreset.zoom, &eptzpreset.focus, &eptzpreset.position, eptzpreset.name);
	
		if(strcmp(eptzpreset.name, "") != 0){
			dbg("%d to %d", i, del_buf_idx);
			
			sprintf(buf, "%d:%d:%d:%d:%d:%08d:%s", eptzpreset.streamid, eptzpreset.index-1, eptzpreset.seq_time, eptzpreset.zoom, eptzpreset.focus, eptzpreset.position, eptzpreset.name);
			strcpy(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+del_buf_idx), buf);
			
			del_buf_idx++;
			
			memset(buf, 0, sizeof(buf));
			strcpy(buf, EPTZPRESET_DEFAULT);
			dbg("clr to %d", i);
			info_write(buf, (EPTZPRESET_STREAM1_ID01+page)+i, strlen(buf), 0);
		}
	}
	
	return 0;
}
char ret_buf[16]; 

int eptzpreset_goto(EPTZPRESET eptzpreset, char* direction, char* name, request * req)
{
	int i;
	int page, profile_page;
	char string_cmp[32];
	//char buf[256];
	//int autofocus = 1, location = 5;
	int xsize, ysize;
	char gotopoint[16];
	int speed = 2;
	int goto_xsize, goto_ysize;
	
	dbg("streamid = %d", eptzpreset.streamid);
	dbg(" seq_idx = %d", eptzpreset.index);
	dbg("seq_time = %d", eptzpreset.seq_time);
	dbg("    zoom = %d", eptzpreset.zoom);
	dbg("   focus = %d", eptzpreset.focus);
	//dbg("position = %d", eptzpreset.position);//NC
	dbg("    name = %s\n", eptzpreset.name);
	dbg("direction = %s", direction);//direction or point
	dbg("     dest = %s", name);
	
	page = (eptzpreset.streamid-1)*20;
	profile_page = (PROFILE_STRUCT_SIZE*(eptzpreset.streamid-1));
	sscanf(conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE+profile_page), "%04d%04d", &xsize, &ysize);	
	
	if(strcmp(direction, "home") == 0){
		sscanf(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE+profile_page), "%dx%d", &xsize, &ysize);
		dbg("EPTZWINDOW_XSIZE = %d", xsize);
		dbg("EPTZWINDOW_YSIZE = %d", ysize);
		xsize = (sys_info->ipcam[PROFILESIZE0_XSIZE+(2*(eptzpreset.streamid-1))].config.value / 2) - (xsize/2);
		ysize = (sys_info->ipcam[PROFILESIZE0_YSIZE+(2*(eptzpreset.streamid-1))].config.value / 2) - (ysize/2);
	}else if(strcmp(direction, "up") == 0){
		ysize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "down") == 0){
		ysize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "right") == 0){
		xsize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "left") == 0){
		xsize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "right_up") == 0){
		xsize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
		ysize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "right_down") == 0){
		xsize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
		ysize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "left_up") == 0){
		xsize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
		ysize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "left_down") == 0){
		xsize -= sys_info->ipcam[EPTZ_SPEED].config.value*speed;
		ysize += sys_info->ipcam[EPTZ_SPEED].config.value*speed;
	}else if(strcmp(direction, "point") == 0){
		for(i=0;i<MAX_EPTZPRESET;i++)
		{
			*string_cmp = '\0';
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &eptzpreset.streamid, &eptzpreset.index, &eptzpreset.seq_time, &eptzpreset.zoom, &eptzpreset.focus, &eptzpreset.position, string_cmp);
			if(strcmp(string_cmp, name) == 0){
				sprintf(gotopoint, "%01d%08d", eptzpreset.streamid, eptzpreset.position);
				dbg("gotopoint=%s\n", gotopoint);
				
				if(ControlSystemData(SFIELD_SET_EPTZ_COORDINATE, gotopoint, strlen(gotopoint)+1, &req->curr_user) < 0){
					dbg("SFIELD_SET_EPTZ_COORDINATE error");
					return -1;
				}
				
				goto_xsize = eptzpreset.position/10000;
				goto_ysize = eptzpreset.position%10000;
				
				if(goto_xsize > sys_info->eptz_status[eptzpreset.streamid-1].xmove_limit){
					DBG("over X range<%d>(lim=%d)", goto_xsize, sys_info->eptz_status[eptzpreset.streamid-1].xmove_limit);
					goto_xsize = sys_info->eptz_status[eptzpreset.streamid-1].xmove_limit;
				}
				if(goto_ysize > sys_info->eptz_status[eptzpreset.streamid-1].ymove_limit){
					DBG("over Y range<%d>(lim=%d)", goto_ysize, sys_info->eptz_status[eptzpreset.streamid-1].ymove_limit);
					goto_ysize = sys_info->eptz_status[eptzpreset.streamid-1].ymove_limit;
				}
				
				//sprintf(buf, "OK goto=%08d", eptzpreset.position);
				sprintf(ret_buf, "OK goto=%04d%04d", goto_xsize, goto_ysize);
				dbg("ret_buf=%s", ret_buf);
				//disable optics focus
				//ControlSystemData(SFIELD_SET_AF_ZOOM_POSITION, &eptzpreset.zoom, sizeof(eptzpreset.zoom), &req->curr_user);
				//ControlSystemData(SFIELD_SET_AF_FOCUS_POSITION, &eptzpreset.focus, sizeof(eptzpreset.focus), &req->curr_user);

			}
		}
		return 0;
	}else{
		dbg("direction error");
		return -1;
	}

	dbg("size<%d, %d>", xsize, ysize);
	sprintf(gotopoint, "%01d%04d%04d", eptzpreset.streamid, xsize, ysize);
	sprintf(ret_buf, "OK goto=%04d%04d", xsize, ysize);
	dbg("gotopoint=%s\n", gotopoint);
		
	if(ControlSystemData(SFIELD_SET_EPTZ_COORDINATE, gotopoint, strlen(gotopoint)+1, &req->curr_user) < 0){
		dbg("SFIELD_SET_EPTZ_COORDINATE error");
		return -1;
	}
	
	return 0;
}
void disable_sequence_autopan(int streamid)
{
	if(streamid == 1){
		sys_info->status.auto_pan &= ~AUTO_PAN_ID01;
		sys_info->status.eptzpreset_seq &= ~SEQ_ID01;
	}else if(streamid == 2){
		sys_info->status.auto_pan &= ~AUTO_PAN_ID02;
		sys_info->status.eptzpreset_seq &= ~SEQ_ID02;
	}else if(streamid == 3){
		sys_info->status.auto_pan &= ~AUTO_PAN_ID03;
		sys_info->status.eptzpreset_seq &= ~SEQ_ID03;
	}else
		dbg("streamid error");
		
}
int uri_eptzpreset(request * req)
{
#ifndef SUPPORT_EPTZ
	return 0;
#endif
	EPTZPRESET eptzpreset;

	char buf[256];
	int i;
	char* action = NULL;
	char name[32];
	char rename[32];
	int  streamid = 0, index = 0, position = 0;
	int tmp;
	char string_cmp[32];
	int page = 0;
	int seq_idx = 0, seq_time = 10;
	int zoom = sys_info->ipcam[AUTOFOCUS_ZOOM_POSTITION].config.value, focus = sys_info->ipcam[AUTOFOCUS_FOCUS_POSTITION].config.value;
	int speed = 10;
	char direction[16];
	int xchange;
	
	req->is_cgi = CGI;
	SQUASH_KA(req);
	
	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "action") == 0){
				action = req->cmd_arg[i].value;
		}
  		if(strcmp(req->cmd_arg[i].name, "streamid") == 0){
  				streamid = atoi(req->cmd_arg[i].value);
				if(sys_info->ipcam[EPTZ_STREAMID].config.value != streamid)
					info_write(&streamid, EPTZ_STREAMID, 0, 0);
  		}
  		if(strcmp(req->cmd_arg[i].name, "index") == 0)
  				seq_idx = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "name") == 0)
  				strcpy(name, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "rename") == 0)
  				strcpy(rename, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "direction") == 0)
  				strcpy(direction, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "time") == 0)
  				seq_time = atoi(req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "speed") == 0)
  				speed = atoi(req->cmd_arg[i].value)*5;
		if(strcmp(req->cmd_arg[i].name, "cleanseq") == 0){
  			for(i=0;i<MAX_SEQUENCE;i++){
				if(index == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
					disable_sequence_autopan(streamid);
					for(i=0;i<MAX_SEQUENCE;i++){
						sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value = 0;
						sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value = EPTZPRESET_SEQ_TIME_DEF;
					}
						break;
				}
			}
		}
	}
	
	if(!action)	action = "NONE";
	
	dbg("action = %s", action);
	dbg("streamid = %d", streamid);
	dbg("index = %d", seq_idx);
	dbg("name = %s", name);
	dbg("seq_time = %d", seq_time);
	
	eptzpreset.streamid = streamid;
	eptzpreset.index = seq_idx;
	strcpy(eptzpreset.name, name);
	position = atoi(conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE+((streamid-1)*PROFILE_STRUCT_SIZE)));
	dbg("position = %d", position);
	eptzpreset.position = position;
	eptzpreset.seq_time = seq_time; //sysenv.dat(seq_time) no used
	eptzpreset.zoom = sys_info->ipcam[AUTOFOCUS_ZOOM_POSTITION].config.value;	//eptzpreset.zoom no used
	eptzpreset.focus = sys_info->ipcam[AUTOFOCUS_FOCUS_POSTITION].config.value;	//eptzpreset.focus no used

	//check range
	if( streamid > MAX_WEB_PROFILE_NUM) goto EPTZPRESET_END;//return 0;
	if( (seq_idx+1) > MAX_SEQUENCE) goto EPTZPRESET_END;//return 0;
	if( seq_time < MIN_SEQ_TIME || seq_time > MAX_SEQ_TIME) goto EPTZPRESET_END;//return 0;
	if( strlen(eptzpreset.name) > MAX_EPTZPRESETNAME_LEN) goto EPTZPRESET_END;//return 0;
	if( strlen(rename) > MAX_EPTZPRESETNAME_LEN) goto EPTZPRESET_END;//return 0;
	
	if(strcmp(action, "add") == 0){
		if(eptzpreset_add(eptzpreset) == 1){
			dbg("name exist");
			send_request_ok_api(req);
			req->status = DONE;
			return 1;
		}	
	}else if(strcmp(action, "del") == 0){
		eptzpreset_del(eptzpreset);
		
	}else if(strcmp(action, "modify") == 0){
		dbg("rename");
		streamid = sys_info->ipcam[EPTZ_STREAMID].config.value;
		page = (streamid-1)*20;
		disable_sequence_autopan(streamid);
		
		for(i=0;i<MAX_EPTZPRESET;i++)
		{
			*string_cmp = '\0';
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &streamid, &index, &seq_time, &zoom, &focus, &position, string_cmp);
			//dbg("string_cmp=%s......name=%s==", string_cmp, name);

			if(strcmp(string_cmp, name) == 0){
				//dbg("i = %d", i);
				
				sprintf(buf, "%d:%d:%d:%d:%d:%08d:%s", streamid, index, seq_time, zoom, focus, position, rename);
				dbg("buf = %s", buf);
				dbg("write stream%d", streamid);
				info_write(buf, (EPTZPRESET_STREAM1_ID01+page)+i, strlen(buf), 0);
				break;
			}
		}
	}else if(strcmp(action, "goto") == 0){
		disable_sequence_autopan(streamid);
		if(eptzpreset_goto(eptzpreset, direction, name, req) < 0)
			dbg("eptzpreset_goto error");
		
			send_request_ok_api(req);
			req->buffer_end += sprintf((void *)req_bufptr(req), "%s\r\n", ret_buf);
			req->status = DONE;
			return 1;
			
	}else if(strcmp(action, "seq_add") == 0){
		streamid = sys_info->ipcam[EPTZ_STREAMID].config.value;
		page = (streamid-1)*20;
		disable_sequence_autopan(streamid);

		for(i=0;i<MAX_SEQUENCE;i++)
		{
			*string_cmp = '\0';
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &eptzpreset.streamid, &index, &tmp, &eptzpreset.zoom, &eptzpreset.focus, &eptzpreset.position, string_cmp);
			if(strcmp(string_cmp, name) == 0){
				index = i+1;
				dbg("index=%d", index);
				break;
			}
		}
		
		for(i=0;i<MAX_SEQUENCE;i++)
		{
			//seq1_idx = i+1;
			
			if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value == 0){
				dbg("add ok, index=%d", index);
				info_write(&index, (EPTZPRESET_STREAM1_SEQ01+page)+i, 0, 0);
				info_write(&eptzpreset.seq_time, (SEQUENCE1_TIME01+page)+i, 0, 0);
				break;
			}
		}
		
	}else if(strcmp(action, "seq_del") == 0){
		streamid = sys_info->ipcam[EPTZ_STREAMID].config.value;
		page = (streamid-1)*20;
		disable_sequence_autopan(streamid);
		
		//seq_idx --; //0 is start
		sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+seq_idx].config.value = 0;
		sys_info->ipcam[(SEQUENCE1_TIME01+page)+seq_idx].config.value = EPTZPRESET_SEQ_TIME_DEF;
		
		dbg("clr to %d", seq_idx);

		for(i=(seq_idx+1);i<MAX_SEQUENCE;i++)
		{
			if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
				dbg("%d to %d", i, i-1);
				sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(i-1)].config.value = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value;
				sys_info->ipcam[(SEQUENCE1_TIME01+page)+(i-1)].config.value = sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value;
				
				dbg("clr to %d", i);
				sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value = 0;
				sys_info->ipcam[(SEQUENCE1_TIME01+page)+i].config.value = EPTZPRESET_SEQ_TIME_DEF;
				//info_write(&seq1_idx, (EPTZPRESET_STREAM1_SEQ01+page)+i, 0, 1);
			}
		}
		info_set_flag(CONF_FLAG_CHANGE);
		
	}else if(strcmp(action, "seq_update") == 0){
		streamid = sys_info->ipcam[EPTZ_STREAMID].config.value;
		page = (streamid-1)*20;
		
		dbg("seq_update ok, index=%d", seq_idx);
		dbg("eptzpreset.seq_time=%d", eptzpreset.seq_time);
		info_write(&eptzpreset.seq_time, (SEQUENCE1_TIME01+page)+seq_idx, 0, 0);
		
	}else if(strcmp(action, "seq_modify") == 0){
		streamid = sys_info->ipcam[EPTZ_STREAMID].config.value;
		page = (streamid-1)*20;
		disable_sequence_autopan(streamid);
		
		if(strcmp(direction, "up") == 0){
			if(seq_idx == 0) goto EPTZPRESET_END;//return 0;
			dbg("up ok");
			xchange = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+seq_idx].config.value;
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+seq_idx].config.value = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(seq_idx-1)].config.value;
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(seq_idx-1)].config.value = xchange;
			
			xchange = sys_info->ipcam[(SEQUENCE1_TIME01+page)+seq_idx].config.value;
			sys_info->ipcam[(SEQUENCE1_TIME01+page)+seq_idx].config.value = sys_info->ipcam[(SEQUENCE1_TIME01+page)+(seq_idx-1)].config.value;
			sys_info->ipcam[(SEQUENCE1_TIME01+page)+(seq_idx-1)].config.value = xchange;
			info_set_flag(CONF_FLAG_CHANGE);
		}else if(strcmp(direction, "down") == 0){
			if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(seq_idx+1)].config.value == 0) goto EPTZPRESET_END;//return 0;
			dbg("down ok");
			xchange = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+seq_idx].config.value;
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+seq_idx].config.value = sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(seq_idx+1)].config.value;
			sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+(seq_idx+1)].config.value = xchange;

			xchange = sys_info->ipcam[(SEQUENCE1_TIME01+page)+seq_idx].config.value;
			sys_info->ipcam[(SEQUENCE1_TIME01+page)+seq_idx].config.value = sys_info->ipcam[(SEQUENCE1_TIME01+page)+(seq_idx+1)].config.value;
			sys_info->ipcam[(SEQUENCE1_TIME01+page)+(seq_idx+1)].config.value = xchange;
			info_set_flag(CONF_FLAG_CHANGE);
		}else
			dbg("direction error");
		
	}else if(strcmp(action, "autopan") == 0){
	
		if(sys_info->ipcam[EPTZ_STREAMID].config.value == 1 && ~sys_info->status.auto_pan & AUTO_PAN_ID01){
			sys_info->status.auto_pan |= AUTO_PAN_ID01;			//enable autopan
			sys_info->status.auto_pan_dir &= ~AUTO_PAN_ID01;	//setting right
			sys_info->status.eptzpreset_seq &= ~SEQ_ID01;		//disable sequence
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 2 && ~sys_info->status.auto_pan & AUTO_PAN_ID02){
			sys_info->status.auto_pan |= AUTO_PAN_ID02;
			sys_info->status.auto_pan_dir &= ~AUTO_PAN_ID02;
			sys_info->status.eptzpreset_seq &= ~SEQ_ID02;
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 3 && ~sys_info->status.auto_pan & AUTO_PAN_ID03){
			sys_info->status.auto_pan |= AUTO_PAN_ID03;
			sys_info->status.auto_pan_dir &= ~AUTO_PAN_ID03;
			sys_info->status.eptzpreset_seq &= ~SEQ_ID03;
		}else
			dbg("streamid error");
		sys_info->ipcam[EPTZ_ADVSPEED].config.value = speed;
		
		dbg("id=%04d, dir=%04d", sys_info->status.auto_pan, sys_info->status.auto_pan_dir);

	}else if(strcmp(action, "seq_go") == 0){
		sys_info->status.sequence_move[streamid-1] = SEQ_INIT;
		if(sys_info->ipcam[EPTZ_STREAMID].config.value == 1 && (~sys_info->status.eptzpreset_seq & SEQ_ID01)){
			if(sys_info->ipcam[EPTZPRESET_STREAM1_SEQ01].config.value != 0)
				sys_info->status.eptzpreset_seq |= SEQ_ID01;		//enable sequence
			sys_info->status.auto_pan &= ~AUTO_PAN_ID01;		//disable autopan
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 2 && (~sys_info->status.eptzpreset_seq & SEQ_ID02)){
			if(sys_info->ipcam[EPTZPRESET_STREAM2_SEQ01].config.value != 0)
				sys_info->status.eptzpreset_seq |= SEQ_ID02;
			sys_info->status.auto_pan &= ~AUTO_PAN_ID02;
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 3 && (~sys_info->status.eptzpreset_seq & SEQ_ID03)){
			if(sys_info->ipcam[EPTZPRESET_STREAM3_SEQ01].config.value != 0)
				sys_info->status.eptzpreset_seq |= SEQ_ID03;
			sys_info->status.auto_pan &= ~AUTO_PAN_ID03;
		}else
			dbg("streamid error");

		sys_info->ipcam[EPTZ_ADVSPEED].config.value = speed;
		dbg("id=%04d", sys_info->status.eptzpreset_seq);

	}else if(strcmp(action, "stop") == 0){
		//gpSys_Info->status.sequence_move[streamid-1] == SEQ_INIT;
		if(sys_info->ipcam[EPTZ_STREAMID].config.value == 1){
			sys_info->status.auto_pan &= ~AUTO_PAN_ID01;
			sys_info->status.eptzpreset_seq &= ~SEQ_ID01;
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 2){
			sys_info->status.auto_pan &= ~AUTO_PAN_ID02;
			sys_info->status.eptzpreset_seq &= ~SEQ_ID02;
		}else if(sys_info->ipcam[EPTZ_STREAMID].config.value == 3){
			sys_info->status.auto_pan &= ~AUTO_PAN_ID03;
			sys_info->status.eptzpreset_seq &= ~SEQ_ID03;
		}else
			dbg("streamid error");
		
		dbg("id=%04d", sys_info->status.eptzpreset_seq);
	}else
		dbg("action error");

EPTZPRESET_END:
	send_request_ok_api(req);
	
	dbg("=======eptzpreset end=======");
	req->status = DONE;
#if 0	//debug
	osd_init();
	char osd_c[64];
	for(i=0;i<10;i++)
	{
		osd_printn(3, 1+i, osd_c, 64);
		osd_printn(3, 1+i, conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), strlen(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i)));
	}
#endif
	return 1;
}

int viscapreset_add(VISCAPRESET viscapreset)
{
	int i, tmp;
	//__u8 homevalue;
	char string_cmp[32];
	char buf[256], temp2[9];

	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 

	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	/**********************Stop Tour Command*******************/
	option = CAM_STOP_TOUR;
	var = 0;			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	/**********************Stop Tour Command*******************/

	//尋找preset名稱是否有重覆的
	for(i = 0;i < MAX_VISCAPRESET; i++){
		*string_cmp = '\0';
		sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp, string_cmp);
		if(strcmp(string_cmp, viscapreset.name) == 0){
			return 2;
		}
	}
	
	dbg("preset add is done");
	
	sprintf(buf, "%d-%s", viscapreset.index+1, viscapreset.name);
	dbg("buf = %s", buf);
	info_write(buf, VISCA_PRESET_ID01+viscapreset.index, strlen(buf), 0);

	//visca  preset_set start
	option = CAM_PRESET_SET;
	sprintf(temp2, "%08X", ((viscapreset.index)<<16) );
	var = strtoul(temp2, NULL, 16);
			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	//visca preset_set end	
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	
	return 0;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return 1;
}

int viscapreset_del(VISCAPRESET viscapreset)
{
	char buf[256];
	int tmp, Fd, i, j, seqclr = 0, clean = 0;
	char string_cmp[32];
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;
	unsigned short num;
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 

	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);	//sequence.vs
	if(Fd == -1)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__);
	
	for(i = 0; i < 8; i++){								//check if preset point exist in sequence line 
		for(j = 0; j < sys_info->vsptz.sequencepoint; j++){
			lseek(Fd, i*sys_info->vsptz.sequencepoint*4+j*4, SEEK_SET);
			read(Fd, &num, sizeof(num));
			if(num == 0)	break;
			if(num == (viscapreset.index+1)){
				//fprintf(stderr,"num = %d\n",num,__LINE__);
				seqclr = 1;
				break;
			}
		}
		if(seqclr == 1)	break;
	}
	if(close(Fd) != 0)
			fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);	
	if(seqclr == 1){
		//fprintf(stderr,"Clear sequence line %d first! %d\n",i+1,__LINE__);
		return 1;
	}
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	/**********************Stop Tour Command*******************/
	option = CAM_STOP_TOUR;
	var = 0;			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	/**********************Stop Tour Command*******************/
	
	dbg(" 	 idx = %d", viscapreset.index);
	dbg("    name = %s", viscapreset.name);
	dbg("del");

	//將在config裡的preset給清空
	sscanf(conf_string_read(VISCA_PRESET_ID01+viscapreset.index), "%d-%s", &tmp, string_cmp);

	if(strstr(string_cmp, "(H)") != NULL){
		info_write(&clean, (VISCA_HOME_NUMBER), sizeof(clean), 0);
		info_write(&clean, (VISCA_HOME_POSITION), sizeof(clean), 0);
	}	
	
	strcpy(viscapreset.name, "nonsetting");
	sprintf(buf, "%d-%s", tmp, viscapreset.name);
			
	dbg("buf = %s", buf);
	info_write(buf, VISCA_PRESET_ID01+viscapreset.index, strlen(buf), 0);	

	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	return 0;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return -1;
}

int viscapreset_goto(VISCAPRESET viscapreset, char* name, request * req)
{
	int i,pre_num;
	char string_cmp[32];
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	char temp2[9];
	unsigned int option, var, info_value, info_extend;
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
	dbg("index = %d", viscapreset.index);
	dbg("name = %s\n", viscapreset.name);

	for(i=0;i<MAX_VISCAPRESET;i++){
		*string_cmp = '\0';
		sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &viscapreset.index, string_cmp);
		if(strcmp(string_cmp, name) == 0){
			break;
		}
	}

	pre_num = viscapreset.index-1;

	//visca  preset_set start
	// parsing the instruction to option and arguement.
	option = CAM_PRESET_GO;
	sprintf(temp2, "%08X", (pre_num<<16) );
	var = strtoul(temp2, NULL, 16);
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return -1;
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	//visca preset_set end
	
	sys_info->vsptz.zoomratio[0] = '\0';	//clear zoom ratio on live video
	return 0;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return -1;
}

int uri_viscapreset(request * req)
{
#ifndef SUPPORT_VISCA
	return 0;
#endif
	VISCAPRESET viscapreset;

	int i, ret;
	char* action = NULL;
	char name[32], rename[32];
	char pre_num[4]="";
	//char string_cmp[32];
	//char buf[256];
	
	req->is_cgi = CGI;
	SQUASH_KA(req);

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "action") == 0){
				action = req->cmd_arg[i].value;
		}
		if(strcmp(req->cmd_arg[i].name, "name") == 0)
  				strcpy(name, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "rename") == 0)
  				strcpy(rename, req->cmd_arg[i].value);
		if(strcmp(req->cmd_arg[i].name, "number") == 0)
  				strcpy(pre_num, req->cmd_arg[i].value);	
	}
	
	if(!action)	
		action = "NONE";
	
	viscapreset.index = atoi(pre_num)-1;
	
	dbg("action = %s", action);
	dbg("name = %s", name);
	dbg("index = %d", viscapreset.index);
	
	if(strcmp(action, "add") == 0)
		strcpy(viscapreset.name, name);
	else if(strcmp(action, "modify") == 0)
		strcpy(viscapreset.name, rename);

	//check range
	if( strlen(viscapreset.name) > MAX_VISCAPRESETNAME_LEN) goto VISCAPRESET_END;//return 0;
	if( strlen(rename) > MAX_VISCAPRESETNAME_LEN) goto VISCAPRESET_END;//return 0;
	if( (strlen(name) == 4) && ((*name == 0x48) || (*name == 0x68)) && ((*(name+1) == 0x4F) || (*(name+1) == 0x6F)) 
	&& ((*(name+2) == 0x4D) || (*(name+2) == 0x6D))	 && ((*(name+3) == 0x45) || (*(name+3) == 0x65)) ) goto VISCAPRESET_END;//return 0;

	if(strcmp(action, "add") == 0){
		if(viscapreset_add(viscapreset) == 2){
			fprintf(stderr,"Preset name already exist");
			send_request_ok_api(req);
			req->status = DONE;
			return 1;
		}
	}
	else if(strcmp(action, "del") == 0){
		ret = viscapreset_del(viscapreset);
		if(ret == 1){
			send_request_ok_api(req);
			req->buffer_end += sprintf((void *)req_bufptr(req), "NG seqclr=1\r\n");
			req->status = DONE;
			return 1;
		}	
	}
	else if(strcmp(action, "modify") == 0){
		dbg("modify");
		if(viscapreset_add(viscapreset) == 2){
			fprintf(stderr,"Preset name already exist");
			send_request_ok_api(req);
			req->status = DONE;
			return 1;
		}
	}
	else if(strcmp(action, "goto") == 0){
		if(viscapreset_goto(viscapreset, name, req) < 0)
			dbg("viscapreset_goto error");
		
		send_request_ok_api(req);
		//req->buffer_end += sprintf((void *)req_bufptr(req), "%s\r\n", ret_buf);
		req->status = DONE;
		return 1;		
	}
	else
		dbg("action error");

		
VISCAPRESET_END:
	send_request_ok_api(req);
	
	dbg("=======viscapreset end=======");
	req->status = DONE;
	return 1;
}

int viscahome_set(char *num)
{
	__u8 homevalue = 1;
	__u16 value;
	char string_cmp[32];
	int tmp;
	value = atoi(num) - 1;
	sscanf(conf_string_read(VISCA_PRESET_ID01+value), "%d-%s", &tmp, string_cmp);
	if( strcmp(string_cmp,"nonsetting") == 0 ){
		dbg("Preset point is not setting yet!\n");
		return 0;
	}
	if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8){
		fprintf(stderr,"(H) repeated!\n");
		return 0;
	}
	
	snprintf(string_cmp, sizeof(string_cmp), "%s(H)", conf_string_read(VISCA_PRESET_ID01+value));
	info_write(string_cmp, VISCA_PRESET_ID01+value, strlen(string_cmp), 0);
	value++;
	info_write(&value, (VISCA_HOME_NUMBER), sizeof(value), 0);
	info_write(&homevalue, (VISCA_HOME_POSITION), sizeof(homevalue), 0);
	return 0;
}

int viscahome_reset(void)
{
	__u8 homevalue = 0; 
	__u16 num = 0;
	char string_cmp[32];
	char buf[256];
	int tmp, home_val, diff;
	
	if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8){
		home_val = sys_info->ipcam[VISCA_HOME_NUMBER].config.u16 - 1;
		sscanf(conf_string_read(VISCA_PRESET_ID01+home_val), "%d-%s", &tmp, string_cmp);
		diff = strstr(string_cmp, "(H)") - string_cmp;
		string_cmp[diff] = '\0';
		sprintf(buf, "%d-%s", tmp, string_cmp);
		info_write(buf, VISCA_PRESET_ID01+home_val, strlen(buf), 0);
	}
	info_write(&num, (VISCA_HOME_NUMBER), sizeof(num), 0);
	info_write(&homevalue, (VISCA_HOME_POSITION), sizeof(homevalue), 0);
	return 0;
}

int viscahome_go(void)
{
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1;

	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8){
		option = CAM_PRESET_GO;
		var = ((sys_info->ipcam[VISCA_HOME_NUMBER].config.u16-1)<<16); //0x00FF0000;
		if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
			goto VISCA_ERROR;
		if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
			goto VISCA_ERROR;
	}
	else{
		option = CAM_PANTILT_HOME;
		var = 0;			
		if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
			goto VISCA_ERROR;
		if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x52) )
			goto VISCA_ERROR;
	}
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	//memset(sys_info->vsptz.zoomratio, 0, sizeof(sys_info->vsptz.zoomratio));
	sys_info->vsptz.zoomratio[0] = '\0';
	return 0;
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return 1;
}

int uri_viscahome(request * req)
{
#ifndef SUPPORT_VISCA
	return 0;
#endif

	int i;
	char* action = NULL;
	char home_num[4]="";

	req->is_cgi = CGI;
	SQUASH_KA(req);

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "action") == 0){
			action = req->cmd_arg[i].value;
		}
		if(strcmp(req->cmd_arg[i].name, "position") == 0){
  			strncpy(home_num, req->cmd_arg[i].value, 3);
			//action = "sethome";
		}
	}
	if(!action)	
		goto VISCAHOME_END;
	//fprintf(stderr,"action = %s\n", action);
	if(strcmp(action, "reset") == 0){
		viscahome_reset();	
	}
	else if(strcmp(action, "gohome") == 0){
		viscahome_go();	
	}
	else if( (strcmp(action, "set") == 0) && (atoi(home_num)>=1) && (atoi(home_num)<=256) ){
		viscahome_set(home_num);
	}
	else
		dbg("action error");
		
VISCAHOME_END:
	send_request_ok_api(req);
	dbg("=======viscahome end=======");
	req->status = DONE;
	return 1;
}
int viscaicr_auto(int value)
{
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1;

	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	option = CAM_AUTO_ICR_ON;
	var = 0;			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	if (sys_info->ipcam[DNC_MODE].config.u8 != value){
		info_write(&value, DNC_MODE, 0, 0);
	}

	sys_info->status.dnc_state = 0;
	
	return 0;
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return 1;
}

int viscaicr_day(int value)
{
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1;
	
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	option = CAM_ICR_OFF;
	var = 0;			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	
	if (sys_info->ipcam[DNC_MODE].config.u8 != value){
		info_write(&value, DNC_MODE, 0, 0);
	}
	sys_info->status.dnc_state = 1;
	
	return 0;
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return 1;
}

int viscaicr_night(int value)
{
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	unsigned int option, var, info_value, info_extend;

	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1;
	
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		return 1;
	option = CAM_ICR_ON;
	var = 0;			
	if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
		goto VISCA_ERROR;
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	
	if (sys_info->ipcam[DNC_MODE].config.u8 != value){
		info_write(&value, DNC_MODE, 0, 0);
	}
	sys_info->status.dnc_state = 2;
	
	return 0;
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return 1;
}

int viscaicr_schedule(int value, char* sch_time)
{
	int start_tmp, end_tmp;
	//fprintf(stderr, "sch_time = %s\n", sch_time);
	if (sys_info->ipcam[DNC_MODE].config.u8 != value){
		info_write(&value, DNC_MODE, 0, 0);
	}
	if(sscanf(sch_time, "%06u%06u", &start_tmp, &end_tmp) != 2)
    	return -1;
	info_write(&start_tmp, DNC_SCHEDULE_START, 0, 0);	
	info_write(&end_tmp, DNC_SCHEDULE_END, 0, 0);
	return 0;
}

int uri_viscaicr(request * req)
{
#ifndef SUPPORT_VISCA
	return 0;
#endif

	int i, value;
	char* mode = NULL;
	char* sch_time = NULL; 
	req->is_cgi = CGI;
	SQUASH_KA(req);

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "mode") == 0){
			mode = req->cmd_arg[i].value;
		}
		if(strcmp(req->cmd_arg[i].name, "time") == 0)
  			sch_time = req->cmd_arg[i].value;
	}
	value = atoi(mode);
	//fprintf(stderr,"mode = %s\n", mode);
	//fprintf(stderr,"name = %s", name);
	if(value > 3 || value < 0) goto VISCAICR_END;

	if(strcmp(mode, "0") == 0){
		viscaicr_auto(value);
	}
	else if(strcmp(mode, "1") == 0){
		viscaicr_day(value);	
	}
	else if(strcmp(mode, "2") == 0){
		viscaicr_night(value);	
	}
	else if(strcmp(mode, "3") == 0){
		viscaicr_schedule(value, sch_time);	
	}
	else
		dbg("action error");

		
VISCAICR_END:
	send_request_ok_api(req);
	
	dbg("=======viscaicr end=======");
	req->status = DONE;
	return 1;
}

int viscaburn_start(int value)
{
	int loop = 0;
	info_write(&value, VISCA_BURNTEST_ENABLE, 0, 0);
	info_write(&loop, VISCA_BURNTEST_LOOP, 0, 0);
	return 0;
}

int viscaburn_stop(int value)
{
	//sys_info->ipcam[VISCA_BURNTEST_ENABLE].config.u8 = 0;
	info_write(&value, VISCA_BURNTEST_ENABLE, 0, 0);	
	return 0;
}

int uri_viscaburn(request * req)
{
#ifndef SUPPORT_VISCA
	return 0;
#endif
	int i, value;
	char* mode = NULL;
	req->is_cgi = CGI;
	SQUASH_KA(req);

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "mode") == 0){
			mode = req->cmd_arg[i].value;
		}
	}
	value = atoi(mode);
	//fprintf(stderr,"mode = %s\n", mode);
	//fprintf(stderr,"value = %d", value);
	if(value > 1 || value < 0) goto VISCABURN_END;

	if(strcmp(mode, "0") == 0){
		viscaburn_stop(value);
	}
	else if(strcmp(mode, "1") == 0){
		viscaburn_start(value);	
	}
	else
		dbg("action error");
		
VISCABURN_END:
	send_request_ok_api(req);
	
	dbg("=======viscaburn end=======");
	req->status = DONE;
	return 1;
}

int uri_viscaserialnumber(request * req)
{
#ifndef SUPPORT_VISCA
	return 0;
#endif
	BIOS_DATA bios;
	int i;
	char* sn = NULL;
	//char buf[30]="";
	req->is_cgi = CGI;
	SQUASH_KA(req);

	if ( uri_decoding(req, req->query_string) < 0) {
        send_r_bad_request(req);
        return 0;
    }
	for(i = 0 ; i <= (req->cmd_count-1) ; i++){
		if(strcmp(req->cmd_arg[i].name, "serialnumber") == 0){
			sn = req->cmd_arg[i].value;
		}
	}
	if(sn != NULL){
		if(strlen(sn) > 15) goto VISCASN_END;
		
		if (bios_data_read(&bios) < 0)
    		bios_data_set_default(&bios);
		
		for(i=0;i<16;i++){
			bios.sn[i] = *(sn+i);
		}
		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			dbg("BIOS_AUTO_CORRECT!\n");
		dbg("bios.sn = %s\n", bios.sn);
	}
	else
		dbg("error");
VISCASN_END:
	send_request_ok_api(req);
	
	dbg("=======viscasn end=======");
	req->status = DONE;
	return 1;
}

int uri_burntest_stream(request * req)
{
	int i,profile_id = 0;

	uri_decoding(req, req->query_string);
	for (i = 0; i < req->cmd_count; i++) {
		if(strcmp(req->cmd_arg[i].name, "nowprofileid") == 0) {
			profile_id = atoi(req->cmd_arg[i].value);
		}
		else if(strcmp(req->cmd_arg[i].name, "audiostream") == 0) {
			if (req->cmd_arg[i].value[0] == '1')
				req->req_flag.audio_on = 1;
		}
	}
	return get_av_stream(req , profile_id-1);
}

HTTP_URI HttpUri [] =
{
    {JPEG_FRAME_URI				,uri_jpeg			,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
//    {"/ipcam/mjpeg.cgi"			,uri_mjpeg_1		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
//    {"/ipcam/mjpegcif.cgi"		,uri_mjpeg_2		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
//    {"/ipcam/mpeg4.cgi"			,uri_mpeg4_1		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
//    {"/ipcam/mpeg4cif.cgi"		,uri_mpeg4_2		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
//    {"/ipcam/mpeg4cif_2.cgi"	,uri_mpeg4_3		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/ipcam/stream.cgi"		,uri_av_stream		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/ipcam/multiprofile.cgi"	,uri_getstreaminfo	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/ipcam/speakstream.cgi"	,NULL				,AUTHORITY_VIEWER   ,URI_FLAG_DIRECT_READ|URI_FLAG_DIRECT_WRITE ,NULL },
    {"/dev/speaker.cgi"			,NULL				,AUTHORITY_VIEWER   ,URI_FLAG_DIRECT_READ|URI_FLAG_DIRECT_WRITE ,NULL },

    {http_stream[0]				,uri_http_stream1	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {http_stream[1]				,uri_http_stream2	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {http_stream[2]				,uri_http_stream3	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#if MAX_PROFILE_NUM >= 4
    {http_stream[3]				,uri_http_stream4	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#endif
    {"/vb.htm"					,uri_vb_htm			,AUTHORITY_NONE     ,URI_FLAG_VIRTUAL_PAGE|URI_FLAG_CHECK_LOGIN ,NULL },
    {"/ini.htm"					,uri_ini_htm		,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/keyword.htm"				,uri_keyword_htm	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/cmdlist.htm"				,uri_cmdlist_htm	,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
  
    // Disable by Alex. 2010.04.12, NO MORE USED
    //{"/sdget.htm"				,uri_sdget_htm		,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    //{"/sddel.htm"				,uri_sddel_htm		,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#if 0
	{"/sdlist.htm"              ,uri_sdlist_htm     ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#else
    {"/sdlist.htm"              ,NULL     			,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE ,NULL },
#endif    
    {"/config/sdcard.cgi"       ,uri_sdcard         ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/sdcard_format.cgi",uri_sdcard_format  ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#if 1
	{"/config/sdcard_list.cgi"  ,uri_sdcard_list    ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#else
	{"/config/sdcard_list.cgi"  ,NULL    			,AUTHORITY_OPERATOR ,0 ,NULL },
#endif
	{"/config/sdcard_download.cgi" ,uri_sdcard_download ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/sdcard_delete.cgi"  ,uri_sdcard_delete    ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },

    {"/"                        ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/aenable.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/aevt.htm"                ,uri_aevt_htm       ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/cgi-bin/exportconf.cgi"  ,uri_export_conf    ,AUTHORITY_OPERATOR ,URI_FLAG_VOLATILE_FILE,NULL },
    {"/cgi-bin/exportcsr.cgi"   ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
    {"/cgi-bin/eventserver.cgi" ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
#if	SUPPORT_ONVIF
    //{"/cgi-bin/onvif_device.cgi"  ,NULL               ,AUTHORITY_NONE,0                    ,NULL },
    //{"/cgi-bin/onvif_media.cgi"  ,NULL               ,AUTHORITY_NONE,0                     ,NULL },
    {"/onvif/device_service"  ,NULL               ,AUTHORITY_NONE,0                    ,NULL },
#endif
 
    {"/cgi-bin/exportcert.cgi"  ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
    {"/cgi-bin/loadconf.cgi"    ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_DIRECT_WRITE 	,NULL },
    {"/cgi-bin/https.cgi"       ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
    {"/cgi-bin/wifi_scan.cgi"   ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
    {"/cgi-bin/wifi_config.cgi" ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
    {"/cgi-bin/network_test.cgi" ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },

	{"/cgi-bin/lencontrol.cgi"   ,NULL               ,AUTHORITY_OPERATOR ,0                     ,NULL },
   
    
#ifdef CONFIG_BRAND_DLINK
    {"/cgi-bin/upnpportforwarding.cgi" 	,NULL       ,AUTHORITY_OPERATOR ,URI_FLAG_DIRECT_WRITE ,NULL },
    {"/cgi-bin/upnpportforwardingtest.cgi" 	,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_DIRECT_WRITE ,NULL },
    {"/cgi-bin/format.cgi"              ,NULL         ,AUTHORITY_OPERATOR ,URI_FLAG_DIRECT_WRITE ,NULL },
#else
    {"/cgi-bin/upnpportforwarding.cgi"  ,NULL	,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/cgi-bin/upnpportforwardingtest.cgi"	    ,NULL   ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/cgi-bin/format.cgi"		    ,NULL	  ,AUTHORITY_OPERATOR ,0 ,NULL },
#endif

	{"/cgi-bin/sdlist.cgi"      ,NULL         ,AUTHORITY_OPERATOR ,0 ,NULL },
	{"/cgi-bin/sddownload.cgi"      ,uri_sddownload         ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/cgi-bin/checksum.cgi"			,uri_check_sum		,AUTHORITY_OPERATOR   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	
    {"/armio.htm"               ,NULL               ,AUTHORITY_ADMIN    ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/axtext.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/bwcntl.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_armio.htm"             ,NULL               ,AUTHORITY_ADMIN    ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_axtext.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_bwcntl.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_dv840output.htm"       ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_help.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_lang.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_mcenter.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/cms.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_ndhcpsvr.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_nftphost.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_nupnp.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_sccd.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_svideo.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_tailpage.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/c_update.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/datetime.htm"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/dv840output.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/faq_cs_cz.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_de_de.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_en_us.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_es_es.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_fi_fi.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_fr_fr.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_hu_hu.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_it_it.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_nl_nl.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_pl_pl.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_pt_pt.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_ro_ro.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_sv_se.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_zh_cn.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/faq_zh_tw.html"          ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/help.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/https_csr.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/https_cert.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/image.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/imgdaynight.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/img_h264.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/img.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/imgmask.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/imgtune.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/index.htm"               ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/index.html"              ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup.htm"          		,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/advanced.htm"          	,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/maintenance.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/K_dragon.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/K_fastrax.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/K_lilin.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/K_seeku.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/K_sensor.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/lang.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/logout.htm"              ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/main.htm"                ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/mcenter.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/motion.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nddns.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ndhcpsvr.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/net.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nftphost.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nftp.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nipfilter.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/npppoe.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nsmtp.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nsntp.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nupnp.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nwireless.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/pda.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/pppoedes_cs_cz.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_de_de.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_en_us.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_es_es.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_fi_fi.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_fr_fr.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_hu_hu.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_it_it.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_nl_nl.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_pl_pl.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_pt_pt.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_ro_ro.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_sv_se.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_zh_cn.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/pppoedes_zh_tw.html"     ,NULL               ,AUTHORITY_OPERATOR ,0 ,NULL },
    {"/p_pelcod.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/p_pelcop.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ptz.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ptz_left.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/renable.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/rsch.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/saudio.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/sccd.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/sdigital.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/sdt.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setcard.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setftp.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setsmtp.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setvid.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/srs.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/sseq.htm"                ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/sts.htm"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/suser.htm"               ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/svideo.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/tailpage.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/update.cgi"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_DIRECT_WRITE   ,NULL },
    {"/update.htm"              ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/version.htm"             ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
	#ifdef SUPPORT_VISCA
	{"/var.js"                  ,NULL               ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
    {"/index.js"                ,NULL               ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
    #else
	{"/var.js"                  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/index.js"                ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	#endif
    {"/setup.js"                ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wait_httpscgi.htm"       ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/image_privacy.htm"		,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/setup_image_focus.htm"	,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/setup_image_preset.htm"	,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/image_focus.htm"			,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/setup_port.htm"			,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/maintenance_user.htm"	,NULL				,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE	 ,NULL },

    {"/be/help.css"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/lc_en_us.css"       ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/lc_tyco.css"        ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/setup.css"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/help.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/index.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/setup.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/tyco.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/app.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/net.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/setup.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/storage.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/index.htm"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/setuphelp.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/status.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/be/system.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/be/pelcod.js"		  ,NULL				  ,AUTHORITY_VIEWER	  ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/be/pelcop.js"		  ,NULL				  ,AUTHORITY_VIEWER	  ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/be/ptzctrl.js"		  ,NULL				  ,AUTHORITY_VIEWER	  ,URI_FLAG_NEED_PARSE   ,NULL },

    {"/vt/help.css"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/lc_en_us.css"       ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/lc_tyco.css"        ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/setup.css"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/help.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/index.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/setup.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/tyco.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/app.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/net.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/setup.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/storage.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/index.htm"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/setuphelp.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/status.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/vt/system.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/vt/pelcod.js"		  ,NULL				  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/vt/pelcop.js"		  ,NULL				  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/vt/ptzctrl.js"		  ,NULL				  ,AUTHORITY_VIEWER	  ,URI_FLAG_NEED_PARSE   ,NULL },
	
    {"/tc/help.css"	      ,NULL		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/lc_en_us.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/tc/lc_tyco.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/tc/setup.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
    {"/tc/help.js"	      ,NULL		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/tc/index.js"	      ,NULL		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/setup.js" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/tc/tyco.js"		  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/tc/app.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/net.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/setup.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/storage.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/index.htm"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/tc/setuphelp.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/status.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/system.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/pelcod.js"		,NULL		,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/pelcop.js"		,NULL		,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/tc/ptzctrl.js"		,NULL		,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/ap/help.css"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/lc_en_us.css"       ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/lc_tyco.css"        ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/setup.css"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/help.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/index.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/setup.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/tyco.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/app.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/net.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/setup.htm"          ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/storage.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/index.htm"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/setuphelp.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/status.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ap/system.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/ap/pelcod.js"		  ,NULL				  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/ap/pelcop.js"		  ,NULL				  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/ap/ptzctrl.js"		  ,NULL				  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/vd/help.css" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/lc_en_us.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/lc_tyco.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/setup.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/help.js"		  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/index.js" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/setup.js" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/tyco.js"		  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/app.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/net.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/setup.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/storage.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/index.htm"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/setuphelp.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/status.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/system.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/vd/pelcod.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/vd/pelcop.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/vd/ptzctrl.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	
	{"/fmm/help.css" 		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/lc_en_us.css" 	  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/lc_tyco.css"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/setup.css"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/help.js"			  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/index.js" 		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/setup.js" 		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/tyco.js"			  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/app.htm"			  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/net.htm"			  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/setup.htm"		  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/storage.htm"		  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/index.htm"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/setuphelp.htm"	  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/status.htm"		  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/system.htm"		  ,NULL 			  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/pelcod.js"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/pelcop.js"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/fmm/ptzctrl.js"		  ,NULL 			  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },

	{"/aue/help.css" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/lc_en_us.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/lc_tyco.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/setup.css"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/help.js"		  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/index.js" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/setup.js" 	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/tyco.js"		  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/app.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/net.htm"		  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/setup.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/storage.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/index.htm"	  ,NULL 	      ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/setuphelp.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/status.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/system.htm"	  ,NULL 	      ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/aue/pelcod.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/aue/pelcop.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/aue/ptzctrl.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },

	{"/ix/help.css"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/lc_en_us.css"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/lc_tyco.css"   ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/setup.css"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/help.js" 	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/index.js"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/setup.js"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/tyco.js" 	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/app.htm" 	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/net.htm" 	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/setup.htm"	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/storage.htm"   ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/index.htm"	  ,NULL 		  ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/setuphelp.htm"	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/status.htm"	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/system.htm"	  ,NULL 		  ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
	{"/ix/pelcod.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/ix/pelcop.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/ix/ptzctrl.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },

	{"/nb/help.css"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/lc_en_us.css"       ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/lc_tyco.css"        ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/setup.css"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nb/help.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nb/index.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/setup.js"           ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/tyco.js"            ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/app.htm"            ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/net.htm"            ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/setup.htm"          ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/storage.htm"        ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/index.htm"          ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/setuphelp.htm"      ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/status.htm"         ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/system.htm"         ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/nb/pelcod.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nb/pelcop.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nb/ptzctrl.js"		,NULL		,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
 //   {"/nb/image_privacy.htm"           ,NULL               ,AUTHORITY_OPERATOR   ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/net_network.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_event_event.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_event.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_event.js"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_event_media.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_event_recording.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/app_motion.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	#ifdef SUPPORT_VISCA
	{"/common.js"   		  ,NULL               ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
	#else
	{"/common.js"   		  ,NULL               ,AUTHORITY_VIEWER   	,URI_FLAG_NEED_PARSE   ,NULL },
	#endif
	{"/image_audio_video.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/image_image.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nbonjour.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/nddns.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_access_list.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_dynamic.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_https.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_portset.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_pppoe.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/net_traffic.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/storage_sdlist.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/storage_sdlist_refresh.htm" 	  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/system_digtal_in.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/system_digtal_out.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/system_icr.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/system_rs485.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/system_time_ady.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	{"/system_user.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/system_backup_restore.htm"   		  ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	
#ifdef CONFIG_BRAND_DLINK
	{"/status_log.htm"	    ,uri_status_log_htm       ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE	 ,NULL },
#else
	{"/status_log.htm"	    ,NULL                     ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
#endif

//modify by jiahung 20101029, define all sensor ??
#if 1//defined ( SENSOR_USE_MT9V136 ) || defined ( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_IMX035 ) || defined ( SENSOR_USE_OV2715 ) || defined IMGS_TS2713 || defined IMGS_ADV7180
    {"/nvirtualserver.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/status_info.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    //{"/tyco/status_info.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup2.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup3.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup4.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup5.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup6.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },     
    {"/wizard.js"               ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	#ifdef SUPPORT_VISCA
	{"/ptzctrl.js"              ,NULL               ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
	#else
	{"/ptzctrl.js"              ,NULL               ,AUTHORITY_VIEWER   ,URI_FLAG_NEED_PARSE   ,NULL },
	#endif
#endif


#ifdef CONFIG_BRAND_DLINK
    {"/setup_wizard.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_network.htm"       ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_https.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_ipfilter.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_dynamic.htm"       ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_image.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_audio_video.htm"   ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
#ifdef SUPPORT_VISCA
	{"/setup_viscasetup.htm"	,NULL				,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },		//test ryan
	{"/setup_visca_sequence.htm",NULL				,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },		//test ryan
	{"/dcs_test.htm"			,NULL				,AUTHORITY_NONE 	,URI_FLAG_NEED_PARSE   ,NULL },		//test ryan
#endif
	{"/setup_motion.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_time_ady.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_recording.htm"     ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_snapshot.htm"      ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_digital.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_digital_in.htm"     ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_rs485.htm"         ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_preset_position.htm",NULL              ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_icr.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_access_list.htm"   ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_sdlist.htm"        ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_digital_io.htm"    ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/maintenance_device.htm"  ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/maintenance_backup_restore.htm"      ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/maintenance_firmware_upgrade.htm"    ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },

    //{"/status_info.htm"            ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    //{"/status_log.htm"          ,uri_status_log_htm       ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },

    {"/checkout.htm"            ,NULL               ,AUTHORITY_VIEWER ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup1.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_internet_setup7.htm"          ,NULL   ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL }, 

    {"/wizard_ipcam_setup1.htm" ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_ipcam_setup2.htm" ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_ipcam_setup3.htm" ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_ipcam_setup4.htm" ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/wizard_ipcam_setup5.htm" ,NULL		    ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/help.js"                 ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/ptz_dlink.js"                 ,NULL               ,AUTHORITY_NONE ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_qos.htm"           ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
#endif

	{"/img_mpro.htm"			,NULL			,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },    
	{"/filelist.htm"			,NULL			,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
    {"/system_digital_io.htm"   ,NULL               ,AUTHORITY_OPERATOR  ,URI_FLAG_NEED_PARSE  ,NULL },
#ifdef CONFIG_BRAND_DLINK
	{"/setup_preset_position.htm"			,NULL			,AUTHORITY_VIEWER	,URI_FLAG_NEED_PARSE   ,NULL },
#endif

//#if SUPPORT_SAMBA
//    {"/recodesetting.htm"       ,NULL               ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
//#endif

    {"/setup_wifi_2.htm"         ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },

	{"/cgi-bin/para_info.cgi"         ,uri_para_info           ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/cgi-bin/api_string.cgi"         ,uri_api_string           ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	
#ifdef SUPPORT_EVENT_2G
    {"/setup_event.htm"         ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_event.js"          ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_event_event.htm"   ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_event_media.htm"   ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_event_recording.htm"   ,NULL    ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
    {"/setup_event_server.htm"   ,NULL        ,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
#endif
#ifdef CONFIG_BRAND_DLINK //NIPCA
    {"/common/info.cgi"         ,uri_info           ,AUTHORITY_NONE   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/camera_info.cgi"  ,uri_camera_info    ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/datetime.cgi"  	,uri_datetime       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/users/verify.cgi"        ,uri_verify         ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL }, 
    {"/eng/admin/st_device.cgi" ,uri_st_device      ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/user_mod.cgi"     ,uri_user_mod       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/user_list.cgi"    ,uri_user_list      ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/user_del.cgi"     ,uri_user_del      	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/stream_info.cgi"  ,uri_stream_info    ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/sensor_reset.cgi" ,uri_sensor_reset   ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/icr.cgi"    		,uri_icr      		,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/irled.cgi"		,uri_irled  		,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/network.cgi"    	,uri_network      	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/pppoe.cgi"    	,uri_pppoe      	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
   	{"/config/ddnsproviders.cgi",uri_ddnsproviders  ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
   	{"/config/ddns.cgi"    		,uri_ddns  			,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/upnp.cgi"    		,uri_upnp  			,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/httpport.cgi"    	,uri_httpport  		,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/video.cgi"        ,uri_video          ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/motion.cgi"       ,uri_motion         ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/sensor_info.cgi"  ,uri_sensor_info    ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/sensor.cgi"       ,uri_sensor         ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
   	{"/config/mic.cgi"       	,uri_mic         	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
   	{"/config/speaker.cgi"      ,uri_speaker        ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/led.cgi"			,uri_led         	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/system_reboot.cgi",uri_reboot         ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/system_reset.cgi"	,uri_reset         	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/io.cgi"           ,uri_io             ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },   
    {"/config/rs485.cgi"        ,uri_rs485          ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/image/jpeg.cgi"          ,uri_dlink_jpeg     ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/video/mjpg.cgi"          ,uri_dlink_mjpeg    ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/video/ACVS.cgi"          ,uri_dlink_mpeg4    ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/video/ACVS-H264.cgi"     ,uri_dlink_h264     ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/audio/ACAS.cgi"          ,uri_dlink_audio    ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/audio/audio.cgi"         ,uri_dlink_audio    ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/video/video.cgi"         ,uri_dlink_video    ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/rtspurl.cgi"		,uri_rtspurl        ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/audio.cgi"		,uri_audio        	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },

    {"/config/ptz_info.cgi"     ,uri_ptz_info       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_pos.cgi"		,uri_ptz_pos       	,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_step.cgi"     ,uri_ptz_step       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_preset_list.cgi"   ,uri_ptz_preset_list     ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_preset.cgi"   ,uri_ptz_preset     ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_move.cgi"     ,uri_ptz_move       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_direction.cgi",uri_ptz_direction  ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_move_rel.cgi"	,uri_ptz_move_rel   ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/ptz_home.cgi"     ,uri_ptz_home       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/auto_patrol.cgi"  ,uri_auto_partol    ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/auto_pan.cgi"     ,uri_auto_pan       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/config_auto_patrol.cgi"     ,uri_config_auto_patrol       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/focus_type.cgi"	  ,uri_focus_type		,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/config/manual_focus.cgi"     ,uri_manual_focus       ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },

    {"/config/rs485_do.cgi" 	,NULL   			,AUTHORITY_ADMIN   ,0 ,NULL },
    {"/config/notify.cgi"		,uri_notify  		,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
    {"/config/notify_stream.cgi",uri_notify_stream  ,AUTHORITY_ADMIN   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#endif
	{"/ipcam/audio.cgi"          ,uri_appro_audio   ,AUTHORITY_VIEWER   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/cgi-bin/eptzpreset.cgi"   ,uri_eptzpreset    ,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
#if defined (SUPPORT_VISCA) && defined(CONFIG_BRAND_DLINK)
	{"/cgi-bin/visca.cgi"   	 ,NULL  			,AUTHORITY_OPERATOR ,0 					   ,NULL },
	{"/cgi-bin/viscapreset.cgi"  ,uri_viscapreset  ,AUTHORITY_OPERATOR  ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/cgi-bin/viscahome.cgi"    ,uri_viscahome   	,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/cgi-bin/viscaicr.cgi"     ,uri_viscaicr   	,AUTHORITY_OPERATOR ,URI_FLAG_VIRTUAL_PAGE ,NULL },

	{"/cgi-bin/viscaburntest.cgi",uri_viscaburn   	,AUTHORITY_NONE 	,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/cgi-bin/viscasn.cgi"		 ,uri_viscaserialnumber   	,AUTHORITY_NONE 	,URI_FLAG_VIRTUAL_PAGE ,NULL },
	{"/D-Link.js"                ,NULL              ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/setInnerHTML.js"                ,NULL        ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/visca.js"                 ,NULL              ,AUTHORITY_NONE   	,URI_FLAG_NEED_PARSE   ,NULL },
	{"/burntest/stream.cgi"		 ,uri_burntest_stream,AUTHORITY_NONE   	,URI_FLAG_VIRTUAL_PAGE ,NULL },
#endif

	 {"/cgi-bin/shutter.cgi"		,uri_shutter  		,AUTHORITY_OPERATOR   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	 {"/cgi-bin/exposure.cgi"		,uri_expoure  		,AUTHORITY_OPERATOR   ,URI_FLAG_VIRTUAL_PAGE ,NULL },
	 
#if 0	/* old type */
	{ "/live1.sdp", NULL, AUTHORITY_NONE, URI_FLAG_DIRECT_READ | URI_FLAG_DIRECT_WRITE, NULL },
#else	/* old type */
#if 0	/* access by viewer ? */
	{ rtsp_stream[0]            , uri_rtsp_stream1 , AUTHORITY_VIEWER       , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
	{ rtsp_stream[1]            , uri_rtsp_stream2 , AUTHORITY_VIEWER       , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
	{ rtsp_stream[2]            , uri_rtsp_stream3 , AUTHORITY_VIEWER       , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
#if MAX_PROFILE_NUM >= 4
	{ rtsp_stream[3]            , uri_rtsp_stream4 , AUTHORITY_VIEWER       , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
#endif
#else	/* access by viewer ? */
	{ rtsp_stream[0]            , uri_rtsp_stream1 , AUTHORITY_NONE         , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
	{ rtsp_stream[1]            , uri_rtsp_stream2 , AUTHORITY_NONE         , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
	{ rtsp_stream[2]            , uri_rtsp_stream3 , AUTHORITY_NONE         , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
#if MAX_PROFILE_NUM >= 4
	{ rtsp_stream[3]            , uri_rtsp_stream4 , AUTHORITY_NONE         , URI_FLAG_VIRTUAL_PAGE | URI_FLAG_RTSP_O_HTTP, NULL },
#endif
#endif	/* access by viewer ? */
#endif	/* old type */
};
    
#define URI_HASH_SIZE	(sizeof(HttpUri)/sizeof(HTTP_URI))
      
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
unsigned int uri_hash_cal_value(char *name)
{     
	    unsigned int value = 0;
      
	    while (*name)
	    	value = value * 37 + (unsigned int)(*name++);
	    return value;
}     
      
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void uri_hash_insert_entry(URI_HASH_TABLE *table, HTTP_URI *arg)
{     
	  if (table->entry) {
	  	arg->next = table->entry;
	  }
	  table->entry = arg;
}     
      
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int uri_hash_table_init(void)
{     
	  int i;

	if ( (uri_hash = (URI_HASH_TABLE *)calloc(sizeof(URI_HASH_TABLE), MAX_URI_HASH_SIZE)) == NULL) {
		dbg("uri_hash_table_init: Allocate memory fail!!!\n");
		return -1;
	}
	for (i=0; i<URI_HASH_SIZE; i++) {
		uri_hash_insert_entry(uri_hash+(uri_hash_cal_value(HttpUri[i].name)%MAX_URI_HASH_SIZE), HttpUri+i);
	}
	return 0;
}

void uri_hash_table_release(void)
{     
	int i;
	
	for (i=0; i<URI_HASH_SIZE; i++)
		HttpUri[i].next = NULL;

	free(uri_hash);
	uri_hash = NULL;
}

HTTP_URI *http_uri_search(char *arg)
{
	HTTP_URI *opt;

	opt = uri_hash[uri_hash_cal_value(arg)%MAX_URI_HASH_SIZE].entry;
	
	while (opt) {
		if ( strcasecmp(opt->name, arg) == 0 )
			return opt;
		opt = opt->next;
	}
	return NULL;
}

#endif


/*
 * Name: new_request
 * Description: Obtains a request struct off the free list, or if the
 * free list is empty, allocates memory
 *
 * Return value: pointer to initialized request
 */

request *new_request(void)
{
    static int request_count = 0;
    request *req;

    if (request_free) {
        req = request_free;     /* first on free list */
        dequeue(&request_free, request_free); /* dequeue the head */
    } else {
        if (request_count >= MAX_REQUEST)
            return NULL;
        req = (request *) malloc(sizeof (request));
        if (!req) {
            log_error_time();
            perror("malloc for new request");
            return NULL;
        }
        request_count++;
    }

#ifdef SERVER_SSL 
	req->ssl = NULL;
#endif /*SERVER_SSL*/

    memset(req, 0, offsetof(request, buffer) + 1);
#ifdef DAVINCI_IPCAM
    req->busy_flag = BUSY_FLAG_AUDIO|BUSY_FLAG_VIDEO;
#endif

    return req;
}

request *get_sock_request(int server_s)
{
    int fd;                     /* socket */
    struct SOCKADDR remote_addr; /* address */
    struct SOCKADDR salocal;
    int remote_addrlen = sizeof (struct SOCKADDR);
    request *conn;              /* connection */
    size_t len;
    static int system_bufsize = 0; /* Default size of SNDBUF given by system */

    remote_addr.S_FAMILY = 0xdead;
    fd = accept(server_s, (struct sockaddr *) &remote_addr,
                &remote_addrlen);

    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            /* abnormal error */
            WARN("accept");
        else
            /* no requests */
            pending_requests = 0;
        return NULL;
    }
    if (fd >= FD_SETSIZE) {
        WARN("Got fd >= FD_SETSIZE.");
        close(fd);
	return NULL;
    }
#ifdef DEBUGNONINET
    /* This shows up due to race conditions in some Linux kernels
       when the client closes the socket sometime between
       the select() and accept() syscalls.
       Code and description by Larry Doolittle <ldoolitt@boa.org>
     */
#define HEX(x) (((x)>9)?(('a'-10)+(x)):('0'+(x)))
    if (remote_addr.sin_family != AF_INET) {
        struct sockaddr *bogus = (struct sockaddr *) &remote_addr;
        char *ap, ablock[44];
        int i;
        close(fd);
        log_error_time();
        for (ap = ablock, i = 0; i < remote_addrlen && i < 14; i++) {
            *ap++ = ' ';
            *ap++ = HEX((bogus->sa_data[i] >> 4) & 0x0f);
            *ap++ = HEX(bogus->sa_data[i] & 0x0f);
        }
        *ap = '\0';
        fprintf(stderr, "non-INET connection attempt: socket %d, "
                "sa_family = %hu, sa_data[%d] = %s\n",
                fd, bogus->sa_family, remote_addrlen, ablock);
        return NULL;
    }
#endif

/* XXX Either delete this, or document why it's needed */
/* Pointed out 3-Oct-1999 by Paul Saab <paul@mu.org> */
#ifdef REUSE_EACH_CLIENT_CONNECTION_SOCKET
    if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &sock_opt,
                    sizeof (sock_opt))) == -1) {
        DIE("setsockopt: unable to set SO_REUSEADDR");
    }
#endif

    len = sizeof(salocal);

    if (getsockname(fd, (struct sockaddr *) &salocal, &len) != 0) {
        WARN("getsockname");
        close(fd);
        return NULL;
    }

    conn = new_request();
    if (!conn) {
        close(fd);
        return NULL;
    }
    conn->fd = fd;
    conn->status = READ_HEADER;
    conn->header_line = conn->client_stream;
    conn->time_last = current_time;
    conn->kacount = ka_max;

    ascii_sockaddr(&salocal, conn->local_ip_addr, NI_MAXHOST);

    /* nonblocking socket */
    if (set_nonblock_fd(conn->fd) == -1)
        WARN("fcntl: unable to set new socket to non-block");

    /* set close on exec to true */
    if (fcntl(conn->fd, F_SETFD, 1) == -1)
        WARN("fctnl: unable to set close-on-exec for new socket");

    /* Increase buffer size if we have to.
     * Only ask the system the buffer size on the first request,
     * and assume all subsequent sockets have the same size.
     */
    if (system_bufsize == 0) {
        len = sizeof (system_bufsize);
        if (getsockopt
            (conn->fd, SOL_SOCKET, SO_SNDBUF, &system_bufsize, &len) == 0
            && len == sizeof (system_bufsize)) {
            /*
               fprintf(stderr, "%sgetsockopt reports SNDBUF %d\n",
               get_commonlog_time(), system_bufsize);
             */
            ;
        } else {
            WARN("getsockopt(SNDBUF)");
            system_bufsize = 1;
        }
    }
    if (system_bufsize < sockbufsize) {
        if (setsockopt
            (conn->fd, SOL_SOCKET, SO_SNDBUF, (void *) &sockbufsize,
             sizeof (sockbufsize)) == -1) {
            WARN("setsockopt: unable to set socket buffer size");
#ifdef DIE_ON_ERROR_TUNING_SNDBUF
            exit(errno);
#endif
        }
    }

	/* add by Alex, 2008.10.14  */
#ifdef DAVINCI_IPCAM
    conn->remote_ip = remote_addr.sin_addr.s_addr;
#endif

    /* for log file and possible use by CGI programs */
    ascii_sockaddr(&remote_addr, conn->remote_ip_addr, NI_MAXHOST);

    /* for possible use by CGI programs */
    conn->remote_port = net_port(&remote_addr);

    status.requests++;

#ifdef USE_TCPNODELAY
    /* Thanks to Jef Poskanzer <jef@acme.com> for this tweak */
    {
        int one = 1;
        if (setsockopt(conn->fd, IPPROTO_TCP, TCP_NODELAY,
                       (void *) &one, sizeof (one)) == -1) {
            DIE("setsockopt: unable to set TCP_NODELAY");
        }

    }
#endif

#ifndef NO_RATE_LIMIT
    if (conn->fd > max_connections) {
        send_r_service_unavailable(conn);
        conn->status = DONE;
        pending_requests = 0;
    }
#endif                          /* NO_RATE_LIMIT */

    total_connections++;
    enqueue(&request_ready, conn);
    return conn;
}

/*
 * Name: get_request
 *
 * Description: Polls the server socket for a request.  If one exists,
 * does some basic initialization and adds it to the ready queue;.
 */
void get_request(int server_s)
{
	get_sock_request(server_s);
}

#ifdef SERVER_SSL
void get_ssl_request(void)
{
	request *conn;

	conn = get_sock_request(server_ssl);
	if (!conn)
		return;
	conn->ssl = SSL_new (ctx);
	if(conn->ssl == NULL){
		printf("Couldn't create ssl connection stuff\n");
		return;
	}
	SSL_set_fd (conn->ssl, conn->fd);
	if(SSL_accept(conn->ssl) <= 0){
		ERR_print_errors_fp(stderr);
		return;
	}
	else{/*printf("SSL_accepted\n");*/}
}
#endif /*SERVER_SSL*/

void free_client_socket(void)
{
    request *current;
	
    current = request_ready;

    while (current) {
        if (current->fd > 0)
            close(current->fd);
        current = current->next;
    }
}

/*
 * Name: free_request
 *
 * Description: Deallocates memory for a finished request and closes
 * down socket.
 */

static void free_request(request ** list_head_addr, request * req)
{
    int i, stream_type, profile_id;
    /* free_request should *never* get called by anything but
       process_requests */

    if (req->buffer_end && req->status != DEAD) {
        req->status = DONE;
        return;
    }
    /* put request on the free list */
    dequeue(list_head_addr, req); /* dequeue from ready or block list */

    if (req->logline)           /* access log */
        log_access(req);

#ifdef CONFIG_STREAM_LIMIT
	if (req->req_flag.is_stream)
		stream_count--;
#ifdef BOA_OSD_DEBUG
	{
		char buffer[50];
		sprintf(buffer, "stream count=%d ", stream_count);
		osd_print(5, 3, buffer);
	}
#endif
#endif

	req->nipca_notify_stream = 0;

#ifdef DAVINCI_IPCAM
    stream_type = req->http_stream & FMT_MASK;
    profile_id = req->http_stream & SUBSTREAM_MASK;
    if (stream_type == URI_STREAM_JPEG) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_MJPEG|AV_OP_UNLOCK, req->video.serial_lock, NULL);
    }
    else  if (stream_type == URI_STREAM_MJPEG) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_MJPEG|AV_OP_UNLOCK, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else if (stream_type == URI_STREAM_MPEG4) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_MPEG4|AV_OP_UNLOCK, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else if (stream_type == URI_STREAM_H264) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_H264|AV_OP_UNLOCK, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else if (stream_type == URI_HTTP_MJPEG) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_MJPEG|AV_OP_UNLOCK, req->video.serial_lock, NULL);
    }
    else if (stream_type == URI_DLINK_STREAM_MPEG4) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_MPEG4|AV_OP_UNLOCK, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
	else if (stream_type == URI_DLINK_STREAM_H264) {
        if (req->video.serial_lock > 0)
            GetAVData(profile_id|AV_FMT_H264|AV_OP_UNLOCK, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else if (stream_type == URI_STREAM_AVC) {
        if (req->video.serial_lock > 0)
            GetAVData(AV_OP_UNLOCK_AVC, req->video.serial_lock, NULL);
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }	 
    else if (stream_type == URI_AUDIO_G726) {
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else if (stream_type == URI_AUDIO_STREAM) {
        if (req->audio.serial_lock > 0)
            GetAVData(AV_OP_SUB_STREAM_1|AV_FMT_AUDIO|AV_OP_UNLOCK, req->audio.serial_lock, NULL);
    }
    else {
#endif
        if (req->mmap_entry_var)
            release_mmap(req->mmap_entry_var);
        else if (req->data_mem) {
#ifdef DAVINCI_IPCAM
            if (req->mmap_ptr)
                munmap(req->mmap_ptr, req->mmap_size);
            else
#endif
                munmap(req->data_mem, req->filesize);
        }
#ifdef DAVINCI_IPCAM
    }
#endif

    if (req->data_fd)
        close(req->data_fd);

    if (req->post_data_fd)
        close(req->post_data_fd);

    if (req->response_status >= 400)
        status.errors++;

    for (i = COMMON_CGI_COUNT; i < req->cgi_env_index; ++i) {
        if (req->cgi_env[i]) {
            free(req->cgi_env[i]);
        } else {
            log_error_time();
            fprintf(stderr, "Warning: CGI Environment contains NULL value" \
                    "(index %d of %d).\n", i, req->cgi_env_index);
        }
    }

#ifdef DAVINCI_IPCAM
    if (req->connect) {
        req->connect->counter--;
        req->connect->time_last = sys_info->second;
    }
    if (req->cmd_string)
        free(req->cmd_string);
    if (req->export_file)
        free(req->export_file);
    if (req->mem_flag & MFLAG_IS_MEMORY) {
        if (req->data_mem)
            free(req->data_mem);
    }
#endif
    if (req->pathname)
        free(req->pathname);
    if (req->path_info)
        free(req->path_info);
    if (req->path_translated)
        free(req->path_translated);
    if (req->script_name)
        free(req->script_name);

    if ((req->keepalive == KA_ACTIVE) &&
        (req->response_status < 500) && req->kacount > 0) {
        int bytes_to_move;

        request *conn = new_request();
        if (!conn) {
            /* errors already reported */
            enqueue(&request_free, req);
            close(req->fd);
            total_connections--;
#ifdef SERVER_SSL
		SSL_free(req->ssl);
#endif /*SERVER_SSL*/
            return;
        }
        conn->fd = req->fd;
        conn->status = READ_HEADER;
        conn->header_line = conn->client_stream;
        conn->kacount = req->kacount - 1;
#ifdef SERVER_SSL
        conn->ssl = req->ssl; /*MN*/
#endif /*SERVER_SSL*/

        /* close enough and we avoid a call to time(NULL) */
        conn->time_last = req->time_last;

        /* for log file and possible use by CGI programs */
        memcpy(conn->remote_ip_addr, req->remote_ip_addr, NI_MAXHOST);
        memcpy(conn->local_ip_addr, req->local_ip_addr, NI_MAXHOST);

        conn->remote_ip= req->remote_ip;
        /* for possible use by CGI programs */
        conn->remote_port = req->remote_port;

        status.requests++;

        /* we haven't parsed beyond req->parse_pos, so... */
        bytes_to_move = req->client_stream_pos - req->parse_pos;

        if (bytes_to_move) {
            memcpy(conn->client_stream,
                   req->client_stream + req->parse_pos, bytes_to_move);
            conn->client_stream_pos = bytes_to_move;
        }
        enqueue(&request_block, conn);
        BOA_FD_SET(conn->fd, &block_read_fdset);

        enqueue(&request_free, req);
        return;
    }

    /*
     While debugging some weird errors, Jon Nelson learned that
     some versions of Netscape Navigator break the
     HTTP specification.

     Some research on the issue brought up:

     http://www.apache.org/docs/misc/known_client_problems.html

     As quoted here:

     "
     Trailing CRLF on POSTs

     This is a legacy issue. The CERN webserver required POST
     data to have an extra CRLF following it. Thus many
     clients send an extra CRLF that is not included in the
     Content-Length of the request. Apache works around this
     problem by eating any empty lines which appear before a
     request.
     "

     Boa will (for now) hack around this stupid bug in Netscape
     (and Internet Exploder)
     by reading up to 32k after the connection is all but closed.
     This should eliminate any remaining spurious crlf sent
     by the client.

     Building bugs *into* software to be compatable is
     just plain wrong
     */
#if 0
    if (req->method == M_POST) {

        char buf[32768];
        read(req->fd, buf, 32768);

    }
#else
	if ( req->method == M_POST )
	{
		char buf[32768];

		if ( ( ! req->is_cgi ) || ( ! req->http_uri ) ||	// not cgi or not http_url
		     ( ! ( req->http_uri->uri_flag & URI_FLAG_DIRECT_WRITE ) ) ||	// CGI has finished          
		     ( ! ( req->http_uri->uri_flag & URI_FLAG_DIRECT_READ ) ) )		// CGI don't need direct stdin
			read( req->fd, buf, 32768 );
	}
#endif
    close(req->fd);
    total_connections--;
#ifdef SERVER_SSL
    SSL_free(req->ssl);
#endif /*SERVER_SSL*/

    enqueue(&request_free, req);

#ifdef DAVINCI_IPCAM
    if (req->req_flag.set_mac) {
        BIOS_DATA bios;
        char buffer[80];
        req->req_flag.set_mac = 0;
        if (bios_data_read(&bios) < 0)
            bios_data_set_default(&bios);
#ifdef PLAT_DM355
        sprintf(buffer, "ifconfig eth0 down hw ether %02X:%02X:%02X:%02X:%02X:%02X", bios.mac0[0],bios.mac0[1],bios.mac0[2],bios.mac0[3],bios.mac0[4],bios.mac0[5]);
        system(buffer);
#else
        sprintf(buffer, "ifconfig eth0 hw ether %02X:%02X:%02X:%02X:%02X:%02X", bios.mac0[0],bios.mac0[1],bios.mac0[2],bios.mac0[3],bios.mac0[4],bios.mac0[5]);
        system(buffer);
        system("ifconfig eth0 down");
#endif
        uninterrupted_sleep(1,0);
        system("ifconfig eth0 up");
//        net_nic_down("eth0");
//        net_set_hwaddr("eth0", bios.mac0);
//        uninterrupted_sleep(1,0);
//        net_nic_up("eth0");
    }

    if (req->req_flag.reboot) {
		req->req_flag.reboot = 0;
		fprintf(stderr, "System reboot....\n");
		system("/sbin/reboot");
    }

    if (req->req_flag.restart) {
        void boa_release(void);
        req->req_flag.restart = 0;
        switch(fork()) {
        case -1:
            /* error */
            perror("fork");
            exit(1);
            break;
        case 0:
            /* child, success */
            usleep(200000);
            dbg("restart boa ...");
//            system("/opt/ipnc/boa -c /etc &");
            execl("/opt/ipnc/boa", "boa", "-c", "/etc", NULL);
            perror("execl boa");
            _exit(1);
            break;
        default:
            /* parent, success */
            dbg("boa exit...");
            boa_release();
            exit(0);
            break;
        }
    }
#endif

    return;
}

/*
 * Name: process_requests
 *
 * Description: Iterates through the ready queue, passing each request
 * to the appropriate handler for processing.  It monitors the
 * return value from handler functions, all of which return -1
 * to indicate a block, 0 on completion and 1 to remain on the
 * ready list for more procesing.
 */

void process_requests(int server_s)
{
    int retval = 0;
    request *current, *trailer;
#ifdef DAVINCI_IPCAM
    HTTP_CONNECTION *connect;
    int busy;
    static time_t old_time;
#endif

#ifdef SERVER_SSL
    if(do_sock){
#endif /*SERVER_SSL*/
        if (pending_requests) {
            get_request(server_s);
#ifdef ORIGINAL_BEHAVIOR
            pending_requests = 0;
#endif
        }
#ifdef SERVER_SSL
    }
#endif /*SERVER_SSL*/

#ifdef SERVER_SSL
    if (do_sock < 2) {
        if(FD_ISSET(server_ssl, &block_read_fdset)){ /*If we have the main SSL server socket*/
/*         printf("SSL request received!!\n");*/
            get_ssl_request();
        }
    } 
#endif /*SERVER_SSL*/

	if (av_stream_pending == 1) {
		current=request_ready;
		while (current) {
			if (current->http_stream) {
				trailer = current;
				current = current->next;
				free_request(&request_ready, trailer);
			}
			else
				current = current->next;
		}
		rtsp_o_http_notify_restart();
		av_stream_pending = 2;
		ApproInterfaceExit();
	}
	else if (av_stream_pending == 2) {
		if (current_time - pending_time > 15)
			av_stream_pending = -1;
	}
	else if (av_stream_pending == -1) {
		if(ApproDrvInit(BOA_MSG_TYPE) < 0)
			exit(1);
    	if (func_get_mem(NULL)) {
	        ApproDrvExit();
    	    exit(1);
	    }
            start_rtsp_o_http_thread();
            av_stream_pending = 0;
	}

    current = request_ready;
#ifdef DAVINCI_IPCAM
    busy = 0;
    if (old_time != sys_info->second) {
        for (connect=connect_ready; connect; connect=connect->next) {
            if (connect != connect_guest || sys_info->user_auth_mode) {
                if (connect->counter == 0 && sys_info->second > connect->time_last+HTTP_CONNECTION_TIMEOUT)
                    free_connection(connect);
            }
        }
        old_time = sys_info->second;
    }
#endif
    while (current) {
        time(&current_time);
        if (current->buffer_end && /* there is data in the buffer */
            current->status != DEAD && current->status != DONE) {
            retval = req_flush(current);
            /*
             * retval can be -2=error, -1=blocked, or bytes left
             */
            if (retval == -2) { /* error */
                current->status = DEAD;
                retval = 0;
            } else if (retval >= 0) {
                /* notice the >= which is different from below?
                   Here, we may just be flushing headers.
                   We don't want to return 0 because we are not DONE
                   or DEAD */

                retval = 1;
            }
        } else {
            switch (current->status) {
            case READ_HEADER:
            case ONE_CR:
            case ONE_LF:
            case TWO_CR:
                retval = read_header(current);
                break;
            case BODY_READ:
                retval = read_body(current);
                break;
            case BODY_WRITE:
                retval = write_body(current);
                break;
            case WRITE:
                retval = process_get(current);
                break;
            case PIPE_READ:
                retval = read_from_pipe(current);
                break;
            case PIPE_WRITE:
                retval = write_from_pipe(current);
                break;
            case DONE:
                /* a non-status that will terminate the request */
                retval = req_flush(current);
                /*
                 * retval can be -2=error, -1=blocked, or bytes left
                 */
                if (retval == -2) { /* error */
                    current->status = DEAD;
                    retval = 0;
                } else if (retval > 0) {
                    retval = 1;
                }
                break;
            case DEAD:
                retval = 0;
                current->buffer_end = 0;
                SQUASH_KA(current);
                break;
            default:
                retval = 0;
                fprintf(stderr, "Unknown status (%d), "
                        "closing!\n", current->status);
                current->status = DEAD;
                break;
            }

        }

        if (sigterm_flag)
            SQUASH_KA(current);

        /* we put this here instead of after the switch so that
         * if we are on the last request, and get_request is successful,
         * current->next is valid!
         */
        if (pending_requests)
            get_request(server_s);

#ifdef DAVINCI_IPCAM
        busy |= current->busy_flag;
        current->busy_flag |= (BUSY_FLAG_AUDIO|BUSY_FLAG_VIDEO);
#ifdef CONFIG_STREAM_TIMEOUT
        if (current->last_stream_time && (sys_info->second > current->last_stream_time+STREAM_TIMEOUT_SEC)) {
#ifdef BOA_OSD_DEBUG
            static int count = 1;
            char buffer[30];
            sprintf(buffer, "stream failed %d", count++);
            osd_print(5, 10, buffer);
#endif  // BOA_OSD_DEBUG
            retval = 0;
        }
#endif  // CONFIG_STREAM_TIMEOUT
#endif  // DAVINCI_IPCAM

        switch (retval) {
        case -1:               /* request blocked */
            trailer = current;
            current = current->next;
            block_request(trailer);
            break;
        case 0:                /* request complete */
            current->time_last = current_time;
            trailer = current;
            current = current->next;
            free_request(&request_ready, trailer);
            break;
        case 1:                /* more to do */
            current->time_last = current_time;
            current = current->next;
            break;
        case 2:
            if ((current->http_uri) && (current->http_uri->uri_flag & URI_FLAG_RTSP_O_HTTP)) {
                trailer = current->next;
		if ( ! ( current = rtsp_o_http_insert_req( current ) ) )
                    current = trailer;
                break;
            }
            /* go through */
        default:
            log_error_time();
            fprintf(stderr, "Unknown retval in process.c - "
                    "Status: %d, retval: %d\n", current->status, retval);
            current = current->next;
            break;
        }
    }
#ifdef DAVINCI_IPCAM
    sys_info->status.http= (busy & BUSY_FLAG_STREAM);
    if ((busy & (BUSY_FLAG_AUDIO|BUSY_FLAG_VIDEO)) == 0) {
//		fprintf(stderr, "S", busy);
		usleep(100);
    }
	else {
//		fprintf(stderr, "B", busy);
	}
#endif	
}

/*
 * Name: process_logline
 *
 * Description: This is called with the first req->header_line received
 * by a request, called "logline" because it is logged to a file.
 * It is parsed to determine request type and method, then passed to
 * translate_uri for further parsing.  Also sets up CGI environment if
 * needed.
 */

int process_logline(request * req)
{
    char *stop, *stop2;
    static char *SIMPLE_HTTP_VERSION = "HTTP/0.9";

    req->logline = req->client_stream;
    if (!memcmp(req->logline, "GET ", 4)) {
        req->method = M_GET;
        stop = req->logline + 3;
    }
    else if (!memcmp(req->logline, "HEAD ", 5)) {
        /* head is just get w/no body */
        req->method = M_HEAD;
        stop = req->logline + 4;
    }
    else if (!memcmp(req->logline, "POST ", 5)) {
        req->method = M_POST;
        stop = req->logline + 4;
    }
    else if (!memcmp(req->logline, "DESCRIBE ", 9)) {
        req->method = M_GET;	/* treat to rtsp */
        stop = req->logline + 8;
    }
    else {
        log_error_time();
        fprintf(stderr, "malformed request: \"%s\"\n", req->logline);
        send_r_not_implemented(req);
        return 0;
    }

    req->http_version = SIMPLE_HTTP_VERSION;
    req->simple = 1;

    /* Guaranteed to find ' ' since we matched a method above */
//    stop = req->logline + 3;
//    if (*stop != ' ')
//        ++stop;

    /* scan to start of non-whitespace */
    while (*(++stop) == ' ');

    stop2 = stop;

    /* scan to end of non-whitespace */
    while (*stop2 != '\0' && *stop2 != ' ')
        ++stop2;

    if (stop2 - stop > MAX_HEADER_LENGTH) {
        log_error_time();
        fprintf(stderr, "URI too long %d: \"%s\"\n", MAX_HEADER_LENGTH,
                req->logline);
        send_r_bad_request(req);
        return 0;
    }
    memcpy(req->request_uri, stop, stop2 - stop);
    req->request_uri[stop2 - stop] = '\0';

    if (*stop2 == ' ') {
        /* if found, we should get an HTTP/x.x */
        unsigned int p1, p2;

        /* scan to end of whitespace */
        ++stop2;
        while (*stop2 == ' ' && *stop2 != '\0')
            ++stop2;

        /* scan in HTTP/major.minor */
        if (sscanf(stop2, "HTTP/%u.%u", &p1, &p2) == 2) {
            /* HTTP/{0.9,1.0,1.1} */
            if (p1 == 1 && (p2 == 0 || p2 == 1)) {
                req->http_version = stop2;
                req->simple = 0;
            } else if (p1 > 1 || (p1 != 0 && p2 > 1)) {
                goto BAD_VERSION;
            }

        } else if (sscanf(stop2, "RTSP/%u.%u", &p1, &p2) == 2) {

            memcpy(stop2, "HTTP/1.1", 8);
            req->http_version = stop2;
            for (stop += 8; *stop != '/' && *stop != 0; stop++);
            for (stop2 = stop + 1; *stop2 != ' ' && *stop2 != 0; stop2++);
            if (*stop) {
                memcpy(req->request_uri, stop, stop2 - stop);
                req->request_uri[stop2 - stop] = 0;
            } else
                strcpy(req->request_uri, "/");
            req->simple = 0;

        } else {
        	
            goto BAD_VERSION;
        }
    }

    if (req->method == M_HEAD && req->simple) {
        send_r_bad_request(req);
        return 0;
    }
    req->cgi_env_index = COMMON_CGI_COUNT;

    return 1;

BAD_VERSION:
    log_error_time();
    fprintf(stderr, "bogus HTTP version: \"%s\"\n", stop2);
    send_r_bad_request(req);
    return 0;
}

/*
 * Name: process_header_end
 *
 * Description: takes a request and performs some final checking before
 * init_cgi or init_get
 * Returns 0 for error or NPH, or 1 for success
 */

int process_header_end(request * req)
{
    if (!req->logline) {
        send_r_error(req);
        return 0;
    }

    /* Percent-decode request */
    if (unescape_uri(req->request_uri, &(req->query_string)) == 0) {
        log_error_doc(req);
        fputs("Problem unescaping uri\n", stderr);
        send_r_bad_request(req);
        return 0;
    }

    /* clean pathname */
    clean_pathname(req->request_uri);

    if (req->request_uri[0] != '/') {
        send_r_bad_request(req);
        return 0;
    }

    if (translate_uri(req) == 0) { /* unescape, parse uri */
        SQUASH_KA(req);
        return 0;               /* failure, close down */
    }

#ifdef DAVINCI_IPCAM
    req->authority = AUTHORITY_NONE;
    if (req->http_uri) {
		switch (auth_authorize(req)) {
		case ERROR_UNAUTHORIZED:
			if (req->http_uri->authority < req->authority) {
		 		send_r_unauthorized(req, server_name);
				return 0;
			}
			break;
		case ERROR_SERVICE_UNAVAILABLE:
			send_r_service_unavailable(req);
			return 0;
		case ERROR_BAD_REQUEST:
	        send_r_bad_request(req);
			return 0;
		default:
			if (req->http_uri->authority < req->authority) {
		 		send_r_unauthorized(req, server_name);
				return 0;
			}
			break;
		}

        if (req->http_uri->handler) {
            int ret = (*req->http_uri->handler)(req);
            if (ret >= 0)
               return ret;
        }
    }
#endif

    if (req->method == M_POST) {
        if (req->http_uri && (req->http_uri->uri_flag & URI_FLAG_DIRECT_READ)) {
            req->post_data_fd = dup(req->fd);
            return init_cgi(req);
        }
        req->post_data_fd = create_temporary_file(1, NULL, 0);
        if (req->post_data_fd == 0)
            return(0);
        return(1); /* success */
    }

    if (req->is_cgi) {
        return init_cgi(req);
    }

    req->status = WRITE;
    return init_get(req);       /* get and head */
}

/*
 * Name: process_option_line
 *
 * Description: Parses the contents of req->header_line and takes
 * appropriate action.
 */

int process_option_line(request * req)
{
    char c, *value, *line = req->header_line;

    /* Start by aggressively hacking the in-place copy of the header line */

#ifdef FASCIST_LOGGING
    log_error_time();
    fprintf(stderr, "%s:%d - Parsing \"%s\"\n", __FILE__, __LINE__, line);
#endif

    value = strchr(line, ':');
    if (value == NULL)
        return 0;
    *value++ = '\0';            /* overwrite the : */
    to_upper(line);             /* header types are case-insensitive */
    while ((c = *value) && (c == ' ' || c == '\t'))
        value++;

    if (!memcmp(line, "IF_MODIFIED_SINCE", 18) && !req->if_modified_since)
        req->if_modified_since = value;

    else if (!memcmp(line, "CONTENT_TYPE", 13) && !req->content_type)
        req->content_type = value;

    else if (!memcmp(line, "CONTENT_LENGTH", 15) && !req->content_length)
        req->content_length = value;

#ifdef DAVINCI_IPCAM
    else if (!memcmp(line,"AUTHORIZATION", 14) && !req->authorization)
        req->authorization = value;
#endif

    else if (!memcmp(line, "CONNECTION", 11) &&
             ka_max && req->keepalive != KA_STOPPED) {
        req->keepalive = (!strncasecmp(value, "Keep-Alive", 10) ?
                          KA_ACTIVE : KA_STOPPED);
    }
    /* #ifdef ACCEPT_ON */
    else if (!memcmp(line, "ACCEPT", 7))
        add_accept_header(req, value);
    /* #endif */

    /* Need agent and referer for logs */
    else if (!memcmp(line, "REFERER", 8)) {
        req->header_referer = value;
        if (!add_cgi_env(req, "REFERER", value, 1))
            return 0;
    } else if (!memcmp(line, "USER_AGENT", 11)) {
        req->header_user_agent = value;
        if (!add_cgi_env(req, "USER_AGENT", value, 1))
            return 0;
    } else if (!memcmp( line, "X_SESSIONCOOKIE", 15)) {
#if 0	/* old style */
        if (!add_cgi_env(req, "SESSIONKEY", value, 0))
            return 0;
#endif	/* old style */
        strncpy( req->sessionid, value, 27 );
    }else {
        if (!add_cgi_env(req, line, value, 1))
            return 0;
    }
    return 1;
}

/*
 * Name: add_accept_header
 * Description: Adds a mime_type to a requests accept char buffer
 *   silently ignore any that don't fit -
 *   shouldn't happen because of relative buffer sizes
 */

void add_accept_header(request * req, char *mime_type)
{
#ifdef ACCEPT_ON
    int l = strlen(req->accept);
    int l2 = strlen(mime_type);

    if ((l + l2 + 2) >= MAX_HEADER_LENGTH)
        return;

    if (req->accept[0] == '\0')
        strcpy(req->accept, mime_type);
    else {
        req->accept[l] = ',';
        req->accept[l + 1] = ' ';
        memcpy(req->accept + l + 2, mime_type, l2 + 1);
        /* the +1 is for the '\0' */
        /*
           sprintf(req->accept + l, ", %s", mime_type);
         */
    }
#endif
}

void free_requests( void )
{
	request *next;

	while ( request_free != NULL )
	{
		next = request_free->next;
		free( request_free );
		request_free = next;
	}
}

static int rtsp_over_http( request *req )
{
	static unsigned pseudosessionid = 1;
	char stuff[30];

	if ( req->method == M_GET )
	{
		if ( *req->sessionid == 0 )
		{
			sprintf( req->sessionid, "%010d", pseudosessionid++ );
			fprintf( stderr, "no session id, assign to %s\n", req->sessionid );
		}
		req->kacount = KA_ACTIVE;
		req->response_status = R_REQUEST_OK;
		req_write( req, "HTTP/1.0 200 OK\r\n" );
		req_write( req, "Content-Type: application/x-rtsp-tunnelled\r\n" );
		req_write( req, "Date: " );
		rfc822_time_buf( stuff, 0 );
		stuff[29] = 0;
		req_write( req, stuff );
		req_write( req, "\r\nServer: " SERVER_VERSION "\r\n" );
		req_write( req, "Accept-Ranges: bytes\r\n" );
		req_write( req, "X-SessionCookie: " );
		snprintf( stuff, 29, "%s\r\n", req->sessionid );
		req_write( req, stuff );
		req_write( req, "Connection: close\r\n\r\n" );
		req->status = WRITE;
		req_flush( req );
	 	return 2;
	}
	else if ( req->method == M_POST )
	{
		req->kacount = KA_ACTIVE;
		req->status = BODY_READ;
		return 2;
	}
	req->status = DONE;
	return 0;
}
