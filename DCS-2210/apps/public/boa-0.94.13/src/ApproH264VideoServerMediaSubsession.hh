#ifndef _APPRO_H264_VIDEO_SERVER_MEDIA_SUBSESSION_HH
#define _APPRO_H264_VIDEO_SERVER_MEDIA_SUBSESSION_HH

#ifndef _APPRO_SERVER_MEDIA_SUBSESSION_HH
#include "ApproServerMediaSubsession.hh"
#endif

class ApproH264VideoServerMediaSubsession: public ApproServerMediaSubsession
{
public:
	static ApproH264VideoServerMediaSubsession *createNew( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate );
	void setDoneFlag() { fDoneFlag = ~0; }
	void checkForAuxSDPLine1();
private:
	ApproH264VideoServerMediaSubsession( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate );
	virtual ~ApproH264VideoServerMediaSubsession() {}
	virtual char const *getAuxSDPLine( RTPSink *rtpSink, FramedSource *inputSource );
	virtual FramedSource *createNewStreamSource( unsigned clientSessionId, unsigned &estBitrate );
	virtual RTPSink *createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource );
	char fDoneFlag; // used when setting up 'SDPlines'
	RTPSink *fDummyRTPSink; // ditto
};

#endif
