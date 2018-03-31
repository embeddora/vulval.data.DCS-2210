#include "ApproJPEGVideoServerMediaSubsession.hh"
#include "ApproJPEGStreamSource.hh"
#include <JPEGVideoRTPSink.hh>

ApproJPEGVideoServerMediaSubsession *ApproJPEGVideoServerMediaSubsession::createNew( UsageEnvironment &env, ApproInput &mjpegInput, unsigned estimatedBitrate )
{
	return new ApproJPEGVideoServerMediaSubsession( env, mjpegInput, estimatedBitrate );
}

ApproJPEGVideoServerMediaSubsession::ApproJPEGVideoServerMediaSubsession(UsageEnvironment &env, ApproInput &mjpegInput, unsigned estimatedBitrate )
	: ApproServerMediaSubsession( env, mjpegInput, estimatedBitrate )
{
}

FramedSource *ApproJPEGVideoServerMediaSubsession::createNewStreamSource( unsigned /*clientSessionId*/, unsigned &estBitrate )
{
	estBitrate = fEstimatedKbps;
	return ApproJPEGStreamSource::createNew( fApproInput.videoSource() );
}

RTPSink *ApproJPEGVideoServerMediaSubsession::createNewRTPSink( Groupsock * rtpGroupsock, unsigned char /*rtpPayloadTypeIfDynamic*/, FramedSource * /*inputSource*/ )
{
	setVideoRTPSinkBufferSize();
	return JPEGVideoRTPSink::createNew( envir(), rtpGroupsock );
}
