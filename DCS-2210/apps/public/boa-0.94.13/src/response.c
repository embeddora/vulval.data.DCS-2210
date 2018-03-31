/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@boa.org>
 *  Some changes Copyright (C) 1996-99 Jon Nelson <jnelson@boa.org>
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

/* $Id: response.c,v 1.41.2.1 2002/06/06 05:08:54 jnelson Exp $*/

#include <sysinfo.h>
#include <storage.h>
#include <ipnc_define.h>

#include "boa.h"
#include "acs.h"

#define HTML "text/html; charset=ISO-8859-1"
#define CRLF "\r\n"


extern SysInfo *sys_info;

void print_content_type(request * req)
{
    req_write(req, "Content-Type: ");
    req_write(req, get_mime_type(req->request_uri));
    req_write(req, "\r\n");
}

void print_content_length(request * req)
{
    req_write(req, "Content-Length: ");
    req_write(req, simple_itoa(req->filesize+req->extra_size));
    req_write(req, "\r\n");
}

void print_last_modified(request * req)
{
    static char lm[] = "Last-Modified: "
        "                             " "\r\n";
    rfc822_time_buf(lm + 15, req->last_modified);
    req_write(req, lm);
}

void print_ka_phrase(request * req)
{
    if (req->kacount > 0 &&
        req->keepalive == KA_ACTIVE && req->response_status < 500) {
        req_write(req, "Connection: Keep-Alive\r\nKeep-Alive: timeout=");
        req_write(req, simple_itoa(ka_timeout));
        req_write(req, ", max=");
        req_write(req, simple_itoa(req->kacount));
        req_write(req, "\r\n");
    } else
        req_write(req, "Connection: close\r\n");
}
void print_dlink_http_headers(request * req)
{
    static char stuff[] = "Date: "
        "                             "
        "\r\nServer: dcs-lig-httpd\r\n";  //SERVER_VERSION

    rfc822_time_buf(stuff + 6, 0);
    req_write(req, stuff);
   
}
void print_http_headers(request * req)
{
    static char stuff[] = "Date: "
        "                             "
        "\r\nServer: " SERVER_VERSION "\r\n";

    rfc822_time_buf(stuff + 6, 0);
    req_write(req, stuff);
    print_ka_phrase(req);
}

/* The routines above are only called by the routines below.
 * The rest of Boa only enters through the routines below.
 */

/* R_REQUEST_OK: 200 */
void send_r_request_ok(request * req)
{
    req->response_status = R_REQUEST_OK;
    if (req->simple)
        return;

//    req_write(req, "HTTP/1.1 200 OK\r\nConnection: close\r\n");
    req_write(req, "HTTP/1.1 200 OK\r\n");
    print_http_headers(req);

    if (!req->is_cgi) {
        print_content_type(req);
        if (req->export_file) {
            req_write(req, "Content-Disposition: attachment; filename=");
            req_write(req, req->export_file);
            req_write(req, "\r\n");
        }
        print_content_length(req);
        print_last_modified(req);
        req_write(req, "\r\n");
    }
}

#ifdef DAVINCI_IPCAM
/*
void print_content_ext_length(request * req, int len)
{
    req_write(req, "Content-Length: ");
    req_write(req, simple_itoa(req->filesize+len));
    req_write(req, "\r\n");
}*/

void send_request_ok_no_cache(request * req)
{
    req->response_status = R_REQUEST_OK;

    req_write(req, "HTTP/1.1 200 OK\r\nPragma: no-cache\r\nCache-Control: no-store\r\nConnection: close\r\n");

    if (!req->is_cgi) {
        print_content_type(req);
        print_content_length(req);
        req_write(req, "\r\n");
    }
}

void send_request_ok_text(request * req)
{
    req->response_status = R_REQUEST_OK;

    req_write(req, "HTTP/1.1 200 OK\r\nContent-type: text/plain\r\nConnection: close\r\nPragma: no-cache\r\nCache-Control: no-store\r\n");
    print_content_length(req);
    req_write(req, "\r\n");
}
void send_request_ok_no_length(request * req)
{
    req->response_status = R_REQUEST_OK;

    req_write(req, "HTTP/1.1 200 OK\r\nContent-type: text/plain\r\nConnection: close\r\nPragma: no-cache\r\nCache-Control: no-store\r\n");
    req_write(req, "\r\n");
}

void send_request_ok_sdget(request * req)
{
    req->response_status = R_REQUEST_OK;

    req_write(req, "HTTP/1.1 200 OK\r\nContent-type: text/html\r\nConnection: close\r\nPragma: no-cache\r\nCache-Control: no-store\r\n");
    print_content_length(req);
    req_write(req, "\r\n");
}
void send_request_dlink_ok(request * req, int size)
{
		req->response_status = R_REQUEST_OK;
//    req_write(req, "HTTP/1.1 200 OK\r\nConnection: close\r\n");
    req_write(req, "HTTP/1.1 200 OK\r\n");

    print_last_modified(req);
    req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
    req_write(req, "Pragma: no-cache\r\n");
    req_write(req, "Content-type: text/plain\r\n");
	 	req_write(req, "Content-Length: ");
	 	req_write(req, simple_itoa(size));
	  req_write(req, "\r\n");
	 	req_write(req, "Connection: close\r\n");
    print_dlink_http_headers(req);
  	req_write(req, "\r\n");
}
void send_request_dlink_ok_mjpg(request * req)
{
	req->response_status = R_REQUEST_OK;
	req_write(req, "HTTP/1.1 200 OK\r\n");
//  print_last_modified(req);
  req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
  req_write(req, "Pragma: no-cache\r\n");
  req_write(req, "Content-Type: multipart/x-mixed-replace;boundary=myboundary\r\n");
	req_write(req, "\r\n");
	print_mjpeg_headers(req);
	//req_write(req, CRLF);
}
void send_request_dlink_ok_mpeg4(request * req)
{
	req->response_status = R_REQUEST_OK;
	req_write(req, "HTTP/1.1 200 OK\r\n");
  print_last_modified(req);
  req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
  req_write(req, "Pragma: no-cache\r\n");
  req_write(req, "Content-type: video/ACVS\r\n");
	req_write(req, "\r\n");
}
void send_request_dlink_ok_audio(request * req)
{
	req->response_status = R_REQUEST_OK;
	req_write(req, "HTTP/1.1 200 OK\r\n");
  print_last_modified(req);
  req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
  req_write(req, "Pragma: no-cache\r\n");
  req_write(req, "Content-type: audio/ACAS\r\n");
	req_write(req, "\r\n");
}
void send_request_uni_xml_ok(request * req)
{
		req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\n");
    req_write(req, "Transfer-Encoding: chunked\r\n");
    req_write(req, "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n");
    req_write(req, "Connection: close\r\n");
    print_last_modified(req);
    req_write(req, "Cache-Control: no-cache, no-store, must-revalidate\r\n");
    req_write(req, "Pragma: no-cache\r\n");
    req_write(req, "Content-type: text/xml\r\n");
    //req_write(req, "Content-Length: ");
    //req_write(req, simple_itoa(size));
    //req_write(req, "\r\n");
	 	print_dlink_http_headers(req);
  	req_write(req, "\r\n");
}
void send_request_ok_api(request * req)
{
    req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\nContent-type: text/html\r\nPragma: no-cache\r\nCache-Control: no-store\r\n\r\n");
}

void send_request_ok_jpeg(request * req)
{
    req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nPragma: no-cache\r\nCache-Control: no-store\r\n");
//    print_content_ext_length(req, sizeof(struct JPEG_HEADER));
    print_content_length(req);
    req_write(req, "\r\n");
}

inline void print_ipcam_status(request *req)
{
    unsigned int status = 0;
    char buf[32] = "";

    // Digital Input status bit 0 ~ bit 7
    if(sys_info->status.alarm1)
        status |= (1 << 0);
    if(sys_info->status.alarm2)
        status |= (1 << 1);
    if(sys_info->status.alarm3)
        status |= (1 << 2);
    if(sys_info->status.alarm4)
        status |= (1 << 3);
    if(sys_info->status.alarm5)
        status |= (1 << 4);
    if(sys_info->status.alarm6)
        status |= (1 << 5);

    // Video lost status bit 8 ~ bit 15
    if(sys_info->videoloss)
        status |= (1 << 8);

    // Video motion bit 16
    if(sys_info->status.motion)
        status |= (1 << 16);

    // Reserved bit 17 ~ bit 19
    // SD card status bit 20 and bit 21
    if(sys_info->status.mmc & SD_ON_DETECT)
        status |= (1 << 21);
    if(sys_info->status.mmc & SD_MOUNTED)
        status |= (2 << 21);
    if(sys_info->status.mmc & SD_FAIL)
        status |= (3 << 21);

    // SD card lock status bit 22
    if(sys_info->status.mmc & SD_LOCK)
        status |= (1 << 22);

    // Reserved bit 23
    // Digital output status
    if(sys_info->status.alarm_out1)
        status |= (1 << 24);

    // Digital output always on
    //if(sys_info->config.lan_config.gioout_alwayson[1])
    if(sys_info->ipcam[GIOOUT_ALWAYSON1].config.value)
        status |= (1 << 25);

    snprintf(buf, sizeof(buf), "X-Status: %08X"CRLF, status);
    req_write(req, buf);
}

inline void print_vszoom_tag(request *req)
{
    char zoom[5];
    req_write(req, "X-Zoom: ");
    sprintf(zoom, "%s" CRLF, sys_info->vsptz.zoomratio);
    req_write(req, zoom);
}

inline void print_video_tag(request *req)
{
    req_write(req, "X-Tag: ");
    req_write(req, simple_itoa(req->video.av_data.serial));
    req_write(req, CRLF);
}

inline void print_audio_tag(request *req)
{
    req_write(req, "X-Tag: ");
    req_write(req, simple_itoa(req->audio.av_data.serial));
    req_write(req, CRLF);
}

inline void print_stream_flag(request *req)
{
    req_write(req, "X-Flags: 0" CRLF);
}

inline void print_video_resolution(request *req)
{
    char res[32];
    req_write(req, "X-Resolution: ");
    sprintf(res, "%d*%d" CRLF, req->video.av_data.width, req->video.av_data.height);
    req_write(req, res);
}

void print_video_timestamp(request *req)
{
	char buffer[32];
	sprintf(buffer, "X-Timestamp: %lld" CRLF, req->video.av_data.timestamp);
    req_write(req, buffer);
}

void print_audio_timestamp(request *req)
{
	char buffer[32];
	sprintf(buffer, "X-Timestamp: %lld" CRLF, req->audio.av_data.timestamp);
    req_write(req, buffer);
}

inline void print_audio_state(request *req)
{
    //if (sys_info->config.lan_config.audioinenable && req->req_flag.audio_on)
    if (sys_info->ipcam[AUDIOINENABLE].config.u8 && req->req_flag.audio_on)
        req_write(req, "X-Audio: 1" CRLF);
    else
        req_write(req, "X-Audio: 0" CRLF);
}

inline void print_frame_rate(request *req)
{
	char buffer[32];
    sprintf(buffer, "X-Framerate: %.2f" CRLF, req->video.av_data.framerate);
    req_write(req, buffer);
}

void write_time_string(char *buf)
{
    struct tm *t;
    char *p;
    unsigned int a;

    t = gmtime(&current_time);

    p = buf + 18;
    /* p points to the last char in the buf */

    a = t->tm_sec;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_min;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ':';
    a = t->tm_hour;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = ' ';

    a = t->tm_mday;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = '-';
    a = t->tm_mon+1;
    *p-- = '0' + a % 10;
    *p-- = '0' + a / 10;
    *p-- = '-';
    a = 1900 + t->tm_year;
    while (a) {
        *p-- = '0' + a % 10;
        a /= 10;
    }
}
void print_dlink_mpeg4_headers(request * req)
{
	ACS_VideoHeader MPEG4HEADER;
	//time_t dlinktime = time(NULL);
	struct timeval tv;
	gettimeofday(&tv,NULL);
	req->response_status = R_REQUEST_OK;	
//  	int prof_val;
  	
	MPEG4HEADER.ulHdrID 			= 0xF5010000;
	MPEG4HEADER.ulHdrLength 		= sizeof(ACS_VideoHeader);	
	MPEG4HEADER.ulDataLength 		= req->filesize + req->extra_size;
	MPEG4HEADER.ulSequenceNumber	= req->video.av_data.serial;
	MPEG4HEADER.ulTimeSec			= (unsigned long)tv.tv_sec;	
	MPEG4HEADER.ulTimeUSec			= (unsigned long)tv.tv_usec;
	memcpy(&MPEG4HEADER.ulDataCheckSum, req->video.av_data.ptr + req->video.av_data.size -4, 4);	
	if(req->video.av_data.frameType == AV_FRAME_TYPE_I_FRAME)
		MPEG4HEADER.usCodingType	= 0;
	else 
		MPEG4HEADER.usCodingType	= 1;

	MPEG4HEADER.usFrameRate 		= (unsigned short)req->video.av_data.framerate;
	MPEG4HEADER.usWidth 			= req->video.av_data.width;
	MPEG4HEADER.usHeight			= req->video.av_data.height;
	MPEG4HEADER.ucMDBitmap			= 0;
	memset(MPEG4HEADER.ucMDPowers, 0, sizeof(MPEG4HEADER.ucMDPowers));
#if 1
	req_copy(req, (char *)&MPEG4HEADER, sizeof(MPEG4HEADER));
#else
	memcpy(req->buffer + req->buffer_end, &MPEG4HEADER, sizeof(MPEG4HEADER));
	req->buffer_end += sizeof(MPEG4HEADER);
#endif
}
void print_dlink_audio_headers(request * req)
{
	ACS_AudioHeader AUDIOHEADER;
	//time_t dlinktime = time(NULL);
	struct timeval tv;
	gettimeofday(&tv,NULL);
	req->response_status = R_REQUEST_OK;
	AUDIOHEADER.ulHdrID 				= 0xF6010000;
	AUDIOHEADER.ulHdrLength 			= sizeof(ACS_AudioHeader);	
	AUDIOHEADER.ulDataLength 			= req->filesize;
	AUDIOHEADER.ulSequenceNumber		= req->audio.av_data.serial;
	AUDIOHEADER.ulTimeSec				= (unsigned long)tv.tv_sec;
	AUDIOHEADER.ulTimeUSec				= (unsigned long)tv.tv_usec;
	memcpy(&AUDIOHEADER.ulDataCheckSum, req->data_mem + req->filesize -4, 4);
	AUDIOHEADER.usFormat				= 0x1000;
	AUDIOHEADER.usChannels				= 1;
	AUDIOHEADER.usSampleRate			= 8000;
	AUDIOHEADER.usSampleBits			= 4;
	AUDIOHEADER.ulReserved				= 0;
	memcpy(req->buffer + req->buffer_end, &AUDIOHEADER, sizeof(AUDIOHEADER));
	//fprintf(stderr, "AUDIOHEADERLength = %d\n", sizeof(AUDIOHEADER));
	
	req->buffer_end += sizeof(AUDIOHEADER);
}

void print_mjpeg_headers(request * req)
{
    req_write(req, HTTP_BOUNDARY "\r\nContent-Type: image/jpeg\r\n");
    print_content_length(req);

	print_ipcam_status(req);
	print_video_tag(req);
	print_stream_flag(req);
	print_frame_rate(req);
	print_video_resolution(req);
	print_audio_state(req);
	print_video_timestamp(req);
	print_vszoom_tag(req);

    req_write(req, CRLF);
}

void print_mpeg4_headers(request * req)
{
    req_write(req, HTTP_BOUNDARY "\r\nContent-Type: image/mpeg4\r\n");
    print_content_length(req);

	print_ipcam_status(req);
	print_video_tag(req);
	print_stream_flag(req);
	print_frame_rate(req);
	print_video_resolution(req);
	print_audio_state(req);
	print_video_timestamp(req);
	print_vszoom_tag(req);

    req_write(req, CRLF);
}

void print_H264_headers( request *req )
{
    req_write( req, HTTP_BOUNDARY CRLF "Content-Type: video/x-h264" CRLF );
    print_content_length(req);

	print_ipcam_status(req);
	print_video_tag(req);
	print_stream_flag(req);
	print_frame_rate(req);
	print_video_resolution(req);
	print_audio_state(req);
	print_video_timestamp(req);
	print_vszoom_tag(req);

    req_write(req, CRLF);
}

void print_audio_headers(request * req)
{
    //req_write(req, HTTP_BOUNDARY "\r\nContent-Type: audio/wav\r\n");
    //print_content_length(req);
    //if( strcmp( sys_info->config.audio_info.codec_name , AUDIO_CODEC_G726 ) == 0 ){
    if( strcmp( conf_string_read(AUDIO_INFO_CODEC_NAME) , AUDIO_CODEC_G726 ) == 0 ){
    	req_write(req, HTTP_BOUNDARY "\r\nContent-Type: audio/x-adpcm\r\n");
    	print_content_length(req);
    	req_write(req, "X-Codec: G726" CRLF);
	req_write(req, "X-Bitrate: 32000" CRLF);
    //}else if( strcmp( sys_info->config.audio_info.codec_name , AUDIO_CODEC_G711 ) == 0 ){
    }else if( strcmp( conf_string_read(AUDIO_INFO_CODEC_NAME) , AUDIO_CODEC_G711 ) == 0 ){
	req_write(req, HTTP_BOUNDARY "\r\nContent-Type: audio/x-mulaw\r\n");
    	print_content_length(req);
    	req_write(req, "X-Codec: U-LAW" CRLF);
	req_write(req, "X-Bitrate: 64000" CRLF);
    }else{
	req_write(req, HTTP_BOUNDARY "\r\nContent-Type: audio/aac\r\n");
	print_content_length(req);
    	req_write(req, "X-Codec: U-LAW" CRLF);
	req_write(req, "X-Bitrate: 64000" CRLF);
    }
   
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
    req_write(req, "X-Tag: -1" CRLF);
#else
    req_write(req, "X-Tag: ");
    req_write(req, simple_itoa(req->audio.serial_lock));
    req_write(req, CRLF);
#endif
	print_audio_timestamp(req);
    req_write(req, CRLF);
}

void send_request_ok_mjpeg(request * req)
{
    req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\nConnection: Close\r\nPragma: no-cache\r\nCache-Control: no-store\r\nContent-Type: multipart/x-mixed-replace;boundary=" HTTP_BOUNDARY "\r\n\r\n");
    print_mjpeg_headers(req);
}

void send_request_ok_mpeg4(request * req)
{
    req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\nConnection: Close\r\nPragma: no-cache\r\nCache-Control: no-store\r\nContent-Type: multipart/x-mixed-replace;boundary=" HTTP_BOUNDARY "\r\n\r\n");
    print_mpeg4_headers(req);
}

void send_request_ok_H264( request *req )
{
	req->response_status = R_REQUEST_OK;
	req_write( req, "HTTP/1.1 200 OK\r\nConnection: Close\r\nPragma: no-cache\r\nCache-Control: no-store\r\nContent-Type: multipart/x-mixed-replace;boundary=" HTTP_BOUNDARY "\r\n\r\n" );
	print_H264_headers( req );
}
void send_request_ok_audio(request * req)
{
    req->response_status = R_REQUEST_OK;
    req_write(req, "HTTP/1.1 200 OK\r\nConnection: Close\r\nPragma: no-cache\r\nCache-Control: no-store\r\nContent-Type: multipart/x-mixed-replace;boundary=" HTTP_BOUNDARY "\r\n\r\n");
    //print_audio_headers(req);
}

#endif

/* R_MOVED_PERM: 301 */
void send_r_moved_perm(request * req, char *url)
{
    SQUASH_KA(req);
    req->response_status = R_MOVED_PERM;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 301 Moved Permanently\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n");

        req_write(req, "Location: ");
        req_write_escape_http(req, url);
        req_write(req, "\r\n\r\n");
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>301 Moved Permanently</TITLE></HEAD>\n"
                  "<BODY>\n<H1>301 Moved</H1>The document has moved\n"
                  "<A HREF=\"");
        req_write_escape_html(req, url);
        req_write(req, "\">here</A>.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_MOVED_TEMP: 302 */
void send_r_moved_temp(request * req, char *url, char *more_hdr)
{
    SQUASH_KA(req);
    req->response_status = R_MOVED_TEMP;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 302 Moved Temporarily\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n");

        req_write(req, "Location: ");
        req_write_escape_http(req, url);
        req_write(req, "\r\n");
        req_write(req, more_hdr);
        req_write(req, "\r\n\r\n");
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>302 Moved Temporarily</TITLE></HEAD>\n"
                  "<BODY>\n<H1>302 Moved</H1>The document has moved\n"
                  "<A HREF=\"");
        req_write_escape_html(req, url);
        req_write(req, "\">here</A>.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_NOT_MODIFIED: 304 */
void send_r_not_modified(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_NOT_MODIFIED;
    req_write(req, "HTTP/1.0 304 Not Modified\r\n");
    print_http_headers(req);
    print_content_type(req);
    req_write(req, "\r\n");
    req_flush(req);
}

/* R_BAD_REQUEST: 400 */
void send_r_bad_request(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_BAD_REQUEST;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 400 Bad Request\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD)
        req_write(req,
                  "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n"
                  "<BODY><H1>400 Bad Request</H1>\nYour client has issued "
                  "a malformed or illegal request.\n</BODY></HTML>\n");
    req_flush(req);
}

/* R_UNAUTHORIZED: 401 */
void send_r_unauthorized(request * req, char *realm_name)
{
    SQUASH_KA(req);
    req->response_status = R_UNAUTHORIZED;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 401 Unauthorized\r\n");
        print_http_headers(req);
        req_write(req, "WWW-Authenticate: Basic realm=\"");
        req_write(req, realm_name);
        req_write(req, "\"\r\n");
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>401 Unauthorized</TITLE></HEAD>\n"
                  "<BODY><H1>401 Unauthorized</H1>\nYour client does not "
                  "have permission to get URL ");
        req_write_escape_html(req, req->request_uri);
        req_write(req, " from this server.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_FORBIDDEN: 403 */
void send_r_forbidden(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_FORBIDDEN;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 403 Forbidden\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n"
                  "<BODY><H1>403 Forbidden</H1>\nYour client does not "
                  "have permission to get URL ");
        req_write_escape_html(req, req->request_uri);
        req_write(req, " from this server.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_NOT_FOUND: 404 */
void send_r_not_found(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_NOT_FOUND;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 404 Not Found\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                  "<BODY><H1>404 Not Found</H1>\nThe requested URL ");
        req_write_escape_html(req, req->request_uri);
        req_write(req, " was not found on this server.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_ERROR: 500 */
void send_r_error(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_ERROR;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 500 Server Error\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>500 Server Error</TITLE></HEAD>\n"
                  "<BODY><H1>500 Server Error</H1>\nThe server encountered "
                  "an internal error and could not complete your request.\n"
                  "</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_NOT_IMP: 501 */
void send_r_not_implemented(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_NOT_IMP;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 501 Not Implemented\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n"
                  "<BODY><H1>501 Not Implemented</H1>\nPOST to non-script "
                  "is not supported in Boa.\n</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_BAD_GATEWAY: 502 */
void send_r_bad_gateway(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_BAD_GATEWAY;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 502 Bad Gateway" CRLF);
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML CRLF CRLF); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>502 Bad Gateway</TITLE></HEAD>\n"
                  "<BODY><H1>502 Bad Gateway</H1>\nThe CGI was "
                  "not CGI/1.1 compliant.\n" "</BODY></HTML>\n");
    }
    req_flush(req);
}

/* R_SERVICE_UNAVAILABLE: 503 */
void send_r_service_unavailable(request * req) /* 503 */
{
    static char body[] =
        "<HTML><HEAD><TITLE>503 Service Unavailable</TITLE></HEAD>\n"
        "<BODY><H1>503 Service Unavailable</H1>\n"
        "There are too many connections in use right now.\r\n"
        "Please try again later.\r\n</BODY></HTML>\n";
    static int _body_len;
    static char *body_len;

    if (!_body_len)
        _body_len = strlen(body);
    if (!body_len)
        body_len = strdup(simple_itoa(_body_len));
    if (!body_len) {
        log_error_time();
        perror("strdup of _body_len from simple_itoa");
    }


    SQUASH_KA(req);
    req->response_status = R_SERVICE_UNAV;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 503 Service Unavailable\r\n");
        print_http_headers(req);
        if (body_len) {
            req_write(req, "Content-Length: ");
            req_write(req, body_len);
            req_write(req, "\r\n");
        }
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header
                                                               */
    }
    if (req->method != M_HEAD) {
        req_write(req, body);
    }
    req_flush(req);
}


/* R_NOT_IMP: 505 */
void send_r_bad_version(request * req)
{
    SQUASH_KA(req);
    req->response_status = R_BAD_VERSION;
    if (!req->simple) {
        req_write(req, "HTTP/1.0 505 HTTP Version Not Supported\r\n");
        print_http_headers(req);
        req_write(req, "Content-Type: " HTML "\r\n\r\n"); /* terminate header */
    }
    if (req->method != M_HEAD) {
        req_write(req,
                  "<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n"
                  "<BODY><H1>505 HTTP Version Not Supported</H1>\nHTTP versions "
                  "other than 0.9 and 1.0 "
                  "are not supported in Boa.\n<p><p>Version encountered: ");
        req_write(req, req->http_version);
        req_write(req, "<p><p></BODY></HTML>\n");
    }
    req_flush(req);
}
