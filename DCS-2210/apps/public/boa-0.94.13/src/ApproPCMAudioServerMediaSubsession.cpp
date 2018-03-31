#include "ApproPCMAudioServerMediaSubsession.hh"
#include <liveMedia.hh>

extern unsigned audioSamplingFrequency;
extern unsigned audioNumChannels;

ApproPCMAudioServerMediaSubsession *ApproPCMAudioServerMediaSubsession::createNew( UsageEnvironment &env, ApproInput &Input )
{
	return new ApproPCMAudioServerMediaSubsession( env, Input );
}

ApproPCMAudioServerMediaSubsession::ApproPCMAudioServerMediaSubsession( UsageEnvironment &env, ApproInput &Input )
	: ApproServerMediaSubsession( env, Input, audioSamplingFrequency * 8 * audioNumChannels )
{
}

FramedSource *ApproPCMAudioServerMediaSubsession::createNewStreamSource( unsigned /*clientSessionId*/, unsigned &estBitrate )
{
	estBitrate = fEstimatedKbps;
	return fApproInput.audioSource();
}

RTPSink *ApproPCMAudioServerMediaSubsession::createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * /*inputSource*/ )
{
	setAudioRTPSinkBufferSize();
	return SimpleRTPSink::createNew( envir(), rtpGroupsock, 0, audioSamplingFrequency, "audio", "PCMU", audioNumChannels );
}
