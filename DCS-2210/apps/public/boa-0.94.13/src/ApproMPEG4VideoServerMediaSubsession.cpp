#include "ApproMPEG4VideoServerMediaSubsession.hh"
#include <MPEG4ESVideoRTPSink.hh>
#include <MPEG4VideoStreamDiscreteFramer.hh>

ApproMPEG4VideoServerMediaSubsession *ApproMPEG4VideoServerMediaSubsession::createNew( UsageEnvironment &env, ApproInput &mpeg4Input, unsigned estimatedBitrate )
{
	return new ApproMPEG4VideoServerMediaSubsession( env, mpeg4Input, estimatedBitrate );
}

ApproMPEG4VideoServerMediaSubsession::ApproMPEG4VideoServerMediaSubsession( UsageEnvironment &env, ApproInput &mpeg4Input, unsigned estimatedBitrate )
	: ApproServerMediaSubsession( env, mpeg4Input, estimatedBitrate )
{
}

void afterPlayingDummy( void *clientData )
{
	ApproMPEG4VideoServerMediaSubsession *subsess = (ApproMPEG4VideoServerMediaSubsession *) clientData;
	subsess->setDoneFlag();
}

void checkForAuxSDPLine( void *clientData )
{
	ApproMPEG4VideoServerMediaSubsession *subsess = (ApproMPEG4VideoServerMediaSubsession *) clientData;
	subsess->checkForAuxSDPLine1();
}

void ApproMPEG4VideoServerMediaSubsession::checkForAuxSDPLine1()
{
	if ( fDummyRTPSink->auxSDPLine() != NULL )
		setDoneFlag();
  	else
		nextTask() = envir().taskScheduler().scheduleDelayedTask( 100000, (TaskFunc *) checkForAuxSDPLine, this );
}

char const *ApproMPEG4VideoServerMediaSubsession::getAuxSDPLine( RTPSink *rtpSink, FramedSource *inputSource )
{
	fDummyRTPSink = rtpSink;
	fDummyRTPSink->startPlaying( *inputSource, afterPlayingDummy, this );
	checkForAuxSDPLine( this );
	fDoneFlag = 0;
	envir().taskScheduler().doEventLoop( &fDoneFlag );
	char const *auxSDPLine = fDummyRTPSink->auxSDPLine();
	return auxSDPLine;
}

FramedSource *ApproMPEG4VideoServerMediaSubsession::createNewStreamSource( unsigned /*clientSessionId*/, unsigned& estBitrate )
{
	estBitrate = fEstimatedKbps;
	return MPEG4VideoStreamDiscreteFramer::createNew( envir(), fApproInput.videoSource() );
}

RTPSink *ApproMPEG4VideoServerMediaSubsession::createNewRTPSink( Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * /*inputSource*/ )
{
	setVideoRTPSinkBufferSize();
	return MPEG4ESVideoRTPSink::createNew( envir(), rtpGroupsock, rtpPayloadTypeIfDynamic );
}
