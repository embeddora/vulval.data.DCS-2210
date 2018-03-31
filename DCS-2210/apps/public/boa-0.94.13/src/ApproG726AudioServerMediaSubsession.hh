#ifndef _Appro_G726_AUDIO_SERVER_MEDIA_SUBSESSION_HH
#define _Appro_G726_AUDIO_SERVER_MEDIA_SUBSESSION_HH

#include "ApproServerMediaSubsession.hh"

class ApproG726AudioServerMediaSubsession: public ApproServerMediaSubsession
{
public:
	static ApproG726AudioServerMediaSubsession *createNew( UsageEnvironment &env, ApproInput &Input );
private:
	ApproG726AudioServerMediaSubsession( UsageEnvironment &env, ApproInput &Input );
	virtual ~ApproG726AudioServerMediaSubsession() {}
	virtual FramedSource *createNewStreamSource( unsigned clientSessionId, unsigned &estBitrate );
	virtual RTPSink *createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource );
};

#endif
