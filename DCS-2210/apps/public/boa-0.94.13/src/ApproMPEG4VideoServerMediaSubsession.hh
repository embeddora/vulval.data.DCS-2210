#ifndef _Appro_MPEG4_VIDEO_SERVER_MEDIA_SUBSESSION_HH
#define _Appro_MPEG4_VIDEO_SERVER_MEDIA_SUBSESSION_HH

#ifndef _Appro_SERVER_MEDIA_SUBSESSION_HH
#include "ApproServerMediaSubsession.hh"
#endif

class ApproMPEG4VideoServerMediaSubsession: public ApproServerMediaSubsession
{
public:
	static ApproMPEG4VideoServerMediaSubsession *createNew( UsageEnvironment &env, ApproInput &mpeg4Input, unsigned estimatedBitrate );
	void setDoneFlag() { fDoneFlag = ~0; }
	void checkForAuxSDPLine1();
private:
	ApproMPEG4VideoServerMediaSubsession( UsageEnvironment &env, ApproInput &mpeg4Input, unsigned estimatedBitrate );
	virtual ~ApproMPEG4VideoServerMediaSubsession() {}
	virtual char const *getAuxSDPLine( RTPSink *rtpSink, FramedSource *inputSource );
	virtual FramedSource *createNewStreamSource( unsigned clientSessionId, unsigned &estBitrate );
	virtual RTPSink *createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource );
	char fDoneFlag; // used when setting up 'SDPlines'
	RTPSink *fDummyRTPSink; // ditto
};

#endif
