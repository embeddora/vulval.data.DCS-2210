#include <signal.h>
#include <BasicUsageEnvironment.hh>
#include <getopt.h>
#include <liveMedia.hh>
#include "Err.hh"
#include "ApproJPEGVideoServerMediaSubsession.hh"
#include "ApproMPEG4VideoServerMediaSubsession.hh"
#include "ApproH264VideoServerMediaSubsession.hh"
#include "ApproG726AudioServerMediaSubsession.hh"
#include "ApproPCMAudioServerMediaSubsession.hh"

#include "RTSP_O_HTTPServer.hh"

#include <ipnc_define.h>
#include <ApproDrvMsg.h>
#include <Appro_interface.h>
#include <system_default.h>
#include <sysconf.h>
#include <sysinfo.h>
#include <parser_name.h>

//#define ACCESS_CONTROL

#define MAX_STREAM_DESC_LENG	128

#define AV_FLAG_VIDEO			(1<<0)
#define AV_FLAG_AUDIO			(1<<1)
#define AV_FLAG_MASK			(AV_FLAG_VIDEO|AV_FLAG_AUDIO)

enum {
	AUDIO_FORMAT_G711,
	AUDIO_FORMAT_G726,
//	AUDIO_FORMAT_AAC,
//	AUDIO_FORMAT_GSM,
};

typedef struct {
	ApproInput *input_device;
	int av_flag;
	int video_type;		// mjpeg, mpeg4, h264
	int audio_type;		// g711, g726, aac, gsm
	int profile_id;
	int bitrate;
	const char *access_name;
	char description[MAX_STREAM_DESC_LENG];
} RtspStream;

/* video & audio + video only + audio only */
#define MAX_RTSP_STREAM		(MAX_PROFILE_NUM + MAX_PROFILE_NUM + 1)

int MjpegVideoBitrate = 1500000;
int Mpeg4VideoBitrate = 1500000;
int H264VideoBitrate = 1500000;

unsigned audioSamplingFrequency = 8000;
unsigned audioNumChannels = 1;

extern SysInfo *sys_info;

void set_video_type(int type, RtspStream *stream)
{
	switch (type) {
		case SEND_FMT_JPEG:
			stream->video_type = VIDEO_TYPE_MJPEG;
			stream->bitrate = MjpegVideoBitrate;
			break;
		case SEND_FMT_MPEG4:
			stream->video_type = VIDEO_TYPE_MPEG4;
			stream->bitrate = Mpeg4VideoBitrate;
			break;
		case SEND_FMT_H264:
			stream->video_type = VIDEO_TYPE_H264;
			stream->bitrate = H264VideoBitrate;
			break;
		default:
			stream->av_flag &= ~AV_FLAG_VIDEO; // turn off video stream
	}
}

void set_audio_type(char *codec, RtspStream *stream)
{
	if ( ! strcmp( codec, AUDIO_CODEC_G711 ) )
		stream->audio_type = AUDIO_FORMAT_G711;
	else if ( ! strcmp( codec, AUDIO_CODEC_G726 ) )
		stream->audio_type = AUDIO_FORMAT_G726;
	else
		stream->av_flag &= ~AV_FLAG_AUDIO; // turn off audio stream
}

static RtspStream rtspstream[MAX_RTSP_STREAM];
static TaskScheduler *scheduler = NULL;
static UsageEnvironment *Env = NULL;
static UserAuthenticationDatabase *authDB = NULL;
static RTSP_O_HTTPServer *rtspServer = NULL;

extern "C"
int init_live555( void )
{
	int idx, count;
	// Begin by setting up our usage environment:
	scheduler = BasicTaskScheduler::createNew();
	Env = BasicUsageEnvironment::createNew( *scheduler );
#ifdef ACCESS_CONTROL
	// To implement client access control to the RTSP server, do the following:
	int i;
	authDB = new UserAuthenticationDatabase;
	USER_INFO *account = sys_info->config.acounts;
	for( i = 0; i < ACOUNT_NUM; i++ )
	{
		if ( account[i].name[0] )
			authDB->addUserRecord( account[i].name, account[i].passwd );
	}
#endif
	memset( rtspstream, 0, sizeof( rtspstream ) );
	count = 0;
#ifdef SUPPORT_PROFILE_NUMBER
	for ( idx = 0; idx < sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value; idx++ )
#else
	for ( idx = 0; idx < MAX_PROFILE_NUM; idx++ )
#endif
	{
		rtspstream[count].av_flag = AV_FLAG_VIDEO | AV_FLAG_AUDIO;
		rtspstream[count].profile_id = sys_info->profile_config.profile_codec_idx[idx] - 1;
		set_video_type( sys_info->profile_config.profile_codec_fmt[idx], rtspstream + count );
		set_audio_type( conf_string_read( AUDIO_INFO_CODEC_NAME ), rtspstream + count );
		rtspstream[count].access_name = conf_string_read( STREAM0_RTSPSTREAMNAME + idx * 2 );
		snprintf( rtspstream[count].description, MAX_STREAM_DESC_LENG, "RTSP/RTP stream %d from %s", idx + 1, conf_string_read( TITLE ) );
		count++;
	}
	// Create the RTSP server:
	// Normal case: Streaming from a built-in RTSP server:
	rtspServer = RTSP_O_HTTPServer::createNew( *Env, authDB );
	if ( rtspServer == NULL )
	{
		*Env << "Failed to create RTSP Over HTTP server: " << Env->getResultMsg() << "\n";
		exit( 1 );
	}

	for ( idx = 0; idx < MAX_RTSP_STREAM; idx++ )
	{
		if ( rtspstream[idx].av_flag & AV_FLAG_MASK )
		{
			rtspstream[idx].input_device = ApproInput::createNew( *Env, rtspstream[idx].video_type, rtspstream[idx].profile_id );
			if ( rtspstream[idx].input_device == NULL )
			{
				err( *Env ) << "Failed to create input device " << idx << "\n";
				exit( 1 );
			}
			ServerMediaSession *sms
				= ServerMediaSession::createNew( *Env, rtspstream[idx].access_name, rtspstream[idx].access_name, rtspstream[idx].description );
			if ( rtspstream[idx].av_flag & AV_FLAG_VIDEO )
			{
				if ( rtspstream[idx].video_type == VIDEO_TYPE_MJPEG )
					sms->addSubsession( ApproJPEGVideoServerMediaSubsession::createNew( sms->envir(), *rtspstream[idx].input_device, rtspstream[idx].bitrate ) );
				else if ( rtspstream[idx].video_type == VIDEO_TYPE_MPEG4 )
					sms->addSubsession( ApproMPEG4VideoServerMediaSubsession::createNew( sms->envir(), *rtspstream[idx].input_device, rtspstream[idx].bitrate ) );
				else if ( rtspstream[idx].video_type == VIDEO_TYPE_H264 )
					sms->addSubsession( ApproH264VideoServerMediaSubsession::createNew( sms->envir(), *rtspstream[idx].input_device, rtspstream[idx].bitrate ) );
			}
			if ( rtspstream[idx].av_flag & AV_FLAG_AUDIO )
			{
				if ( rtspstream[idx].audio_type == AUDIO_FORMAT_G711 )
					sms->addSubsession( ApproPCMAudioServerMediaSubsession::createNew( sms->envir(), *rtspstream[idx].input_device ) );
				else if ( rtspstream[idx].audio_type == AUDIO_FORMAT_G726 )
					sms->addSubsession( ApproG726AudioServerMediaSubsession::createNew( sms->envir(), *rtspstream[idx].input_device ) );
			}
			rtspServer->addServerMediaSession( sms );
		}
	}
	return 0;
}

extern "C"
void do_live555( void )
{
	( (BasicTaskScheduler0 *) scheduler )->SingleStep( 100 );
}

extern "C"
void addClientConnection( RTSP_DATA *clientdata )
{
	rtspServer->addClientConnection( clientdata );
}

extern "C"
void incoming_hander( void *session )
{
	RTSP_O_HTTPServer::incomingHandler( session );
}

extern "C"
int exit_live555()
{
	int idx;

	Medium::close(rtspServer); // will also reclaim "sms" and its "ServerMediaSubsession"s
	for (idx=0; idx<MAX_RTSP_STREAM; idx++)
		if (rtspstream[idx].input_device)
			Medium::close(rtspstream[idx].input_device);
	Env->reclaim();
	delete scheduler;

	return 0; // only to prevent compiler warning
}
