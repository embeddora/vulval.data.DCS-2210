#ifndef _Appro_PCM_AUDIO_SERVER_MEDIA_SUBSESSION_HH
#define _Appro_PCM_AUDIO_SERVER_MEDIA_SUBSESSION_HH

#include "ApproServerMediaSubsession.hh"

class ApproPCMAudioServerMediaSubsession: public ApproServerMediaSubsession
{
public:
	static ApproPCMAudioServerMediaSubsession *createNew( UsageEnvironment &env, ApproInput &Input );
private:
	ApproPCMAudioServerMediaSubsession( UsageEnvironment &env, ApproInput &Input );
	virtual ~ApproPCMAudioServerMediaSubsession() {}
	virtual FramedSource *createNewStreamSource( unsigned clientSessionId, unsigned &estBitrate );
	virtual RTPSink *createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource );
};

#endif
