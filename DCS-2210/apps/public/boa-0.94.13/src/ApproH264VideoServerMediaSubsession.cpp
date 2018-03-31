#include "ApproH264VideoServerMediaSubsession.hh"
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamFramer.hh>
#include <Base64.hh>

ApproH264VideoServerMediaSubsession *ApproH264VideoServerMediaSubsession::createNew( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate )
{
	return new ApproH264VideoServerMediaSubsession( env, Input, estimatedBitrate );
}

ApproH264VideoServerMediaSubsession::ApproH264VideoServerMediaSubsession( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate )
	: ApproServerMediaSubsession( env, Input, estimatedBitrate )
{
}

static void afterPlayingDummy( void *clientData )
{
	ApproH264VideoServerMediaSubsession *subsess = (ApproH264VideoServerMediaSubsession *) clientData;

	subsess->setDoneFlag();
}

static void checkForAuxSDPLine( void *clientData )
{
	ApproH264VideoServerMediaSubsession *subsess = (ApproH264VideoServerMediaSubsession *) clientData;

	subsess->checkForAuxSDPLine1();
}

void ApproH264VideoServerMediaSubsession::checkForAuxSDPLine1()
{
	if ( fDummyRTPSink->auxSDPLine() != NULL )
		setDoneFlag();
  	else
		nextTask() = envir().taskScheduler().scheduleDelayedTask( 100000, (TaskFunc *) checkForAuxSDPLine, this );
}

char const *ApproH264VideoServerMediaSubsession::getAuxSDPLine( RTPSink *rtpSink, FramedSource *inputSource )
{
#if defined( LIVE )
	return rtpSink->auxSDPLine();
#elif defined( LIVE_20110616 )
	fDummyRTPSink = rtpSink;
	fDummyRTPSink->startPlaying( *inputSource, afterPlayingDummy, this );
	checkForAuxSDPLine( this );
	fDoneFlag = 0;
	envir().taskScheduler().doEventLoop( &fDoneFlag );
	char const *auxSDPLine = fDummyRTPSink->auxSDPLine();
	return auxSDPLine;
#endif
}

FramedSource *ApproH264VideoServerMediaSubsession::createNewStreamSource( unsigned /*clientSessionId*/, unsigned& estBitrate )
{
	estBitrate = fEstimatedKbps;
	return H264VideoStreamFramer::createNew( envir(), fApproInput.videoSource() );
}

#if defined( LIVE )
#define MAX_H264_HEADER		200
int GetSpropParameterSets( ApproInput &fApproInput, unsigned &profile_level_id, char *pBuff )
{
	static char tempBuff[512];
	char *ptr = tempBuff;
	char *pSPS = NULL;
	char *pPPS = NULL;
	char *pSPSEncode = NULL;
	char *pPPSEncode = NULL;
	int SPSlen = 0;
	int PPSlen = 0;
	int header_size;
	AV_DATA av_data;
	unsigned int cnt = 0;

	while ( ( ! pSPS ) || ( ! pPPS ) )
	{
		GetAVData( fApproInput.ProfileId|AV_OP_LOCK_H264_IFRAME, -1, &av_data );
		header_size = ( av_data.size < MAX_H264_HEADER ) ? av_data.size : MAX_H264_HEADER;
		memcpy( ptr = tempBuff, av_data.ptr, header_size );
		GetAVData( fApproInput.ProfileId|AV_OP_UNLOCK_H264, av_data.serial, NULL );
		while ( ptr < ( &tempBuff[header_size - 4] ) )
		{
			for ( cnt = 0; cnt < 3; cnt++ )	/* find 3 00 */
				if ( *( ptr + cnt ) != 0 )
					break;
			if ( cnt >= 3 )			/* skip more 00 */
				while ( ! *( ptr + cnt ) )
					cnt++;
			if ( ( cnt < 3 ) || ( *( ptr + cnt ) != 1 ) )
			{
				ptr += ( cnt + 1 );
				continue;
			}
			/* 00 00 00 01 found */
			if ( pSPS && ( ! SPSlen ) )
				SPSlen = ptr - pSPS;
			if ( pPPS && ( ! PPSlen ) )
				PPSlen = ptr - pPPS;
			ptr += ( cnt + 1 );
			switch ( *ptr & 0x1F )
			{
				case 7:
					if ( ! pSPS )
					{
						pSPS = ptr;
						SPSlen = 0;
					}
					break;
				case 8:
					if ( ! pPPS )
					{
						pPPS = ptr;
						PPSlen = 0;
					}
					break;
				default:
					ptr++;
					break;
			}
			if ( pSPS && pPPS && ( ! SPSlen ) && ( !PPSlen ) )
				break;
		}
	}
	profile_level_id = ( pSPS[1] << 16 ) + ( pSPS[2] << 8 ) + pSPS[3];
	pSPSEncode = base64Encode( pSPS, SPSlen );
	pPPSEncode = base64Encode( pPPS, PPSlen );
	sprintf( pBuff,"%s,%s;", (char *) pSPSEncode, (char *) pPPSEncode );
	delete[] pSPSEncode;
	delete[] pPPSEncode;
	return 0;
}
#endif

RTPSink *ApproH264VideoServerMediaSubsession::createNewRTPSink( Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * /*inputSource*/ )
{
#if defined( LIVE )
	unsigned profile_level_id;
	char sps_buf[512];

	setVideoRTPSinkBufferSize();
	GetSpropParameterSets( fApproInput, profile_level_id, sps_buf );
	return H264VideoRTPSink::createNew( envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, profile_level_id, sps_buf );
#elif defined( LIVE_20110616 )
	setVideoRTPSinkBufferSize();
	return H264VideoRTPSink::createNew( envir(), rtpGroupsock, rtpPayloadTypeIfDynamic );
#else
	return NULL;
#endif
}
