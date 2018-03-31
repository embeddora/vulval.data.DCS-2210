/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996,99 Larry Doolittle <ldoolitt@boa.org>
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

/* $Id: get.c,v 1.76.2.5 2002/07/26 03:05:59 jnelson Exp $*/

#include "boa.h"
#include "acs.h"
#include "sysinfo.h"
#include "rtsp_o_http.h"
#ifdef SERVER_SSL
#include <openssl/ssl.h>
#endif
#ifdef DAVINCI_IPCAM
#define LOCAL_DEBUG
#include <debug.h>
#include "web_translate.h"
#include "defines.h"
#endif
int nowtime = 0;
int timecnt = 0;
char oldbuf[256];

/* local prototypes */
int get_cachedir_file(request * req, struct stat *statbuf);
int index_directory(request * req, char *dest_filename);

#ifdef DAVINCI_IPCAM

int dbg_log(char *data)
{
	static FILE *fptr = NULL;

	if (fptr == NULL)
		fptr = fopen("/mnt/data/boa_log.txt", "a");

	if (fptr) {
		fprintf(fptr, "%d/%d/%d %02d%02d%02d %s\n", sys_info->tm_now.tm_year, sys_info->tm_now.tm_mon, sys_info->tm_now.tm_mday,
			sys_info->tm_now.tm_hour, sys_info->tm_now.tm_min, sys_info->tm_now.tm_sec, data);
		return 0;
	}
	return -1;
}

char **strrfind(char *strdata)
{
	static char *tag[2];
	
	while (*strdata) {
		if (strdata[0] == '<' && strdata[1] == '%') {
			tag[0] = strdata;
			strdata += 2;
			while (*strdata) {
				if (strdata[0] == '%' && strdata[1] == '>') {
					tag[1] = strdata;
					return tag;
				}
				else if (strdata[0] == '<' && strdata[1] == '%') {
					tag[0] = strdata;
					strdata += 2;
				}
				else
					strdata++;
			}
			return NULL;
		}
		else
			strdata++;
	}
	return NULL;
}


int html_argument_parse(request *req, char *data, char *result)
{
	int len1, len2, size, total_size=0;
	char **tag, *arg, *arg1, zero_str[1]={0x00};

	while (1) {		
		tag = strrfind(data);
		if (tag) {
			len1 = tag[0]-data;
			arg = tag[0]+2;

			len2 = tag[1]-data+2;
			memcpy(result, data, len2);
			*tag[1] = '\0';

			arg1 = strchr(arg, '.');
			if (arg1) 
				*arg1++ = '\0';
			else
				arg1 = zero_str;
			
			size = TranslateWebPara(req, result+len1, arg, arg1);
	
			if (size < 0) {
				result += len2;
				total_size += len2;
			}
			else {
				result += (len1+size);
				total_size += (len1+size);
			}
			data += len2;
		}
		else {
			size = strlen(data);
			total_size += size;
			memcpy(result, data, size);
			break;
		}
	}
	return total_size;
}
#endif  // DAVINCI_IPCAM

/*
 * Name: init_get
 * Description: Initializes a non-script GET or HEAD request.
 *
 * Return values:
 *   0: finished or error, request will be freed
 *   1: successfully initialized, added to ready queue
 */

int init_get(request * req)
{
    int data_fd, saved_errno, dynamic=0;
    struct stat statbuf;
    volatile int bytes;

    data_fd = open(req->pathname, O_RDONLY);
    saved_errno = errno; /* might not get used */

#ifdef GUNZIP
    if (data_fd == -1 && errno == ENOENT) {
        /* cannot open */
        /* it's either a gunzipped file or a directory */
        char gzip_pathname[MAX_PATH_LENGTH];
        int len;

        len = strlen(req->pathname);

        memcpy(gzip_pathname, req->pathname, len);
        memcpy(gzip_pathname + len, ".gz", 3);
        gzip_pathname[len + 3] = '\0';
        data_fd = open(gzip_pathname, O_RDONLY);
        if (data_fd != -1) {
            close(data_fd);

            req->response_status = R_REQUEST_OK;
            if (req->pathname)
                free(req->pathname);
            req->pathname = strdup(gzip_pathname);
            if (!req->pathname) {
                log_error_time();
                perror("strdup");
                send_r_error(req);
                return 0;
            }
            if (!req->simple) {
                req_write(req, "HTTP/1.0 200 OK-GUNZIP\r\n");
                print_http_headers(req);
                print_content_type(req);
                print_last_modified(req);
                req_write(req, "\r\n");
                req_flush(req);
            }
            if (req->method == M_HEAD)
                return 0;

            return init_cgi(req);
        }
    }
#endif

    if (data_fd == -1) {
        log_error_doc(req);
        errno = saved_errno;
        perror("document open");

        if (saved_errno == ENOENT)
            send_r_not_found(req);
        else if (saved_errno == EACCES)
            send_r_forbidden(req);
        else
            send_r_bad_request(req);
        return 0;
    }

    fstat(data_fd, &statbuf);

    if (S_ISDIR(statbuf.st_mode)) { /* directory */
        close(data_fd);         /* close dir */

        if (req->pathname[strlen(req->pathname) - 1] != '/') {
            char buffer[3 * MAX_PATH_LENGTH + 128];

            if (server_port != 80)
                sprintf(buffer, "http://%s:%d%s/", server_name,
                        server_port, req->request_uri);
            else
                sprintf(buffer, "http://%s%s/", server_name,
                        req->request_uri);
            send_r_moved_perm(req, buffer);
            return 0;
        }
        data_fd = get_dir(req, &statbuf); /* updates statbuf */

        if (data_fd == -1)      /* couldn't do it */
            return 0;           /* errors reported by get_dir */
        else if (data_fd <= 1)
            /* data_fd == 0 -> close it down, 1 -> continue */
            return data_fd;
        /* else, data_fd contains the fd of the file... */
    }
#ifdef DAVINCI_IPCAM
    if (req->http_uri)
        dynamic = req->http_uri->uri_flag & (URI_FLAG_NEED_PARSE|URI_FLAG_VOLATILE_FILE);
#endif
    if (req->if_modified_since && !dynamic &&
        !modified_since(&(statbuf.st_mtime), req->if_modified_since)) {
        send_r_not_modified(req);
        close(data_fd);
        return 0;
    }
    req->filesize = statbuf.st_size;
    req->last_modified = statbuf.st_mtime;

    if (req->method == M_HEAD || req->filesize == 0) {
        send_r_request_ok(req);
        close(data_fd);
        return 0;
    }

    if (req->filesize > MAX_FILE_MMAP) {
        send_r_request_ok(req); /* All's well */
        req->status = PIPE_READ;
        req->cgi_status = CGI_BUFFER;
        req->data_fd = data_fd;
        req_flush(req);         /* this should *always* complete due to
                                   the size of the I/O buffers */
        req->header_line = req->header_end = req->buffer;
        return 1;
    }

    if (req->filesize == 0) {  /* done */
        send_r_request_ok(req);     /* All's well *so far* */
        close(data_fd);
        return 1;
    }

    /* NOTE: I (Jon Nelson) tried performing a read(2)
     * into the output buffer provided the file data would
     * fit, before mmapping, and if successful, writing that
     * and stopping there -- all to avoid the cost
     * of a mmap.  Oddly, it was *slower* in benchmarks.
     */
    req->mmap_entry_var = find_mmap(data_fd, &statbuf);
    if (req->mmap_entry_var == NULL) {
        req->buffer_end = 0;
        if (errno == ENOENT)
            send_r_not_found(req);
        else if (errno == EACCES)
            send_r_forbidden(req);
        else
            send_r_bad_request(req);
        close(data_fd);
        return 0;
    }
    req->data_mem = req->mmap_entry_var->mmap;
    close(data_fd);             /* close data file */

    if ((long) req->data_mem == -1) {
        boa_perror(req, "mmap");
        return 0;
    }

#ifdef DAVINCI_IPCAM
    if (dynamic & URI_FLAG_NEED_PARSE) {
            char *addr = (char *)malloc(req->filesize + (HTTP_PARSE_LENGTH + 1) );
            if (addr) {
                req->mem_flag |= MFLAG_IS_MEMORY;
                req->mmap_ptr = req->data_mem;
                req->mmap_size = req->filesize;
                memcpy(addr+HTTP_PARSE_LENGTH, req->data_mem, req->filesize);
                addr[req->filesize+HTTP_PARSE_LENGTH] = '\0';
                req->data_mem = addr;
                req->filesize = html_argument_parse(req, addr+HTTP_PARSE_LENGTH, req->data_mem);
                send_request_ok_no_cache(req);     /* All's well */
                return 1;
            }
    }
#endif  // DAVINCI_IPCAM

    send_r_request_ok(req);     /* All's well */

    bytes = BUFFER_SIZE - req->buffer_end;

    /* bytes is now how much the buffer can hold
     * after the headers
     */

    if (bytes > 0) {
        if (bytes > req->filesize)
            bytes = req->filesize;

        if (sigsetjmp(env, 1) == 0) {
            handle_sigbus = 1;
            memcpy(req->buffer + req->buffer_end, req->data_mem, bytes);
            handle_sigbus = 0;
            /* OK, SIGBUS **after** this point is very bad! */
        } else {
            /* sigbus! */
            log_error_doc(req);
            reset_output_buffer(req);
            send_r_error(req);
            fprintf(stderr, "%sGot SIGBUS in memcpy!\n", get_commonlog_time());
            return 0;
        }
        req->buffer_end += bytes;
        req->filepos += bytes;
        if (req->filesize == req->filepos) {
            req_flush(req);
            req->status = DONE;
        }
    }

    /* We lose statbuf here, so make sure response has been sent */
    return 1;
}

#ifdef DAVINCI_IPCAM
int audio_get_dlink(request *req)
{
	//if (!sys_info->config.lan_config.audioinenable ) {
	if (!sys_info->ipcam[AUDIOINENABLE].config.u8) {
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
		return 0;
	}

	if (get_audio_data(&req->audio, 0) != RET_STREAM_SUCCESS) {
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
		return -1;
	}

	req->data_mem = req->audio.av_data.ptr;
	req->filesize = req->audio.av_data.size;
	req->filepos = 0;
	reset_output_buffer(req);
	//req_write(req, "\r\n");
	print_dlink_audio_headers(req);
	return 1;
}
#if 1
int audio_get(request * req, int video_type)
{
	//if ((audio_turns++ & 1) || !sys_info->config.lan_config.audioinenable || !req->req_flag.audio_on) {
	if ((req->audio_turns++ & 1) || !sys_info->ipcam[AUDIOINENABLE].config.u8 || !req->req_flag.audio_on) {
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
		return 0;
	}

	if (get_audio_data(&req->audio, 0) != RET_STREAM_SUCCESS) {
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
			return -1;
	}
	
	req->data_mem = req->audio.av_data.ptr;
	req->filesize = req->audio.av_data.size;
	req->filepos = 0;
	reset_output_buffer(req);
	req_write(req, "\r\n");
	print_audio_headers(req);

	return 1;
}
#else
int audio_get(request * req)
{
	AV_DATA av_data;
	int ret;

	if (req->audio_book == 0) {
		GetAVData(AV_OP_GET_ULAW_SERIAL, -1, &av_data );
		if (av_data.serial <= req->audio_lock) {
			dbg("av_data.serial <= req->audio_lock!!!\n");
			req->busy_flag &= ~BUSY_FLAG_AUDIO;
			return 0;
		}
		req->audio_book = av_data.serial;
	}
	
	ret = GetAVData(AV_OP_LOCK_ULAW, req->audio_book, &av_data );
	if (ret == RET_SUCCESS) {
		if (req->audio_lock > 0)
			GetAVData(AV_OP_UNLOCK_ULAW, req->audio_lock, NULL);

		if (req->audio_length + av_data.size > SOCKETBUF_SIZE)
			req->audio_length = 0;

		memcpy(req->audio_data+req->audio_length, av_data.ptr, av_data.size);
		req->audio_length += av_data.size;
		req->audio_lock = av_data.serial;
		req->audio_book = av_data.serial + 1;
//dbg("av_data.serial = %d, av_data.size = %d\n", av_data.serial, av_data.size);
		return 1;
	}
	else if (ret == RET_NO_VALID_DATA) {
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
		//usleep(1000);
		return 0;
	}
	else {
		req->audio_book = 0;
		dbg("ERROR, ret=%d\n", ret);
		return -1;
	}
	
}

void audio_send(request * req)
{
	req->data_mem = req->audio_data;
	req->filesize = req->audio_length;
	req->filepos = 0;
	req->audio_length = 0;
	reset_output_buffer(req);
	req_write(req, "\r\n");
	print_audio_headers(req);
}
#endif
#endif

static struct JPEG_HEADER jpeg_header = {
	{0xff, 0xd8},
	{0xff, 0xe0},
	0x3e00,	// length
	0,		// csum
	0,		// size
	2004,	// year
	1,		// day
	2,		// month
	30,		// minute
	12,		// hour
	0,		// minisecond
	59,		// second
	1,		// serial
	0,		// ID
	MACHINE_CODE,	// type
	0,		// quality
	16,		// rate
	0,		// resolution
	0,		// scale_factor
	4,		// flags
	0,		// alarm
	352,		// width
	224		// height
};	

__u16 check_sum(__u16 *data, int size)
{
	int i, cnt;
    __u32 sum = 0;

   	while(size) {
       	if(size > 65533) {
			cnt = 65534;
			size -= 65534;
		}
		else {
			cnt = size;
			size = 0;
		}
		for(i=0; i<cnt; i++) {
			sum += *data++;
		}
		sum = (sum >> 16) + (sum & 0xffff);
		sum += (sum >> 16);
		sum &= 0xffff;
   	}
	return ((~sum) & 0xffff);
}

void add_jpeg_header(request * req)
{
	jpeg_header.year = sys_info->tm_now.tm_year;
	jpeg_header.day = sys_info->tm_now.tm_mday;
	jpeg_header.month = sys_info->tm_now.tm_mon;
	jpeg_header.minute = sys_info->tm_now.tm_min;
	jpeg_header.hour = sys_info->tm_now.tm_hour;
	jpeg_header.second = sys_info->tm_now.tm_sec;
	jpeg_header.size = req->filesize;
	jpeg_header.width = req->video.av_data.width;
	jpeg_header.height = req->video.av_data.height;
	jpeg_header.serial = req->video.serial_lock;
	jpeg_header.csum = check_sum((__u16 *)&jpeg_header, sizeof(struct JPEG_HEADER));
	memcpy(req->buffer+req->buffer_end, &jpeg_header, sizeof(struct JPEG_HEADER));
	req->buffer_end += sizeof(struct JPEG_HEADER);
}


/*
 * Name: process_get
 * Description: Writes a chunk of data to the socket.
 *
 * Return values:
 *  -1: request blocked, move to blocked queue
 *   0: EOF or error, close it down
 *   1: successful write, recycle in ready queue
 */

int process_get(request * req)
{
//	int profileID;
    int bytes_written;
    volatile int bytes_to_write;
    bytes_to_write = req->filesize - req->filepos;
    if (bytes_to_write > SOCKETBUF_SIZE)
        bytes_to_write = SOCKETBUF_SIZE;


    if (sigsetjmp(env, 1) == 0) {
        handle_sigbus = 1;
#ifdef SERVER_SSL
	if(req->ssl == NULL){
#endif /*SERVER_SSL*/
        bytes_written = write(req->fd, req->data_mem + req->filepos,
                              bytes_to_write);
#ifdef SERVER_SSL
	}else{
		bytes_written = SSL_write(req->ssl, req->data_mem + req->filepos, bytes_to_write);
#if 0
		printf("SSL_write\n");
#endif /*0*/
	}
#endif /*SERVER_SSL*/
        handle_sigbus = 0;
        /* OK, SIGBUS **after** this point is very bad! */
    } else {
        /* sigbus! */
        log_error_doc(req);
        /* sending an error here is inappropriate
         * if we are here, the file is mmapped, and thus,
         * a content-length has been sent. If we send fewer bytes
         * the client knows there has been a problem.
         * We run the risk of accidentally sending the right number
         * of bytes (or a few too many) and the client
         * won't be the wiser.
         */
        req->status = DEAD;
        fprintf(stderr, "%sGot SIGBUS in write(2)!\n", get_commonlog_time());
        return 0;
    }

    if (bytes_written < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -1;
        }
        /* request blocked at the pipe level, but keep going */
        else {
            if (errno != EPIPE) {
                log_error_doc(req);
                /* Can generate lots of log entries, */
                perror("write");
                /* OK to disable if your logs get too big */
            }
            req->status = DEAD;
            return 0;
        }
    }
    req->filepos += bytes_written;

    if (req->filepos == req->filesize) { /* EOF */

#ifdef DAVINCI_IPCAM

#ifdef USE_NEW_STREAM_API
        if ((req->http_stream & FMT_MASK) == URI_STREAM_MJPEG) {
            int profileID = req->http_stream & SUBSTREAM_MASK;
            if (audio_get(req, 0) > 0)
                return 1;
            if (get_mjpeg_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size+1; // FixMe: add one byte for ActiveX display
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                req->extra_size = sizeof(struct JPEG_HEADER);
                req_write(req, "\r\n");
                print_mjpeg_headers(req);
                add_jpeg_header(req);
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
            return 1;
        }
        if ((req->http_stream & FMT_MASK) == URI_STREAM_MPEG4) {
            int profileID = req->http_stream & SUBSTREAM_MASK;
            if (audio_get(req, 0) > 0)
                return 1;
            if (get_mpeg4_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size;
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                req_write(req, "\r\n");
                print_mpeg4_headers(req);
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
            return 1;
        }
        if ((req->http_stream & FMT_MASK) == URI_STREAM_H264) {
            int profileID = req->http_stream & SUBSTREAM_MASK;
            if (audio_get(req, 0) > 0)
                return 1;
            if (get_h264_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size;
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                req_write( req, "\r\n" );
                print_H264_headers( req );
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
            return 1;
        }
        if ((req->http_stream & FMT_MASK) == URI_HTTP_MJPEG) {
            int profileID = req->http_stream & SUBSTREAM_MASK;
            if (audio_get(req, 0) > 0)
                return 1;
            if (get_mjpeg_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size;
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                req_write(req, "\r\n");
                print_mjpeg_headers(req);
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
            return 1;
        }
        if ((req->http_stream & FMT_MASK) == URI_DLINK_STREAM_MPEG4) {
			 int profileID = req->http_stream & SUBSTREAM_MASK;
            //if (audio_get(req, 0) > 0)
            //    return 1;
            req->busy_flag &= ~BUSY_FLAG_AUDIO;
            if (get_mpeg4_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size;
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                //req_write(req, "\r\n");
                print_dlink_mpeg4_headers(req);
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
            return 1;
        }
		if ((req->http_stream & FMT_MASK) == URI_DLINK_STREAM_H264) {
			 int profileID = req->http_stream & SUBSTREAM_MASK;
            //if (audio_get(req, 0) > 0)
                //return 1;
            req->busy_flag &= ~BUSY_FLAG_AUDIO;
            if (get_h264_data(&req->video, profileID,0) == RET_STREAM_SUCCESS) {
				//req->busy_flag &= ~BUSY_FLAG_VIDEO;
                req->data_mem = req->video.av_data.ptr;
                req->filesize = req->video.av_data.size;
                req->filepos = 0;
            	
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                //req_write(req, "\r\n");
                print_dlink_mpeg4_headers(req);
            }
			else
				req->busy_flag &= ~BUSY_FLAG_VIDEO;
			
            return 1;
        }
		if ((req->http_stream & FMT_MASK) == URI_STREAM_AVC) {
            int ret;
			if (audio_get(req, 0) > 0)
				return 1;
			ret = GetAVData( AV_OP_LOCK_AVC, req->video.serial_book, &req->video.av_data );
			if ( ret == RET_SUCCESS )
			{
				GetAVData( AV_OP_UNLOCK_AVC, req->video.serial_lock, NULL );
				req->data_mem = req->video.av_data.ptr;
				req->filesize = req->video.av_data.size;
				req->filepos = 0;
				req->video.serial_lock = req->video.av_data.serial;
				if ( avc_check_lastframeinGOP( req->video.av_data.serial ) )
				{
					unsigned tempiframe;

					tempiframe = avc_newest_iframeserial();
					if ( tempiframe > ( req->video.av_data.serial + 1 ) )
						req->video.serial_book = tempiframe;
					else
						req->video.serial_book = req->video.av_data.serial + 1;
				}
				else
					req->video.serial_book = req->video.av_data.serial + 1;
				reset_output_buffer( req );
				req_write( req, "\r\n" );
				print_H264_headers( req );
//				fprintf( stderr, "lock serial %d\n", req->serial_lock );
				return 1;
			}
			else if ( ret == RET_NO_VALID_DATA ) {
				req->busy_flag &= ~ BUSY_FLAG_VIDEO;
//				fprintf( stderr, "no data " );
				return 1;
			}
			else {
//				fprintf( stderr, "relock cause %d\n", ret );
				GetAVData( AV_OP_LOCK_AVC_IFRAME, -1, &req->video.av_data );
				req->video.serial_book = req->video.av_data.serial;
				dbg( "lock error ret=%d\n", ret );
				return 1;
			}
		}
		if (req->http_stream == URI_AUDIO_G726) {
			req->busy_flag &= ~BUSY_FLAG_VIDEO;

			if (!sys_info->ipcam[AUDIOINENABLE].config.u8) {
				req->busy_flag &= ~BUSY_FLAG_AUDIO;
				return 1;
			}

			if (get_audio_data(&req->audio, 0) != RET_STREAM_SUCCESS) {
				req->busy_flag &= ~BUSY_FLAG_AUDIO;
				return 1;
			}

			req->data_mem = req->audio.av_data.ptr;
			req->filesize = req->audio.av_data.size;
			req->filepos = 0;
			reset_output_buffer(req);
			//req_write(req, "\r\n");
			print_dlink_audio_headers(req);
			return 1;

        }
#else  // USE_NEW_STREAM_API
         if (req->http_stream == URI_DLINK_STREAM_MPEG4) {
            int ret;
           // if (audio_get(req, FMT_MPEG4) > 0)
			//	return 1;
            ret = GetAVData(AV_OP_LOCK_MP4, req->serial_book, &req->av_data);
            
            if (ret == RET_SUCCESS ) {
              
                GetAVData(AV_OP_UNLOCK_MP4, req->serial_lock, NULL);
                req->data_mem = req->av_data.ptr;
                req->filesize = req->av_data.size;
                req->filepos = 0;
                
                req->serial_lock = req->av_data.serial;
                req->serial_book = req->av_data.serial+1;

                // for synchronization
                if (req->av_data.flags & AV_FLAGS_MP4_LAST_FRAME) {
                    AV_DATA last;
                    GetAVData(AV_OP_GET_MPEG4_SERIAL, -1, &last);
                    if (last.serial != req->serial_book)
                        req->serial_book = last.serial;
                }
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                //req_write(req, "\r\n");
                
                print_dlink_mpeg4_headers(req);
        
                return 1;
            }
            else if (ret == RET_NO_VALID_DATA) {
                req->busy_flag &= ~BUSY_FLAG_VIDEO;
				//usleep(10000);
                return 1;
            }
            else {
                GetAVData(AV_OP_GET_MPEG4_SERIAL, -1, &req->av_data );
                req->serial_book = req->av_data.serial;
                dbg("lock error ret=%d\n", ret);
                return 1;
            }        
        }

		if ( (req->http_stream & FMT_MASK) == URI_STREAM_H264 )
		{
			int ret;
			profileID = req->http_stream & SUBSTREAM_MASK;

			if ( audio_get( req, FMT_MPEG4 ) > 0 )
				return 1;
#if defined( PLAT_DM365 )
			ret = GetAVData(AV_OP_LOCK_H264 | profileID, req->serial_book, &req->av_data);
            if (ret == RET_SUCCESS) {
				if( req->serial_lock >= 0 ){
					if( (ret = GetAVData(AV_OP_UNLOCK_H264 | profileID, req->serial_lock, NULL)) != RET_SUCCESS ){
						dbg("lock error ret=%d\n", ret);
						return 1;
					}
				}
                req->data_mem = req->av_data.ptr;
                req->filesize = req->av_data.size;
                req->filepos = 0;
                req->serial_lock = req->av_data.serial;
                req->serial_book = req->av_data.serial+1;

                // for synchronization
                if (req->av_data.flags & AV_FLAGS_MP4_LAST_FRAME) {
                    AV_DATA last;
                    GetAVData(AV_OP_GET_H264_SERIAL | profileID , -1, &last);
                    if (last.serial != req->serial_book)
                        req->serial_book = last.serial;
                }
                reset_output_buffer(req);
#ifdef CONFIG_STREAM_TIMEOUT
                req->last_stream_time = sys_info->second;
#endif
                req_write(req, "\r\n");
                print_H264_headers( req );
                return 1;
            }
            else if (ret == RET_NO_VALID_DATA) {
                req->busy_flag &= ~BUSY_FLAG_VIDEO;
				//usleep(10000);
                return 1;
            }else if (ret == RET_OVERWRTE) {
				req->serial_lock = -1;
                req->serial_book++;
                return 1;
            }
            else {
                GetAVData(AV_OP_GET_H264_SERIAL | profileID, -1, &req->av_data );
                req->serial_book = req->av_data.serial;
                dbg("lock error ret=%d\n", ret);
                return 1;
            }
#elif defined( PLAT_DM355 )
			ret = GetAVData( AV_OP_LOCK_AVC, req->serial_book, &req->av_data );
			if ( ret == RET_SUCCESS )
			{
				 GetAVData( AV_OP_UNLOCK_AVC, req->serial_lock, NULL );
				 req->data_mem = req->av_data.ptr;
				 req->filesize = req->av_data.size;
				 req->filepos = 0;
				 req->serial_lock = req->av_data.serial;
				if ( avc_check_lastframeinGOP( req->av_data.serial ) )
				{
					unsigned tempiframe;

					tempiframe = avc_newest_iframeserial();
					if ( tempiframe > ( req->av_data.serial + 1 ) )
						req->serial_book = tempiframe;
					else
						req->serial_book = req->av_data.serial + 1;
				}
				else
				 req->serial_book = req->av_data.serial + 1;
				 reset_output_buffer( req );
				 req_write( req, "\r\n" );
				 print_H264_headers( req );
//				 fprintf( stderr, "lock serial %d\n", req->serial_lock );
				 return 1;
			 }
			 else
			 	if ( ret == RET_NO_VALID_DATA )
			 	{
					req->busy_flag &= ~ BUSY_FLAG_VIDEO;
//					fprintf( stderr, "no data " );
					return 1;
				}
				else
				{
//					fprintf( stderr, "relock cause %d\n", ret );
					GetAVData( AV_OP_LOCK_AVC_IFRAME, -1, &req->av_data );
					req->serial_book = req->av_data.serial;
					dbg( "lock error ret=%d\n", ret );
					return 1;
				}
#else
#error Unknown platform
#endif
		}
        if (req->http_stream == URI_AUDIO_G726) {
            int ret;
         
#if 1
#if 0
		while (audio_get(req) > 0);
		if (req->audio_length > AUDIO_SEND_SIZE) {
			audio_send(req);
      		      return 1;
		}
#else

     if (audio_get_dlink(req, FMT_AUDIO) > 0)	//exe_
			return 1;
#endif
#else
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
#endif
			
            ret = GetAVData(AV_OP_LOCK_ULAW, req->serial_book, &req->av_data);
            if (ret == RET_SUCCESS ) {
              
                GetAVData(AV_OP_UNLOCK_ULAW, req->serial_lock, NULL);
                req->data_mem = req->av_data.ptr;
                req->filesize = req->av_data.size;
                req->filepos = 0;
                
                req->serial_lock = req->av_data.serial;
                req->serial_book = req->av_data.serial+1;

                reset_output_buffer(req);
                //req_write(req, "\r\n");
                print_dlink_audio_headers(req);
        				
                return 1;
            }
            else if (ret == RET_NO_VALID_DATA) {
                req->busy_flag &= ~BUSY_FLAG_AUDIO;
                return 1;
            }
          	else {
				req->audio_book = 0;
				dbg("ERROR, ret=%d\n", ret);
				return -1;
			}
						
        }
		if (req->http_stream == URI_AUDIO_STREAM) {
            int ret;
         
#if 1
#if 0
		while (audio_get(req) > 0);
		if (req->audio_length > AUDIO_SEND_SIZE) {
			audio_send(req);
      		      return 1;
		}
#else
     if (audio_get(req, FMT_AUDIO) > 0)	//exe_
			return 1;
#endif
#else
		req->busy_flag &= ~BUSY_FLAG_AUDIO;
#endif

            ret = GetAVData(AV_OP_LOCK_ULAW, req->serial_book, &req->av_data);
            
            if (ret == RET_SUCCESS ) {
              
                GetAVData(AV_OP_UNLOCK_ULAW, req->serial_lock, NULL);
                req->data_mem = req->av_data.ptr;
                req->filesize = req->av_data.size;
                req->filepos = 0;
                
                req->serial_lock = req->av_data.serial;
                req->serial_book = req->av_data.serial+1;

                reset_output_buffer(req);
                req_write(req, "\r\n");
                
                print_audio_headers(req);
        				
                return 1;
            }
            else if (ret == RET_NO_VALID_DATA) {
                req->busy_flag &= ~BUSY_FLAG_AUDIO;
                return 1;
            }
          	else {
				req->audio_book = 0;
				dbg("ERROR, ret=%d\n", ret);
				return -1;
			}
						
        }
#endif  // USE_NEW_STREAM_API
        if (req->http_stream == URI_NOTIFY_STREAM) {

    		if((req->nipca_notify_time != sys_info->second) && (req->nipca_notify_stream == 1)){
				char newbuf[MAX_NIPCA_NOTIFY_LEN];
				size_t size = 0;

				size += sprintf(newbuf+size,"md1=%s\r\n", sys_info->status.motion? "on":"off");
				//size += sprintf(newbuf+size,"mdv1=%d\r\n", sys_info->status.motion_cpercentage);
#if GIOIN_NUM >= 1
				size += sprintf(newbuf+size,"input1=%s\r\n", sys_info->status.alarm1? "on":"off");
#endif
#if GIOIN_NUM >= 2
				size += sprintf(newbuf+size,"input2=%s\r\n", sys_info->status.alarm2? "on":"off");
#endif
#if GIOIN_NUM >= 3
				size += sprintf(newbuf+size,"input3=%s\r\n", sys_info->status.alarm3? "on":"off");
#endif
#if GIOIN_NUM >= 4
				size += sprintf(newbuf+size,"input4=%s\r\n", sys_info->status.alarm4? "on":"off");
#endif
#if GIOIN_NUM >= 5
				size += sprintf(newbuf+size,"input5=%s\r\n", sys_info->status.alarm5? "on":"off");
#endif
#if GIOIN_NUM >= 6
				size += sprintf(newbuf+size,"input6=%s\r\n", sys_info->status.alarm6? "on":"off");
#endif
#if GIOIN_NUM >= 7
				size += sprintf(newbuf+size,"input7=%s\r\n", sys_info->status.alarm7? "on":"off");
#endif
#if GIOIN_NUM >= 8
				size += sprintf(newbuf+size,"input8=%s\r\n", sys_info->status.alarm8? "on":"off");
#endif

				//size += sprintf(newbuf+size,"storagefull=%s\r\n", str[0]);
				//size += sprintf(newbuf+size,"storagefail=%s\r\n", str[0]);
				size += sprintf(newbuf+size,"recording=%s\r\n", sys_info->status.on_record? "on":"off");
				//size += sprintf(newbuf+size,"snapshooting=%s\r\n", str[0]);
				size += sprintf(newbuf+size,"output1=%s\r\n", sys_info->status.alarm_out1? "on":"off");
				size += sprintf(newbuf+size,"vsignal=on\r\n");
				//size += sprintf(newbuf+size,"speaker=%s\r\n", str[0]);
				//size += sprintf(newbuf+size,"mic=%s\r\n", str[0]);
				size += sprintf(newbuf+size,"cameraname=%s\r\n", conf_string_read(TITLE));
    			
    			//fprintf(stderr, "sec = %ld\n", req->nipca_notify_time);
				//fprintf(stderr, "sec = %ld\n", sys_info->second);
    			reset_output_buffer(req);
    			if(strcmp(req->nipca_notify_buf, newbuf) != 0){
					//fprintf(stderr, "status change\n");
    				strcpy(req->nipca_notify_buf, newbuf);
    				req->nipca_notify_time = sys_info->second;
    				req_write(req, newbuf);
    			}

    			if((sys_info->second - req->nipca_notify_time) >= 30){
    				req->nipca_notify_time = sys_info->second;
    				req_write(req, "keep_alive\r\n");	
    				//fprintf(stderr, "30s\n");
    			}
    		}

    		req->busy_flag = 0;
            return 1;
    	}
#endif  // DAVINCI_IPCAM
        return 0;
    } else
        return 1;               /* more to do */
}

/*
 * Name: get_dir
 * Description: Called from process_get if the request is a directory.
 * statbuf must describe directory on input, since we may need its
 *   device, inode, and mtime.
 * statbuf is updated, since we may need to check mtimes of a cache.
 * returns:
 *  -1 error
 *  0  cgi (either gunzip or auto-generated)
 *  >0  file descriptor of file
 */

int get_dir(request * req, struct stat *statbuf)
{

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;

    if (directory_index) {      /* look for index.html first?? */
        strcpy(pathname_with_index, req->pathname);
        strcat(pathname_with_index, directory_index);
        /*
           sprintf(pathname_with_index, "%s%s", req->pathname, directory_index);
         */

        data_fd = open(pathname_with_index, O_RDONLY);

        if (data_fd != -1) {    /* user's index file */
            strcpy(req->request_uri, directory_index); /* for mimetype */
            fstat(data_fd, statbuf);
            return data_fd;
        }
        if (errno == EACCES) {
            send_r_forbidden(req);
            return -1;
        } else if (errno != ENOENT) {
            /* if there is an error *other* than EACCES or ENOENT */
            send_r_not_found(req);
            return -1;
        }

#ifdef GUNZIP
        /* if we are here, trying index.html didn't work
         * try index.html.gz
         */
        strcat(pathname_with_index, ".gz");
        data_fd = open(pathname_with_index, O_RDONLY);
        if (data_fd != -1) {    /* user's index file */
            close(data_fd);

            req->response_status = R_REQUEST_OK;
            SQUASH_KA(req);
            if (req->pathname)
                free(req->pathname);
            req->pathname = strdup(pathname_with_index);
            if (!req->pathname) {
                log_error_time();
                perror("strdup");
                send_r_error(req);
                return 0;
            }
            if (!req->simple) {
                req_write(req, "HTTP/1.0 200 OK-GUNZIP\r\n");
                print_http_headers(req);
                print_last_modified(req);
                req_write(req, "Content-Type: ");
                req_write(req, get_mime_type(directory_index));
                req_write(req, "\r\n\r\n");
                req_flush(req);
            }
            if (req->method == M_HEAD)
                return 0;
            return init_cgi(req);
        }
#endif
    }

    /* only here if index.html, index.html.gz don't exist */
    if (dirmaker != NULL) {     /* don't look for index.html... maybe automake? */
        req->response_status = R_REQUEST_OK;
        SQUASH_KA(req);

        /* the indexer should take care of all headers */
        if (!req->simple) {
            req_write(req, "HTTP/1.0 200 OK\r\n");
            print_http_headers(req);
            print_last_modified(req);
            req_write(req, "Content-Type: text/html\r\n\r\n");
            req_flush(req);
        }
        if (req->method == M_HEAD)
            return 0;

        return init_cgi(req);
        /* in this case, 0 means success */
    } else if (cachedir) {
        return get_cachedir_file(req, statbuf);
    } else {                    /* neither index.html nor autogenerate are allowed */
        send_r_forbidden(req);
        return -1;              /* nothing worked */
    }
}

int get_cachedir_file(request * req, struct stat *statbuf)
{

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;
    time_t real_dir_mtime;

    real_dir_mtime = statbuf->st_mtime;
    sprintf(pathname_with_index, "%s/dir.%d.%ld",
            cachedir, (int) statbuf->st_dev, statbuf->st_ino);
    data_fd = open(pathname_with_index, O_RDONLY);

    if (data_fd != -1) {        /* index cache */

        fstat(data_fd, statbuf);
        if (statbuf->st_mtime > real_dir_mtime) {
            statbuf->st_mtime = real_dir_mtime; /* lie */
            strcpy(req->request_uri, directory_index); /* for mimetype */
            return data_fd;
        }
        close(data_fd);
        unlink(pathname_with_index); /* cache is stale, delete it */
    }
    if (index_directory(req, pathname_with_index) == -1)
        return -1;

    data_fd = open(pathname_with_index, O_RDONLY); /* Last chance */
    if (data_fd != -1) {
        strcpy(req->request_uri, directory_index); /* for mimetype */
        fstat(data_fd, statbuf);
        statbuf->st_mtime = real_dir_mtime; /* lie */
        return data_fd;
    }

    boa_perror(req, "re-opening dircache");
    return -1;                  /* Nothing worked. */

}

/*
 * Name: index_directory
 * Description: Called from get_cachedir_file if a directory html
 * has to be generated on the fly
 * returns -1 for problem, else 0
 * This version is the fastest, ugliest, and most accurate yet.
 * It solves the "stale size or type" problem by not ever giving
 * the size or type.  This also speeds it up since no per-file
 * stat() is required.
 */

int index_directory(request * req, char *dest_filename)
{
    DIR *request_dir;
    FILE *fdstream;
    struct dirent *dirbuf;
    int bytes = 0;
    char *escname = NULL;

    if (chdir(req->pathname) == -1) {
        if (errno == EACCES || errno == EPERM) {
            send_r_forbidden(req);
        } else {
            log_error_doc(req);
            perror("chdir");
            send_r_bad_request(req);
        }
        return -1;
    }

    request_dir = opendir(".");
    if (request_dir == NULL) {
        int errno_save = errno;
        send_r_error(req);
        log_error_time();
        fprintf(stderr, "directory \"%s\": ", req->pathname);
        errno = errno_save;
        perror("opendir");
        return -1;
    }

    fdstream = fopen(dest_filename, "w");
    if (fdstream == NULL) {
        boa_perror(req, "dircache fopen");
        closedir(request_dir);
        return -1;
    }

    bytes += fprintf(fdstream,
                     "<HTML><HEAD>\n<TITLE>Index of %s</TITLE>\n</HEAD>\n\n",
                     req->request_uri);
    bytes += fprintf(fdstream, "<BODY>\n\n<H2>Index of %s</H2>\n\n<PRE>\n",
                     req->request_uri);

    while ((dirbuf = readdir(request_dir))) {
        if (!strcmp(dirbuf->d_name, "."))
            continue;

        if (!strcmp(dirbuf->d_name, "..")) {
            bytes += fprintf(fdstream,
                             " [DIR] <A HREF=\"../\">Parent Directory</A>\n");
            continue;
        }

        if ((escname = escape_string(dirbuf->d_name, NULL)) != NULL) {
            bytes += fprintf(fdstream, " <A HREF=\"%s\">%s</A>\n",
                             escname, dirbuf->d_name);
            free(escname);
            escname = NULL;
        }
    }
    closedir(request_dir);
    bytes += fprintf(fdstream, "</PRE>\n\n</BODY>\n</HTML>\n");

    fclose(fdstream);

    chdir(server_root);

    req->filesize = bytes;      /* for logging transfer size */
    return 0;                   /* success */
}
