#ifndef _Appro_JPEG_VIDEO_SERVER_MEDIA_SUBSESSION_HH
#define _Appro_JPEG_VIDEO_SERVER_MEDIA_SUBSESSION_HH

#ifndef _Appro_SERVER_MEDIA_SUBSESSION_HH
#include "ApproServerMediaSubsession.hh"
#endif

class ApproJPEGVideoServerMediaSubsession: public ApproServerMediaSubsession
{
public:
	static ApproJPEGVideoServerMediaSubsession *createNew( UsageEnvironment &env, ApproInput &mjpegInput, unsigned estimatedBitrate );
private:
	ApproJPEGVideoServerMediaSubsession( UsageEnvironment &env, ApproInput &mjpegInput, unsigned estimatedBitrate );
	virtual ~ApproJPEGVideoServerMediaSubsession() {}
	virtual FramedSource *createNewStreamSource( unsigned clientSessionId, unsigned& estBitrate );
	virtual RTPSink *createNewRTPSink( Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource );
};

#endif
