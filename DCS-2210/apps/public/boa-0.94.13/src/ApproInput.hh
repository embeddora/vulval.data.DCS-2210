#ifndef _APPRO_INPUT_HH
#define _APPRO_INPUT_HH

#include <MediaSink.hh>
#include <Appro_interface.h>

class ApproInput: public Medium
{
public:
	static ApproInput *createNew( UsageEnvironment &env, int vType, int profile_id );
	FramedSource *videoSource();
	FramedSource *audioSource();
	int videoType;
	int ProfileId;
private:
	ApproInput( UsageEnvironment &env, int vType, int profile_id );
	virtual ~ApproInput() {}
	friend class VideoOpenFileSource;
	friend class AudioOpenFileSource;
	FramedSource *fOurVideoSource;
	FramedSource *fOurAudioSource;
};

#ifdef PLAT_DM355
#define VIDEO_TYPE_MJPEG	0
#define VIDEO_TYPE_MPEG4	1
#define VIDEO_TYPE_H264		2
#else
#define VIDEO_TYPE_MJPEG	AV_FMT_MJPEG
#define VIDEO_TYPE_MPEG4	AV_FMT_MPEG4
#define VIDEO_TYPE_H264		AV_FMT_H264
#endif

// Functions to set the optimal buffer size for RTP sink objects.
// These should be called before each RTPSink is created.
#define AUDIO_MAX_FRAME_SIZE 20480
#define VIDEO_MAX_FRAME_SIZE 400000
inline void setAudioRTPSinkBufferSize() { if (OutPacketBuffer::maxSize != VIDEO_MAX_FRAME_SIZE) OutPacketBuffer::maxSize = AUDIO_MAX_FRAME_SIZE; }
inline void setVideoRTPSinkBufferSize() { OutPacketBuffer::maxSize = VIDEO_MAX_FRAME_SIZE; }

#endif
