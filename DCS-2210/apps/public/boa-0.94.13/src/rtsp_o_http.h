#ifndef	_RTSP_O_HTTP_H_
#define	_RTSP_O_HTTP_H_

#include "boa.h"

enum
{
	STATUS_CLOSED = 0,
	STATUS_WAIT_SOCKET,
	STATUS_ACTIVE,
	STATUS_WAIT_CLOSE
};

#define	FLAGS_IN_OK		( 1u << 0 )
#define	FLAGS_OUT_OK		( 1u << 1 )
#define	FLAGS_LIVE555_SETUP_OK	( 1u << 16 )
#define	FLAGS_LIVE555_PLAY	( 1u << 17 )

#define	FLAGS_CONNECT_OK	( FLAGS_IN_OK | FLAGS_OUT_OK )
#define	FLAGS_LIVE_OK		( FLAGS_LIVE555_SETUP_OK | FLAGS_LIVE555_PLAY )

typedef
struct RTSP_DATA
{
	char sessionid[28];		/* session key */
	void *rtspclientsession;	/* rtspclient session */
	request *outreq;		/* S->C request, GET */
	request *inreq;			/* C->S request, POST */
	int status;			/* close or active */
	int flags;			/* flags */
	struct RTSP_DATA *prev;
	struct RTSP_DATA *next;
	
}
RTSP_DATA;

#endif	/* _RTSP_O_HTTP_H_ */
