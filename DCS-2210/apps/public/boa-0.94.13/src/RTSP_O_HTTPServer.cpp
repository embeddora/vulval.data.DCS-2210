#include "RTSPServer.hh"
#include "RTSP_O_HTTPServer.hh"
#include "RTSPCommon.hh"
#include <GroupsockHelper.hh>

#include <time.h> // for "strftime()" and "gmtime()"

////////// RTSP_O_HTTPServer //////////

RTSP_O_HTTPServer *RTSP_O_HTTPServer::createNew(
	UsageEnvironment &env, UserAuthenticationDatabase *authDatabase )
{
	return new RTSP_O_HTTPServer( env, authDatabase );
}

Boolean RTSP_O_HTTPServer::lookupByName(
	UsageEnvironment &env, char const *name, RTSP_O_HTTPServer *&resultServer )
{
	Medium *medium;

	resultServer = NULL; // unless we succeed
	if ( ! Medium::lookupByName( env, name, medium ) )
		return False;
	if ( ! medium->isRTSPServer() )
	{
		env.setResultMsg( name, " is not a RTSP server" );
		return False;
	}
	resultServer = (RTSP_O_HTTPServer *) medium;
	return True;
}

void RTSP_O_HTTPServer::addServerMediaSession( ServerMediaSession *serverMediaSession )
{
	if ( serverMediaSession == NULL )
		return;
	char const *sessionName = serverMediaSession->streamName();
	if ( sessionName == NULL )
		sessionName = "";
	ServerMediaSession *existingSession =
		(ServerMediaSession *)( fServerMediaSessions->Add( sessionName, (void *) serverMediaSession ) );
	removeServerMediaSession( existingSession ); // if any
}

ServerMediaSession *RTSP_O_HTTPServer::lookupServerMediaSession( char const *streamName )
{
	return (ServerMediaSession *) ( fServerMediaSessions->Lookup( streamName ) );
}

void RTSP_O_HTTPServer::removeServerMediaSession( ServerMediaSession *serverMediaSession )
{
	if ( serverMediaSession == NULL )
		return;
	fServerMediaSessions->Remove( serverMediaSession->streamName() );
	if ( serverMediaSession->referenceCount() == 0 )
		Medium::close( serverMediaSession );
	else
		serverMediaSession->deleteWhenUnreferenced() = True;
}

char *RTSP_O_HTTPServer::rtspURL( ServerMediaSession const *serverMediaSession ) const
{
	struct sockaddr_in ourAddress;
	char urlBuffer[100];
	char const* sessionName = serverMediaSession->streamName();

	ourAddress.sin_addr.s_addr = ourIPAddress( envir() ); // hack
	sprintf( urlBuffer, "rtsp://%s/", our_inet_ntoa( ourAddress.sin_addr ) );
	char *resultURL = new char[strlen( urlBuffer ) + strlen( sessionName ) + 1];
	sprintf( resultURL, "%s%s", urlBuffer, sessionName );
	return resultURL;
}

RTSP_O_HTTPServer::RTSP_O_HTTPServer(
	UsageEnvironment &env, UserAuthenticationDatabase *authDatabase )
	: Medium( env ), fAuthDB( authDatabase ),
	  fServerMediaSessions( HashTable::create( STRING_HASH_KEYS ) ), fSessionIdCounter( 0 )
{
}

RTSP_O_HTTPServer::~RTSP_O_HTTPServer()
{
	ServerMediaSession *serverMediaSession;

	while ( ( serverMediaSession = (ServerMediaSession *) fServerMediaSessions->RemoveNext() ) != NULL )
		removeServerMediaSession( serverMediaSession );
	delete fServerMediaSessions;
}

Boolean RTSP_O_HTTPServer::isRTSPServer() const
{
	return True;
}

void RTSP_O_HTTPServer::addClientConnection( RTSP_DATA *workdata )
{
	if ( ! workdata->rtspclientsession )
	{
		// Create a new object for this RTSP session:
		if ( ++fSessionIdCounter == 0 )
			++fSessionIdCounter;
		workdata->rtspclientsession = new RTSPClientSession( *this, fSessionIdCounter, workdata );
	}
}

////////// RTSP_O_HTTPServer::RTSPClientSession //////////

RTSP_O_HTTPServer::RTSPClientSession::RTSPClientSession( RTSP_O_HTTPServer &ourServer, unsigned sessionId, RTSP_DATA *workdata )
	: fOurServer( ourServer ), fOurSessionId( sessionId ),
	  fOurServerMediaSession( NULL ), fIsMulticast( False ), fSessionIsActive( True ), fStreamAfterSETUP( False ),
	  fTCPStreamIdCount( 0 ), fNumStreamStates( 0 ), fStreamStates( NULL ), fClientData( workdata )
{
	resetRequestBuffer();
	workdata->flags |= FLAGS_LIVE555_SETUP_OK;
	workdata->flags &= ~ FLAGS_LIVE555_PLAY;
}

RTSP_O_HTTPServer::RTSPClientSession::~RTSPClientSession()
{
	reclaimStreamStates();
	if ( fOurServerMediaSession != NULL )
	{
		fOurServerMediaSession->decrementReferenceCount();
		if ( fOurServerMediaSession->referenceCount() == 0 &&
		     fOurServerMediaSession->deleteWhenUnreferenced() )
			fOurServer.removeServerMediaSession( fOurServerMediaSession );
	}
}

void RTSP_O_HTTPServer::RTSPClientSession::reclaimStreamStates()
{
	for ( unsigned i = 0; i < fNumStreamStates; ++i )
		if ( fStreamStates[i].subsession != NULL )
			fStreamStates[i].subsession->deleteStream( fOurSessionId, fStreamStates[i].streamToken );
	delete[] fStreamStates;
	fStreamStates = NULL;
	fNumStreamStates = 0;
}

void RTSP_O_HTTPServer::RTSPClientSession::resetRequestBuffer()
{
	fRequestBufferBytesLeft = sizeof fRequestBuffer;
}

extern "C" int base64decode( void *dst, char *src, int maxlen );

void RTSP_O_HTTPServer::RTSPClientSession::incomingRequestHandler1()
{
	request *req = fClientData->inreq;
	int len = 0;
	char *check;
	int bytesRead = 0;

	if ( req )
	{
		if ( req->parse_pos < req->client_stream_pos )
		{
			req->client_stream[req->client_stream_pos] = 0;
			len = base64decode( req->buffer + req->buffer_end, req->client_stream + req->parse_pos, BUFFER_SIZE - req->buffer_end ) - 1;
			req->parse_pos = req->client_stream_pos = 0;
			req->buffer_end += len;
		}
		do
		{
			for ( check = req->buffer + req->buffer_start;
			      ( check - req->buffer ) <= ( req->buffer_end - 4 );
			      check++ )
			{
				if ( ! memcmp( check, "\r\n\r\n", 4 ) )
				{
					check += 4;
					resetRequestBuffer();
					memcpy( fRequestBuffer, req->buffer + req->buffer_start, bytesRead = check - ( req->buffer + req->buffer_start ) );
					fRequestBuffer[bytesRead] = 0;
					if ( ( check - req->buffer ) >= req->buffer_end )
						req->buffer_start = req->buffer_end = 0;
					else
						req->buffer_start = check - req->buffer;
					break;
				}
				if ( ( check - req->buffer ) == ( req->buffer_end - 4 ) )
					return;
			}
			if ( bytesRead && ( (unsigned) bytesRead < fRequestBufferBytesLeft ) )
			{
				char cmdName[RTSP_PARAM_STRING_MAX];
				char urlPreSuffix[RTSP_PARAM_STRING_MAX];
				char urlSuffix[RTSP_PARAM_STRING_MAX];
				char cseq[RTSP_PARAM_STRING_MAX];

				if ( ! parseRTSPRequestString( (char *) fRequestBuffer, bytesRead,
				                               cmdName, sizeof cmdName,
				                               urlPreSuffix, sizeof urlPreSuffix,
				                               urlSuffix, sizeof urlSuffix,
				                               cseq, sizeof cseq ) )
					handleCmd_bad( cseq );
				else
				{
//					fprintf( stderr, "cmdName \"%s\", urlPreSuffix \"%s\", urlSuffix \"%s\"\n", cmdName, urlPreSuffix, urlSuffix );
					if ( strcmp( cmdName, "OPTIONS" ) == 0 )
						handleCmd_OPTIONS( cseq );
					else if ( strcmp( cmdName, "DESCRIBE" ) == 0 )
						handleCmd_DESCRIBE( cseq, urlSuffix, (char const *) fRequestBuffer );
					else if ( strcmp( cmdName, "SETUP" ) == 0 )
						handleCmd_SETUP(cseq, urlPreSuffix, urlSuffix, (char const*)fRequestBuffer);
					else if ( ( strcmp( cmdName, "TEARDOWN" ) == 0 ) ||
					          ( strcmp( cmdName, "PLAY" ) == 0 ) ||
					          ( strcmp( cmdName, "PAUSE" ) == 0 ) ||
					          ( strcmp( cmdName, "GET_PARAMETER" ) == 0 ) )
						handleCmd_withinSession( cmdName, urlPreSuffix, urlSuffix, cseq, (char const *) fRequestBuffer );
					else
						handleCmd_notSupported( cseq );
				}
				if ( fClientData->outreq )
					write( fClientData->outreq->fd, fResponseBuffer, strlen( (char const *) fResponseBuffer ) );
				if ( strcmp( cmdName, "SETUP" ) == 0 && fStreamAfterSETUP )
					handleCmd_withinSession( "PLAY", urlPreSuffix, urlSuffix, cseq, (char const *) fRequestBuffer );
				resetRequestBuffer(); // to prepare for any subsequent request
			}
		}
		while ( fSessionIsActive && ( ( req->buffer_end - req->buffer_start ) > 0 ) );
	}
	if ( ( ! ( fClientData->flags & FLAGS_CONNECT_OK ) ) ||
	     ( (unsigned) bytesRead >= fRequestBufferBytesLeft ) )
	{
		fSessionIsActive = False;
	}
	if ( ! fSessionIsActive )
	{
		fClientData->flags &= ~ ( FLAGS_LIVE555_SETUP_OK | FLAGS_LIVE555_PLAY );
		if ( fClientData->status != STATUS_CLOSED )
			fClientData->status = STATUS_WAIT_CLOSE;
		delete this;
	}
}

// Generate a "Date:" header for use in a RTSP response:
static char const *dateHeader()
{
	static char buf[200];
	time_t tt = time(NULL);

	strftime( buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime( &tt ) );
	return buf;
}

//static char const* allowedCommandNames = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER";
static char const* allowedCommandNames = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE";

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_bad( char const * /*cseq*/ )
{
	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",
	          dateHeader(), allowedCommandNames );
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_notSupported( char const *cseq )
{
	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
	          cseq, dateHeader(), allowedCommandNames );
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_notFound( char const *cseq )
{
	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 404 Stream Not Found\r\nCSeq: %s\r\n%s\r\n",
	          cseq, dateHeader() );
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_OPTIONS( char const *cseq )
{
	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
	          cseq, dateHeader(), allowedCommandNames );
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_DESCRIBE(char const* cseq, char const* urlSuffix,
		     char const* fullRequestStr) {
  char* sdpDescription = NULL;
  char* rtspURL = NULL;
  do {
      if (!authenticationOK("DESCRIBE", cseq, urlSuffix, fullRequestStr))
          break;

    ServerMediaSession* session = fOurServer.lookupServerMediaSession(urlSuffix);
    if (session == NULL) {
      handleCmd_notFound(cseq);
      break;
    }

    sdpDescription = session->generateSDPDescription();
    if (sdpDescription == NULL) {
      snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
	       "RTSP/1.0 404 File Not Found, Or In Incorrect Format\r\n"
	       "CSeq: %s\r\n"
	       "%s\r\n",
	       cseq,
	       dateHeader());
     break;
    }
    unsigned sdpDescriptionSize = strlen(sdpDescription);

    rtspURL = fOurServer.rtspURL( session );

    snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
	     "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
	     "%s"
	     "Content-Base: %s/\r\n"
	     "Content-Type: application/sdp\r\n"
	     "Content-Length: %d\r\n\r\n"
	     "%s",
	     cseq,
	     dateHeader(),
	     rtspURL,
	     sdpDescriptionSize,
	     sdpDescription);
  } while (0);

  delete[] sdpDescription;
  delete[] rtspURL;
}

static void parseTransportHeader(char const* buf,
				 char*& destinationAddressStr,
				 u_int8_t& destinationTTL,
				 unsigned char& rtpChannelId, // if TCP
				 unsigned char& rtcpChannelId // if TCP
				 ) {
  // Initialize the result parameters to default values:
  destinationAddressStr = NULL;
  destinationTTL = 255;
  rtpChannelId = rtcpChannelId = 0xFF;

  unsigned ttl, rtpCid, rtcpCid;

  // First, find "Transport:"
  while (1) {
    if (*buf == '\0') return; // not found
    if (_strncasecmp(buf, "Transport: ", 11) == 0) break;
    ++buf;
  }

  // Then, run through each of the fields, looking for ones we handle:
  char const* fields = buf + 11;
  char* field = strDupSize(fields);
  while (sscanf(fields, "%[^;]", field) == 1) {
    if (_strncasecmp(field, "destination=", 12) == 0) {
      delete[] destinationAddressStr;
      destinationAddressStr = strDup(field+12);
    } else if (sscanf(field, "ttl%u", &ttl) == 1) {
      destinationTTL = (u_int8_t)ttl;
    } else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
      rtpChannelId = (unsigned char)rtpCid;
      rtcpChannelId = (unsigned char)rtcpCid;
    }

    fields += strlen(field);
    while (*fields == ';') ++fields; // skip over separating ';' chars
    if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
  }
  delete[] field;
}

static Boolean parsePlayNowHeader(char const* buf) {
  // Find "x-playNow:" header, if present
  while (1) {
    if (*buf == '\0') return False; // not found
    if (_strncasecmp(buf, "x-playNow:", 10) == 0) break;
    ++buf;
  }

  return True;
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_SETUP(char const* cseq,
		  char const* urlPreSuffix, char const* urlSuffix,
		  char const* fullRequestStr) {
  char const* streamName = urlPreSuffix;
  char const* trackId = urlSuffix;

  if (fOurServerMediaSession != NULL
      && strcmp(streamName, fOurServerMediaSession->streamName()) != 0) {
    fOurServerMediaSession = NULL;
  }
  if (fOurServerMediaSession == NULL) {

    if (streamName[0] != '\0' ||
	fOurServer.lookupServerMediaSession("") != NULL) { // normal case
    } else { // weird case: there was no track id in the URL
      streamName = urlSuffix;
      trackId = NULL;
    }
    fOurServerMediaSession = fOurServer.lookupServerMediaSession(streamName);
    if (fOurServerMediaSession == NULL) {
      handleCmd_notFound(cseq);
      return;
    }

    fOurServerMediaSession->incrementReferenceCount();

    // Set up our array of states for this session's subsessions (tracks):
    reclaimStreamStates();
    ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
    for (fNumStreamStates = 0; iter.next() != NULL; ++fNumStreamStates) {}
    fStreamStates = new struct streamState[fNumStreamStates];
    iter.reset();
    ServerMediaSubsession* subsession;
    for (unsigned i = 0; i < fNumStreamStates; ++i) {
      subsession = iter.next();
      fStreamStates[i].subsession = subsession;
      fStreamStates[i].streamToken = NULL; // for now; reset by SETUP later
    }
  }

  // Look up information for the specified subsession (track):
  ServerMediaSubsession* subsession = NULL;
  unsigned streamNum;
  if (trackId != NULL && trackId[0] != '\0') { // normal case
    for (streamNum = 0; streamNum < fNumStreamStates; ++streamNum) {
      subsession = fStreamStates[streamNum].subsession;
      if (subsession != NULL && strcmp(trackId, subsession->trackId()) == 0) break;
    }
    if (streamNum >= fNumStreamStates) {
      handleCmd_notFound(cseq);
      return;
    }
  } else {
    if (fNumStreamStates != 1) {
      handleCmd_bad(cseq);
      return;
    }
    streamNum = 0;
    subsession = fStreamStates[streamNum].subsession;
  }
  char* clientsDestinationAddressStr;
  u_int8_t clientsDestinationTTL;
  unsigned char rtpChannelId, rtcpChannelId;
  int tcpSocketNum;
  parseTransportHeader(fullRequestStr, clientsDestinationAddressStr, clientsDestinationTTL,
		       rtpChannelId, rtcpChannelId);
  rtpChannelId = fTCPStreamIdCount++;
  rtcpChannelId = fTCPStreamIdCount++;
#if defined( LIVE )
  float rangeStart = 0.0, rangeEnd = 0.0;
#elif defined( LIVE_20110616 )
  double rangeStart = 0.0, rangeEnd = 0.0;
#endif
  fStreamAfterSETUP = parseRangeHeader(fullRequestStr, rangeStart, rangeEnd) ||
                      parsePlayNowHeader(fullRequestStr);
  netAddressBits destinationAddress = 0;
  u_int8_t destinationTTL = 255;
  if ( fClientData->outreq )
    tcpSocketNum = fClientData->outreq->fd;
  else
    tcpSocketNum = -1;
  delete[] clientsDestinationAddressStr;
  Port clientRTPPort(-1);
  Port clientRTCPPort(-1);
  Port serverRTPPort(0);
  Port serverRTCPPort(0);
  subsession->getStreamParameters(fOurSessionId, 0,
				  clientRTPPort, clientRTCPPort,
				  tcpSocketNum, rtpChannelId, rtcpChannelId,
				  destinationAddress, destinationTTL, fIsMulticast,
				  serverRTPPort, serverRTCPPort,
				  fStreamStates[streamNum].streamToken);
  snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "%s"
            "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
            "Session: %d;timeout=80\r\n\r\n",
            cseq,
            dateHeader(),
            rtpChannelId, rtcpChannelId,
            fOurSessionId);
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_withinSession(char const* cmdName,
			  char const* urlPreSuffix, char const* urlSuffix,
			  char const* cseq, char const* fullRequestStr) {
  if (fOurServerMediaSession == NULL) { // There wasn't a previous SETUP!
    handleCmd_notSupported(cseq);
    return;
  }
  ServerMediaSubsession* subsession;
  if (urlSuffix[0] != '\0' &&
      strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0) {
    ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
    while ((subsession = iter.next()) != NULL) {
      if (strcmp(subsession->trackId(), urlSuffix) == 0) break; // success
    }
    if (subsession == NULL) { // no such track!
      handleCmd_notFound(cseq);
      return;
    }
  } else if (strcmp(fOurServerMediaSession->streamName(), urlSuffix) == 0 ||
	     strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0) {
    subsession = NULL;
  } else { // the request doesn't match a known stream and/or track at all!
    handleCmd_notFound(cseq);
    return;
  }

  if (strcmp(cmdName, "TEARDOWN") == 0) {
    handleCmd_TEARDOWN(subsession, cseq);
  } else if (strcmp(cmdName, "PLAY") == 0) {
    handleCmd_PLAY(subsession, cseq, fullRequestStr);
  } else if (strcmp(cmdName, "PAUSE") == 0) {
    handleCmd_PAUSE(subsession, cseq);
  } else if (strcmp(cmdName, "GET_PARAMETER") == 0) {
    handleCmd_GET_PARAMETER(subsession, cseq, fullRequestStr);
  } else if (strcmp(cmdName, "SET_PARAMETER") == 0) {
    handleCmd_SET_PARAMETER(subsession, cseq, fullRequestStr);
  }
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_TEARDOWN( ServerMediaSubsession * /*subsession*/, char const * cseq )
{
	snprintf( (char *) fResponseBuffer, sizeof( fResponseBuffer ),
	          "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n", cseq, dateHeader() );
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

static Boolean parseScaleHeader(char const* buf, float& scale) {
  // Initialize the result parameter to a default value:
  scale = 1.0;

  // First, find "Scale:"
  while (1) {
    if (*buf == '\0') return False; // not found
    if (_strncasecmp(buf, "Scale: ", 7) == 0) break;
    ++buf;
  }

  // Then, run through each of the fields, looking for ones we handle:
  char const* fields = buf + 7;
  while (*fields == ' ') ++fields;
#if defined( LIVE )
  float sc;
#elif defined( LIVE_20110616 )
  double sc;
#endif
  if (sscanf(fields, "%f", &sc) == 1) {
    scale = sc;
  } else {
    return False; // The header is malformed
  }

  return True;
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_PLAY(ServerMediaSubsession* subsession, char const* cseq,
		   char const* fullRequestStr) {
  char* rtspURL = fOurServer.rtspURL( fOurServerMediaSession );
  unsigned rtspURLSize = strlen(rtspURL);

  //// Parse the client's "Scale:" header, if any: 
  float scale;
  Boolean sawScaleHeader = parseScaleHeader(fullRequestStr, scale);

  // Try to set the stream's scale factor to this value:
  if (subsession == NULL /*aggregate op*/) {
    fOurServerMediaSession->testScaleFactor(scale);
  } else {
    subsession->testScaleFactor(scale);
  }

  char buf[100];
  char* scaleHeader;
  if (!sawScaleHeader) {
    buf[0] = '\0'; // Because we didn't see a Scale: header, don't send one back
  } else {
    sprintf(buf, "Scale: %f\r\n", scale);
  }
  scaleHeader = strDup(buf);

  //// Parse the client's "Range:" header, if any: 
#if defined( LIVE )
  float rangeStart = 0.0, rangeEnd = 0.0;
#elif defined( LIVE_20110616 )
  double rangeStart = 0.0, rangeEnd = 0.0;
#endif
  Boolean sawRangeHeader = parseRangeHeader(fullRequestStr, rangeStart, rangeEnd);

  // Use this information, plus the stream's duration (if known), to create
  // our own "Range:" header, for the response:
#if defined( LIVE )
  float duration = subsession == NULL /*aggregate op*/
#elif defined( LIVE_20110616 )
  double duration = subsession == NULL /*aggregate op*/
#endif
    ? fOurServerMediaSession->duration() : subsession->duration();
  if (duration < 0.0) {
    // We're an aggregate PLAY, but the subsessions have different durations.
    // Use the largest of these durations in our header
    duration = -duration;
  }

  if (rangeEnd < 0.0 || rangeEnd > duration) rangeEnd = duration;
  if (rangeStart < 0.0) {
    rangeStart = 0.0;
  } else if (rangeEnd > 0.0 && scale > 0.0 && rangeStart > rangeEnd) {
    rangeStart = rangeEnd;
  }

  char* rangeHeader;
  if (!sawRangeHeader) {
    buf[0] = '\0'; // Because we didn't see a Range: header, don't send one back
  } else if (rangeEnd == 0.0 && scale >= 0.0) {
    sprintf(buf, "Range: npt=%.3f-\r\n", rangeStart);
  } else {
    sprintf(buf, "Range: npt=%.3f-%.3f\r\n", rangeStart, rangeEnd);
  }
  rangeHeader = strDup(buf);

  // Create a "RTP-Info:" line.  It will get filled in from each subsession's state:
  char const* rtpInfoFmt =
    "%s" // "RTP-Info:", plus any preceding rtpInfo items
    "%s" // comma separator, if needed 
    "url=%s/%s"
    ";seq=%d"
    ";rtptime=%u"
    ;
  unsigned rtpInfoFmtSize = strlen(rtpInfoFmt);
  char* rtpInfo = strDup("RTP-Info: ");
  unsigned i, numRTPInfoItems = 0;

  // Do any required seeking/scaling on each subsession, before starting streaming:
  for (i = 0; i < fNumStreamStates; ++i) {
    if (subsession == NULL /* means: aggregated operation */
	|| subsession == fStreamStates[i].subsession) {
      if (sawScaleHeader) {
	fStreamStates[i].subsession->setStreamScale(fOurSessionId,
						    fStreamStates[i].streamToken,
						    scale);
      }
      if (sawRangeHeader) {
#if defined( LIVE )
	fStreamStates[i].subsession->seekStream(fOurSessionId,
						fStreamStates[i].streamToken,
						rangeStart);
#elif defined( LIVE_20110616 )
        double streamDuration = 0.0;
	if (rangeEnd > 0.0 && (rangeEnd+0.001) < duration) { // the 0.001 is because we limited the values to 3 decimal places
	  // We want the stream to end early.  Set the duration we want:
	  streamDuration = rangeEnd - rangeStart;
	  if (streamDuration < 0.0) streamDuration = -streamDuration; // should happen only if scale < 0.0
	}
	fStreamStates[i].subsession->seekStream(fOurSessionId,
						fStreamStates[i].streamToken,
						rangeStart, streamDuration);
#endif
      }
    }
  }

  // Now, start streaming:
  for (i = 0; i < fNumStreamStates; ++i) {
    if (subsession == NULL /* means: aggregated operation */
	|| subsession == fStreamStates[i].subsession) {
      unsigned short rtpSeqNum = 0;
      unsigned rtpTimestamp = 0;
#if defined( LIVE )
      fStreamStates[i].subsession->startStream(fOurSessionId,
					       fStreamStates[i].streamToken,
					       NULL,
					       this,
					       rtpSeqNum,
					       rtpTimestamp);
#elif defined( LIVE_20110616 )
      fStreamStates[i].subsession->startStream(fOurSessionId,
					       fStreamStates[i].streamToken,
					       NULL,
					       this,
					       rtpSeqNum,
					       rtpTimestamp,
					       NULL,
					       this);
#endif
      const char *urlSuffix = fStreamStates[i].subsession->trackId();
      char* prevRTPInfo = rtpInfo;
      unsigned rtpInfoSize = rtpInfoFmtSize
	+ strlen(prevRTPInfo)
	+ 1
	+ rtspURLSize + strlen(urlSuffix)
	+ 5 /*max unsigned short len*/
	+ 10 /*max unsigned (32-bit) len*/
	+ 2 /*allows for trailing \r\n at final end of string*/; 
      rtpInfo = new char[rtpInfoSize];
      sprintf(rtpInfo, rtpInfoFmt,
	      prevRTPInfo,
	      numRTPInfoItems++ == 0 ? "" : ",",
	      rtspURL, urlSuffix,
	      rtpSeqNum
	      ,rtpTimestamp
	      );
      delete[] prevRTPInfo;
    }
  }
  if (numRTPInfoItems == 0) {
    rtpInfo[0] = '\0';
  } else {
    unsigned rtpInfoLen = strlen(rtpInfo);
    rtpInfo[rtpInfoLen] = '\r';
    rtpInfo[rtpInfoLen+1] = '\n';
    rtpInfo[rtpInfoLen+2] = '\0';
  }

  // Fill in the response:
  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
	   "RTSP/1.0 200 OK\r\n"
	   "CSeq: %s\r\n"
	   "%s"
	   "%s"
	   "%s"
           "Session: %d;timeout=80\r\n"
           "%s\r\n",
           cseq,
           dateHeader(),
           scaleHeader,
           rangeHeader,
           fOurSessionId,
           rtpInfo);
  delete[] rtpInfo; delete[] rangeHeader;
  delete[] scaleHeader; delete[] rtspURL;
  if ( fClientData->outreq )
    envir().taskScheduler().turnOffBackgroundReadHandling( fClientData->outreq->fd );
  fClientData->flags |= FLAGS_LIVE555_PLAY;
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_PAUSE(ServerMediaSubsession* subsession, char const* cseq) {
  for (unsigned i = 0; i < fNumStreamStates; ++i) {
    if (subsession == NULL /* means: aggregated operation */
	|| subsession == fStreamStates[i].subsession) {
      fStreamStates[i].subsession->pauseStream(fOurSessionId,
					       fStreamStates[i].streamToken);
    }
  }
  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
	   "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %d\r\n\r\n",
	   cseq, dateHeader(), fOurSessionId);
  fClientData->flags &= ~ FLAGS_LIVE555_PLAY;
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_GET_PARAMETER(
	ServerMediaSubsession *subsession, char const *cseq, char const* /*fullRequestStr*/ ) {

	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %d\r\n\r\n",
	          cseq, dateHeader(), fOurSessionId );
}

void RTSP_O_HTTPServer::RTSPClientSession::handleCmd_SET_PARAMETER(
	ServerMediaSubsession *subsession, char const *cseq, char const* /*fullRequestStr*/ ) {

	snprintf( (char *) fResponseBuffer, sizeof fResponseBuffer,
	          "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %d\r\n\r\n",
	          cseq, dateHeader(), fOurSessionId );
}

static Boolean parseAuthorizationHeader( char const *buf, char const *&username, char const *&realm, char const *&nonce, char const *&uri, char const *&response )
{
  // Initialize the result parameters to default values:
  username = realm = nonce = uri = response = NULL;

  // First, find "Authorization:"
  while (1) {
    if (*buf == '\0') return False; // not found
    if (_strncasecmp(buf, "Authorization: Digest ", 22) == 0) break;
    ++buf;
  }

  // Then, run through each of the fields, looking for ones we handle:
  char const* fields = buf + 22;
  while (*fields == ' ') ++fields;
  char* parameter = strDupSize(fields);
  char* value = strDupSize(fields);
  while (1) {
    value[0] = '\0';
    if (sscanf(fields, "%[^=]=\"%[^\"]\"", parameter, value) != 2 &&
	sscanf(fields, "%[^=]=\"\"", parameter) != 1) {
      break;
    }
    if (strcmp(parameter, "username") == 0) {
      username = strDup(value);
    } else if (strcmp(parameter, "realm") == 0) {
      realm = strDup(value);
    } else if (strcmp(parameter, "nonce") == 0) {
      nonce = strDup(value);
    } else if (strcmp(parameter, "uri") == 0) {
      uri = strDup(value);
    } else if (strcmp(parameter, "response") == 0) {
      response = strDup(value);
    }

    fields += strlen(parameter) + 2 /*="*/ + strlen(value) + 1 /*"*/;
    while (*fields == ',' || *fields == ' ') ++fields;
        // skip over any separating ',' and ' ' chars
    if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
  }
  delete[] parameter; delete[] value;
  return True;
}

Boolean RTSP_O_HTTPServer::RTSPClientSession::authenticationOK(
	char const *cmdName, char const* cseq, char const *urlSuffix, char const *fullRequestStr )
{
  if (fOurServer.fAuthDB == NULL) return True;

  char const* username = NULL; char const* realm = NULL; char const* nonce = NULL;
  char const* uri = NULL; char const* response = NULL;
  Boolean success = False;

  do {
    if (fCurrentAuthenticator.nonce() == NULL) break;

    if ( ( ! parseAuthorizationHeader( fullRequestStr, username, realm, nonce, uri, response ) )
	|| username == NULL
	|| realm == NULL || strcmp(realm, fCurrentAuthenticator.realm()) != 0
	|| nonce == NULL || strcmp(nonce, fCurrentAuthenticator.nonce()) != 0
	|| uri == NULL || response == NULL) {
      break;
    }

    // Next, the username has to be known to us:
    char const* password = fOurServer.fAuthDB->lookupPassword(username);
    if (password == NULL) break;
    fCurrentAuthenticator.
      setUsernameAndPassword(username, password,
			     fOurServer.fAuthDB->passwordsAreMD5());

    char const* ourResponse
      = fCurrentAuthenticator.computeDigestResponse(cmdName, uri);
    success = (strcmp(ourResponse, response) == 0);
    fCurrentAuthenticator.reclaimDigestResponse(ourResponse);
  } while (0);

  delete[] (char*)username; delete[] (char*)realm; delete[] (char*)nonce;
  delete[] (char*)uri; delete[] (char*)response;

  if (success) return True;
  fCurrentAuthenticator.setRealmAndRandomNonce(fOurServer.fAuthDB->realm());
  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
	   "RTSP/1.0 401 Unauthorized\r\n"
	   "CSeq: %s\r\n"
	   "%s"
	   "WWW-Authenticate: Digest realm=\"%s\", nonce=\"%s\"\r\n\r\n",
	   cseq,
	   dateHeader(),
	   fCurrentAuthenticator.realm(), fCurrentAuthenticator.nonce());
  return False;
}

