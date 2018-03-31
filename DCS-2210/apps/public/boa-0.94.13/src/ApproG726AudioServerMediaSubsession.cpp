#include "ApproG726AudioServerMediaSubsession.hh"
#include <liveMedia.hh>

extern unsigned audioSamplingFrequency;
extern unsigned audioNumChannels;

ApproG726AudioServerMediaSubsession *ApproG726AudioServerMediaSubsession::createNew( UsageEnvironment &env, ApproInput &Input )
{
	return new ApproG726AudioServerMediaSubsession( env, Input );
}

ApproG726AudioServerMediaSubsession::ApproG726AudioServerMediaSubsession( UsageEnvironment &env, ApproInput &Input )
	: ApproServerMediaSubsession( env, Input, audioSamplingFrequency * 8 * audioNumChannels )
{
}

FramedSource *ApproG726AudioServerMediaSubsession::createNewStreamSource( unsigned /*clientSessionId*/, unsigned &estBitrate )
{
	estBitrate = fEstimatedKbps;
	return fApproInput.audioSource();
}

RTPSink *ApproG726AudioServerMediaSubsession::createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * /*inputSource*/ )
{
	setAudioRTPSinkBufferSize();
	return SimpleRTPSink::createNew( envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, audioSamplingFrequency, "audio", "G726-32", audioNumChannels );
}
