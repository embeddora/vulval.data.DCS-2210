#include "ApproInput.hh"
#include "Err.hh"
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <linux/soundcard.h>
#include <sysconf.h>

extern unsigned audioSamplingFrequency;
extern unsigned audioNumChannels;
extern SysInfo *sys_info;

typedef
struct s_buf_t
{
	char *buf;
	int len;
	struct s_buf_t *next;
}
buf_t;

////////// OpenFileSource definition //////////
// A common "FramedSource" subclass, used for reading from an open file:

class OpenFileSource: public FramedSource
{
protected:
	OpenFileSource( UsageEnvironment &env, ApproInput &input );
	virtual ~OpenFileSource() {}
	virtual int readFromFile() = 0;
private:
	virtual void doGetNextFrame();
	static void incomingDataHandler( OpenFileSource *source );
	void incomingDataHandler1();
protected:
	ApproInput &fInput;
	struct timeval fBaseTime;
};


////////// VideoOpenFileSource definition //////////

class VideoOpenFileSource: public OpenFileSource
{
public:
	VideoOpenFileSource( UsageEnvironment &env, ApproInput &input );
	virtual ~VideoOpenFileSource();
protected:
	virtual int readFromFile();
private:
	STREAM_DATA stream;
	char dbuffer[1024];
	unsigned totalsize;
	buf_t *now_buf_ptr;
};

////////// AudioOpenFileSource definition //////////

class AudioOpenFileSource: public OpenFileSource
{
public:
	AudioOpenFileSource( UsageEnvironment &env, ApproInput &input );
	virtual ~AudioOpenFileSource();
protected: // redefined virtual functions:
	virtual int readFromFile();
private:
	STREAM_DATA stream;
};

////////// ApproInput implementation //////////

ApproInput *ApproInput::createNew( UsageEnvironment &env, int vType, int profile_id )
{
	return new ApproInput( env, vType, profile_id );
}

FramedSource *ApproInput::videoSource()
{
	if ( fOurVideoSource == NULL )
		fOurVideoSource = new VideoOpenFileSource( envir(), *this );
	return fOurVideoSource;
}

FramedSource *ApproInput::audioSource()
{
	if ( fOurAudioSource == NULL )
		fOurAudioSource = new AudioOpenFileSource( envir(), *this );
	return fOurAudioSource;
}

ApproInput::ApproInput( UsageEnvironment &env, int vType, int profile_id )
  : Medium( env ), videoType( vType ), ProfileId( profile_id ), fOurVideoSource( NULL ), fOurAudioSource( NULL )
{
}

////////// OpenFileSource implementation //////////

OpenFileSource::OpenFileSource( UsageEnvironment &env, ApproInput &input )
  : FramedSource( env ), fInput( input )
{
	fBaseTime.tv_sec = 0;
	fBaseTime.tv_usec = 0;
}

void OpenFileSource::doGetNextFrame()
{
	this->incomingDataHandler1();
}

void OpenFileSource::incomingDataHandler( OpenFileSource *source )
{
	source->incomingDataHandler1();
}

void OpenFileSource::incomingDataHandler1()
{
	int ret;
	
	if ( ! isCurrentlyAwaitingData() )
		return; // we're not ready for the data yet
	ret = readFromFile();
	if ( ret < 0 )
	{
		handleClosure( this );
		fprintf( stderr, "In Grab Image, the source stops being readable!!!!\n" );
	}
	else if ( ret == 0 )
		nextTask() = envir().taskScheduler().scheduleDelayedTask( 10000, (TaskFunc *) incomingDataHandler, this );
	else
		nextTask() = envir().taskScheduler().scheduleDelayedTask( 0, (TaskFunc *) afterGetting, this );
}

////////// VideoOpenFileSource implementation //////////
VideoOpenFileSource::VideoOpenFileSource( UsageEnvironment &env, ApproInput &input )
	: OpenFileSource( env, input ), totalsize( 0 ), now_buf_ptr( NULL )
{
	memset( &stream, 0, sizeof( stream ) );
	memset( &dbuffer, 0, sizeof( dbuffer ) );
}

VideoOpenFileSource::~VideoOpenFileSource()
{
	fInput.fOurVideoSource = NULL;
	if ( stream.serial_lock > 0 )
		GetAVData( fInput.ProfileId | fInput.videoType | AV_OP_UNLOCK, stream.serial_lock, NULL );
}

const char mpeg4_header[] = { 0x00, 0x00, 0x01, 0xb0, 0x01, 0x00, 0x00, 0x01, 0xb5, 0x09 };

int VideoOpenFileSource::readFromFile()
{
	int AvailSize = 0;
	char *ptr;

	/* Old codes are cleared by Ned Wu in April 26, 2011. */
	/* If you need those codes, checkout from CVS please. */
	switch ( fInput.videoType )
	{
		case VIDEO_TYPE_MJPEG:
			if ( ! now_buf_ptr )
			{
				if ( get_mjpeg_data( &stream, fInput.ProfileId, sys_info->osd_debug.rtsp_framerate ) != RET_STREAM_SUCCESS )
					return 0;
				now_buf_ptr = (buf_t *) dbuffer;
				now_buf_ptr->buf = stream.av_data.ptr;
				now_buf_ptr->len = stream.av_data.size;
				now_buf_ptr->next = NULL;
				totalsize = stream.av_data.size;
			}
			break;
		case VIDEO_TYPE_MPEG4:
			if ( ! now_buf_ptr )
			{
				if ( sys_info->onvif.sendiframe )
				{
					if ( stream.serial_lock )
						GetAVData( fInput.ProfileId | fInput.videoType | AV_OP_UNLOCK, stream.serial_lock, NULL );
					memset( &stream, 0, sizeof( stream ) );
				}
				if ( get_mpeg4_data( &stream, fInput.ProfileId, sys_info->osd_debug.rtsp_framerate ) != RET_SUCCESS )
					return 0;
				if ( sys_info->onvif.sendiframe )
					sys_info->onvif.sendiframe = 0;
				if ( stream.av_data.ptr[4] & 0xC0 )	/* assume every frame begin in '000001b6xx' */
				{	/* 1:P frame */
					now_buf_ptr = (buf_t *) dbuffer;
					now_buf_ptr->buf = stream.av_data.ptr;
					now_buf_ptr->len = stream.av_data.size;
					now_buf_ptr->next = NULL;
					totalsize = stream.av_data.size;
				}
				else
				{	/* 0:I frame */
					AV_DATA vol_data;
					buf_t *next;

					if ( GetAVData( fInput.ProfileId | AV_OP_LOCK_MP4_VOL, -1, &vol_data ) != RET_SUCCESS )
					{
						fprintf( stderr, "Error on Get Vol data\n" );
						return -1;
					}
					now_buf_ptr = (buf_t *) dbuffer;
					now_buf_ptr->buf = (char *) ( now_buf_ptr + 2 );
					now_buf_ptr->len = sizeof( mpeg4_header ) + vol_data.size;
					next = now_buf_ptr->next = now_buf_ptr + 1;
					memcpy( now_buf_ptr->buf, mpeg4_header, sizeof( mpeg4_header ) );
					memcpy( now_buf_ptr->buf + sizeof( mpeg4_header ), vol_data.ptr, vol_data.size );
					next->buf = stream.av_data.ptr;
					next->len = stream.av_data.size;
					next->next = NULL;
					totalsize = sizeof( mpeg4_header ) + vol_data.size + stream.av_data.size;
				}
			}
			break;
		case VIDEO_TYPE_H264:
			if ( ! now_buf_ptr )
			{
				if ( sys_info->onvif.sendiframe )
				{
					if ( stream.serial_lock )
						GetAVData( fInput.ProfileId | fInput.videoType | AV_OP_UNLOCK, stream.serial_lock, NULL );
					memset( &stream, 0, sizeof( stream ) );
				}
				if ( get_h264_data( &stream, fInput.ProfileId, sys_info->osd_debug.rtsp_framerate ) != RET_SUCCESS )
					return 0;
				if ( sys_info->onvif.sendiframe )
					sys_info->onvif.sendiframe = 0;
				now_buf_ptr = (buf_t *) dbuffer;
				now_buf_ptr->buf = stream.av_data.ptr;
				now_buf_ptr->len = stream.av_data.size;
				now_buf_ptr->next = NULL;
#if defined( LIVE )
				if ( ( now_buf_ptr->buf[4] & 0x1F ) <= 5 )
				{
					now_buf_ptr->len -= 4;
					now_buf_ptr->buf += 4;
				}
				else
				{
					int i, start = 0;

					for ( ptr = now_buf_ptr->buf, i = 5; i < now_buf_ptr->len; i++ )
					{
						if ( ( ptr[i] == 0 ) && ( ptr[i + 1] == 0 ) && ( ptr[i + 2] == 0 ) &&
						     ( ptr[i + 3] == 1 ) && ( ( ptr[i + 4] & 0x1F ) == 5 ) )
						{
							start = i + 4;
							break;
						}
					}
					if ( start )
					{
						now_buf_ptr->len -= start;
						now_buf_ptr->buf += start;
					}
					else
					{
						fprintf( stderr, "I-frame header not found\n" );
						return -1;
					}
				}
				totalsize = now_buf_ptr->len;
#endif	/* defined( LIVE ) */
			}
#if defined( LIVE_20110616 )
			totalsize = now_buf_ptr->len;
#endif	/* defined( LIVE_20110616 ) */
			break;
		default:
			return -1;
	}
	if ( totalsize > fMaxSize )
	{
		fprintf( stderr, "Frame Truncated\n" );
		fNumTruncatedBytes = totalsize - fMaxSize;
		AvailSize = fFrameSize = fMaxSize;
	}
	else
	{
		fNumTruncatedBytes = 0;
		AvailSize = fFrameSize = totalsize;
	}
	for ( ptr = (char *) fTo; AvailSize && now_buf_ptr; )
	{
		if ( AvailSize > now_buf_ptr->len )
		{
			memcpy( ptr, now_buf_ptr->buf, now_buf_ptr->len );
			AvailSize -= now_buf_ptr->len;
			ptr += now_buf_ptr->len;
			now_buf_ptr = now_buf_ptr->next;
		}
		else
		{
			memcpy( ptr, now_buf_ptr->buf, AvailSize );
			now_buf_ptr->buf += AvailSize;
			now_buf_ptr->len -= AvailSize;
			if ( ! now_buf_ptr->len )
				now_buf_ptr = now_buf_ptr->next;
			AvailSize = 0;
		}
	}
	if ( ! fNumTruncatedBytes )
		now_buf_ptr = NULL;
	fPresentationTime.tv_sec = stream.av_data.timestamp / 1000;
	fPresentationTime.tv_usec = ( stream.av_data.timestamp % 1000 ) * 1000;
#if 0
	if ( ( fBaseTime.tv_sec == 0 ) && ( fBaseTime.tv_usec == 0 ) )
		fBaseTime = fPresentationTime;
	if ( fPresentationTime.tv_usec >= fBaseTime.tv_usec )
	{
		fPresentationTime.tv_sec -= fBaseTime.tv_sec;
		fPresentationTime.tv_usec -= fBaseTime.tv_usec;
	}
	else
	{
		fPresentationTime.tv_sec -= fBaseTime.tv_sec + 1;
		fPresentationTime.tv_usec = fPresentationTime.tv_usec + 1000000 - fBaseTime.tv_usec;
	}
#endif
	return 1;
}

////////// AudioOpenFileSource implementation //////////

AudioOpenFileSource::AudioOpenFileSource( UsageEnvironment &env, ApproInput &input )
	: OpenFileSource( env, input )
{
	memset( &stream, 0, sizeof( stream ) );
}

AudioOpenFileSource::~AudioOpenFileSource()
{
	fInput.fOurAudioSource = NULL;
	if ( stream.serial_lock > 0 )
		GetAVData( AV_FMT_AUDIO | AV_OP_UNLOCK, stream.serial_lock, NULL );
}

int AudioOpenFileSource::readFromFile()
{
	int timeinc;

	// Read available audio data:
	if ( get_audio_data(&stream, 0) != RET_STREAM_SUCCESS )
		return 0;
	if ( stream.av_data.size > fMaxSize )
		stream.av_data.size = fMaxSize;
	memcpy( fTo, stream.av_data.ptr, stream.av_data.size );
	fFrameSize = stream.av_data.size;
	fPresentationTime.tv_sec = stream.av_data.timestamp / 1000;
	fPresentationTime.tv_usec = ( stream.av_data.timestamp % 1000 ) * 1000;
#if defined( LIVE )
	/* PR#2665 fix from Robin
	 * Assuming audio format = AFMT_S16_LE
	 * Get the current time
	 * Substract the time increment of the audio oss buffer, which is equal to
	 * buffer_size / channel_number / sample_rate / sample_size ==> 400+ millisec
	 */
	timeinc = fFrameSize * 1000 / audioNumChannels / ( audioSamplingFrequency / 1000 ) / 2;
	while ( fPresentationTime.tv_usec < timeinc )
	{
		fPresentationTime.tv_sec -= 1;
		timeinc -= 1000000;
	}
	fPresentationTime.tv_usec -= timeinc;
#endif
#if 0
	if ( ( fBaseTime.tv_sec == 0 ) && ( fBaseTime.tv_usec == 0 ) )
		fBaseTime = fPresentationTime;
	if ( fPresentationTime.tv_usec >= fBaseTime.tv_usec )
	{
		fPresentationTime.tv_sec -= fBaseTime.tv_sec;
		fPresentationTime.tv_usec -= fBaseTime.tv_usec;
	}
	else
	{
		fPresentationTime.tv_sec -= fBaseTime.tv_sec + 1;
		fPresentationTime.tv_usec = fPresentationTime.tv_usec + 1000000 - fBaseTime.tv_usec;
	}
#endif
	return 1;
}
