#define  LOCAL_DEBUG
#include <debug.h>
#include "boa.h"
#include <sys/resource.h>
#ifdef DAVINCI_IPCAM
#include "ApproDrvMsg.h"
#include "Appro_interface.h"
#include "cmem.h"
#include "Msg_Def.h"
#include <file_msg_drv.h>
#include <share_mem.h>
#include <sys_msg_drv.h>
#include <sysctrl.h>
#include <sysinfo.h>
#include <utility.h>
#endif
#include <pthread.h>
#include "rtsp_o_http.h"

static struct thread_data_t
{
	int hup;
	int used_count;
	RTSP_DATA *used_list;
	RTSP_DATA *free_list;
	request *return_list;
	request *insert_req;
	pthread_mutex_t insert_mutex;
	pthread_cond_t insert_cond;
}
thread_data;

enum
{
	NO_SPACE = 0,
	NEW_ID,
	EXIST_ID
};

static void *rtsp_o_http_thread( void *env );
static int search_adpative_record( struct thread_data_t *thread_data, char *sessionid, RTSP_DATA **ptr );
static void update_record( struct thread_data_t *thread_data );

int start_rtsp_o_http_thread( void )
{
	pthread_t rtsp_o_http_threadid;

	pthread_mutex_init( &thread_data.insert_mutex, NULL );
	thread_data.insert_req = NULL;
	thread_data.hup = 0;
	thread_data.used_list = NULL;
	thread_data.free_list = NULL;
	thread_data.return_list = NULL;
	return pthread_create( &rtsp_o_http_threadid, NULL, rtsp_o_http_thread, &thread_data );
}

void rtsp_o_http_notify_restart( void )
{
	thread_data.hup = 1;
}

request *rtsp_o_http_get_return_req( void )
{
	request *ret;

	if ( ret = thread_data.return_list )
	{
		dequeue( &thread_data.return_list, ret );
		ret->status = DONE;	/* done, will close */
		ret->buffer_end = 0;	/* avoid to send anything */
	}
	return ret;
}

request *rtsp_o_http_insert_req( request *req )
{
	struct timespec timeoutcount;
	request *ret;

	ret = req->next;
	pthread_mutex_unlock( &thread_data.insert_mutex );
	pthread_cond_init( &thread_data.insert_cond, NULL );
	if ( pthread_mutex_trylock( &thread_data.insert_mutex ) )
	{
		sleep( 1 );
		if ( pthread_mutex_trylock( &thread_data.insert_mutex ) )
		{
			fprintf( stderr, "lock insert mutex fail\n" );
			return NULL;
		}
	}
	thread_data.insert_req = req;
	timeoutcount.tv_sec = time( NULL ) + 3;
	timeoutcount.tv_nsec = 0;
	if ( ! pthread_cond_timedwait( &thread_data.insert_cond, &thread_data.insert_mutex, &timeoutcount ) )
	{
		pthread_mutex_unlock( &thread_data.insert_mutex );
		dequeue( &request_ready, req );
		return ret;
	}
	return NULL;
}

/* link to cpp */
int init_live555( void );
void do_live555( void );
void addClientConnection( RTSP_DATA *clientdata );
void incoming_hander( void *session );
int exit_live555( void );

static void *rtsp_o_http_thread( void *env )
{
	struct thread_data_t *thread_data = env;
	RTSP_DATA *workdata;
	request *req;
	fd_set readset, w_readset;
	int max_fd_num;
	struct timeval tmv;
	int ret;

	init_live555();
	FD_ZERO( &readset );
	max_fd_num = -1;
	for ( ; thread_data->hup == 0; )
	{
		if ( thread_data->insert_req )
		{
			pthread_mutex_lock( &thread_data->insert_mutex );
			req = thread_data->insert_req;
			if ( ( ( ret = search_adpative_record( thread_data, req->sessionid, &workdata ) ) != NO_SPACE ) &&
			     ( workdata->status != STATUS_WAIT_CLOSE ) )
			{
				fprintf( stderr, "insert %s, session id is \"%s\"\n",
				                 req->method == M_GET ? "out socket" : "in socket",
				                 req->sessionid );
				if ( req->method == M_GET )
				{
					if ( ! workdata->outreq )
					{
						workdata->outreq = req;
						workdata->flags |= FLAGS_OUT_OK;
						if ( ret == NEW_ID )
							addClientConnection( workdata );
						if ( req->fd > max_fd_num )
							max_fd_num = req->fd;
						FD_SET( req->fd, &readset );
					}
				}
				else
				{
					if ( ! workdata->inreq )
					{
						workdata->inreq = req;
						workdata->flags |= FLAGS_IN_OK;
						if ( ret == NEW_ID )
							addClientConnection( workdata );
						if ( req->fd > max_fd_num )
							max_fd_num = req->fd;
						FD_SET( req->fd, &readset );
					}
				}
				if ( ( workdata->flags & FLAGS_CONNECT_OK ) == FLAGS_CONNECT_OK )
					workdata->status = STATUS_ACTIVE;
				else
					workdata->status = STATUS_WAIT_SOCKET;
			}
			thread_data->insert_req = NULL;
			pthread_cond_signal( &thread_data->insert_cond );
			pthread_mutex_unlock( &thread_data->insert_mutex );
		}
		tmv.tv_sec = 0;
		tmv.tv_usec = 0;
		if ( thread_data->used_list )
		{
			w_readset = readset;
			switch ( ret = select( max_fd_num + 1, &w_readset, NULL, NULL, &tmv ) )
			{
				case 0:		/* success, but not action */
					break;
				case -1:	/* error */
					perror( "select:" );
					break;
				default:	/* action */
					for ( workdata = thread_data->used_list; workdata; workdata = workdata->next )
					{
						int action;

						action = 0;
						if ( ( req = workdata->inreq ) && FD_ISSET( req->fd, &w_readset ) )
						{
							ret = read( req->fd, req->client_stream + req->client_stream_pos, CLIENT_STREAM_SIZE - req->client_stream_pos );
							if ( ( ! ret ) ||
							     ( ( ret < 0 ) && ( errno != EWOULDBLOCK ) && ( errno != EAGAIN ) ) )
							{
								FD_CLR( req->fd, &readset );
								workdata->flags &= ~ FLAGS_IN_OK;
								workdata->inreq = NULL;
								fprintf( stderr, "delete in socket, session id is \"%s\"\n", req->sessionid );
								enqueue( &thread_data->return_list, req );
								action = 1;
							}
							else if ( ret > 0 )
								req->client_stream_pos += ret;
						}
						if ( ( req = workdata->outreq ) && FD_ISSET( req->fd, &w_readset ) )
						{
							ret = read( req->fd, req->client_stream, CLIENT_STREAM_SIZE );
							if ( ( ! ret ) ||
							     ( ( ret < 0 ) && ( errno != EWOULDBLOCK ) && ( errno != EAGAIN ) ) )
							{
								FD_CLR( req->fd, &readset );
								workdata->flags &= ~ FLAGS_OUT_OK;
								workdata->outreq = NULL;
								fprintf( stderr, "delete out socket, session id is \"%s\"\n", req->sessionid );
								enqueue( &thread_data->return_list, req );
								action = 1;
							}
						}
						if ( ( workdata->status != STATUS_WAIT_CLOSE ) && action )
						{
							if ( workdata->flags & FLAGS_CONNECT_OK )
								workdata->status = STATUS_WAIT_SOCKET;
							else
								workdata->status = STATUS_WAIT_CLOSE;
						}
					}
					break;
			}
			do_live555();
			update_record( thread_data );
		}
		else
		{
			max_fd_num = -1;
			FD_ZERO( &readset );
			usleep( 100000 );
		}
	}
	exit_live555();
	pthread_exit( NULL );
}

static int search_adpative_record( struct thread_data_t *thread_data, char *sessionid, RTSP_DATA **ptr )
{
	RTSP_DATA *work_ptr;

	for ( work_ptr = thread_data->used_list; work_ptr; work_ptr = work_ptr->next )
	{
		if ( strcmp( work_ptr->sessionid, sessionid ) == 0 )
		{
			*ptr = work_ptr;
			return EXIST_ID;
		}
	}
	if ( thread_data->free_list )
	{
		work_ptr = thread_data->free_list;
		if ( work_ptr->next )
			work_ptr->next->prev = NULL;
		thread_data->free_list = work_ptr->next;
	}
	else
	{
		if ( ! ( work_ptr = malloc( sizeof( RTSP_DATA ) ) ) )
			return NO_SPACE;
	}
	memset( work_ptr, 0, sizeof( RTSP_DATA ) );
	work_ptr->status = STATUS_CLOSED;
	work_ptr->prev = NULL;
	work_ptr->next = thread_data->used_list;
	if ( thread_data->used_list )
		thread_data->used_list->prev = work_ptr;
	*ptr = thread_data->used_list = work_ptr;
	strncpy( work_ptr->sessionid, sessionid, 27 );
	return NEW_ID;
}

static void update_record( struct thread_data_t *thread_data )
{
	RTSP_DATA *work_ptr, *tmp_ptr;

	for ( work_ptr = thread_data->used_list; work_ptr; )
	{
		tmp_ptr = work_ptr->next;
		switch ( work_ptr->status )
		{
			case STATUS_ACTIVE:
				if ( work_ptr->inreq->parse_pos < work_ptr->inreq->client_stream_pos )
					incoming_hander( work_ptr->rtspclientsession );
				break;
			case STATUS_WAIT_CLOSE:
				if ( ( ! ( work_ptr->flags & FLAGS_CONNECT_OK ) ) && 
				     ( work_ptr->flags & FLAGS_LIVE_OK ) )
					incoming_hander( work_ptr->rtspclientsession );
				if ( work_ptr->flags )
					break;
				if ( work_ptr->prev )
					work_ptr->prev->next = work_ptr->next;
				else
					thread_data->used_list = work_ptr->next;
				if ( work_ptr->next )
					work_ptr->next->prev = work_ptr->prev;
				memset( work_ptr, 0, sizeof( RTSP_DATA ) );
				work_ptr->status = STATUS_CLOSED;
				work_ptr->prev = NULL;
				work_ptr->next = thread_data->free_list;
				thread_data->free_list = work_ptr;
				break;
			default:
			case STATUS_CLOSED:
			case STATUS_WAIT_SOCKET:
				break;
		}
		work_ptr = tmp_ptr;
	}
}
